/*
 * This file is part of GtkSourceView
 *
 * Copyright 2007-2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright 2009 Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright 2013 Sébastien Wilmet <swilmet@gnome.org>
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtksourcecompletion-private.h"
#include "gtksourcecompletioncontext-private.h"
#include "gtksourcecompletionlist-private.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourcebuffer.h"
#include "gtksourceview-private.h"

/**
 * GtkSourceCompletion:
 *
 * Main Completion Object.
 *
 * The completion system helps the user when they writes some text,
 * such as words, command names, functions, and suchlike. Proposals can
 * be shown, to complete the text the user is writing. Each proposal can
 * contain an additional piece of information (for example
 * documentation), that is displayed when the "Details" button is
 * clicked.
 *
 * Proposals are created via a [iface@CompletionProvider]. There can
 * be for example a provider to complete words (see [class@CompletionWords]),
 * another provider for the completion of
 * function names, etc. To add a provider, call
 * [method@Completion.add_provider].
 *
 * The [iface@CompletionProposal] interface represents a proposal.
 *
 * If a proposal contains extra information (see
 * %GTK_SOURCE_COMPLETION_COLUMN_DETAILS), it will be
 * displayed in a supplemental details window, which appears when
 * the "Details" button is clicked.
 *
 * Each [class@View] object is associated with a [class@Completion]
 * instance. This instance can be obtained with
 * [method@View.get_completion]. The [class@View] class contains also the
 * [signal@View::show-completion] signal.
 *
 * A same [iface@CompletionProvider] object can be used for several
 * `GtkSourceCompletion`'s.
 */

#define DEFAULT_PAGE_SIZE 5

struct _GtkSourceCompletion
{
	GObject parent_instance;

	/* The GtkSourceView that we are providing results for. This can be
	 * used by providers to get a reference.
	 */
	GtkSourceView *view;

	/* A cancellable that we'll monitor to cancel anything that is currently
	 * in-flight. This is reset to a new GCancellable after each time
	 * g_cancellable_cancel() is called.
	 */
	GCancellable *cancellable;

	/* An array of providers that have been registered. These will be queried
	 * when input is provided for completion.
	 */
	GPtrArray *providers;

	/* If we are currently performing a completion, the context is stored
	 * here. It will be cleared as soon as it's no longer valid to
	 * (re)display.
	 */
	GtkSourceCompletionContext *context;

	/* The signal group is used to track changes to the context while it is
	 * our current context. That includes handling notification of the first
	 * result so that we can show the window, etc.
	 */
	GSignalGroup *context_signals;

	/* Signals to changes in the underlying GtkTextBuffer that we use to
	 * determine where and how we can do completion.
	 */
	GSignalGroup *buffer_signals;

	/* We need to track various events on the view to ensure that we don't
	 * activate at incorrect times.
	 */
	GSignalGroup *view_signals;

	/* The display popover for results */
	GtkSourceCompletionList *display;

	/* The completion mark for alignment */
	GtkTextMark *completion_mark;

	/* Our current event while processing so that we can get access to it
	 * from a callback back into the completion instance.
	 */
	const GdkKeyEvent *current_event;

	/* Our cached font description to apply to views. */
	PangoFontDescription *font_desc;

	/* If we have a queued update to refilter after deletions, this will be
	 * set to the GSource id.
	 */
	guint queued_update;

	/* This value is incremented/decremented based on if we need to suppress
	 * visibility of the completion window (and avoid doing queries).
	 */
	guint block_count;

	/* Re-entrancy protection for gtk_source_completion_show(). */
	guint showing;

	/* The number of rows to display. This is propagated to the window if/when
	 * the window is created.
	 */
	guint page_size;

	/* Handler for gtk_widget_add_tick_callback() to do delayed calls to
	 * gtk_widget_set_visible(FALSE) (so that we don't potentially flap between
	 * hide/show while typing.
	 */
	guint hide_tick_handler;

	/* If we have a completion actively in play */
	guint waiting_for_results : 1;

	/* If we should refilter after the in-flight context completes */
	guint needs_refilter : 1;

	/* If the first item is automatically selected */
	guint select_on_show : 1;

	/* If we remember to re-show the info window */
	guint remember_info_visibility : 1;

	/* If icon column is visible */
	guint show_icons : 1;

	guint disposed : 1;
};

G_DEFINE_TYPE (GtkSourceCompletion, gtk_source_completion, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_BUFFER,
	PROP_PAGE_SIZE,
	PROP_REMEMBER_INFO_VISIBILITY,
	PROP_SELECT_ON_SHOW,
	PROP_SHOW_ICONS,
	PROP_VIEW,
	N_PROPS
};

enum {
	ACTIVATE,
	PROVIDER_ADDED,
	PROVIDER_REMOVED,
	SHOW,
	HIDE,
	N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
display_show (GtkSourceCompletion *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	if (self->hide_tick_handler != 0)
	{
		/* See display_hide() for why a tick handler is used */
		gtk_widget_remove_tick_callback (GTK_WIDGET (self->view),
		                                 self->hide_tick_handler);
		self->hide_tick_handler = 0;
	}

	if (gtk_widget_get_mapped (GTK_WIDGET (self->view)))
	{
		gtk_widget_set_visible (GTK_WIDGET (_gtk_source_completion_get_display (self)), TRUE);
	}
}

static gboolean
display_hide_cb (GtkWidget     *widget,
                 GdkFrameClock *frame_clock,
                 gpointer       user_data)
{
	GtkSourceCompletion *self = user_data;

	g_assert (GTK_SOURCE_IS_VIEW (widget));
	g_assert (GDK_IS_FRAME_CLOCK (frame_clock));
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	self->hide_tick_handler = 0;

	if (self->display != NULL)
	{
		gtk_widget_set_visible (GTK_WIDGET (self->display), FALSE);
	}

	return G_SOURCE_REMOVE;
}

static void
display_hide (GtkSourceCompletion *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	/* We don't want to hide immediately because we might get another
	 * change that causes the assistant to be redisplayed before the
	 * next frame. Flapping the visibility is really distracting, so we'll
	 * wait until the next start of the frame clock cycle to hide.
	 *
	 * We use the GtkSourceView's frame clock for this instead of the GtkPopover
	 * because that is the frame clock we need to synchronize with to be sure
	 * were not invalidating allocations during an active frame cycle.
	 */

	if (self->display == NULL ||
	    self->hide_tick_handler != 0 ||
	    !gtk_widget_get_visible (GTK_WIDGET (self->display)))
	{
		return;
	}

	self->hide_tick_handler =
		gtk_widget_add_tick_callback (GTK_WIDGET (self->view),
		                              display_hide_cb,
		                              g_object_ref (self),
		                              g_object_unref);
}

static gboolean
gtk_source_completion_is_blocked (GtkSourceCompletion *self)
{
	GtkTextBuffer *buffer;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	return self->block_count > 0 ||
	       self->view == NULL ||
	       self->providers->len == 0 ||
	       !gtk_widget_get_visible (GTK_WIDGET (self->view)) ||
	       !gtk_widget_has_focus (GTK_WIDGET (self->view)) ||
	       !(buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view))) ||
	       gtk_text_buffer_get_has_selection (buffer) ||
	       !GTK_SOURCE_IS_VIEW (self->view);
}

static PangoFontDescription *
create_font_description (GtkSourceCompletion *self)
{
	PangoFontDescription *font_desc;
	PangoContext *context;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	if (self->view == NULL)
	{
		return NULL;
	}

	context = gtk_widget_get_pango_context (GTK_WIDGET (self->view));
	font_desc = pango_font_description_copy (pango_context_get_font_description (context));

	/*
	 * Work around issue where when a proposal provides "<b>markup</b>" and
	 * the weight is set in the font description, the <b> markup will not
	 * have it's weight respected. This seems to be happening because the
	 * weight mask is getting set in pango_font_description_from_string()
	 * even if the the value is set to normal. That matter is complicated
	 * because PangoAttrFontDesc and PangoAttrWeight will both have the
	 * same starting offset in the PangoLayout.
	 * https://bugzilla.gnome.org/show_bug.cgi?id=755968
	 */
	if (PANGO_WEIGHT_NORMAL == pango_font_description_get_weight (font_desc))
	{
		pango_font_description_unset_fields (font_desc, PANGO_FONT_MASK_WEIGHT);
	}

	return g_steal_pointer (&font_desc);
}

gboolean
_gtk_source_completion_get_select_on_show (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), FALSE);

	return self->select_on_show;
}

static void
gtk_source_completion_set_select_on_show (GtkSourceCompletion *self,
                                          gboolean             select_on_show)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	select_on_show = !!select_on_show;

	if (self->select_on_show != select_on_show)
	{
		self->select_on_show = select_on_show;
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECT_ON_SHOW]);
	}
}

static void
gtk_source_completion_complete_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
	GtkSourceCompletionContext *context = (GtkSourceCompletionContext *)object;
	GtkSourceCompletion *self = user_data;
	GError *error = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (G_IS_ASYNC_RESULT (result));
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	if (self->context == context)
	{
		self->waiting_for_results = FALSE;
	}

	if (!_gtk_source_completion_context_complete_finish (context, result, &error))
	{
		g_debug ("Completion failed to complete: %s", error->message);
		goto cleanup;
	}

	if (context != self->context)
		goto cleanup;

	if (self->needs_refilter)
	{
		/*
		 * At this point, we've gotten our new results for the context. But we had
		 * new content come in since we fired that request. So we need to ask the
		 * providers to further reduce the list based on updated query text.
		 */
		self->needs_refilter = FALSE;
		_gtk_source_completion_context_refilter (context);
	}

	if (!gtk_source_completion_context_get_empty (context))
	{
		display_show (self);
	}
	else
	{
		display_hide (self);
	}

cleanup:
	g_clear_error (&error);
	g_clear_object (&self);
}

static void
_gtk_source_completion_set_context (GtkSourceCompletion        *self,
                                    GtkSourceCompletionContext *context)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (!context || GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	if (g_set_object (&self->context, context))
	{
		g_clear_handle_id (&self->queued_update, g_source_remove);
		g_signal_group_set_target (self->context_signals, context);
	}
}

static void
gtk_source_completion_cancel (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	/* Nothing can re-use in-flight results now */
	self->waiting_for_results = FALSE;
	self->needs_refilter = FALSE;

	if (self->context != NULL)
	{
		g_cancellable_cancel (self->cancellable);
		g_clear_object (&self->cancellable);

		_gtk_source_completion_set_context (self, NULL);

		if (self->display != NULL)
		{
			_gtk_source_completion_list_set_context (self->display, NULL);
			gtk_widget_set_visible (GTK_WIDGET (self->display), FALSE);
		}
	}
}

static inline gboolean
is_symbol_char (gunichar ch)
{
	return ch == '_' || g_unichar_isalnum (ch);
}

static gboolean
gtk_source_completion_compute_bounds (GtkSourceCompletion *self,
                                      GtkTextIter         *begin,
                                      GtkTextIter         *end)
{
	GtkTextBuffer *buffer;
	GtkTextMark *insert;
	gunichar ch = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (begin != NULL);
	g_assert (end != NULL);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
	insert = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, end, insert);

	*begin = *end;

	do
	{
		if (!gtk_text_iter_backward_char (begin))
			break;
		ch = gtk_text_iter_get_char (begin);
	}
	while (is_symbol_char (ch));

	if (ch && !is_symbol_char (ch))
	{
		gtk_text_iter_forward_char (begin);
	}

	return !gtk_text_iter_equal (begin, end);
}

static void
gtk_source_completion_start (GtkSourceCompletion           *self,
                             GtkSourceCompletionActivation  activation,
                             gboolean                       from_trigger)
{
	GtkSourceCompletionContext *context = NULL;
	GtkTextBuffer *buffer;
	GtkTextIter begin;
	GtkTextIter end;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (self->context == NULL);

	g_clear_handle_id (&self->queued_update, g_source_remove);

	if (!gtk_source_completion_compute_bounds (self, &begin, &end))
	{
		if (!from_trigger && activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
		{
			goto cleanup;
		}

		begin = end;
	}

	context = _gtk_source_completion_context_new (self);
	for (guint i = 0; i < self->providers->len; i++)
		_gtk_source_completion_context_add_provider (context, g_ptr_array_index (self->providers, i));
	_gtk_source_completion_set_context (self, context);

	self->waiting_for_results = TRUE;
	self->needs_refilter = FALSE;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
	gtk_text_buffer_move_mark (buffer, self->completion_mark, &begin);

	_gtk_source_completion_context_complete_async (context,
						       activation,
						       &begin,
						       &end,
						       self->cancellable,
						       gtk_source_completion_complete_cb,
						       g_object_ref (self));

	if (self->display != NULL)
	{
		_gtk_source_completion_list_set_context (self->display, context);

		if (!gtk_source_completion_context_get_empty (context))
		{
			display_show (self);
		}
		else
		{
			display_hide (self);
		}
	}

cleanup:
	g_clear_object (&context);
}

static void
gtk_source_completion_update (GtkSourceCompletion           *self,
                              GtkSourceCompletionActivation  activation,
                              gboolean                       from_trigger)
{
	GtkTextBuffer *buffer;
	GtkTextMark *insert;
	GtkTextIter begin;
	GtkTextIter end;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (self->context != NULL);
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self->context));

	/*
	 * First, find the boundary for the word we are trying to complete. We might
	 * be able to refine a previous query instead of making a new one which can
	 * save on a lot of backend work.
	 */
	gtk_source_completion_compute_bounds (self, &begin, &end);

	if (_gtk_source_completion_context_can_refilter (self->context, &begin, &end))
	{
		/*
		 * Make sure we update providers that have already delivered results
		 * even though some of them won't be ready yet.
		 */
		_gtk_source_completion_context_refilter (self->context);

		/*
		 * If we're waiting for the results still to come in, then just mark
		 * that we need to do post-processing rather than trying to refilter now.
		 */
		if (self->waiting_for_results)
		{
			self->needs_refilter = TRUE;
			return;
		}

		if (!gtk_source_completion_context_get_empty (self->context))
		{
			display_show (self);
		}
		else
		{
			display_hide (self);
		}

		return;
	}

	if (!gtk_source_completion_context_get_bounds (self->context, &begin, &end) ||
	    gtk_text_iter_equal (&begin, &end))
	{
		if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE)
		{
			gtk_source_completion_hide (self);
			return;
		}

		goto reset;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view));
	insert = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert);

	/*
	 * If our completion prefix bounds match the prefix that we looked
	 * at previously, we can possibly refilter the previous context instead
	 * of creating a new context.
	 */

	/*
	 * The context uses GtkTextMark which should have been advanced as
	 * the user continued to type. So if @end matches @iter (our insert
	 * location), then we can possibly update the previous context by
	 * further refining the query to a subset of the result.
	 */
	if (gtk_text_iter_equal (&iter, &end))
	{
		gtk_source_completion_show (self);
		return;
	}

reset:
	gtk_source_completion_cancel (self);
	gtk_source_completion_start (self, activation, from_trigger);
}

static void
gtk_source_completion_real_hide (GtkSourceCompletion *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	if (self->display != NULL)
	{
		gtk_widget_set_visible (GTK_WIDGET (self->display), FALSE);
	}
}

static void
gtk_source_completion_real_show (GtkSourceCompletion *self)
{
	GtkSourceCompletionList *display;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	display = _gtk_source_completion_get_display (self);

	/* If the user is requesting completion manually, we should throw away
	 * our previous results and attempt completion over. Otherwise, providers
	 * which bailed because they were in _INTERACTIVE_ mode will not be
	 * requeried for updated results.
	 */
	g_clear_object (&self->context);

	gtk_source_completion_start (self, GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED, FALSE);

	_gtk_source_completion_list_set_context (display, self->context);

	if (!gtk_source_completion_context_get_empty (self->context))
	{
		display_show (self);
	}
	else
	{
		display_hide (self);
	}
}

static gboolean
gtk_source_completion_queued_update_cb (gpointer user_data)
{
	GtkSourceCompletion *self = user_data;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	self->queued_update = 0;

	if (self->context != NULL)
	{
		gtk_source_completion_update (self, GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE, FALSE);
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_completion_queue_update (GtkSourceCompletion *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	g_clear_handle_id (&self->queued_update, g_source_remove);

	/*
	 * We hit this code path when the user has deleted text. We want to
	 * introduce just a bit of delay so that deleting under heavy key
	 * repeat will not stall doing lots of refiltering.
	 */

	self->queued_update =
		g_timeout_add_full (G_PRIORITY_LOW,
		                    16.7*2 + 1,
		                    gtk_source_completion_queued_update_cb,
		                    self,
		                    NULL);
}

static void
gtk_source_completion_notify_context_empty_cb (GtkSourceCompletion        *self,
                                               GParamSpec                 *pspec,
                                               GtkSourceCompletionContext *context)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (pspec != NULL);
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	if (context != self->context)
	{
		/* Delayed notification from a context we no longer care about.
		 * Just silently drop the notification.
		 */
		return;
	}

	if (gtk_source_completion_context_get_empty (context))
	{
		if (self->display != NULL)
		{
			display_hide (self);
		}
	}
	else
	{
		display_show (self);
	}
}

static void
gtk_source_completion_buffer_delete_range_after_cb (GtkSourceCompletion *self,
						    GtkTextIter         *begin,
						    GtkTextIter         *end,
						    GtkTextBuffer       *buffer)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (GTK_SOURCE_IS_VIEW (self->view));
	g_assert (begin != NULL);
	g_assert (end != NULL);
	g_assert (GTK_IS_TEXT_BUFFER (buffer));

	if (self->context != NULL)
	{
		if (!gtk_source_completion_is_blocked (self))
		{
			GtkTextIter b, e;

			gtk_source_completion_context_get_bounds (self->context, &b, &e);

			/*
			 * If they just backspaced all of the text, then we want to just hide
			 * the completion window since that can get a bit intrusive.
			 */
			if (gtk_text_iter_equal (&b, &e))
			{
				g_clear_handle_id (&self->queued_update, g_source_remove);
				gtk_source_completion_cancel (self);
				return;
			}

			gtk_source_completion_queue_update (self);
		}
	}
}

static void
gtk_source_completion_view_move_cursor_cb (GtkSourceCompletion *self,
                                           GtkMovementStep      step,
                                           gint                 count,
                                           gboolean             extend_selection,
                                           GtkSourceView       *view)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	/* TODO: Should we keep the context alive while we begin a new one?
	 *       Or rather, how can we avoid the hide/show of the widget that
	 *       could result in flicker?
	 */

	if (self->display != NULL &&
	    gtk_widget_get_visible (GTK_WIDGET (self->display)))
	{
		gtk_source_completion_cancel (self);
	}
}

static gboolean
is_single_char (const gchar *text,
                gint         len)
{
	if (len == 1)
		return TRUE;
	else if (len > 6)
		return FALSE;
	else
		return g_utf8_strlen (text, len) == 1;
}

static void
gtk_source_completion_buffer_insert_text_after_cb (GtkSourceCompletion *self,
                                                   GtkTextIter         *iter,
                                                   const gchar         *text,
                                                   gint                 len,
                                                   GtkTextBuffer       *buffer)
{
	const GtkSourceCompletionActivation activation = GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE;
	gboolean from_trigger = FALSE;
	GtkTextIter begin;
	GtkTextIter end;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (iter != NULL);
	g_assert (text != NULL);
	g_assert (len > 0);
	g_assert (GTK_IS_TEXT_BUFFER (buffer));

	g_clear_handle_id (&self->queued_update, g_source_remove);

	if (gtk_source_completion_is_blocked (self) || !is_single_char (text, len))
	{
		gtk_source_completion_cancel (self);
		return;
	}

	if (!gtk_source_completion_compute_bounds (self, &begin, &end))
	{
		GtkTextIter cur = end;

		if (gtk_text_iter_backward_char (&cur))
		{
			gunichar ch = gtk_text_iter_get_char (&cur);

			for (guint i = 0; i < self->providers->len; i++)
			{
				GtkSourceCompletionProvider *provider = g_ptr_array_index (self->providers, i);

				if (gtk_source_completion_provider_is_trigger (provider, &end, ch))
				{
					/*
					 * We got a trigger, but we failed to continue the bounds of a previous
					 * completion. We need to cancel the previous completion (if any) first
					 * and then try to start a new completion due to trigger.
					 */
					from_trigger = TRUE;
					gtk_source_completion_cancel (self);
					goto do_completion;
				}
			}
		}

		gtk_source_completion_cancel (self);
		return;
	}

do_completion:

	if (self->context == NULL)
		gtk_source_completion_start (self, activation, from_trigger);
	else
		gtk_source_completion_update (self, activation, from_trigger);
}

static void
gtk_source_completion_buffer_mark_set_cb (GtkSourceCompletion *self,
                                          const GtkTextIter   *iter,
                                          GtkTextMark         *mark,
                                          GtkTextBuffer       *buffer)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (GTK_IS_TEXT_MARK (mark));
	g_assert (GTK_IS_TEXT_BUFFER (buffer));

	if (mark != gtk_text_buffer_get_insert (buffer))
		return;

	if (_gtk_source_completion_context_iter_invalidates (self->context, iter))
	{
		gtk_source_completion_cancel (self);
	}
}

GtkSourceCompletion *
_gtk_source_completion_new (GtkSourceView *view)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION,
			     "view", view,
			     NULL);
}

static void
gtk_source_completion_set_view (GtkSourceCompletion *self,
                                GtkSourceView       *view)
{
	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	if (g_set_weak_pointer (&self->view, view))
	{
		g_signal_group_set_target (self->view_signals, view);
		g_object_bind_property (view, "buffer",
		                        self->buffer_signals, "target",
		                        G_BINDING_SYNC_CREATE);
	}
}

static void
on_buffer_signals_bind (GtkSourceCompletion *self,
                        GtkSourceBuffer     *buffer,
                        GSignalGroup        *signals_)
{
	GtkTextIter where;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));
	g_assert (G_IS_SIGNAL_GROUP (signals_));

	if (self->disposed)
		return;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &where);
	self->completion_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
	                                                     NULL,
	                                                     &where,
	                                                     TRUE);

	if (self->display != NULL)
	{
		_gtk_source_assistant_set_mark (GTK_SOURCE_ASSISTANT (self->display),
		                                self->completion_mark);
	}
}

static void
gtk_source_completion_dispose (GObject *object)
{
	GtkSourceCompletion *self = (GtkSourceCompletion *)object;

	g_assert (GTK_SOURCE_IS_COMPLETION (self));

	self->disposed = TRUE;

	if (self->hide_tick_handler != 0)
	{
		if (self->view != NULL)
		{
			gtk_widget_remove_tick_callback (GTK_WIDGET (self->view),
			                                 self->hide_tick_handler);
		}

		self->hide_tick_handler = 0;
	}

	g_signal_group_set_target (self->context_signals, NULL);
	g_signal_group_set_target (self->buffer_signals, NULL);
	g_signal_group_set_target (self->view_signals, NULL);

	g_clear_pointer ((GtkSourceAssistant **)&self->display, _gtk_source_assistant_destroy);

	g_clear_object (&self->context);
	g_clear_object (&self->cancellable);

	if (self->providers->len > 0)
	{
		g_ptr_array_remove_range (self->providers, 0, self->providers->len);
	}

	G_OBJECT_CLASS (gtk_source_completion_parent_class)->dispose (object);
}

static void
gtk_source_completion_finalize (GObject *object)
{
	GtkSourceCompletion *self = (GtkSourceCompletion *)object;

	g_clear_weak_pointer (&self->view);

	G_OBJECT_CLASS (gtk_source_completion_parent_class)->finalize (object);
}

static void
gtk_source_completion_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
	case PROP_PAGE_SIZE:
		g_value_set_uint (value, gtk_source_completion_get_page_size (self));
		break;

	case PROP_REMEMBER_INFO_VISIBILITY:
		g_value_set_boolean (value, self->remember_info_visibility);
		break;

	case PROP_SELECT_ON_SHOW:
		g_value_set_boolean (value, _gtk_source_completion_get_select_on_show (self));
		break;

	case PROP_SHOW_ICONS:
		g_value_set_boolean (value, self->show_icons);
		break;

	case PROP_VIEW:
		g_value_set_object (value, self->view);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
	case PROP_PAGE_SIZE:
		gtk_source_completion_set_page_size (self, g_value_get_uint (value));
		break;

	case PROP_REMEMBER_INFO_VISIBILITY:
		self->remember_info_visibility = g_value_get_boolean (value);
		if (self->display != NULL)
		{
			_gtk_source_completion_list_set_remember_info_visibility (self->display,
			                                                          self->remember_info_visibility);
		}
		g_object_notify_by_pspec (object, pspec);
		break;

	case PROP_SELECT_ON_SHOW:
		gtk_source_completion_set_select_on_show (self, g_value_get_boolean (value));
		break;

	case PROP_SHOW_ICONS:
		self->show_icons = g_value_get_boolean (value);
		if (self->display != NULL)
		{
			_gtk_source_completion_list_set_show_icons (self->display, self->show_icons);
		}
		g_object_notify_by_pspec (object, pspec);
		break;

	case PROP_VIEW:
		gtk_source_completion_set_view (self, g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_class_init (GtkSourceCompletionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_completion_dispose;
	object_class->finalize = gtk_source_completion_finalize;
	object_class->get_property = gtk_source_completion_get_property;
	object_class->set_property = gtk_source_completion_set_property;

	/**
	 * GtkSourceCompletion:buffer:
	 *
	 * The #GtkTextBuffer for the #GtkSourceCompletion:view.
	 * This is a convenience property for providers.
	 */
	properties [PROP_BUFFER] =
		g_param_spec_object ("buffer",
		                     "Buffer",
		                     "The buffer for the view",
		                     GTK_TYPE_TEXT_VIEW,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceCompletion:page-size:
	 *
	 * The number of rows to display to the user before scrolling.
	 */
	properties [PROP_PAGE_SIZE] =
		g_param_spec_uint ("page-size",
		                   "Number of Rows",
		                   "Number of rows to display to the user",
		                   1, 32, DEFAULT_PAGE_SIZE,
		                   G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceCompletion:remember-info-visibility:
	 *
	 * Determines whether the visibility of the info window should be saved when the
	 * completion is hidden, and restored when the completion is shown again.
	 */
	properties [PROP_REMEMBER_INFO_VISIBILITY] =
		g_param_spec_boolean ("remember-info-visibility",
		                      "Remember Info Visibility",
		                      "Remember Info Visibility",
		                      FALSE,
		                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletion:select-on-show:
	 *
	 * Determines whether the first proposal should be selected when the completion
	 * is first shown.
	 */
	properties [PROP_SELECT_ON_SHOW] =
		g_param_spec_boolean ("select-on-show",
		                      "Select on Show",
		                      "Select on Show",
		                      FALSE,
		                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletion:show-icons:
	 *
	 * The "show-icons" property denotes if icons should be displayed within
	 * the list of completions presented to the user.
	 */
	properties [PROP_SHOW_ICONS] =
		g_param_spec_boolean ("show-icons",
		                      "Show Icons",
		                      "If icons should be shown in the completion results",
		                      TRUE,
		                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletion:view:
	 *
	 * The "view" property is the #GtkTextView for which this #GtkSourceCompletion
	 * is providing completion features.
	 */
	properties [PROP_VIEW] =
		g_param_spec_object ("view",
		                     "View",
		                     "The text view for which to provide completion",
		                     GTK_SOURCE_TYPE_VIEW,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceCompletion::provider-added:
	 * @self: an #ideCompletion
	 * @provider: a #GtkSourceCompletionProvider
	 *
	 * The "provided-added" signal is emitted when a new provider is
	 * added to the completion.
	 */
	signals [PROVIDER_ADDED] =
		g_signal_new ("provider-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GTK_SOURCE_TYPE_COMPLETION_PROVIDER);
	g_signal_set_va_marshaller (signals [PROVIDER_ADDED],
				    G_TYPE_FROM_CLASS (klass),
				    g_cclosure_marshal_VOID__OBJECTv);

	/**
	 * GtkSourceCompletion::provider-removed:
	 * @self: an #ideCompletion
	 * @provider: a #GtkSourceCompletionProvider
	 *
	 * The "provided-removed" signal is emitted when a provider has
	 * been removed from the completion.
	 */
	signals [PROVIDER_REMOVED] =
		g_signal_new ("provider-removed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GTK_SOURCE_TYPE_COMPLETION_PROVIDER);
	g_signal_set_va_marshaller (signals [PROVIDER_REMOVED],
				    G_TYPE_FROM_CLASS (klass),
				    g_cclosure_marshal_VOID__OBJECTv);

	/**
	 * GtkSourceCompletion::hide:
	 * @self: a #GtkSourceCompletion
	 *
	 * The "hide" signal is emitted when the completion window should
	 * be hidden.
	 */
	signals [HIDE] =
		g_signal_new_class_handler ("hide",
					    G_TYPE_FROM_CLASS (klass),
					    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					    G_CALLBACK (gtk_source_completion_real_hide),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__VOID,
					    G_TYPE_NONE, 0);
	g_signal_set_va_marshaller (signals [HIDE],
				    G_TYPE_FROM_CLASS (klass),
				    g_cclosure_marshal_VOID__VOIDv);

	/**
	 * GtkSourceCompletion::show:
	 * @self: a #GtkSourceCompletion
	 *
	 * The "show" signal is emitted when the completion window should
	 * be shown.
	 */
	signals [SHOW] =
		g_signal_new_class_handler ("show",
					    G_TYPE_FROM_CLASS (klass),
					    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					    G_CALLBACK (gtk_source_completion_real_show),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__VOID,
					    G_TYPE_NONE, 0);
	g_signal_set_va_marshaller (signals [SHOW],
				    G_TYPE_FROM_CLASS (klass),
				    g_cclosure_marshal_VOID__VOIDv);
}

static void
gtk_source_completion_init (GtkSourceCompletion *self)
{
	self->cancellable = g_cancellable_new ();
	self->providers = g_ptr_array_new_with_free_func (g_object_unref);
	self->buffer_signals = g_signal_group_new (GTK_TYPE_TEXT_BUFFER);
	self->context_signals = g_signal_group_new (GTK_SOURCE_TYPE_COMPLETION_CONTEXT);
	self->view_signals = g_signal_group_new (GTK_SOURCE_TYPE_VIEW);
	self->page_size = DEFAULT_PAGE_SIZE;
	self->show_icons = TRUE;

	/*
	 * We want to be notified when the context switches from no results to
	 * having results (or vice-versa, when we've filtered to the point of
	 * no results).
	 */
	g_signal_group_connect_object (self->context_signals,
	                               "notify::empty",
	                               G_CALLBACK (gtk_source_completion_notify_context_empty_cb),
	                               self,
	                               G_CONNECT_SWAPPED);

	/*
	 * We need to know when the buffer inserts or deletes text so that we
	 * possibly start showing the results, or update our previous completion
	 * request.
	 */
	g_signal_connect_object (self->buffer_signals,
	                         "bind",
	                         G_CALLBACK (on_buffer_signals_bind),
	                         self,
	                         G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->buffer_signals,
	                               "delete-range",
	                               G_CALLBACK (gtk_source_completion_buffer_delete_range_after_cb),
	                               self,
	                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->buffer_signals,
	                               "insert-text",
	                               G_CALLBACK (gtk_source_completion_buffer_insert_text_after_cb),
	                               self,
	                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->buffer_signals,
	                               "mark-set",
	                               G_CALLBACK (gtk_source_completion_buffer_mark_set_cb),
	                               self,
	                               G_CONNECT_SWAPPED);

	/*
	 * We track some events on the view that owns our GtkSourceCompletion instance so
	 * that we can hide the window when it definitely should not be displayed.
	 */
	g_signal_group_connect_object (self->view_signals,
	                               "move-cursor",
	                               G_CALLBACK (gtk_source_completion_view_move_cursor_cb),
	                               self,
	                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->view_signals,
	                               "paste-clipboard",
	                               G_CALLBACK (gtk_source_completion_block_interactive),
	                               self,
	                               G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->view_signals,
	                               "paste-clipboard",
	                               G_CALLBACK (gtk_source_completion_unblock_interactive),
	                               self,
	                               G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

/**
 * gtk_source_completion_get_view:
 * @self: a #GtkSourceCompletion
 *
 * Gets the [class@View] that owns the [class@Completion].
 *
 * Returns: (transfer none): A #GtkSourceView
 */
GtkSourceView *
gtk_source_completion_get_view (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), NULL);

	return self->view;
}

/**
 * gtk_source_completion_get_buffer:
 * @self: a #GtkSourceCompletion
 *
 * Gets the connected [class@View]'s [class@Buffer]
 *
 * Returns: (transfer none): A #GtkSourceBuffer
 */
GtkSourceBuffer *
gtk_source_completion_get_buffer (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), NULL);

	return GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));
}

/**
 * gtk_source_completion_add_provider:
 * @self: a #GtkSourceCompletion
 * @provider: a #GtkSourceCompletionProvider
 *
 * Adds a [iface@CompletionProvider] to the list of providers to be queried
 * for completion results.
 */
void
gtk_source_completion_add_provider (GtkSourceCompletion         *self,
                                    GtkSourceCompletionProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));

	g_ptr_array_add (self->providers, g_object_ref (provider));
	g_signal_emit (self, signals [PROVIDER_ADDED], 0, provider);
}

/**
 * gtk_source_completion_remove_provider:
 * @self: a #GtkSourceCompletion
 * @provider: a #GtkSourceCompletionProvider
 *
 * Removes a [iface@CompletionProvider] previously added with
 * [method@Completion.add_provider].
 */
void
gtk_source_completion_remove_provider (GtkSourceCompletion         *self,
                                       GtkSourceCompletionProvider *provider)
{
	GtkSourceCompletionProvider *hold = NULL;

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));

	hold = g_object_ref (provider);

	if (g_ptr_array_remove (self->providers, provider))
	{
		g_signal_emit (self, signals [PROVIDER_REMOVED], 0, hold);
	}

	g_clear_object (&hold);
}

/**
 * gtk_source_completion_show:
 * @self: a #GtkSourceCompletion
 *
 * Emits the "show" signal.
 *
 * When the "show" signal is emitted, the completion window will be
 * displayed if there are any results available.
 */
void
gtk_source_completion_show (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	if (gtk_source_completion_is_blocked (self))
		return;

	self->showing++;
	if (self->showing == 1)
		g_signal_emit (self, signals [SHOW], 0);
	self->showing--;
}

/**
 * gtk_source_completion_hide:
 * @self: a #GtkSourceCompletion
 *
 * Emits the "hide" signal.
 *
 * When the "hide" signal is emitted, the completion window will be
 * dismissed.
 */
void
gtk_source_completion_hide (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	g_signal_emit (self, signals [HIDE], 0);
}

void
gtk_source_completion_block_interactive (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	self->block_count++;

	gtk_source_completion_cancel (self);
}

void
gtk_source_completion_unblock_interactive (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	self->block_count--;
}

void
gtk_source_completion_set_page_size (GtkSourceCompletion *self,
                                     guint                page_size)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));
	g_return_if_fail (page_size > 0);
	g_return_if_fail (page_size <= 32);

	if (self->page_size != page_size)
	{
		self->page_size = page_size;
		if (self->display != NULL)
			_gtk_source_completion_list_set_n_rows (self->display, page_size);
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PAGE_SIZE]);
	}
}

guint
gtk_source_completion_get_page_size (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), 0);

	return self->page_size;
}

void
_gtk_source_completion_activate (GtkSourceCompletion         *self,
                                 GtkSourceCompletionContext  *context,
                                 GtkSourceCompletionProvider *provider,
                                 GtkSourceCompletionProposal *proposal)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

	self->block_count++;

	gtk_source_completion_provider_activate (provider, context, proposal);
	gtk_source_completion_hide (self);
	g_clear_object (&self->context);
	_gtk_source_completion_list_set_context (self->display, NULL);

	self->block_count--;
}

GtkSourceCompletionList *
_gtk_source_completion_get_display (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), NULL);

	if (self->display == NULL)
	{
		self->display = _gtk_source_completion_list_new ();
		_gtk_source_completion_list_set_n_rows (self->display, self->page_size);
		_gtk_source_completion_list_set_font_desc (self->display, self->font_desc);
		_gtk_source_completion_list_set_show_icons (self->display, self->show_icons);
		_gtk_source_completion_list_set_remember_info_visibility (self->display,
		                                                          self->remember_info_visibility);
		_gtk_source_assistant_set_mark (GTK_SOURCE_ASSISTANT (self->display),
		                                self->completion_mark);
		_gtk_source_view_add_assistant (self->view,
		                                GTK_SOURCE_ASSISTANT (self->display));
		_gtk_source_completion_list_set_context (self->display, self->context);
	}

	return self->display;
}

/**
 * gtk_source_completion_fuzzy_match:
 * @haystack: (nullable): the string to be searched.
 * @casefold_needle: A g_utf8_casefold() version of the needle.
 * @priority: (out) (allow-none): An optional location for the score of the match
 *
 * This helper function can do a fuzzy match for you giving a haystack and
 * casefolded needle.
 *
 * Casefold your needle using [func@GLib.utf8_casefold] before
 * running the query.
 *
 * Score will be set with the score of the match upon success. Otherwise,
 * it will be set to zero.
 *
 * Returns: %TRUE if @haystack matched @casefold_needle, otherwise %FALSE.
 */
gboolean
gtk_source_completion_fuzzy_match (const char *haystack,
                                   const char *casefold_needle,
                                   guint      *priority)
{
	gint real_score = 0;

	if (haystack == NULL || haystack[0] == 0)
		return FALSE;

	for (; *casefold_needle; casefold_needle = g_utf8_next_char (casefold_needle))
	{
		gunichar ch = g_utf8_get_char (casefold_needle);
		gunichar chup = g_unichar_toupper (ch);
		const gchar *tmp;
		const gchar *downtmp;
		const gchar *uptmp;

		/*
		* Note that the following code is not really correct. We want
		* to be relatively fast here, but we also don't want to convert
		* strings to casefolded versions for querying on each compare.
		* So we use the casefold version and compare with upper. This
		* works relatively well since we are usually dealing with ASCII
		* for function names and symbols.
		*/

		downtmp = strchr (haystack, ch);
		uptmp = strchr (haystack, chup);

		if (downtmp && uptmp)
			tmp = MIN (downtmp, uptmp);
		else if (downtmp)
			tmp = downtmp;
		else if (uptmp)
			tmp = uptmp;
		else
			return FALSE;

		/*
		 * Here we calculate the cost of this character into the score.
		 * If we matched exactly on the next character, the cost is ZERO.
		 * However, if we had to skip some characters, we have a cost
		 * of 2*distance to the character. This is necessary so that
		 * when we add the cost of the remaining haystack, strings which
		 * exhausted @casefold_needle score lower (higher priority) than
		 * strings which had to skip characters but matched the same
		 * number of characters in the string.
		 */
		real_score += (tmp - haystack) * 2;

		/* Add extra cost if we matched by using toupper */
		if ((gunichar)*haystack == chup)
			real_score += 1;

		/*
		 * * Now move past our matching character so we cannot match
		 * * it a second time.
		 * */
		haystack = tmp + 1;
	}

	if (priority != NULL)
		*priority = real_score + strlen (haystack);

	return TRUE;
}

static void
add_attributes (PangoAttrList **attrs,
                guint           begin,
                guint           end)
{
	PangoAttribute *attr;

	if (*attrs == NULL)
	{
		*attrs = pango_attr_list_new ();
	}

	attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE_LINE);
	attr->start_index = begin;
	attr->end_index = end;
	pango_attr_list_insert (*attrs, g_steal_pointer (&attr));

	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	attr->start_index = begin;
	attr->end_index = end;
	pango_attr_list_insert (*attrs, g_steal_pointer (&attr));
}

/**
 * gtk_source_completion_fuzzy_highlight:
 * @haystack: the string to be highlighted
 * @casefold_query: the typed-text used to highlight @haystack
 *
 * This will add `<b>` tags around matched characters in @haystack
 * based on @casefold_query.
 *
 * Returns: (transfer full) (nullable): a #PangoAttrList or %NULL
 */
PangoAttrList *
gtk_source_completion_fuzzy_highlight (const char *haystack,
                                       const char *casefold_query)
{
	const char *real_haystack = haystack;
	PangoAttrList *attrs = NULL;
	gunichar str_ch;
	gunichar match_ch;
	gboolean element_open = FALSE;
	guint begin = 0;
	guint end = 0;

	if (haystack == NULL || casefold_query == NULL)
	{
		return NULL;
	}

	for (; *haystack; haystack = g_utf8_next_char (haystack))
	{
		str_ch = g_utf8_get_char (haystack);
		match_ch = g_utf8_get_char (casefold_query);

		if ((str_ch == match_ch) || (g_unichar_tolower (str_ch) == g_unichar_tolower (match_ch)))
		{
			if (!element_open)
			{
				begin = haystack - real_haystack;
				element_open = TRUE;
			}

			/* TODO: We could seek to the next char and append in a batch. */
			casefold_query = g_utf8_next_char (casefold_query);
		}
		else
		{
			if (element_open)
			{
				end = haystack - real_haystack;
				add_attributes (&attrs, begin, end);
				element_open = FALSE;
			}
		}
	}

	if (element_open)
	{
		end = haystack - real_haystack;
		add_attributes (&attrs, begin, end);
	}

	return g_steal_pointer (&attrs);
}

void
_gtk_source_completion_css_changed (GtkSourceCompletion *self,
                                    GtkCssStyleChange   *change)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));
	g_return_if_fail (change != NULL);

	g_clear_pointer (&self->font_desc, pango_font_description_free);
	self->font_desc = create_font_description (self);

	if (self->display != NULL)
	{
		_gtk_source_completion_list_set_font_desc (self->display, self->font_desc);
	}
}

gboolean
_gtk_source_completion_get_visible (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (self), FALSE);

	return self->display && gtk_widget_get_visible (GTK_WIDGET (self->display));
}

void
_gtk_source_completion_move_cursor (GtkSourceCompletion *self,
                                    GtkMovementStep      step,
                                    int                  direction)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION (self));

	if (self->display != NULL)
	{
		_gtk_source_completion_list_move_cursor (self->display, step, direction);
	}
}

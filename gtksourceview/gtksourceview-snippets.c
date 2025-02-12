/*
 * This file is part of GtkSourceView
 *
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
 */

#include "config.h"

#include <string.h>

#include "gtksourcebuffer.h"
#include "gtksourceiter-private.h"
#include "gtksourcelanguage.h"
#include "gtksourcesnippet-private.h"
#include "gtksourcesnippetchunk-private.h"
#include "gtksourcesnippetmanager.h"
#include "gtksourceview-private.h"
#include "gtksourceutils-private.h"

static void gtk_source_view_snippets_update_informative (GtkSourceViewSnippets *snippets);

static void
gtk_source_view_snippets_block (GtkSourceViewSnippets *snippets)
{
	g_assert (snippets != NULL);

	g_signal_handler_block (snippets->buffer,
	                        snippets->buffer_insert_text_handler);
	g_signal_handler_block (snippets->buffer,
	                        snippets->buffer_insert_text_after_handler);
	g_signal_handler_block (snippets->buffer,
	                        snippets->buffer_delete_range_handler);
	g_signal_handler_block (snippets->buffer,
	                        snippets->buffer_delete_range_after_handler);
	g_signal_handler_block (snippets->buffer,
	                        snippets->buffer_cursor_moved_handler);
}

static void
gtk_source_view_snippets_unblock (GtkSourceViewSnippets *snippets)
{
	g_assert (snippets != NULL);

	g_signal_handler_unblock (snippets->buffer,
	                          snippets->buffer_insert_text_handler);
	g_signal_handler_unblock (snippets->buffer,
	                          snippets->buffer_insert_text_after_handler);
	g_signal_handler_unblock (snippets->buffer,
	                          snippets->buffer_delete_range_handler);
	g_signal_handler_unblock (snippets->buffer,
	                          snippets->buffer_delete_range_after_handler);
	g_signal_handler_unblock (snippets->buffer,
	                          snippets->buffer_cursor_moved_handler);
}

static void
gtk_source_view_snippets_scroll_to_insert (GtkSourceViewSnippets *snippets)
{
	GtkTextMark *mark;
	GtkTextIter iter;
	GdkRectangle area;
	GdkRectangle visible_rect;
	guint top_margin;
	double x, y;

	g_assert (snippets != NULL);

	g_object_get (snippets->view,
		      "top-margin", &top_margin,
		      NULL);

	mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (snippets->buffer));
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (snippets->buffer), &iter, mark);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (snippets->view), &iter, &area);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (snippets->view), &visible_rect);

	if (area.x < visible_rect.x)
		x = area.x;
	else if (area.x > visible_rect.x + visible_rect.width)
		x = area.x - visible_rect.width;
	else
		x = visible_rect.x;

	if (area.y < visible_rect.y)
		y = area.y;
	else if (area.y > visible_rect.y + visible_rect.height)
		y = area.y - visible_rect.height;
	else
		y = visible_rect.y;

	gtk_adjustment_set_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (snippets->view)), x);
	gtk_adjustment_set_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (snippets->view)), y + top_margin);

	gtk_source_view_snippets_update_informative (snippets);
}

static void
buffer_insert_text_cb (GtkTextBuffer         *buffer,
                       GtkTextIter           *location,
                       const gchar           *text,
                       gint                   len,
                       GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *snippet;

	g_assert (GTK_IS_TEXT_BUFFER (buffer));
	g_assert (location != NULL);
	g_assert (text != NULL);
	g_assert (snippets != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippet != NULL)
	{
		/* We'll complete the user action in the after phase */
		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	}
}

static void
buffer_insert_text_after_cb (GtkTextBuffer         *buffer,
                             GtkTextIter           *location,
                             const gchar           *text,
                             gint                   len,
                             GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *snippet;

	g_assert (GTK_IS_TEXT_BUFFER (buffer));
	g_assert (location != NULL);
	g_assert (text != NULL);
	g_assert (snippets != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippet != NULL)
	{
		gtk_source_view_snippets_block (snippets);
		_gtk_source_snippet_after_insert_text (snippet,
		                                       GTK_TEXT_BUFFER (buffer),
		                                       location,
		                                       text,
		                                       len);
		gtk_source_view_snippets_unblock (snippets);

		/* Copmlete our action from the before phase */
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
	}
}

static void
buffer_delete_range_cb (GtkTextBuffer         *buffer,
                        GtkTextIter           *begin,
                        GtkTextIter           *end,
                        GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *snippet;

	g_assert (GTK_IS_TEXT_BUFFER (buffer));
	g_assert (begin != NULL);
	g_assert (end != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippet != NULL)
	{
		/* If the deletion will affect multiple chunks in the snippet,
		 * then we want to cancel all active snippets and go back to
		 * regular editing.
		 */
		if (_gtk_source_snippet_count_affected_chunks (snippet, begin, end) > 1)
		{
			_gtk_source_view_snippets_pop_all (snippets);
			return;
		}

		/* We'll complete the user action in the after phase */
		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	}
}

static void
buffer_delete_range_after_cb (GtkTextBuffer         *buffer,
                              GtkTextIter           *begin,
                              GtkTextIter           *end,
                              GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *snippet;

	g_assert (GTK_IS_TEXT_BUFFER (buffer));
	g_assert (begin != NULL);
	g_assert (end != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippet != NULL)
	{
		gtk_source_view_snippets_block (snippets);
		_gtk_source_snippet_after_delete_range (snippet,
		                                        GTK_TEXT_BUFFER (buffer),
		                                        begin,
		                                        end);
		gtk_source_view_snippets_unblock (snippets);

		/* Copmlete our action from the before phase */
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
	}
}

static void
buffer_cursor_moved_cb (GtkSourceBuffer       *buffer,
                        GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *snippet;

	g_assert (GTK_SOURCE_IS_BUFFER (buffer));
	g_assert (snippets != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippet != NULL)
	{
		GtkTextMark *insert;

		insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));

		while (snippet != NULL &&
		       !_gtk_source_snippet_insert_set (snippet, insert))
		{
			snippet = g_queue_pop_head (&snippets->queue);
			_gtk_source_snippet_finish (snippet);
			g_object_unref (snippet);

			snippet = g_queue_peek_head (&snippets->queue);
		}

		if (snippet == NULL)
		{
			if (snippets->informative != NULL)
			{
				gtk_widget_set_visible (GTK_WIDGET (snippets->informative), FALSE);
			}
		}
	}
}

void
_gtk_source_view_snippets_set_buffer (GtkSourceViewSnippets *snippets,
                                      GtkSourceBuffer       *buffer)
{
	g_assert (snippets != NULL);

	if (buffer == snippets->buffer)
	{
		return;
	}

	g_queue_clear_full (&snippets->queue, g_object_unref);

	g_clear_signal_handler (&snippets->buffer_insert_text_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_insert_text_after_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_delete_range_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_delete_range_after_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_cursor_moved_handler,
	                        snippets->buffer);

	snippets->buffer = NULL;

	if (GTK_SOURCE_IS_BUFFER (buffer))
	{
		snippets->buffer = buffer;
		snippets->buffer_insert_text_handler =
			g_signal_connect (snippets->buffer,
			                  "insert-text",
			                  G_CALLBACK (buffer_insert_text_cb),
			                  snippets);
		snippets->buffer_insert_text_after_handler =
			g_signal_connect_after (snippets->buffer,
			                        "insert-text",
			                        G_CALLBACK (buffer_insert_text_after_cb),
			                        snippets);
		snippets->buffer_delete_range_handler =
			g_signal_connect (snippets->buffer,
			                  "delete-range",
			                  G_CALLBACK (buffer_delete_range_cb),
			                  snippets);
		snippets->buffer_delete_range_after_handler =
			g_signal_connect_after (snippets->buffer,
			                        "delete-range",
			                        G_CALLBACK (buffer_delete_range_after_cb),
			                        snippets);
		snippets->buffer_cursor_moved_handler =
			g_signal_connect_after (snippets->buffer,
			                        "cursor-moved",
			                        G_CALLBACK (buffer_cursor_moved_cb),
			                        snippets);
	}
}

static void
gtk_source_view_snippets_update_informative (GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippetChunk *chunk;
	GtkSourceSnippet *snippet;
	const char *tooltip_text;
	int position;

	g_assert (snippets != NULL);

	snippet = g_queue_peek_head (&snippets->queue);

	if (snippets->view == NULL || snippet == NULL)
	{
		goto hide_informative;
	}

	position = gtk_source_snippet_get_focus_position (snippet);

	if (position < 0)
	{
		goto hide_informative;
	}

	chunk = snippet->current_chunk;

	if (chunk == NULL)
	{
		goto hide_informative;
	}

	tooltip_text = gtk_source_snippet_chunk_get_tooltip_text (chunk);

	if (tooltip_text == NULL || tooltip_text[0] == 0)
	{
		goto hide_informative;
	}

	if (snippets->informative == NULL)
	{
		snippets->informative = g_object_new (GTK_SOURCE_TYPE_INFORMATIVE,
		                                      "position", GTK_POS_TOP,
		                                      "message-type", GTK_MESSAGE_INFO,
		                                      "icon-name", "completion-snippet-symbolic",
		                                      NULL);
		_gtk_source_view_add_assistant (snippets->view,
		                                GTK_SOURCE_ASSISTANT (snippets->informative));
	}

	_gtk_source_assistant_set_mark (GTK_SOURCE_ASSISTANT (snippets->informative), chunk->begin_mark);
	gtk_source_informative_set_message (snippets->informative, tooltip_text);

	if (gtk_widget_get_visible (GTK_WIDGET (snippets->informative)))
	{
		_gtk_source_assistant_update_position (GTK_SOURCE_ASSISTANT (snippets->informative));
	}
	else
	{
		if (gtk_widget_get_mapped (GTK_WIDGET (snippets->view)))
		{
			gtk_widget_set_visible (GTK_WIDGET (snippets->informative), TRUE);
		}
	}

	return;

hide_informative:
	if (snippets->informative != NULL)
	{
		gtk_widget_set_visible (GTK_WIDGET (snippets->informative), FALSE);
	}
}

static void
gtk_source_view_snippets_notify_position_cb (GtkSourceViewSnippets *snippets,
                                             GParamSpec            *pspec,
                                             GtkSourceSnippet      *snippet)
{
	g_assert (snippets != NULL);
	g_assert (GTK_SOURCE_IS_VIEW (snippets->view));
	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	gtk_source_view_snippets_update_informative (snippets);
}

static void
gtk_source_view_snippets_bind_cb (GSignalGroup          *signals,
                                  GtkSourceSnippet      *snippet,
                                  GtkSourceViewSnippets *snippets)
{
	g_assert (snippets != NULL);
	g_assert (GTK_SOURCE_IS_VIEW (snippets->view));

	gtk_source_view_snippets_update_informative (snippets);
}

void
_gtk_source_view_snippets_init (GtkSourceViewSnippets *snippets,
                                GtkSourceView         *view)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (snippets != NULL);
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	memset (snippets, 0, sizeof *snippets);
	snippets->view = view;

	snippets->snippet_signals = g_signal_group_new (GTK_SOURCE_TYPE_SNIPPET);

	g_signal_connect (snippets->snippet_signals,
	                  "bind",
	                  G_CALLBACK (gtk_source_view_snippets_bind_cb),
	                  snippets);

	g_signal_group_connect_data (snippets->snippet_signals,
	                             "notify::focus-position",
	                             G_CALLBACK (gtk_source_view_snippets_notify_position_cb),
	                             snippets,
	                             NULL,
	                             G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	if (GTK_SOURCE_IS_BUFFER (buffer))
	{
		_gtk_source_view_snippets_set_buffer (snippets,
		                                      GTK_SOURCE_BUFFER (buffer));
	}
}

void
_gtk_source_view_snippets_shutdown (GtkSourceViewSnippets *snippets)
{
	g_assert (snippets != NULL);
	g_assert (snippets->view != NULL);

	g_queue_clear_full (&snippets->queue, g_object_unref);

	g_clear_signal_handler (&snippets->buffer_insert_text_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_insert_text_after_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_delete_range_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_delete_range_after_handler,
	                        snippets->buffer);
	g_clear_signal_handler (&snippets->buffer_cursor_moved_handler,
	                        snippets->buffer);

	if (snippets->informative != NULL)
	{
		_gtk_source_view_remove_assistant (snippets->view,
		                                   GTK_SOURCE_ASSISTANT (snippets->informative));
	}

	if (snippets->snippet_signals != NULL)
	{
		g_signal_group_set_target (snippets->snippet_signals, NULL);
		g_clear_object (&snippets->snippet_signals);
	}

	snippets->buffer = NULL;
	snippets->view = NULL;
}

static GtkSourceSnippet *
lookup_snippet_by_trigger (GtkSourceViewSnippets *snippets,
                           const gchar           *word)
{
	GtkSourceSnippetManager *manager;
	GtkSourceLanguage *language;
	const gchar *language_id = "";

	g_assert (snippets != NULL);
	g_assert (word != NULL);

	if (word[0] == 0)
	{
		return NULL;
	}

	manager = gtk_source_snippet_manager_get_default ();
	language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (snippets->buffer));

	if (language != NULL)
	{
		language_id = gtk_source_language_get_id (language);
	}

	return gtk_source_snippet_manager_get_snippet (manager, NULL, language_id, word);
}

static gboolean
gtk_source_view_snippets_try_expand (GtkSourceViewSnippets *snippets,
                                     GtkTextIter           *iter)
{
	GtkSourceSnippet *snippet;
	GtkTextIter begin;
	gchar *word;

	g_assert (snippets != NULL);
	g_assert (iter != NULL);

	if (gtk_text_iter_starts_line (iter) ||
	    !_gtk_source_iter_ends_full_word (iter))
	{
		return FALSE;
	}

	begin = *iter;

	_gtk_source_iter_backward_full_word_start (&begin);

	if (gtk_text_iter_compare (&begin, iter) >= 0)
	{
		return FALSE;
	}

	word = gtk_text_iter_get_slice (&begin, iter);

	if (word == NULL || *word == 0)
	{
		return FALSE;
	}

	snippet = lookup_snippet_by_trigger (snippets, word);

	g_free (word);

	if (snippet != NULL)
	{
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (snippets->buffer), &begin, iter);
		gtk_source_view_push_snippet (snippets->view, snippet, iter);
		g_object_unref (snippet);
		return TRUE;
	}

	return FALSE;
}

gboolean
_gtk_source_view_snippets_key_pressed (GtkSourceViewSnippets *snippets,
                                       guint                  key,
                                       guint                  keycode,
                                       GdkModifierType        state)
{
	GdkModifierType modifiers;
	gboolean editable;
	gboolean ret = GDK_EVENT_PROPAGATE;

	g_return_val_if_fail (snippets != NULL, FALSE);
	g_return_val_if_fail (snippets->view != NULL, FALSE);

	/* It's possible to get here even when GtkSourceView:enable-snippets
	 * is disabled because applications can also push snippets onto
	 * the view, such as with completion providers.
	 */

	if (snippets->buffer == NULL)
	{
		return GDK_EVENT_PROPAGATE;
	}

	/* Be careful when testing for modifier state equality:
	 * caps lock, num lock,etc need to be taken into account */
	modifiers = gtk_accelerator_get_default_mod_mask ();
	editable = gtk_text_view_get_editable (GTK_TEXT_VIEW (snippets->view));

	if ((key == GDK_KEY_Tab || key == GDK_KEY_KP_Tab || key == GDK_KEY_ISO_Left_Tab) &&
	    ((state & modifiers) == 0 ||
	     (state & modifiers) == GDK_SHIFT_MASK) &&
	    editable &&
	    gtk_text_view_get_accepts_tab (GTK_TEXT_VIEW (snippets->view)))
	{
		GtkSourceSnippet *snippet = g_queue_peek_head (&snippets->queue);
		GtkTextIter begin, end;
		gboolean has_selection;

		/* If we already have a snippet expanded, then we might need
		 * to move forward or backward between snippet positions.
		 */
		if (snippet != NULL)
		{
			_gtk_source_view_hide_completion (snippets->view);

			if ((state & modifiers) == 0)
			{
				if (!_gtk_source_snippet_move_next (snippet))
				{
					_gtk_source_view_snippets_pop (snippets);
				}

				gtk_source_view_snippets_scroll_to_insert (snippets);

				ret = GDK_EVENT_STOP;
				goto cleanup;
			}
			else if (state & GDK_SHIFT_MASK)
			{
				if (!_gtk_source_snippet_move_previous (snippet))
				{
					_gtk_source_view_snippets_pop (snippets);
				}

				gtk_source_view_snippets_scroll_to_insert (snippets);

				ret = GDK_EVENT_STOP;
				goto cleanup;
			}
		}

		has_selection = gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (snippets->buffer),
			                                              &begin, &end);

		/* tab: if there is no selection and the current word is a
		 * snippet trigger, then we should expand that snippet.
		 */
		if ((state & modifiers) == 0 &&
		    !has_selection &&
		    gtk_source_view_snippets_try_expand (snippets, &end))
		{
			_gtk_source_view_hide_completion (snippets->view);

			gtk_source_view_snippets_scroll_to_insert (snippets);

			ret = GDK_EVENT_STOP;
			goto cleanup;
		}
	}

cleanup:
	if (ret != GDK_EVENT_PROPAGATE && snippets->queue.length == 0)
	{
		if (snippets->informative)
		{
			gtk_widget_set_visible (GTK_WIDGET (snippets->informative), FALSE);
		}
	}

	return ret;
}

void
_gtk_source_view_snippets_push (GtkSourceViewSnippets *snippets,
                                GtkSourceSnippet      *snippet,
                                GtkTextIter           *iter)
{
	GtkTextMark *mark;
	gboolean more_to_focus;

	g_return_if_fail (snippets != NULL);
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (iter != NULL);

	if (snippets->buffer == NULL)
	{
		return;
	}

	g_queue_push_head (&snippets->queue, g_object_ref (snippet));

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (snippets->buffer));
	gtk_source_view_snippets_block (snippets);
	more_to_focus = _gtk_source_snippet_begin (snippet, snippets->buffer, iter);
	gtk_source_view_snippets_unblock (snippets);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (snippets->buffer));

	mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (snippets->buffer));
	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (snippets->view), mark);

	if (!more_to_focus)
	{
		_gtk_source_view_snippets_pop (snippets);
	}
	else
	{
		g_signal_group_set_target (snippets->snippet_signals, snippet);
	}
}

void
_gtk_source_view_snippets_pop (GtkSourceViewSnippets *snippets)
{
	GtkSourceSnippet *next_snippet;
	GtkSourceSnippet *snippet;

	g_return_if_fail (snippets != NULL);

	if (snippets->buffer == NULL)
	{
		return;
	}

	snippet = g_queue_pop_head (&snippets->queue);

	if (snippet != NULL)
	{
		_gtk_source_snippet_finish (snippet);

		next_snippet = g_queue_peek_head (&snippets->queue);

		if (next_snippet != NULL)
		{
			gchar *new_text;

			new_text = _gtk_source_snippet_get_edited_text (snippet);
			_gtk_source_snippet_replace_current_chunk_text (next_snippet, new_text);
			_gtk_source_snippet_move_next (next_snippet);

			g_free (new_text);
		}

		gtk_source_view_snippets_scroll_to_insert (snippets);

		g_object_unref (snippet);
	}

	snippet = g_queue_peek_head (&snippets->queue);
	g_signal_group_set_target (snippets->snippet_signals, snippet);

	if (snippet == NULL)
	{
		if (snippets->informative != NULL)
		{
			gtk_widget_set_visible (GTK_WIDGET (snippets->informative), FALSE);
		}
	}
}

void
_gtk_source_view_snippets_pop_all (GtkSourceViewSnippets *self)
{
	g_return_if_fail (self != NULL);

	while (self->queue.length > 0)
	{
		_gtk_source_view_snippets_pop (self);
	}
}

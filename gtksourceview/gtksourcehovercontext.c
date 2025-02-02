/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcebuffer.h"
#include "gtksourcehovercontext-private.h"
#include "gtksourcehoverdisplay.h"
#include "gtksourcehoverprovider.h"
#include "gtksourceview.h"

/**
 * GtkSourceHoverContext:
 *
 * Context for populating [class@HoverDisplay] contents.
 *
 * `GtkSourceHoverContext` contains information about the request to populate
 * contents for a [class@HoverDisplay].
 *
 * It can be used to retrieve the [class@View], [class@Buffer], and
 * [struct@Gtk.TextIter] for the regions of text which are being displayed.
 *
 * Use [method@HoverContext.get_bounds] to get the word that was
 * requested. [method@HoverContext.get_iter] will get you the location
 * of the pointer when the request was made.
 */

struct _GtkSourceHoverContext
{
	GObject          parent_instance;

	GtkSourceView   *view;
	GtkSourceBuffer *buffer;

	GPtrArray       *providers;

	GtkTextMark     *begin;
	GtkTextMark     *end;
	GtkTextMark     *location;
};

G_DEFINE_TYPE (GtkSourceHoverContext, gtk_source_hover_context, G_TYPE_OBJECT)

typedef struct
{
	guint n_active;
	guint n_success;
} Populate;

static void
gtk_source_hover_context_dispose (GObject *object)
{
	GtkSourceHoverContext *self = (GtkSourceHoverContext *)object;

	if (self->buffer != NULL)
	{
		if (self->begin != NULL)
			gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (self->buffer), self->begin);

		if (self->end != NULL)
			gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (self->buffer), self->end);

		if (self->location != NULL)
			gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (self->buffer), self->location);
	}

	g_clear_object (&self->begin);
	g_clear_object (&self->end);
	g_clear_object (&self->location);

	if (self->providers->len > 0)
	{
		g_ptr_array_remove_range (self->providers, 0, self->providers->len);
	}

	g_clear_weak_pointer (&self->buffer);
	g_clear_weak_pointer (&self->view);

	G_OBJECT_CLASS (gtk_source_hover_context_parent_class)->dispose (object);
}

static void
gtk_source_hover_context_finalize (GObject *object)
{
	GtkSourceHoverContext *self = (GtkSourceHoverContext *)object;

	g_clear_pointer (&self->providers, g_ptr_array_unref);

	G_OBJECT_CLASS (gtk_source_hover_context_parent_class)->finalize (object);
}

static void
gtk_source_hover_context_class_init (GtkSourceHoverContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_hover_context_dispose;
	object_class->finalize = gtk_source_hover_context_finalize;
}

static void
gtk_source_hover_context_init (GtkSourceHoverContext *self)
{
	self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}

void
_gtk_source_hover_context_add_provider (GtkSourceHoverContext  *self,
                                        GtkSourceHoverProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_PROVIDER (provider));

	for (guint i = 0; i < self->providers->len; i++)
	{
		if (g_ptr_array_index (self->providers, i) == (gpointer)provider)
		{
			return;
		}
	}

	g_ptr_array_add (self->providers, g_object_ref (provider));
}

/**
 * gtk_source_hover_context_get_view:
 * @self: a #GtkSourceHoverContext
 *
 * Returns: (transfer none): the #GtkSourceView that owns the context
 */
GtkSourceView *
gtk_source_hover_context_get_view (GtkSourceHoverContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self), NULL);

	return self->view;
}

/**
 * gtk_source_hover_context_get_buffer:
 * @self: a #GtkSourceHoverContext
 *
 * A convenience function to get the buffer.
 *
 * Returns: (transfer none): The #GtkSourceBuffer for the view
 */
GtkSourceBuffer *
gtk_source_hover_context_get_buffer (GtkSourceHoverContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self), NULL);
	g_return_val_if_fail (self->view != NULL, NULL);

	return GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->view)));
}

static GtkTextMark *
create_mark (GtkSourceHoverContext *self,
             const GtkTextIter     *iter,
             gboolean               left_gravity)
{
	GtkTextMark *mark;
	GtkTextBuffer *buffer;

	g_assert (GTK_SOURCE_IS_HOVER_CONTEXT (self));

	buffer = GTK_TEXT_BUFFER (self->buffer);
	mark = gtk_text_buffer_create_mark (buffer, NULL, iter, left_gravity);

	return g_object_ref (mark);
}

GtkSourceHoverContext *
_gtk_source_hover_context_new (GtkSourceView     *view,
                               const GtkTextIter *begin,
                               const GtkTextIter *end,
                               const GtkTextIter *location)
{
	GtkSourceHoverContext *self;
	GtkSourceBuffer *buffer;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);
	g_return_val_if_fail (begin != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);
	g_return_val_if_fail (location != NULL, NULL);

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	self = g_object_new (GTK_SOURCE_TYPE_HOVER_CONTEXT, NULL);

	g_set_weak_pointer (&self->view, view);
	g_set_weak_pointer (&self->buffer, buffer);

	self->begin = create_mark (self, begin, TRUE);
	self->end = create_mark (self, end, FALSE);
	self->location = create_mark (self, location, FALSE);

	return self;
}

static void
gtk_source_hover_context_populate_cb (GObject      *object,
                                      GAsyncResult *result,
                                      gpointer      user_data)
{
	GtkSourceHoverProvider *provider = (GtkSourceHoverProvider *)object;
	GTask *task = user_data;
	Populate *state;
	GError *error = NULL;

	g_assert (GTK_SOURCE_IS_HOVER_PROVIDER (provider));
	g_assert (G_IS_ASYNC_RESULT (result));
	g_assert (G_IS_TASK (task));

	state = g_task_get_task_data (task);

	if (!gtk_source_hover_provider_populate_finish (provider, result, &error))
	{
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
		{
			g_debug ("%s population failed", error->message);
		}

		g_clear_error (&error);
	}
	else
	{
		state->n_success++;
	}

	if (--state->n_active == 0)
	{
		if (state->n_success > 0)
		{
			g_task_return_boolean (task, TRUE);
		}
		else
		{
			g_task_return_new_error (task,
			                         G_IO_ERROR,
			                         G_IO_ERROR_NOT_SUPPORTED,
			                         "No hover providers populated the context");
		}
	}

	g_object_unref (task);
}

void
_gtk_source_hover_context_populate_async (GtkSourceHoverContext *self,
                                          GtkSourceHoverDisplay *display,
                                          GCancellable          *cancellable,
                                          GAsyncReadyCallback    callback,
                                          gpointer               user_data)
{
	Populate *state;
	GTask *task;

	g_return_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (display));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	state = g_new0 (Populate, 1);
	state->n_active = self->providers->len;

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gtk_source_hover_context_populate_async);
	g_task_set_task_data (task, state, g_free);

	if (self->view == NULL || self->buffer == NULL)
	{
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_CANCELLED,
		                         "Cannot populate, view destroyed");
	}
	else if (g_task_return_error_if_cancelled (task))
	{
		/* Do nothing */
	}
	else if (self->providers->len  == 0)
	{
		g_task_return_boolean (task, TRUE);
	}
	else
	{
		for (guint i = 0; i < self->providers->len; i++)
		{
			GtkSourceHoverProvider *provider = g_ptr_array_index (self->providers, i);

			g_assert (GTK_SOURCE_IS_HOVER_PROVIDER (provider));

			gtk_source_hover_provider_populate_async (provider,
			                                          self,
			                                          display,
			                                          cancellable,
			                                          gtk_source_hover_context_populate_cb,
			                                          g_object_ref (task));
		}
	}

	g_object_unref (task);
}

gboolean
_gtk_source_hover_context_populate_finish (GtkSourceHoverContext  *self,
                                           GAsyncResult           *result,
                                           GError                **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gtk_source_hover_context_get_iter:
 * @self: an #GtkSourceHoverContext
 * @iter: (out): a #GtkTextIter
 *
 * Gets the location of the pointer where the request was made.
 *
 * Returns: %TRUE if the mark is still valid and @iter was set.
 */
gboolean
gtk_source_hover_context_get_iter (GtkSourceHoverContext *self,
                                   GtkTextIter           *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (self->buffer == NULL)
		return FALSE;

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), iter, self->location);

	return TRUE;
}

/**
 * gtk_source_hover_context_get_bounds:
 * @self: an #GtkSourceHoverContext
 * @begin: (out) (optional): a #GtkTextIter
 * @end: (out) (optional): a #GtkTextIter
 *
 * Gets the current word bounds of the hover.
 *
 * If @begin is non-%NULL, it will be set to the start position of the
 * current word being hovered.
 *
 * If @end is non-%NULL, it will be set to the end position for the
 * current word being hovered.
 *
 * Returns: %TRUE if the marks are still valid and @begin or @end was set.
 */
gboolean
gtk_source_hover_context_get_bounds (GtkSourceHoverContext *self,
                                     GtkTextIter           *begin,
                                     GtkTextIter           *end)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (self), FALSE);

	if (self->buffer == NULL)
		return FALSE;

	if (begin != NULL)
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), begin, self->begin);

	if (end != NULL)
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (self->buffer), end, self->end);

	return TRUE;
}

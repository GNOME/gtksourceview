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
#include "gtksourceview.h"

#include "gtksourcevim.h"
#include "gtksourcevimcommand.h"
#include "gtksourcevimcommandbar.h"
#include "gtksourceviminsert.h"
#include "gtksourcevimnormal.h"
#include "gtksourcevimreplace.h"
#include "gtksourcevimvisual.h"

struct _GtkSourceVim
{
	GtkSourceVimState  parent_instance;
	GString           *command_text;
	GtkSourceBuffer   *buffer;
	guint              constrain_insert_source;
	guint              in_handle_event : 1;
};

G_DEFINE_TYPE (GtkSourceVim, gtk_source_vim, GTK_SOURCE_TYPE_VIM_STATE)

enum {
	PROP_0,
	PROP_COMMAND_TEXT,
	PROP_COMMAND_BAR_TEXT,
	N_PROPS
};

enum {
	EXECUTE_COMMAND,
	FORMAT,
	READY,
	SPLIT,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

GtkSourceVim *
gtk_source_vim_new (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	return g_object_new (GTK_SOURCE_TYPE_VIM,
	                     "view", view,
	                     NULL);
}

static gboolean
gtk_source_vim_handle_event (GtkSourceVimState *state,
                             GdkEvent          *event)
{
	GtkSourceVim *self = (GtkSourceVim *)state;
	GtkSourceVimState *current;
	gboolean ret = FALSE;

	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (event != NULL);

	self->in_handle_event = TRUE;

	g_clear_handle_id (&self->constrain_insert_source, g_source_remove);

	current = gtk_source_vim_state_get_current (state);
	if (current == state)
		goto finish;

	ret = gtk_source_vim_state_handle_event (current, event);

	g_string_truncate (self->command_text, 0);
	gtk_source_vim_state_append_command (state, self->command_text);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_TEXT]);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_BAR_TEXT]);

finish:
	self->in_handle_event = FALSE;

	return ret;
}

static gboolean
constrain_insert_source (gpointer data)
{
	GtkSourceVim *self = data;
	GtkSourceVimState *current;
	GtkSourceBuffer *buffer;
	GtkTextIter iter, selection;

	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (self->in_handle_event == FALSE);

	self->constrain_insert_source = 0;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (self));

	self->in_handle_event = TRUE;

	if (GTK_SOURCE_IS_VIM_VISUAL (current))
	{
		gtk_source_vim_visual_warp (GTK_SOURCE_VIM_VISUAL (current), &iter);
	}
	else if (!GTK_SOURCE_IS_VIM_INSERT (current) &&
	         !GTK_SOURCE_IS_VIM_REPLACE (current) &&
	         !gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
	{
		if (gtk_text_iter_ends_line (&iter) &&
		    !gtk_text_iter_starts_line (&iter))
		{
			gtk_text_iter_backward_char (&iter);
			gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
		}
	}
	else if (GTK_SOURCE_IS_VIM_NORMAL (current) &&
	         gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
	{
		GtkSourceVimState *visual;

		/* Enter visual mode */
		visual = gtk_source_vim_visual_new (GTK_SOURCE_VIM_VISUAL_CHAR);
		gtk_source_vim_state_push (current, visual);
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_TEXT]);
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_BAR_TEXT]);
	}

	self->in_handle_event = FALSE;

	return G_SOURCE_REMOVE;
}

static void
on_cursor_moved_cb (GtkSourceVim    *self,
                    GtkSourceBuffer *buffer)
{
	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));

	if (self->in_handle_event)
		return;

	/* Make sure we are placed on a character instead of on a \n
	 * which is possible when the user clicks with a button or other
	 * external tools. Don't do it until an idle callback though so
	 * that we don't affect anything currently processing.
	 */
	if (self->constrain_insert_source == 0)
	{
		self->constrain_insert_source =
			g_timeout_add_full (G_PRIORITY_LOW, 35,
			                    constrain_insert_source, self, NULL);
	}
}

static void
on_notify_buffer_cb (GtkSourceVim  *self,
                     GParamSpec    *pspec,
                     GtkSourceView *view)
{
	GtkSourceBuffer *buffer;

	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	if (self->buffer == buffer)
		return;

	if (self->buffer != NULL)
	{
		g_signal_handlers_disconnect_by_func (self->buffer,
		                                      G_CALLBACK (on_cursor_moved_cb),
		                                      self);
		g_clear_object (&self->buffer);
	}

	g_set_object (&self->buffer, buffer);

	if (buffer != NULL)
	{
		g_signal_connect_object (buffer,
		                         "cursor-moved",
		                         G_CALLBACK (on_cursor_moved_cb),
		                         self,
		                         G_CONNECT_SWAPPED);
		on_cursor_moved_cb (self, buffer);
	}
}

static void
gtk_source_vim_view_set (GtkSourceVimState *state)
{
	GtkSourceVim *self = (GtkSourceVim *)state;
	GtkSourceView *view;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (gtk_source_vim_state_get_child (state) == NULL);

	view = gtk_source_vim_state_get_view (state);
	gtk_source_vim_state_get_buffer (state, &iter, NULL);

	g_signal_connect_object (view,
				 "notify::buffer",
				 G_CALLBACK (on_notify_buffer_cb),
				 self,
				 G_CONNECT_SWAPPED);
	on_notify_buffer_cb (self, NULL, view);

	gtk_source_vim_state_push_jump (state, &iter);

	gtk_source_vim_state_push (state, gtk_source_vim_normal_new ());
}

static void
gtk_source_vim_resume (GtkSourceVimState *state,
                       GtkSourceVimState *from)
{
	GtkSourceView *view;

	g_assert (GTK_SOURCE_IS_VIM (state));

	if (!(view = gtk_source_vim_state_get_view (state)))
		return;

	gtk_text_view_set_overwrite (GTK_TEXT_VIEW (view), FALSE);
}

static void
gtk_source_vim_dispose (GObject *object)
{
	GtkSourceVim *self = (GtkSourceVim *)object;

	g_clear_handle_id (&self->constrain_insert_source, g_source_remove);
	g_clear_object (&self->buffer);

	if (self->command_text != NULL)
	{
		g_string_free (self->command_text, TRUE);
		self->command_text = NULL;
	}

	G_OBJECT_CLASS (gtk_source_vim_parent_class)->dispose (object);
}

static void
gtk_source_vim_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	GtkSourceVim *self = GTK_SOURCE_VIM (object);

	switch (prop_id)
	{
		case PROP_COMMAND_TEXT:
			g_value_set_string (value, gtk_source_vim_get_command_text (self));
			break;

		case PROP_COMMAND_BAR_TEXT:
			g_value_set_string (value, gtk_source_vim_get_command_bar_text (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_class_init (GtkSourceVimClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_dispose;
	object_class->get_property = gtk_source_vim_get_property;

	state_class->handle_event = gtk_source_vim_handle_event;
	state_class->view_set = gtk_source_vim_view_set;
	state_class->resume = gtk_source_vim_resume;

	properties [PROP_COMMAND_TEXT] =
		g_param_spec_string ("command-text",
		                     "Command Text",
		                     "Command Text",
		                     NULL,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_COMMAND_BAR_TEXT] =
		g_param_spec_string ("command-bar-text",
		                     "Command Bar Text",
		                     "Command Bar Text",
		                     NULL,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceVim::execute-command:
	 * @self: a #GtkSourceVim
	 * @command: the command to execute
	 *
	 * The "execute-command" signal is emitted when the user has requested
	 * a command to be executed from the command bar (or possibly other
	 * VIM commands internally).
	 *
	 * If the command is something GtkSourceVim can handle internally,
	 * it will do so. Otherwise the application is responsible for
	 * handling it.
	 *
	 * Returns: %TRUE if handled, otherwise %FALSE.
	 */
	signals[EXECUTE_COMMAND] =
		g_signal_new_class_handler ("execute-command",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST,
		                            NULL,
		                            g_signal_accumulator_true_handled, NULL,
		                            NULL,
		                            G_TYPE_BOOLEAN,
		                            1,
		                            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * GtkSourceVim::format:
	 * @self: a #GtkSourceVim
	 * @begin: the beginning of the text range
	 * @end: the end of the text range
	 *
	 * Requests that the text range @begin to @end be reformatted.
	 * Applications should conntect to this signal to implement
	 * reformatting as they would like.
	 */
	signals[FORMAT] =
		g_signal_new ("format",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0, NULL, NULL, NULL,
		              G_TYPE_NONE, 2, GTK_TYPE_TEXT_ITER, GTK_TYPE_TEXT_ITER);

	signals[READY] =
		g_signal_new ("ready",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 0);

	/**
	 * GtkSourceVim::split:
	 * @self: a #GtkSourceVim
	 * @orientation: a #GtkOrientation for vertical or horizontal
	 * @new_document: %TRUE if a new document should be created
	 * @focus_split: %TRUE if the new document should be focused
	 * @numeric: a numeric value provided with the command such as
	 *   the number of columns for the split
	 */
	signals[SPLIT] =
		g_signal_new ("split",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              3,
		              GTK_TYPE_ORIENTATION,
		              G_TYPE_BOOLEAN,
		              G_TYPE_BOOLEAN,
		              G_TYPE_INT);
}

static void
gtk_source_vim_init (GtkSourceVim *self)
{
	self->command_text = g_string_new (NULL);
}

const char *
gtk_source_vim_get_command_text (GtkSourceVim *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM (self), NULL);

	return self->command_text->str;
}

const char *
gtk_source_vim_get_command_bar_text (GtkSourceVim *self)
{
	GtkSourceVimState *current;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM (self), NULL);

	current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (self));

	while (current != NULL)
	{
		if (GTK_SOURCE_IS_VIM_COMMAND_BAR (current))
		{
			return gtk_source_vim_command_bar_get_text (GTK_SOURCE_VIM_COMMAND_BAR (current));
		}

		if (GTK_SOURCE_VIM_STATE_GET_CLASS (current)->get_command_bar_text)
		{
			return GTK_SOURCE_VIM_STATE_GET_CLASS (current)->get_command_bar_text (current);
		}

		if (GTK_SOURCE_VIM_STATE_GET_CLASS (current)->command_bar_text)
		{
			return GTK_SOURCE_VIM_STATE_GET_CLASS (current)->command_bar_text;
		}

		current = gtk_source_vim_state_get_parent (current);
	}

	return "";
}

void
gtk_source_vim_emit_split (GtkSourceVim   *self,
                           GtkOrientation  orientation,
                           gboolean        new_document,
                           gboolean        focus_split,
                           int             numeric)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM (self));

	g_signal_emit (self, signals[SPLIT], 0,
	               orientation, new_document, focus_split, numeric);
}

void
gtk_source_vim_reset (GtkSourceVim *self)
{
	GtkSourceVimState *current;

	g_return_if_fail (GTK_SOURCE_IS_VIM (self));

	/* Clear everything up to the top-most Normal mode */
	while ((current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (self))))
	{
		GtkSourceVimState *parent = gtk_source_vim_state_get_parent (current);

		if (parent == NULL || parent == GTK_SOURCE_VIM_STATE (self))
			break;

		gtk_source_vim_state_pop (current);
	}

	current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (self));

	/* If we found the normal mode (should always happen), then
	 * also tell it to clear anything in progress.
	 */
	if (GTK_SOURCE_IS_VIM_NORMAL (current))
	{
		gtk_source_vim_normal_clear (GTK_SOURCE_VIM_NORMAL (current));
	}
}

gboolean
gtk_source_vim_emit_execute_command (GtkSourceVim *self,
                                     const char   *command)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM (self), FALSE);

	g_signal_emit (self, signals[EXECUTE_COMMAND], 0, command, &ret);

	return ret;
}

void
gtk_source_vim_emit_ready (GtkSourceVim *self)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM (self));

	g_signal_emit (self, signals[READY], 0);
}

void
gtk_source_vim_emit_format (GtkSourceVim *self,
                            GtkTextIter  *begin,
                            GtkTextIter  *end)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM (self));
	g_return_if_fail (begin != NULL);
	g_return_if_fail (end != NULL);

	g_signal_emit (self, signals[FORMAT], 0, begin, end);
}

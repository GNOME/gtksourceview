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

#include "gtksourceindenter.h"
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
	GtkGestureClick   *click;
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
	FILTER,
	FORMAT,
	READY,
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
gtk_source_vim_real_format (GtkSourceVim *self,
                            GtkTextIter  *begin,
                            GtkTextIter  *end)
{
	return FALSE;
}

typedef struct _LineReader
{
  const char *contents;
  gsize       length;
  gssize      pos;
} LineReader;

static void
line_reader_init (LineReader *reader,
		  const char *contents,
		  gssize      length)
{
  if (length < 0)
    length = strlen (contents);

  if (contents != NULL)
    {
      reader->contents = contents;
      reader->length = length;
      reader->pos = 0;
    }
  else
    {
      reader->contents = NULL;
      reader->length = 0;
      reader->pos = 0;
    }
}

static const char *
line_reader_next (LineReader *reader,
		  gsize      *length)
{
  const char *ret = NULL;

  if ((reader->contents == NULL) || (reader->pos >= reader->length))
    {
      *length = 0;
      return NULL;
    }

  ret = &reader->contents [reader->pos];

  for (; reader->pos < reader->length; reader->pos++)
    {
      if (reader->contents [reader->pos] == '\n')
        {
          *length = &reader->contents [reader->pos] - ret;
          /* Ingore the \r in \r\n if provided */
          if (*length > 0 && reader->pos > 0 && reader->contents [reader->pos - 1] == '\r')
            (*length)--;
          reader->pos++;
          return ret;
        }
    }

  *length = &reader->contents [reader->pos] - ret;

  return ret;
}

static gboolean
gtk_source_vim_real_filter (GtkSourceVim *self,
                            GtkTextIter  *begin,
                            GtkTextIter  *end)
{
	GtkSourceIndenter *indenter;
	GtkSourceView *view;
	GtkTextBuffer *buffer;
	GtkTextMark *begin_mark;
	GtkTextMark *end_mark;
	GtkTextIter iter;
	LineReader reader;
	const char *line;
	char *text;
	gsize line_len;
	guint count = 0;

	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (begin != NULL);
	g_assert (end != NULL);

	buffer = gtk_text_iter_get_buffer (begin);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	/* If there is no indenter, we can't do anything here */
	if (!(indenter = gtk_source_view_get_indenter (view)))
	{
		return FALSE;
	}

	gtk_text_iter_order (begin, end);

	/* Remove trialing \n which might have happened from linewise */
	if (gtk_text_iter_starts_line (end) &&
	    gtk_text_iter_get_line (begin) != gtk_text_iter_get_line (end))
	{
		gtk_text_iter_backward_char (end);
	}

	if (!gtk_text_iter_starts_line (begin))
	{
		gtk_text_iter_set_line_offset (begin, 0);
	}

	if (!gtk_text_iter_ends_line (end))
	{
		gtk_text_iter_forward_to_line_end (end);
	}

	if (gtk_text_iter_equal (begin, end))
	{
		return FALSE;
	}

	/* Create temporary marks for bounds checking */
	begin_mark = gtk_text_buffer_create_mark (buffer, NULL, begin, TRUE);
	end_mark = gtk_text_buffer_create_mark (buffer, NULL, end, FALSE);

	/* Remove all text, as if we try to do this within the buffer it has
	 * the chance to really hammer applications which process events on
	 * buffer changes.
	 */
	text = gtk_text_iter_get_slice (begin, end);
	gtk_text_buffer_delete (buffer, begin, end);

	iter = *begin;
	line_reader_init (&reader, text, -1);
	while ((line = line_reader_next (&reader, &line_len)))
	{
		char *stripped = g_strstrip (g_strndup (line, line_len));
		guint offset;

		if (count > 0)
		{
			gtk_text_buffer_insert (buffer, &iter, "\n", -1);
		}

		offset = gtk_text_iter_get_offset (&iter);
		gtk_text_buffer_insert (buffer, &iter, stripped, -1);
		gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);

		gtk_source_indenter_indent (indenter, view, &iter);

		if (!gtk_text_iter_ends_line (&iter))
		{
			gtk_text_iter_forward_to_line_end (&iter);
		}

		count++;

		g_free (stripped);
	}

	/* Revalidate iter positions */
	gtk_text_buffer_get_iter_at_mark (buffer, begin, begin_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, end, end_mark);

	/* Remove our temporary marks */
	gtk_text_buffer_delete_mark (buffer, begin_mark);
	gtk_text_buffer_delete_mark (buffer, end_mark);

	g_free (text);

	return TRUE;
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
		gtk_source_vim_visual_warp (GTK_SOURCE_VIM_VISUAL (current), &iter, &selection);
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

		/* Try to emulate the selection as if it happened in Visual mode
		 * by first clearing the selection and then warp visual mode
		 * to where the insert cursor is now.
		 */
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &selection, &selection);
		visual = gtk_source_vim_visual_new (GTK_SOURCE_VIM_VISUAL_CHAR);
		gtk_source_vim_state_push (current, visual);
		gtk_source_vim_visual_warp (GTK_SOURCE_VIM_VISUAL (visual), &iter, &selection);
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_TEXT]);
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_BAR_TEXT]);
	}

	self->in_handle_event = FALSE;

	return G_SOURCE_REMOVE;
}

static void
queue_constrain (GtkSourceVim *self)
{
	g_assert (GTK_SOURCE_IS_VIM (self));

	if (self->in_handle_event)
		return;

	/* Make sure we are placed on a character instead of on a \n
	 * which is possible when the user clicks with a button or other
	 * external tools. Don't do it until an idle callback though so
	 * that we don't affect anything currently processing.
	 */
	if (self->constrain_insert_source == 0)
	{
		self->constrain_insert_source = g_idle_add (constrain_insert_source, self);
	}
}

static void
on_click_released_cb (GtkSourceVim    *self,
                      int              n_press,
                      double           x,
                      double           y,
                      GtkGestureClick *click)
{
	g_assert (GTK_SOURCE_IS_VIM (self));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	if (n_press == 1)
	{
		queue_constrain (self);
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

	self->click = GTK_GESTURE_CLICK (gtk_gesture_click_new ());
	g_signal_connect_object (self->click,
	                         "released",
	                         G_CALLBACK (on_click_released_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (view),
	                           GTK_EVENT_CONTROLLER (self->click));

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
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	if (view != NULL && self->click)
	{
		gtk_widget_remove_controller (GTK_WIDGET (view),
		                              GTK_EVENT_CONTROLLER (self->click));
		self->click = NULL;
	}

	g_clear_handle_id (&self->constrain_insert_source, g_source_remove);

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
	 * Requests that the text range @begin to @end be formatted.
	 *
	 * This is equivalent to the `gq` command in Vim.
	 *
	 * Applications should conntect to this signal to implement
	 * formatting as they would like.
	 *
	 * Returns: %TRUE if the format request was handled; otherwise %FALSE
	 */
	signals[FORMAT] =
		g_signal_new_class_handler ("format",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST,
		                            G_CALLBACK (gtk_source_vim_real_format),
		                            g_signal_accumulator_true_handled, NULL,
		                            NULL,
		                            G_TYPE_BOOLEAN,
		                            2,
		                            GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		                            GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * GtkSourceVim::filter:
	 * @self: a #GtkSourceVim
	 * @begin: the beginning of the text range
	 * @end: the end of the text range
	 *
	 * Requests that the text range @begin to @end be filtered (transformed
	 * in some way and replaced).
	 *
	 * Applications should conntect to this signal to implement
	 * filtering as they would like.
	 *
	 * The default handler will attempt to filter by using the
	 * #GtkSourceView's indenter to reindent each line.
	 *
	 * In the future, some effort may be made to restrict line width for
	 * languages and contexts which are known to be safe.
	 *
	 * Returns: %TRUE if the filter request was handled; otherwise %FALSE
	 */
	signals[FILTER] =
		g_signal_new_class_handler ("filter",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST,
		                            G_CALLBACK (gtk_source_vim_real_filter),
		                            g_signal_accumulator_true_handled, NULL,
		                            NULL,
		                            G_TYPE_BOOLEAN,
		                            2,
		                            GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		                            GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);

	signals[READY] =
		g_signal_new ("ready",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 0);
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

gboolean
gtk_source_vim_emit_filter (GtkSourceVim *self,
                            GtkTextIter  *begin,
                            GtkTextIter  *end)
{
	gboolean ret;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM (self), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	gtk_text_iter_order (begin, end);

	g_signal_emit (self, signals[FILTER], 0, begin, end, &ret);

	return ret;
}

gboolean
gtk_source_vim_emit_format (GtkSourceVim *self,
                            GtkTextIter  *begin,
                            GtkTextIter  *end)
{
	gboolean ret;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM (self), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	gtk_text_iter_order (begin, end);

	g_signal_emit (self, signals[FORMAT], 0, begin, end, &ret);

	return ret;
}

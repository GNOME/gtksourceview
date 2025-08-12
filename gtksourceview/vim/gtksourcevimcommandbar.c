/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021-2022 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcevim.h"
#include "gtksourcevimcommand.h"
#include "gtksourcevimcommandbar.h"

#define MAX_HISTORY 25

struct _GtkSourceVimCommandBar
{
	GtkSourceVimState parent_instance;
	GtkSourceVimCommand *command;
	GString *buffer;
	char *typed;
	int history_pos;
};

G_DEFINE_TYPE (GtkSourceVimCommandBar, gtk_source_vim_command_bar, GTK_SOURCE_TYPE_VIM_STATE)

static GPtrArray *history;

GtkSourceVimState *
gtk_source_vim_command_bar_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_COMMAND_BAR, NULL);
}

GtkSourceVimState *
gtk_source_vim_command_bar_take_command (GtkSourceVimCommandBar *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_COMMAND_BAR (self), NULL);

	return GTK_SOURCE_VIM_STATE (g_steal_pointer (&self->command));
}

static void
gtk_source_vim_command_bar_dispose (GObject *object)
{
	GtkSourceVimCommandBar *self = (GtkSourceVimCommandBar *)object;

	g_clear_pointer (&self->typed, g_free);

	if (self->buffer != NULL)
	{
		g_string_free (self->buffer, TRUE);
		self->buffer = NULL;
	}

	g_clear_object (&self->command);

	G_OBJECT_CLASS (gtk_source_vim_command_bar_parent_class)->dispose (object);
}

static void
do_notify (GtkSourceVimCommandBar *self)
{
	GtkSourceVimState *root;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));

	root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self));

	if (GTK_SOURCE_IS_VIM (root))
	{
		g_object_notify (G_OBJECT (root), "command-bar-text");
	}
}

static void
move_history (GtkSourceVimCommandBar *self,
              int                     direction)
{
	int position;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));

	if (history->len == 0)
	{
		return;
	}

	if (self->typed == NULL && self->buffer->len > 0)
	{
		self->typed = g_strdup (self->buffer->str);
	}

	direction = direction < 0 ? -1 : 1;
	position = self->history_pos + direction;

	while (position >= 0 && position < history->len)
	{
		const char *item = g_ptr_array_index (history, position);

		if (self->typed == NULL || g_str_has_prefix (item, self->typed))
		{
			self->history_pos = position;
			g_string_truncate (self->buffer, 0);
			g_string_append (self->buffer, item);
			return;
		}

		position += direction;
	}

	/* Reset to typed text if we exhausted the tail of history */
	if (position >= history->len && self->typed != NULL)
	{
		self->history_pos = history->len;
		g_string_truncate (self->buffer, 0);
		g_string_append (self->buffer, self->typed);
	}
}

static void
complete_command (GtkSourceVimCommandBar *self,
                  const char             *prefix)
{
	static const char *commands[] = {
		":colorscheme",
		":write",
		":quit",
		":edit",
		":open",
		":file",
		":set",
	};

	g_assert (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));

	for (guint i = 0; i < G_N_ELEMENTS (commands); i++)
	{
		if (g_str_has_prefix (commands[i], prefix))
		{
			g_string_truncate (self->buffer, 0);
			g_string_append (self->buffer, commands[i]);
			g_string_append_c (self->buffer, ' ');
			break;
		}
	}
}

static void
do_execute (GtkSourceVimCommandBar *self,
            const char             *command)
{
	GtkSourceVimState *root;
	GtkSourceVimState *new_state;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));
	g_assert (command != NULL);

	if (history->len > MAX_HISTORY)
	{
		g_ptr_array_remove_range (history, 0, history->len - MAX_HISTORY);
	}

	g_ptr_array_add (history, g_strdup (command));

	root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self));

	if (GTK_SOURCE_IS_VIM (root))
	{
		if (gtk_source_vim_emit_execute_command (GTK_SOURCE_VIM (root), command))
			return;
	}

	if (!(new_state = gtk_source_vim_command_new_parsed (GTK_SOURCE_VIM_STATE (self), command)))
		return;

	gtk_source_vim_state_reparent (new_state, self, &self->command);
	gtk_source_vim_state_repeat (new_state);
	g_object_unref (new_state);
}

static gboolean
gtk_source_vim_command_bar_handle_keypress (GtkSourceVimState *state,
                                            guint              keyval,
                                            guint              keycode,
                                            GdkModifierType    mods,
                                            const char        *str)
{
	GtkSourceVimCommandBar *self = (GtkSourceVimCommandBar *)state;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));

	if (gtk_source_vim_state_is_escape (keyval, mods))
	{
		g_string_truncate (self->buffer, 0);
		do_notify (self);
		gtk_source_vim_state_pop (state);
		return TRUE;
	}

	switch (keyval)
	{
		case GDK_KEY_BackSpace:
		{
			gsize len = g_utf8_strlen (self->buffer->str, -1);

			g_clear_pointer (&self->typed, g_free);

			if (len > 1)
			{
				char *s = g_utf8_offset_to_pointer (self->buffer->str, len-1);
				g_string_truncate (self->buffer, s - self->buffer->str);
				do_notify (self);
			}

			return TRUE;
		}

		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
			complete_command (self, self->buffer->str);
			return TRUE;

		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
			move_history (self, -1);
			return TRUE;

		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
			move_history (self, 1);
			return TRUE;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			g_clear_pointer (&self->typed, g_free);
			do_execute (self, self->buffer->str);
			g_string_truncate (self->buffer, 0);
			do_notify (self);
			gtk_source_vim_state_pop (state);
			return TRUE;

		case GDK_KEY_u:
			if (mods & GDK_CONTROL_MASK)
			{
				g_clear_pointer (&self->typed, g_free);
				g_string_truncate (self->buffer, 1);
				do_notify (self);
				return TRUE;
			}

			break;

		default:
			break;
	}

	if (str[0])
	{
		g_string_append (self->buffer, str);
		g_clear_pointer (&self->typed, g_free);
		do_notify (self);
	}

	return TRUE;
}

static void
gtk_source_vim_command_bar_enter (GtkSourceVimState *state)
{
	GtkSourceVimCommandBar *self = (GtkSourceVimCommandBar *)state;
	GtkSourceView *view;

	g_assert (GTK_SOURCE_VIM_STATE (self));

	self->history_pos = history->len;

	if (self->buffer->len == 0)
	{
		g_string_append_c (self->buffer, ':');
		do_notify (self);
	}

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
}

static void
gtk_source_vim_command_bar_leave (GtkSourceVimState *state)
{
	GtkSourceVimCommandBar *self = (GtkSourceVimCommandBar *)state;
	GtkSourceView *view;

	g_assert (GTK_SOURCE_VIM_STATE (self));

	self->history_pos = 0;

	g_clear_pointer (&self->typed, g_free);
	g_string_truncate (self->buffer, 0);
	do_notify (self);

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), TRUE);
}

static void
gtk_source_vim_command_bar_append_command (GtkSourceVimState *state,
                                           GString           *command_text)
{
	g_string_truncate (command_text, 0);
}

static void
gtk_source_vim_command_bar_class_init (GtkSourceVimCommandBarClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_command_bar_dispose;

	state_class->append_command = gtk_source_vim_command_bar_append_command;
	state_class->enter = gtk_source_vim_command_bar_enter;
	state_class->leave = gtk_source_vim_command_bar_leave;
	state_class->handle_keypress = gtk_source_vim_command_bar_handle_keypress;

	history = g_ptr_array_new_with_free_func (g_free);
}

static void
gtk_source_vim_command_bar_init (GtkSourceVimCommandBar *self)
{
	self->buffer = g_string_new (NULL);
}

const char *
gtk_source_vim_command_bar_get_text (GtkSourceVimCommandBar *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_COMMAND_BAR (self), NULL);

	return self->buffer->str;
}

void
gtk_source_vim_command_bar_set_text (GtkSourceVimCommandBar *self,
                                     const char             *text)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_COMMAND_BAR (self));

	g_string_truncate (self->buffer, 0);
	g_string_append (self->buffer, text);

	do_notify (self);
}

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

#include <glib/gi18n.h>

#include "gtksourcevimreplace.h"
#include "gtksourceviminsertliteral.h"

struct _GtkSourceVimReplace
{
	GtkSourceVimState parent_instance;
};

G_DEFINE_TYPE (GtkSourceVimReplace, gtk_source_vim_replace, GTK_SOURCE_TYPE_VIM_STATE)

GtkSourceVimState *
gtk_source_vim_replace_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_REPLACE, NULL);
}

static void
move_to_zero (GtkSourceVimReplace *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter insert;

	g_assert (GTK_SOURCE_IS_VIM_REPLACE (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &insert, NULL);
	gtk_text_iter_set_line_offset (&insert, 0);
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &insert, &insert);
}

static gboolean
gtk_source_vim_replace_handle_keypress (GtkSourceVimState *state,
                                        guint              keyval,
                                        guint              keycode,
                                        GdkModifierType    mods,
                                        const char        *string)
{
	GtkSourceVimReplace *self = (GtkSourceVimReplace *)state;

	g_assert (GTK_SOURCE_IS_VIM_REPLACE (self));

	if (gtk_source_vim_state_is_escape (keyval, mods) ||
	    gtk_source_vim_state_is_ctrl_c (keyval, mods))
	{
		gtk_source_vim_state_pop (state);
		return TRUE;
	}

	/* Now handle our commands */
	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_u:
				move_to_zero (self);
				return TRUE;

			case GDK_KEY_v:
				gtk_source_vim_state_push (state, gtk_source_vim_insert_literal_new ());
				return TRUE;

			default:
				break;
		}
	}

	return FALSE;
}

static void
gtk_source_vim_replace_enter (GtkSourceVimState *state)
{
	g_assert (GTK_SOURCE_IS_VIM_REPLACE (state));

	gtk_source_vim_state_set_overwrite (state, TRUE);
	gtk_source_vim_state_scroll_insert_onscreen (state);
	gtk_source_vim_state_begin_user_action (state);
}

static void
gtk_source_vim_replace_leave (GtkSourceVimState *state)
{
	g_assert (GTK_SOURCE_IS_VIM_REPLACE (state));

	gtk_source_vim_state_end_user_action (state);
}

static void
gtk_source_vim_replace_append_command (GtkSourceVimState *state,
                                       GString           *string)
{
	/* command should be empty during replace */
	g_string_truncate (string, 0);
}

static void
gtk_source_vim_replace_class_init (GtkSourceVimReplaceClass *klass)
{
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	state_class->command_bar_text = _("-- REPLACE --");
	state_class->append_command = gtk_source_vim_replace_append_command;
	state_class->handle_keypress = gtk_source_vim_replace_handle_keypress;
	state_class->enter = gtk_source_vim_replace_enter;
	state_class->leave = gtk_source_vim_replace_leave;
}

static void
gtk_source_vim_replace_init (GtkSourceVimReplace *self)
{
	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);
}

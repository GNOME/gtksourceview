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

#include "gtksourcevimcharpending.h"
#include "gtksourceviminsertliteral.h"

struct _GtkSourceVimCharPending
{
	GtkSourceVimState parent_class;
	gunichar character;
	char string[16];
};

G_DEFINE_TYPE (GtkSourceVimCharPending, gtk_source_vim_char_pending, GTK_SOURCE_TYPE_VIM_STATE)

static gboolean
gtk_source_vim_char_pending_handle_keypress (GtkSourceVimState *state,
                                             guint              keyval,
                                             guint              keycode,
                                             GdkModifierType    mods,
                                             const char        *string)
{
	GtkSourceVimCharPending *self = (GtkSourceVimCharPending *)state;

	g_assert (GTK_SOURCE_IS_VIM_CHAR_PENDING (self));

	if (gtk_source_vim_state_is_escape (keyval, mods))
	{
		goto completed;
	}

	gtk_source_vim_state_keyval_unescaped (keyval, mods, self->string);

	if (self->string[0] != 0)
	{
		if ((self->string[0] & 0x80) == 0x80)
			self->character = g_utf8_get_char (self->string);
		else
			self->character = self->string[0];
	}

completed:
	gtk_source_vim_state_pop (state);

	return TRUE;
}

static void
gtk_source_vim_char_pending_class_init (GtkSourceVimCharPendingClass *klass)
{
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	state_class->handle_keypress = gtk_source_vim_char_pending_handle_keypress;
}

static void
gtk_source_vim_char_pending_init (GtkSourceVimCharPending *self)
{
}

GtkSourceVimState *
gtk_source_vim_char_pending_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_CHAR_PENDING, NULL);
}

gunichar
gtk_source_vim_char_pending_get_character (GtkSourceVimCharPending *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_CHAR_PENDING (self), 0);

	return self->character;
}

const char *
gtk_source_vim_char_pending_get_string (GtkSourceVimCharPending *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_CHAR_PENDING (self), NULL);

	return self->string;
}

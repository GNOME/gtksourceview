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

#include "gtksourceviminsertliteral.h"

struct _GtkSourceVimInsertLiteral
{
	GtkSourceVimState parent_instance;
};

G_DEFINE_TYPE (GtkSourceVimInsertLiteral, gtk_source_vim_insert_literal, GTK_SOURCE_TYPE_VIM_STATE)

GtkSourceVimState *
gtk_source_vim_insert_literal_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_INSERT_LITERAL, NULL);
}

static gboolean
do_literal (GtkSourceVimInsertLiteral *self,
            const char                *string)
{
	g_assert (GTK_SOURCE_IS_VIM_INSERT_LITERAL (self));
	g_assert (string != NULL);

	if (string[0] != 0)
	{
		GtkSourceBuffer *buffer;
		GtkSourceView *view;
		GtkTextIter insert;

		view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
		buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &insert, NULL);

		if (gtk_text_view_get_overwrite (GTK_TEXT_VIEW (view)))
		{
			GtkTextIter end = insert;

			if (gtk_text_iter_forward_char (&end))
			{
				gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &insert, &end);
			}
		}

		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &insert, string, -1);
	}

	gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));

	return TRUE;
}

static gboolean
gtk_source_vim_insert_literal_handle_keypress (GtkSourceVimState *state,
                                               guint              keyval,
                                               guint              keycode,
                                               GdkModifierType    mods,
                                               const char        *string)
{
	GtkSourceVimInsertLiteral *self = (GtkSourceVimInsertLiteral *)state;
	char outbuf[16] = {0};

	g_assert (GTK_SOURCE_IS_VIM_INSERT_LITERAL (self));

	gtk_source_vim_state_keyval_unescaped (keyval, mods, outbuf);

	return do_literal (self, outbuf);
}

static void
gtk_source_vim_insert_literal_class_init (GtkSourceVimInsertLiteralClass *klass)
{
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	state_class->handle_keypress = gtk_source_vim_insert_literal_handle_keypress;
}

static void
gtk_source_vim_insert_literal_init (GtkSourceVimInsertLiteral *self)
{
}

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

#include "gtksourcevimmarks.h"

struct _GtkSourceVimMarks
{
	GtkSourceVimState parent_instance;
	GHashTable *marks;
};

G_DEFINE_TYPE (GtkSourceVimMarks, gtk_source_vim_marks, GTK_SOURCE_TYPE_VIM_STATE)

GtkSourceVimState *
gtk_source_vim_marks_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_MARKS, NULL);
}

static void
remove_mark (gpointer data)
{
	GtkTextMark *mark = data;
	GtkTextBuffer *buffer = gtk_text_mark_get_buffer (mark);
	gtk_text_buffer_delete_mark (buffer, mark);
	g_object_unref (mark);
}

static void
gtk_source_vim_marks_dispose (GObject *object)
{
	GtkSourceVimMarks *self = (GtkSourceVimMarks *)object;

	g_clear_pointer (&self->marks, g_hash_table_unref);

	G_OBJECT_CLASS (gtk_source_vim_marks_parent_class)->dispose (object);
}

static void
gtk_source_vim_marks_class_init (GtkSourceVimMarksClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_marks_dispose;
}

static void
gtk_source_vim_marks_init (GtkSourceVimMarks *self)
{
	self->marks = g_hash_table_new_full (g_str_hash,
	                                     g_str_equal,
	                                     NULL,
	                                     remove_mark);
}

GtkTextMark *
gtk_source_vim_marks_get_mark (GtkSourceVimMarks *self,
                               const char        *name)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MARKS (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	if (name[0] == '<' || name[0] == '>')
	{
		GtkTextIter iter, selection;
		GtkSourceBuffer *buffer;

		buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

		if (gtk_text_iter_compare (&iter, &selection) <= 0)
		{
			if (name[0] == '<')
				return gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
			else
				return gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (buffer));
		}
		else
		{
			if (name[0] == '<')
				return gtk_text_buffer_get_selection_bound (GTK_TEXT_BUFFER (buffer));
			else
				return gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
		}
	}

	return g_hash_table_lookup (self->marks, name);
}

gboolean
gtk_source_vim_marks_get_iter (GtkSourceVimMarks *self,
                               const char        *name,
                               GtkTextIter       *iter)
{
	GtkTextMark *mark;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MARKS (self), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	if (!(mark = gtk_source_vim_marks_get_mark (self, name)))
		return FALSE;

	if (iter == NULL)
		return TRUE;

	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (mark), iter, mark);

	return TRUE;
}

void
gtk_source_vim_marks_set_mark (GtkSourceVimMarks *self,
                               const char        *name,
                               const GtkTextIter *iter)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;

	g_return_if_fail (GTK_SOURCE_IS_VIM_MARKS (self));
	g_return_if_fail (name != NULL);

	if (iter == NULL)
	{
		g_hash_table_remove (self->marks, name);
		return;
	}

	if (!(mark = gtk_source_vim_marks_get_mark (self, name)))
	{
		buffer = GTK_TEXT_BUFFER (gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL));
		mark = gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE);
		g_hash_table_insert (self->marks,
		                     (char *)g_intern_string (name),
		                     g_object_ref (mark));
	}
	else
	{
		buffer = gtk_text_mark_get_buffer (mark);
		gtk_text_buffer_move_mark (buffer, mark, iter);
	}
}

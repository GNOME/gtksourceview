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

#include "gtksourcevimjumplist.h"

#define MAX_JUMPS 100
#define JUMP_INIT {{0}}

typedef struct
{
	GList        link;
	GtkTextMark *mark;
} Jump;

struct _GtkSourceVimJumplist
{
	GtkSourceVimState parent_instance;
	GQueue back;
	GQueue forward;
};

G_DEFINE_TYPE (GtkSourceVimJumplist, gtk_source_vim_jumplist, GTK_SOURCE_TYPE_VIM_STATE)

GtkSourceVimState *
gtk_source_vim_jumplist_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_JUMPLIST, NULL);
}

static void
jump_free (Jump *j)
{
	g_assert (j->link.data == j);
	g_assert (j->link.prev == NULL);
	g_assert (j->link.next == NULL);

	j->link.data = NULL;

	if (j->mark != NULL)
	{
		GtkTextBuffer *buffer = gtk_text_mark_get_buffer (j->mark);
		gtk_text_buffer_delete_mark (buffer, j->mark);
		g_object_unref (j->mark);
		j->mark = NULL;
	}

	g_slice_free (Jump, j);
}

static gboolean
jump_equal (const Jump *a,
            const Jump *b)
{
	GtkTextIter ai, bi;

	g_assert (GTK_IS_TEXT_MARK (a->mark));
	g_assert (GTK_IS_TEXT_MARK (b->mark));

	if (a == b)
		return TRUE;

	if (a->mark == b->mark)
		return TRUE;

	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (a->mark), &ai, a->mark);
	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (b->mark), &bi, b->mark);

	if (gtk_text_iter_get_line (&ai) == gtk_text_iter_get_line (&bi))
		return TRUE;

	return FALSE;
}

static void
clear_queue (GQueue *q)
{
	while (q->length > 0)
	{
		Jump *head = q->head->data;
		g_queue_unlink (q, &head->link);
		jump_free (head);
	}
}

static void
gtk_source_vim_jumplist_dispose (GObject *object)
{
	GtkSourceVimJumplist *self = (GtkSourceVimJumplist *)object;

	clear_queue (&self->back);
	clear_queue (&self->forward);

	G_OBJECT_CLASS (gtk_source_vim_jumplist_parent_class)->dispose (object);
}

static void
gtk_source_vim_jumplist_class_init (GtkSourceVimJumplistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_jumplist_dispose;
}

static void
gtk_source_vim_jumplist_init (GtkSourceVimJumplist *self)
{
}

void
gtk_source_vim_jumplist_push (GtkSourceVimJumplist *self,
                              const GtkTextIter    *iter)
{
	GtkTextBuffer *buffer;
	Jump *j;

	g_return_if_fail (GTK_SOURCE_IS_VIM_JUMPLIST (self));
	g_return_if_fail (iter != NULL);

	buffer = gtk_text_iter_get_buffer (iter);

	j = g_slice_new0 (Jump);
	j->link.data = j;
	j->mark = g_object_ref (gtk_text_buffer_create_mark (buffer, NULL, iter, TRUE));

	g_assert (GTK_IS_TEXT_MARK (j->mark));

	for (const GList *item = self->back.tail; item; item = item->prev)
	{
		Jump *j2 = item->data;

		if (jump_equal (j, j2))
		{
			g_queue_unlink (&self->back, &j2->link);
			jump_free (j2);
			goto push;
		}
	}

	for (const GList *item = self->forward.head; item; item = item->next)
	{
		Jump *j2 = item->data;

		if (jump_equal (j, j2))
		{
			g_queue_unlink (&self->forward, &j2->link);
			jump_free (j2);
			goto push;
		}
	}

push:
	if (self->back.length + self->forward.length >= MAX_JUMPS)
	{
		if (self->back.length > 0)
		{
			Jump *head = self->back.head->data;
			g_queue_unlink (&self->back, &head->link);
			jump_free (head);
		}
		else
		{
			Jump *tail = self->forward.tail->data;
			g_queue_unlink (&self->forward, &tail->link);
			jump_free (tail);
		}
	}

	g_queue_push_tail_link (&self->back, &j->link);
}

gboolean
gtk_source_vim_jumplist_previous (GtkSourceVimJumplist *self,
                                  GtkTextIter          *iter)
{
	GtkSourceBuffer *buffer;
	GtkTextIter before;
	Jump current = JUMP_INIT;
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_JUMPLIST (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &before, NULL);

	current.mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
	current.link.data = &current;

	gtk_source_vim_jumplist_push (self, &before);

	while (!ret && self->back.length > 0)
	{
		Jump *j = g_queue_peek_tail (&self->back);

		if (!jump_equal (&current, j))
		{
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, j->mark);
			ret = TRUE;
		}

		g_queue_unlink (&self->back, &j->link);
		g_queue_push_head_link (&self->forward, &j->link);
	}

	return ret;
}

gboolean
gtk_source_vim_jumplist_next (GtkSourceVimJumplist *self,
                              GtkTextIter          *iter)
{
	GtkSourceBuffer *buffer;
	GtkTextIter before;
	Jump current = JUMP_INIT;
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_JUMPLIST (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &before, NULL);

	current.mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
	current.link.data = &current;

	gtk_source_vim_jumplist_push (self, &before);

	while (!ret && self->forward.length > 0)
	{
		Jump *j = g_queue_peek_head (&self->forward);

		if (!jump_equal (&current, j))
		{
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, j->mark);
			ret = TRUE;
		}

		g_queue_unlink (&self->forward, &j->link);
		g_queue_push_tail_link (&self->back, &j->link);
	}

	return ret;
}

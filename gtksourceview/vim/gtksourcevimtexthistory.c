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

#include "gtksourcevimregisters.h"
#include "gtksourcevimtexthistory.h"

typedef enum
{
	OP_INSERT,
	OP_DELETE,
	OP_BACKSPACE,
} OpKind;

typedef struct
{
	OpKind kind : 2;
	guint length : 30;
	guint offset;
} Op;

struct _GtkSourceVimTextHistory
{
	GObject      parent_instance;
	GArray      *ops;
	GString     *bytes;
	int          cursor_position;
};

G_DEFINE_TYPE (GtkSourceVimTextHistory, gtk_source_vim_text_history, GTK_SOURCE_TYPE_VIM_STATE)

GtkSourceVimState *
gtk_source_vim_text_history_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_TEXT_HISTORY, NULL);
}

static void
gtk_source_vim_text_history_truncate (GtkSourceVimTextHistory *self)
{
	g_assert (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));

	g_string_truncate (self->bytes, 0);

	if (self->ops->len > 0)
	{
		g_array_remove_range (self->ops, 0, self->ops->len);
	}
}

static void
gtk_source_vim_text_history_insert_text_cb (GtkSourceVimTextHistory *self,
                                            const GtkTextIter       *iter,
                                            const char              *text,
                                            int                      len,
                                            GtkSourceBuffer         *buffer)
{
	guint position;
	Op op;

	g_assert (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));
	g_assert (iter != NULL);
	g_assert (gtk_text_iter_get_buffer (iter) == GTK_TEXT_BUFFER (buffer));
	g_assert (text != NULL);

	if (len == 0)
		return;

	position = gtk_text_iter_get_offset (iter);

	if ((int)position != self->cursor_position)
	{
		gtk_source_vim_text_history_truncate (self);
	}

	op.kind = OP_INSERT;
	op.length = g_utf8_strlen (text, len);
	op.offset = self->bytes->len;

	g_string_append_len (self->bytes, text, len);
	g_array_append_val (self->ops, op);

	self->cursor_position = position + op.length;
}

static void
gtk_source_vim_text_history_delete_range_cb (GtkSourceVimTextHistory *self,
                                             const GtkTextIter       *begin,
                                             const GtkTextIter       *end,
                                             GtkSourceBuffer         *buffer)
{
	GtkTextIter a, b;
	Op op;

	g_assert (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));
	g_assert (begin != NULL);
	g_assert (end != NULL);
	g_assert (gtk_text_iter_get_buffer (begin) == gtk_text_iter_get_buffer (end));

	if (gtk_text_iter_get_offset (begin) == gtk_text_iter_get_offset (end))
		return;

	a = *begin;
	b = *end;
	gtk_text_iter_order (&a, &b);

	op.length = (int)gtk_text_iter_get_offset (&b) - (int)gtk_text_iter_get_offset (&a);
	op.offset = 0;

	if (gtk_text_iter_get_offset (&a) == self->cursor_position)
	{
		op.kind = OP_DELETE;
		g_array_append_val (self->ops, op);
	}
	else if (gtk_text_iter_get_offset (&b) == self->cursor_position)
	{
		op.kind = OP_BACKSPACE;
		g_array_append_val (self->ops, op);
	}
	else
	{
		gtk_source_vim_text_history_truncate (self);
	}

	self->cursor_position = gtk_text_iter_get_offset (&a);
}

static void
gtk_source_vim_text_history_dispose (GObject *object)
{
	GtkSourceVimTextHistory *self = (GtkSourceVimTextHistory *)object;

	g_clear_pointer (&self->ops, g_array_unref);

	if (self->bytes)
	{
		g_string_free (self->bytes, TRUE);
		self->bytes = NULL;
	}

	G_OBJECT_CLASS (gtk_source_vim_text_history_parent_class)->dispose (object);
}

static void
gtk_source_vim_text_history_class_init (GtkSourceVimTextHistoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_text_history_dispose;
}

static void
gtk_source_vim_text_history_init (GtkSourceVimTextHistory *self)
{
	self->bytes = g_string_new (NULL);
	self->ops = g_array_new (FALSE, FALSE, sizeof (Op));
}

void
gtk_source_vim_text_history_begin (GtkSourceVimTextHistory *self)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);

	g_signal_connect_object (buffer,
	                         "insert-text",
	                         G_CALLBACK (gtk_source_vim_text_history_insert_text_cb),
	                         self,
	                         G_CONNECT_SWAPPED);

	g_signal_connect_object (buffer,
	                         "delete-range",
	                         G_CALLBACK (gtk_source_vim_text_history_delete_range_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
}

/*
 * string_truncate_n_chars:
 * @str: the GString
 * @n_chars: the number of chars to remove
 *
 * Removes @n_chars from the tail of @str, possibly taking into
 * account UTF-8 characters that are multi-width.
 */
static void
string_truncate_n_chars (GString *str,
                         gsize    n_chars)
{
	if (str == NULL)
	{
		return;
	}

	if (n_chars >= str->len)
	{
		g_string_truncate (str, 0);
		return;
	}

	g_assert (str->len > 0);

	while (n_chars > 0 && str->len > 0)
	{
		guchar ch = str->str[--str->len];

		/* If high bit is zero we have a one-byte char. If we have
		 * reached the byte with 0xC0 mask set then we are at the
		 * first character of a multi-byte char.
		 */
		if ((ch & 0x80) == 0 || (ch & 0xC0) == 0xC0)
		{
			n_chars--;
		}
	}

	str->str[str->len] = 0;
}

void
gtk_source_vim_text_history_end (GtkSourceVimTextHistory *self)
{
	GtkSourceVimState *registers;
	GtkSourceBuffer *buffer;
	GString *inserted;

	g_return_if_fail (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);

	g_signal_handlers_disconnect_by_func (buffer,
					      G_CALLBACK (gtk_source_vim_text_history_insert_text_cb),
					      self);
	g_signal_handlers_disconnect_by_func (buffer,
					      G_CALLBACK (gtk_source_vim_text_history_delete_range_cb),
					      self);

	/* Collect the inserted text into a single string and then set that
	 * in the "." register which is a read-only register to the user
	 * containing the last inserted text.
	 */
	inserted = g_string_new (NULL);
	for (guint i = 0; i < self->ops->len; i++)
	{
		const Op *op = &g_array_index (self->ops, Op, i);
		const char *str = self->bytes->str + op->offset;

		switch (op->kind)
		{
			case OP_INSERT:
				g_string_append_len (inserted, str, g_utf8_offset_to_pointer (str, op->length) - str);
				break;

			case OP_BACKSPACE:
				string_truncate_n_chars (inserted, op->length);
				break;

			default:
			case OP_DELETE:
				break;
		}
	}

	registers = gtk_source_vim_state_get_registers (GTK_SOURCE_VIM_STATE (self));
	gtk_source_vim_registers_set (GTK_SOURCE_VIM_REGISTERS (registers), ".", inserted->str);
	g_string_free (inserted, TRUE);
}

void
gtk_source_vim_text_history_replay (GtkSourceVimTextHistory *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter end;
	const char *str;
	int len;

	g_return_if_fail (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, NULL);

	for (guint i = 0; i < self->ops->len; i++)
	{
		const Op *op = &g_array_index (self->ops, Op, i);

		switch (op->kind)
		{
			case OP_INSERT:
				str = self->bytes->str + op->offset;
				len = g_utf8_offset_to_pointer (str, op->length) - str;
				gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, str, len);
				break;

			case OP_DELETE:
				end = iter;
				gtk_text_iter_forward_chars (&end, op->length);
				gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &end);
				break;

			case OP_BACKSPACE:
				end = iter;
				gtk_text_iter_backward_chars (&end, op->length);
				gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &end);
				break;

			default:
				g_assert_not_reached ();
		}
	}
}

gboolean
gtk_source_vim_text_history_is_empty (GtkSourceVimTextHistory *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_TEXT_HISTORY (self), FALSE);

	return self->ops->len == 0;
}

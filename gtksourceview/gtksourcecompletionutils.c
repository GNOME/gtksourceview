/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcecompletionutils.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright (C) 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include "gtksourcecompletionutils.h"
#include "gtksourcebuffer.h"

/**
 * is_separator:
 * @ch: the character to check.
 *
 * A separator is a character like (, a space etc. An _ is not a separator.
 *
 * Returns: %TRUE if @ch is a separator.
 */
static gboolean
is_separator (const gunichar ch)
{
	if (g_unichar_isprint (ch) &&
	    (g_unichar_isalnum (ch) || ch == g_utf8_get_char ("_")))
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * gtk_source_completion_utils_get_word_iter:
 * @buffer: a #GtkTextBuffer.
 * @start_word: assign it the start position of the word
 * @end_word: assign it the end position of the word
 */
void
gtk_source_completion_utils_get_word_iter (GtkTextBuffer *buffer,
					   GtkTextIter   *start_word,
					   GtkTextIter   *end_word)
{
	gtk_text_buffer_get_iter_at_mark (buffer,
	                                  end_word,
	                                  gtk_text_buffer_get_insert (buffer));

	*start_word = *end_word;

	while (gtk_text_iter_backward_char (start_word))
	{
		gunichar ch = gtk_text_iter_get_char (start_word);

		if (is_separator (ch))
		{
			gtk_text_iter_forward_char (start_word);
			return;
		}
	}
}

/**
 * gtk_source_completion_utils_replace_current_word:
 * @buffer: a #GtkTextBuffer.
 * @text: (allow-none): The text to be inserted instead of the current word.
 *
 * Replaces the current word in the @buffer with the new word.
 */
void
gtk_source_completion_utils_replace_current_word (GtkTextBuffer *buffer,
						  const gchar   *text)
{
	GtkTextIter word_start;
	GtkTextIter word_end;

	gtk_source_completion_utils_get_word_iter (buffer, &word_start, &word_end);

	gtk_text_buffer_begin_user_action (buffer);

	gtk_text_buffer_delete (buffer, &word_start, &word_end);

	if (text != NULL)
	{
		gtk_text_buffer_insert (buffer, &word_start, text, -1);
	}

	gtk_text_buffer_end_user_action (buffer);
}

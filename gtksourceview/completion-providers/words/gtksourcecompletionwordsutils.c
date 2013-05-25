/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 * gtksourcecompletionwordsutils.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 * Copyright (C) 2013 - Sébastien Wilmet
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourcecompletionwordsutils.h"
#include <string.h>

gboolean
gtk_source_completion_words_utils_forward_word_end (GtkTextIter    *iter,
                                                    CharacterCheck  valid,
                                                    gpointer        data)
{
	/* Go backward as long as there are word characters */
	while (TRUE)
	{
		/* Ending a line is good */
		if (gtk_text_iter_ends_line (iter))
		{
			break;
		}

		/* Check if the next character is a valid word character */
		if (!valid (gtk_text_iter_get_char (iter), data))
		{
			break;
		}

		gtk_text_iter_forward_char (iter);
	}

	return TRUE;
}

static gboolean
valid_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && (ch == '_' || g_unichar_isalnum (ch));
}

static gboolean
valid_start_char (gunichar ch)
{
	return !g_unichar_isdigit (ch);
}

/* Get the word at the end of @text.
 * Returns %NULL if not found.
 * Free the return value with g_free().
 */
gchar *
_gtk_source_completion_words_utils_get_end_word (gchar *text)
{
	gchar *cur_char = text + strlen (text);
	gboolean word_found = FALSE;
	gunichar ch;

	while (TRUE)
	{
		gchar *prev_char = g_utf8_find_prev_char (text, cur_char);

		if (prev_char == NULL)
		{
			break;
		}

		ch = g_utf8_get_char (prev_char);

		if (!valid_word_char (ch))
		{
			break;
		}

		word_found = TRUE;
		cur_char = prev_char;
	}

	if (!word_found)
	{
		return NULL;
	}

	ch = g_utf8_get_char (cur_char);

	if (!valid_start_char (ch))
	{
		return NULL;
	}

	return g_strdup (cur_char);
}

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 */

#include "config.h"

#include <errno.h>
#include <stdlib.h>

#include "gtksourcesnippetbundle-private.h"
#include "gtksourcesnippetchunk.h"

typedef struct
{
	GString   *cur_text;
	GPtrArray *chunks;
	guint      lineno;
} TextParser;

static void
flush_chunk (TextParser *parser)
{
	g_assert (parser != NULL);
	g_assert (parser->chunks != NULL);

	if (parser->cur_text->len > 0)
	{
		GtkSourceSnippetChunk *chunk = gtk_source_snippet_chunk_new ();

		gtk_source_snippet_chunk_set_spec (chunk, parser->cur_text->str);
		g_ptr_array_add (parser->chunks, g_object_ref_sink (chunk));
		g_string_truncate (parser->cur_text, 0);
	}
}

static void
do_part_named (TextParser  *parser,
               const gchar *name)
{
	GtkSourceSnippetChunk *chunk = gtk_source_snippet_chunk_new ();
	gchar *spec = g_strdup_printf ("$%s", name);

	gtk_source_snippet_chunk_set_spec (chunk, spec);
	gtk_source_snippet_chunk_set_focus_position (chunk, -1);

	g_ptr_array_add (parser->chunks, g_object_ref_sink (chunk));

	g_free (spec);
}

static void
do_part_linked (TextParser *parser,
                gint        n)
{
	GtkSourceSnippetChunk *chunk = gtk_source_snippet_chunk_new ();

	if (n > 0)
	{
		gchar text[12];

		g_snprintf (text, sizeof text, "$%u", n);
		text[sizeof text - 1] = '\0';

		gtk_source_snippet_chunk_set_spec (chunk, text);
	}
	else
	{
		gtk_source_snippet_chunk_set_spec (chunk, "");
		gtk_source_snippet_chunk_set_focus_position (chunk, 0);
	}

	g_ptr_array_add (parser->chunks, g_object_ref_sink (chunk));
}

static void
do_part_simple (TextParser  *parser,
                const gchar *line)
{
	g_string_append (parser->cur_text, line);
}

static void
do_part_n (TextParser  *parser,
           gint         n,
           const gchar *inner)
{
	GtkSourceSnippetChunk *chunk;

	g_assert (parser != NULL);
	g_assert (n >= -1);
	g_assert (inner != NULL);

	chunk = gtk_source_snippet_chunk_new ();
	gtk_source_snippet_chunk_set_spec (chunk, n ? inner : "");
	gtk_source_snippet_chunk_set_focus_position (chunk, n);

	g_ptr_array_add (parser->chunks, g_object_ref_sink (chunk));
}

static gboolean
parse_variable (const gchar  *line,
                glong        *n,
                gchar       **inner,
                const gchar **endptr,
                gchar       **name)
{
	gboolean has_inner = FALSE;
	char *end = NULL;
	gint brackets;
	gint i;

	*n = -1;
	*inner = NULL;
	*endptr = NULL;
	*name = NULL;

	g_assert (*line == '$');

	line++;

	*endptr = line;

	if (!*line)
	{
		*endptr = NULL;
		return FALSE;
	}

	if (*line == '{')
	{
		has_inner = TRUE;
		line++;
	}

	if (g_ascii_isdigit (*line))
	{
		errno = 0;
		*n = strtol (line, &end, 10);

		if (((*n == LONG_MIN) || (*n == LONG_MAX)) && errno == ERANGE)
			return FALSE;
		else if (*n < 0)
			return FALSE;

		line = end;
	}
	else if (g_ascii_isalpha (*line) || *line == '_')
	{
		const gchar *cur;

		for (cur = line; *cur; cur++)
		{
			if (g_ascii_isalnum (*cur) || *cur == '_')
				continue;
			break;
		}

		*endptr = cur;
		*name = g_strndup (line, cur - line);
		*n = -2;
		return TRUE;
	}

	if (has_inner)
	{
		if (*line == ':')
			line++;

		brackets = 1;

		for (i = 0; line[i]; i++)
		{
			switch (line[i])
			{
			case '{':
				brackets++;
				break;

			case '}':
				brackets--;
				break;

			default:
				break;
			}

			if (!brackets)
			{
				*inner = g_strndup (line, i);
				*endptr = &line[i + 1];
				return TRUE;
			}
		}

		return FALSE;
	}

	*endptr = line;

	return TRUE;
}

static void
do_part (TextParser  *parser,
         const gchar *line)
{
	const gchar *dollar;
	gchar *str;
	gchar *inner;
	gchar *name;
	glong n;

	g_assert (line != NULL);

again:
	if (!*line)
		return;

	if (!(dollar = strchr (line, '$')))
	{
		do_part_simple (parser, line);
		return;
	}

	/*
	 * Parse up to the next $ as a simple.
	 * If it is $N or ${N} then it is a linked chunk w/o tabstop.
	 * If it is ${N:""} then it is a chunk w/ tabstop.
	 * If it is ${blah|upper} then it is a non-tab stop chunk performing
	 * some sort of of expansion.
	 */

	g_assert (dollar >= line);

	if (dollar != line)
	{
		str = g_strndup (line, (dollar - line));
		do_part_simple (parser, str);
		g_free (str);
		line = dollar;
	}

parse_dollar:
	inner = NULL;

	if (!parse_variable (line, &n, &inner, &line, &name))
	{
		do_part_simple (parser, line);
		return;
	}

#if 0
	g_printerr ("Parse Variable: N=%ld  inner=\"%s\"\n", n, inner);
	g_printerr ("  Left over: \"%s\"\n", line);
#endif

	flush_chunk (parser);

	if (inner != NULL)
	{
		do_part_n (parser, n, inner);
		g_clear_pointer (&inner, g_free);
	}
	else if (n == -2 && name)
	{
		do_part_named (parser, name);
	}
	else
	{
		do_part_linked (parser, n);
	}

	g_free (name);

	if (line != NULL)
	{
		if (*line == '$')
		{
			goto parse_dollar;
		}
		else
		{
			goto again;
		}
	}
}

static gboolean
feed_line (TextParser  *parser,
           const gchar *line)
{
	g_assert (parser != NULL);
	g_assert (line != NULL);

	if (parser->cur_text->len || parser->chunks->len > 0)
	{
		g_string_append_c (parser->cur_text, '\n');
	}

	do_part (parser, line);

	return TRUE;
}

GPtrArray *
_gtk_source_snippet_bundle_parse_text (const gchar  *text,
                                       GError      **error)
{
	TextParser parser;
	gchar **lines = NULL;

	g_return_val_if_fail (text != NULL, NULL);

	/* Setup the parser */
	parser.cur_text = g_string_new (NULL);
	parser.lineno = 0;
	parser.chunks = g_ptr_array_new_with_free_func (g_object_unref);

	lines = g_strsplit (text, "\n", 0);
	for (guint i = 0; lines[i] != NULL; i++)
	{
		parser.lineno++;

		if (!feed_line (&parser, lines[i]))
		{
			goto handle_error;
		}
	}

	flush_chunk (&parser);

	goto finish;

handle_error:
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_INVALID_DATA,
		     "Failed to parse snippet text at line %u",
		     parser.lineno);
	g_clear_pointer (&parser.chunks, g_ptr_array_unref);

finish:
	g_string_free (parser.cur_text, TRUE);
	g_strfreev (lines);

	return g_steal_pointer (&parser.chunks);
}

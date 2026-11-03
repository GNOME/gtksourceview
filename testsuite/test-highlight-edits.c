/*
 * This file is part of GtkSourceView
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include <gtksourceview/gtksource.h>

static GtkSourceLanguageManager *languages;
static GtkSourceStyleScheme *style;

static void
highlight (GtkSourceBuffer *buffer)
{
	GtkTextIter begin, end;
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
}

static GtkSourceBuffer *
new_buffer (const char *id,
            const char *text)
{
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (languages, id);
	GtkSourceBuffer *buffer;

	g_assert_nonnull (language);
	buffer = gtk_source_buffer_new_with_language (language);
	gtk_source_buffer_set_style_scheme (buffer, style);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
	highlight (buffer);

	return buffer;
}

static int
compare_string (gconstpointer a,
                gconstpointer b,
                gpointer      unused)
{
	return g_strcmp0 (*(char * const *)a, *(char * const *)b);
}

static char *
snapshot (GtkSourceBuffer *buffer)
{
	GString *result = g_string_new (NULL);
	GtkTextIter iter, end;
	GHashTable *descriptions = g_hash_table_new_full (NULL, NULL, NULL, g_free);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &iter, &end);
	while (gtk_text_iter_compare (&iter, &end) < 0)
	{
		char **classes = gtk_source_buffer_get_context_classes_at_iter (buffer, &iter);
		GSList *tags = gtk_text_iter_get_tags (&iter);
		g_qsort_with_data (classes, g_strv_length (classes), sizeof (char *), compare_string, NULL);
		g_string_append_printf (result, "%u:", gtk_text_iter_get_char (&iter));
		for (guint i = 0; classes[i]; i++)
			g_string_append_printf (result, "%s,", classes[i]);
		g_string_append_c (result, '|');

		for (GSList *l = tags; l; l = l->next)
		{
			GtkTextTag *tag = l->data;
			const char *description = g_hash_table_lookup (descriptions, tag);

			if (description == NULL)
			{
				GdkRGBA *fg = NULL, *bg = NULL;
				gboolean fg_set, bg_set, weight_set, italic_set, underline_set, strike_set;
				gboolean strike;
				int weight, italic, underline;
				char *name = NULL;
				char *fg_string, *bg_string, *value;

				g_object_get (tag, "name", &name,
				              "foreground-set", &fg_set,
				              "foreground-rgba", &fg,
				              "background-set", &bg_set,
				              "background-rgba", &bg,
				              "weight-set", &weight_set,
				              "weight", &weight,
				              "style-set", &italic_set,
				              "style", &italic,
				              "underline-set", &underline_set,
				              "underline", &underline,
				              "strikethrough-set", &strike_set,
				              "strikethrough", &strike,
				              NULL);
				fg_string = fg_set && fg ? gdk_rgba_to_string (fg) : g_strdup ("");
				bg_string = bg_set && bg ? gdk_rgba_to_string (bg) : g_strdup ("");
				value = name ? g_strdup ("") :
					g_strdup_printf ("{%s;%s;%d:%d;%d:%d;%d:%d;%d:%d}",
					                 fg_string, bg_string, weight_set, weight_set ? weight : 0,
					                 italic_set, italic_set ? italic : 0,
					                 underline_set, underline_set ? underline : 0,
					                 strike_set, strike_set ? strike : 0);
				g_hash_table_insert (descriptions, tag, value);
				description = value;
				g_free (name);
				g_free (fg_string);
				g_free (bg_string);
				g_clear_pointer (&fg, gdk_rgba_free);
				g_clear_pointer (&bg, gdk_rgba_free);
			}
			g_string_append (result, description);
		}
		g_string_append_c (result, '\n');

		g_slist_free (tags);
		g_strfreev (classes);

		gtk_text_iter_forward_char (&iter);
	}

	g_hash_table_unref (descriptions);

	return g_string_free (result, FALSE);
}

static void
assert_fresh (GtkSourceBuffer *buffer,
              const char      *id)
{
	GtkTextIter begin, end;
	GtkSourceBuffer *fresh;
	char *text, *actual, *expected;

	highlight (buffer);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
	fresh = new_buffer (id, text);
	actual = snapshot (buffer);
	expected = snapshot (fresh);

	if (g_strcmp0 (actual, expected) != 0)
	{
		char **a = g_strsplit (actual, "\n", -1);
		char **b = g_strsplit (expected, "\n", -1);

		for (guint i = 0; a[i] != NULL && b[i] != NULL; i++)
		{
			if (g_strcmp0 (a[i], b[i]) != 0)
			{
				g_test_message ("first mismatch at character %u in %s", i, id);
				g_test_message ("text: %s", text);
				g_assert_cmpstr (a[i], ==, b[i]);
			}
		}

		g_strfreev (a);
		g_strfreev (b);
		g_assert_not_reached ();
	}

	g_free (actual);
	g_free (expected);
	g_free (text);
	g_object_unref (fresh);
}

static void
test_edit_batches (gconstpointer data)
{
	const char *id = data;
	const char *initial =
	        "int main() { /* α comment */ const char *s = R\"tag(\"raw\")tag\"; }\n"
	        "value = f\"ordinary {a + 42} tail\"\n"
	        "text = '''triple\n\nstring'''\n"
	        "<script>let x = \"embedded\"; // comment\n</script>\n"
	        "[[ [[ [[ int value ]] ]] ]]\n"
	        "#if 0\n#if 1\nignored\n#endif\n#endif\n";
	const char *inserts[] = { "x", " ", "\n", "/*", "*/", "\"", "'", "}", "α", "é", "<", ">", "\\", "[[", "]]" };
	GtkSourceBuffer *buffer = new_buffer (id, initial);

	for (guint batch = 0; batch < 80; batch++)
	{
		/* Include batches beyond the bounded journal's fallback threshold. */
		guint edits = batch % 13 == 0 ? 40 : 1 + batch % 7;
		for (guint i = 0; i < edits; i++)
		{
			int length = gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (buffer));
			int offset = g_test_rand_int_range (0, length + 1);
			GtkTextIter begin, end;

			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
			if (length > 0 && offset < length && g_test_rand_int_range (0, 3) == 0)
			{
				end = begin;
				gtk_text_iter_forward_chars (&end, MIN (length - offset, g_test_rand_int_range (1, 5)));
				gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
			}
			else
			{
				gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin,
				                        inserts[g_test_rand_int_range (0, G_N_ELEMENTS (inserts))], -1);
			}
		}
		g_test_message ("%s batch %u", id, batch);
		assert_fresh (buffer, id);
	}

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "", 0);
	assert_fresh (buffer, id);

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), initial, -1);
	assert_fresh (buffer, id);

	g_object_unref (buffer);
}

static void
test_language_attach_before_parse (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (languages, "c");
	GtkTextIter begin, end;

	gtk_source_buffer_set_style_scheme (buffer, style);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "int value;\n/* comment */\n", -1);
	gtk_source_buffer_set_language (buffer, language);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "const ", -1);
	assert_fresh (buffer, "c");

	/* Reattaching an engine over existing text has the same initial range. */
	gtk_source_buffer_set_language (buffer, NULL);
	gtk_source_buffer_set_language (buffer, language);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	end = begin;
	gtk_text_iter_forward_chars (&end, 6);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	assert_fresh (buffer, "c");

	g_object_unref (buffer);
}

static void
test_terminal_line_endings (void)
{
	const char *line_endings[] = { "\n", "\r\n", "\xE2\x80\xA9" };

	for (guint ending = 0; ending < G_N_ELEMENTS (line_endings); ending++)
	{
		char *text = g_strdup_printf ("/*%sx", line_endings[ending]);
		GtkSourceBuffer *buffer = new_buffer ("c", text);
		GtkTextIter begin, end;
		int eol_length = g_utf8_strlen (line_endings[ending], -1);

		/* A formerly interior newline becomes the final line ending. Its
		 * lexical membership must agree with a fresh parse of the same text.
		 */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 2 + eol_length);
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &end);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		assert_fresh (buffer, "c");
		for (int i = 0; i < eol_length; i++)
		{
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 2 + i);
			g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &begin, "comment"));
		}

		/* Closing the comment before that newline must remove inherited tags
		 * on all code points of CRLF, as well as single-code-point endings.
		 */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 2);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "*/", 2);
		assert_fresh (buffer, "c");
		for (int i = 0; i < eol_length; i++)
		{
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 4 + i);
			g_assert_false (gtk_source_buffer_iter_has_context_class (buffer, &begin, "comment"));
		}

		g_object_unref (buffer);
		g_free (text);
	}
}

static void
test_partial_line_endings (void)
{
	const char *line_endings[] = { "\n", "\r\n", "\xE2\x80\xA9" };
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (languages, "c");

	for (guint ending = 0; ending < G_N_ELEMENTS (line_endings); ending++)
	{
		char *text = g_strdup_printf ("/*%sinside%s*/%safter%s",
		                              line_endings[ending], line_endings[ending],
		                              line_endings[ending], line_endings[ending]);
		GtkSourceBuffer *buffer = gtk_source_buffer_new_with_language (language);
		GtkSourceBuffer *fresh = new_buffer ("c", text);
		GtkTextIter begin, end;
		char *actual, *expected;
		char **a, **b;
		int eol_length = g_utf8_strlen (line_endings[ending], -1);

		gtk_source_buffer_set_style_scheme (buffer, style);
		gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 1);
		gtk_source_buffer_ensure_highlight (buffer, &begin, &end);

		/* Both styles and classes at the last analyzed line ending must match
		 * those obtained when it is interior to one larger parsing batch.
		 */
		actual = snapshot (buffer);
		expected = snapshot (fresh);
		a = g_strsplit (actual, "\n", -1);
		b = g_strsplit (expected, "\n", -1);
		for (int i = 0; i < eol_length; i++)
			g_assert_cmpstr (a[2 + i], ==, b[2 + i]);
		g_strfreev (a);
		g_strfreev (b);
		g_free (actual);
		g_free (expected);

		assert_fresh (buffer, "c");
		g_object_unref (fresh);
		g_object_unref (buffer);
		g_free (text);
	}
}

int
main (int    argc,
      char **argv)
{
	const char *paths[] = { TOP_SRCDIR "/data/language-specs", NULL };
	const char *styles[] = { TOP_SRCDIR "/data/styles", NULL };
	const char *ids[] = { "c", "cpp", "python3", "html", "js", "markdown", "sh" };
	GtkSourceStyleSchemeManager *manager;
	int result;

	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	languages = gtk_source_language_manager_new ();
	gtk_source_language_manager_set_search_path (languages, paths);
	manager = gtk_source_style_scheme_manager_new ();
	gtk_source_style_scheme_manager_set_search_path (manager, styles);
	style = gtk_source_style_scheme_manager_get_scheme (manager, "Adwaita");
	g_assert_nonnull (style);

	for (guint i = 0; i < G_N_ELEMENTS (ids); i++)
	{
		char *name = g_strdup_printf ("/highlight/edits/%s", ids[i]);
		g_test_add_data_func (name, ids[i], test_edit_batches);
		g_free (name);
	}

	g_test_add_func ("/highlight/edits/language-attach-before-parse", test_language_attach_before_parse);
	g_test_add_func ("/highlight/edits/terminal-line-endings", test_terminal_line_endings);
	g_test_add_func ("/highlight/edits/partial-line-endings", test_partial_line_endings);
	result = g_test_run ();
	g_object_unref (manager);
	g_object_unref (languages);
	gtk_source_finalize ();

	return result;
}

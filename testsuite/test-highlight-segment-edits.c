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

#include "test-highlight-utils.h"

static void
assert_matches_fresh (GtkSourceBuffer *buffer)
{
	GtkTextIter begin, end, actual, expected;
	char *text;
	GtkSourceBuffer *fresh;

	test_highlight_buffer_ensure (buffer);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	text = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
	fresh = test_highlight_buffer_new ("test-segment-tree", text);
	test_highlight_buffer_ensure (fresh);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &actual);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (fresh), &expected);

	do
	{
		char **a = gtk_source_buffer_get_context_classes_at_iter (buffer, &actual);
		char **b = gtk_source_buffer_get_context_classes_at_iter (fresh, &expected);
		GSList *actual_tags = gtk_text_iter_get_tags (&actual);
		GSList *expected_tags = gtk_text_iter_get_tags (&expected);

		g_assert_cmpuint (g_strv_length (a), ==, g_strv_length (b));
		for (guint i = 0; a[i] != NULL; i++)
			g_assert_true (g_strv_contains ((const char * const *)b, a[i]));
		g_assert_cmpuint (g_slist_length (actual_tags), ==, g_slist_length (expected_tags));
		g_slist_free (actual_tags);
		g_slist_free (expected_tags);
		g_strfreev (a);
		g_strfreev (b);
		gtk_text_iter_forward_char (&expected);
	}
	while (gtk_text_iter_forward_char (&actual));

	g_object_unref (fresh);
	g_free (text);
}

static void
test_nested_reclamation (void)
{
	GString *text = g_string_new (NULL);
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;

	for (guint i = 0; i < 128; i++)
		g_string_append_c (text, '[');
	g_string_append (text, "éclair");
	for (guint i = 0; i < 128; i++)
		g_string_append_c (text, ']');
	g_string_append_c (text, '\n');
	buffer = test_highlight_buffer_new ("test-segment-tree", text->str);

	for (guint pass = 0; pass < 10; pass++)
	{
		test_highlight_buffer_ensure (buffer);
		/* Bottom-up teardown used to leave a deferred grandchild dangling
		 * when only its immediate parent was kept alive by the transaction.
		 */
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		test_highlight_buffer_ensure (buffer);
		gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text->str, text->len);
		test_highlight_buffer_ensure (buffer);

		/* Revive cached contexts while reparsing the containing line. */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 130);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "🙂", -1);
		assert_matches_fresh (buffer);
		gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text->str, text->len);
	}

	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

static void
test_indexed_edits (void)
{
	GString *text = g_string_new ("[\n");
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;
	const guint lines[] = { 800, 20, 500, 799, 999, 1 };

	for (guint i = 0; i < 1000; i++)
		g_string_append (text, "alpha beta gamma delta\n");
	g_string_append (text, "]\n");
	buffer = test_highlight_buffer_new ("test-segment-tree", text->str);

	/* A partial visible range builds an index over existing siblings. The
	 * subsequent suffix parse inserts into that already materialized index.
	 */
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &begin, 350);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 400);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	test_highlight_buffer_ensure (buffer);

	for (guint pass = 0; pass < G_N_ELEMENTS (lines); pass++)
	{
		gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &begin, lines[pass], 2);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "[é 🙂]", -1);
		assert_matches_fresh (buffer);
		gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &begin, lines[pass], 2);
		end = begin;
		gtk_text_iter_forward_chars (&end, 5);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		assert_matches_fresh (buffer);
	}

	/* Deleting across lines splits, prunes, merges, and reparents indexed lists. */
	gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &begin, 200, 2);
	gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &end, 700, 3);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	assert_matches_fresh (buffer);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	test_highlight_buffer_ensure (buffer);
	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	g_test_add_func ("/highlight/nested-reclamation", test_nested_reclamation);
	g_test_add_func ("/highlight/indexed-segment-edits", test_indexed_edits);
	return g_test_run ();
}

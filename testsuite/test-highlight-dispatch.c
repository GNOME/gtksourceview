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
assert_class (GtkSourceBuffer *buffer,
              guint            offset,
              const char      *expected)
{
	GtkTextIter iter;
	char **classes;

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
	classes = gtk_source_buffer_get_context_classes_at_iter (buffer, &iter);
	g_assert_cmpuint (g_strv_length (classes), ==, 1);
	g_assert_cmpstr (classes[0], ==, expected);
	g_strfreev (classes);
}

static void
test_compiled_dispatch (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-dispatch", "!abxxOLDnew\n!abxxOLDnew\n");
	GtkTextIter begin, end;

	test_highlight_buffer_ensure (buffer);

	for (guint pass = 0; pass < 3; pass++)
	{
		assert_class (buffer, 0, "first");
		assert_class (buffer, 1, "primary");
		assert_class (buffer, 2, "primary");
		assert_class (buffer, 3, "once");
		assert_class (buffer, 4, "fallback");
		for (guint offset = 5; offset < 8; offset++)
			assert_class (buffer, offset, "fallback");
		for (guint offset = 8; offset < 11; offset++)
			assert_class (buffer, offset, "replacement");

		assert_class (buffer, 12, "fallback");
		assert_class (buffer, 13, "primary");
		assert_class (buffer, 14, "primary");
		assert_class (buffer, 15, "fallback");
		assert_class (buffer, 16, "fallback");

		/* Force reparsing with the same immutable definition plan. */
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, " ", 1);
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
		end = begin;
		gtk_text_iter_forward_char (&end);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	}

	g_object_unref (buffer);
}

static void
test_literal_runs (void)
{
	GtkSourceBuffer *buffer;

	/* Ordinary Unicode, an escaped quote, interpolation and a closing quote. */
	buffer = test_highlight_buffer_new ("test-literal-runs", "\"αβ ordinary\\\" text{x} tail\" outside");
	test_highlight_buffer_ensure (buffer);
	for (guint i = 1; i < 12; i++)
		test_highlight_assert_class (buffer, i, "literal", TRUE);
	test_highlight_assert_class (buffer, 12, "escape", TRUE);
	test_highlight_assert_class (buffer, 13, "escape", TRUE);
	test_highlight_assert_class (buffer, 19, "interpolation", TRUE);
	test_highlight_assert_class (buffer, 20, "interpolation", TRUE);
	test_highlight_assert_class (buffer, 21, "interpolation", TRUE);
	test_highlight_assert_class (buffer, 27, "string", TRUE);
	test_highlight_assert_class (buffer, 28, "string", FALSE);
	g_object_unref (buffer);

	/* Dynamic host terminators must stop an unterminated embedded string. */
	buffer = test_highlight_buffer_new ("test-literal-runs", "<!\"abc!> outside");
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 4, "literal", TRUE);
	test_highlight_assert_class (buffer, 6, "string", FALSE);
	test_highlight_assert_class (buffer, 6, "host", TRUE);
	test_highlight_assert_class (buffer, 8, "host", FALSE);
	g_object_unref (buffer);

	/* Rules without a known first byte, including nullable rules, must
	 * disable the run optimization rather than consuming through a transition.
	 */
	buffer = test_highlight_buffer_new ("test-literal-runs", "[abcSTOPxyz] (abc!outside)");
	test_highlight_buffer_ensure (buffer);
	for (guint i = 4; i < 8; i++)
		test_highlight_assert_class (buffer, i, "word", TRUE);
	test_highlight_assert_class (buffer, 15, "nullable", TRUE);
	test_highlight_assert_class (buffer, 16, "nullable", TRUE);
	test_highlight_assert_class (buffer, 17, "nullable", FALSE);
	g_object_unref (buffer);
}

static void
test_long_unicode_literal_run (void)
{
	GString *text = g_string_new ("\"");
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;

	/* Cross the run's scheduling boundary in the middle of a multibyte code
	 * point. The resulting intervals must use characters, not bytes.
	 */
	for (guint i = 0; i < 22000; i++)
		g_string_append (text, "€");
	g_string_append (text, "\" tail");
	buffer = test_highlight_buffer_new ("test-literal-runs", text->str);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 1, "literal", TRUE);
	test_highlight_assert_class (buffer, 21846, "literal", TRUE);
	test_highlight_assert_class (buffer, 22000, "literal", TRUE);
	test_highlight_assert_class (buffer, 22001, "literal", FALSE);
	test_highlight_assert_class (buffer, 22002, "string", FALSE);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 16000);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "{x}", -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	test_highlight_assert_class (buffer, 15999, "literal", TRUE);
	test_highlight_assert_class (buffer, 16001, "interpolation", TRUE);
	test_highlight_assert_class (buffer, 16003, "literal", TRUE);
	test_highlight_assert_class (buffer, 22003, "literal", TRUE);
	test_highlight_assert_class (buffer, 22005, "string", FALSE);

	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

static void
test_ancestor_boundaries (void)
{
	const struct {
		const char *text;
		guint start;
		guint end;
		guint after;
	} cases[] = {
		{ "<αβ hello> outside", 1, 9, 10 },
		{ "[abcENDtail", 1, 4, 7 },
		{ "(ax! outside", 1, 3, 4 },
		{ "{[abc!! outside", 2, 5, 7 },
	};

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
	{
		GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-ancestor-boundaries", cases[i].text);

		test_highlight_buffer_ensure (buffer);

		for (guint offset = cases[i].start; offset < cases[i].end; offset++)
		{
			test_highlight_assert_class (buffer, offset, "token", TRUE);
			test_highlight_assert_class (buffer, offset, "capture", TRUE);
		}
		test_highlight_assert_class (buffer, cases[i].end, "token", FALSE);
		test_highlight_assert_class (buffer, cases[i].end, "capture", FALSE);
		test_highlight_assert_class (buffer, cases[i].after, "outer", FALSE);
		test_highlight_assert_class (buffer, cases[i].after, "inner", FALSE);

		g_object_unref (buffer);
	}
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	g_test_add_func ("/highlight/compiled-dispatch", test_compiled_dispatch);
	g_test_add_func ("/highlight/literal-runs", test_literal_runs);
	g_test_add_func ("/highlight/ancestor-boundaries", test_ancestor_boundaries);
	g_test_add_func ("/highlight/long-unicode-literal-run", test_long_unicode_literal_run);
	return g_test_run ();
}

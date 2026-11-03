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
updated (GtkSourceBuffer *buffer,
         GtkTextIter     *begin,
         GtkTextIter     *end,
         int             *last)
{
	*last = MAX (*last, gtk_text_iter_get_offset (end));
}

static void
test_column_zero_convergence (void)
{
	GString *text = g_string_new (NULL);
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;
	int last = 0;

	for (guint i = 0; i < 2000; i++)
		g_string_append (text, "int value = 123; /* comment */\n");

	buffer = test_highlight_buffer_new ("c", text->str);
	test_highlight_buffer_ensure (buffer);
	g_signal_connect (buffer, "highlight-updated", G_CALLBACK (updated), &last);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, " ", 1);
	test_highlight_buffer_ensure (buffer);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 1);
	g_assert_cmpint (last, >, 0);
	g_assert_cmpint (last, <=, gtk_text_iter_get_offset (&end));
	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

static void
test_once_history (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-convergence", "a\nx\nx\n");
	GtkTextIter begin, end;

	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 2, "once", TRUE);
	test_highlight_assert_class (buffer, 4, "normal", TRUE);

	/* A retained future once-only match must not suppress an earlier match. */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "x", 1);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 0, "once", TRUE);
	test_highlight_assert_class (buffer, 3, "once", FALSE);
	test_highlight_assert_class (buffer, 3, "normal", TRUE);

	/* Removing the old once-only match changes eligibility in the suffix. */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	end = begin;
	gtk_text_iter_forward_char (&end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 2, "once", TRUE);
	test_highlight_assert_class (buffer, 2, "normal", FALSE);
	g_object_unref (buffer);
}

static void
test_first_line_moves (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-convergence", " !a\nrest\n");
	GtkTextIter begin, end;

	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 1, "first", TRUE);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "\n", 1);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 2, "first", FALSE);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
	end = begin;
	gtk_text_iter_forward_char (&end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 1, "first", TRUE);
	g_object_unref (buffer);
}

static int
compare_class (gconstpointer a,
               gconstpointer b,
               gpointer      unused)
{
	return g_strcmp0 (*(char * const *)a, *(char * const *)b);
}

static void
assert_fresh_classes (GtkSourceBuffer *buffer)
{
	GtkTextIter begin, end, expected;
	GtkSourceBuffer *fresh;
	char *text;

	test_highlight_buffer_ensure (buffer);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	text = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
	fresh = test_highlight_buffer_new ("test-boundary-state", text);
	test_highlight_buffer_ensure (fresh);
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (fresh), &expected);

	while (gtk_text_iter_compare (&begin, &end) < 0)
	{
		char **a = gtk_source_buffer_get_context_classes_at_iter (buffer, &begin);
		char **b = gtk_source_buffer_get_context_classes_at_iter (fresh, &expected);
		g_qsort_with_data (a, g_strv_length (a), sizeof (char *), compare_class, NULL);
		g_qsort_with_data (b, g_strv_length (b), sizeof (char *), compare_class, NULL);
		g_assert_true (g_strv_equal ((const char * const *)a, (const char * const *)b));
		g_assert_false (gtk_source_buffer_iter_has_context_class (buffer, &begin, "empty-capture"));
		g_strfreev (a);
		g_strfreev (b);
		gtk_text_iter_forward_char (&begin);
		gtk_text_iter_forward_char (&expected);
	}

	g_free (text);
	g_object_unref (fresh);
}

static void
test_boundary_states (void)
{
	const char *lines[] = { "[\316\261]\nword\n", "(\316\261)\nword\n", "<A>\n\316\261\n</A>\nword\n" };

	for (guint i = 0; i < G_N_ELEMENTS (lines); i++)
	{
		char *text = g_strconcat ("plain\n", lines[i], NULL);
		GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-boundary-state", text);
		GtkTextIter begin, end;
		int last = 0;

		test_highlight_buffer_ensure (buffer);
		g_signal_connect (buffer, "highlight-updated", G_CALLBACK (updated), &last);
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, " ", 1);
		test_highlight_buffer_ensure (buffer);
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 1);
		g_assert_cmpint (last, >, 0);
		g_assert_cmpint (last, <=, gtk_text_iter_get_offset (&end));
		assert_fresh_classes (buffer);
		g_object_unref (buffer);
		g_free (text);
	}
}

static void
test_captured_continuation (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-boundary-state", "<A>\n\316\261\n</B>\ntail\n</A>\nend\n");
	GtkTextIter begin, end;

	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 10, "captured", TRUE);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 1);
	end = begin;
	gtk_text_iter_forward_char (&end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "B", 1);
	assert_fresh_classes (buffer);
	test_highlight_assert_class (buffer, 10, "captured", FALSE);
	g_object_unref (buffer);
}

static void
test_nested_once_history (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-boundary-state", "{\na\nx\nx\n}\n{\nx\nx\n}\n");
	GtkTextIter begin, end;

	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 4, "once", TRUE);
	test_highlight_assert_class (buffer, 6, "normal", TRUE);
	test_highlight_assert_class (buffer, 12, "once", TRUE);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 2);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "x", 1);
	assert_fresh_classes (buffer);
	test_highlight_assert_class (buffer, 2, "once", TRUE);
	test_highlight_assert_class (buffer, 5, "normal", TRUE);
	test_highlight_assert_class (buffer, 13, "once", TRUE);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, 2);
	end = begin;
	gtk_text_iter_forward_char (&end);
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
	assert_fresh_classes (buffer);
	test_highlight_assert_class (buffer, 4, "once", TRUE);
	test_highlight_assert_class (buffer, 12, "once", TRUE);
	g_object_unref (buffer);
}

int
main (int    argc,
      char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	g_test_add_func ("/highlight/convergence/column-zero", test_column_zero_convergence);
	g_test_add_func ("/highlight/convergence/once-history", test_once_history);
	g_test_add_func ("/highlight/convergence/first-line-moves", test_first_line_moves);
	g_test_add_func ("/highlight/convergence/boundary-states", test_boundary_states);
	g_test_add_func ("/highlight/convergence/captured-continuation", test_captured_continuation);
	g_test_add_func ("/highlight/convergence/nested-once-history", test_nested_once_history);
	result = g_test_run ();
	gtk_source_finalize ();
	return result;
}

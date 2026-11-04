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

typedef struct { int first; int last; guint calls; } Updated;

static void
updated_cb (GtkSourceBuffer *buffer,
            GtkTextIter     *begin,
            GtkTextIter     *end,
            Updated         *updated)
{
	updated->first = MIN (updated->first, gtk_text_iter_get_offset (begin));
	updated->last = MAX (updated->last, gtk_text_iter_get_offset (end));
	updated->calls++;
}

static void
test_visible_islands (void)
{
	GString *text = g_string_new (NULL);
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end, iter;
	Updated updated = { G_MAXINT, 0, 0 };

	for (guint i = 0; i < 5000; i++)
		g_string_append (text, "# comment 123\nword\n");
	buffer = test_highlight_buffer_new ("test-line-local", text->str);
	g_signal_connect (buffer, "highlight-updated", G_CALLBACK (updated_cb), &updated);

	for (guint pass = 0; pass < 2; pass++)
	{
		int line = pass == 0 ? 9000 : 4000;
		updated = (Updated) { G_MAXINT, 0, 0 };
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &begin, line);
		end = begin;
		gtk_text_iter_forward_lines (&end, 4);
		gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
		g_assert_cmpuint (updated.calls, >, 0);
		g_assert_cmpint (updated.first, ==, gtk_text_iter_get_offset (&begin));
		g_assert_cmpint (updated.last, <=, gtk_text_iter_get_offset (&end));
		iter = begin;
		gtk_text_iter_forward_char (&iter);
		g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"));
		gtk_text_iter_forward_line (&iter);
		g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "word"));

		updated.calls = 0;
		gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
		g_assert_cmpuint (updated.calls, ==, 0);
	}

	/* Earlier edits must move these ranges without marking gaps as parsed. */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "# new\n", -1);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &begin, 9001);
	end = begin;
	gtk_text_iter_forward_lines (&end, 2);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	iter = begin;
	gtk_text_iter_forward_char (&iter);
	g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "comment"));

	/* Reconcile every remaining gap and check the entire resulting buffer. */
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);

	for (guint line = 1; line < 10001; line++)
	{
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, line);
		g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, line % 2 == 1 ? "comment" : "word"));
	}

	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

static void
test_continuation_rejects_proof (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-line-continued", "# continued\\\ninside\noutside\n");
	GtkTextIter begin, end;
	Updated updated = { G_MAXINT, 0, 0 };

	g_signal_connect (buffer, "highlight-updated", G_CALLBACK (updated_cb), &updated);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &begin, 1);
	end = begin;
	gtk_text_iter_forward_line (&end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	g_assert_cmpint (updated.first, ==, 0);
	g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &begin, "comment"));
	g_object_unref (buffer);
}

static void
test_python_blank_is_not_restart (void)
{
	GtkSourceBuffer *buffer = test_highlight_buffer_new ("python3", "value = '''first\n\ninside\n'''\noutside\n");
	GtkTextIter begin, end;
	Updated updated = { G_MAXINT, 0, 0 };

	g_signal_connect (buffer, "highlight-updated", G_CALLBACK (updated_cb), &updated);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &begin, 2);
	end = begin;
	gtk_text_iter_forward_line (&end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	g_assert_cmpint (updated.first, ==, 0);
	g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &begin, "string"));
	g_object_unref (buffer);
}

static void
assert_island_matches (GtkSourceBuffer *buffer,
                       GtkSourceBuffer *fresh,
                       GtkTextIter     *begin,
                       GtkTextIter     *end)
{
	const char *classes[] = { "comment", "number", "word" };
	GtkTextIter actual = *begin;
	GtkTextIter expected;

	gtk_source_buffer_ensure_highlight (buffer, begin, end);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (fresh), &expected,
	                                    gtk_text_iter_get_offset (begin));
	while (gtk_text_iter_compare (&actual, end) < 0)
	{
		for (guint i = 0; i < G_N_ELEMENTS (classes); i++)
			g_assert_cmpint (gtk_source_buffer_iter_has_context_class (buffer, &actual, classes[i]),
			                 ==,
			                 gtk_source_buffer_iter_has_context_class (fresh, &expected, classes[i]));
		gtk_text_iter_forward_char (&actual);
		gtk_text_iter_forward_char (&expected);
	}
}

static void
test_islands_with_pending_edits (void)
{
	const char *inserts[] = { "#", "\n", "\r\n", "\342\200\251", "\316\261", "word 23" };
	GString *text = g_string_new (NULL);
	GtkSourceBuffer *buffer;

	for (guint i = 0; i < 1200; i++)
		g_string_append (text, "# comment 123\nword\n");
	buffer = test_highlight_buffer_new ("test-line-local", text->str);

	for (guint batch = 0; batch < 8; batch++)
	{
		GtkTextIter begin, end;
		GtkSourceBuffer *fresh;
		char *contents;
		guint edits = batch == 4 ? 40 : 3;

		/* Include edits in unknown gaps and previously requested islands,
		 * separated by more than the journal's nearby-edit merge distance.
		 * One larger batch also exercises its bounding-range fallback.
		 */
		for (guint edit = 0; edit < edits; edit++)
		{
			int length = gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (buffer));
			int offset = (edit % 3) * (length / 3) + g_test_rand_int_range (0, 20);
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);

			/* Keep CRLF pairs intact, as interactive editing does. Splitting
			 * one can give GTK different line boundaries from a fresh buffer
			 * containing the same text, invalidating this comparison.
			 */
			if (!gtk_text_iter_is_cursor_position (&begin))
				gtk_text_iter_forward_cursor_position (&begin);

			if ((batch + edit) % 3 == 0)
			{
				end = begin;
				gtk_text_iter_forward_cursor_positions (&end, 1 + edit % 4);
				gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
			}
			else
			{
				gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
				                        &begin,
				                        inserts[g_test_rand_int_range (0, G_N_ELEMENTS (inserts))], -1);
			}
		}

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
		contents = gtk_text_buffer_get_slice (GTK_TEXT_BUFFER (buffer), &begin, &end, TRUE);
		fresh = test_highlight_buffer_new ("test-line-local", contents);
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (fresh), &begin, &end);
		gtk_source_buffer_ensure_highlight (fresh, &begin, &end);

		/* Reverse and partial-line requests leave both a prefix and middle
		 * gaps unknown while all requested class memberships match a parse
		 * from the beginning of the final text.
		 */
		for (int island = 3; island >= 1; island--)
		{
			int length = gtk_text_buffer_get_char_count (GTK_TEXT_BUFFER (buffer));
			int offset = island == 3 ? length - 150 : island * (length / 3) + 3;
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
			end = begin;
			gtk_text_iter_forward_chars (&end, 101);
			assert_island_matches (buffer, fresh, &begin, &end);
		}

		if (batch == 7)
		{
			gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
			assert_island_matches (buffer, fresh, &begin, &end);
		}

		g_free (contents);
		g_object_unref (fresh);
	}

	g_object_unref (buffer);
	g_string_free (text, TRUE);
}

static void
test_delete_line_prefix (void)
{
	const guint batches[] = { 1, 40 };

	for (guint i = 0; i < G_N_ELEMENTS (batches); i++)
	{
		GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-line-local", "word\nx# comment\n");
		GtkTextIter begin, end;
		int offset = 5 + batches[i];

		test_highlight_buffer_ensure (buffer);
		for (guint edit = 0; edit < batches[i]; edit++)
		{
			gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, "a", 1);
		}

		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, offset);
		end = begin;
		gtk_text_iter_forward_char (&end);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		test_highlight_buffer_ensure (buffer);
		test_highlight_assert_class (buffer, offset, "comment", TRUE);
		g_object_unref (buffer);
	}
}

static void
test_first_line_edits (void)
{
	const char *prefixes[] = { "\n", "\n\n", "new\n", "new\nprefix " };

	for (guint i = 0; i < G_N_ELEMENTS (prefixes); i++)
	{
		GtkSourceBuffer *buffer = test_highlight_buffer_new ("test-first-line", "a! b\nrest\n");
		GtkTextIter begin, end, iter;
		int inserted = g_utf8_strlen (prefixes[i], -1);

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1);
		g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "first"));

		for (guint pass = 0; pass < 3; pass++)
		{
			gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &begin, prefixes[i], -1);
			gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
			gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1 + inserted);
			g_assert_false (gtk_source_buffer_iter_has_context_class (buffer, &iter, "first"));

			gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &begin);
			end = begin;
			gtk_text_iter_forward_chars (&end, inserted);
			gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
			gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
			gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 1);
			g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "first"));
		}

		g_object_unref (buffer);
	}
}

int
main (int    argc,
      char **argv)
{
	int result;

	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	g_test_add_func ("/highlight/jump/certified-islands", test_visible_islands);
	g_test_add_func ("/highlight/jump/continued-eol-rejected", test_continuation_rejects_proof);
	g_test_add_func ("/highlight/jump/python-blank-rejected", test_python_blank_is_not_restart);
	g_test_add_func ("/highlight/jump/islands-with-pending-edits", test_islands_with_pending_edits);
	g_test_add_func ("/highlight/jump/first-line-edits", test_first_line_edits);
	g_test_add_func ("/highlight/jump/delete-line-prefix", test_delete_line_prefix);
	result = g_test_run ();
	gtk_source_finalize ();
	return result;
}

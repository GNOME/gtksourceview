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

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	gtk_source_init ();
	g_test_add_func ("/highlight/compiled-dispatch", test_compiled_dispatch);
	return g_test_run ();
}

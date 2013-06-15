/*
 * test-search.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourcesearch.h"

static void
flush_queue (void)
{
	while (gtk_events_pending ())
	{
		gtk_main_iteration ();
	}
}

/* Without insertion or deletion of text in the buffer afterwards. */
static void
test_occurrences_count_simple (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GtkTextIter iter;
	guint occurrences_count;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Some foo\nSome bar\n", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	gtk_source_buffer_set_search_text (buffer, "world");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	gtk_source_buffer_set_search_text (buffer, "Some");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	gtk_source_buffer_set_search_text (buffer, "foo");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	gtk_source_buffer_set_search_text (buffer, "world");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	g_object_unref (buffer);
}

static void
test_occurrences_count_with_insert (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter iter;
	guint occurrences_count;

	/* Contents: "foobar" */
	gtk_text_buffer_get_start_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foobar", -1);

	gtk_source_buffer_set_search_text (source_buffer, "foo");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	/* Contents: "foobar " */
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, " ", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	/* Contents: "foobar foobeer" */
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foobeer", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	/* Contents: "foo bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 3);
	gtk_text_buffer_insert (text_buffer, &iter, " ", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	/* Contents: "foto bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 2);
	gtk_text_buffer_insert (text_buffer, &iter, "t", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	/* Contents: "footo bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 2);
	gtk_text_buffer_insert (text_buffer, &iter, "o", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	/* Contents: "foofooto bar foobeer" */
	gtk_text_buffer_get_start_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 3);

	/* Contents: "fooTfooto bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 3);
	gtk_text_buffer_insert (text_buffer, &iter, "T", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 3);

	g_object_unref (source_buffer);
}

static void
test_occurrences_count_with_delete (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter start;
	GtkTextIter end;
	guint occurrences_count;

	gtk_source_buffer_set_search_text (source_buffer, "foo");

	/* Contents: "foo" -> "" */
	gtk_text_buffer_set_text (text_buffer, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	gtk_text_buffer_get_bounds (text_buffer, &start, &end);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	/* Contents: "foo" -> "oo" */
	gtk_text_buffer_set_text (text_buffer, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 1);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	/* Contents: "foobar foobeer" -> "foobar" */
	gtk_text_buffer_set_text (text_buffer, "foobar foobeer", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 6);
	gtk_text_buffer_get_end_iter (text_buffer, &end);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	/* Contents: "foo[foo]foo" -> "foofoo" */
	gtk_text_buffer_set_text (text_buffer, "foofoofoo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 3);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 3);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 6);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	/* Contents: "fo[of]oo" -> "fooo" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 2);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 4);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	/* Contents: "foto" -> "foo" */
	gtk_text_buffer_set_text (text_buffer, "foto", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 0);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 2);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 3);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	g_object_unref (source_buffer);
}

static void
test_occurrences_count_multiple_lines (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	guint occurrences_count;

	gtk_source_buffer_set_search_text (source_buffer, "world\nhello");

	gtk_text_buffer_set_text (text_buffer, "hello world\nhello world\nhello world\n", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 2);

	gtk_source_buffer_set_search_text (source_buffer, "world\n");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 3);

	gtk_source_buffer_set_search_text (source_buffer, "\nhello world\n");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpuint (occurrences_count, ==, 1);

	g_object_unref (source_buffer);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Search/occurrences-count/simple", test_occurrences_count_simple);
	g_test_add_func ("/Search/occurrences-count/with-insert", test_occurrences_count_with_insert);
	g_test_add_func ("/Search/occurrences-count/with-delete", test_occurrences_count_with_delete);
	g_test_add_func ("/Search/occurrences-count/multiple-lines", test_occurrences_count_multiple_lines);

	return g_test_run ();
}

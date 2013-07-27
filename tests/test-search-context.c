/*
 * test-search-context.c
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

typedef struct
{
	gint match_start_offset;
	gint match_end_offset;
	guint found : 1;
} SearchResult;

typedef struct
{
	SearchResult *results;
	gint result_num;
	guint forward : 1;
} AsyncData;

static void check_async_search_results (GtkSourceBuffer *source_buffer,
					SearchResult    *results,
					gboolean         forward,
					gboolean         start_check);

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
	gint occurrences_count;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "Some foo\nSome bar\n", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	gtk_source_buffer_set_search_text (buffer, "world");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	gtk_source_buffer_set_search_text (buffer, "Some");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	gtk_source_buffer_set_search_text (buffer, "foo");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	gtk_source_buffer_set_search_text (buffer, "world");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	g_object_unref (buffer);
}

static void
test_occurrences_count_with_insert (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter iter;
	gint occurrences_count;

	/* Contents: "foobar" */
	gtk_text_buffer_get_start_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foobar", -1);

	gtk_source_buffer_set_search_text (source_buffer, "foo");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Contents: "foobar " */
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, " ", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Contents: "foobar foobeer" */
	gtk_text_buffer_get_end_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foobeer", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	/* Contents: "foo bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 3);
	gtk_text_buffer_insert (text_buffer, &iter, " ", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	/* Contents: "foto bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 2);
	gtk_text_buffer_insert (text_buffer, &iter, "t", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Contents: "footo bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 2);
	gtk_text_buffer_insert (text_buffer, &iter, "o", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	/* Contents: "foofooto bar foobeer" */
	gtk_text_buffer_get_start_iter (text_buffer, &iter);
	gtk_text_buffer_insert (text_buffer, &iter, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 3);

	/* Contents: "fooTfooto bar foobeer" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &iter, 3);
	gtk_text_buffer_insert (text_buffer, &iter, "T", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 3);

	g_object_unref (source_buffer);
}

static void
test_occurrences_count_with_delete (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter start;
	GtkTextIter end;
	gint occurrences_count;

	gtk_source_buffer_set_search_text (source_buffer, "foo");

	/* Contents: "foo" -> "" */
	gtk_text_buffer_set_text (text_buffer, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	gtk_text_buffer_get_bounds (text_buffer, &start, &end);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	/* Contents: "foo" -> "oo" */
	gtk_text_buffer_set_text (text_buffer, "foo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 1);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	/* Contents: "foobar foobeer" -> "foobar" */
	gtk_text_buffer_set_text (text_buffer, "foobar foobeer", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 6);
	gtk_text_buffer_get_end_iter (text_buffer, &end);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Contents: "foo[foo]foo" -> "foofoo" */
	gtk_text_buffer_set_text (text_buffer, "foofoofoo", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 3);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 3);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 6);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	/* Contents: "fo[of]oo" -> "fooo" */
	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 2);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 4);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Contents: "foto" -> "foo" */
	gtk_text_buffer_set_text (text_buffer, "foto", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 2);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 3);
	gtk_text_buffer_delete (text_buffer, &start, &end);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	g_object_unref (source_buffer);
}

static void
test_occurrences_count_multiple_lines (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gint occurrences_count;

	gtk_source_buffer_set_search_text (source_buffer, "world\nhello");

	gtk_text_buffer_set_text (text_buffer, "hello world\nhello world\nhello world\n", -1);
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	gtk_source_buffer_set_search_text (source_buffer, "world\n");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 3);

	gtk_source_buffer_set_search_text (source_buffer, "\nhello world\n");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	g_object_unref (source_buffer);
}

static void
test_case_sensitivity (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gboolean case_sensitive;
	gint occurrences_count;

	gtk_text_buffer_set_text (text_buffer, "Case", -1);
	gtk_source_buffer_set_search_text (source_buffer, "case");

	gtk_source_buffer_set_case_sensitive_search (source_buffer, TRUE);
	case_sensitive = gtk_source_buffer_get_case_sensitive_search (source_buffer);
	g_assert (case_sensitive);

	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 0);

	gtk_source_buffer_set_case_sensitive_search (source_buffer, FALSE);
	case_sensitive = gtk_source_buffer_get_case_sensitive_search (source_buffer);
	g_assert (!case_sensitive);

	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	g_object_unref (source_buffer);
}

static void
test_search_at_word_boundaries (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gboolean at_word_boundaries;
	gint occurrences_count;

	gtk_text_buffer_set_text (text_buffer, "AtWordBoundaries AtWord", -1);
	gtk_source_buffer_set_search_text (source_buffer, "AtWord");

	gtk_source_buffer_set_search_at_word_boundaries (source_buffer, TRUE);
	at_word_boundaries = gtk_source_buffer_get_search_at_word_boundaries (source_buffer);
	g_assert (at_word_boundaries);

	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	gtk_source_buffer_set_search_at_word_boundaries (source_buffer, FALSE);
	at_word_boundaries = gtk_source_buffer_get_search_at_word_boundaries (source_buffer);
	g_assert (!at_word_boundaries);

	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	g_object_unref (source_buffer);
}

static void
check_search_results (GtkSourceBuffer *source_buffer,
		      SearchResult    *results,
		      gboolean         forward)
{
	GtkTextIter iter;
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	gtk_text_buffer_get_start_iter (text_buffer, &iter);

	do
	{
		gint i;
		gboolean found;
		GtkTextIter match_start;
		GtkTextIter match_end;

		i = gtk_text_iter_get_offset (&iter);

		if (forward)
		{
			found = gtk_source_buffer_forward_search (source_buffer,
								  &iter,
								  &match_start,
								  &match_end);
		}
		else
		{
			found = gtk_source_buffer_backward_search (source_buffer,
								   &iter,
								   &match_start,
								   &match_end);
		}

		g_assert (found == results[i].found);

		if (found)
		{
			gint match_start_offset = gtk_text_iter_get_offset (&match_start);
			gint match_end_offset = gtk_text_iter_get_offset (&match_end);

			g_assert_cmpint (match_start_offset, ==, results[i].match_start_offset);
			g_assert_cmpint (match_end_offset, ==, results[i].match_end_offset);
		}
	}
	while (gtk_text_iter_forward_char (&iter));
}

static void
finish_check_result (GtkSourceBuffer *source_buffer,
		     GAsyncResult    *result,
		     AsyncData       *data)
{
	GtkTextIter match_start;
	GtkTextIter match_end;
	gboolean found;
	SearchResult search_result = data->results[data->result_num];

	if (data->forward)
	{
		found = gtk_source_buffer_forward_search_finish (source_buffer,
								 result,
								 &match_start,
								 &match_end,
								 NULL);
	}
	else
	{
		found = gtk_source_buffer_backward_search_finish (source_buffer,
								  result,
								  &match_start,
								  &match_end,
								  NULL);
	}

	g_assert (found == search_result.found);

	if (found)
	{
		gint match_start_offset = gtk_text_iter_get_offset (&match_start);
		gint match_end_offset = gtk_text_iter_get_offset (&match_end);

		g_assert_cmpint (match_start_offset, ==, search_result.match_start_offset);
		g_assert_cmpint (match_end_offset, ==, search_result.match_end_offset);
	}

	check_async_search_results (source_buffer,
				    data->results,
				    data->forward,
				    FALSE);

	g_slice_free (AsyncData, data);
}

static void
check_async_search_results (GtkSourceBuffer *source_buffer,
			    SearchResult    *results,
			    gboolean         forward,
			    gboolean         start_check)
{
	static GtkTextIter iter;
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	AsyncData *data;

	if (start_check)
	{
		gtk_text_buffer_get_start_iter (text_buffer, &iter);
	}
	else if (!gtk_text_iter_forward_char (&iter))
	{
		gtk_main_quit ();
		return;
	}

	data = g_slice_new (AsyncData);
	data->results = results;
	data->result_num = gtk_text_iter_get_offset (&iter);
	data->forward = forward;

	if (forward)
	{
		gtk_source_buffer_forward_search_async (source_buffer,
							&iter,
							NULL,
							(GAsyncReadyCallback)finish_check_result,
							data);
	}
	else
	{
		gtk_source_buffer_backward_search_async (source_buffer,
							 &iter,
							 NULL,
							 (GAsyncReadyCallback)finish_check_result,
							 data);
	}
}

static void
test_forward_search (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gboolean wrap_around;

	static SearchResult results1[] =
	{
		{ 0, 2, TRUE },
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE }
	};

	static SearchResult results2[] =
	{
		{ 0, 2, TRUE },
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 0, FALSE },
		{ 0, 0, FALSE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");

	/* Wrap around: TRUE */

	gtk_source_buffer_set_search_wrap_around (source_buffer, TRUE);
	wrap_around = gtk_source_buffer_get_search_wrap_around (source_buffer);
	g_assert (wrap_around);

	check_search_results (source_buffer, results1, TRUE);

	g_test_trap_subprocess ("/Search/forward/subprocess/async-wrap-around", 0, 0);
	g_test_trap_assert_passed ();

	/* Wrap around: FALSE */

	gtk_source_buffer_set_search_wrap_around (source_buffer, FALSE);
	wrap_around = gtk_source_buffer_get_search_wrap_around (source_buffer);
	g_assert (!wrap_around);

	check_search_results (source_buffer, results2, TRUE);

	g_test_trap_subprocess ("/Search/forward/subprocess/async-normal", 0, 0);
	g_test_trap_assert_passed ();

	g_object_unref (source_buffer);
}

static void
test_async_forward_search_normal (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	static SearchResult results[] =
	{
		{ 0, 2, TRUE },
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 0, FALSE },
		{ 0, 0, FALSE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	gtk_source_buffer_set_search_wrap_around (source_buffer, FALSE);

	check_async_search_results (source_buffer, results, TRUE, TRUE);

	gtk_main ();
	g_object_unref (source_buffer);
}

static void
test_async_forward_search_wrap_around (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	static SearchResult results[] =
	{
		{ 0, 2, TRUE },
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	gtk_source_buffer_set_search_wrap_around (source_buffer, TRUE);

	check_async_search_results (source_buffer, results, TRUE, TRUE);

	gtk_main ();
	g_object_unref (source_buffer);
}

static void
test_backward_search (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	static SearchResult results1[] =
	{
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE },
		{ 2, 4, TRUE }
	};

	static SearchResult results2[] =
	{
		{ 0, 0, FALSE },
		{ 0, 0, FALSE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE },
		{ 2, 4, TRUE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");

	/* Wrap around: TRUE */

	gtk_source_buffer_set_search_wrap_around (source_buffer, TRUE);
	check_search_results (source_buffer, results1, FALSE);

	g_test_trap_subprocess ("/Search/backward/subprocess/async-wrap-around", 0, 0);
	g_test_trap_assert_passed ();

	/* Wrap around: FALSE */

	gtk_source_buffer_set_search_wrap_around (source_buffer, FALSE);
	check_search_results (source_buffer, results2, FALSE);

	g_test_trap_subprocess ("/Search/backward/subprocess/async-normal", 0, 0);
	g_test_trap_assert_passed ();

	g_object_unref (source_buffer);
}

static void
test_async_backward_search_normal (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	static SearchResult results[] =
	{
		{ 0, 0, FALSE },
		{ 0, 0, FALSE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE },
		{ 2, 4, TRUE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	gtk_source_buffer_set_search_wrap_around (source_buffer, FALSE);

	check_async_search_results (source_buffer, results, FALSE, TRUE);

	gtk_main ();
	g_object_unref (source_buffer);
}

static void
test_async_backward_search_wrap_around (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);

	static SearchResult results[] =
	{
		{ 2, 4, TRUE },
		{ 2, 4, TRUE },
		{ 0, 2, TRUE },
		{ 0, 2, TRUE },
		{ 2, 4, TRUE }
	};

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	gtk_source_buffer_set_search_wrap_around (source_buffer, TRUE);

	check_async_search_results (source_buffer, results, FALSE, TRUE);

	gtk_main ();
	g_object_unref (source_buffer);
}

static void
test_highlight (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	gboolean highlight;

	gtk_source_buffer_set_highlight_search (source_buffer, TRUE);
	highlight = gtk_source_buffer_get_highlight_search (source_buffer);
	g_assert (highlight);

	gtk_source_buffer_set_highlight_search (source_buffer, FALSE);
	highlight = gtk_source_buffer_get_highlight_search (source_buffer);
	g_assert (!highlight);

	g_object_unref (source_buffer);
}

static void
test_get_search_text (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	const gchar *search_text;

	search_text = gtk_source_buffer_get_search_text (source_buffer);
	g_assert (search_text == NULL);

	gtk_source_buffer_set_search_text (source_buffer, "");
	search_text = gtk_source_buffer_get_search_text (source_buffer);
	g_assert (search_text == NULL);

	gtk_source_buffer_set_search_text (source_buffer, "search-text");
	search_text = gtk_source_buffer_get_search_text (source_buffer);
	g_assert_cmpstr (search_text, ==, "search-text");

	g_object_unref (source_buffer);
}

static void
test_occurrence_position (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter start;
	GtkTextIter end;
	gint pos;

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	flush_queue ();

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	end = start;
	gtk_text_iter_forward_chars (&end, 2);

	pos = gtk_source_buffer_get_search_occurrence_position (source_buffer, &start, &end);
	g_assert_cmpint (pos, ==, 1);

	gtk_text_iter_forward_char (&start);
	gtk_text_iter_forward_char (&end);
	pos = gtk_source_buffer_get_search_occurrence_position (source_buffer, &start, &end);
	g_assert_cmpint (pos, ==, 0);

	gtk_text_iter_forward_char (&start);
	gtk_text_iter_forward_char (&end);
	pos = gtk_source_buffer_get_search_occurrence_position (source_buffer, &start, &end);
	g_assert_cmpint (pos, ==, 2);

	g_object_unref (source_buffer);
}

static void
test_replace (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkTextIter start;
	GtkTextIter end;
	gboolean replaced;
	gchar *contents;

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	flush_queue ();

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 1);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 3);

	replaced = gtk_source_buffer_search_replace (source_buffer, &start, &end, "bb", 2);
	g_assert (!replaced);

	gtk_text_buffer_get_iter_at_offset (text_buffer, &start, 2);
	gtk_text_buffer_get_iter_at_offset (text_buffer, &end, 4);

	replaced = gtk_source_buffer_search_replace (source_buffer, &start, &end, "bb", 2);
	g_assert (replaced);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_end_iter (text_buffer, &end);

	contents = gtk_text_iter_get_visible_text (&start, &end);
	g_assert_cmpstr (contents, ==, "aabb");

	g_free (contents);
	g_object_unref (source_buffer);
}

static void
test_replace_all (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gint nb_replacements;
	GtkTextIter start;
	GtkTextIter end;
	gchar *contents;

	gtk_text_buffer_set_text (text_buffer, "aaaa", -1);
	gtk_source_buffer_set_search_text (source_buffer, "aa");
	flush_queue ();

	nb_replacements = gtk_source_buffer_search_replace_all (source_buffer, "bb", 2);
	g_assert_cmpint (nb_replacements, ==, 2);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_end_iter (text_buffer, &end);

	contents = gtk_text_iter_get_visible_text (&start, &end);
	g_assert_cmpstr (contents, ==, "bbbb");

	g_free (contents);
	g_object_unref (source_buffer);
}

static void
test_regex (void)
{
	GtkSourceBuffer *source_buffer = gtk_source_buffer_new (NULL);
	GtkTextBuffer *text_buffer = GTK_TEXT_BUFFER (source_buffer);
	gboolean regex_enabled;
	gint occurrences_count;
	GtkTextIter start;
	GtkTextIter end;
	gchar *contents;

	gtk_text_buffer_set_text (text_buffer, "hello\nworld\n", -1);
	gtk_source_buffer_set_regex_search (source_buffer, TRUE);
	regex_enabled = gtk_source_buffer_get_regex_search (source_buffer);
	g_assert (regex_enabled);

	/* Simple regex */

	gtk_source_buffer_set_search_text (source_buffer, "\\w+");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 2);

	/* Test partial matching */

	gtk_source_buffer_set_search_text (source_buffer, "(.*\n)*");
	flush_queue ();
	occurrences_count = gtk_source_buffer_get_search_occurrences_count (source_buffer);
	g_assert_cmpint (occurrences_count, ==, 1);

	/* Test replace */

	gtk_text_buffer_set_text (text_buffer, "aa#bb", -1);
	gtk_source_buffer_set_search_text (source_buffer, "(\\w+)#(\\w+)");
	flush_queue ();

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_end_iter (text_buffer, &end);
	gtk_source_buffer_search_replace (source_buffer, &start, &end, "\\2#\\1", -1);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_end_iter (text_buffer, &end);
	contents = gtk_text_iter_get_visible_text (&start, &end);
	g_assert_cmpstr (contents, ==, "bb#aa");
	g_free (contents);

	/* Test replace all */

	gtk_text_buffer_set_text (text_buffer, "aa#bb cc#dd", -1);
	gtk_source_buffer_search_replace_all (source_buffer, "\\2#\\1", -1);

	gtk_text_buffer_get_start_iter (text_buffer, &start);
	gtk_text_buffer_get_end_iter (text_buffer, &end);
	contents = gtk_text_iter_get_visible_text (&start, &end);
	g_assert_cmpstr (contents, ==, "bb#aa dd#cc");
	g_free (contents);

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
	g_test_add_func ("/Search/case-sensitivity", test_case_sensitivity);
	g_test_add_func ("/Search/at-word-boundaries", test_search_at_word_boundaries);
	g_test_add_func ("/Search/forward", test_forward_search);
	g_test_add_func ("/Search/forward/subprocess/async-normal", test_async_forward_search_normal);
	g_test_add_func ("/Search/forward/subprocess/async-wrap-around", test_async_forward_search_wrap_around);
	g_test_add_func ("/Search/backward", test_backward_search);
	g_test_add_func ("/Search/backward/subprocess/async-normal", test_async_backward_search_normal);
	g_test_add_func ("/Search/backward/subprocess/async-wrap-around", test_async_backward_search_wrap_around);
	g_test_add_func ("/Search/highlight", test_highlight);
	g_test_add_func ("/Search/get-search-text", test_get_search_text);
	g_test_add_func ("/Search/occurrence-position", test_occurrence_position);
	g_test_add_func ("/Search/replace", test_replace);
	g_test_add_func ("/Search/replace", test_replace_all);
	g_test_add_func ("/Search/regex", test_regex);

	return g_test_run ();
}

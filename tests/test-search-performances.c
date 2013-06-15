/*
 * test-search-performances.c
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

/* This measures the execution times for basic search (with
 * gtk_text_iter_forward_search()), and "smart" search (the first search with
 * gtk_text_iter_forward_search(), later searches with
 * gtk_text_iter_forward_to_tag_toggle()).
 * For the "smart" search, only the first search is measured. Later searches
 * are really fast (going to the previous/next occurrence is done in O(1)).
 * Different search flags are also tested. We can see a big difference between
 * the case sensitive search and case insensitive.
 */

static void
on_notify_search_occurrences_count_cb (GtkSourceBuffer *buffer,
				       GParamSpec      *spec,
				       GTimer          *timer)
{
	g_print ("smart asynchronous search, case sensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter match_end;
	GTimer *timer;
	gint i;
	GtkTextSearchFlags flags;

	gtk_init (&argc, &argv);

	buffer = gtk_source_buffer_new (NULL);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	for (i = 0; i < 1000000; i++)
	{
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
					&iter,
					"A line of text to fill the text buffer. Is it long enough?\n",
					-1);
	}

	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "foo\n", -1);

	/* Basic search, no flags */

	timer = g_timer_new ();

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = 0;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, no flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Basic search, with flags always enabled by gsv */

	g_timer_start (timer);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = GTK_TEXT_SEARCH_VISIBLE_ONLY | GTK_TEXT_SEARCH_TEXT_ONLY;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, visible and text only flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Basic search, with default flags in gsv */

	g_timer_start (timer);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	flags = GTK_TEXT_SEARCH_VISIBLE_ONLY |
		GTK_TEXT_SEARCH_TEXT_ONLY |
		GTK_TEXT_SEARCH_CASE_INSENSITIVE;

	while (gtk_text_iter_forward_search (&iter, "foo", flags, NULL, &match_end, NULL))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("basic forward search, all flags: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Smart forward search, with default flags in gsv */

	g_timer_start (timer);

	gtk_source_buffer_set_search_wrap_around (buffer, FALSE);
	gtk_source_buffer_set_search_text (buffer, "foo");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_buffer_forward_search (buffer, &iter, NULL, &match_end))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("smart synchronous forward search, case insensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Smart forward search, case sensitive */

	g_timer_start (timer);

	gtk_source_buffer_set_search_text (buffer, NULL);
	gtk_source_buffer_set_case_sensitive_search (buffer, TRUE);
	gtk_source_buffer_set_search_text (buffer, "foo");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	while (gtk_source_buffer_forward_search (buffer, &iter, NULL, &match_end))
	{
		iter = match_end;
	}

	g_timer_stop (timer);
	g_print ("smart synchronous forward search, case sensitive: %lf seconds.\n",
		 g_timer_elapsed (timer, NULL));

	/* Smart search, case sensitive, asynchronous */

	/* The asynchronous overhead doesn't depend on the search flags, it
	 * depends on the maximum number of lines to scan in one batch, and
	 * (obviously), on the buffer size.
	 * You can tune SCAN_BATCH_SIZE in gtksourcesearch.c to see a difference
	 * in the overhead.
	 */

	g_signal_connect (buffer,
			  "notify::search-occurrences-count",
			  G_CALLBACK (on_notify_search_occurrences_count_cb),
			  timer);

	g_timer_start (timer);

	gtk_source_buffer_set_search_text (buffer, NULL);
	gtk_source_buffer_set_search_text (buffer, "foo");

	gtk_main ();
	return 0;
}

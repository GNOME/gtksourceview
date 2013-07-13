/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcesearch.c
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

#include "gtksourcesearch.h"
#include "gtksourcebuffer.h"
#include "gtksourcestylescheme.h"
#include "gtktextregion.h"
#include "gtksourcestyle-private.h"

#include <string.h>

/* Implementation overview:
 *
 * When the state of the search changes (the text to search or the options), we
 * have to update the highlighting and the properties values (the number of
 * occurrences). To do so, a simple solution is to first remove all the
 * found_tag, so we have a clean buffer to analyze. The problem with this
 * solution is that there is some flickering when the user modifies the text to
 * search, because removing all the found_tag's can take some time. So we keep
 * the old found_tag's, and when we must highlight the matches in a certain
 * region, we first remove the found_tag's in this region and then we highlight
 * the newly found matches by applying the found_tag to them.
 *
 * If we only want to highlight the matches, without counting the number of
 * occurrences, a good solution would be to highlight only the visible region of
 * the buffer on the screen. So it would be useless to always scan all the
 * buffer.
 *
 * But we actually want the number of occurrences! So we have to scan all the
 * buffer. When the state of the search changes, an idle callback is installed,
 * which will scan the buffer to highlight the matches. To avoid flickering, the
 * visible region on the screen is put in a higher priority region to highlight,
 * so the idle callback will first scan this region.
 *
 * Why highlighting the non-visible matches? What we want is to (1) highlight
 * the visible matches and (2) count the number of occurrences. The code would
 * indeed be simpler if these two tasks were clearly separated (in two different
 * idle callbacks, with different regions to scan). With this simpler solution,
 * we would always use forward_search() and backward_search() to navigate
 * through the occurrences. But we can do better than that!
 * forward_to_tag_toggle() and backward_to_tag_toggle() are far more efficient:
 * once the buffer has been scanned, going to the previous or the next
 * occurrence is done in O(1). We must just pay attention to contiguous matches.
 *
 * While the user is typing the text in the search entry, the buffer is scanned
 * to count the number of occurrences. And when the user wants to do an
 * operation (go to the next occurrence for example), chances are that the
 * buffer has already been scanned entirely, so almost all the operations will
 * be really fast.
 *
 * Extreme example:
 * <occurrence> [1 GB of text] <next-occurrence>
 * Once the buffer is scanned, switching between the occurrences will be almost
 * instantaneous.
 *
 * So how to count the number of occurrences then? Remember that the buffer
 * contents can be modified during the scan, and that we keep the old
 * found_tag's. Moreover, when we encounter an old found_tag region, in the
 * general case we can not say how many occurrences there are in this region,
 * since a found_tag region can contain contiguous matches. Take for example the
 * found_tag region "aa": was it the "aa" search match, or two times "a"?
 * The implemented solution is to set occurrences_count to 0 when the search
 * state changes, even if old matches are still there. Because it is not
 * possible to count the old matches to decrement occurrences_count (and storing
 * the previous search text would not be sufficient, because even older matches
 * can still be there). To increment and decrement occurrences_count, there is
 * the scan_region, the region to scan. If an occurrence is contained in
 * scan_region, it means that it has not already been scanned, so
 * occurrences_count doesn't take into account this occurrence. On the other
 * hand, if we find an occurrence outside scan_region, the occurrence is
 * normally correctly highlighted, and occurrences_count take it into account.
 *
 * So when we highlight or when we remove the highlight of an occurrence (on
 * text insertion, deletion, when scanning, etc.), we increment or decrement
 * occurrences_count depending on whether the occurrence was already taken into
 * account by occurrences_count.
 *
 * If the code seems too complicated and contains strange bugs, you have two
 * choices:
 * - Write more unit tests, understand correctly the code and fix it.
 * - Rewrite the code to implement the simpler solution explained above :-)
 */

/*
#define ENABLE_DEBUG
*/
#undef ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

/* Maximum number of lines to scan in one batch.
 * A lower value means more overhead when scanning the buffer asynchronously.
 */
#define SCAN_BATCH_SIZE 100

struct _GtkSourceSearchPrivate
{
	GtkTextBuffer *buffer;

	/* The region to scan and highlight. If NULL, the scan is finished. */
	GtkTextRegion *scan_region;

	/* The region to scan and highlight in priority. I.e. the visible part
	 * of the buffer on the screen.
	 */
	GtkTextRegion *high_priority_region;

	/* An asynchronous running task. task_region has a higher priority than
	 * scan_region, but a lower priority than high_priority_region.
	 */
	GTask *task;
	GtkTextRegion *task_region;

	gulong idle_scan_id;

	gint occurrences_count;

	GtkTextTag *found_tag;

	/* State of the search. If text is NULL, the search is disabled. */
	gchar *text;
	gint text_nb_lines;
	GtkTextSearchFlags flags;
	guint at_word_boundaries : 1;
	guint wrap_around : 1;
	guint highlight : 1;
};

/* Data for the asynchronous forward and backward search tasks. */
typedef struct
{
	GtkTextMark *start_at;
	GtkTextIter match_start;
	GtkTextIter match_end;
	guint found : 1;
	guint wrapped_around : 1;

	/* forward or backward */
	guint is_forward : 1;
} ForwardBackwardData;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceSearch, _gtk_source_search, G_TYPE_OBJECT);

static void		install_idle_scan		(GtkSourceSearch *search);

static gboolean
dispose_has_run (GtkSourceSearch *search)
{
	return search->priv->buffer == NULL;
}

static void
sync_found_tag (GtkSourceSearch *search)
{
	GtkSourceStyleScheme *style_scheme;
	GtkSourceStyle *style = NULL;

	if (dispose_has_run (search))
	{
		return;
	}

	if (!search->priv->highlight)
	{
		_gtk_source_style_apply (NULL, search->priv->found_tag);
		return;
	}

	style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (search->priv->buffer));

	if (style_scheme != NULL)
	{
		style = gtk_source_style_scheme_get_style (style_scheme, "search-match");
	}

	if (style == NULL)
	{
		g_warning ("search-match style not available.");
	}

	_gtk_source_style_apply (style, search->priv->found_tag);
}

/* Make sure to call this function when the buffer is constructed, else the tag
 * table is created too early.
 */
static void
init_found_tag (GtkSourceSearch *search)
{
	search->priv->found_tag = gtk_text_buffer_create_tag (search->priv->buffer, NULL, NULL);

	sync_found_tag (search);

	g_signal_connect_object (search->priv->buffer,
				 "notify::style-scheme",
				 G_CALLBACK (sync_found_tag),
				 search,
				 G_CONNECT_SWAPPED);
}

static void
text_tag_set_highest_priority (GtkTextTag    *tag,
			       GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	gint n;

	table = gtk_text_buffer_get_tag_table (buffer);
	n = gtk_text_tag_table_get_size (table);
	gtk_text_tag_set_priority (tag, n - 1);
}

/* A TextRegion can contain empty subregions. So checking the number of
 * subregions is not sufficient.
 * When calling gtk_text_region_add() with equal iters, the subregion is not
 * added. But when a subregion becomes empty, due to text deletion, the
 * subregion is not removed from the TextRegion.
 */
static gboolean
is_text_region_empty (GtkTextRegion *region)
{
	GtkTextRegionIterator region_iter;

	if (region == NULL)
	{
		return TRUE;
	}

	gtk_text_region_get_iterator (region, &region_iter, 0);

	while (!gtk_text_region_iterator_is_end (&region_iter))
	{
		GtkTextIter region_start;
		GtkTextIter region_end;

		gtk_text_region_iterator_get_subregion (&region_iter,
							&region_start,
							&region_end);

		if (!gtk_text_iter_equal (&region_start, &region_end))
		{
			return FALSE;
		}

		gtk_text_region_iterator_next (&region_iter);
	}

	return TRUE;
}

/* Sets @start and @end to the first non-empty subregion.
 * Returns FALSE if the region is empty.
 */
static gboolean
get_first_subregion (GtkTextRegion *region,
		     GtkTextIter   *start,
		     GtkTextIter   *end)
{
	GtkTextRegionIterator region_iter;

	if (region == NULL)
	{
		return FALSE;
	}

	gtk_text_region_get_iterator (region, &region_iter, 0);

	while (!gtk_text_region_iterator_is_end (&region_iter))
	{
		gtk_text_region_iterator_get_subregion (&region_iter, start, end);

		if (!gtk_text_iter_equal (start, end))
		{
			return TRUE;
		}

		gtk_text_region_iterator_next (&region_iter);
	}

	return FALSE;
}

/* Sets @start and @end to the last non-empty subregion.
 * Returns FALSE if the region is empty.
 */
static gboolean
get_last_subregion (GtkTextRegion *region,
		    GtkTextIter   *start,
		    GtkTextIter   *end)
{
	GtkTextRegionIterator region_iter;
	gboolean found = FALSE;

	if (region == NULL)
	{
		return FALSE;
	}

	gtk_text_region_get_iterator (region, &region_iter, 0);

	while (!gtk_text_region_iterator_is_end (&region_iter))
	{
		GtkTextIter start_subregion;
		GtkTextIter end_subregion;

		gtk_text_region_iterator_get_subregion (&region_iter,
							&start_subregion,
							&end_subregion);

		if (!gtk_text_iter_equal (&start_subregion, &end_subregion))
		{
			found = TRUE;
			*start = start_subregion;
			*end = end_subregion;
		}

		gtk_text_region_iterator_next (&region_iter);
	}

	return found;
}

static void
clear_task (GtkSourceSearch *search)
{
	if (search->priv->task_region != NULL)
	{
		gtk_text_region_destroy (search->priv->task_region, TRUE);
		search->priv->task_region = NULL;
	}

	if (search->priv->task != NULL)
	{
		GCancellable *cancellable = g_task_get_cancellable (search->priv->task);

		if (cancellable != NULL)
		{
			g_cancellable_cancel (cancellable);
			g_task_return_error_if_cancelled (search->priv->task);
		}

		g_clear_object (&search->priv->task);
	}
}

static void
clear_search (GtkSourceSearch *search)
{
	if (search->priv->scan_region != NULL)
	{
		gtk_text_region_destroy (search->priv->scan_region, TRUE);
		search->priv->scan_region = NULL;
	}

	if (search->priv->high_priority_region != NULL)
	{
		gtk_text_region_destroy (search->priv->high_priority_region, TRUE);
		search->priv->high_priority_region = NULL;
	}

	if (search->priv->idle_scan_id != 0)
	{
		g_source_remove (search->priv->idle_scan_id);
		search->priv->idle_scan_id = 0;
	}

	clear_task (search);

	search->priv->occurrences_count = 0;
}

static gboolean
basic_forward_search (GtkSourceSearch   *search,
		      const GtkTextIter *iter,
		      GtkTextIter       *match_start,
		      GtkTextIter       *match_end,
		      const GtkTextIter *limit)
{
	GtkTextIter begin_search = *iter;

	if (search->priv->text == NULL)
	{
		return FALSE;
	}

	while (TRUE)
	{
		gboolean found = gtk_text_iter_forward_search (&begin_search,
							       search->priv->text,
							       search->priv->flags,
							       match_start,
							       match_end,
							       limit);

		if (!found || !search->priv->at_word_boundaries)
		{
			return found;
		}

		if (gtk_text_iter_starts_word (match_start) &&
		    gtk_text_iter_ends_word (match_end))
		{
			return TRUE;
		}

		begin_search = *match_end;
	}
}

static gboolean
basic_backward_search (GtkSourceSearch   *search,
		       const GtkTextIter *iter,
		       GtkTextIter       *match_start,
		       GtkTextIter       *match_end,
		       const GtkTextIter *limit)
{
	GtkTextIter begin_search = *iter;

	if (search->priv->text == NULL)
	{
		return FALSE;
	}

	while (TRUE)
	{
		gboolean found = gtk_text_iter_backward_search (&begin_search,
							        search->priv->text,
							        search->priv->flags,
							        match_start,
							        match_end,
							        limit);

		if (!found || !search->priv->at_word_boundaries)
		{
			return found;
		}

		if (gtk_text_iter_starts_word (match_start) &&
		    gtk_text_iter_ends_word (match_end))
		{
			return TRUE;
		}

		begin_search = *match_start;
	}
}

static void
forward_backward_data_free (ForwardBackwardData *data)
{
	if (data->start_at != NULL)
	{
		GtkTextBuffer *buffer = gtk_text_mark_get_buffer (data->start_at);
		gtk_text_buffer_delete_mark (buffer, data->start_at);
	}

	g_slice_free (ForwardBackwardData, data);
}

/* Returns TRUE if finished. */
static gboolean
smart_forward_search_async_step (GtkSourceSearch *search,
				 GtkTextIter     *start_at,
				 gboolean        *wrapped_around)
{
	GtkTextIter iter = *start_at;
	GtkTextIter limit;
	GtkTextRegion *region = NULL;
	ForwardBackwardData *task_data;

	if (gtk_text_iter_is_end (start_at))
	{
		if (search->priv->text != NULL &&
		    !*wrapped_around &&
		    search->priv->wrap_around)
		{
			gtk_text_buffer_get_start_iter (search->priv->buffer, start_at);
			*wrapped_around = TRUE;
			return FALSE;
		}

		task_data = g_slice_new0 (ForwardBackwardData);
		task_data->found = FALSE;
		task_data->is_forward = TRUE;
		task_data->wrapped_around = *wrapped_around;

		g_task_return_pointer (search->priv->task,
				       task_data,
				       (GDestroyNotify)forward_backward_data_free);

		g_clear_object (&search->priv->task);
		return TRUE;
	}

	if (!gtk_text_iter_has_tag (&iter, search->priv->found_tag))
	{
		gtk_text_iter_forward_to_tag_toggle (&iter, search->priv->found_tag);
	}

	limit = iter;
	gtk_text_iter_forward_to_tag_toggle (&limit, search->priv->found_tag);

	if (search->priv->scan_region != NULL)
	{
		region = gtk_text_region_intersect (search->priv->scan_region, start_at, &limit);
	}

	if (is_text_region_empty (region))
	{
		GtkTextIter match_start;
		GtkTextIter match_end;
		gboolean found;

		found = basic_forward_search (search, &iter, &match_start, &match_end, &limit);

		if (region != NULL)
		{
			gtk_text_region_destroy (region, TRUE);
		}

		if (found)
		{
			task_data = g_slice_new0 (ForwardBackwardData);
			task_data->found = TRUE;
			task_data->match_start = match_start;
			task_data->match_end = match_end;
			task_data->is_forward = TRUE;
			task_data->wrapped_around = *wrapped_around;

			g_task_return_pointer (search->priv->task,
					       task_data,
					       (GDestroyNotify)forward_backward_data_free);

			g_clear_object (&search->priv->task);
			return TRUE;
		}

		*start_at = limit;
		return FALSE;
	}

	task_data = g_slice_new0 (ForwardBackwardData);
	task_data->is_forward = TRUE;
	task_data->wrapped_around = *wrapped_around;
	task_data->start_at = gtk_text_buffer_create_mark (search->priv->buffer,
							   NULL,
							   start_at,
							   TRUE);

	g_task_set_task_data (search->priv->task,
			      task_data,
			      (GDestroyNotify)forward_backward_data_free);

	if (search->priv->task_region != NULL)
	{
		gtk_text_region_destroy (search->priv->task_region, TRUE);
	}

	search->priv->task_region = region;

	install_idle_scan (search);

	/* The idle that scan the task region will call
	 * smart_forward_search_async() to continue the task. But for the
	 * moment, we are done.
	 */
	return TRUE;
}

static void
smart_forward_search_async (GtkSourceSearch   *search,
			    const GtkTextIter *start_at,
			    gboolean           wrapped_around)
{
	GtkTextIter iter = *start_at;

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	/* A recursive function would have been more natural, but a loop is
	 * better to avoid stack overflows.
	 */
	while (!smart_forward_search_async_step (search, &iter, &wrapped_around));
}


/* Returns TRUE if finished. */
static gboolean
smart_backward_search_async_step (GtkSourceSearch *search,
				  GtkTextIter     *start_at,
				  gboolean        *wrapped_around)
{
	GtkTextIter iter = *start_at;
	GtkTextIter limit;
	GtkTextRegion *region = NULL;
	ForwardBackwardData *task_data;

	if (gtk_text_iter_is_start (start_at))
	{
		if (search->priv->text != NULL &&
		    !*wrapped_around &&
		    search->priv->wrap_around)
		{
			gtk_text_buffer_get_end_iter (search->priv->buffer, start_at);
			*wrapped_around = TRUE;
			return FALSE;
		}

		task_data = g_slice_new0 (ForwardBackwardData);
		task_data->found = FALSE;
		task_data->is_forward = FALSE;
		task_data->wrapped_around = *wrapped_around;

		g_task_return_pointer (search->priv->task,
				       task_data,
				       (GDestroyNotify)forward_backward_data_free);

		g_clear_object (&search->priv->task);
		return TRUE;
	}

	if (gtk_text_iter_begins_tag (&iter, search->priv->found_tag) ||
	    (!gtk_text_iter_has_tag (&iter, search->priv->found_tag) &&
	     !gtk_text_iter_ends_tag (&iter, search->priv->found_tag)))
	{
		gtk_text_iter_backward_to_tag_toggle (&iter, search->priv->found_tag);
	}

	limit = iter;
	gtk_text_iter_backward_to_tag_toggle (&limit, search->priv->found_tag);

	if (search->priv->scan_region != NULL)
	{
		region = gtk_text_region_intersect (search->priv->scan_region, &limit, start_at);
	}

	if (is_text_region_empty (region))
	{
		GtkTextIter match_start;
		GtkTextIter match_end;
		gboolean found;

		found = basic_backward_search (search, &iter, &match_start, &match_end, &limit);

		if (region != NULL)
		{
			gtk_text_region_destroy (region, TRUE);
		}

		if (found)
		{
			task_data = g_slice_new0 (ForwardBackwardData);
			task_data->found = TRUE;
			task_data->match_start = match_start;
			task_data->match_end = match_end;
			task_data->is_forward = FALSE;
			task_data->wrapped_around = *wrapped_around;

			g_task_return_pointer (search->priv->task,
					       task_data,
					       (GDestroyNotify)forward_backward_data_free);

			g_clear_object (&search->priv->task);
			return TRUE;
		}

		*start_at = limit;
		return FALSE;
	}

	task_data = g_slice_new0 (ForwardBackwardData);
	task_data->is_forward = FALSE;
	task_data->wrapped_around = *wrapped_around;
	task_data->start_at = gtk_text_buffer_create_mark (search->priv->buffer,
							   NULL,
							   start_at,
							   TRUE);

	g_task_set_task_data (search->priv->task,
			      task_data,
			      (GDestroyNotify)forward_backward_data_free);

	if (search->priv->task_region != NULL)
	{
		gtk_text_region_destroy (search->priv->task_region, TRUE);
	}

	search->priv->task_region = region;

	install_idle_scan (search);

	/* The idle that scan the task region will call
	 * smart_backward_search_async() to continue the task. But for the
	 * moment, we are done.
	 */
	return TRUE;
}

static void
smart_backward_search_async (GtkSourceSearch   *search,
			     const GtkTextIter *start_at,
			     gboolean           wrapped_around)
{
	GtkTextIter iter = *start_at;

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	/* A recursive function would have been more natural, but a loop is
	 * better to avoid stack overflows.
	 */
	while (!smart_backward_search_async_step (search, &iter, &wrapped_around));
}

/* Adjust the subregion so we are sure that all matches that are visible or
 * partially visible between @start and @end are highlighted.
 */
static void
adjust_subregion (GtkSourceSearch *search,
		  GtkTextIter     *start,
		  GtkTextIter     *end)
{
	DEBUG ({
		g_print ("adjust_subregion(), before adjusting: [%u (%u), %u (%u)]\n",
			 gtk_text_iter_get_line (start), gtk_text_iter_get_offset (start),
			 gtk_text_iter_get_line (end), gtk_text_iter_get_offset (end));
	});

	gtk_text_iter_backward_lines (start, MAX (0, search->priv->text_nb_lines - 1));
	gtk_text_iter_forward_lines (end, MAX (0, search->priv->text_nb_lines - 1));

	if (!gtk_text_iter_starts_line (start))
	{
		gtk_text_iter_set_line_offset (start, 0);
	}

	if (!gtk_text_iter_ends_line (end))
	{
		gtk_text_iter_forward_to_line_end (end);
	}

	/* When we are in the middle of a found_tag, a simple solution is to
	 * always backward_to_tag_toggle(). The problem is that occurrences can
	 * be contiguous. So a full scan of the buffer can have a O(n^2) in the
	 * worst case, if we use the simple solution. Therefore we use a more
	 * complicated solution, that checks if we are in an old found_tag or
	 * not.
	 */

	if (gtk_text_iter_has_tag (start, search->priv->found_tag))
	{
		if (is_text_region_empty (search->priv->scan_region))
		{
			/* 'start' is in a correct match, we can skip it. */
			gtk_text_iter_forward_to_tag_toggle (start, search->priv->found_tag);
		}
		else
		{
			GtkTextIter tag_start = *start;
			GtkTextIter tag_end = *start;
			GtkTextRegion *region;

			if (!gtk_text_iter_begins_tag (&tag_start, search->priv->found_tag))
			{
				gtk_text_iter_backward_to_tag_toggle (&tag_start, search->priv->found_tag);
			}

			gtk_text_iter_forward_to_tag_toggle (&tag_end, search->priv->found_tag);

			region = gtk_text_region_intersect (search->priv->scan_region,
							    &tag_start,
							    &tag_end);

			if (is_text_region_empty (region))
			{
				/* 'region' has already been scanned, so 'start' is in a
				 * correct match, we can skip it.
				 */
				*start = tag_end;
			}
			else
			{
				/* 'region' has not already been scanned completely, so
				 * 'start' is most probably in an old match that must be
				 * removed.
				 */
				*start = tag_start;
			}

			if (region != NULL)
			{
				gtk_text_region_destroy (region, TRUE);
			}
		}
	}

	/* Symmetric for 'end'. */

	if (gtk_text_iter_has_tag (end, search->priv->found_tag))
	{
		if (is_text_region_empty (search->priv->scan_region))
		{
			/* 'end' is in a correct match, we can skip it. */

			if (!gtk_text_iter_begins_tag (end, search->priv->found_tag))
			{
				gtk_text_iter_backward_to_tag_toggle (end, search->priv->found_tag);
			}
		}
		else
		{
			GtkTextIter tag_start = *end;
			GtkTextIter tag_end = *end;
			GtkTextRegion *region;

			if (!gtk_text_iter_begins_tag (&tag_start, search->priv->found_tag))
			{
				gtk_text_iter_backward_to_tag_toggle (&tag_start, search->priv->found_tag);
			}

			gtk_text_iter_forward_to_tag_toggle (&tag_end, search->priv->found_tag);

			region = gtk_text_region_intersect (search->priv->scan_region,
							    &tag_start,
							    &tag_end);

			if (is_text_region_empty (region))
			{
				/* 'region' has already been scanned, so 'end' is in a
				 * correct match, we can skip it.
				 */
				*end = tag_start;
			}
			else
			{
				/* 'region' has not already been scanned completely, so
				 * 'end' is most probably in an old match that must be
				 * removed.
				 */
				*end = tag_end;
			}

			if (region != NULL)
			{
				gtk_text_region_destroy (region, TRUE);
			}
		}
	}

	DEBUG ({
		g_print ("adjust_subregion(), after adjusting: [%u (%u), %u (%u)]\n",
			 gtk_text_iter_get_line (start), gtk_text_iter_get_offset (start),
			 gtk_text_iter_get_line (end), gtk_text_iter_get_offset (end));
	});
}

/* Do not take into account the scan_region. Search just with
 * forward_to_tag_toggle(), and checks that it is not an old match.
 */
static gboolean
smart_forward_search_without_scanning (GtkSourceSearch   *search,
				       const GtkTextIter *start_at,
				       GtkTextIter       *match_start,
				       GtkTextIter       *match_end,
				       const GtkTextIter *stop_at)
{
	GtkTextIter iter = *start_at;

	g_assert (start_at != NULL);
	g_assert (stop_at != NULL);

	if (search->priv->text == NULL)
	{
		return FALSE;
	}

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	while (gtk_text_iter_compare (&iter, stop_at) < 0)
	{
		GtkTextIter limit;

		if (!gtk_text_iter_has_tag (&iter, search->priv->found_tag))
		{
			gtk_text_iter_forward_to_tag_toggle (&iter, search->priv->found_tag);
		}

		limit = iter;
		gtk_text_iter_forward_to_tag_toggle (&limit, search->priv->found_tag);

		if (gtk_text_iter_compare (stop_at, &limit) < 0)
		{
			limit = *stop_at;
		}

		if (basic_forward_search (search, &iter, match_start, match_end, &limit))
		{
			return TRUE;
		}

		iter = limit;
	}

	return FALSE;
}

/* Remove the occurrences in the range. @start and @end may be adjusted, if they
 * are in a found_tag region.
 */
static void
remove_occurrences_in_range (GtkSourceSearch *search,
			     GtkTextIter     *start,
			     GtkTextIter     *end)
{
	GtkTextIter iter;
	GtkTextIter match_start;
	GtkTextIter match_end;

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	if (gtk_text_iter_has_tag (start, search->priv->found_tag) &&
	    !gtk_text_iter_begins_tag (start, search->priv->found_tag))
	{
		gtk_text_iter_backward_to_tag_toggle (start, search->priv->found_tag);
	}

	if (gtk_text_iter_has_tag (end, search->priv->found_tag) &&
	    !gtk_text_iter_begins_tag (end, search->priv->found_tag))
	{
		gtk_text_iter_forward_to_tag_toggle (end, search->priv->found_tag);
	}

	iter = *start;

	while (smart_forward_search_without_scanning (search, &iter, &match_start, &match_end, end))
	{
		if (search->priv->scan_region == NULL)
		{
			/* The occurrence has already been scanned, and thus
			 * occurrence_count take it into account. */
			search->priv->occurrences_count--;
		}
		else
		{
			GtkTextRegion *region = gtk_text_region_intersect (search->priv->scan_region,
									   &match_start,
									   &match_end);

			if (is_text_region_empty (region))
			{
				search->priv->occurrences_count--;
			}

			if (region != NULL)
			{
				gtk_text_region_destroy (region, TRUE);
			}
		}

		iter = match_end;
	}

	gtk_text_buffer_remove_tag (search->priv->buffer,
				    search->priv->found_tag,
				    start,
				    end);
}

static void
scan_subregion (GtkSourceSearch *search,
		GtkTextIter     *start,
		GtkTextIter     *end)
{
	GtkTextIter iter;
	GtkTextIter *limit;
	gboolean found = TRUE;

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	/* Make sure the 'found' tag has the priority over syntax highlighting
	 * tags. */
	text_tag_set_highest_priority (search->priv->found_tag,
				       search->priv->buffer);

	adjust_subregion (search, start, end);
	remove_occurrences_in_range (search, start, end);

	if (search->priv->scan_region != NULL)
	{
		DEBUG ({
			g_print ("Region to scan, before:\n");
			gtk_text_region_debug_print (search->priv->scan_region);
		});

		gtk_text_region_subtract (search->priv->scan_region, start, end);

		DEBUG ({
			g_print ("Region to scan, after:\n");
			gtk_text_region_debug_print (search->priv->scan_region);
		});
	}

	if (search->priv->task_region != NULL)
	{
		gtk_text_region_subtract (search->priv->task_region, start, end);
	}

	if (search->priv->text == NULL)
	{
		/* We have removed the found_tag, we are done. */
		return;
	}

	iter = *start;

	if (gtk_text_iter_is_end (end))
	{
		limit = NULL;
	}
	else
	{
		limit = end;
	}

	do
	{
		GtkTextIter match_start;
		GtkTextIter match_end;

		found = basic_forward_search (search, &iter, &match_start, &match_end, limit);

		if (found)
		{
			gtk_text_buffer_apply_tag (search->priv->buffer,
						   search->priv->found_tag,
						   &match_start,
						   &match_end);

			search->priv->occurrences_count++;
		}

		iter = match_end;

	} while (found);
}

static void
scan_all_region (GtkSourceSearch *search,
		 GtkTextRegion   *region_to_highlight)
{
	gint nb_subregions = gtk_text_region_subregions (region_to_highlight);
	GtkTextIter start_search;
	GtkTextIter end_search;

	if (nb_subregions == 0)
	{
		return;
	}

	gtk_text_region_nth_subregion (region_to_highlight,
				       0,
				       &start_search,
				       NULL);

	gtk_text_region_nth_subregion (region_to_highlight,
				       nb_subregions - 1,
				       NULL,
				       &end_search);

	gtk_text_iter_order (&start_search, &end_search);

	scan_subregion (search, &start_search, &end_search);
}

/* Scan a chunk of the region. If the region is small enough, all the region
 * will be scanned. But if the region is big, scanning only the chunk will not
 * block the UI normally. Begin the scan at the beginning of the region.
 */
static void
scan_region_forward (GtkSourceSearch *search,
		     GtkTextRegion   *region)
{
	gint nb_remaining_lines = SCAN_BATCH_SIZE;
	GtkTextIter start;
	GtkTextIter end;

	while (nb_remaining_lines > 0 &&
	       get_first_subregion (region, &start, &end))
	{
		GtkTextIter limit = start;
		gint start_line;
		gint limit_line;

		gtk_text_iter_forward_lines (&limit, nb_remaining_lines);

		if (gtk_text_iter_compare (&end, &limit) < 0)
		{
			limit = end;
		}

		scan_subregion (search, &start, &limit);

		start_line = gtk_text_iter_get_line (&start);
		limit_line = gtk_text_iter_get_line (&limit);

		nb_remaining_lines -= limit_line - start_line;
	}
}

/* Same as scan_region_forward(), but begins the scan at the end of the region. */
static void
scan_region_backward (GtkSourceSearch *search,
		      GtkTextRegion   *region)
{
	gint nb_remaining_lines = SCAN_BATCH_SIZE;
	GtkTextIter start;
	GtkTextIter end;

	while (nb_remaining_lines > 0 &&
	       get_last_subregion (region, &start, &end))
	{
		GtkTextIter limit = end;
		gint limit_line;
		gint end_line;

		gtk_text_iter_backward_lines (&limit, nb_remaining_lines);

		if (gtk_text_iter_compare (&limit, &start) < 0)
		{
			limit = start;
		}

		scan_subregion (search, &limit, &end);

		limit_line = gtk_text_iter_get_line (&limit);
		end_line = gtk_text_iter_get_line (&end);

		nb_remaining_lines -= end_line - limit_line;
	}
}

static void
scan_task_region (GtkSourceSearch *search)
{
	ForwardBackwardData *task_data = g_task_get_task_data (search->priv->task);
	GtkTextIter start_at;

	if (task_data->is_forward)
	{
		scan_region_forward (search, search->priv->task_region);
	}
	else
	{
		scan_region_backward (search, search->priv->task_region);
	}

	if (search->priv->task_region != NULL)
	{
		gtk_text_region_destroy (search->priv->task_region, TRUE);
		search->priv->task_region = NULL;
	}

	gtk_text_buffer_get_iter_at_mark (search->priv->buffer,
					  &start_at,
					  task_data->start_at);

	if (task_data->is_forward)
	{
		smart_forward_search_async (search,
					    &start_at,
					    task_data->wrapped_around);
	}
	else
	{
		smart_backward_search_async (search,
					     &start_at,
					     task_data->wrapped_around);
	}
}

static gboolean
idle_scan_cb (GtkSourceSearch *search)
{
	if (search->priv->high_priority_region != NULL)
	{
		/* Normally the high priority region is not really big, since it
		 * is the visible area on the screen. So we can highlight it in
		 * one batch.
		 */
		scan_all_region (search, search->priv->high_priority_region);

		gtk_text_region_destroy (search->priv->high_priority_region, TRUE);
		search->priv->high_priority_region = NULL;

		return G_SOURCE_CONTINUE;
	}

	if (search->priv->task_region != NULL)
	{
		scan_task_region (search);
		return G_SOURCE_CONTINUE;
	}

	scan_region_forward (search, search->priv->scan_region);

	if (is_text_region_empty (search->priv->scan_region))
	{
		search->priv->idle_scan_id = 0;

		g_object_notify (G_OBJECT (search->priv->buffer), "search-occurrences-count");

		if (search->priv->scan_region != NULL)
		{
			gtk_text_region_destroy (search->priv->scan_region, TRUE);
			search->priv->scan_region = NULL;
		}

		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void
install_idle_scan (GtkSourceSearch *search)
{
	if (search->priv->idle_scan_id == 0)
	{
		search->priv->idle_scan_id = g_idle_add ((GSourceFunc)idle_scan_cb, search);
	}
}

/* Returns TRUE when finished. */
static gboolean
smart_forward_search_step (GtkSourceSearch *search,
			   GtkTextIter     *start_at,
			   GtkTextIter     *match_start,
			   GtkTextIter     *match_end)
{
	GtkTextIter iter = *start_at;
	GtkTextIter limit;
	GtkTextRegion *region = NULL;

	if (!gtk_text_iter_has_tag (&iter, search->priv->found_tag))
	{
		gtk_text_iter_forward_to_tag_toggle (&iter, search->priv->found_tag);
	}

	limit = iter;
	gtk_text_iter_forward_to_tag_toggle (&limit, search->priv->found_tag);

	if (search->priv->scan_region != NULL)
	{
		region = gtk_text_region_intersect (search->priv->scan_region, start_at, &limit);
	}

	if (is_text_region_empty (region))
	{
		gboolean found = basic_forward_search (search, &iter, match_start, match_end, &limit);

		if (region != NULL)
		{
			gtk_text_region_destroy (region, TRUE);
		}

		if (found)
		{
			return TRUE;
		}

		*start_at = limit;
		return FALSE;
	}

	scan_all_region (search, region);
	gtk_text_region_destroy (region, TRUE);
	return FALSE;
}

/* Doesn't wrap around. */
static gboolean
smart_forward_search (GtkSourceSearch   *search,
		      const GtkTextIter *start_at,
		      GtkTextIter       *match_start,
		      GtkTextIter       *match_end)
{
	GtkTextIter iter = *start_at;

	if (search->priv->text == NULL)
	{
		return FALSE;
	}

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	while (!gtk_text_iter_is_end (&iter))
	{
		if (smart_forward_search_step (search, &iter, match_start, match_end))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/* Returns TRUE when finished. */
static gboolean
smart_backward_search_step (GtkSourceSearch *search,
			    GtkTextIter     *start_at,
			    GtkTextIter     *match_start,
			    GtkTextIter     *match_end)
{
	GtkTextIter iter = *start_at;
	GtkTextIter limit;
	GtkTextRegion *region = NULL;

	if (gtk_text_iter_begins_tag (&iter, search->priv->found_tag) ||
	    (!gtk_text_iter_has_tag (&iter, search->priv->found_tag) &&
	     !gtk_text_iter_ends_tag (&iter, search->priv->found_tag)))
	{
		gtk_text_iter_backward_to_tag_toggle (&iter, search->priv->found_tag);
	}

	limit = iter;
	gtk_text_iter_backward_to_tag_toggle (&limit, search->priv->found_tag);

	if (search->priv->scan_region != NULL)
	{
		region = gtk_text_region_intersect (search->priv->scan_region, &limit, start_at);
	}

	if (is_text_region_empty (region))
	{
		gboolean found = basic_backward_search (search, &iter, match_start, match_end, &limit);

		if (region != NULL)
		{
			gtk_text_region_destroy (region, TRUE);
		}

		if (found)
		{
			return TRUE;
		}

		*start_at = limit;
		return FALSE;
	}

	scan_all_region (search, region);
	gtk_text_region_destroy (region, TRUE);
	return FALSE;
}

/* Doesn't wrap around. */
static gboolean
smart_backward_search (GtkSourceSearch   *search,
		       const GtkTextIter *start_at,
		       GtkTextIter       *match_start,
		       GtkTextIter       *match_end)
{
	GtkTextIter iter = *start_at;

	if (search->priv->text == NULL)
	{
		return FALSE;
	}

	if (search->priv->found_tag == NULL)
	{
		init_found_tag (search);
	}

	while (!gtk_text_iter_is_start (&iter))
	{
		if (smart_backward_search_step (search, &iter, match_start, match_end))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static void
add_subregion_to_scan (GtkSourceSearch   *search,
		       const GtkTextIter *subregion_start,
		       const GtkTextIter *subregion_end)
{
	GtkTextIter start = *subregion_start;
	GtkTextIter end = *subregion_end;

	if (search->priv->scan_region == NULL)
	{
		search->priv->scan_region = gtk_text_region_new (search->priv->buffer);
	}

	DEBUG ({
		g_print ("add_subregion_to_scan(): region to scan, before:\n");
		gtk_text_region_debug_print (search->priv->scan_region);
	});

	gtk_text_region_add (search->priv->scan_region, &start, &end);

	DEBUG ({
		g_print ("add_subregion_to_scan(): region to scan, after:\n");
		gtk_text_region_debug_print (search->priv->scan_region);
	});

	install_idle_scan (search);

	/* The highlighting can be modified a bit backward and forward the
	 * region.
	 */
	gtk_text_iter_backward_lines (&start, search->priv->text_nb_lines);
	gtk_text_iter_forward_lines (&end, search->priv->text_nb_lines);

	g_signal_emit_by_name (search->priv->buffer, "highlight-updated", &start, &end);
}

static void
update (GtkSourceSearch *search)
{
	GtkTextIter start;
	GtkTextIter end;

	if (dispose_has_run (search))
	{
		return;
	}

	clear_search (search);

	search->priv->scan_region = gtk_text_region_new (search->priv->buffer);

	gtk_text_buffer_get_bounds (search->priv->buffer, &start, &end);
	add_subregion_to_scan (search, &start, &end);
}

static void
insert_text_before_cb (GtkSourceSearch *search,
		       GtkTextIter     *location,
		       gchar           *text,
		       gint             length)
{
	clear_task (search);

	if (search->priv->text != NULL)
	{
		GtkTextIter start = *location;
		GtkTextIter end = *location;

		remove_occurrences_in_range (search, &start, &end);
		add_subregion_to_scan (search, &start, &end);
	}
}

static void
insert_text_after_cb (GtkSourceSearch *search,
		      GtkTextIter     *location,
		      gchar           *text,
		      gint             length)
{
	GtkTextIter start;
	GtkTextIter end;

	start = end = *location;

	gtk_text_iter_backward_chars (&start,
				      g_utf8_strlen (text, length));

	add_subregion_to_scan (search, &start, &end);
}

static void
delete_range_before_cb (GtkSourceSearch *search,
		        GtkTextIter     *delete_start,
		        GtkTextIter     *delete_end)
{
	GtkTextIter start_buffer;
	GtkTextIter end_buffer;

	clear_task (search);

	gtk_text_buffer_get_bounds (search->priv->buffer, &start_buffer, &end_buffer);

	if (gtk_text_iter_equal (delete_start, &start_buffer) &&
	    gtk_text_iter_equal (delete_end, &end_buffer))
	{
		/* Special case when removing all the text. */
		search->priv->occurrences_count = 0;
		return;
	}

	if (search->priv->text != NULL)
	{
		GtkTextIter start = *delete_start;
		GtkTextIter end = *delete_end;

		gtk_text_iter_backward_lines (&start, search->priv->text_nb_lines);
		gtk_text_iter_forward_lines (&end, search->priv->text_nb_lines);

		remove_occurrences_in_range (search, &start, &end);
		add_subregion_to_scan (search, &start, &end);
	}
}

static void
delete_range_after_cb (GtkSourceSearch *search,
		       GtkTextIter     *start,
		       GtkTextIter     *end)
{
	add_subregion_to_scan (search, start, end);
}

static void
set_buffer (GtkSourceSearch *search,
	    GtkSourceBuffer *buffer)
{
	g_assert (search->priv->buffer == NULL);

	search->priv->buffer = GTK_TEXT_BUFFER (buffer);

	g_signal_connect_object (buffer,
				 "insert-text",
				 G_CALLBACK (insert_text_before_cb),
				 search,
				 G_CONNECT_SWAPPED);

	g_signal_connect_object (buffer,
				 "insert-text",
				 G_CALLBACK (insert_text_after_cb),
				 search,
				 G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	g_signal_connect_object (buffer,
				 "delete-range",
				 G_CALLBACK (delete_range_before_cb),
				 search,
				 G_CONNECT_SWAPPED);

	g_signal_connect_object (buffer,
				 "delete-range",
				 G_CALLBACK (delete_range_after_cb),
				 search,
				 G_CONNECT_AFTER | G_CONNECT_SWAPPED);
}

static void
_gtk_source_search_dispose (GObject *object)
{
	GtkSourceSearch *search = GTK_SOURCE_SEARCH (object);

	clear_search (search);

	search->priv->buffer = NULL;

	G_OBJECT_CLASS (_gtk_source_search_parent_class)->dispose (object);
}

static void
_gtk_source_search_finalize (GObject *object)
{
	GtkSourceSearch *search = GTK_SOURCE_SEARCH (object);

	g_free (search->priv->text);

	G_OBJECT_CLASS (_gtk_source_search_parent_class)->finalize (object);
}

static void
_gtk_source_search_class_init (GtkSourceSearchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = _gtk_source_search_dispose;
	object_class->finalize = _gtk_source_search_finalize;
}

static void
_gtk_source_search_init (GtkSourceSearch *search)
{
	search->priv = _gtk_source_search_get_instance_private (search);
}

GtkSourceSearch *
_gtk_source_search_new (GtkSourceBuffer *buffer)
{
	GtkSourceSearch *search;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	search = g_object_new (GTK_SOURCE_TYPE_SEARCH, NULL);
	set_buffer (search, buffer);

	return search;
}

static gint
compute_number_of_lines (const gchar *text)
{
	const gchar *p;
	gint len;
	gint nb_of_lines = 1;

	if (text == NULL)
	{
		return 0;
	}

	len = strlen (text);
	p = text;

	while (len > 0)
	{
		gint delimiter;
		gint next_paragraph;

		pango_find_paragraph_boundary (p, len, &delimiter, &next_paragraph);

		if (delimiter == next_paragraph)
		{
			/* not found */
			break;
		}

		p += next_paragraph;
		len -= next_paragraph;
		nb_of_lines++;
	}

	return nb_of_lines;
}

void
_gtk_source_search_set_text (GtkSourceSearch *search,
			     const gchar     *text)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));
	g_return_if_fail (text == NULL || g_utf8_validate (text, -1, NULL));

	g_free (search->priv->text);

	if (text == NULL || *text == '\0')
	{
		search->priv->text = NULL;
	}
	else
	{
		search->priv->text = g_strdup (text);
	}

	search->priv->text_nb_lines = compute_number_of_lines (search->priv->text);

	update (search);
}

const gchar *
_gtk_source_search_get_text (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), NULL);

	return search->priv->text;
}

void
_gtk_source_search_set_case_sensitive (GtkSourceSearch *search,
				       gboolean         case_sensitive)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));

	if (case_sensitive)
	{
		search->priv->flags &= ~GTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}
	else
	{
		search->priv->flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;
	}

	update (search);
}

gboolean
_gtk_source_search_get_case_sensitive (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);

	return (search->priv->flags & GTK_TEXT_SEARCH_CASE_INSENSITIVE) == 0;
}

void
_gtk_source_search_set_at_word_boundaries (GtkSourceSearch *search,
					   gboolean         at_word_boundaries)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));

	search->priv->at_word_boundaries = at_word_boundaries;
	update (search);
}

gboolean
_gtk_source_search_get_at_word_boundaries (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);

	return search->priv->at_word_boundaries;
}

void
_gtk_source_search_set_wrap_around (GtkSourceSearch *search,
				    gboolean         wrap_around)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));

	search->priv->wrap_around = wrap_around;
	update (search);
}

gboolean
_gtk_source_search_get_wrap_around (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);

	return search->priv->wrap_around;
}

void
_gtk_source_search_set_highlight (GtkSourceSearch *search,
				  gboolean         highlight)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));

	search->priv->highlight = highlight;

	if (search->priv->found_tag != NULL)
	{
		sync_found_tag (search);
	}
}

gboolean
_gtk_source_search_get_highlight (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);

	return search->priv->highlight;
}

gint
_gtk_source_search_get_occurrences_count (GtkSourceSearch *search)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), 0);

	return is_text_region_empty (search->priv->scan_region) ? search->priv->occurrences_count : -1;
}

gint
_gtk_source_search_get_occurrence_position (GtkSourceSearch   *search,
					    const GtkTextIter *match_start,
					    const GtkTextIter *match_end)
{
	GtkTextIter m_start;
	GtkTextIter m_end;
	GtkTextIter iter;
	gboolean found;
	gint position = 0;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), -1);
	g_return_val_if_fail (match_start != NULL, -1);
	g_return_val_if_fail (match_end != NULL, -1);

	if (dispose_has_run (search))
	{
		return -1;
	}

	/* Verify that the occurrence is correct. */

	found = basic_forward_search (search,
				      match_start,
				      &m_start,
				      &m_end,
				      match_end);

	if (!found ||
	    !gtk_text_iter_equal (match_start, &m_start) ||
	    !gtk_text_iter_equal (match_end, &m_end))
	{
		return 0;
	}

	/* Verify that the scan region is empty between the start of the buffer
	 * and the end of the occurrence.
	 */

	gtk_text_buffer_get_start_iter (search->priv->buffer, &iter);

	if (search->priv->scan_region != NULL)
	{
		GtkTextRegion *region = gtk_text_region_intersect (search->priv->scan_region,
								   &iter,
								   match_end);

		gboolean empty = is_text_region_empty (region);

		if (region != NULL)
		{
			gtk_text_region_destroy (region, TRUE);
		}

		if (!empty)
		{
			return -1;
		}
	}

	/* Everything is fine, count the number of previous occurrences. */

	while (smart_forward_search_without_scanning (search, &iter, &m_start, &m_end, match_start))
	{
		position++;
		iter = m_end;
	}

	return position + 1;
}

void
_gtk_source_search_update_highlight (GtkSourceSearch   *search,
				     const GtkTextIter *start,
				     const GtkTextIter *end,
				     gboolean           synchronous)
{
	GtkTextRegion *region_to_highlight;

	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	if (dispose_has_run (search) ||
	    is_text_region_empty (search->priv->scan_region))
	{
		return;
	}

	region_to_highlight = gtk_text_region_intersect (search->priv->scan_region,
							 start,
							 end);

	if (is_text_region_empty (region_to_highlight))
	{
		if (region_to_highlight != NULL)
		{
			gtk_text_region_destroy (region_to_highlight, TRUE);
		}

		return;
	}

	if (synchronous)
	{
		scan_all_region (search, region_to_highlight);
		gtk_text_region_destroy (region_to_highlight, TRUE);
	}
	else
	{
		if (search->priv->high_priority_region != NULL)
		{
			/* The high priority region is used to highlight the
			 * region visible on screen. So if we are here, that
			 * means that the visible region has changed. So we can
			 * destroy the old high_priority_region.
			 */
			gtk_text_region_destroy (search->priv->high_priority_region, TRUE);
		}

		search->priv->high_priority_region = region_to_highlight;
		install_idle_scan (search);
	}
}

gboolean
_gtk_source_search_forward (GtkSourceSearch   *search,
			    const GtkTextIter *iter,
			    GtkTextIter       *match_start,
			    GtkTextIter       *match_end)
{
	gboolean found;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (dispose_has_run (search))
	{
		return FALSE;
	}

	found = smart_forward_search (search, iter, match_start, match_end);

	if (!found && search->priv->wrap_around)
	{
		GtkTextIter start_iter;
		gtk_text_buffer_get_start_iter (search->priv->buffer, &start_iter);

		found = smart_forward_search (search, &start_iter, match_start, match_end);
	}

	return found;
}

void
_gtk_source_search_forward_async (GtkSourceSearch     *search,
				  const GtkTextIter   *iter,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));
	g_return_if_fail (iter != NULL);

	if (dispose_has_run (search))
	{
		return;
	}

	clear_task (search);
	search->priv->task = g_task_new (search->priv->buffer, cancellable, callback, user_data);

	smart_forward_search_async (search, iter, FALSE);
}

gboolean
_gtk_source_search_forward_finish (GtkSourceSearch  *search,
				   GAsyncResult     *result,
				   GtkTextIter      *match_start,
				   GtkTextIter      *match_end,
				   GError          **error)
{
	ForwardBackwardData *data;
	gboolean found;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);

	if (dispose_has_run (search))
	{
		return FALSE;
	}

	g_return_val_if_fail (g_task_is_valid (result, search->priv->buffer), FALSE);

	data = g_task_propagate_pointer (G_TASK (result), error);

	if (data == NULL)
	{
		return FALSE;
	}

	found = data->found;

	if (found)
	{
		if (match_start != NULL)
		{
			*match_start = data->match_start;
		}

		if (match_end != NULL)
		{
			*match_end = data->match_end;
		}
	}

	forward_backward_data_free (data);
	return found;
}

gboolean
_gtk_source_search_backward (GtkSourceSearch   *search,
			     const GtkTextIter *iter,
			     GtkTextIter       *match_start,
			     GtkTextIter       *match_end)
{
	gboolean found;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (dispose_has_run (search))
	{
		return FALSE;
	}

	found = smart_backward_search (search, iter, match_start, match_end);

	if (!found && search->priv->wrap_around)
	{
		GtkTextIter end_iter;

		gtk_text_buffer_get_end_iter (search->priv->buffer, &end_iter);

		found = smart_backward_search (search, &end_iter, match_start, match_end);
	}

	return found;
}

void
_gtk_source_search_backward_async (GtkSourceSearch     *search,
				   const GtkTextIter   *iter,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
	g_return_if_fail (GTK_SOURCE_IS_SEARCH (search));
	g_return_if_fail (iter != NULL);

	if (dispose_has_run (search))
	{
		return;
	}

	clear_task (search);
	search->priv->task = g_task_new (search->priv->buffer, cancellable, callback, user_data);

	smart_backward_search_async (search, iter, FALSE);
}

gboolean
_gtk_source_search_backward_finish (GtkSourceSearch  *search,
				    GAsyncResult     *result,
				    GtkTextIter      *match_start,
				    GtkTextIter      *match_end,
				    GError          **error)
{
	return _gtk_source_search_forward_finish (search,
						  result,
						  match_start,
						  match_end,
						  error);
}

gboolean
_gtk_source_search_replace (GtkSourceSearch   *search,
			    const GtkTextIter *match_start,
			    const GtkTextIter *match_end,
			    const gchar       *replace,
			    gint               replace_length)
{
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), FALSE);
	g_return_val_if_fail (match_start != NULL, FALSE);
	g_return_val_if_fail (match_end != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);

	if (dispose_has_run (search))
	{
		return FALSE;
	}

	basic_forward_search (search, match_start, &start, &end, match_end);

	if (!gtk_text_iter_equal (match_start, &start) ||
	    !gtk_text_iter_equal (match_end, &end))
	{
		return FALSE;
	}

	gtk_text_buffer_begin_user_action (search->priv->buffer);

	gtk_text_buffer_delete (search->priv->buffer, &start, &end);
	gtk_text_buffer_insert (search->priv->buffer, &start, replace, replace_length);

	gtk_text_buffer_end_user_action (search->priv->buffer);

	return TRUE;
}

guint
_gtk_source_search_replace_all (GtkSourceSearch *search,
				const gchar     *replace,
				gint             replace_length)
{
	GtkTextIter iter;
	GtkTextIter match_start;
	GtkTextIter match_end;
	guint nb_matches_replaced = 0;
	gboolean highlight_matching_brackets;

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH (search), 0);
	g_return_val_if_fail (replace != NULL, 0);

	if (dispose_has_run (search))
	{
		return 0;
	}

	g_signal_handlers_block_by_func (search->priv->buffer, insert_text_before_cb, search);
	g_signal_handlers_block_by_func (search->priv->buffer, insert_text_after_cb, search);
	g_signal_handlers_block_by_func (search->priv->buffer, delete_range_before_cb, search);
	g_signal_handlers_block_by_func (search->priv->buffer, delete_range_after_cb, search);

	highlight_matching_brackets =
		gtk_source_buffer_get_highlight_matching_brackets (GTK_SOURCE_BUFFER (search->priv->buffer));

	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (search->priv->buffer),
							   FALSE);

	gtk_text_buffer_get_start_iter (search->priv->buffer, &iter);

	gtk_text_buffer_begin_user_action (search->priv->buffer);

	while (smart_forward_search (search, &iter, &match_start, &match_end))
	{
		gtk_text_buffer_delete (search->priv->buffer, &match_start, &match_end);
		gtk_text_buffer_insert (search->priv->buffer, &match_end, replace, replace_length);

		iter = match_end;
		nb_matches_replaced++;
	}

	gtk_text_buffer_end_user_action (search->priv->buffer);

	gtk_source_buffer_set_highlight_matching_brackets (GTK_SOURCE_BUFFER (search->priv->buffer),
							   highlight_matching_brackets);

	g_signal_handlers_unblock_by_func (search->priv->buffer, insert_text_before_cb, search);
	g_signal_handlers_unblock_by_func (search->priv->buffer, insert_text_after_cb, search);
	g_signal_handlers_unblock_by_func (search->priv->buffer, delete_range_before_cb, search);
	g_signal_handlers_unblock_by_func (search->priv->buffer, delete_range_after_cb, search);

	update (search);

	return nb_matches_replaced;
}

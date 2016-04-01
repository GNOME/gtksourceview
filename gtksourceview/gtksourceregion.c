/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * gtksourceregion.h - GtkTextMark based region utility functions
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2002 Gustavo Giráldez <gustavo.giraldez@gmx.net>
 * Copyright (C) 2016 Sébastien Wilmet <swilmet@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourceregion.h"

#undef ENABLE_DEBUG
/*
#define ENABLE_DEBUG
*/

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

typedef struct _Subregion
{
	GtkTextMark *start;
	GtkTextMark *end;
} Subregion;

struct _GtkSourceRegion
{
	GtkTextBuffer *buffer;
	GList         *subregions;
	guint32        time_stamp;
};

typedef struct _GtkSourceRegionIteratorReal GtkSourceRegionIteratorReal;

struct _GtkSourceRegionIteratorReal
{
	GtkSourceRegion *region;
	guint32 region_time_stamp;
	GList *subregions;
};

/* Find and return a subregion node which contains the given text
 * iter.  If left_side is TRUE, return the subregion which contains
 * the text iter or which is the leftmost; else return the rightmost
 * subregion.
 */
static GList *
find_nearest_subregion (GtkSourceRegion   *region,
			const GtkTextIter *iter,
			GList             *begin,
			gboolean           leftmost,
			gboolean           include_edges)
{
	GList *retval;
	GList *l;

	g_return_val_if_fail (region != NULL, NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	if (begin == NULL)
	{
		begin = region->subregions;
	}

	if (begin != NULL)
	{
		retval = begin->prev;
	}
	else
	{
		retval = NULL;
	}

	for (l = begin; l != NULL; l = l->next)
	{
		GtkTextIter sr_iter;
		Subregion *sr = l->data;
		gint cmp;

		if (!leftmost)
		{
			gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_iter, sr->end);
			cmp = gtk_text_iter_compare (iter, &sr_iter);
			if (cmp < 0 || (cmp == 0 && include_edges))
			{
				retval = l;
				break;
			}

		}
		else
		{
			gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_iter, sr->start);
			cmp = gtk_text_iter_compare (iter, &sr_iter);
			if (cmp > 0 || (cmp == 0 && include_edges))
			{
				retval = l;
			}
			else
			{
				break;
			}
		}
	}

	return retval;
}

GtkSourceRegion *
gtk_source_region_new (GtkTextBuffer *buffer)
{
	GtkSourceRegion *region;

	g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

	region = g_slice_new (GtkSourceRegion);
	region->subregions = NULL;
	region->time_stamp = 0;
	region->buffer = buffer;

	g_object_add_weak_pointer (G_OBJECT (buffer),
				   (gpointer *)&region->buffer);

	return region;
}

void
gtk_source_region_destroy (GtkSourceRegion *region)
{
	g_return_if_fail (region != NULL);

	while (region->subregions)
	{
		Subregion *sr = region->subregions->data;

		if (region->buffer != NULL)
		{
			gtk_text_buffer_delete_mark (region->buffer, sr->start);
			gtk_text_buffer_delete_mark (region->buffer, sr->end);
		}

		g_slice_free (Subregion, sr);
		region->subregions = g_list_delete_link (region->subregions,
							 region->subregions);
	}

	if (region->buffer != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (region->buffer),
					      (gpointer *)&region->buffer);

		region->buffer = NULL;
	}

	g_slice_free (GtkSourceRegion, region);
}

GtkTextBuffer *
gtk_source_region_get_buffer (GtkSourceRegion *region)
{
	g_return_val_if_fail (region != NULL, NULL);

	return region->buffer;
}

static void
gtk_source_region_clear_zero_length_subregions (GtkSourceRegion *region)
{
	GList *node;

	g_return_if_fail (region != NULL);

	node = region->subregions;
	while (node != NULL)
	{
		Subregion *sr = node->data;
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_iter_at_mark (region->buffer, &start, sr->start);
		gtk_text_buffer_get_iter_at_mark (region->buffer, &end, sr->end);

		if (gtk_text_iter_equal (&start, &end))
		{
			gtk_text_buffer_delete_mark (region->buffer, sr->start);
			gtk_text_buffer_delete_mark (region->buffer, sr->end);
			g_slice_free (Subregion, sr);

			if (node == region->subregions)
			{
				region->subregions = node = g_list_delete_link (node, node);
			}
			else
			{
				node = g_list_delete_link (node, node);
			}

			region->time_stamp++;
		}
		else
		{
			node = node->next;
		}
	}
}

void
gtk_source_region_add (GtkSourceRegion   *region,
		       const GtkTextIter *_start,
		       const GtkTextIter *_end)
{
	GList *start_node;
	GList *end_node;
	GtkTextIter start;
	GtkTextIter end;

	g_return_if_fail (region != NULL);
	g_return_if_fail (_start != NULL);
	g_return_if_fail (_end != NULL);

	if (region->buffer == NULL)
	{
		return;
	}

	start = *_start;
	end = *_end;

	DEBUG (g_print ("---\n"));
	DEBUG (gtk_source_region_debug_print (region));
	DEBUG (g_message ("region_add (%d, %d)",
			  gtk_text_iter_get_offset (&start),
			  gtk_text_iter_get_offset (&end)));

	gtk_text_iter_order (&start, &end);

	/* Don't add zero-length regions. */
	if (gtk_text_iter_equal (&start, &end))
	{
		return;
	}

	/* Find bounding subregions. */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, TRUE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, TRUE);

	if (start_node == NULL || end_node == NULL || end_node == start_node->prev)
	{
		/* Create the new subregion. */
		Subregion *sr = g_slice_new0 (Subregion);
		sr->start = gtk_text_buffer_create_mark (region->buffer, NULL, &start, TRUE);
		sr->end = gtk_text_buffer_create_mark (region->buffer, NULL, &end, FALSE);

		if (start_node == NULL)
		{
			/* Append the new region. */
			region->subregions = g_list_append (region->subregions, sr);
		}
		else if (end_node == NULL)
		{
			/* Prepend the new region. */
			region->subregions = g_list_prepend (region->subregions, sr);
		}
		else
		{
			/* We are in the middle of two subregions. */
			region->subregions = g_list_insert_before (region->subregions, start_node, sr);
		}
	}
	else
	{
		GtkTextIter iter;
		Subregion *sr = start_node->data;

		if (start_node != end_node)
		{
			/* We need to merge some subregions. */
			GList *l = start_node->next;
			Subregion *q;

			gtk_text_buffer_delete_mark (region->buffer, sr->end);

			while (l != end_node)
			{
				q = l->data;
				gtk_text_buffer_delete_mark (region->buffer, q->start);
				gtk_text_buffer_delete_mark (region->buffer, q->end);
				g_slice_free (Subregion, q);
				l = g_list_delete_link (l, l);
			}

			q = l->data;
			gtk_text_buffer_delete_mark (region->buffer, q->start);
			sr->end = q->end;
			g_slice_free (Subregion, q);
			l = g_list_delete_link (l, l);
		}

		/* Now move marks if that action expands the region. */
		gtk_text_buffer_get_iter_at_mark (region->buffer, &iter, sr->start);
		if (gtk_text_iter_compare (&iter, &start) > 0)
		{
			gtk_text_buffer_move_mark (region->buffer, sr->start, &start);
		}

		gtk_text_buffer_get_iter_at_mark (region->buffer, &iter, sr->end);
		if (gtk_text_iter_compare (&iter, &end) < 0)
		{
			gtk_text_buffer_move_mark (region->buffer, sr->end, &end);
		}
	}

	region->time_stamp++;

	DEBUG (gtk_source_region_debug_print (region));
}

void
gtk_source_region_subtract (GtkSourceRegion   *region,
			    const GtkTextIter *_start,
			    const GtkTextIter *_end)
{
	GList *start_node;
	GList *end_node;
	GList *node;
	GtkTextIter sr_start_iter;
	GtkTextIter sr_end_iter;
	gboolean done;
	gboolean start_is_outside;
	gboolean end_is_outside;
	Subregion *sr;
	GtkTextIter start;
	GtkTextIter end;

	g_return_if_fail (region != NULL);
	g_return_if_fail (_start != NULL);
	g_return_if_fail (_end != NULL);

	if (region->buffer == NULL)
	{
		return;
	}

	start = *_start;
	end = *_end;

	DEBUG (g_print ("---\n"));
	DEBUG (gtk_source_region_debug_print (region));
	DEBUG (g_message ("region_substract (%d, %d)",
			  gtk_text_iter_get_offset (&start),
			  gtk_text_iter_get_offset (&end)));

	gtk_text_iter_order (&start, &end);

	/* Find bounding subregions. */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, FALSE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, FALSE);

	/* Easy case first. */
	if (start_node == NULL || end_node == NULL || end_node == start_node->prev)
	{
		return;
	}

	/* Deal with the start point. */
	start_is_outside = end_is_outside = FALSE;

	sr = start_node->data;
	gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
	gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

	if (gtk_text_iter_in_range (&start, &sr_start_iter, &sr_end_iter) &&
	    !gtk_text_iter_equal (&start, &sr_start_iter))
	{
		/* The starting point is inside the first subregion. */
		if (gtk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter) &&
		    !gtk_text_iter_equal (&end, &sr_end_iter))
		{
			/* The ending point is also inside the first
			 * subregion: we need to split.
			 */
			Subregion *new_sr = g_slice_new0 (Subregion);
			new_sr->end = sr->end;
			new_sr->start = gtk_text_buffer_create_mark (region->buffer,
								     NULL,
								     &end,
								     TRUE);

			start_node = g_list_insert_before (start_node, start_node->next, new_sr);

			sr->end = gtk_text_buffer_create_mark (region->buffer,
							       NULL,
							       &start,
							       FALSE);

			/* No further processing needed. */
			DEBUG (g_message ("subregion splitted"));

			return;
		}
		else
		{
			/* The ending point is outside, so just move
			 * the end of the subregion to the starting point.
			 */
			gtk_text_buffer_move_mark (region->buffer, sr->end, &start);
		}
	}
	else
	{
		/* The starting point is outside (and so to the left)
		 * of the first subregion.
		 */
		DEBUG (g_message ("start is outside"));

		start_is_outside = TRUE;
	}

	/* Deal with the end point. */
	if (start_node != end_node)
	{
		sr = end_node->data;
		gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
		gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);
	}

	if (gtk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter) &&
	    !gtk_text_iter_equal (&end, &sr_end_iter))
	{
		/* Ending point is inside, move the start mark. */
		gtk_text_buffer_move_mark (region->buffer, sr->start, &end);
	}
	else
	{
		end_is_outside = TRUE;
		DEBUG (g_message ("end is outside"));
	}

	/* Finally remove any intermediate subregions. */
	done = FALSE;
	node = start_node;

	while (!done)
	{
		if (node == end_node)
		{
			/* We are done, exit in the next iteration. */
			done = TRUE;
		}

		if ((node == start_node && !start_is_outside) ||
		    (node == end_node && !end_is_outside))
		{
			/* Skip starting or ending node. */
			node = node->next;
		}
		else
		{
			GList *l = node->next;
			sr = node->data;
			gtk_text_buffer_delete_mark (region->buffer, sr->start);
			gtk_text_buffer_delete_mark (region->buffer, sr->end);
			g_slice_free (Subregion, sr);
			region->subregions = g_list_delete_link (region->subregions,
								 node);
			node = l;
		}
	}

	region->time_stamp++;

	DEBUG (gtk_source_region_debug_print (region));

	/* Now get rid of empty subregions. */
	gtk_source_region_clear_zero_length_subregions (region);

	DEBUG (gtk_source_region_debug_print (region));
}

gint
gtk_source_region_subregions (GtkSourceRegion *region)
{
	g_return_val_if_fail (region != NULL, 0);

	return g_list_length (region->subregions);
}

gboolean
gtk_source_region_nth_subregion (GtkSourceRegion *region,
				 guint            subregion,
				 GtkTextIter     *start,
				 GtkTextIter     *end)
{
	Subregion *sr;

	g_return_val_if_fail (region != NULL, FALSE);

	if (region->buffer == NULL)
	{
		return FALSE;
	}

	sr = g_list_nth_data (region->subregions, subregion);
	if (sr == NULL)
	{
		return FALSE;
	}

	if (start != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (region->buffer, start, sr->start);
	}
	if (end != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (region->buffer, end, sr->end);
	}

	return TRUE;
}

GtkSourceRegion *
gtk_source_region_intersect (GtkSourceRegion   *region,
			     const GtkTextIter *_start,
			     const GtkTextIter *_end)
{
	GList *start_node;
	GList *end_node;
	GList *node;
	GtkTextIter sr_start_iter;
	GtkTextIter sr_end_iter;
	Subregion *sr;
	Subregion *new_sr;
	gboolean done;
	GtkSourceRegion *new_region;
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (region != NULL, NULL);
	g_return_val_if_fail (_start != NULL, NULL);
	g_return_val_if_fail (_end != NULL, NULL);

	if (region->buffer == NULL)
	{
		return NULL;
	}

	start = *_start;
	end = *_end;

	gtk_text_iter_order (&start, &end);

	/* Find bounding subregions. */
	start_node = find_nearest_subregion (region, &start, NULL, FALSE, FALSE);
	end_node = find_nearest_subregion (region, &end, start_node, TRUE, FALSE);

	/* Easy case first. */
	if (start_node == NULL || end_node == NULL || end_node == start_node->prev)
	{
		return NULL;
	}

	new_region = gtk_source_region_new (region->buffer);
	done = FALSE;

	sr = start_node->data;
	gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
	gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

	/* Starting node. */
	if (gtk_text_iter_in_range (&start, &sr_start_iter, &sr_end_iter))
	{
		new_sr = g_slice_new0 (Subregion);
		new_region->subregions = g_list_prepend (new_region->subregions, new_sr);

		new_sr->start = gtk_text_buffer_create_mark (new_region->buffer,
							     NULL,
							     &start,
							     TRUE);

		if (start_node == end_node)
		{
			/* Things will finish shortly. */
			done = TRUE;
			if (gtk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter))
			{
				new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
									   NULL,
									   &end,
									   FALSE);
			}
			else
			{
				new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
									   NULL,
									   &sr_end_iter,
									   FALSE);
			}
		}
		else
		{
			new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
								   NULL,
								   &sr_end_iter,
								   FALSE);
		}

		node = start_node->next;
	}
	else
	{
		/* start should be the same as the subregion, so copy it in the
		 * loop.
		 */
		node = start_node;
	}

	if (!done)
	{
		while (node != end_node)
		{
			/* Copy intermediate subregions verbatim. */
			sr = node->data;
			gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
			gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

			new_sr = g_slice_new0 (Subregion);
			new_region->subregions = g_list_prepend (new_region->subregions, new_sr);

			new_sr->start = gtk_text_buffer_create_mark (new_region->buffer,
								     NULL,
								     &sr_start_iter,
								     TRUE);

			new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
								   NULL,
								   &sr_end_iter,
								   FALSE);

			/* Next node. */
			node = node->next;
		}

		/* Ending node. */
		sr = node->data;
		gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_start_iter, sr->start);
		gtk_text_buffer_get_iter_at_mark (region->buffer, &sr_end_iter, sr->end);

		new_sr = g_slice_new0 (Subregion);
		new_region->subregions = g_list_prepend (new_region->subregions, new_sr);

		new_sr->start = gtk_text_buffer_create_mark (new_region->buffer,
							     NULL,
							     &sr_start_iter,
							     TRUE);

		if (gtk_text_iter_in_range (&end, &sr_start_iter, &sr_end_iter))
		{
			new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
								   NULL,
								   &end,
								   FALSE);
		}
		else
		{
			new_sr->end = gtk_text_buffer_create_mark (new_region->buffer,
								   NULL,
								   &sr_end_iter,
								   FALSE);
		}
	}

	new_region->subregions = g_list_reverse (new_region->subregions);
	return new_region;
}

static gboolean
check_iterator (GtkSourceRegionIteratorReal *real)
{
	if ((real->region == NULL) ||
	    (real->region_time_stamp != real->region->time_stamp))
	{
		g_warning ("Invalid iterator: either the iterator "
			   "is uninitialized, or the region "
			   "has been modified since the iterator "
			   "was created.");

                return FALSE;
	}

	return TRUE;
}

void
gtk_source_region_get_iterator (GtkSourceRegion         *region,
				GtkSourceRegionIterator *iter,
				guint                    start)
{
	GtkSourceRegionIteratorReal *real;

	g_return_if_fail (region != NULL);
	g_return_if_fail (iter != NULL);

	real = (GtkSourceRegionIteratorReal *)iter;

	/* region->subregions may be NULL, -> end iter */

	real->region = region;
	real->subregions = g_list_nth (region->subregions, start);
	real->region_time_stamp = region->time_stamp;
}

gboolean
gtk_source_region_iterator_is_end (GtkSourceRegionIterator *iter)
{
	GtkSourceRegionIteratorReal *real;

	g_return_val_if_fail (iter != NULL, FALSE);

	real = (GtkSourceRegionIteratorReal *)iter;
	g_return_val_if_fail (check_iterator (real), FALSE);

	return real->subregions == NULL;
}

gboolean
gtk_source_region_iterator_next (GtkSourceRegionIterator *iter)
{
	GtkSourceRegionIteratorReal *real;

	g_return_val_if_fail (iter != NULL, FALSE);

	real = (GtkSourceRegionIteratorReal *)iter;
	g_return_val_if_fail (check_iterator (real), FALSE);

	if (real->subregions != NULL)
	{
		real->subregions = real->subregions->next;
		return TRUE;
	}

	return FALSE;
}

gboolean
gtk_source_region_iterator_get_subregion (GtkSourceRegionIterator *iter,
					  GtkTextIter             *start,
					  GtkTextIter             *end)
{
	GtkSourceRegionIteratorReal *real;
	Subregion *sr;

	g_return_val_if_fail (iter != NULL, FALSE);

	real = (GtkSourceRegionIteratorReal *)iter;
	g_return_val_if_fail (check_iterator (real), FALSE);
	g_return_val_if_fail (real->subregions != NULL, FALSE);

	if (real->region->buffer == NULL)
	{
		return FALSE;
	}

	sr = (Subregion*)real->subregions->data;
	g_return_val_if_fail (sr != NULL, FALSE);

	if (start != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (real->region->buffer, start, sr->start);
	}

	if (end != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (real->region->buffer, end, sr->end);
	}

	return TRUE;
}

void
gtk_source_region_debug_print (GtkSourceRegion *region)
{
	GList *l;

	g_return_if_fail (region != NULL);

	if (region->buffer == NULL)
	{
		return;
	}

	g_print ("Subregions: ");

	for (l = region->subregions; l != NULL; l = l->next)
	{
		Subregion *sr = l->data;
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_iter_at_mark (region->buffer, &start, sr->start);
		gtk_text_buffer_get_iter_at_mark (region->buffer, &end, sr->end);

		g_print ("%d-%d ",
			 gtk_text_iter_get_offset (&start),
			 gtk_text_iter_get_offset (&end));
	}
	g_print ("\n");
}

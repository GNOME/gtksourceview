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

#pragma once

#include <glib.h>

typedef struct _Segment Segment;
typedef struct _Context Context;
typedef struct _SubPattern SubPattern;

struct _Segment
{
	Segment *parent;
	Segment *next;
	Segment *prev;
	Segment *children;
	Segment *last_child;

	/* Keep list order. Build an index when querying a large sibling list. */
	GSequence *children_index;
	GSequenceIter *index_iter;

	/* This is NULL if and only if it's a dummy segment which denotes
	 * inserted or deleted text.
	 */
	Context *context;

	/* Subpatterns found in this segment. */
	SubPattern *sub_patterns;

	/* The context is used in the interval [start_at; end_at). */
	int start_at;
	int end_at;

	/* In case of container contexts, start_len/end_len is length in chars
	 * of start/end match.
	 */
	int start_len;
	int end_len;

	/* Whether this segment is a whole good segment, or it's an end of
	 * a bigger one left after erase_segments() call.
	 */
	guint n_children;
	guint is_start : 1;
};

#define GTK_SOURCE_SEGMENT_INDEX_MIN_CHILDREN 128

static inline void
_gtk_source_segment_index_build (Segment *parent)
{
	if (parent->children_index != NULL)
		return;

	parent->children_index = g_sequence_new (NULL);
	for (Segment *child = parent->children; child != NULL; child = child->next)
	{
		g_assert (child->index_iter == NULL);
		child->index_iter = g_sequence_append (parent->children_index, child);
	}
}

/* Call after the child has been linked into its parent's sibling list. */
static inline void
_gtk_source_segment_index_insert (Segment *child)
{
	Segment *parent = child->parent;

	g_assert (parent != NULL);
	g_assert (child->index_iter == NULL);
	parent->n_children++;

	if (parent->children_index != NULL)
	{
		GSequenceIter *next = child->next != NULL
		                    ? child->next->index_iter
		                    : g_sequence_get_end_iter (parent->children_index);

		g_assert (next != NULL);

		child->index_iter = g_sequence_insert_before (next, child);
	}
}

static inline void
_gtk_source_segment_index_remove (Segment *child)
{
	g_assert (child->parent != NULL);
	g_assert (child->parent->n_children > 0);

	child->parent->n_children--;

	if (child->index_iter != NULL)
	{
		g_sequence_remove (child->index_iter);
		child->index_iter = NULL;
	}
}

static inline void
_gtk_source_segment_index_clear (Segment *parent)
{
	if (parent->children_index != NULL)
	{
		GSequenceIter *iter = g_sequence_get_begin_iter (parent->children_index);

		while (!g_sequence_iter_is_end (iter))
		{
			Segment *child = g_sequence_get (iter);

			child->index_iter = NULL;
			iter = g_sequence_iter_next (iter);
		}

		g_clear_pointer (&parent->children_index, g_sequence_free);
	}
}

static inline void
_gtk_source_segment_index_split (Segment *left,
                                 Segment *right)
{
	if (left->children_index != NULL && right->children != NULL)
	{
		right->children_index = g_sequence_new (NULL);
		g_sequence_move_range (g_sequence_get_end_iter (right->children_index),
		                       right->children->index_iter,
		                       g_sequence_get_end_iter (left->children_index));
	}
}

static inline void
_gtk_source_segment_index_merge (Segment *left,
                                 Segment *right)
{
	if (left->children_index != NULL || right->children_index != NULL)
	{
		_gtk_source_segment_index_build (left);
		_gtk_source_segment_index_build (right);

		g_sequence_move_range (g_sequence_get_end_iter (left->children_index),
		                       g_sequence_get_begin_iter (right->children_index),
		                       g_sequence_get_end_iter (right->children_index));

		g_clear_pointer (&right->children_index, g_sequence_free);
	}

	left->n_children += right->n_children;
	right->n_children = 0;
}

static inline int
_gtk_source_segment_compare_end (gconstpointer a,
                                 gconstpointer b,
                                 gpointer      lower_bound)
{
	const Segment *left = a;
	const Segment *right = b;
	int order = (left->end_at > right->end_at) - (left->end_at < right->end_at);

	if (order == 0 && lower_bound != NULL)
	{
		if (a == lower_bound)
			return -1;
		if (b == lower_bound)
			return 1;
	}

	return order;
}

/* Seek the first child whose end is >= offset (inclusive) or > offset. End
 * offsets are ordered because siblings do not overlap. Uniform suffix shifts
 * preserve that ordering and require no updates to the positional index.
 */
static inline Segment *
_gtk_source_segment_seek_children (Segment  *parent,
                                   int       offset,
                                   gboolean  inclusive)
{
	if (parent->children == NULL)
		return NULL;

	if (parent->children->end_at > offset ||
	    (inclusive && parent->children->end_at == offset))
		return parent->children;

	if (parent->last_child->end_at < offset ||
	    (!inclusive && parent->last_child->end_at == offset))
		return NULL;

	if (parent->children_index == NULL &&
	    parent->n_children >= GTK_SOURCE_SEGMENT_INDEX_MIN_CHILDREN)
		_gtk_source_segment_index_build (parent);

	if (parent->children_index != NULL)
	{
		Segment key = { .end_at = offset };
		GSequenceIter *iter;

		iter = g_sequence_search (parent->children_index, &key,
		                          _gtk_source_segment_compare_end,
		                          inclusive ? &key : NULL);
		return g_sequence_iter_is_end (iter) ? NULL : g_sequence_get (iter);
	}

	for (Segment *child = parent->children; child != NULL; child = child->next)
	{
		if (child->end_at > offset || (inclusive && child->end_at == offset))
			return child;
	}

	return NULL;
}

#define GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT(s,o) ((s)->start_at == (o) && (s)->end_at == (o))
#define GTK_SOURCE_SEGMENT_CONTAINS(s,o) ((s)->start_at <= (o) && (s)->end_at > (o))
#define GTK_SOURCE_SEGMENT_DISTANCE(s,o) (MIN (ABS ((s)->start_at - (o)), ABS ((s)->end_at - (o))))

/* @segment must contain @offset. Zero-length siblings have precedence at a
 * shared starting offset. Their linked-list order is semantically significant.
 */
static inline Segment *
_gtk_source_segment_lookup_inside (Segment *segment,
                                   int      offset)
{
	Segment *child;

	g_assert (segment->start_at <= offset && segment->end_at > offset);

	if (segment->children == NULL ||
	    segment->children->start_at > offset || segment->last_child->end_at < offset)
		return segment;

	/* Appending syntax and looking up either end do not need an index. */
	if (GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT (segment->children, offset) ||
	    GTK_SOURCE_SEGMENT_CONTAINS (segment->children, offset))
	{
		child = segment->children;
	}
	else if (GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT (segment->last_child, offset) ||
	         GTK_SOURCE_SEGMENT_CONTAINS (segment->last_child, offset))
	{
		child = segment->last_child;
	}
	else if (segment->children_index != NULL ||
	         segment->n_children >= GTK_SOURCE_SEGMENT_INDEX_MIN_CHILDREN)
	{
		child = _gtk_source_segment_seek_children (segment, offset, TRUE);
	}
	else if (GTK_SOURCE_SEGMENT_DISTANCE (segment->children, offset) <= GTK_SOURCE_SEGMENT_DISTANCE (segment->last_child, offset))
	{
		child = segment->children;
	}
	else
	{
		child = segment->last_child;
		while (child->prev != NULL && child->start_at > offset)
			child = child->prev;
	}

	/* A backward seek may land after zero-length siblings at this offset.
	 * All search paths must select the first such sibling.
	 */
	if (child != NULL)
	{
		while (child->prev != NULL && GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT (child->prev, offset))
			child = child->prev;
	}

	for (; child != NULL && child->start_at <= offset; child = child->next)
	{
		if (GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT (child, offset))
			return child;

		if (GTK_SOURCE_SEGMENT_CONTAINS (child, offset))
			return _gtk_source_segment_lookup_inside (child, offset);
	}

	return segment;
}

#undef GTK_SOURCE_SEGMENT_IS_ZERO_LEN_AT
#undef GTK_SOURCE_SEGMENT_CONTAINS
#undef GTK_SOURCE_SEGMENT_DISTANCE

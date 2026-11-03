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
	guint is_start : 1;
};

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

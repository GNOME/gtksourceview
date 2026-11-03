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

#include <glib.h>
#include "gtksourceview/gtksourcesegment-private.h"

static Segment *
append_segment (Segment *parent,
                gint     start,
                gint     end)
{
  Segment *segment = g_new0 (Segment, 1);

  segment->start_at = start;
  segment->end_at = end;
  segment->parent = parent;
  if (parent != NULL)
    {
      segment->prev = parent->last_child;
      if (segment->prev != NULL)
        {
          g_assert_cmpint (segment->prev->end_at, <=, start);
          segment->prev->next = segment;
        }
      else
        parent->children = segment;
      parent->last_child = segment;
    }
  return segment;
}

static void
free_segments (Segment *segment)
{
  Segment *child = segment->children;

  while (child != NULL)
    {
      Segment *next = child->next;
      free_segments (child);
      child = next;
    }
  g_free (segment);
}

/* Deliberately independent of distance heuristics and any sibling index. */
static Segment *
lookup_linear (Segment *segment,
               int      offset)
{
	for (Segment *child = segment->children; child != NULL; child = child->next)
	{
		if (child->start_at > offset)
			break;

		if (child->start_at == offset && child->end_at == offset)
			return child;

		if (child->start_at <= offset && offset < child->end_at)
			return lookup_linear (child, offset);
	}

	return segment;
}

static void
test_zero_length_ties (void)
{
	Segment *root = append_segment (NULL, 0, 100);
	Segment *first;

	append_segment (root, 0, 16);
	first = append_segment (root, 16, 16);
	append_segment (root, 16, 16);
	append_segment (root, 16, 25);
	append_segment (root, 30, 90);
	append_segment (root, 90, 100);

	g_assert_true (_gtk_source_segment_lookup_inside (root, 16) == first);
	for (int offset = 0; offset < 100; offset++)
		g_assert_true (_gtk_source_segment_lookup_inside (root, offset) == lookup_linear (root, offset));

	free_segments (root);
}

static void
populate (Segment *parent,
          guint    depth)
{
	int offset = parent->start_at;

	for (guint i = 0; i < 30 && offset < parent->end_at; i++)
	{
		Segment *child;
		int width;

		offset += g_test_rand_int_range (0, 4);
		if (offset >= parent->end_at)
			break;
		width = MIN (g_test_rand_int_range (0, 16), parent->end_at - offset);
		if (g_test_rand_int_range (0, 4) == 0)
			append_segment (parent, offset, offset);
		child = append_segment (parent, offset, offset + width);
		if (width > 0 && depth > 0)
			populate (child, depth - 1);
		offset += width;
	}
}

static void
test_nested_lookup (void)
{

	for (guint trial = 0; trial < 400; trial++)
	{
		Segment *root = append_segment (NULL, 0, 512);

		populate (root, 4);
		for (int offset = 0; offset < 512; offset++)
			g_assert_true (_gtk_source_segment_lookup_inside (root, offset) == lookup_linear (root, offset));

		free_segments (root);
	}
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/segment/zero-length-ties", test_zero_length_ties);
	g_test_add_func ("/segment/nested-lookup", test_nested_lookup);
	return g_test_run ();
}

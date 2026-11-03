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
                int      start,
                int      end)
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
		{
			parent->children = segment;
		}

		parent->last_child = segment;
		_gtk_source_segment_index_insert (segment);
	}

	return segment;
}

static void
free_segments (Segment *segment)
{
	Segment *child = segment->children;

	_gtk_source_segment_index_clear (segment);

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

static Segment *
seek_linear (Segment  *parent,
             int       offset,
             gboolean  inclusive)
{
	for (Segment *child = parent->children; child != NULL; child = child->next)
	{
		if (child->end_at > offset || (inclusive && child->end_at == offset))
			return child;
	}

	return NULL;
}

static void
assert_index (Segment *parent)
{
	guint count = 0;
	GSequenceIter *iter = parent->children_index ? g_sequence_get_begin_iter (parent->children_index) : NULL;

	for (Segment *child = parent->children; child != NULL; child = child->next)
	{
		count++;

		g_assert_true (child->parent == parent);

		if (child->next != NULL)
		{
			g_assert_true (child->next->prev == child);
			g_assert_cmpint (child->end_at, <=, child->next->start_at);
		}

		if (iter != NULL)
		{
			g_assert_false (g_sequence_iter_is_end (iter));
			g_assert_true (g_sequence_get (iter) == child);
			g_assert_true (child->index_iter == iter);
			iter = g_sequence_iter_next (iter);
		}
		else
		{
			g_assert_null (child->index_iter);
		}
	}

	g_assert_cmpuint (count, ==, parent->n_children);

	if (iter != NULL)
		g_assert_true (g_sequence_iter_is_end (iter));
}

static void
test_indexed_boundaries (void)
{
	Segment *root = append_segment (NULL, 0, 4000);

	for (guint i = 0; i < 1000; i++)
	{
		if (i % 7 == 0)
		{
			append_segment (root, i * 4, i * 4);
			append_segment (root, i * 4, i * 4);
		}
		append_segment (root, i * 4, i * 4 + 3);
	}

	g_assert_null (root->children_index);

	/* Appending children should not create an index. */
	_gtk_source_segment_lookup_inside (root, 0);
	_gtk_source_segment_lookup_inside (root, 3998);
	g_assert_null (root->children_index);

	for (int offset = 0; offset < 4000; offset++)
	{
		g_assert_true (_gtk_source_segment_lookup_inside (root, offset) == lookup_linear (root, offset));
		g_assert_true (_gtk_source_segment_seek_children (root, offset, FALSE) == seek_linear (root, offset, FALSE));
		g_assert_true (_gtk_source_segment_seek_children (root, offset, TRUE) == seek_linear (root, offset, TRUE));
	}

	g_assert_nonnull (root->children_index);
	assert_index (root);
	free_segments (root);
}

static Segment *
insert_zero_before (Segment *next)
{
	Segment *child = g_new0 (Segment, 1);
	Segment *parent = next->parent;

	child->parent = parent;
	child->start_at = child->end_at = next->start_at;
	child->next = next;
	child->prev = next->prev;

	if (child->prev != NULL)
		child->prev->next = child;
	else
		parent->children = child;

	next->prev = child;

	_gtk_source_segment_index_insert (child);

	return child;
}

static void
remove_child (Segment *child)
{
	_gtk_source_segment_index_remove (child);

	if (child->next != NULL)
		child->next->prev = child->prev;
	else
		child->parent->last_child = child->prev;

	if (child->prev != NULL)
		child->prev->next = child->next;
	else
		child->parent->children = child->next;

	free_segments (child);
}

static void
test_index_mutation (void)
{
	Segment *root = append_segment (NULL, 0, 10000);

	for (guint i = 0; i < 1000; i++)
		append_segment (root, i * 10, i * 10 + 5);

	_gtk_source_segment_index_build (root);

	for (guint trial = 0; trial < 1000; trial++)
	{
		guint position = g_test_rand_int_range (0, root->n_children);
		Segment *child = g_sequence_get (g_sequence_get_iter_at_pos (root->children_index, position));

		if (g_test_rand_bit ())
			insert_zero_before (child);
		else
			remove_child (child);

		assert_index (root);

		for (guint i = 0; i < 30; i++)
		{
			int offset = g_test_rand_int_range (0, root->end_at);
			g_assert_true (_gtk_source_segment_lookup_inside (root, offset) == lookup_linear (root, offset));
			g_assert_true (_gtk_source_segment_seek_children (root, offset, TRUE) == seek_linear (root, offset, TRUE));
		}
	}

	for (Segment *child = root->children; child != NULL; child = child->next)
	{
		if (child->start_at >= 5000)
		{
			child->start_at += 7;
			child->end_at += 7;
		}
	}

	root->end_at += 7;
	for (int offset = 4990; offset < 5020; offset++)
		g_assert_true (_gtk_source_segment_seek_children (root, offset, TRUE) == seek_linear (root, offset, TRUE));
	assert_index (root);

	free_segments (root);
}

static void
test_index_split_merge (void)
{
	for (guint mode = 0; mode < 4; mode++)
	{
		Segment *root = append_segment (NULL, 0, 4000);
		Segment *left = append_segment (root, 0, 2000);
		Segment *right = append_segment (root, 2000, 4000);
		Segment *split;

		for (guint i = 0; i < 500; i++)
			append_segment (left, i * 4, i * 4 + 4);
		for (guint i = 500; i < 1000; i++)
			append_segment (right, i * 4, i * 4 + 4);

		if (mode & 1)
			_gtk_source_segment_index_build (left);

		if (mode & 2)
			_gtk_source_segment_index_build (right);

		_gtk_source_segment_index_merge (left, right);
		split = right->children;

		for (Segment *child = right->children; child != NULL; child = child->next)
			child->parent = left;

		left->last_child->next = right->children;
		right->children->prev = left->last_child;
		left->last_child = right->last_child;
		right->children = right->last_child = NULL;
		assert_index (left);
		assert_index (right);

		right->children = split;
		right->last_child = left->last_child;
		left->last_child = split->prev;
		left->last_child->next = NULL;
		split->prev = NULL;
		left->n_children = right->n_children = 500;
		for (Segment *child = right->children; child != NULL; child = child->next)
			child->parent = right;

		_gtk_source_segment_index_split (left, right);

		assert_index (left);
		assert_index (right);

		for (int offset = 0; offset < 4000; offset++)
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
	g_test_add_func ("/segment/indexed-boundaries", test_indexed_boundaries);
	g_test_add_func ("/segment/index-mutation", test_index_mutation);
	g_test_add_func ("/segment/index-split-merge", test_index_split_merge);
	return g_test_run ();
}

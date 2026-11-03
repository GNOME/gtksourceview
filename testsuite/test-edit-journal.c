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
#include "gtksourceview/gtksourceeditjournal-private.h"

static void
assert_rejected_unchanged (GtkSourceEditRange *ranges,
                           guint               n_ranges,
                           int                 initial_length,
                           int                 final_length)
{
	gpointer original = g_memdup2 (ranges, sizeof *ranges * n_ranges);

	g_assert_false (_gtk_source_edit_ranges_prepare (ranges, n_ranges, initial_length, final_length));
	g_assert_cmpmem (ranges, sizeof *ranges * n_ranges, original, sizeof *ranges * n_ranges);
	g_free (original);
}

static void
test_offset_limits (void)
{
	GtkSourceEditRange corrected_overflow[] = {
		{ 0, 0, -1000000000 },
		{ 500000000, 1500000000, 1000000000 },
	};
	GtkSourceEditRange length_overflow[] = {
		{ 0, 0, -20 },
		{ 100, 120, 20 },
	};
	GtkSourceEditRange at_limit[] = { { 100, 120, 20 } };
	GtkSourceEditRange wide_merge[] = { { 0, 0, (gint64)G_MAXINT + 1 } };
	GtkSourceEditRange invalid_range[] = { { 2, 3, 4 } };
	GtkSourceEditRange wrong_length[] = { { 2, 4, 2 } };

	/* Neither case needs an enormous GtkTextBuffer allocation. The first
	 * exceeds the offset limit while undoing preceding deletions. The second
	 * has small corrected offsets but its temporary tree length overflows.
	 */
	assert_rejected_unchanged (corrected_overflow, G_N_ELEMENTS (corrected_overflow), 1500000000, 1500000000);
	assert_rejected_unchanged (length_overflow, G_N_ELEMENTS (length_overflow), G_MAXINT - 10, G_MAXINT - 10);
	assert_rejected_unchanged (wide_merge, G_N_ELEMENTS (wide_merge), 0, 0);
	assert_rejected_unchanged (invalid_range, G_N_ELEMENTS (invalid_range), 10, 14);
	assert_rejected_unchanged (wrong_length, G_N_ELEMENTS (wrong_length), 10, 10);
	g_assert_true (_gtk_source_edit_ranges_prepare (at_limit, G_N_ELEMENTS (at_limit), G_MAXINT - 20, G_MAXINT));
	g_assert_cmpint (at_limit[0].start, ==, 100);
	g_assert_cmpint (at_limit[0].end, ==, 120);
}

static void
test_scaled_coordinate_oracle (void)
{

	for (guint trial = 0; trial < 2000; trial++)
	{
		GtkSourceEditRange edits[4];
		gint64 original_start[4], replacement_end[4];
		gint64 total_delta = 0;
		gint64 maximum_length = 200;
		gint64 current_length = 200;

		for (guint i = 0; i < G_N_ELEMENTS (edits); i++)
		{
			int old_length = g_test_rand_int_range (0, 12);
			int new_length = g_test_rand_int_range (0, 12);
			int old_start = 5 + 45 * i;

			original_start[i] = old_start;
			replacement_end[i] = old_start + new_length;
			edits[i].start = old_start + total_delta;
			edits[i].end = edits[i].start + new_length;
			edits[i].delta = new_length - old_length;
			total_delta += edits[i].delta;
		}

		for (guint i = G_N_ELEMENTS (edits); i > 0; i--)
		{
			current_length += edits[i - 1].delta;
			maximum_length = MAX (maximum_length, current_length);
		}

		for (guint big = 0; big < 2; big++)
		{
			GtkSourceEditRange scaled[G_N_ELEMENTS (edits)];
			gint64 scale = big ? G_MAXINT / MAX ((gint64)200, 200 + total_delta) : 1;
			gboolean expected_safe = maximum_length * scale <= G_MAXINT;

			for (guint i = 0; i < G_N_ELEMENTS (edits); i++)
			{
				scaled[i].start = edits[i].start * scale;
				scaled[i].end = edits[i].end * scale;
				scaled[i].delta = edits[i].delta * scale;
			}

			if (!expected_safe)
			{
				assert_rejected_unchanged (scaled, G_N_ELEMENTS (scaled), 200 * scale, (200 + total_delta) * scale);
			}
			else
			{
				g_assert_true (_gtk_source_edit_ranges_prepare (scaled, G_N_ELEMENTS (scaled),
				                                                200 * scale, (200 + total_delta) * scale));
				for (guint i = 0; i < G_N_ELEMENTS (scaled); i++)
				{
					g_assert_cmpint (scaled[i].start, ==, original_start[i] * scale);
					g_assert_cmpint (scaled[i].end, ==, replacement_end[i] * scale);
				}
			}
		}
	}

}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/edit-journal/offset-limits", test_offset_limits);
	g_test_add_func ("/edit-journal/scaled-coordinate-oracle", test_scaled_coordinate_oracle);
	return g_test_run ();
}

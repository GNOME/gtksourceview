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

typedef struct
{
	int start;
	int end;
	gint64 delta;
} GtkSourceEditRange;

static inline gboolean
_gtk_source_edit_ranges_prepare (GtkSourceEditRange *ranges,
                                 guint               n_ranges,
                                 int                 initial_length,
                                 int                 final_length)
{
	gint64 preceding_delta = 0;
	gint64 intermediate_length = initial_length;

	if (initial_length < 0 || final_length < 0)
		return FALSE;

	for (guint i = 0; i < n_ranges; i++)
	{
		GtkSourceEditRange *range = &ranges[i];
		gint64 start = range->start - preceding_delta;
		gint64 end = range->end - preceding_delta;

		if (range->start < 0 || range->start > range->end ||
		    range->end > final_length ||
		    (i > 0 && ranges[i - 1].end > range->start) ||
		    range->delta < -(gint64)G_MAXINT || range->delta > G_MAXINT ||
		    start < 0 || end > G_MAXINT ||
		    end - range->delta < start || end - range->delta > initial_length)
			return FALSE;

		preceding_delta += range->delta;
	}

	if ((gint64)initial_length + preceding_delta != final_length)
		return FALSE;

	for (guint i = n_ranges; i > 0; i--)
	{
		GtkSourceEditRange *range = &ranges[i - 1];
		gint64 next_length = intermediate_length + range->delta;
		gint64 end;

		preceding_delta -= range->delta;
		end = range->end - preceding_delta;
		if (next_length < 0 || next_length > G_MAXINT ||
		    end > next_length || end - range->delta > intermediate_length)
			return FALSE;

		intermediate_length = next_length;
	}

	/* Validation above guarantees that each narrowing conversion is exact. */
	preceding_delta = 0;
	for (guint i = 0; i < n_ranges; i++)
	{
		GtkSourceEditRange *range = &ranges[i];

		range->start = (int)(range->start - preceding_delta);
		range->end = (int)(range->end - preceding_delta);
		preceding_delta += range->delta;
	}

	return TRUE;
}

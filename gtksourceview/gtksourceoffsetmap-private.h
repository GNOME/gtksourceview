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

#define GTK_SOURCE_OFFSET_MAP_STRIDE 256

typedef struct
{
	int byte;
	int character;
} GtkSourceOffsetCheckpoint;

typedef struct
{
	const char                *text;
	GArray                    *checkpoints;
	int                        byte_length;
	int                        char_length;
	GtkSourceOffsetCheckpoint  cursor;
} GtkSourceOffsetMap;

static inline void
_gtk_source_offset_map_init (GtkSourceOffsetMap *map,
                             const char         *text,
                             int                 byte_length,
                             int                 char_length)
{
	*map = (GtkSourceOffsetMap) {
		.text = text,
		.byte_length = byte_length,
		.char_length = char_length,
	};
}

static inline void
_gtk_source_offset_map_clear (GtkSourceOffsetMap *map)
{
	g_clear_pointer (&map->checkpoints, g_array_unref);
}

static inline int
_gtk_source_offset_map_lookup (GtkSourceOffsetMap *map,
                               int                 byte)
{
	GtkSourceOffsetCheckpoint cursor;

	/* PCRE uses -1 for an unmatched capture. */
	if (byte == -1)
		return -1;

	g_assert (byte >= 0 && byte <= map->byte_length);

	if (map->byte_length == map->char_length)
		return byte;

	cursor = map->cursor;

	if (byte < cursor.byte || byte - cursor.byte > GTK_SOURCE_OFFSET_MAP_STRIDE)
	{
		if (map->byte_length < GTK_SOURCE_OFFSET_MAP_STRIDE)
		{
			cursor = (GtkSourceOffsetCheckpoint) { 0, 0 };
		}
		else
		{
			guint needed = byte / GTK_SOURCE_OFFSET_MAP_STRIDE + 1;

			if (map->checkpoints == NULL)
				map->checkpoints = g_array_new (FALSE, FALSE, sizeof (GtkSourceOffsetCheckpoint));

			if (map->checkpoints->len < needed)
			{
				guint first = map->checkpoints->len;
				GtkSourceOffsetCheckpoint point = { 0, 0 };

				if (first != 0)
					point = g_array_index (map->checkpoints, GtkSourceOffsetCheckpoint, first - 1);

				g_array_set_size (map->checkpoints, needed);

				for (guint i = first; i < needed; i++)
				{
					int target = i * GTK_SOURCE_OFFSET_MAP_STRIDE;

					while (point.byte < target)
					{
						int next = g_utf8_next_char (map->text + point.byte) - map->text;

						if (next > target)
							break;

						point.byte = next;
						point.character++;
					}

					g_array_index (map->checkpoints, GtkSourceOffsetCheckpoint, i) = point;
				}
			}

			cursor = g_array_index (map->checkpoints, GtkSourceOffsetCheckpoint,
			                        byte / GTK_SOURCE_OFFSET_MAP_STRIDE);
		}
	}

	cursor.character += g_utf8_pointer_to_offset (map->text + cursor.byte,
	                                              map->text + byte);
	cursor.byte = byte;
	map->cursor = cursor;

	return cursor.character;
}

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

#include "gtksourceview/gtksourceoffsetmap-private.h"

static void
check_text (const char *text)
{
	GtkSourceOffsetMap map;
	GArray *bytes = g_array_new (FALSE, FALSE, sizeof (int));
	int length = strlen (text);
	int characters = g_utf8_strlen (text, length);

	_gtk_source_offset_map_init (&map, text, length, characters);
	g_assert_cmpint (_gtk_source_offset_map_lookup (&map, -1), ==, -1);

	for (const char *p = text; ; p = g_utf8_next_char (p))
	{
		int offset = p - text;
		g_array_append_val (bytes, offset);
		if (*p == 0)
			break;
	}

	/* Captures can be nested, unordered, empty, or absent. */
	for (guint i = 0; i < bytes->len; i++)
		g_assert_cmpint (_gtk_source_offset_map_lookup (&map, g_array_index (bytes, int, i)), ==, i);
	for (int i = bytes->len - 1; i >= 0; i--)
		g_assert_cmpint (_gtk_source_offset_map_lookup (&map, g_array_index (bytes, int, i)), ==, i);
	for (guint i = 0; i < 10000; i++)
	{
		guint index = g_test_rand_int_range (0, bytes->len);
		g_assert_cmpint (_gtk_source_offset_map_lookup (&map, g_array_index (bytes, int, index)), ==, index);
	}

	if (length == characters)
		g_assert_null (map.checkpoints);

	_gtk_source_offset_map_clear (&map);
	g_array_unref (bytes);
}

static void
test_offsets (void)
{
	GString *text = g_string_new (NULL);

	check_text ("");
	check_text ("a");
	check_text ("é");
	check_text ("aé🦊e\314\201α\r\n");

	for (guint i = 0; i < 4096; i++)
		g_string_append (text, "0123456789abcdef");
	check_text (text->str);

	/* Every alignment of a multi-byte character across a checkpoint. */
	for (guint i = 0; i < 8; i++)
	{
		g_string_truncate (text, 0);
		for (guint j = 0; j < 253 + i; j++)
			g_string_append_c (text, 'a');
		for (guint j = 0; j < 1024; j++)
			g_string_append (text, "é🦊e\314\201α");
		check_text (text->str);
	}

	g_string_free (text, TRUE);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/offset-map/capture-boundaries", test_offsets);
	return g_test_run ();
}

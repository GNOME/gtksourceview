/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014 - Sébastien Wilmet <swilmet@gnome.org>
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
 */

#pragma once

#include <glib.h>
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

/*
 * GtkSourceEncodingDuplicates:
 * @GTK_SOURCE_ENCODING_DUPLICATES_KEEP_FIRST: Keep the first occurrence.
 * @GTK_SOURCE_ENCODING_DUPLICATES_KEEP_LAST: Keep the last occurrence.
 *
 * Specifies which encoding occurrence to keep when removing duplicated
 * encodings in a list with [method@Encoding.remove_duplicates].
 */
typedef enum _GtkSourceEncodingDuplicates
{
	GTK_SOURCE_ENCODING_DUPLICATES_KEEP_FIRST,
	GTK_SOURCE_ENCODING_DUPLICATES_KEEP_LAST
} GtkSourceEncodingDuplicates;

GTK_SOURCE_INTERNAL
GSList *_gtk_source_encoding_remove_duplicates (GSList                      *encodings,
                                                GtkSourceEncodingDuplicates  removal_type);

G_END_DECLS

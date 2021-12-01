/*
 * This file is part of GtkSourceView
 *
 * Copyright 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_SPACE_DRAWER (gtk_source_space_drawer_get_type())

/**
 * GtkSourceSpaceTypeFlags:
 * @GTK_SOURCE_SPACE_TYPE_NONE: No flags.
 * @GTK_SOURCE_SPACE_TYPE_SPACE: Space character.
 * @GTK_SOURCE_SPACE_TYPE_TAB: Tab character.
 * @GTK_SOURCE_SPACE_TYPE_NEWLINE: Line break character. If the
 *   #GtkSourceBuffer:implicit-trailing-newline property is %TRUE,
 *   #GtkSourceSpaceDrawer also draws a line break at the end of the buffer.
 * @GTK_SOURCE_SPACE_TYPE_NBSP: Non-breaking space character.
 * @GTK_SOURCE_SPACE_TYPE_ALL: All white spaces.
 *
 * #GtkSourceSpaceTypeFlags contains flags for white space types.
 */
typedef enum _GtkSourceSpaceTypeFlags
{
	GTK_SOURCE_SPACE_TYPE_NONE      = 0,
	GTK_SOURCE_SPACE_TYPE_SPACE     = 1 << 0,
	GTK_SOURCE_SPACE_TYPE_TAB       = 1 << 1,
	GTK_SOURCE_SPACE_TYPE_NEWLINE   = 1 << 2,
	GTK_SOURCE_SPACE_TYPE_NBSP      = 1 << 3,
	GTK_SOURCE_SPACE_TYPE_ALL       = 0xf
} GtkSourceSpaceTypeFlags;

/**
 * GtkSourceSpaceLocationFlags:
 * @GTK_SOURCE_SPACE_LOCATION_NONE: No flags.
 * @GTK_SOURCE_SPACE_LOCATION_LEADING: Leading white spaces on a line, i.e. the
 *   indentation.
 * @GTK_SOURCE_SPACE_LOCATION_INSIDE_TEXT: White spaces inside a line of text.
 * @GTK_SOURCE_SPACE_LOCATION_TRAILING: Trailing white spaces on a line.
 * @GTK_SOURCE_SPACE_LOCATION_ALL: White spaces anywhere.
 *
 * #GtkSourceSpaceLocationFlags contains flags for white space locations.
 *
 * If a line contains only white spaces (no text), the white spaces match both
 * %GTK_SOURCE_SPACE_LOCATION_LEADING and %GTK_SOURCE_SPACE_LOCATION_TRAILING.
 */
typedef enum _GtkSourceSpaceLocationFlags
{
	GTK_SOURCE_SPACE_LOCATION_NONE        = 0,
	GTK_SOURCE_SPACE_LOCATION_LEADING     = 1 << 0,
	GTK_SOURCE_SPACE_LOCATION_INSIDE_TEXT = 1 << 1,
	GTK_SOURCE_SPACE_LOCATION_TRAILING    = 1 << 2,
	GTK_SOURCE_SPACE_LOCATION_ALL         = 0x7
} GtkSourceSpaceLocationFlags;

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceSpaceDrawer, gtk_source_space_drawer, GTK_SOURCE, SPACE_DRAWER, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSpaceDrawer    *gtk_source_space_drawer_new                     (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSpaceTypeFlags  gtk_source_space_drawer_get_types_for_locations (GtkSourceSpaceDrawer        *drawer,
                                                                          GtkSourceSpaceLocationFlags  locations);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_space_drawer_set_types_for_locations (GtkSourceSpaceDrawer        *drawer,
                                                                          GtkSourceSpaceLocationFlags  locations,
                                                                          GtkSourceSpaceTypeFlags      types);
GTK_SOURCE_AVAILABLE_IN_ALL
GVariant                *gtk_source_space_drawer_get_matrix              (GtkSourceSpaceDrawer        *drawer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_space_drawer_set_matrix              (GtkSourceSpaceDrawer        *drawer,
                                                                          GVariant                    *matrix);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_space_drawer_get_enable_matrix       (GtkSourceSpaceDrawer        *drawer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_space_drawer_set_enable_matrix       (GtkSourceSpaceDrawer        *drawer,
                                                                          gboolean                     enable_matrix);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_space_drawer_bind_matrix_setting     (GtkSourceSpaceDrawer        *drawer,
                                                                          GSettings                   *settings,
                                                                          const gchar                 *key,
                                                                          GSettingsBindFlags           flags);

G_END_DECLS

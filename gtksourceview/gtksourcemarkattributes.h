/*
 * This file is part of GtkSourceView
 *
 * Copyright 2010 - Jesse van den Kieboom
 * Copyright 2010 - Krzesimir Nowak
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

#define GTK_SOURCE_TYPE_MARK_ATTRIBUTES (gtk_source_mark_attributes_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceMarkAttributes, gtk_source_mark_attributes, GTK_SOURCE, MARK_ATTRIBUTES, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceMarkAttributes *gtk_source_mark_attributes_new                (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_mark_attributes_set_background     (GtkSourceMarkAttributes *attributes,
                                                                        const GdkRGBA           *background);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_mark_attributes_get_background     (GtkSourceMarkAttributes *attributes,
                                                                        GdkRGBA                 *background);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_mark_attributes_set_icon_name      (GtkSourceMarkAttributes *attributes,
                                                                        const gchar             *icon_name);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_mark_attributes_get_icon_name      (GtkSourceMarkAttributes *attributes);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_mark_attributes_set_gicon          (GtkSourceMarkAttributes *attributes,
                                                                        GIcon                   *gicon);
GTK_SOURCE_AVAILABLE_IN_ALL
GIcon                   *gtk_source_mark_attributes_get_gicon          (GtkSourceMarkAttributes *attributes);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_mark_attributes_set_pixbuf         (GtkSourceMarkAttributes *attributes,
                                                                        const GdkPixbuf         *pixbuf);
GTK_SOURCE_AVAILABLE_IN_ALL
const GdkPixbuf         *gtk_source_mark_attributes_get_pixbuf         (GtkSourceMarkAttributes *attributes);
GTK_SOURCE_AVAILABLE_IN_ALL
GdkPaintable            *gtk_source_mark_attributes_render_icon        (GtkSourceMarkAttributes *attributes,
                                                                        GtkWidget               *widget,
                                                                        gint                     size);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar                   *gtk_source_mark_attributes_get_tooltip_text   (GtkSourceMarkAttributes *attributes,
                                                                        GtkSourceMark           *mark);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar                   *gtk_source_mark_attributes_get_tooltip_markup (GtkSourceMarkAttributes *attributes,
                                                                        GtkSourceMark           *mark);

G_END_DECLS

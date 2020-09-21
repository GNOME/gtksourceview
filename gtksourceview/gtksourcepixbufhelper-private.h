/*
 * This file is part of GtkSourceView
 *
 * Copyright 2010 - Jesse van den Kieboom
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

#include <gtk/gtk.h>

#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkSourcePixbufHelper *gtk_source_pixbuf_helper_new           (void);
G_GNUC_INTERNAL
void                   gtk_source_pixbuf_helper_free          (GtkSourcePixbufHelper *helper);
G_GNUC_INTERNAL
void                   gtk_source_pixbuf_helper_set_pixbuf    (GtkSourcePixbufHelper *helper,
                                                               const GdkPixbuf       *pixbuf);
G_GNUC_INTERNAL
GdkPixbuf             *gtk_source_pixbuf_helper_get_pixbuf    (GtkSourcePixbufHelper *helper);
G_GNUC_INTERNAL
void                   gtk_source_pixbuf_helper_set_icon_name (GtkSourcePixbufHelper *helper,
                                                               const gchar           *icon_name);
G_GNUC_INTERNAL
const gchar           *gtk_source_pixbuf_helper_get_icon_name (GtkSourcePixbufHelper *helper);
G_GNUC_INTERNAL
void                   gtk_source_pixbuf_helper_set_gicon     (GtkSourcePixbufHelper *helper,
                                                               GIcon                 *gicon);
G_GNUC_INTERNAL
GIcon                 *gtk_source_pixbuf_helper_get_gicon     (GtkSourcePixbufHelper *helper);
G_GNUC_INTERNAL
GdkPaintable          *gtk_source_pixbuf_helper_render        (GtkSourcePixbufHelper *helper,
                                                               GtkWidget             *widget,
                                                               gint                   size);

G_END_DECLS

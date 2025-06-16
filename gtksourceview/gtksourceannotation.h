/*
 * gtksourceannotation.h
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_ANNOTATION (gtk_source_annotation_get_type())


GTK_SOURCE_AVAILABLE_IN_5_18
G_DECLARE_FINAL_TYPE (GtkSourceAnnotation, gtk_source_annotation, GTK_SOURCE, ANNOTATION, GObject)

GTK_SOURCE_AVAILABLE_IN_5_18
GtkSourceAnnotation *gtk_source_annotation_new            (const char          *text,
                                                           const char          *icon_name,
                                                           int                  line);
GTK_SOURCE_AVAILABLE_IN_5_18
GtkSourceAnnotation *gtk_source_annotation_new_with_color (const char          *text,
                                                           const char          *icon_name,
                                                           int                  line,
                                                           const GdkRGBA       *color);
GTK_SOURCE_AVAILABLE_IN_5_18
const char          *gtk_source_annotation_get_text       (GtkSourceAnnotation *self);
GTK_SOURCE_AVAILABLE_IN_5_18
const char          *gtk_source_annotation_get_icon_name  (GtkSourceAnnotation *self);
GTK_SOURCE_AVAILABLE_IN_5_18
int                  gtk_source_annotation_get_line       (GtkSourceAnnotation *self);
GTK_SOURCE_AVAILABLE_IN_5_18
gboolean             gtk_source_annotation_get_color      (GtkSourceAnnotation *self,
                                                           GdkRGBA             *color);

G_END_DECLS

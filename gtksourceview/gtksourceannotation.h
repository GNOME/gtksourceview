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

/**
 * GtkSourceAnnotationStyle:
 * @GTK_SOURCE_ANNOTATION_STYLE_NONE: same color as drawn spaces
 * @GTK_SOURCE_ANNOTATION_STYLE_WARNING: same as the diff:changed-line foreground color
 * @GTK_SOURCE_ANNOTATION_STYLE_ERROR: same as the diff:removed-line foreground color
 * @GTK_SOURCE_ANNOTATION_STYLE_ACCENT: same as the diff:added-line foreground color
 *
 * Since: 5.18
 */
typedef enum _GtkSourceAnnotationStyle
{
	GTK_SOURCE_ANNOTATION_STYLE_NONE,
	GTK_SOURCE_ANNOTATION_STYLE_WARNING,
	GTK_SOURCE_ANNOTATION_STYLE_ERROR,
	GTK_SOURCE_ANNOTATION_STYLE_ACCENT
} GtkSourceAnnotationStyle;

#define GTK_SOURCE_TYPE_ANNOTATION (gtk_source_annotation_get_type())


GTK_SOURCE_AVAILABLE_IN_5_18
G_DECLARE_FINAL_TYPE (GtkSourceAnnotation, gtk_source_annotation, GTK_SOURCE, ANNOTATION, GObject)

GTK_SOURCE_AVAILABLE_IN_5_18
GtkSourceAnnotation     *gtk_source_annotation_new             (const char              *description,
                                                                GIcon                   *icon,
                                                                int                      line,
                                                                GtkSourceAnnotationStyle style);
GTK_SOURCE_AVAILABLE_IN_5_18
const char              *gtk_source_annotation_get_description (GtkSourceAnnotation     *self);
GTK_SOURCE_AVAILABLE_IN_5_18
GIcon                   *gtk_source_annotation_get_icon        (GtkSourceAnnotation     *self);
GTK_SOURCE_AVAILABLE_IN_5_18
int                      gtk_source_annotation_get_line        (GtkSourceAnnotation     *self);
GTK_SOURCE_AVAILABLE_IN_5_18
GtkSourceAnnotationStyle gtk_source_annotation_get_style       (GtkSourceAnnotation     *self);

G_END_DECLS

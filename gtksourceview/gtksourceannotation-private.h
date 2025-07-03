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

#include <gtk/gtk.h>

#include "gtksourceannotation.h"
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

GTK_SOURCE_INTERNAL
gboolean _gtk_source_annotation_get_rect       (GtkSourceAnnotation *self,
                                                GdkRectangle        *rect);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_annotation_contains_point (GtkSourceAnnotation *self,
                                                int                  x,
                                                int                  y);
GTK_SOURCE_INTERNAL
void     _gtk_source_annotation_draw           (GtkSourceAnnotation *self,
                                                GtkSnapshot         *snapshot,
                                                GtkSourceView       *view,
                                                GdkRectangle         rectangle,
                                                const GdkRGBA       *color);

G_END_DECLS

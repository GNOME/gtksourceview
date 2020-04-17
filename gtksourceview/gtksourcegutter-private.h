/*
 * This file is part of GtkSourceView
 *
 * Copyright 2009 - Jesse van den Kieboom
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

#include "gtksourcetypes.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkSourceGutter      *_gtk_source_gutter_new            (GtkTextWindowType     type,
                                                         GtkSourceView        *view);
G_GNUC_INTERNAL
GtkSourceGutterLines *_gtk_source_gutter_get_lines      (GtkSourceGutter      *gutter);
G_GNUC_INTERNAL
void                  _gtk_source_gutter_queue_draw     (GtkSourceGutter      *gutter);
G_GNUC_INTERNAL
void                  _gtk_source_gutter_css_changed    (GtkSourceGutter      *gutter,
                                                         GtkCssStyleChange    *change);
G_GNUC_INTERNAL
void                  _gtk_source_gutter_apply_scheme   (GtkSourceGutter      *gutter,
                                                         GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
void                  _gtk_source_gutter_unapply_scheme (GtkSourceGutter      *gutter,
                                                         GtkSourceStyleScheme *scheme);

G_END_DECLS

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

#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"
#include "gtksourcegutterrendererpixbuf.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_GUTTER_RENDERER_MARKS	(gtk_source_gutter_renderer_marks_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceGutterRendererMarks, gtk_source_gutter_renderer_marks, GTK_SOURCE, GUTTER_RENDERER_MARKS, GtkSourceGutterRendererPixbuf)

G_GNUC_INTERNAL
GtkSourceGutterRenderer *gtk_source_gutter_renderer_marks_new (void);

G_END_DECLS

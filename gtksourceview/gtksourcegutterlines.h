/*
 * This file is part of GtkSourceView
 *
 * Copyright 2019 - Christian Hergert <chergert@redhat.com>
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
#include "gtksourcegutterrenderer.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_GUTTER_LINES (gtk_source_gutter_lines_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceGutterLines, gtk_source_gutter_lines, GTK_SOURCE, GUTTER_LINES, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
guint          gtk_source_gutter_lines_get_first        (GtkSourceGutterLines                 *lines);
GTK_SOURCE_AVAILABLE_IN_ALL
guint          gtk_source_gutter_lines_get_last         (GtkSourceGutterLines                 *lines);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_get_iter_at_line (GtkSourceGutterLines                 *lines,
                                                         GtkTextIter                          *iter,
                                                         guint                                 line);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkTextView   *gtk_source_gutter_lines_get_view         (GtkSourceGutterLines                 *lines);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkTextBuffer *gtk_source_gutter_lines_get_buffer       (GtkSourceGutterLines                 *lines);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_add_qclass       (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         GQuark                                qname);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_add_class        (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         const gchar                          *name);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_remove_class     (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         const gchar                          *name);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_remove_qclass    (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         GQuark                                qname);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean       gtk_source_gutter_lines_has_class        (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         const gchar                          *name);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean       gtk_source_gutter_lines_has_qclass       (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         GQuark                                qname);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean       gtk_source_gutter_lines_is_cursor        (GtkSourceGutterLines                 *lines,
                                                         guint                                 line);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean       gtk_source_gutter_lines_is_prelit        (GtkSourceGutterLines                 *lines,
                                                         guint                                 line);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean       gtk_source_gutter_lines_is_selected      (GtkSourceGutterLines                 *lines,
                                                         guint                                 line);
GTK_SOURCE_AVAILABLE_IN_ALL
void           gtk_source_gutter_lines_get_line_yrange  (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         GtkSourceGutterRendererAlignmentMode  mode,
                                                         gint                                 *y,
                                                         gint                                 *height)
  G_GNUC_DEPRECATED_FOR (gtk_source_gutter_lines_get_line_extent);
GTK_SOURCE_AVAILABLE_IN_5_16
void           gtk_source_gutter_lines_get_line_extent  (GtkSourceGutterLines                 *lines,
                                                         guint                                 line,
                                                         GtkSourceGutterRendererAlignmentMode  mode,
                                                         double                               *y,
                                                         double                               *height);
GTK_SOURCE_AVAILABLE_IN_5_6
gboolean       gtk_source_gutter_lines_has_any_class    (GtkSourceGutterLines                 *lines,
                                                         guint                                 line);

G_END_DECLS

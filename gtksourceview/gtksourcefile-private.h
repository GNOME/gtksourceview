/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014, 2015 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gtksourcefile.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void             _gtk_source_file_set_encoding            (GtkSourceFile            *file,
                                                           const GtkSourceEncoding  *encoding);
G_GNUC_INTERNAL
void             _gtk_source_file_set_newline_type        (GtkSourceFile            *file,
                                                           GtkSourceNewlineType      newline_type);
G_GNUC_INTERNAL
void             _gtk_source_file_set_compression_type    (GtkSourceFile            *file,
                                                           GtkSourceCompressionType  compression_type);
G_GNUC_INTERNAL
GMountOperation *_gtk_source_file_create_mount_operation  (GtkSourceFile            *file);
G_GNUC_INTERNAL
gboolean         _gtk_source_file_get_modification_time   (GtkSourceFile            *file,
                                                           gint64                   *modification_time);
G_GNUC_INTERNAL
void             _gtk_source_file_set_modification_time   (GtkSourceFile            *file,
                                                           gint64                    modification_time);
G_GNUC_INTERNAL
void             _gtk_source_file_set_externally_modified (GtkSourceFile            *file,
                                                           gboolean                  externally_modified);
G_GNUC_INTERNAL
void             _gtk_source_file_set_deleted             (GtkSourceFile            *file,
                                                           gboolean                  deleted);
G_GNUC_INTERNAL
void             _gtk_source_file_set_readonly            (GtkSourceFile            *file,
                                                           gboolean                  readonly);

G_END_DECLS

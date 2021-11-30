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

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_FILE (gtk_source_file_get_type())

/**
 * GtkSourceNewlineType:
 * @GTK_SOURCE_NEWLINE_TYPE_LF: line feed, used on UNIX.
 * @GTK_SOURCE_NEWLINE_TYPE_CR: carriage return, used on Mac.
 * @GTK_SOURCE_NEWLINE_TYPE_CR_LF: carriage return followed by a line feed, used
 *   on Windows.
 */
typedef enum _GtkSourceNewlineType
{
	GTK_SOURCE_NEWLINE_TYPE_LF,
	GTK_SOURCE_NEWLINE_TYPE_CR,
	GTK_SOURCE_NEWLINE_TYPE_CR_LF
} GtkSourceNewlineType;

/**
 * GTK_SOURCE_NEWLINE_TYPE_DEFAULT:
 *
 * The default newline type on the current OS.
 */
#ifdef G_OS_WIN32
#define GTK_SOURCE_NEWLINE_TYPE_DEFAULT GTK_SOURCE_NEWLINE_TYPE_CR_LF
#else
#define GTK_SOURCE_NEWLINE_TYPE_DEFAULT GTK_SOURCE_NEWLINE_TYPE_LF
#endif

/**
 * GtkSourceCompressionType:
 * @GTK_SOURCE_COMPRESSION_TYPE_NONE: plain text.
 * @GTK_SOURCE_COMPRESSION_TYPE_GZIP: gzip compression.
 */
typedef enum _GtkSourceCompressionType
{
	GTK_SOURCE_COMPRESSION_TYPE_NONE,
	GTK_SOURCE_COMPRESSION_TYPE_GZIP
} GtkSourceCompressionType;

/**
 * GtkSourceMountOperationFactory:
 * @file: a #GtkSourceFile.
 * @userdata: user data
 *
 * Type definition for a function that will be called to create a
 * [class@Gio.MountOperation]. This is useful for creating a [class@Gtk.MountOperation].
 */
typedef GMountOperation *(*GtkSourceMountOperationFactory) (GtkSourceFile *file,
                                                            gpointer       userdata);

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceFile, gtk_source_file, GTK_SOURCE, FILE, GObject)

struct _GtkSourceFileClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};


GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFile            *gtk_source_file_new                         (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GFile                    *gtk_source_file_get_location                (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_set_location                (GtkSourceFile                  *file,
                                                                       GFile                          *location);
GTK_SOURCE_AVAILABLE_IN_ALL
const GtkSourceEncoding  *gtk_source_file_get_encoding                (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceNewlineType      gtk_source_file_get_newline_type            (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompressionType  gtk_source_file_get_compression_type        (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_set_mount_operation_factory (GtkSourceFile                  *file,
                                                                       GtkSourceMountOperationFactory  callback,
                                                                       gpointer                        user_data,
                                                                       GDestroyNotify                  notify);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_check_file_on_disk          (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_is_local                    (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_is_externally_modified      (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_is_deleted                  (GtkSourceFile                  *file);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_is_readonly                 (GtkSourceFile                  *file);

G_END_DECLS

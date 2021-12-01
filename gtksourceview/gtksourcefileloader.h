/*
 * This file is part of GtkSourceView
 *
 * Copyright 2005 - Paolo Maggi
 * Copyright 2007 - Paolo Maggi, Steve Frécinaux
 * Copyright 2008 - Jesse van den Kieboom
 * Copyright 2014 - Sébastien Wilmet
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
#include "gtksourcefile.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_FILE_LOADER  (gtk_source_file_loader_get_type())
#define GTK_SOURCE_FILE_LOADER_ERROR (gtk_source_file_loader_error_quark())

/**
 * GtkSourceFileLoaderError:
 * @GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG: The file is too big.
 * @GTK_SOURCE_FILE_LOADER_ERROR_ENCODING_AUTO_DETECTION_FAILED: It is not
 * possible to detect the encoding automatically.
 * @GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK: There was an encoding
 * conversion error and it was needed to use a fallback character.
 *
 * An error code used with the %GTK_SOURCE_FILE_LOADER_ERROR domain.
 */
typedef enum _GtkSourceFileLoaderError
{
	GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
	GTK_SOURCE_FILE_LOADER_ERROR_ENCODING_AUTO_DETECTION_FAILED,
	GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK
} GtkSourceFileLoaderError;

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceFileLoader, gtk_source_file_loader, GTK_SOURCE, FILE_LOADER, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GQuark                    gtk_source_file_loader_error_quark             (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFileLoader      *gtk_source_file_loader_new                     (GtkSourceBuffer        *buffer,
                                                                          GtkSourceFile          *file);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFileLoader      *gtk_source_file_loader_new_from_stream         (GtkSourceBuffer        *buffer,
                                                                          GtkSourceFile          *file,
                                                                          GInputStream           *stream);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_loader_set_candidate_encodings (GtkSourceFileLoader    *loader,
                                                                          GSList                 *candidate_encodings);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceBuffer          *gtk_source_file_loader_get_buffer              (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFile            *gtk_source_file_loader_get_file                (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GFile                    *gtk_source_file_loader_get_location            (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GInputStream             *gtk_source_file_loader_get_input_stream        (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_loader_load_async              (GtkSourceFileLoader    *loader,
                                                                          gint                    io_priority,
                                                                          GCancellable           *cancellable,
                                                                          GFileProgressCallback   progress_callback,
                                                                          gpointer                progress_callback_data,
                                                                          GDestroyNotify          progress_callback_notify,
                                                                          GAsyncReadyCallback     callback,
                                                                          gpointer                user_data);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_loader_load_finish             (GtkSourceFileLoader    *loader,
                                                                          GAsyncResult           *result,
                                                                          GError                **error);
GTK_SOURCE_AVAILABLE_IN_ALL
const GtkSourceEncoding  *gtk_source_file_loader_get_encoding            (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceNewlineType      gtk_source_file_loader_get_newline_type        (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompressionType  gtk_source_file_loader_get_compression_type    (GtkSourceFileLoader    *loader);

G_END_DECLS

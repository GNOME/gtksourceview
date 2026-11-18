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
 * @GTK_SOURCE_FILE_LOADER_ERROR_LINE_TOO_LONG: A line length decision was declined.
 * @GTK_SOURCE_FILE_LOADER_ERROR_TOO_MANY_LINES: A line count decision was declined.
 *
 * An error code used with the %GTK_SOURCE_FILE_LOADER_ERROR domain.
 */
typedef enum _GtkSourceFileLoaderError
{
	GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
	GTK_SOURCE_FILE_LOADER_ERROR_ENCODING_AUTO_DETECTION_FAILED,
	GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK,
	GTK_SOURCE_FILE_LOADER_ERROR_LINE_TOO_LONG,
	GTK_SOURCE_FILE_LOADER_ERROR_TOO_MANY_LINES
} GtkSourceFileLoaderError;

/**
 * GtkSourceFileLoaderPolicyReason:
 * @GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE: Expanded size in bytes.
 * @GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH: Decoded characters on a line.
 * @GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT: Number of lines.
 *
 * Since: 5.24
 */
typedef enum _GtkSourceFileLoaderPolicyReason
{
	GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE,
	GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH,
	GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT
} GtkSourceFileLoaderPolicyReason;

#define GTK_SOURCE_TYPE_FILE_LOADER_POLICY_DECISION (gtk_source_file_loader_policy_decision_get_type())

GTK_SOURCE_AVAILABLE_IN_5_24
G_DECLARE_FINAL_TYPE (GtkSourceFileLoaderPolicyDecision, gtk_source_file_loader_policy_decision, GTK_SOURCE, FILE_LOADER_POLICY_DECISION, GObject)

GTK_SOURCE_AVAILABLE_IN_5_24
GtkSourceFileLoaderPolicyReason gtk_source_file_loader_policy_decision_get_reason    (GtkSourceFileLoaderPolicyDecision *decision);
GTK_SOURCE_AVAILABLE_IN_5_24
guint64                         gtk_source_file_loader_policy_decision_get_threshold (GtkSourceFileLoaderPolicyDecision *decision);
GTK_SOURCE_AVAILABLE_IN_5_24
guint64                         gtk_source_file_loader_policy_decision_get_value     (GtkSourceFileLoaderPolicyDecision *decision);
GTK_SOURCE_AVAILABLE_IN_5_24
void                            gtk_source_file_loader_policy_decision_ignore        (GtkSourceFileLoaderPolicyDecision *decision);
GTK_SOURCE_AVAILABLE_IN_5_24
void                            gtk_source_file_loader_policy_decision_use           (GtkSourceFileLoaderPolicyDecision *decision);

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceFileLoader, gtk_source_file_loader, GTK_SOURCE, FILE_LOADER, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GQuark                    gtk_source_file_loader_error_quark               (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceBuffer          *gtk_source_file_loader_get_buffer                (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompressionType  gtk_source_file_loader_get_compression_type      (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
const GtkSourceEncoding  *gtk_source_file_loader_get_encoding              (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFile            *gtk_source_file_loader_get_file                  (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GInputStream             *gtk_source_file_loader_get_input_stream          (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_5_24
guint64                   gtk_source_file_loader_get_line_count_threshold  (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_5_24
guint64                   gtk_source_file_loader_get_line_length_threshold (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GFile                    *gtk_source_file_loader_get_location              (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_5_22
guint64                   gtk_source_file_loader_get_max_size              (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceNewlineType      gtk_source_file_loader_get_newline_type          (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_5_24
guint64                   gtk_source_file_loader_get_size_threshold        (GtkSourceFileLoader    *loader);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_loader_load_async                (GtkSourceFileLoader    *loader,
                                                                            gint                    io_priority,
                                                                            GCancellable           *cancellable,
                                                                            GFileProgressCallback   progress_callback,
                                                                            gpointer                progress_callback_data,
                                                                            GDestroyNotify          progress_callback_notify,
                                                                            GAsyncReadyCallback     callback,
                                                                            gpointer                user_data);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                  gtk_source_file_loader_load_finish               (GtkSourceFileLoader    *loader,
                                                                            GAsyncResult           *result,
                                                                            GError                **error);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFileLoader      *gtk_source_file_loader_new                       (GtkSourceBuffer        *buffer,
                                                                            GtkSourceFile          *file);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceFileLoader      *gtk_source_file_loader_new_from_stream           (GtkSourceBuffer        *buffer,
                                                                            GtkSourceFile          *file,
                                                                            GInputStream           *stream);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_file_loader_set_candidate_encodings   (GtkSourceFileLoader    *loader,
                                                                            GSList                 *candidate_encodings);
GTK_SOURCE_AVAILABLE_IN_5_24
void                      gtk_source_file_loader_set_line_count_threshold  (GtkSourceFileLoader    *loader,
                                                                            guint64                 threshold);
GTK_SOURCE_AVAILABLE_IN_5_24
void                      gtk_source_file_loader_set_line_length_threshold (GtkSourceFileLoader    *loader,
                                                                            guint64                 threshold);
GTK_SOURCE_AVAILABLE_IN_5_22
void                      gtk_source_file_loader_set_max_size              (GtkSourceFileLoader    *loader,
                                                                            guint64                 max_size);
GTK_SOURCE_AVAILABLE_IN_5_24
void                      gtk_source_file_loader_set_size_threshold        (GtkSourceFileLoader    *loader,
                                                                            guint64                 threshold);

G_END_DECLS

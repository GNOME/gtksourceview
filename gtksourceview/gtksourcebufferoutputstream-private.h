/*
 * This file is part of GtkSourceView
 *
 * Copyright 2010 - Ignacio Casal Quinteiro
 * Copyright 2014 - Sébastien Wilmet <swilmet@gnome.org>
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
#include "gtksourcebuffer.h"
#include "gtksourcefile.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_BUFFER_OUTPUT_STREAM (gtk_source_buffer_output_stream_get_type())

GTK_SOURCE_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceBufferOutputStream, gtk_source_buffer_output_stream, GTK_SOURCE, BUFFER_OUTPUT_STREAM, GOutputStream)

GTK_SOURCE_INTERNAL
GtkSourceBufferOutputStream *gtk_source_buffer_output_stream_new                 (GtkSourceBuffer             *buffer,
                                                                                  GSList                      *candidate_encodings,
                                                                                  gboolean                     remove_trailing_newline);
GTK_SOURCE_INTERNAL
GtkSourceNewlineType         gtk_source_buffer_output_stream_detect_newline_type (GtkSourceBufferOutputStream *stream);
GTK_SOURCE_INTERNAL
const GtkSourceEncoding     *gtk_source_buffer_output_stream_get_guessed         (GtkSourceBufferOutputStream *stream);
GTK_SOURCE_INTERNAL
guint                        gtk_source_buffer_output_stream_get_num_fallbacks   (GtkSourceBufferOutputStream *stream);

G_END_DECLS

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#define GTK_SOURCE_TYPE_SNIPPET_CHUNK (gtk_source_snippet_chunk_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceSnippetChunk, gtk_source_snippet_chunk, GTK_SOURCE, SNIPPET_CHUNK, GInitiallyUnowned)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetChunk   *gtk_source_snippet_chunk_new                (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetChunk   *gtk_source_snippet_chunk_copy               (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetContext *gtk_source_snippet_chunk_get_context        (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_context        (GtkSourceSnippetChunk   *chunk,
                                                                      GtkSourceSnippetContext *context);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_chunk_get_spec           (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_spec           (GtkSourceSnippetChunk   *chunk,
                                                                      const gchar             *spec);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                     gtk_source_snippet_chunk_get_focus_position (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_focus_position (GtkSourceSnippetChunk   *chunk,
                                                                      gint                     focus_position);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_chunk_get_text           (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_text           (GtkSourceSnippetChunk   *chunk,
                                                                      const gchar             *text);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_snippet_chunk_get_text_set       (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_text_set       (GtkSourceSnippetChunk   *chunk,
                                                                      gboolean                 text_set);
GTK_SOURCE_AVAILABLE_IN_ALL
const char              *gtk_source_snippet_chunk_get_tooltip_text   (GtkSourceSnippetChunk   *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_chunk_set_tooltip_text   (GtkSourceSnippetChunk   *chunk,
                                                                      const char              *tooltip_text);

G_END_DECLS

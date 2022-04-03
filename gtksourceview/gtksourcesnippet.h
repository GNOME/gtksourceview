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

#define GTK_SOURCE_TYPE_SNIPPET (gtk_source_snippet_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceSnippet, gtk_source_snippet, GTK_SOURCE, SNIPPET, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippet        *gtk_source_snippet_new                        (const gchar           *trigger,
                                                                        const gchar           *language_id);
GTK_SOURCE_AVAILABLE_IN_5_6
GtkSourceSnippet        *gtk_source_snippet_new_parsed                 (const char            *text,
                                                                        GError               **error);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippet        *gtk_source_snippet_copy                       (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_get_name                   (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_set_name                   (GtkSourceSnippet      *snippet,
                                                                        const gchar           *name);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_get_trigger                (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_set_trigger                (GtkSourceSnippet      *snippet,
                                                                        const gchar           *trigger);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_get_language_id            (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_set_language_id            (GtkSourceSnippet      *snippet,
                                                                        const gchar           *language_id);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_get_description            (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_set_description            (GtkSourceSnippet      *snippet,
                                                                        const gchar           *description);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_add_chunk                  (GtkSourceSnippet      *snippet,
                                                                        GtkSourceSnippetChunk *chunk);
GTK_SOURCE_AVAILABLE_IN_ALL
guint                    gtk_source_snippet_get_n_chunks               (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                     gtk_source_snippet_get_focus_position         (GtkSourceSnippet      *snippet);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetChunk   *gtk_source_snippet_get_nth_chunk              (GtkSourceSnippet      *snippet,
                                                                        guint                  nth);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetContext *gtk_source_snippet_get_context                (GtkSourceSnippet      *snippet);

G_END_DECLS

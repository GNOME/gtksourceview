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

#define GTK_SOURCE_TYPE_SNIPPET_CONTEXT (gtk_source_snippet_context_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceSnippetContext, gtk_source_snippet_context, GTK_SOURCE, SNIPPET_CONTEXT, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetContext *gtk_source_snippet_context_new             (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_clear_variables (GtkSourceSnippetContext *self);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_set_variable    (GtkSourceSnippetContext *self,
                                                                     const gchar             *key,
                                                                     const gchar             *value);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_set_constant    (GtkSourceSnippetContext *self,
                                                                     const gchar             *key,
                                                                     const gchar             *value);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_snippet_context_get_variable    (GtkSourceSnippetContext *self,
                                                                     const gchar             *key);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar                   *gtk_source_snippet_context_expand          (GtkSourceSnippetContext *self,
                                                                     const gchar             *input);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_set_tab_width   (GtkSourceSnippetContext *self,
                                                                     gint                     tab_width);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_set_use_spaces  (GtkSourceSnippetContext *self,
                                                                     gboolean                 use_spaces);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_snippet_context_set_line_prefix (GtkSourceSnippetContext *self,
                                                                     const gchar             *line_prefix);

G_END_DECLS

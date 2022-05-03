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

#include <gio/gio.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_SNIPPET_MANAGER (gtk_source_snippet_manager_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceSnippetManager, gtk_source_snippet_manager, GTK_SOURCE, SNIPPET_MANAGER, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippetManager  *gtk_source_snippet_manager_get_default     (void);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const      *gtk_source_snippet_manager_get_search_path (GtkSourceSnippetManager *self);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_snippet_manager_set_search_path (GtkSourceSnippetManager *self,
                                                                      const gchar * const     *dirs);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSnippet         *gtk_source_snippet_manager_get_snippet     (GtkSourceSnippetManager *self,
                                                                      const gchar             *group,
                                                                      const gchar             *language_id,
                                                                      const gchar             *trigger);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             **gtk_source_snippet_manager_list_groups     (GtkSourceSnippetManager *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GListModel               *gtk_source_snippet_manager_list_matching   (GtkSourceSnippetManager *self,
                                                                      const gchar             *group,
                                                                      const gchar             *language_id,
                                                                      const gchar             *trigger_prefix);
GTK_SOURCE_AVAILABLE_IN_5_6
GListModel               *gtk_source_snippet_manager_list_all        (GtkSourceSnippetManager *self);

G_END_DECLS

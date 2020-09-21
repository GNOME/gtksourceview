/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003-2007 - Paolo Maggi <paolo.maggi@polito.it>
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

#include <glib-object.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_STYLE_SCHEME_MANAGER (gtk_source_style_scheme_manager_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceStyleSchemeManager, gtk_source_style_scheme_manager, GTK_SOURCE, STYLE_SCHEME_MANAGER, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceStyleSchemeManager *gtk_source_style_scheme_manager_new                 (void);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceStyleSchemeManager *gtk_source_style_scheme_manager_get_default         (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                         gtk_source_style_scheme_manager_set_search_path     (GtkSourceStyleSchemeManager  *manager,
                                                                                  const gchar * const          *path);
GTK_SOURCE_AVAILABLE_IN_ALL
void                         gtk_source_style_scheme_manager_append_search_path  (GtkSourceStyleSchemeManager  *manager,
                                                                                  const gchar                  *path);
GTK_SOURCE_AVAILABLE_IN_ALL
void                         gtk_source_style_scheme_manager_prepend_search_path (GtkSourceStyleSchemeManager  *manager,
                                                                                  const gchar                  *path);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const         *gtk_source_style_scheme_manager_get_search_path     (GtkSourceStyleSchemeManager  *manager);
GTK_SOURCE_AVAILABLE_IN_ALL
void                         gtk_source_style_scheme_manager_force_rescan        (GtkSourceStyleSchemeManager  *manager);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const         *gtk_source_style_scheme_manager_get_scheme_ids      (GtkSourceStyleSchemeManager  *manager);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceStyleScheme        *gtk_source_style_scheme_manager_get_scheme          (GtkSourceStyleSchemeManager  *manager,
                                                                                  const gchar                  *scheme_id);

G_END_DECLS

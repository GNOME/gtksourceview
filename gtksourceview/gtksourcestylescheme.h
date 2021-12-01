/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

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

#define GTK_SOURCE_TYPE_STYLE_SCHEME (gtk_source_style_scheme_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceStyleScheme, gtk_source_style_scheme, GTK_SOURCE, STYLE_SCHEME, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
const gchar         *gtk_source_style_scheme_get_id          (GtkSourceStyleScheme *scheme);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar         *gtk_source_style_scheme_get_name        (GtkSourceStyleScheme *scheme);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar         *gtk_source_style_scheme_get_description (GtkSourceStyleScheme *scheme);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const *gtk_source_style_scheme_get_authors     (GtkSourceStyleScheme *scheme);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar         *gtk_source_style_scheme_get_filename    (GtkSourceStyleScheme *scheme);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceStyle      *gtk_source_style_scheme_get_style       (GtkSourceStyleScheme *scheme,
                                                              const gchar          *style_id);
GTK_SOURCE_AVAILABLE_IN_5_4
const char          *gtk_source_style_scheme_get_metadata    (GtkSourceStyleScheme *scheme,
                                                              const char           *name);

G_END_DECLS

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003 - Paolo Maggi <paolo.maggi@polito.it>
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

#define GTK_SOURCE_TYPE_LANGUAGE (gtk_source_language_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceLanguage, gtk_source_language, GTK_SOURCE, LANGUAGE, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_id             (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_name           (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_section        (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean      gtk_source_language_get_hidden         (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_metadata       (GtkSourceLanguage *language,
                                                      const gchar       *name);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar       **gtk_source_language_get_mime_types     (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar       **gtk_source_language_get_globs          (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
gchar       **gtk_source_language_get_style_ids      (GtkSourceLanguage *language);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_style_name     (GtkSourceLanguage *language,
                                                      const gchar       *style_id);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar  *gtk_source_language_get_style_fallback (GtkSourceLanguage *language,
                                                      const gchar       *style_id);

G_END_DECLS

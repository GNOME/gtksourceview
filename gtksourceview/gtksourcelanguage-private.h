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

#include <glib.h>

#include "gtksourcecontextengine-private.h"
#include "gtksourcelanguagemanager.h"

G_BEGIN_DECLS

#define GTK_SOURCE_LANGUAGE_VERSION_1_0 100
#define GTK_SOURCE_LANGUAGE_VERSION_2_0 200

typedef struct
{
	gchar *name;
	gchar *map_to;
} GtkSourceStyleInfo;

GtkSourceLanguage        *_gtk_source_language_new_from_file          (const gchar              *filename,
                                                                       GtkSourceLanguageManager *lm);
GtkSourceLanguageManager *_gtk_source_language_get_language_manager   (GtkSourceLanguage        *language);
const gchar              *_gtk_source_language_get_file_name          (GtkSourceLanguage        *language);
void                      _gtk_source_language_parser_finalize        (void);
gchar                    *_gtk_source_language_translate_string       (GtkSourceLanguage        *language,
                                                                       const gchar              *string);
void                      _gtk_source_language_define_language_styles (GtkSourceLanguage        *language);
gboolean                  _gtk_source_language_file_parse_version2    (GtkSourceLanguage        *language,
                                                                       GtkSourceContextData     *ctx_data);
GtkSourceEngine          *_gtk_source_language_create_engine          (GtkSourceLanguage        *language);
void                      _gtk_source_language_clear_ctx_data         (GtkSourceLanguage        *language,
                                                                       GtkSourceContextData     *ctx_data);
GHashTable               *_gtk_source_language_get_styles             (GtkSourceLanguage        *language);
GtkSourceStyleInfo       *_gtk_source_style_info_new                  (const gchar              *name,
                                                                       const gchar              *map_to);
GtkSourceStyleInfo       *_gtk_source_style_info_copy                 (GtkSourceStyleInfo       *info);
void                      _gtk_source_style_info_free                 (GtkSourceStyleInfo       *info);

G_END_DECLS

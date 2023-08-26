/*
 * This file is part of GtkSourceView
 *
 * Copyright 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#define GTK_SOURCE_TYPE_SEARCH_SETTINGS (gtk_source_search_settings_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceSearchSettings, gtk_source_search_settings, GTK_SOURCE, SEARCH_SETTINGS, GObject)

struct _GtkSourceSearchSettingsClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceSearchSettings *gtk_source_search_settings_new                    (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_search_settings_set_search_text        (GtkSourceSearchSettings *settings,
                                                                            const gchar             *search_text);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_search_settings_get_search_text        (GtkSourceSearchSettings *settings);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_search_settings_set_case_sensitive     (GtkSourceSearchSettings *settings,
                                                                            gboolean                 case_sensitive);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_search_settings_get_case_sensitive     (GtkSourceSearchSettings *settings);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_search_settings_set_at_word_boundaries (GtkSourceSearchSettings *settings,
                                                                            gboolean                 at_word_boundaries);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_search_settings_get_at_word_boundaries (GtkSourceSearchSettings *settings);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_search_settings_set_wrap_around        (GtkSourceSearchSettings *settings,
                                                                            gboolean                 wrap_around);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_search_settings_get_wrap_around        (GtkSourceSearchSettings *settings);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_search_settings_set_regex_enabled      (GtkSourceSearchSettings *settings,
                                                                            gboolean                 regex_enabled);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                 gtk_source_search_settings_get_regex_enabled      (GtkSourceSearchSettings *settings);

GTK_SOURCE_AVAILABLE_IN_5_12
gboolean                 gtk_source_search_settings_get_visible_only       (GtkSourceSearchSettings *settings);

GTK_SOURCE_AVAILABLE_IN_5_12
void                     gtk_source_search_settings_set_visible_only       (GtkSourceSearchSettings *settings,
                                                                            gboolean                 visible_only);

G_END_DECLS

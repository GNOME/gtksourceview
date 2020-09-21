/*
 * This file is part of GtkSourceView
 *
 * Copyright 2010 - Jesse van den Kieboom
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

#include "gtksourcetypes.h"
#include "gtksourcegutterrenderer.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT (gtk_source_gutter_renderer_text_get_type())

struct _GtkSourceGutterRendererTextClass
{
	GtkSourceGutterRendererClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceGutterRendererText, gtk_source_gutter_renderer_text, GTK_SOURCE, GUTTER_RENDERER_TEXT, GtkSourceGutterRenderer)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceGutterRenderer *gtk_source_gutter_renderer_text_new            (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_text_set_markup     (GtkSourceGutterRendererText *renderer,
                                                                         const gchar                 *markup,
                                                                         gint                         length);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_text_set_text       (GtkSourceGutterRendererText *renderer,
                                                                         const gchar                 *text,
                                                                         gint                         length);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_text_measure        (GtkSourceGutterRendererText *renderer,
                                                                         const gchar                 *text,
                                                                         gint                        *width,
                                                                         gint                        *height);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_text_measure_markup (GtkSourceGutterRendererText *renderer,
                                                                         const gchar                 *markup,
                                                                         gint                        *width,
                                                                         gint                        *height);

G_END_DECLS

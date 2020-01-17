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

#define GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF (gtk_source_gutter_renderer_pixbuf_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceGutterRendererPixbuf, gtk_source_gutter_renderer_pixbuf, GTK_SOURCE, GUTTER_RENDERER_PIXBUF, GtkSourceGutterRenderer)

struct _GtkSourceGutterRendererPixbufClass
{
	GtkSourceGutterRendererClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceGutterRenderer *gtk_source_gutter_renderer_pixbuf_new               (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_pixbuf_set_pixbuf        (GtkSourceGutterRendererPixbuf *renderer,
                                                                              GdkPixbuf                     *pixbuf);
GTK_SOURCE_AVAILABLE_IN_ALL
GdkPixbuf               *gtk_source_gutter_renderer_pixbuf_get_pixbuf        (GtkSourceGutterRendererPixbuf *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_pixbuf_set_gicon         (GtkSourceGutterRendererPixbuf *renderer,
                                                                              GIcon                         *icon);
GTK_SOURCE_AVAILABLE_IN_ALL
GIcon                   *gtk_source_gutter_renderer_pixbuf_get_gicon         (GtkSourceGutterRendererPixbuf *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_pixbuf_set_icon_name     (GtkSourceGutterRendererPixbuf *renderer,
                                                                              const gchar                   *icon_name);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar             *gtk_source_gutter_renderer_pixbuf_get_icon_name     (GtkSourceGutterRendererPixbuf *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
GdkPaintable            *gtk_source_gutter_renderer_pixbuf_get_paintable     (GtkSourceGutterRendererPixbuf *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_pixbuf_set_paintable     (GtkSourceGutterRendererPixbuf *renderer,
                                                                              GdkPaintable                  *paintable);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_gutter_renderer_pixbuf_overlay_paintable (GtkSourceGutterRendererPixbuf *renderer,
                                                                              GdkPaintable                  *paintable);

G_END_DECLS

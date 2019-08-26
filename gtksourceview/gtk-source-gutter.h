/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
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

#ifndef GTK_SOURCE_GUTTER_H
#define GTK_SOURCE_GUTTER_H

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <gtksourceview/gtk-source-types.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_GUTTER			(gtk_source_gutter_get_type ())
#define GTK_SOURCE_GUTTER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_GUTTER, GtkSourceGutter))
#define GTK_SOURCE_GUTTER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_GUTTER, GtkSourceGutterClass))
#define GTK_SOURCE_IS_GUTTER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_GUTTER))
#define GTK_SOURCE_IS_GUTTER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_GUTTER))
#define GTK_SOURCE_GUTTER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_SOURCE_TYPE_GUTTER, GtkSourceGutterClass))

typedef struct _GtkSourceGutterClass	GtkSourceGutterClass;
typedef struct _GtkSourceGutterPrivate	GtkSourceGutterPrivate;

struct _GtkSourceGutter
{
	GObject parent;

	GtkSourceGutterPrivate *priv;
};

struct _GtkSourceGutterClass
{
	GObjectClass parent_class;

	gpointer padding[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GType gtk_source_gutter_get_type 		(void) G_GNUC_CONST;

GTK_SOURCE_AVAILABLE_IN_3_24
GtkSourceView *
     gtk_source_gutter_get_view			(GtkSourceGutter         *gutter);

GTK_SOURCE_AVAILABLE_IN_3_24
GtkTextWindowType
     gtk_source_gutter_get_window_type		(GtkSourceGutter         *gutter);

GTK_SOURCE_AVAILABLE_IN_ALL
gboolean gtk_source_gutter_insert               (GtkSourceGutter         *gutter,
                                                 GtkSourceGutterRenderer *renderer,
                                                 gint                     position);

GTK_SOURCE_AVAILABLE_IN_ALL
void gtk_source_gutter_reorder			(GtkSourceGutter	 *gutter,
                                                 GtkSourceGutterRenderer *renderer,
                                                 gint                     position);

GTK_SOURCE_AVAILABLE_IN_ALL
void gtk_source_gutter_remove			(GtkSourceGutter         *gutter,
                                                 GtkSourceGutterRenderer *renderer);

GTK_SOURCE_AVAILABLE_IN_ALL
void gtk_source_gutter_queue_draw		(GtkSourceGutter         *gutter);

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceGutterRenderer *
     gtk_source_gutter_get_renderer_at_pos      (GtkSourceGutter         *gutter,
                                                 gint                     x,
                                                 gint                     y);

G_END_DECLS

#endif /* GTK_SOURCE_GUTTER_H */

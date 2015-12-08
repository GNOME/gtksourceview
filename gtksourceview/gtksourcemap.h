/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcemap.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Ignacio Casal Quinteiro <icq@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SOURCE_MAP_H__
#define __GTK_SOURCE_MAP_H__

#include <gtk/gtk.h>
#include <gtksourceview/gtksourceview.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_MAP            (gtk_source_map_get_type())
#define GTK_SOURCE_MAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_MAP, GtkSourceMap))
#define GTK_SOURCE_MAP_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_MAP, GtkSourceMap const))
#define GTK_SOURCE_MAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GTK_SOURCE_TYPE_MAP, GtkSourceMapClass))
#define GTK_SOURCE_IS_MAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_MAP))
#define GTK_SOURCE_IS_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GTK_SOURCE_TYPE_MAP))
#define GTK_SOURCE_MAP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GTK_SOURCE_TYPE_MAP, GtkSourceMapClass))

typedef struct _GtkSourceMap        GtkSourceMap;
typedef struct _GtkSourceMapClass   GtkSourceMapClass;

struct _GtkSourceMap
{
	GtkSourceView parent_instance;
};

struct _GtkSourceMapClass
{
	GtkSourceViewClass parent_class;

	gpointer padding[10];
};

GTK_SOURCE_AVAILABLE_IN_3_18
GType			 gtk_source_map_get_type	(void);

GTK_SOURCE_AVAILABLE_IN_3_18
GtkWidget		*gtk_source_map_new		(void);

GTK_SOURCE_AVAILABLE_IN_3_18
void			 gtk_source_map_set_view	(GtkSourceMap  *map,
							 GtkSourceView *view);

GTK_SOURCE_AVAILABLE_IN_3_18
GtkSourceView		*gtk_source_map_get_view	(GtkSourceMap  *map);

G_END_DECLS

#endif /* __GTK_SOURCE_MAP_H__ */

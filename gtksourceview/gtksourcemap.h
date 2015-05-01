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

#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_MAP (gtk_source_map_get_type ())
G_DECLARE_DERIVABLE_TYPE (GtkSourceMap, gtk_source_map, GTK_SOURCE, MAP, GtkBin)

struct _GtkSourceMapClass
{
	GtkBinClass parent_class;

	gpointer padding[10];
};

GtkWidget		*gtk_source_map_new		(void);

void			 gtk_source_map_set_view	(GtkSourceMap  *map,
                                                         GtkSourceView *view);

GtkSourceView		*gtk_source_map_get_view	(GtkSourceMap  *map);

GtkSourceView		*gtk_source_map_get_child_view	(GtkSourceMap  *map);

G_END_DECLS

#endif /* __GTK_SOURCE_MAP_H__ */

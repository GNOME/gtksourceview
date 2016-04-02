/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * gtksourceregion.h - GtkTextMark based region utility functions
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2002 Gustavo Giráldez <gustavo.giraldez@gmx.net>
 * Copyright (C) 2016 Sébastien Wilmet <swilmet@gnome.org>
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GTK_SOURCE_REGION_H__
#define __GTK_SOURCE_REGION_H__

#include <gtk/gtk.h>
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

typedef struct _GtkSourceRegion		GtkSourceRegion;
typedef struct _GtkSourceRegionIter	GtkSourceRegionIter;

struct _GtkSourceRegionIter
{
	/* GtkSourceRegionIter is an opaque datatype; ignore all these fields.
	 * Initialize the iter with gtk_source_region_get_start_region_iter
	 * function
	 */
	/*< private >*/
	gpointer dummy1;
	guint32  dummy2;
	gpointer dummy3;
};

GTK_SOURCE_INTERNAL
GtkSourceRegion *	gtk_source_region_new			(GtkTextBuffer *buffer);

GTK_SOURCE_INTERNAL
void			gtk_source_region_destroy		(GtkSourceRegion *region);

GTK_SOURCE_INTERNAL
GtkTextBuffer *		gtk_source_region_get_buffer		(GtkSourceRegion *region);

GTK_SOURCE_INTERNAL
void			gtk_source_region_add			(GtkSourceRegion   *region,
								 const GtkTextIter *_start,
								 const GtkTextIter *_end);

GTK_SOURCE_INTERNAL
void			gtk_source_region_subtract		(GtkSourceRegion   *region,
								 const GtkTextIter *_start,
								 const GtkTextIter *_end);

GTK_SOURCE_INTERNAL
GtkSourceRegion *	gtk_source_region_intersect		(GtkSourceRegion   *region,
								 const GtkTextIter *_start,
								 const GtkTextIter *_end);

GTK_SOURCE_INTERNAL
guint			gtk_source_region_get_subregion_count	(GtkSourceRegion *region);

GTK_SOURCE_INTERNAL
gboolean		gtk_source_region_nth_subregion		(GtkSourceRegion *region,
								 guint            subregion,
								 GtkTextIter     *start,
								 GtkTextIter     *end);

GTK_SOURCE_INTERNAL
void			gtk_source_region_get_start_region_iter	(GtkSourceRegion     *region,
								 GtkSourceRegionIter *iter);

GTK_SOURCE_INTERNAL
gboolean		gtk_source_region_iter_is_end		(GtkSourceRegionIter *iter);

/* Returns FALSE if iterator is the end iterator */
GTK_SOURCE_INTERNAL
gboolean		gtk_source_region_iter_next		(GtkSourceRegionIter *iter);

GTK_SOURCE_INTERNAL
gboolean		gtk_source_region_iter_get_subregion	(GtkSourceRegionIter *iter,
								 GtkTextIter         *start,
								 GtkTextIter         *end);

GTK_SOURCE_INTERNAL
void			_gtk_source_region_debug_print		(GtkSourceRegion *region);

G_END_DECLS

#endif /* __GTK_SOURCE_REGION_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcemarker.h
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_MARKER_H__
#define __GTK_SOURCE_MARKER_H__

#include <gtk/gtktextmark.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_MARKER            (gtk_source_marker_get_type ())
#define GTK_SOURCE_MARKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_MARKER, GtkSourceMarker))
#define GTK_SOURCE_MARKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_MARKER, GtkSourceMarkerClass))
#define GTK_IS_SOURCE_MARKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_MARKER))
#define GTK_IS_SOURCE_MARKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_MARKER))
#define GTK_SOURCE_MARKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_MARKER, GtkSourceMarkerClass))

typedef GtkTextMark      GtkSourceMarker;
typedef GtkTextMarkClass GtkSourceMarkerClass;

GType                 gtk_source_marker_get_type        (void) G_GNUC_CONST;

void                  gtk_source_marker_set_marker_type (GtkSourceMarker *marker,
							 const gchar     *type);
gchar                *gtk_source_marker_get_marker_type (GtkSourceMarker *marker);
gint                  gtk_source_marker_get_line        (GtkSourceMarker *marker);
G_CONST_RETURN gchar *gtk_source_marker_get_name        (GtkSourceMarker *marker);
GtkSourceBuffer      *gtk_source_marker_get_buffer      (GtkSourceMarker *marker);
GtkSourceMarker      *gtk_source_marker_next            (GtkSourceMarker *marker);
GtkSourceMarker      *gtk_source_marker_prev            (GtkSourceMarker *marker);

/* Private API */
void                  _gtk_source_marker_changed        (GtkSourceMarker *marker);
void                  _gtk_source_marker_link           (GtkSourceMarker *marker,
							 GtkSourceMarker *sibling,
							 gboolean         after_sibling);
void                  _gtk_source_marker_unlink         (GtkSourceMarker *marker);

G_END_DECLS

#endif

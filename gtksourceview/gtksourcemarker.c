/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcemarker.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourcebuffer.h"
#include "gtksourcemarker.h"

static GQuark quark_marker_type = 0;
static GQuark quark_next_marker = 0;
static GQuark quark_prev_marker = 0;


GType 
gtk_source_marker_get_type (void)
{
	static GType our_type = 0;

	/* this function's solely purpose is to allow us to later
	 * derive GtkSourceMarker from other base object and not break
	 * ABI compatibility (since this way the type is resolved at
	 * runtime) */
	if (!our_type) {
		our_type = GTK_TYPE_TEXT_MARK;

		quark_marker_type = g_quark_from_static_string ("gtk-source-marker-type");
		quark_next_marker = g_quark_from_static_string ("gtk-source-marker-next");
		quark_prev_marker = g_quark_from_static_string ("gtk-source-marker-prev");
	}
	
	return our_type;
}

void 
gtk_source_marker_set_marker_type (GtkSourceMarker *marker,
				   const gchar     *type)
{
	g_return_if_fail (marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	
	g_object_set_qdata_full (G_OBJECT (marker), quark_marker_type,
				 g_strdup (type), (GDestroyNotify) g_free);
	_gtk_source_marker_changed (marker);
}

/**
 * gtk_source_marker_get_marker_type:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the marker type of this @marker.
 * 
 * Return value: the marker type.
 **/
gchar *
gtk_source_marker_get_marker_type (GtkSourceMarker *marker)
{
	g_return_val_if_fail (marker != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), NULL);
	
	return g_strdup (g_object_get_qdata (G_OBJECT (marker), quark_marker_type));
}

/**
 * gtk_source_marker_get_line:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the line number of this @marker.
 * 
 * Return value: the line number.
 **/
gint
gtk_source_marker_get_line (GtkSourceMarker *marker)
{
	GtkTextIter iter;
	
	g_return_val_if_fail (marker != NULL, -1);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), -1);
	g_return_val_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)), -1);

	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (GTK_TEXT_MARK (marker)),
					  &iter, GTK_TEXT_MARK (marker));

	return gtk_text_iter_get_line (&iter);
}

/**
 * gtk_source_marker_get_name:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the name of this @marker.
 * 
 * Return value: the name.
 **/
G_CONST_RETURN gchar *
gtk_source_marker_get_name (GtkSourceMarker *marker)
{
	g_return_val_if_fail (marker != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), NULL);
	
	return gtk_text_mark_get_name (GTK_TEXT_MARK (marker));
}
 
/**
 * gtk_source_marker_get_buffer:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the buffer associated with this @marker.
 * 
 * Return value: the #GtkSourceBuffer.
 **/
GtkSourceBuffer *
gtk_source_marker_get_buffer (GtkSourceMarker *marker)
{
	GtkTextBuffer *buffer;

	g_return_val_if_fail (marker != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), NULL);
	
	buffer = gtk_text_mark_get_buffer (GTK_TEXT_MARK (marker));

	if (GTK_IS_SOURCE_BUFFER (buffer))
		return GTK_SOURCE_BUFFER (buffer);
	else
		return NULL;
}

void
_gtk_source_marker_changed (GtkSourceMarker *marker)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;

	g_return_if_fail (marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));

	buffer = GTK_SOURCE_BUFFER (gtk_text_mark_get_buffer (GTK_TEXT_MARK (marker)));
	
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &iter,
					  GTK_TEXT_MARK (marker));
	gtk_text_iter_set_line_offset (&iter, 0);

	g_signal_emit_by_name (buffer, "marker_updated", &iter);
}

void
_gtk_source_marker_link (GtkSourceMarker *marker,
			 GtkSourceMarker *sibling,
			 gboolean         after_sibling)
{
	GtkSourceMarker *tmp;
	
	g_return_if_fail (marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));

	if (!sibling)
		return;
	
	g_return_if_fail (GTK_IS_SOURCE_MARKER (sibling));

	if (after_sibling)
	{
		tmp = g_object_get_qdata (G_OBJECT (sibling), quark_next_marker);

		g_object_set_qdata (G_OBJECT (marker), quark_next_marker, tmp);
		g_object_set_qdata (G_OBJECT (marker), quark_prev_marker, sibling);

		g_object_set_qdata (G_OBJECT (sibling), quark_next_marker, marker);
		if (tmp)
			g_object_set_qdata (G_OBJECT (tmp), quark_prev_marker, marker);
	}
	else
	{
		tmp = g_object_get_qdata (G_OBJECT (sibling), quark_prev_marker);

		g_object_set_qdata (G_OBJECT (marker), quark_next_marker, sibling);
		g_object_set_qdata (G_OBJECT (marker), quark_prev_marker, tmp);

		g_object_set_qdata (G_OBJECT (sibling), quark_prev_marker, marker);
		if (tmp)
			g_object_set_qdata (G_OBJECT (tmp), quark_next_marker, marker);
	}
}

void
_gtk_source_marker_unlink (GtkSourceMarker *marker)
{
	GtkSourceMarker *m1, *m2;
	
	g_return_if_fail (marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));

	m1 = g_object_steal_qdata (G_OBJECT (marker), quark_prev_marker);
	m2 = g_object_steal_qdata (G_OBJECT (marker), quark_next_marker);

	if (m1)
		g_object_set_qdata (G_OBJECT (m1), quark_next_marker, m2);
	if (m2)
		g_object_set_qdata (G_OBJECT (m2), quark_prev_marker, m1);
}

/**
 * gtk_source_marker_next:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the next marker after @marker.
 * 
 * Return value: a #GtkSourceMarker.
 **/
GtkSourceMarker *
gtk_source_marker_next (GtkSourceMarker *marker)
{
	g_return_val_if_fail (marker != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), NULL);
	
	return g_object_get_qdata (G_OBJECT (marker), quark_next_marker);
}

/**
 * gtk_source_marker_prev:
 * @marker: a #GtkSourceMarker.
 * 
 * Gets the previous marker before @marker.
 * 
 * Return value: a #GtkSourceMarker.
 **/
GtkSourceMarker *
gtk_source_marker_prev (GtkSourceMarker *marker)
{
	g_return_val_if_fail (marker != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARKER (marker), NULL);
	
	return g_object_get_qdata (G_OBJECT (marker), quark_prev_marker);
}


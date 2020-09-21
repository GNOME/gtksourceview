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

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_GUTTER_RENDERER (gtk_source_gutter_renderer_get_type())

/**
 * GtkSourceGutterRendererAlignmentMode:
 * @GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL: The full cell.
 * @GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST: The first line.
 * @GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST: The last line.
 *
 * The alignment mode of the renderer, when a cell spans multiple lines (due to
 * text wrapping).
 **/
typedef enum _GtkSourceGutterRendererAlignmentMode
{
	GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL,
	GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST,
	GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST,
} GtkSourceGutterRendererAlignmentMode;

struct _GtkSourceGutterRendererClass
{
	GtkWidgetClass parent_class;

	void     (*query_data)            (GtkSourceGutterRenderer      *renderer,
	                                   GtkSourceGutterLines         *lines,
	                                   guint                         line);
	void     (*begin)                 (GtkSourceGutterRenderer      *renderer,
	                                   GtkSourceGutterLines         *lines);
	void     (*snapshot_line)         (GtkSourceGutterRenderer      *renderer,
	                                   GtkSnapshot                  *snapshot,
	                                   GtkSourceGutterLines         *lines,
	                                   guint                         line);
	void     (*end)                   (GtkSourceGutterRenderer      *renderer);
	/**
	 * GtkSourceGutterRendererClass::change_view:
	 * @renderer: a #GtkSourceGutterRenderer.
	 * @old_view: (nullable): the old #GtkTextView.
	 *
	 * This is called when the text view changes for @renderer.
	 */
	void     (*change_view)           (GtkSourceGutterRenderer      *renderer,
	                                   GtkSourceView                *old_view);
	/**
	 * GtkSourceGutterRendererClass::change_buffer:
	 * @renderer: a #GtkSourceGutterRenderer.
	 * @old_buffer: (nullable): the old #GtkTextBuffer.
	 *
	 * This is called when the text buffer changes for @renderer.
	 */
	void     (*change_buffer)         (GtkSourceGutterRenderer      *renderer,
	                                   GtkSourceBuffer              *old_buffer);
	/* Signal handlers */
	gboolean (*query_activatable)     (GtkSourceGutterRenderer      *renderer,
	                                   GtkTextIter                  *iter,
	                                   GdkRectangle                 *area);
	void     (*activate)              (GtkSourceGutterRenderer      *renderer,
	                                   GtkTextIter                  *iter,
	                                   GdkRectangle                 *area,
	                                   guint                         button,
	                                   GdkModifierType               state,
	                                   gint                          n_presses);

	/*< private >*/
	gpointer _reserved[20];
};

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceGutterRenderer, gtk_source_gutter_renderer, GTK_SOURCE, GUTTER_RENDERER, GtkWidget)

GTK_SOURCE_AVAILABLE_IN_ALL
gfloat                                gtk_source_gutter_renderer_get_xalign         (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_set_xalign         (GtkSourceGutterRenderer              *renderer,
                                                                                     gfloat                                xalign);
GTK_SOURCE_AVAILABLE_IN_ALL
gfloat                                gtk_source_gutter_renderer_get_yalign         (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_set_yalign         (GtkSourceGutterRenderer              *renderer,
                                                                                     gfloat                                yalign);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                                  gtk_source_gutter_renderer_get_xpad           (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_set_xpad           (GtkSourceGutterRenderer              *renderer,
                                                                                     gint                                  xpad);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                                  gtk_source_gutter_renderer_get_ypad           (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_set_ypad           (GtkSourceGutterRenderer              *renderer,
                                                                                     gint                                  ypad);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceGutterRendererAlignmentMode  gtk_source_gutter_renderer_get_alignment_mode (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_set_alignment_mode (GtkSourceGutterRenderer              *renderer,
                                                                                     GtkSourceGutterRendererAlignmentMode  mode);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceBuffer                      *gtk_source_gutter_renderer_get_buffer         (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceView                        *gtk_source_gutter_renderer_get_view           (GtkSourceGutterRenderer              *renderer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_activate           (GtkSourceGutterRenderer              *renderer,
                                                                                     const GtkTextIter                    *iter,
                                                                                     const GdkRectangle                   *area,
                                                                                     guint                                 button,
                                                                                     GdkModifierType                       state,
                                                                                     gint                                  n_presses);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                              gtk_source_gutter_renderer_query_activatable  (GtkSourceGutterRenderer              *renderer,
                                                                                     const GtkTextIter                    *iter,
                                                                                     const GdkRectangle                   *area);
GTK_SOURCE_AVAILABLE_IN_ALL
void                                  gtk_source_gutter_renderer_align_cell         (GtkSourceGutterRenderer              *renderer,
                                                                                     guint                                 line,
                                                                                     gfloat                                width,
                                                                                     gfloat                                height,
                                                                                     gfloat                               *x,
                                                                                     gfloat                               *y);

G_END_DECLS

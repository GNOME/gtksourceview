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

#include "config.h"

#include "gtksourcegutterrenderermarks-private.h"
#include "gtksourceview.h"
#include "gtksourcebuffer.h"
#include "gtksourcemarkattributes.h"
#include "gtksourcemark.h"

#define COMPOSITE_ALPHA 225

struct _GtkSourceGutterRendererMarks
{
	GtkSourceGutterRendererPixbuf parent_instance;
};

G_DEFINE_TYPE (GtkSourceGutterRendererMarks, gtk_source_gutter_renderer_marks, GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF)

static gint
sort_marks_by_priority (gconstpointer m1,
                        gconstpointer m2,
                        gpointer      data)
{
	GtkSourceMark *mark1 = (GtkSourceMark *)m1;
	GtkSourceMark *mark2 = (GtkSourceMark *)m2;
	GtkSourceView *view = data;
	GtkTextIter iter1, iter2;
	gint line1;
	gint line2;

	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (GTK_TEXT_MARK (mark1)),
	                                  &iter1,
	                                  GTK_TEXT_MARK (mark1));

	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (GTK_TEXT_MARK (mark2)),
	                                  &iter2,
	                                  GTK_TEXT_MARK (mark2));

	line1 = gtk_text_iter_get_line (&iter1);
	line2 = gtk_text_iter_get_line (&iter2);

	if (line1 == line2)
	{
		gint priority1 = -1;
		gint priority2 = -1;

		gtk_source_view_get_mark_attributes (view,
		                                     gtk_source_mark_get_category (mark1),
		                                     &priority1);

		gtk_source_view_get_mark_attributes (view,
		                                     gtk_source_mark_get_category (mark2),
		                                     &priority2);

		return priority1 - priority2;
	}
	else
	{
		return line2 - line1;
	}
}

static int
measure_line_height (GtkSourceView *view)
{
	PangoLayout *layout;
	gint height = 12;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), "QWERTY");

	if (layout)
	{
		pango_layout_get_pixel_size (layout, NULL, &height);
		g_object_unref (layout);
	}

	return height - 2;
}

static void
composite_marks (GtkSourceView                 *view,
                 GtkSourceGutterRendererPixbuf *renderer,
                 GSList                        *marks,
                 gint                           size)
{
	/* Draw the mark with higher priority */
	marks = g_slist_sort_with_data (marks, sort_marks_by_priority, view);

	gtk_source_gutter_renderer_pixbuf_set_paintable (renderer, NULL);

	/* composite all the pixbufs for the marks present at the line */
	do
	{
		GtkSourceMark *mark;
		GtkSourceMarkAttributes *attrs;
		GdkPaintable *paintable;

		mark = marks->data;
		attrs = gtk_source_view_get_mark_attributes (view,
		                                             gtk_source_mark_get_category (mark),
		                                             NULL);

		if (attrs != NULL)
		{
			paintable = gtk_source_mark_attributes_render_icon (attrs,
			                                                    GTK_WIDGET (view),
			                                                    size);

			if (paintable != NULL)
			{
				gtk_source_gutter_renderer_pixbuf_overlay_paintable (renderer, paintable);
			}
		}

		marks = g_slist_next (marks);
	}
	while (marks);
}

static void
gutter_renderer_query_data (GtkSourceGutterRenderer *renderer,
                            GtkSourceGutterLines    *lines,
                            guint                    line)
{
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter;
	GSList *marks;

	view = GTK_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (renderer));
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, line);
	marks = gtk_source_buffer_get_source_marks_at_iter (buffer, &iter, NULL);

	if (marks != NULL)
	{
		gint size = measure_line_height (view);
		composite_marks (view, GTK_SOURCE_GUTTER_RENDERER_PIXBUF (renderer), marks, size);
		g_object_set (G_OBJECT (renderer),
		              "xpad", 2,
		              "yalign", 0.5,
		              "xalign", 0.5,
		              "alignment-mode", GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST,
		              NULL);
		g_slist_free (marks);
	}
	else
	{
		gtk_source_gutter_renderer_pixbuf_set_paintable (GTK_SOURCE_GUTTER_RENDERER_PIXBUF (renderer),
		                                                 NULL);
	}
}

static gboolean
set_tooltip_widget_from_marks (GtkSourceView *view,
                               GtkTooltip    *tooltip,
                               GSList        *marks)
{
	const gint icon_size = 16;
	GtkGrid *grid = NULL;
	gint row_num = 0;

	for (; marks; marks = g_slist_next (marks))
	{
		const gchar *category;
		GtkSourceMark *mark;
		GtkSourceMarkAttributes *attrs;
		gchar *text;
		gboolean ismarkup = FALSE;
		GtkWidget *label;
		GdkPaintable *paintable;

		mark = marks->data;
		category = gtk_source_mark_get_category (mark);

		attrs = gtk_source_view_get_mark_attributes (view, category, NULL);

		if (attrs == NULL)
		{
			continue;
		}

		text = gtk_source_mark_attributes_get_tooltip_markup (attrs, mark);

		if (text == NULL)
		{
			text = gtk_source_mark_attributes_get_tooltip_text (attrs, mark);
		}
		else
		{
			ismarkup = TRUE;
		}

		if (text == NULL)
		{
			continue;
		}

		if (grid == NULL)
		{
			grid = GTK_GRID (gtk_grid_new ());
			gtk_grid_set_column_spacing (grid, 4);
			gtk_widget_set_visible (GTK_WIDGET (grid), TRUE);
		}

		label = gtk_label_new (NULL);

		if (ismarkup)
		{
			gtk_label_set_markup (GTK_LABEL (label), text);
		}
		else
		{
			gtk_label_set_text (GTK_LABEL (label), text);
		}

		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_widget_set_valign (label, GTK_ALIGN_START);
		gtk_widget_set_visible (label, TRUE);

		paintable = gtk_source_mark_attributes_render_icon (attrs,
		                                                    GTK_WIDGET (view),
		                                                    icon_size);

		if (paintable == NULL)
		{
			gtk_grid_attach (grid, label, 0, row_num, 2, 1);
		}
		else
		{
			GtkWidget *image;

			image = gtk_image_new_from_paintable (paintable);

			gtk_widget_set_halign (image, GTK_ALIGN_START);
			gtk_widget_set_valign (image, GTK_ALIGN_START);
			gtk_widget_set_visible (image, TRUE);

			gtk_grid_attach (grid, image, 0, row_num, 1, 1);
			gtk_grid_attach (grid, label, 1, row_num, 1, 1);
		}

		row_num++;

		if (marks->next != NULL)
		{
			GtkWidget *separator;

			separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);

			gtk_widget_set_visible (separator, TRUE);

			gtk_grid_attach (grid, separator, 0, row_num, 2, 1);
			row_num++;
		}

		g_free (text);
	}

	if (grid == NULL)
	{
		return FALSE;
	}

	gtk_tooltip_set_custom (tooltip, GTK_WIDGET (grid));

	return TRUE;
}

static gboolean
gutter_renderer_query_tooltip (GtkWidget    *widget,
                               gint          x,
                               gint          y,
                               gboolean      keyboard,
                               GtkTooltip   *tooltip)
{
	GtkSourceGutterRenderer *renderer;
	GSList *marks;
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	gboolean ret;

	renderer = GTK_SOURCE_GUTTER_RENDERER (widget);
	view = GTK_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (renderer));
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, 0, y);

	marks = gtk_source_buffer_get_source_marks_at_iter (buffer, &iter, NULL);

	if (marks != NULL)
	{
		marks = g_slist_sort_with_data (marks,
		                                sort_marks_by_priority,
		                                view);

		marks = g_slist_reverse (marks);

		ret = set_tooltip_widget_from_marks (view, tooltip, marks);

		g_slist_free (marks);

		return ret;
	}

	return FALSE;
}

static gboolean
gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   GdkRectangle            *area)
{
	return TRUE;
}

static void
gutter_renderer_change_view (GtkSourceGutterRenderer *renderer,
			     GtkSourceView           *old_view)
{
	GtkSourceView *view;

	view = gtk_source_gutter_renderer_get_view (renderer);

	if (view != NULL)
	{
		gtk_widget_set_size_request (GTK_WIDGET (renderer), 
		                             measure_line_height (view),
		                             -1);
	}

	if (GTK_SOURCE_GUTTER_RENDERER_CLASS (gtk_source_gutter_renderer_marks_parent_class)->change_view != NULL)
	{
		GTK_SOURCE_GUTTER_RENDERER_CLASS (gtk_source_gutter_renderer_marks_parent_class)->change_view (renderer,
													       old_view);
	}
}

static void
gtk_source_gutter_renderer_marks_class_init (GtkSourceGutterRendererMarksClass *klass)
{
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->query_tooltip = gutter_renderer_query_tooltip;

	renderer_class->query_data = gutter_renderer_query_data;
	renderer_class->query_activatable = gutter_renderer_query_activatable;
	renderer_class->change_view = gutter_renderer_change_view;
}

static void
gtk_source_gutter_renderer_marks_init (GtkSourceGutterRendererMarks *self)
{
}

GtkSourceGutterRenderer *
gtk_source_gutter_renderer_marks_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_GUTTER_RENDERER_MARKS, NULL);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
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

#include "gtksourcegutterrendererlines-private.h"
#include "gtksourcegutterlines.h"
#include "gtksourceutils-private.h"
#include "gtksourceview.h"

struct _GtkSourceGutterRendererLines
{
	GtkSourceGutterRendererText parent_instance;
	gint num_line_digits;
	gint prev_line_num;
	guint cursor_visible : 1;
};

G_DEFINE_TYPE (GtkSourceGutterRendererLines, _gtk_source_gutter_renderer_lines, GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT)

static inline gint
count_num_digits (gint num_lines)
{
	if (num_lines < 100)
	{
		return 2;
	}
	else if (num_lines < 1000)
	{
		return 3;
	}
	else if (num_lines < 10000)
	{
		return 4;
	}
	else if (num_lines < 100000)
	{
		return 5;
	}
	else if (num_lines < 1000000)
	{
		return 6;
	}
	else
	{
		return 10;
	}
}

static void
recalculate_size (GtkSourceGutterRendererLines *renderer)
{
	GtkSourceBuffer *buffer;
	gint num_lines;
	gint num_digits;

	buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));
	num_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	num_digits = count_num_digits (num_lines);

	if (num_digits != renderer->num_line_digits)
	{
		renderer->num_line_digits = num_digits;
		gtk_widget_queue_resize (GTK_WIDGET (renderer));
	}
}

static void
on_buffer_changed (GtkSourceBuffer              *buffer,
                   GtkSourceGutterRendererLines *renderer)
{
	recalculate_size (renderer);
}

static void
on_buffer_cursor_moved (GtkSourceBuffer              *buffer,
                        GtkSourceGutterRendererLines *renderer)
{
	if (renderer->cursor_visible)
	{
		/* Redraw if the current-line needs updating */
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

static void
gutter_renderer_change_buffer (GtkSourceGutterRenderer *renderer,
                               GtkSourceBuffer         *old_buffer)
{
	GtkSourceGutterRendererLines *lines = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);
	GtkSourceBuffer *buffer;

	if (old_buffer != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_buffer,
						      on_buffer_changed,
						      lines);
		g_signal_handlers_disconnect_by_func (old_buffer,
						      on_buffer_cursor_moved,
						      lines);
	}

	buffer = gtk_source_gutter_renderer_get_buffer (renderer);

	lines->prev_line_num = 0;

	if (buffer != NULL)
	{
		g_signal_connect_object (buffer,
					 "changed",
					 G_CALLBACK (on_buffer_changed),
					 lines,
					 0);

		g_signal_connect_object (buffer,
					 "cursor-moved",
					 G_CALLBACK (on_buffer_cursor_moved),
					 lines,
					 0);

		recalculate_size (lines);
	}

	GTK_SOURCE_GUTTER_RENDERER_CLASS (_gtk_source_gutter_renderer_lines_parent_class)->change_buffer (renderer, old_buffer);
}

static void
on_view_style_updated (GtkTextView                  *view,
                       GtkSourceGutterRendererLines *renderer)
{
	/* Force to recalculate the size. */
	renderer->num_line_digits = -1;
	recalculate_size (renderer);
}

static void
on_view_notify_cursor_visible (GtkTextView                  *view,
                               GParamSpec                   *pspec,
                               GtkSourceGutterRendererLines *renderer)
{
	renderer->cursor_visible = gtk_text_view_get_cursor_visible (view);
}

static void
gutter_renderer_change_view (GtkSourceGutterRenderer *renderer,
			     GtkSourceView             *old_view)
{
	GtkSourceView *new_view;

	if (old_view != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_view,
						      on_view_style_updated,
						      renderer);
		g_signal_handlers_disconnect_by_func (old_view,
						      on_view_notify_cursor_visible,
						      renderer);
	}

	new_view = gtk_source_gutter_renderer_get_view (renderer);

	if (new_view != NULL)
	{
		g_signal_connect_object (new_view,
					 "style-updated",
					 G_CALLBACK (on_view_style_updated),
					 renderer,
					 0);

		g_signal_connect_object (new_view,
					 "notify::cursor-visible",
					 G_CALLBACK (on_view_notify_cursor_visible),
					 renderer,
					 0);

		GTK_SOURCE_GUTTER_RENDERER_LINES (renderer)->cursor_visible = gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (new_view));
	}

	GTK_SOURCE_GUTTER_RENDERER_CLASS (_gtk_source_gutter_renderer_lines_parent_class)->change_view (renderer, old_view);
}

static void
extend_selection_to_line (GtkSourceGutterRendererLines *renderer,
                          GtkTextIter                  *line_start)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter line_end;
	GtkSourceBuffer *buffer;

	buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));

	gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer),
	                                      &start,
	                                      &end);

	line_end = *line_start;

	if (!gtk_text_iter_ends_line (&line_end))
	{
		gtk_text_iter_forward_to_line_end (&line_end);
	}

	if (gtk_text_iter_compare (&start, line_start) < 0)
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
		                              &start,
		                              &line_end);
	}
	else if (gtk_text_iter_compare (&end, &line_end) < 0)
	{
		/* if the selection is in this line, extend
		 * the selection to the whole line */
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
		                              &line_end,
		                              line_start);
	}
	else
	{
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer),
		                              &end,
		                              line_start);
	}
}

static void
select_line (GtkSourceGutterRendererLines *renderer,
             GtkTextIter                  *line_start)
{
	GtkTextIter iter;
	GtkSourceBuffer *buffer;

	buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));

	iter = *line_start;

	if (!gtk_text_iter_ends_line (&iter))
	{
		gtk_text_iter_forward_to_line_end (&iter);
	}

	/* Select the line, put the cursor at the end of the line */
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, line_start);
}

static void
gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                          GtkTextIter             *iter,
                          GdkRectangle            *rect,
                          guint                    button,
                          GdkModifierType          state,
                          gint                     n_presses)
{
	GtkSourceGutterRendererLines *lines;
	GtkSourceBuffer *buffer;

	lines = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);

	if (button != 1)
	{
		return;
	}

	buffer = gtk_source_gutter_renderer_get_buffer (renderer);

	if (n_presses == 1)
	{
		if ((state & GDK_CONTROL_MASK) != 0)
		{
			/* Single click + Ctrl -> select the line */
			select_line (lines, iter);
		}
		else if ((state & GDK_SHIFT_MASK) != 0)
		{
			/* Single click + Shift -> extended current
			   selection to include the clicked line */
			extend_selection_to_line (lines, iter);
		}
		else
		{
			gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (buffer), iter);
		}
	}
	else if (n_presses == 2)
	{
		select_line (lines, iter);
	}
}

static gboolean
gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   GdkRectangle            *area)
{
	return gtk_source_gutter_renderer_get_buffer (renderer) != NULL;
}

static void
gtk_source_gutter_renderer_lines_measure (GtkWidget      *widget,
                                          GtkOrientation  orientation,
                                          int             for_size,
                                          int            *minimum,
                                          int            *natural,
                                          int            *minimum_baseline,
                                          int            *natural_baseline)
{
	GtkSourceGutterRendererLines *renderer = GTK_SOURCE_GUTTER_RENDERER_LINES (widget);

	if (orientation == GTK_ORIENTATION_VERTICAL)
	{
		*minimum = 0;
		*natural = 0;
	}
	else
	{
		GtkSourceBuffer *buffer;
		gchar markup[32];
		guint num_lines;
		gint size;
		gint xpad;

		buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));
		num_lines = MAX (99, gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer)));

		g_snprintf (markup, sizeof markup, "<b>%u</b>", num_lines);
		gtk_source_gutter_renderer_text_measure_markup (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer),
								markup,
								&size,
								NULL);

		xpad = gtk_source_gutter_renderer_get_xpad (GTK_SOURCE_GUTTER_RENDERER (renderer));

		*natural = *minimum = size + xpad * 2;
	}

	*minimum_baseline = -1;
	*natural_baseline = -1;
}

static void
gtk_source_gutter_renderer_lines_query_data (GtkSourceGutterRenderer *renderer,
                                             GtkSourceGutterLines    *lines,
                                             guint                    line)
{
	GtkSourceGutterRendererLines *self = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);
	gint len;

	if G_UNLIKELY (self->cursor_visible && gtk_source_gutter_lines_is_cursor (lines, line))
	{
		gchar text[32];

		len = g_snprintf (text, sizeof text, "<b>%d</b>", line + 1);
		gtk_source_gutter_renderer_text_set_markup (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer), text, len);
	}
	else
	{
		const gchar *text;

		len = _gtk_source_utils_int_to_string (line + 1, &text);
		gtk_source_gutter_renderer_text_set_text (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer), text, len);
	}
}

static void
_gtk_source_gutter_renderer_lines_class_init (GtkSourceGutterRendererLinesClass *klass)
{
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->measure = gtk_source_gutter_renderer_lines_measure;

	renderer_class->query_activatable = gutter_renderer_query_activatable;
	renderer_class->query_data = gtk_source_gutter_renderer_lines_query_data;
	renderer_class->activate = gutter_renderer_activate;
	renderer_class->change_buffer = gutter_renderer_change_buffer;
	renderer_class->change_view = gutter_renderer_change_view;
}

static void
_gtk_source_gutter_renderer_lines_init (GtkSourceGutterRendererLines *self)
{
}

GtkSourceGutterRenderer *
_gtk_source_gutter_renderer_lines_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_GUTTER_RENDERER_LINES, NULL);
}

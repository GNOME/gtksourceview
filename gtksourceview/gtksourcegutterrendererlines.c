/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcegutterrendererlines.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourcegutterrendererlines.h"
#include "gtksourceview.h"

#define GTK_SOURCE_GUTTER_RENDERER_LINES_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_GUTTER_RENDERER_LINES, GtkSourceGutterRendererLinesPrivate))

struct _GtkSourceGutterRendererLinesPrivate
{
	GtkTextBuffer *buffer;

	gint num_line_digits;

	guint changed_handler_id;
	guint buffer_notify_handler_id;
};

G_DEFINE_TYPE (GtkSourceGutterRendererLines, gtk_source_gutter_renderer_lines, GTK_TYPE_SOURCE_GUTTER_RENDERER_TEXT)

static void
gtk_source_gutter_renderer_lines_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_gutter_renderer_lines_parent_class)->finalize (object);
}

static void
recalculate_size (GtkSourceGutterRendererLines *renderer)
{
	gint num_lines;
	gint num_digits = 0;
	gint num;

	num_lines = gtk_text_buffer_get_line_count (renderer->priv->buffer);

	num = num_lines;

	while (num > 0)
	{
		num /= 10;
		++num_digits;
	}

	num_digits = MAX (num_digits, 2);

	if (num_digits != renderer->priv->num_line_digits)
	{
		gchar *markup;
		gint size;

		renderer->priv->num_line_digits = num_digits;

		num_lines = MAX (num_lines, 99);

		markup = g_strdup_printf ("<b>%d</b>", num_lines);
		gtk_source_gutter_renderer_text_measure_markup (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer),
		                                                markup,
		                                                &size,
		                                                NULL);
		g_free (markup);

		gtk_source_gutter_renderer_set_size (GTK_SOURCE_GUTTER_RENDERER (renderer),
		                                     size);
	}
}

static void
on_buffer_changed (GtkSourceBuffer              *buffer,
                   GtkSourceGutterRendererLines *renderer)
{
	recalculate_size (renderer);
}

static void
buffer_notify (GtkSourceGutterRendererLines *renderer,
               gpointer                      where_the_object_was)
{
	renderer->priv->buffer = NULL;
}

static void
set_buffer (GtkSourceGutterRendererLines *renderer,
            GtkTextBuffer                *buffer)
{
	if (renderer->priv->buffer)
	{
		g_signal_handler_disconnect (renderer->priv->buffer,
		                             renderer->priv->changed_handler_id);

		g_object_weak_unref (G_OBJECT (renderer->priv->buffer),
		                     (GWeakNotify)buffer_notify,
		                     renderer);

		renderer->priv->buffer = NULL;
	}

	if (buffer)
	{
		renderer->priv->buffer = buffer;

		renderer->priv->changed_handler_id =
			g_signal_connect (renderer->priv->buffer,
			                  "changed",
			                  G_CALLBACK (on_buffer_changed),
			                  renderer);

		g_object_weak_ref (G_OBJECT (renderer->priv->buffer),
		                   (GWeakNotify)buffer_notify,
		                   renderer);

		recalculate_size (renderer);
	}
}

static void
gutter_renderer_query_data (GtkSourceGutterRenderer      *renderer,
                            GtkTextIter                  *start,
                            GtkTextIter                  *end,
                            GtkSourceGutterRendererState  state)
{
	gchar *text;
	gint line;
	gboolean current_line;

	line = gtk_text_iter_get_line (start) + 1;

	current_line = (state & GTK_SOURCE_GUTTER_RENDERER_STATE_CURSOR) &&
	               gtk_text_view_get_cursor_visible (gtk_source_gutter_renderer_get_view (renderer));

	if (current_line)
	{
		text = g_strdup_printf ("<b>%d</b>", line);
	}
	else
	{
		text = g_strdup_printf ("%d", line);
	}

	gtk_source_gutter_renderer_text_set_markup (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer),
	                                            text,
	                                            -1);

	g_free (text);
}

static void
on_buffer_notify (GtkTextView                  *view,
                  GParamSpec                   *spec,
                  GtkSourceGutterRendererLines *renderer)
{
	set_buffer (renderer, gtk_text_view_get_buffer (view));
}

static void
gtk_source_gutter_renderer_lines_dispose (GObject *object)
{
	GtkSourceGutterRenderer *renderer;
	GtkSourceGutterRendererLines *lines;
	GtkTextView *view;

	renderer = GTK_SOURCE_GUTTER_RENDERER (object);
	lines = GTK_SOURCE_GUTTER_RENDERER_LINES (object);

	view = gtk_source_gutter_renderer_get_view (renderer);

	if (view != NULL && lines->priv->buffer_notify_handler_id != 0)
	{
		g_signal_handler_disconnect (view,
		                             lines->priv->buffer_notify_handler_id);

		lines->priv->buffer_notify_handler_id = 0;
	}

	set_buffer (lines, NULL);

	G_OBJECT_CLASS (gtk_source_gutter_renderer_lines_parent_class)->dispose (object);
}

static void
extend_selection_to_line (GtkSourceGutterRendererLines *renderer,
                          GtkTextIter                  *line_start)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter line_end;

	gtk_text_buffer_get_selection_bounds (renderer->priv->buffer, &start, &end);

	line_end = *line_start;

	if (!gtk_text_iter_ends_line (&line_end))
	{
		gtk_text_iter_forward_to_line_end (&line_end);
	}

	if (gtk_text_iter_compare (&start, line_start) < 0)
	{
		gtk_text_buffer_select_range (renderer->priv->buffer,
		                              &start,
		                              &line_end);
	}
	else if (gtk_text_iter_compare (&end, &line_end) < 0)
	{
		/* if the selection is in this line, extend
		 * the selection to the whole line */
		gtk_text_buffer_select_range (renderer->priv->buffer,
		                              &line_end,
		                              line_start);
	}
	else
	{
		gtk_text_buffer_select_range (renderer->priv->buffer,
		                              &end,
		                              line_start);
	}
}

static void
select_line (GtkSourceGutterRendererLines *renderer,
             GtkTextIter                  *line_start)
{
	GtkTextIter iter;

	iter = *line_start;

	if (!gtk_text_iter_ends_line (&iter))
	{
		gtk_text_iter_forward_to_line_end (&iter);
	}

	/* Select the line, put the cursor at the end of the line */
	gtk_text_buffer_select_range (renderer->priv->buffer, &iter, line_start);
}

static void
gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                          GtkTextIter             *iter,
                          const GdkRectangle      *rect,
                          GdkEvent                *event)
{
	GtkSourceGutterRendererLines *lines;

	lines = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);

	if (event->type == GDK_BUTTON_PRESS && (event->button.button == 1))
	{
		if ((event->button.state & GDK_CONTROL_MASK) != 0)
		{
			/* Single click + Ctrl -> select the line */
			select_line (lines, iter);
		}
		else if ((event->button.state & GDK_SHIFT_MASK) != 0)
		{
			/* Single click + Shift -> extended current
			   selection to include the clicked line */
			extend_selection_to_line (lines, iter);
		}
		else
		{
			gtk_text_buffer_place_cursor (lines->priv->buffer, iter);
		}
	}
	else if (event->type == GDK_2BUTTON_PRESS && (event->button.button == 1))
	{
		select_line (lines, iter);
	}
}

static gboolean
gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                   GtkTextIter             *iter,
                                   const GdkRectangle      *area,
                                   GdkEvent                *event)
{
	return GTK_SOURCE_GUTTER_RENDERER_LINES (renderer)->priv->buffer != NULL;
}

static void
gtk_source_gutter_renderer_lines_constructed (GObject *gobject)
{
	GtkSourceGutterRendererLines *lines;
	GtkSourceGutterRenderer *renderer;
	GtkTextView *view;

	renderer = GTK_SOURCE_GUTTER_RENDERER (gobject);
	lines = GTK_SOURCE_GUTTER_RENDERER_LINES (gobject);

	view = gtk_source_gutter_renderer_get_view (renderer);

	lines->priv->buffer_notify_handler_id =
		g_signal_connect (view,
		                  "notify::buffer",
		                  G_CALLBACK (on_buffer_notify),
		                  lines);

	set_buffer (lines, gtk_text_view_get_buffer (view));
}

static void
gtk_source_gutter_renderer_lines_class_init (GtkSourceGutterRendererLinesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

	object_class->finalize = gtk_source_gutter_renderer_lines_finalize;
	object_class->dispose = gtk_source_gutter_renderer_lines_dispose;
	object_class->constructed = gtk_source_gutter_renderer_lines_constructed;

	renderer_class->query_data = gutter_renderer_query_data;
	renderer_class->query_activatable = gutter_renderer_query_activatable;
	renderer_class->activate = gutter_renderer_activate;

	g_type_class_add_private (object_class, sizeof(GtkSourceGutterRendererLinesPrivate));
}

static void
gtk_source_gutter_renderer_lines_init (GtkSourceGutterRendererLines *self)
{
	self->priv = GTK_SOURCE_GUTTER_RENDERER_LINES_GET_PRIVATE (self);
}

GtkSourceGutterRenderer *
gtk_source_gutter_renderer_lines_new ()
{
	return g_object_new (GTK_TYPE_SOURCE_GUTTER_RENDERER_LINES, NULL);
}

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

#include "gtksourcegutterrendererlines-private.h"
#include "gtksourcegutterrenderertext-private.h"
#include "gtksourcegutterlines.h"
#include "gtksourceutils-private.h"
#include "gtksourceview.h"

struct _GtkSourceGutterRendererLines
{
	GtkSourceGutterRendererText parent_instance;
	PangoFont *cached_font;
	PangoFont *cached_bold_font;
	PangoGlyphInfo cached_infos[10];
	PangoGlyphInfo cached_bold_infos[10];
	GdkRGBA foreground_color;
	GdkRGBA current_line_color;
	int cached_baseline;
	int cached_bold_baseline;
	int cached_height;
	int num_line_digits;
	int prev_line_num;
	guint highlight_current_line : 1;
	guint cursor_visible : 1;
	guint current_line_bold : 1;
};

G_DEFINE_FINAL_TYPE (GtkSourceGutterRendererLines, _gtk_source_gutter_renderer_lines, GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT)

static void
update_cached_items (GtkSourceGutterRendererLines *self)
{
	PangoLayout *layout;
	PangoAttrList *attrs;
	PangoLayoutLine *line;
	int width, height;

	g_assert (GTK_SOURCE_IS_GUTTER_RENDERER_LINES (self));

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), "0123456789");
	self->cached_baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

	if ((line = pango_layout_get_line_readonly (layout, 0)))
	{
		if (line->runs != NULL)
		{
			const PangoGlyphItem *glyph_item = line->runs->data;
			const PangoGlyphString *glyphs = glyph_item->glyphs;
			PangoFont *font = glyph_item->item->analysis.font;
			guint n_chars = MIN (10, glyph_item->item->num_chars);

			g_set_object (&self->cached_font, font);

			for (guint i = 0; i < n_chars; i++)
			{
				self->cached_infos[i] = glyphs->glyphs[i];
			}
		}
	}

	pango_layout_get_pixel_size (layout, &width, &height);
	self->cached_height = height;

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_layout_set_attributes (layout, attrs);

	self->cached_bold_baseline = pango_layout_get_baseline (layout) / PANGO_SCALE;

	if ((line = pango_layout_get_line_readonly (layout, 0)))
	{
		if (line->runs != NULL)
		{
			const PangoGlyphItem *glyph_item = line->runs->data;
			const PangoGlyphString *glyphs = glyph_item->glyphs;
			PangoFont *font = glyph_item->item->analysis.font;
			guint n_chars = MIN (10, glyph_item->item->num_chars);

			g_set_object (&self->cached_bold_font, font);

			for (guint i = 0; i < n_chars; i++)
			{
				self->cached_bold_infos[i] = glyphs->glyphs[i];
			}
		}
	}

	pango_attr_list_unref (attrs);
	g_object_unref (layout);
}

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
	gint num_lines = 1;
	gint num_digits;

	buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));

	if (buffer != NULL)
	{
		num_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	}

	num_digits = count_num_digits (num_lines);

	if (num_digits < 2)
	{
		num_digits = 2;
	}

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
	if (renderer->cursor_visible || renderer->highlight_current_line)
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
gtk_source_gutter_renderer_lines_css_changed (GtkWidget         *widget,
                                              GtkCssStyleChange *change)
{
	GtkSourceGutterRendererLines *renderer = GTK_SOURCE_GUTTER_RENDERER_LINES (widget);

	GTK_WIDGET_CLASS (_gtk_source_gutter_renderer_lines_parent_class)->css_changed (widget, change);

	update_cached_items (renderer);

	/* Force to recalculate the size. */
	renderer->num_line_digits = -1;
	recalculate_size (renderer);
}

static void
on_view_notify (GtkSourceView                *view,
                GParamSpec                   *pspec,
                GtkSourceGutterRendererLines *renderer)
{
	renderer->cursor_visible = gtk_text_view_get_cursor_visible (GTK_TEXT_VIEW (view));
	renderer->highlight_current_line = gtk_source_view_get_highlight_current_line (view);
}

static void
gutter_renderer_change_view (GtkSourceGutterRenderer *renderer,
			     GtkSourceView             *old_view)
{
	GtkSourceView *new_view;

	if (old_view != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_view, on_view_notify, renderer);
	}

	new_view = gtk_source_gutter_renderer_get_view (renderer);

	if (new_view != NULL)
	{
		g_signal_connect_object (new_view,
		                         "notify::cursor-visible",
		                         G_CALLBACK (on_view_notify),
		                         renderer,
		                         0);
		g_signal_connect_object (new_view,
		                         "notify::highlight-current-line",
		                         G_CALLBACK (on_view_notify),
		                         renderer,
		                         0);

		on_view_notify (new_view, NULL, GTK_SOURCE_GUTTER_RENDERER_LINES (renderer));
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
		guint num_lines = 0;
		gint size;
		gint xpad;

		buffer = gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (renderer));

		if (buffer != NULL)
		{
			num_lines = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
		}

		if (num_lines < 99)
		{
			num_lines = 99;
		}

		g_snprintf (markup, sizeof markup, "%u", num_lines);
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
gtk_source_gutter_renderer_lines_snapshot_line (GtkSourceGutterRenderer *renderer,
                                                GtkSnapshot             *snapshot,
                                                GtkSourceGutterLines    *lines,
                                                guint                    line)
{
	GtkSourceGutterRendererLines *self = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);
	const PangoGlyphInfo *cached_infos;
	PangoGlyphString glyph_string = {0};
	PangoGlyphInfo glyph_info[12];
	GskRenderNode *node;
	const GdkRGBA *color;
	const gchar *text;
	PangoFont *font;
	float x, y;
	int height, width;
	int baseline;
	int len;

	if (self->cached_font == NULL)
	{
		return;
	}

	cached_infos = self->cached_infos;
	font = self->cached_font;
	baseline = self->cached_baseline;

	if (gtk_source_gutter_lines_is_cursor (lines, line))
	{
		color = &self->current_line_color;

		if (self->current_line_bold)
		{
			cached_infos = self->cached_bold_infos;
			font = self->cached_bold_font;
			baseline = self->cached_bold_baseline;
		}
	}
	else
	{
		color = &self->foreground_color;
	}

	len = _gtk_source_utils_int_to_string (line + 1, &text);

	glyph_string.num_glyphs = len;
	glyph_string.log_clusters = 0;
	glyph_string.glyphs = glyph_info;

	width = 0;
	height = self->cached_height;

	for (int i = 0; i < len; i++)
	{
		int index = text[i] - '0';

		g_assert (index >= 0);
		g_assert (index < 10);

		glyph_info[i] = cached_infos[index];

		width += glyph_info[i].geometry.width / PANGO_SCALE;
	}

	gtk_source_gutter_renderer_align_cell (renderer, line, width, height, &x, &y);

	node = gsk_text_node_new (font,
	                          &glyph_string,
	                          color,
	                          &GRAPHENE_POINT_INIT (x, y + baseline));
	gtk_snapshot_append_node (snapshot, node);
	gsk_render_node_unref (node);
}

static void
gtk_source_gutter_renderer_lines_begin (GtkSourceGutterRenderer *renderer,
                                        GtkSourceGutterLines    *lines)
{
	GtkSourceGutterRendererLines *self = GTK_SOURCE_GUTTER_RENDERER_LINES (renderer);
	gboolean current_line_bold;

	g_assert (GTK_SOURCE_IS_GUTTER_RENDERER_LINES (self));
	g_assert (GTK_SOURCE_IS_GUTTER_LINES (lines));

	GTK_SOURCE_GUTTER_RENDERER_CLASS (_gtk_source_gutter_renderer_lines_parent_class)->begin (renderer, lines);

	_gtk_source_gutter_renderer_text_get_draw (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer),
						   &self->foreground_color,
						   &self->current_line_color,
						   &current_line_bold);

	self->current_line_bold = !!current_line_bold;
}

static void
gtk_source_gutter_renderer_lines_query_data (GtkSourceGutterRenderer *renderer,
                                             GtkSourceGutterLines    *lines,
                                             guint                    line)
{
}

static void
gtk_source_gutter_renderer_lines_dispose (GObject *object)
{
	GtkSourceGutterRendererLines *self = GTK_SOURCE_GUTTER_RENDERER_LINES (object);

	g_clear_object (&self->cached_font);
	g_clear_object (&self->cached_bold_font);

	G_OBJECT_CLASS (_gtk_source_gutter_renderer_lines_parent_class)->dispose (object);
}

static void
_gtk_source_gutter_renderer_lines_class_init (GtkSourceGutterRendererLinesClass *klass)
{
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_gutter_renderer_lines_dispose;

	widget_class->measure = gtk_source_gutter_renderer_lines_measure;
	widget_class->css_changed = gtk_source_gutter_renderer_lines_css_changed;

	renderer_class->activate = gutter_renderer_activate;
	renderer_class->begin = gtk_source_gutter_renderer_lines_begin;
	renderer_class->change_buffer = gutter_renderer_change_buffer;
	renderer_class->change_view = gutter_renderer_change_view;
	renderer_class->query_activatable = gutter_renderer_query_activatable;
	renderer_class->query_data = gtk_source_gutter_renderer_lines_query_data;
	renderer_class->snapshot_line = gtk_source_gutter_renderer_lines_snapshot_line;
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

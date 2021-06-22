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

#include "gtksourcegutterrenderertext.h"

/**
 * SECTION:gutterrenderertext
 * @Short_description: Renders text in the gutter
 * @Title: GtkSourceGutterRendererText
 * @See_also: #GtkSourceGutterRenderer, #GtkSourceGutter
 *
 * A #GtkSourceGutterRendererText can be used to render text in a cell of
 * #GtkSourceGutter.
 */

typedef struct
{
	int width;
	int height;
} Size;

typedef struct
{
	gchar       *text;
	PangoLayout *cached_layout;
	GdkRGBA      cached_color;
	gsize        text_len;
	Size         cached_sizes[5];
	guint        is_markup : 1;
} GtkSourceGutterRendererTextPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceGutterRendererText, gtk_source_gutter_renderer_text, GTK_SOURCE_TYPE_GUTTER_RENDERER)

enum
{
	PROP_0,
	PROP_MARKUP,
	PROP_TEXT,
	N_PROPS
};

static void
gtk_source_gutter_renderer_text_clear_cached_sizes (GtkSourceGutterRendererText *text)
{
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (text);

	for (guint i = 0; i < G_N_ELEMENTS (priv->cached_sizes); i++)
	{
		priv->cached_sizes[i].width = -1;
		priv->cached_sizes[i].height = -1;
	}
}

static inline void
gtk_source_gutter_renderer_text_get_size (GtkSourceGutterRendererTextPrivate *priv,
                                          PangoLayout                        *layout,
                                          int                                 text_len,
                                          int                                *width,
                                          int                                *height)
{
	g_assert (text_len > 0);

	if G_UNLIKELY (text_len > G_N_ELEMENTS (priv->cached_sizes) ||
	               priv->cached_sizes[text_len-1].width == -1)
	{
		pango_layout_get_pixel_size (layout, width, height);

		if (text_len <= G_N_ELEMENTS (priv->cached_sizes))
		{
			priv->cached_sizes[text_len-1].width = *width;
			priv->cached_sizes[text_len-1].height = *height;
		}
	}
	else
	{
		*width = priv->cached_sizes[text_len-1].width;
		*height = priv->cached_sizes[text_len-1].height;
	}
}

static void
gtk_source_gutter_renderer_text_begin (GtkSourceGutterRenderer *renderer,
                                       GtkSourceGutterLines    *lines)
{

	GtkSourceGutterRendererText *text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (text);

	GTK_SOURCE_GUTTER_RENDERER_CLASS (gtk_source_gutter_renderer_text_parent_class)->begin (renderer, lines);

	g_clear_object (&priv->cached_layout);
	priv->cached_layout = gtk_widget_create_pango_layout (GTK_WIDGET (renderer), NULL);

	gtk_source_gutter_renderer_text_clear_cached_sizes (text);
}

static void
gtk_source_gutter_renderer_text_snapshot_line (GtkSourceGutterRenderer *renderer,
                                               GtkSnapshot             *snapshot,
                                               GtkSourceGutterLines    *lines,
                                               guint                    line)
{
	GtkSourceGutterRendererText *text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (text);
	PangoLayout *layout;
	float x;
	float y;
	int width;
	int height;

	if (priv->text == NULL || priv->text_len == 0)
	{
		return;
	}

	layout = priv->cached_layout;

	if (priv->is_markup)
	{
		pango_layout_set_markup (layout,
		                         priv->text,
		                         priv->text_len);
	}
	else
	{
		pango_layout_set_text (layout,
		                       priv->text,
		                       priv->text_len);
	}

	gtk_source_gutter_renderer_text_get_size (priv, layout, priv->text_len, &width, &height);
	gtk_source_gutter_renderer_align_cell (renderer, line, width, height, &x, &y);

	gtk_snapshot_render_layout (snapshot,
	                            gtk_widget_get_style_context (GTK_WIDGET (text)),
	                            x,
	                            y,
	                            layout);

	if (priv->is_markup)
	{
		pango_layout_set_attributes (layout, NULL);
	}
}

static void
gtk_source_gutter_renderer_text_end (GtkSourceGutterRenderer *renderer)
{

	GtkSourceGutterRendererText *text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (text);

	GTK_SOURCE_GUTTER_RENDERER_CLASS (gtk_source_gutter_renderer_text_parent_class)->end (renderer);

	g_clear_object (&priv->cached_layout);
}

static void
measure_text (GtkSourceGutterRendererText *renderer,
              const gchar                 *markup,
              const gchar                 *text,
              gint                        *width,
              gint                        *height)
{
	GtkSourceView *view;
	PangoLayout *layout;

	if (width != NULL)
	{
		*width = 0;
	}

	if (height != NULL)
	{
		*height = 0;
	}

	view = gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (renderer));

	if (view == NULL)
	{
		return;
	}

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), NULL);

	if (layout == NULL)
	{
		return;
	}

	if (markup != NULL)
	{
		pango_layout_set_markup (layout, markup, -1);
	}
	else
	{
		pango_layout_set_text (layout, text, -1);
	}

	pango_layout_get_pixel_size (layout, width, height);

	g_object_unref (layout);
}

/**
 * gtk_source_gutter_renderer_text_measure:
 * @renderer: a #GtkSourceGutterRendererText.
 * @text: the text to measure.
 * @width: (out) (optional): location to store the width of the text in pixels,
 *   or %NULL.
 * @height: (out) (optional): location to store the height of the text in
 *   pixels, or %NULL.
 *
 * Measures the text provided using the pango layout used by the
 * #GtkSourceGutterRendererText.
 */
void
gtk_source_gutter_renderer_text_measure (GtkSourceGutterRendererText *renderer,
                                         const gchar                 *text,
                                         gint                        *width,
                                         gint                        *height)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_TEXT (renderer));
	g_return_if_fail (text != NULL);

	measure_text (renderer, NULL, text, width, height);
}

/**
 * gtk_source_gutter_renderer_text_measure_markup:
 * @renderer: a #GtkSourceGutterRendererText.
 * @markup: the pango markup to measure.
 * @width: (out) (optional): location to store the width of the text in pixels,
 *   or %NULL.
 * @height: (out) (optional): location to store the height of the text in
 *   pixels, or %NULL.
 *
 * Measures the pango markup provided using the pango layout used by the
 * #GtkSourceGutterRendererText.
 */
void
gtk_source_gutter_renderer_text_measure_markup (GtkSourceGutterRendererText *renderer,
                                                const gchar                 *markup,
                                                gint                        *width,
                                                gint                        *height)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_TEXT (renderer));
	g_return_if_fail (markup != NULL);

	measure_text (renderer, markup, NULL, width, height);
}

static void
gtk_source_gutter_renderer_text_real_measure (GtkWidget      *widget,
                                              GtkOrientation  orientation,
                                              int             for_size,
                                              int            *minimum,
                                              int            *natural,
                                              int            *minimum_baseline,
                                              int            *natural_baseline)
{
	GtkSourceGutterRendererText *renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (widget);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (renderer);

	*minimum = 0;
	*natural = 0;
	*minimum_baseline = -1;
	*natural_baseline = -1;

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
		gint xpad = gtk_source_gutter_renderer_get_xpad (GTK_SOURCE_GUTTER_RENDERER (renderer));
		gint width = 0;
		gint height = 0;

		if (priv->text != NULL)
		{
			if (priv->is_markup)
			{
				measure_text (renderer, priv->text, NULL, &width, &height);
			}
			else
			{
				measure_text (renderer, NULL, priv->text, &width, &height);
			}
		}

		*natural = *minimum = width + 2 * xpad;
	}

}

static void
gtk_source_gutter_renderer_text_finalize (GObject *object)
{
	GtkSourceGutterRendererText *renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (renderer);

	g_clear_pointer (&priv->text, g_free);
	g_clear_object (&priv->cached_layout);

	G_OBJECT_CLASS (gtk_source_gutter_renderer_text_parent_class)->finalize (object);
}

static void
set_text (GtkSourceGutterRendererText *renderer,
          const gchar                 *text,
          gint                         length,
          gboolean                     is_markup)
{
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (renderer);

	g_free (priv->text);

	if (text == NULL)
	{
		priv->text_len = 0;
		priv->text = NULL;
		priv->is_markup = FALSE;
	}
	else
	{
		priv->text_len = length >= 0 ? length : strlen (text);
		priv->text = g_strndup (text, priv->text_len);
		priv->is_markup = !!is_markup;
	}
}

static void
gtk_source_gutter_renderer_text_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
	GtkSourceGutterRendererText *renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);

	switch (prop_id)
	{
		case PROP_MARKUP:
			set_text (renderer, g_value_get_string (value), -1, TRUE);
			break;
		case PROP_TEXT:
			set_text (renderer, g_value_get_string (value), -1, FALSE);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_text_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
	GtkSourceGutterRendererText *renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (renderer);

	switch (prop_id)
	{
		case PROP_MARKUP:
			g_value_set_string (value, priv->is_markup ? priv->text : NULL);
			break;
		case PROP_TEXT:
			g_value_set_string (value, !priv->is_markup ? priv->text : NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_text_class_init (GtkSourceGutterRendererTextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

	object_class->finalize = gtk_source_gutter_renderer_text_finalize;
	object_class->get_property = gtk_source_gutter_renderer_text_get_property;
	object_class->set_property = gtk_source_gutter_renderer_text_set_property;

	widget_class->measure = gtk_source_gutter_renderer_text_real_measure;

	renderer_class->begin = gtk_source_gutter_renderer_text_begin;
	renderer_class->end = gtk_source_gutter_renderer_text_end;
	renderer_class->snapshot_line = gtk_source_gutter_renderer_text_snapshot_line;

	g_object_class_install_property (object_class,
	                                 PROP_MARKUP,
	                                 g_param_spec_string ("markup",
	                                                      "Markup",
	                                                      "The markup",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_TEXT,
	                                 g_param_spec_string ("text",
	                                                      "Text",
	                                                      "The text",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
gtk_source_gutter_renderer_text_init (GtkSourceGutterRendererText *self)
{
	GtkSourceGutterRendererTextPrivate *priv = gtk_source_gutter_renderer_text_get_instance_private (self);

	priv->is_markup = TRUE;

	gtk_source_gutter_renderer_text_clear_cached_sizes (self);
}

/**
 * gtk_source_gutter_renderer_text_new:
 *
 * Create a new #GtkSourceGutterRendererText.
 *
 * Returns: (transfer full): A #GtkSourceGutterRenderer
 *
 **/
GtkSourceGutterRenderer *
gtk_source_gutter_renderer_text_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT, NULL);
}

void
gtk_source_gutter_renderer_text_set_markup (GtkSourceGutterRendererText *renderer,
                                            const gchar                 *markup,
                                            gint                         length)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_TEXT (renderer));

	set_text (renderer, markup, length, TRUE);
}

void
gtk_source_gutter_renderer_text_set_text (GtkSourceGutterRendererText *renderer,
                                          const gchar                 *text,
                                          gint                         length)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_TEXT (renderer));

	set_text (renderer, text, length, FALSE);
}

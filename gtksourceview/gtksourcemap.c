/*
 * Copyright 2015-2020 Christian Hergert <christian@hergert.me>
 * Copyright 2015 Ignacio Casal Quinteiro <icq@gnome.org>
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

#include <math.h>
#include <string.h>

#include "gtksourcemap.h"
#include "gtksourcebuffer.h"
#include "gtksourcecompletion.h"
#include "gtksourcestyle-private.h"
#include "gtksourcestylescheme.h"
#include "gtksourceutils-private.h"
#include "gtksourceview-private.h"

#define SCRUBBER_MIN_HEIGHT 10
#define DRAG_THRESHOLD      5.0

/**
 * GtkSourceMap:
 *
 * Widget that displays a map for a specific [class@View].
 *
 * `GtkSourceMap` is a widget that maps the content of a [class@View] into
 * a smaller view so the user can have a quick overview of the whole document.
 *
 * This works by connecting a [class@View] to to the `GtkSourceMap` using
 * the [property@Map:view] property or [method@Map.set_view].
 *
 * `GtkSourceMap` is a [class@View] object. This means that you can add a
 * [class@GutterRenderer] to a gutter in the same way you would for a
 * [class@View]. One example might be a [class@GutterRenderer] that shows
 * which lines have changed in the document.
 *
 * Additionally, it is desirable to match the font of the `GtkSourceMap` and
 * the [class@View] used for editing. Therefore, [property@Map:font-desc]
 * should be used to set the target font. You will need to adjust this to the
 * desired font size for the map. A 1pt font generally seems to be an
 * appropriate font size. "Monospace 1" is the default. See
 * [method@Pango.FontDescription.set_size] for how to alter the size of an existing
 * [struct@Pango.FontDescription].
 *
 * When FontConfig is available, `GtkSourceMap` will try to use a bundled
 * "block" font to make the map more legible.
 */

/*
 * Implementation Notes:
 *
 * I tried implementing this a few different ways. They are worth noting so
 * that we do not repeat the same mistakes.
 *
 * Originally, I thought using a GtkSourceView to do the rendering was overkill
 * and would likely slow things down too much. But it turns out to have been
 * the best option so far.
 *
 *   - GtkPixelCache support results in very few GtkTextLayout relayout and
 *     sizing changes. Since the pixel cache renders +/- half a screen outside
 *     the visible range, scrolling is also quite smooth as we very rarely
 *     perform a new gtk_text_layout_draw().
 *
 *     With GTK 4, there is no pixel cache and the PangoLayout are cached
 *     instead.
 *
 *   - Performance for this type of widget is dominated by text layout
 *     rendering. When you scale out this far, you increase the number of
 *     layouts to be rendered greatly.
 *
 *   - We can pack GtkSourceGutterRenderer into the child view to provide
 *     additional information. This is quite handy to show information such
 *     as errors, line changes, and anything else that can help the user
 *     quickly jump to the target location.
 *
 * I also tried drawing the contents of the GtkSourceView onto a widget after
 * performing a cairo_scale(). This does not help us much because we ignore
 * pixel cache when cairo_scale is not 1-to-1. This results in layout
 * invalidation and worst case render paths.
 *
 * I also tried rendering the slider (overlay box) during the
 * GtkTextView::draw_layer() vfunc. The problem with this approach is that
 * the slider contents are actually pixel cached. So every time the slider
 * moves we have to invalidate the GtkTextLayout and redraw cached contents.
 * Where as drawing in the GtkTextView::draw() vfunc, after the pixel cache
 * contents have been drawn results in only a composite blend, not
 * invalidating any of the pixel cached text layouts.
 *
 * By default we use a 1pt Monospace font. However, if the Gtksourcemap:font-desc
 * property is set, we will use that instead.
 *
 * We do not render the background grid as it requires a bunch of
 * cpu time for something that will essentially just create a solid
 * color background.
 *
 * The width of the view is determined by the
 * #GtkSourceView:right-margin-position. We cache the width of a
 * single "X" character and multiple that by the right-margin-position.
 * That becomes our size-request width.
 *
 * We do not allow horizontal scrolling so that the overflow text
 * is simply not visible in the minimap.
 *
 * -- Christian
 */

#define DEFAULT_WIDTH 100

struct _GtkSourceMapSlider
{
	GtkWidget parent_instance;
};

#define GTK_SOURCE_TYPE_MAP_SLIDER (gtk_source_map_slider_get_type())
G_DECLARE_FINAL_TYPE (GtkSourceMapSlider, gtk_source_map_slider, GTK_SOURCE, MAP_SLIDER, GtkWidget)
G_DEFINE_TYPE (GtkSourceMapSlider, gtk_source_map_slider, GTK_TYPE_WIDGET)

static void
gtk_source_map_slider_class_init (GtkSourceMapSliderClass *klass)
{
	gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "slider");
}

static void
gtk_source_map_slider_init (GtkSourceMapSlider *self)
{
}

typedef struct
{
	/*
	 * By default, we use "Monospace 1pt". However, most text editing
	 * applications will have a custom font, so we allow them to set
	 * that here. Generally speaking, you will want to continue using
	 * a 1pt font, but if they set GtkSourceMap:font-desc, then they
	 * should also shrink the font to the desired size.
	 *
	 * For example:
	 *   pango_font_description_set_size(font_desc, 1 * PANGO_SCALE);
	 *
	 * Would set a 1pt font on whatever PangoFontDescription you have
	 * in your text editor.
	 */
	PangoFontDescription *font_desc;

	/*
	 * The easiest way to style the slider and the sourceview is
	 * by using CSS. This is necessary since we can't mess with the
	 * fonts used in the textbuffer (as one might using GtkTextTag).
	 */
	GtkCssProvider *css_provider;

	/* The GtkSourceView we are providing a map of */
	GtkSourceView *view;

	/* A weak pointer to the connected buffer */
	GtkTextBuffer *buffer;

	/* The slider widget */
	GtkSourceMapSlider *slider;

	/* The location of the slider in widget coordinate space. */
	GdkRectangle slider_area;

	/* We compare against old values from the vadjustment as it can
	 * notify a bit more than is necessary.
	 */
	double last_vadj_upper;
	double last_vadj_value;

	/* Weak pointer view to child view bindings */
	GBinding *buffer_binding;
	GBinding *indent_width_binding;
	GBinding *tab_width_binding;
	GBinding *bottom_margin_binding;
	GBinding *top_margin_binding;

	/* Our signal handler for view changes */
	gulong view_notify_buffer_handler;
	gulong view_notify_right_margin_position_handler;
	gulong view_vadj_value_changed_handler;
	gulong view_vadj_notify_upper_handler;

	/* Signals connected indirectly to the buffer */
	gulong buffer_notify_style_scheme_handler;

	/* Tick callback to queue work until the next frame to
	 * avoid doing changes during LAYOUT phase.
	 */
	guint update_id;

	/* Denotes if we are in a grab from button press */
	guint in_press : 1;

	/* If we failed to locate a color for the slider, then this will
	 * be set to 0 and that means we need to apply the "selection"
	 * class when drawing so that we get an appropriate color.
	 */
	guint had_color : 1;

	/* If we dragged enough to reach a drag threshold */
	guint reached_drag_threshold : 1;

	/* How much the slider should be shifted from the position of the cursor */
	double slider_y_shift;
} GtkSourceMapPrivate;

enum
{
	PROP_0,
	PROP_VIEW,
	PROP_FONT_DESC,
	N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceMap, gtk_source_map, GTK_SOURCE_TYPE_VIEW)

static GParamSpec *properties[N_PROPERTIES];

static void
get_slider_position (GtkSourceMap *map,
                     int           width,
                     int           height,
                     GdkRectangle *slider_area)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GdkRectangle them_visible_rect, us_visible_rect;
	GtkTextBuffer *buffer;
	GtkTextIter end_iter;
	GdkRectangle end_rect;
	GtkBorder border;
	int us_height;
	int us_width;
	int them_height;

	if (priv->view == NULL)
	{
		return;
	}

	us_width = gtk_widget_get_width (GTK_WIDGET (map));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (map));

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (GTK_WIDGET (map));
		gtk_style_context_get_border (style_context, &border);
	} G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_text_buffer_get_end_iter (buffer, &end_iter);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (map), &end_iter, &end_rect);
	us_height = end_rect.y + end_rect.height;
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->view), &end_iter, &end_rect);
	them_height = end_rect.y + end_rect.height;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (priv->view), &them_visible_rect);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (map), &us_visible_rect);

	slider_area->x = 0;
	slider_area->width = us_width - border.left - border.right;
	slider_area->height = 100;
	slider_area->y = (double)them_visible_rect.y / (double)them_height * (double)us_height;
	slider_area->height = ((double)(them_visible_rect.y + them_visible_rect.height) / (double)them_height * (double)us_height) - slider_area->y;

	slider_area->y -= us_visible_rect.y;
}

static void
gtk_source_map_allocate_slider (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GdkRectangle area;
	int width, height;
	int min, nat;

	g_assert (GTK_SOURCE_IS_MAP (map));

	width = gtk_widget_get_width (GTK_WIDGET (map));
	height = gtk_widget_get_height (GTK_WIDGET (map));

	if (width == 0 || height == 0)
		return;

	get_slider_position (map, width, height, &area);
	gtk_widget_measure (GTK_WIDGET (priv->slider),
	                    GTK_ORIENTATION_VERTICAL,
	                    width, &min, &nat, NULL, NULL);
	area.height = MAX (nat, area.height);
	gtk_widget_size_allocate (GTK_WIDGET (priv->slider), &area, -1);
}

static void
gtk_source_map_rebuild_css (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkSourceStyleScheme *style_scheme;
	GtkSourceStyle *style = NULL;
	GtkSourceStyle *text = NULL;
	GtkTextBuffer *buffer;
	GdkRGBA real_bg;
	const char *color;
	gboolean use_fg = FALSE;
	GString *gstr;
	char *background = NULL;
	char *foreground = NULL;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		return;
	}

	/*
	 * This is where we calculate the CSS that maps the font for the
	 * minimap as well as the styling for the slider.
	 *
	 * The font is calculated from #GtkSourceMap:font-desc. We convert this
	 * to CSS using _gtk_source_utils_pango_font_description_to_css(). It
	 * gets applied to the minimap widget via the CSS style provider which
	 * we attach to the view in gtk_source_map_init().
	 *
	 * The rules for calculating the style for the slider are as follows.
	 *
	 * If the current style scheme provides a background color for the
	 * slider using the "map-overlay" style name, we use that without
	 * any transformations.
	 *
	 * If the style scheme contains a "selection" style scheme, used for
	 * selected text, we use that with a 0.75 alpha value.
	 *
	 * If none of these are met, we take the background from the
	 * #GtkStyleContext using the deprecated
	 * gtk_style_context_get_background_color(). This is non-ideal, but
	 * currently required since we cannot indicate that we want to
	 * alter the alpha for gtk_render_background().
	 */

	gstr = g_string_new (NULL);

	/* Calculate the font if one has been set */
	if (priv->font_desc != NULL)
	{
		gchar *css;

		css = _gtk_source_utils_pango_font_description_to_css (priv->font_desc);
		g_string_append_printf (gstr, "textview { %s }\n", css != NULL ? css : "");
		g_free (css);
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->view));
	style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

	G_GNUC_BEGIN_IGNORE_DEPRECATIONS {
		GtkStyleContext *style_context;

		style_context = gtk_widget_get_style_context (GTK_WIDGET (map));
		if (!gtk_style_context_lookup_color (style_context, "view_bg_color", &real_bg))
			memset (&real_bg, 0, sizeof (real_bg));
	} G_GNUC_END_IGNORE_DEPRECATIONS

	if (style_scheme != NULL)
	{
		if ((text = gtk_source_style_scheme_get_style (style_scheme, "text")))
		{
			char *str;

			g_object_get (text,
				      "background", &str,
				      NULL);

			if (str != NULL)
			{
				gdk_rgba_parse (&real_bg, str);
				g_free (str);
			}
		}

		if (!(style = gtk_source_style_scheme_get_style (style_scheme, "map-overlay")) &&
		    !(style = gtk_source_style_scheme_get_style (style_scheme, "selection")))
		{
			/* Use the foreground color if we can as that will get lightened to
			 * .25 alpha below so that we have *something* rather dark compared
			 * to the background color. Otherwise it will get washed out like it
			 * does with classic.xml.
			 */
			if ((style = gtk_source_style_scheme_get_style (style_scheme, "text")))
			{
				use_fg = TRUE;
			}
		}
	}

	if (style != NULL)
	{
		gboolean background_set;
		gboolean foreground_set;

		g_object_get (style,
		              "background", &background,
		              "background-set", &background_set,
		              "foreground", &foreground,
		              "foreground-set", &foreground_set,
		              NULL);

		if (!background_set)
		{
			g_clear_pointer (&background, g_free);
		}

		if (!foreground_set)
		{
			g_clear_pointer (&foreground, g_free);
		}
	}
	else
	{
		GdkRGBA bg;

		if (_gtk_source_view_get_current_line_background (priv->view, &bg))
		{
			bg.alpha = 1.0;
			background = gdk_rgba_to_string (&bg);
		}
	}

	if (background != NULL)
	{
		GdkRGBA parsed;

		if (gdk_rgba_parse (&parsed, background))
		{
			if (parsed.alpha < 1.0)
			{
				parsed.alpha = 1.0;
				g_free (background);
				background = gdk_rgba_to_string (&parsed);
			}
		}
	}

	if (foreground != NULL)
	{
		GdkRGBA parsed;

		if (gdk_rgba_parse (&parsed, foreground))
		{
			if (parsed.alpha < 1.0)
			{
				parsed.alpha = 1.0;
				g_free (foreground);
				foreground = gdk_rgba_to_string (&parsed);
			}
		}
	}

	color = use_fg ? foreground : background;
	priv->had_color = color != NULL;

	if (color != NULL)
	{
		GdkRGBA to_mix;
		GdkRGBA normal;
		GdkRGBA hover;
		GdkRGBA active;
		char *normal_str;
		char *hover_str;
		char *active_str;

		gdk_rgba_parse (&to_mix, color);

		premix_colors (&normal, &to_mix, &real_bg, real_bg.alpha > 0., .25);
		premix_colors (&hover, &to_mix, &real_bg, real_bg.alpha > 0., .35);
		premix_colors (&active, &to_mix, &real_bg, real_bg.alpha > 0., .5);

		normal_str = gdk_rgba_to_string (&normal);
		hover_str = gdk_rgba_to_string (&hover);
		active_str = gdk_rgba_to_string (&active);

		g_string_append_printf (gstr,
		                        "slider {"
		                        " background-color: %s;"
		                        " transition-duration: 300ms;"
		                        "}\n"
		                        "slider:hover {"
		                        " background-color: %s;"
		                        "}\n"
		                        "slider.dragging:hover {"
		                        " background-color: %s;"
		                        "}\n",
					normal_str, hover_str, active_str);

		g_free (normal_str);
		g_free (hover_str);
		g_free (active_str);
	}

	g_free (background);
	g_free (foreground);

	if (gstr->len > 0)
	{
		gtk_css_provider_load_from_string (priv->css_provider, gstr->str);
	}

	g_string_free (gstr, TRUE);
}

static void
update_child_vadjustment (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GtkAdjustment *vadj;
	GtkAdjustment *child_vadj;
	gdouble value;
	gdouble upper;
	gdouble page_size;
	gdouble child_upper;
	gdouble child_page_size;
	gdouble new_value = 0.0;

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));
	g_object_get (vadj,
	              "upper", &upper,
	              "value", &value,
	              "page-size", &page_size,
	              NULL);

	child_vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (map));
	g_object_get (child_vadj,
	              "upper", &child_upper,
	              "page-size", &child_page_size,
	              NULL);

	/*
	 * FIXME:
	 * Technically we should take into account lower here, but in practice
	 *       it is always 0.0.
	 */
	if (child_page_size < child_upper)
	{
		new_value = (value / (upper - page_size)) * (child_upper - child_page_size);
	}

	gtk_adjustment_set_value (child_vadj, new_value);

	gtk_source_map_allocate_slider (map);
}

static gboolean
gtk_source_map_do_update (GtkWidget     *widget,
                          GdkFrameClock *frame_clock,
			  gpointer       user_data)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	priv->update_id = 0;
	update_child_vadjustment (map);
	return G_SOURCE_REMOVE;
}

static void
gtk_source_map_queue_update (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	g_assert (GTK_SOURCE_IS_MAP (map));

	if (priv->update_id == 0)
	{
		priv->update_id =
			gtk_widget_add_tick_callback (GTK_WIDGET (map),
			                              gtk_source_map_do_update,
			                              NULL, NULL);
	}
}

static void
view_vadj_value_changed (GtkSourceMap  *map,
                         GtkAdjustment *vadj)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	double value = gtk_adjustment_get_value (vadj);

	if (value != priv->last_vadj_value)
	{
		priv->last_vadj_value = value;
		gtk_source_map_queue_update (map);
	}
}

static void
view_vadj_notify_upper (GtkSourceMap  *map,
                        GParamSpec    *pspec,
                        GtkAdjustment *vadj)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	double upper = gtk_adjustment_get_upper (vadj);

	if (upper != priv->last_vadj_upper)
	{
		priv->last_vadj_upper = upper;
		gtk_source_map_queue_update (map);
	}
}

static void
buffer_notify_style_scheme (GtkSourceMap  *map,
                            GParamSpec    *pspec,
                            GtkTextBuffer *buffer)
{
	gtk_source_map_rebuild_css (map);
}

static void
connect_buffer (GtkSourceMap  *map,
                GtkTextBuffer *buffer)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	priv->buffer = buffer;
	g_object_add_weak_pointer (G_OBJECT (buffer), (gpointer *)&priv->buffer);

	priv->buffer_notify_style_scheme_handler =
		g_signal_connect_object (buffer,
		                         "notify::style-scheme",
		                         G_CALLBACK (buffer_notify_style_scheme),
		                         map,
		                         G_CONNECT_SWAPPED);

	buffer_notify_style_scheme (map, NULL, buffer);
}

static void
disconnect_buffer (GtkSourceMap  *map)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->buffer == NULL)
	{
		return;
	}

	if (priv->buffer_notify_style_scheme_handler != 0)
	{
		g_signal_handler_disconnect (priv->buffer,
		                             priv->buffer_notify_style_scheme_handler);
		priv->buffer_notify_style_scheme_handler = 0;
	}

	g_object_remove_weak_pointer (G_OBJECT (priv->buffer), (gpointer *)&priv->buffer);
	priv->buffer = NULL;
}

static void
view_notify_buffer (GtkSourceMap  *map,
                    GParamSpec    *pspec,
                    GtkSourceView *view)
{
	GtkSourceMapPrivate *priv;
	GtkTextBuffer *buffer;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->buffer != NULL)
	{
		disconnect_buffer (map);
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (buffer != NULL)
	{
		connect_buffer (map, buffer);
	}
}

static void
gtk_source_map_set_font_desc (GtkSourceMap               *map,
                              const PangoFontDescription *font_desc)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (font_desc != priv->font_desc)
	{
		g_clear_pointer (&priv->font_desc, pango_font_description_free);

		if (font_desc)
		{
			priv->font_desc = pango_font_description_copy (font_desc);

			if ((pango_font_description_get_set_fields (priv->font_desc) & PANGO_FONT_MASK_SIZE) == 0)
			{
				pango_font_description_set_size (priv->font_desc, 1.75 * PANGO_SCALE);
			}
		}
	}

	gtk_source_map_rebuild_css (map);
}

static void
gtk_source_map_set_font_name (GtkSourceMap *map,
                              const gchar  *font_name)
{
	PangoFontDescription *font_desc;

	if (font_name == NULL)
	{
		font_name = "Monospace 1";
	}

	font_desc = pango_font_description_from_string (font_name);
	gtk_source_map_set_font_desc (map, font_desc);
	pango_font_description_free (font_desc);
}

static void
gtk_source_map_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		return;
	}

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
		if (priv->font_desc == NULL)
		{
			*minimum = *natural = DEFAULT_WIDTH;
		}
		else
		{
			PangoLayout *layout;
			char *text;
			guint right_margin_position;
			int height;
			int width;

			right_margin_position = gtk_source_view_get_right_margin_position (priv->view);

			text = g_malloc (right_margin_position + 1);
			memset (text, 'X', right_margin_position);
			text[right_margin_position] = 0;

			/*
			 * FIXME:
			 *
			 * This seems like the type of thing we should calculate when
			 * rebuilding our CSS since it gets used a bunch and changes
			 * very little.
			 */
			layout = gtk_widget_create_pango_layout (GTK_WIDGET (map), text);
			pango_layout_get_pixel_size (layout, &width, &height);
			g_object_unref (layout);
			g_free (text);

			/* If left-margin is set, try to balance the right side with the same amount
			 * of additional space to keep it aligned.
			 */
			width += (gtk_text_view_get_left_margin (GTK_TEXT_VIEW (map)) * 2);

			*minimum = *natural = width;
		}
	}
	else if (orientation == GTK_ORIENTATION_VERTICAL)
	{
		GTK_WIDGET_CLASS (gtk_source_map_parent_class)->measure (widget,
		                                                         orientation,
		                                                         for_size,
		                                                         minimum,
		                                                         natural,
		                                                         minimum_baseline,
		                                                         natural_baseline);
		*minimum = *natural = 0;
	}
}

/*
 * This scrolls using buffer coordinates.
 * Translate your event location to a buffer coordinate before
 * calling this function.
 */
static void
scroll_to_child_point (GtkSourceMap *map,
                       double        x,
                       double        y)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view != NULL)
	{
		GtkTextIter iter;

		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (map), &iter, x, y);
		_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (priv->view), &iter,
		                               0.0, TRUE, 1.0, 0.5);
	}
}

static gboolean
scale_margin (GBinding *binding,
		const GValue *source_value,
		GValue *target_value,
		gpointer user_data)
{
	int source_value_int = g_value_get_int (source_value);
	int scaled_margin = source_value_int / 4.35;
	g_value_set_int (target_value, scaled_margin);
	return TRUE;
}

static void
connect_view (GtkSourceMap  *map,
              GtkSourceView *view)
{
	GtkSourceMapPrivate *priv;
	GtkAdjustment *vadj;

	priv = gtk_source_map_get_instance_private (map);

	priv->view = view;
	g_object_add_weak_pointer (G_OBJECT (view), (gpointer *)&priv->view);

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));

	priv->buffer_binding =
		g_object_bind_property (view, "buffer",
		                        map, "buffer",
		                        G_BINDING_SYNC_CREATE);
	g_object_add_weak_pointer (G_OBJECT (priv->buffer_binding),
	                           (gpointer *)&priv->buffer_binding);

	priv->indent_width_binding =
		g_object_bind_property (view, "indent-width",
		                        map, "indent-width",
		                        G_BINDING_SYNC_CREATE);
	g_object_add_weak_pointer (G_OBJECT (priv->indent_width_binding),
	                           (gpointer *)&priv->indent_width_binding);

	priv->tab_width_binding =
		g_object_bind_property (view, "tab-width",
		                        map, "tab-width",
		                        G_BINDING_SYNC_CREATE);
	g_object_add_weak_pointer (G_OBJECT (priv->tab_width_binding),
	                           (gpointer *)&priv->tab_width_binding);

	priv->bottom_margin_binding =
		g_object_bind_property_full (view, "bottom-margin",
		                             map, "bottom-margin",
		                             G_BINDING_SYNC_CREATE,
		                             scale_margin,
		                             NULL,
		                             NULL,
		                             NULL);

	g_object_add_weak_pointer (G_OBJECT (priv->bottom_margin_binding),
				   (gpointer *)&priv->bottom_margin_binding);

	priv->top_margin_binding =
		g_object_bind_property_full (view, "top-margin",
		                             map, "top-margin",
		                             G_BINDING_SYNC_CREATE,
		                             scale_margin,
		                             NULL,
		                             NULL,
		                             NULL);

	g_object_add_weak_pointer (G_OBJECT (priv->top_margin_binding),
				   (gpointer *)&priv->top_margin_binding);

	priv->view_notify_buffer_handler =
		g_signal_connect_object (view,
		                         "notify::buffer",
		                         G_CALLBACK (view_notify_buffer),
		                         map,
		                         G_CONNECT_SWAPPED);
	view_notify_buffer (map, NULL, view);

	priv->view_notify_right_margin_position_handler =
		g_signal_connect_object (view,
		                         "notify::right-margin-position",
		                         G_CALLBACK (gtk_widget_queue_resize),
		                         map,
		                         G_CONNECT_SWAPPED);

	priv->view_vadj_value_changed_handler =
		g_signal_connect_object (vadj,
		                         "value-changed",
		                         G_CALLBACK (view_vadj_value_changed),
		                         map,
		                         G_CONNECT_SWAPPED);

	priv->view_vadj_notify_upper_handler =
		g_signal_connect_object (vadj,
		                         "notify::upper",
		                         G_CALLBACK (view_vadj_notify_upper),
		                         map,
		                         G_CONNECT_SWAPPED);

	/* If we are not visible, we want to block certain signal handlers */
	if (!gtk_widget_get_visible (GTK_WIDGET (map)))
	{
		g_signal_handler_block (vadj, priv->view_vadj_value_changed_handler);
		g_signal_handler_block (vadj, priv->view_vadj_notify_upper_handler);
	}

	gtk_source_map_rebuild_css (map);
}

static void
disconnect_view (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkAdjustment *vadj;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		return;
	}

	disconnect_buffer (map);

	if (priv->buffer_binding != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->buffer_binding),
		                              (gpointer *)&priv->buffer_binding);
		g_binding_unbind (priv->buffer_binding);
		priv->buffer_binding = NULL;
	}

	if (priv->indent_width_binding != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->indent_width_binding),
		                              (gpointer *)&priv->indent_width_binding);
		g_binding_unbind (priv->indent_width_binding);
		priv->indent_width_binding = NULL;
	}

	if (priv->tab_width_binding != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->tab_width_binding),
		                              (gpointer *)&priv->tab_width_binding);
		g_binding_unbind (priv->tab_width_binding);
		priv->tab_width_binding = NULL;
	}

	if (priv->bottom_margin_binding != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->bottom_margin_binding),
		                              (gpointer *)&priv->bottom_margin_binding);
		g_binding_unbind (priv->bottom_margin_binding);
		priv->bottom_margin_binding = NULL;
	}

	if (priv->top_margin_binding != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->top_margin_binding),
		                              (gpointer *)&priv->top_margin_binding);
		g_binding_unbind (priv->top_margin_binding);
		priv->top_margin_binding = NULL;
	}

	g_clear_signal_handler (&priv->view_notify_buffer_handler, priv->view);
	g_clear_signal_handler (&priv->view_notify_right_margin_position_handler, priv->view);

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));
	if (vadj != NULL)
	{
		g_signal_handler_disconnect (vadj, priv->view_vadj_value_changed_handler);
		priv->view_vadj_value_changed_handler = 0;

		g_signal_handler_disconnect (vadj, priv->view_vadj_notify_upper_handler);
		priv->view_vadj_notify_upper_handler = 0;
	}

	g_object_remove_weak_pointer (G_OBJECT (priv->view), (gpointer *)&priv->view);
	priv->view = NULL;
}

static void
gtk_source_map_dispose (GObject *object)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (object);
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	disconnect_buffer (map);
	disconnect_view (map);

	g_clear_object (&priv->css_provider);
	g_clear_pointer (&priv->font_desc, pango_font_description_free);

	if (priv->slider)
	{
		gtk_widget_unparent (GTK_WIDGET (priv->slider));
		priv->slider = NULL;
	}

	G_OBJECT_CLASS (gtk_source_map_parent_class)->dispose (object);
}

static void
gtk_source_map_snapshot_layer (GtkTextView      *text_view,
			       GtkTextViewLayer  layer,
			       GtkSnapshot      *snapshot)
{
	g_assert (GTK_SOURCE_IS_MAP (text_view));
	g_assert (GTK_IS_SNAPSHOT (snapshot));

	/* We avoid chaining up to draw layers from GtkSourceView. The details are
	 * too small to see and significantly slow down rendering.
	 */
}

static void
gtk_source_map_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (object);
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	switch (prop_id)
	{
		case PROP_FONT_DESC:
			g_value_set_boxed (value, priv->font_desc);
			break;

		case PROP_VIEW:
			g_value_set_object (value, gtk_source_map_get_view (map));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_map_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			gtk_source_map_set_view (map, g_value_get_object (value));
			break;

		case PROP_FONT_DESC:
			gtk_source_map_set_font_desc (map, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_map_drag_update (GtkSourceMap   *map,
                            double          x,
                            double          y,
                            GtkGestureDrag *drag)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GtkTextBuffer *buffer;
	GdkRectangle area;
	GtkTextIter iter;
	double yratio;
	double begin_x;
	double begin_y;
	int ignored;
	int real_height;
	int height;
	int widget_height;

	if (!priv->reached_drag_threshold && ABS (y) < DRAG_THRESHOLD)
	{
		return;
	}

	priv->reached_drag_threshold = TRUE;

	widget_height = gtk_widget_get_height (GTK_WIDGET (map));

	gtk_gesture_drag_get_start_point (drag, &begin_x, &begin_y);
	y = CLAMP (ceil (begin_y + y), 0, widget_height);

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->measure (GTK_WIDGET (map),
	                                                         GTK_ORIENTATION_VERTICAL,
	                                                         gtk_widget_get_width (GTK_WIDGET (map)),
	                                                         &ignored, &real_height, &ignored, &ignored);

	height = MIN (real_height, widget_height) - gtk_text_view_get_bottom_margin (GTK_TEXT_VIEW (map));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (map));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (map), &iter, &area);

	yratio = (y - priv->slider_y_shift) / (double)height;

	scroll_to_child_point (map, 0, real_height * yratio);
}

static void
gtk_source_map_drag_begin (GtkSourceMap   *map,
                           double          start_x,
                           double          start_y,
                           GtkGestureDrag *drag)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GtkWidget *slider_widget = GTK_WIDGET (priv->slider);
	GtkWidget *map_widget = GTK_WIDGET (map);
	double slider_y;
	double slider_height;
	graphene_rect_t bounds;

	priv->reached_drag_threshold = FALSE;
	gtk_gesture_set_state (GTK_GESTURE (drag), GTK_EVENT_SEQUENCE_CLAIMED);
	gtk_source_map_drag_update (map, 0, 0, drag);

	/* Check if the cursor is inside the slider, if true shift it by the cursor
	 * position relative to it, so the cursor stays in the same position.
	 * Otherwise shift the slider by half it's width
	 */
	if (gtk_widget_compute_bounds (slider_widget, map_widget, &bounds))
	{
		slider_y = bounds.origin.y;
		slider_height = bounds.size.height;

		if (start_y >= slider_y && start_y <= slider_y + slider_height)
		{
			priv->slider_y_shift = start_y - slider_y;
		}
		else
		{
			priv->slider_y_shift = slider_height / 2;
		}
	}
	else
	{
		priv->slider_y_shift = 0;
	}

	gtk_widget_add_css_class (slider_widget, "dragging");
}

static void
gtk_source_map_drag_end (GtkSourceMap   *map,
                         double          start_x,
                         double          start_y,
                         GtkGestureDrag *drag)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	gtk_widget_remove_css_class (GTK_WIDGET (priv->slider), "dragging");
}

static void
gtk_source_map_click_pressed (GtkSourceMap *map,
                              int           n_presses,
                              double        x,
                              double        y,
                              GtkGesture   *click)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GtkWidget *slider_widget = GTK_WIDGET(priv->slider);
	GtkWidget *map_widget = GTK_WIDGET(map);
	double slider_y;
	double slider_height;
	graphene_rect_t bounds;
	GtkTextIter iter;
	int buffer_x, buffer_y;

	g_assert (GTK_SOURCE_IS_MAP (map));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	if (priv->view == NULL)
	{
		return;
	}

	/* If the cursor is inside the slider do nothing */

	if (gtk_widget_compute_bounds (slider_widget, map_widget, &bounds))
	{
		slider_y = bounds.origin.y;
		slider_height = bounds.size.height;

		if (y >= slider_y && y <= slider_y + slider_height)
		{
			/* Don't allow click-through to context menu */
			gtk_gesture_set_state (click, GTK_EVENT_SEQUENCE_CLAIMED);
			return;
		}
	}

	gtk_gesture_set_state (click, GTK_EVENT_SEQUENCE_CLAIMED);

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (map),
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       x, y, &buffer_x, &buffer_y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (map), &iter, 0, buffer_y);
	gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->view), &iter,
	                              0.0, TRUE, 1.0, 0.5);
}

static gboolean
gtk_source_map_scroll (GtkWidget *widget,
                       gdouble    x,
                       gdouble    y)
{
	static const gint scroll_acceleration = 12;
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	/*
	 * FIXME:
	 *
	 * This doesn't propagate kinetic scrolling or anything.
	 * We should probably make something that does that.
	 */
	if (priv->view != NULL)
	{
		gint count = 0;

		if (y > 0)
		{
			count = scroll_acceleration;
		}
		else if (y < 0)
		{
			count = -scroll_acceleration;
		}

		if (count != 0)
		{
			g_signal_emit_by_name (priv->view, "move-viewport", GTK_SCROLL_STEPS, count);
			return GDK_EVENT_STOP;
		}
	}

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_map_state_flags_changed (GtkWidget     *widget,
                                    GtkStateFlags  flags)
{
	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->state_flags_changed (widget, flags);

	gtk_widget_set_cursor (widget, NULL);
}

static void
gtk_source_map_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->realize (widget);

	gtk_widget_set_cursor (widget, NULL);
}

static void
gtk_source_map_show (GtkWidget *widget)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;
	GtkAdjustment *vadj;

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->show (widget);

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view != NULL)
	{
		vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));

		g_signal_handler_unblock (vadj, priv->view_vadj_value_changed_handler);
		g_signal_handler_unblock (vadj, priv->view_vadj_notify_upper_handler);

		g_object_notify (G_OBJECT (vadj), "upper");
		g_signal_emit_by_name (vadj, "value-changed");
	}
}

static void
gtk_source_map_hide (GtkWidget *widget)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;
	GtkAdjustment *vadj;

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->hide (widget);

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view != NULL)
	{
		vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));
		g_signal_handler_block (vadj, priv->view_vadj_value_changed_handler);
		g_signal_handler_block (vadj, priv->view_vadj_notify_upper_handler);
	}
}

static void
gtk_source_map_css_changed (GtkWidget         *widget,
                            GtkCssStyleChange *change)
{
	g_assert (GTK_IS_WIDGET (widget));

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->css_changed (widget, change);

#if GTK_CHECK_VERSION(4,3,1)
	{
		PangoContext *rtl_context;
		PangoContext *ltr_context;

		/* Ensure rounding so that BuilderBlocks aligns properly within
		 * rounding errors between glyphs.
		 */

		rtl_context = gtk_text_view_get_rtl_context (GTK_TEXT_VIEW (widget));
		ltr_context = gtk_text_view_get_ltr_context (GTK_TEXT_VIEW (widget));

		pango_context_set_round_glyph_positions (rtl_context, TRUE);
		pango_context_set_round_glyph_positions (ltr_context, TRUE);
	}
#endif
}

static void
gtk_source_map_size_allocate (GtkWidget *widget,
                              int        width,
                              int        height,
                              int        baseline)
{
	GtkSourceMap *map = (GtkSourceMap *)widget;

	g_assert (GTK_SOURCE_IS_MAP (map));

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->size_allocate (widget, width, height, baseline);

	gtk_source_map_allocate_slider (map);
}

static void
gtk_source_map_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
	GtkSourceMap *map = (GtkSourceMap *)widget;
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	g_assert (GTK_IS_WIDGET (widget));
	g_assert (GTK_IS_SNAPSHOT (snapshot));

	/* Render the slider behind the contents so they are more legible and
	 * we can avoid an RGBA blend on top of the contents.
	 */
	gtk_widget_snapshot_child (GTK_WIDGET (map), GTK_WIDGET (priv->slider), snapshot);

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->snapshot (widget, snapshot);
}

static void
gtk_source_map_constructed (GObject *object)
{
	PangoFontMap *font_map;

	G_OBJECT_CLASS (gtk_source_map_parent_class)->constructed (object);

	font_map = _gtk_source_utils_get_builder_blocks ();

	if (font_map != NULL)
	{
		gtk_widget_set_font_map (GTK_WIDGET (object), font_map);
	}
}

static void
gtk_source_map_class_init (GtkSourceMapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkTextViewClass *text_view_class = GTK_TEXT_VIEW_CLASS (klass);

	object_class->constructed = gtk_source_map_constructed;
	object_class->dispose = gtk_source_map_dispose;
	object_class->get_property = gtk_source_map_get_property;
	object_class->set_property = gtk_source_map_set_property;

	widget_class->measure = gtk_source_map_measure;
	widget_class->hide = gtk_source_map_hide;
	widget_class->show = gtk_source_map_show;
	widget_class->state_flags_changed = gtk_source_map_state_flags_changed;
	widget_class->realize = gtk_source_map_realize;
	widget_class->css_changed = gtk_source_map_css_changed;
	widget_class->size_allocate = gtk_source_map_size_allocate;
	widget_class->snapshot = gtk_source_map_snapshot;

	text_view_class->snapshot_layer = gtk_source_map_snapshot_layer;

	properties[PROP_VIEW] =
		g_param_spec_object ("view",
		                     "View",
		                     "The view this widget is mapping.",
		                     GTK_SOURCE_TYPE_VIEW,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties[PROP_FONT_DESC] =
		g_param_spec_boxed ("font-desc",
		                    "Font Description",
		                    "The Pango font description to use.",
		                    PANGO_TYPE_FONT_DESCRIPTION,
		                    (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gtk_source_map_init (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkSourceCompletion *completion;
	GtkEventController *scroll;
	GtkEventController *press;
	GtkGesture *drag;

	priv = gtk_source_map_get_instance_private (map);

	gtk_widget_add_css_class (GTK_WIDGET (map), "GtkSourceMap");

	priv->css_provider = gtk_css_provider_new ();
        priv->slider = g_object_new (GTK_SOURCE_TYPE_MAP_SLIDER,
				     "width-request", 1,
				     "height-request", 1,
				     NULL);
	gtk_widget_set_parent (GTK_WIDGET (priv->slider), GTK_WIDGET (map));

	_gtk_source_widget_add_css_provider (GTK_WIDGET (map),
	                                     priv->css_provider,
	                                     GTK_SOURCE_STYLE_PROVIDER_PRIORITY + 1);
	_gtk_source_widget_add_css_provider (GTK_WIDGET (priv->slider),
	                                     priv->css_provider,
	                                     GTK_SOURCE_STYLE_PROVIDER_PRIORITY + 1);

	g_object_set (map,
	              "auto-indent", FALSE,
	              "can-focus", FALSE,
	              "editable", FALSE,
	              "hexpand", FALSE,
	              "vexpand", FALSE,
	              "monospace", TRUE,
	              "show-right-margin", FALSE,
	              "visible", TRUE,
	              NULL);

	completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (map));
	gtk_source_completion_block_interactive (completion);

	gtk_source_map_set_font_name (map, "BuilderBlocks");

	drag = gtk_gesture_drag_new ();
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (drag),
                                                    GTK_PHASE_CAPTURE);
	g_signal_connect_swapped (drag,
	                          "drag-begin",
	                          G_CALLBACK (gtk_source_map_drag_begin),
	                          map);
	g_signal_connect_swapped (drag,
	                          "drag-end",
	                          G_CALLBACK (gtk_source_map_drag_end),
	                          map);
	g_signal_connect_swapped (drag,
	                          "drag-update",
	                          G_CALLBACK (gtk_source_map_drag_update),
	                          map);
	gtk_widget_add_controller (GTK_WIDGET (map),
	                           GTK_EVENT_CONTROLLER (g_steal_pointer (&drag)));

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	gtk_event_controller_set_propagation_phase (scroll, GTK_PHASE_CAPTURE);
	g_signal_connect_swapped (scroll,
	                          "scroll",
	                          G_CALLBACK (gtk_source_map_scroll),
	                          map);
	gtk_widget_add_controller (GTK_WIDGET (map), g_steal_pointer (&scroll));

	press = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (press), 0);
	g_signal_connect_swapped (press,
	                          "pressed",
	                          G_CALLBACK (gtk_source_map_click_pressed),
	                          map);
	gtk_event_controller_set_propagation_phase (press, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller (GTK_WIDGET (map), press);
}

/**
 * gtk_source_map_new:
 *
 * Creates a new `GtkSourceMap`.
 *
 * Returns: a new #GtkSourceMap.
 */
GtkWidget *
gtk_source_map_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_MAP, NULL);
}

/**
 * gtk_source_map_set_view:
 * @map: a #GtkSourceMap
 * @view: a #GtkSourceView
 *
 * Sets the view that @map will be doing the mapping to.
 */
void
gtk_source_map_set_view (GtkSourceMap  *map,
                         GtkSourceView *view)
{
	GtkSourceMapPrivate *priv;

	g_return_if_fail (GTK_SOURCE_IS_MAP (map));
	g_return_if_fail (view == NULL || GTK_SOURCE_IS_VIEW (view));

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == view)
	{
		return;
	}

	if (priv->view != NULL)
	{
		disconnect_view (map);
	}

	if (view != NULL)
	{
		connect_view (map, view);
	}

	g_object_notify_by_pspec (G_OBJECT (map), properties[PROP_VIEW]);
}

/**
 * gtk_source_map_get_view:
 * @map: a #GtkSourceMap.
 *
 * Gets the [property@Map:view] property, which is the view this widget is mapping.
 *
 * Returns: (transfer none) (nullable): a #GtkSourceView or %NULL.
 */
GtkSourceView *
gtk_source_map_get_view (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;

	g_return_val_if_fail (GTK_SOURCE_IS_MAP (map), NULL);

	priv = gtk_source_map_get_instance_private (map);

	return priv->view;
}

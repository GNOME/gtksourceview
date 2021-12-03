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

#if ENABLE_FONT_CONFIG
# include <fontconfig/fontconfig.h>
# include <pango/pangocairo.h>
# include <pango/pangofc-fontmap.h>
#endif

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

	/* Weak pointer view to child view bindings */
	GBinding *buffer_binding;
	GBinding *indent_width_binding;
	GBinding *tab_width_binding;

	/* Our signal handler for buffer changes */
	gulong view_notify_buffer_handler;
	gulong view_vadj_value_changed_handler;
	gulong view_vadj_notify_upper_handler;

	/* Signals connected indirectly to the buffer */
	gulong buffer_notify_style_scheme_handler;

	/* Denotes if we are in a grab from button press */
	guint in_press : 1;

	/* If we failed to locate a color for the slider, then this will
	 * be set to 0 and that means we need to apply the "selection"
	 * class when drawing so that we get an appropriate color.
	 */
	guint had_color : 1;

	/* If we dragged enough to reach a drag threshold */
	guint reached_drag_threshold : 1;
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

#if ENABLE_FONT_CONFIG
static FcConfig *map_font_config;

static void
load_override_font (GtkSourceMap *map)
{
	PangoFontDescription *font_desc;
	PangoFontMap *font_map;

	if (g_once_init_enter (&map_font_config))
	{
		const gchar *font_path = PACKAGE_DATADIR"/fonts/BuilderBlocks.ttf";
		FcConfig *config = FcInitLoadConfigAndFonts ();

		if (!g_file_test (font_path, G_FILE_TEST_IS_REGULAR))
			g_debug ("\"%s\" is missing or inaccessible", font_path);

		FcConfigAppFontAddFile (config, (const FcChar8 *)font_path);

		g_once_init_leave (&map_font_config, config);
	}

	font_map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
	pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (font_map), map_font_config);
	gtk_widget_set_font_map (GTK_WIDGET (map), font_map);
	font_desc = pango_font_description_from_string ("BuilderBlocks");

	g_assert (map_font_config != NULL);
	g_assert (font_map != NULL);
	g_assert (font_desc != NULL);

	g_object_set (map, "font-desc", font_desc, NULL);

	pango_font_description_free (font_desc);
	g_object_unref (font_map);
}
#endif

static void
get_slider_position (GtkSourceMap *map,
                     int           width,
                     int           height,
                     GdkRectangle *slider_area)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GdkRectangle them_visible_rect, us_visible_rect;
	GdkRectangle us_alloc;
	GtkStyleContext *style_context;
	GtkTextBuffer *buffer;
	GtkTextIter end_iter;
	GdkRectangle end_rect;
	GtkBorder border;
	int us_height;
	int them_height;

	if (priv->view == NULL)
	{
		return;
	}

	gtk_widget_get_allocation (GTK_WIDGET (map), &us_alloc);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (map));
	style_context = gtk_widget_get_style_context (GTK_WIDGET (map));
	gtk_style_context_get_border (style_context, &border);

	gtk_text_buffer_get_end_iter (buffer, &end_iter);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (map), &end_iter, &end_rect);
	us_height = end_rect.y + end_rect.height;
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->view), &end_iter, &end_rect);
	them_height = end_rect.y + end_rect.height;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (priv->view), &them_visible_rect);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (map), &us_visible_rect);

	slider_area->x = 0;
	slider_area->width = us_alloc.width - border.left - border.right;
	slider_area->height = 100;
	slider_area->y = (double)them_visible_rect.y / (double)them_height * (double)us_height;
	slider_area->height = ((double)(them_visible_rect.y + them_visible_rect.height) / (double)them_height * (double)us_height) - slider_area->y;

	slider_area->y -= us_visible_rect.y;
}

static void
gtk_source_map_rebuild_css (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkSourceStyleScheme *style_scheme;
	GtkSourceStyle *style = NULL;
	GtkTextBuffer *buffer;
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

	if (style_scheme != NULL)
	{
		if (!(style = gtk_source_style_scheme_get_style (style_scheme, "map-overlay")) &&
		    !(style = gtk_source_style_scheme_get_style (style_scheme, "selection")) &&
		    !(style = gtk_source_style_scheme_get_style (style_scheme, "current-line")))
		{
			/* Do Nothing */
		}
	}

	if (style != NULL)
	{
		gboolean foreground_set;
		gboolean background_set;

		g_object_get (style,
		              "foreground", &foreground,
		              "foreground-set", &foreground_set,
		              "background", &background,
		              "background-set", &background_set,
		              NULL);

		if (!foreground_set)
		{
			g_clear_pointer (&foreground, g_free);
		}

		if (!background_set)
		{
			g_clear_pointer (&background, g_free);
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

	priv->had_color = background != NULL;

	if (background != NULL)
	{
		GdkRGBA parsed;

		if (foreground == NULL)
		{
			GtkStyleContext *style_context;
			GdkRGBA color;

			style_context = gtk_widget_get_style_context (GTK_WIDGET (map));
			gtk_style_context_get_color (style_context, &color);
			foreground = gdk_rgba_to_string (&color);
		}

		if (gdk_rgba_parse (&parsed, background))
		{
			if (parsed.alpha < 1.0)
			{
				parsed.alpha = 1.0;
				g_free (background);
				background = gdk_rgba_to_string (&parsed);
			}
		}

		g_string_append_printf (gstr,
		                        "slider {"
		                        " background-color: alpha(%s,.25);"
		                        " transition-duration: 300ms;"
		                        "}\n"
		                        "slider:hover {"
		                        " background-color: alpha(%s,.35);"
		                        "}\n"
		                        "slider.dragging:hover {"
		                        " background-color: alpha(%s,.5);"
		                        "}\n",
		                        background, background, background);
	}

	g_free (background);
	g_free (foreground);

	if (gstr->len > 0)
	{
		/* use -1 to avoid a copy of the string */
		gtk_css_provider_load_from_data (priv->css_provider, gstr->str, -1);
	}

	g_string_free (gstr, TRUE);
}

static void
update_child_vadjustment (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkAdjustment *vadj;
	GtkAdjustment *child_vadj;
	gdouble value;
	gdouble upper;
	gdouble page_size;
	gdouble child_upper;
	gdouble child_page_size;
	gdouble new_value = 0.0;

	priv = gtk_source_map_get_instance_private (map);

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
}

static void
view_vadj_value_changed (GtkSourceMap  *map,
                         GtkAdjustment *vadj)
{
	update_child_vadjustment (map);
	gtk_widget_queue_allocate (GTK_WIDGET (map));
}

static void
view_vadj_notify_upper (GtkSourceMap  *map,
                        GParamSpec    *pspec,
                        GtkAdjustment *vadj)
{
	gtk_widget_queue_allocate (GTK_WIDGET (map));
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
		GtkAllocation alloc;
		GtkTextIter iter;

		gtk_widget_get_allocation (GTK_WIDGET (map), &alloc);
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (map), &iter, x, y);
		_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (priv->view), &iter,
		                               0.0, TRUE, 1.0, 0.5);
	}
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

	priv->view_notify_buffer_handler =
		g_signal_connect_object (view,
		                         "notify::buffer",
		                         G_CALLBACK (view_notify_buffer),
		                         map,
		                         G_CONNECT_SWAPPED);
	view_notify_buffer (map, NULL, view);

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

	if (priv->view_notify_buffer_handler != 0)
	{
		g_signal_handler_disconnect (priv->view, priv->view_notify_buffer_handler);
		priv->view_notify_buffer_handler = 0;
	}

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
	GtkAllocation alloc;
	GdkRectangle area;
	GtkTextIter iter;
	double yratio;
	double begin_x;
	double begin_y;
	int ignored;
	int real_height;
	int height;

	if (!priv->reached_drag_threshold && ABS (y) < DRAG_THRESHOLD)
	{
		return;
	}

	priv->reached_drag_threshold = TRUE;

	gtk_widget_get_allocation (GTK_WIDGET (map), &alloc);
	gtk_gesture_drag_get_start_point (drag, &begin_x, &begin_y);
	y = CLAMP (ceil (begin_y + y), 0, alloc.height);

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->measure (GTK_WIDGET (map),
								 GTK_ORIENTATION_VERTICAL,
								 gtk_widget_get_width (GTK_WIDGET (map)),
								 &ignored, &real_height, &ignored, &ignored);

	height = MIN (real_height, alloc.height);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (map));
	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (map), &iter, &area);

	yratio = y / (double)height;

	scroll_to_child_point (map, 0, real_height * yratio);
}

static void
gtk_source_map_drag_begin (GtkSourceMap   *map,
                           double          start_x,
                           double          start_y,
                           GtkGestureDrag *drag)
{
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	priv->reached_drag_threshold = FALSE;

	gtk_gesture_set_state (GTK_GESTURE (drag), GTK_EVENT_SEQUENCE_CLAIMED);
	gtk_source_map_drag_update (map, 0, 0, drag);

	gtk_widget_add_css_class (GTK_WIDGET (priv->slider), "dragging");
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
	GtkTextIter iter;
	int buffer_x, buffer_y;

	g_assert (GTK_SOURCE_IS_MAP (map));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	if (priv->view == NULL)
	{
		return;
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
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);
	GdkRectangle area;
	int min, nat;

	g_assert (GTK_SOURCE_IS_MAP (map));

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->size_allocate (widget, width, height, baseline);

	get_slider_position (map, width, height, &area);
	gtk_widget_measure (GTK_WIDGET (priv->slider),
	                    GTK_ORIENTATION_VERTICAL,
			    width, &min, &nat, NULL, NULL);
	area.height = MAX (nat, area.height);
	gtk_widget_size_allocate (GTK_WIDGET (priv->slider), &area, -1);
}

static void
gtk_source_map_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
	GtkSourceMap *map = (GtkSourceMap *)widget;
	GtkSourceMapPrivate *priv = gtk_source_map_get_instance_private (map);

	g_assert (GTK_IS_WIDGET (widget));
	g_assert (GTK_IS_SNAPSHOT (snapshot));

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->snapshot (widget, snapshot);

	gtk_widget_snapshot_child (GTK_WIDGET (map), GTK_WIDGET (priv->slider), snapshot);
}

static void
gtk_source_map_constructed (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_map_parent_class)->constructed (object);

#if ENABLE_FONT_CONFIG
	load_override_font (GTK_SOURCE_MAP (object));
#endif
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

	gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (map)),
	                                GTK_STYLE_PROVIDER (priv->css_provider),
	                                GTK_SOURCE_STYLE_PROVIDER_PRIORITY + 1);
	gtk_style_context_add_provider (gtk_widget_get_style_context (GTK_WIDGET (priv->slider)),
	                                GTK_STYLE_PROVIDER (priv->css_provider),
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

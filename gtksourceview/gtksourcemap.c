/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcemap.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Ignacio Casal Quinteiro <icq@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "gtksourcemap.h"
#include "gtksourceview-utils.h"

#include <glib/gi18n.h>

/**
 * SECTION:map
 * @Short_description: Widget that displays a map for a specific #GtkSourceView
 * @Title: GtkSourceMap
 * @See_also: #GtkSourceView
 *
 * #GtkSourceMap is a widget that maps the content of a #GtkSourceView into
 * a smaller view so the user can have a quick overview of the whole document.
 *
 * Call gtk_source_map_set_view() or set the #GtkSourceMap:view property
 * to set the view that the #GtkSourceMap should do the mapping from.
 *
 * #GtkSourceMap uses another #GtkSourceView object to do the mapping. It can be
 * retrieved with gtk_source_map_get_child_view() in order to extend it. For
 * example, one could extend it by adding a gutter that shows the changes done
 * while editing the document.
 *
 * #GtkSourceMap derives from #GtkBin only as an implementation detail. Applications
 * should not use any API specific to #GtkBin to operate on this object. It should
 * be treated as a normal #GtkWidget.
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
 *   - Performance for this type of widget is dominated by text layout
 *     rendering. When you scale out this car, you increase the number of
 *     layouts to be rendered greatly.
 *
 *   - We can pack GtkSourceGutterRenderer into the child view to provide
 *     additional information. This is quite handy to show information such
 *     as errors, line changes, and anything else that can help the user
 *     quickly jump to the target location.
 *
 * I also tried drawing the contents of the GtkSourceView onto a widget after
 * performing a cairo_scale(). This does not help us much because we ignore
 * pixel cache when cair_scale is not 1-to-1. This results in layout
 * invalidation and worst case render paths.
 *
 * I also tried rendering the scrubber (overlay box) during the
 * GtkTextView::draw_layer() vfunc. The problem with this approach is that
 * the scrubber contents are actually pixel cached. So every time the scrubber
 * moves we have to invalidate the GtkTextLayout and redraw cached contents.
 * Where as using an overlay widget allows us to simply perform an alpha blend
 * during the composite phase. The pixel cache'd GtkSourceView contents remain
 * unchanged, and there continue to essentially be a memcpy().
 *
 * In the future, we might consider bundling a custom font for the source map.
 * Other overview maps have used a "block" font. However, they typically do
 * that because of the glyph rendering cost. Since we have pixel cache, that
 * deficiency is largely a non-issue. But Pango recently got support for
 * embedding fonts in the application, so it is at least possible to bundle
 * our own font.
 *
 * -- Christian
 */

#define DEFAULT_WIDTH        100
#define CONCEAL_TIMEOUT      2000

typedef struct
{
	PangoFontDescription *font_desc;

	/*
	 * We use a separate CSS provider for the view (controlling the font)
	 * and the overlay box due to the inability to get both styled
	 * correctly from the same CSS provider. If someone can determine
	 * the cause of this, we should merge them into a single provider.
	 */
	GtkCssProvider *view_css_provider;
	GtkCssProvider *box_css_provider;

	/*
	 * We use an overlay to place the scrubber above the source view.
	 * Doing so allows us to composite the two widgets together instead
	 * invalidating the child_view's pixel cache by rendering during its
	 * draw_layer() vfunc.
	 */
	GtkOverlay *overlay;

	/*
	 * This is our minimap widget. We adjust the font to be a 1pt font
	 * but otherwise try to render things as close to the same as the
	 * view we are providing a map of. We do not render the background
	 * grid as it requires a lot of cpu time and isn't visible anyway.
	 * The width of the view is controlled by "right-margin-position"
	 * multiplied by the size of a single character. Content that wraps
	 * beyond right-margin-position is not displayed.
	 */
	GtkSourceView *child_view;

	/*
	 * overlay_box is our scrubber widget. It is the translucent overlay
	 * that the user can grab and drag to perform an accelerated scroll.
	 */
	GtkEventBox *overlay_box;

	/* The GtkSourceView we are providing a map of */
	GtkSourceView *view;

	gboolean in_press : 1;
} GtkSourceMapPrivate;

enum
{
	PROP_0,
	PROP_VIEW,
	PROP_FONT_DESC,
	LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceMap, gtk_source_map, GTK_TYPE_BIN)

static GParamSpec *pspecs[LAST_PROP];

static void
gtk_source_map_rebuild_css (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkSourceStyleScheme *style_scheme;
	GtkTextBuffer *buffer;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		return;
	}

	if (priv->font_desc != NULL)
	{
		gchar *css;
		gchar *tmp;

		tmp = _gtk_source_pango_font_description_to_css (priv->font_desc);
		css = g_strdup_printf ("GtkSourceView { %s }\n", tmp != NULL ? tmp : "");
		gtk_css_provider_load_from_data (priv->view_css_provider, css, -1, NULL);
		g_free (css);
		g_free (tmp);
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->view));
	style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

	if (style_scheme != NULL)
	{
		gchar *background = NULL;
		GtkSourceStyle *style;

		style = gtk_source_style_scheme_get_style (style_scheme, "map-overlay");
		if (style == NULL)
		{
			GtkStyleContext *context;
			GdkRGBA color;

			context = gtk_widget_get_style_context (GTK_WIDGET (priv->view));
			gtk_style_context_save (context);
			gtk_style_context_add_class (context, "view");
			gtk_style_context_get_background_color (context,
			                                        GTK_STATE_FLAG_SELECTED,
			                                        &color);
			gtk_style_context_restore (context);
			background = gdk_rgba_to_string (&color);
		}
		else
		{
			g_object_get (style,
			              "background", &background,
			              NULL);
		}

		if (background != NULL)
		{
			gchar *css;

			css = g_strdup_printf ("GtkSourceMap GtkEventBox {\n"
			                       "\tbackground-color: %s;\n"
			                       "\topacity: 0.75;\n"
			                       "\tborder-top: 1px solid shade(%s,0.9);\n"
			                       "\tborder-bottom: 1px solid shade(%s,0.9);\n"
			                       "}\n",
			                       background,
			                       background,
			                       background);

			gtk_css_provider_load_from_data (priv->box_css_provider, css, -1, NULL);
			g_free (css);
		}

		g_free (background);
	}
}

static void
update_scrubber_height (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkAllocation alloc;
	gdouble ratio;
	gint child_height;
	gint view_height;

	priv = gtk_source_map_get_instance_private (map);

	gtk_widget_get_allocation (GTK_WIDGET (priv->view), &alloc);
	gtk_widget_get_preferred_height (GTK_WIDGET (priv->view),
	                                 NULL, &view_height);
	gtk_widget_get_preferred_height (GTK_WIDGET (priv->child_view),
	                                 NULL, &child_height);

	ratio = alloc.height / (gdouble)view_height;
	child_height *= ratio;

	if (child_height > 0)
	{
		g_object_set (priv->overlay_box,
		              "height-request", child_height,
		              NULL);
	}
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
	gdouble child_value;
	gdouble child_upper;
	gdouble child_page_size;
	gdouble new_value = 0.0;

	priv = gtk_source_map_get_instance_private (map);

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));
	child_vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->child_view));

	g_object_get (vadj,
	              "upper", &upper,
	              "value", &value,
	              "page-size", &page_size,
	              NULL);

	g_object_get (child_vadj,
	              "upper", &child_upper,
	              "value", &child_value,
	              "page-size", &child_page_size,
	              NULL);

	/*
	 * TODO: Technically we should take into account lower here, but in practice
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
	GtkSourceMapPrivate *priv;
	gdouble page_size;
	gdouble upper;
	gdouble lower;

	priv = gtk_source_map_get_instance_private (map);

	gtk_widget_queue_resize (GTK_WIDGET (priv->overlay_box));

	g_object_get (vadj,
	              "lower", &lower,
	              "page-size", &page_size,
	              "upper", &upper,
	              NULL);

	update_child_vadjustment (map);
}

static void
view_vadj_notify_upper (GtkSourceMap  *map,
                        GParamSpec    *pspec,
                        GtkAdjustment *vadj)
{
	update_scrubber_height (map);
}

static void
buffer_notify_style_scheme (GtkSourceMap  *map,
                            GParamSpec    *pspec,
                            GtkTextBuffer *buffer)
{
	gtk_source_map_rebuild_css (map);
}

static void
view_notify_buffer (GtkSourceMap  *map,
                    GParamSpec    *pspec,
                    GtkSourceView *view)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_signal_connect_object (buffer,
	                         "notify::style-scheme",
	                         G_CALLBACK (buffer_notify_style_scheme),
	                         map,
	                         G_CONNECT_SWAPPED);

	gtk_source_map_rebuild_css (map);
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
gtk_source_map_get_preferred_width (GtkWidget *widget,
                                    gint      *mininum_width,
                                    gint      *natural_width)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;
	PangoLayout *layout;
	guint right_margin_position;
	gint height;
	gint width;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->font_desc == NULL)
	{
		*mininum_width = *natural_width = DEFAULT_WIDTH;
		return;
	}

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->child_view), "X");
	pango_layout_get_pixel_size (layout, &width, &height);
	g_object_unref (layout);

	right_margin_position = gtk_source_view_get_right_margin_position (priv->view);
	width *= right_margin_position;

	*mininum_width = *natural_width = width;
}

static void
gtk_source_map_get_preferred_height (GtkWidget *widget,
                                     gint      *minimum_height,
                                     gint      *natural_height)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		*minimum_height = *natural_height = 0;
		return;
	}

	gtk_widget_get_preferred_height (GTK_WIDGET (priv->child_view),
	                                 minimum_height, natural_height);

	*minimum_height = 0;
}

static gboolean
child_view_button_press_event (GtkSourceMap   *map,
                               GdkEventButton *event,
                               GtkSourceView  *child_view)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view != NULL)
	{
		GtkTextIter iter;
		gint x;
		gint y;

		gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (child_view),
		                                       GTK_TEXT_WINDOW_WIDGET,
		                                       event->x, event->y, &x, &y);
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (child_view),
		                                    &iter, x, y);
		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->view), &iter,
		                              0.0, TRUE, 1.0, 0.5);
	}

	return GDK_EVENT_STOP;
}

static void
child_view_state_flags_changed (GtkWidget     *widget,
                                GtkStateFlags  flags,
                                GtkWidget     *child_view)
{
	GdkWindow *window;

	window = gtk_text_view_get_window (GTK_TEXT_VIEW (child_view),
	                                   GTK_TEXT_WINDOW_TEXT);
	if (window != NULL)
	{
		gdk_window_set_cursor (window, NULL);
	}
}

static void
child_view_realize_after (GtkWidget *widget,
                          GtkWidget *child_view)
{
	child_view_state_flags_changed (widget, 0, child_view);
}

static gboolean
overlay_box_button_press_event (GtkSourceMap   *map,
                                GdkEventButton *event,
                                GtkEventBox    *overlay_box)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	gtk_grab_add (GTK_WIDGET (overlay_box));

	priv->in_press = TRUE;

	return GDK_EVENT_PROPAGATE;
}

static gboolean
overlay_box_button_release_event (GtkSourceMap   *map,
                                  GdkEventButton *event,
                                  GtkEventBox    *overlay_box)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	priv->in_press = FALSE;

	gtk_grab_remove (GTK_WIDGET (overlay_box));

	return GDK_EVENT_PROPAGATE;
}

static gboolean
overlay_box_motion_notify_event (GtkSourceMap   *map,
                                 GdkEventMotion *event,
                                 GtkEventBox    *overlay_box)
{
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->in_press && (priv->view != NULL))
	{
		GtkAllocation alloc;
		GtkAllocation child_alloc;
		GtkTextBuffer *buffer;
		GdkRectangle rect;
		GtkTextIter iter;
		gdouble ratio;
		gint child_height;
		gint x;
		gint y;

		gtk_widget_get_allocation (GTK_WIDGET (overlay_box), &alloc);
		gtk_widget_get_allocation (GTK_WIDGET (priv->child_view), &child_alloc);

		gtk_widget_translate_coordinates (GTK_WIDGET (overlay_box),
		                                  GTK_WIDGET (priv->child_view),
		                                  event->x, event->y, &x, &y);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->child_view));
		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->child_view),
		                                 &iter, &rect);

		child_height = MIN (child_alloc.height, (rect.y + rect.height));

		y = CLAMP (y, child_alloc.y, child_alloc.y + child_height) - child_alloc.y;
		ratio = (gdouble)y / (gdouble)child_height;
		y = (rect.y + rect.height) * ratio;

		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (priv->child_view),
		                                    &iter, x, y);

		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (priv->view), &iter,
		                              0.0, TRUE, 1.0, 0.5);
	}

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_map_size_allocate (GtkWidget     *widget,
                              GtkAllocation *alloc)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->size_allocate (widget, alloc);

	update_scrubber_height (map);
}

static gboolean
gtk_source_map_do_scroll_event (GtkSourceMap   *map,
                                GdkEventScroll *event,
                                GtkWidget      *widget)
{
	GtkSourceMapPrivate *priv;
	static const gint scroll_acceleration = 4;

	priv = gtk_source_map_get_instance_private (map);

	/*
	 * TODO: This doesn't propagate kinetic scrolling or anything.
	 *       We should probably make something that does that.
	 */
	if (priv->view != NULL)
	{
		gdouble x;
		gdouble y;
		gint count = 0;

		if (event->direction == GDK_SCROLL_UP)
		{
			count = -scroll_acceleration;
		}
		else if (event->direction == GDK_SCROLL_DOWN)
		{
			count = scroll_acceleration;
		}
		else
		{
			gdk_event_get_scroll_deltas ((GdkEvent *)event, &x, &y);

			if (y > 0)
			{
				count = scroll_acceleration;
			}
			else if (y < 0)
			{
				count = -scroll_acceleration;
			}
		}

		if (count != 0)
		{
			g_signal_emit_by_name (priv->view, "move-viewport",
			                       GTK_SCROLL_STEPS, count);
		}
	}

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_map_destroy (GtkWidget *widget)
{
	GtkSourceMap *map = GTK_SOURCE_MAP (widget);
	GtkSourceMapPrivate *priv;

	priv = gtk_source_map_get_instance_private (map);

	g_clear_object (&priv->box_css_provider);
	g_clear_object (&priv->view_css_provider);
	g_clear_pointer (&priv->font_desc, pango_font_description_free);

	if (priv->view != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->view),
		                              (gpointer *)&priv->view);
		priv->view = NULL;
	}

	GTK_WIDGET_CLASS (gtk_source_map_parent_class)->destroy (widget);
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
gtk_source_map_class_init (GtkSourceMapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = gtk_source_map_get_property;
	object_class->set_property = gtk_source_map_set_property;

	widget_class->destroy = gtk_source_map_destroy;
	widget_class->get_preferred_height = gtk_source_map_get_preferred_height;
	widget_class->get_preferred_width = gtk_source_map_get_preferred_width;
	widget_class->size_allocate = gtk_source_map_size_allocate;

	pspecs[PROP_VIEW] =
		g_param_spec_object ("view",
		                     _("View"),
		                     _("The view this widget is mapping."),
		                     GTK_SOURCE_TYPE_VIEW,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	pspecs[PROP_FONT_DESC] =
		g_param_spec_boxed ("font-desc",
		                    _("Font Description"),
		                    _("The Pango font description to use."),
		                    PANGO_TYPE_FONT_DESCRIPTION,
		                    (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, LAST_PROP, pspecs);
}

static gboolean
gtk_source_map_get_child_position (GtkOverlay   *overlay,
                                   GtkWidget    *child,
                                   GdkRectangle *alloc,
                                   GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkTextIter iter;
	GdkRectangle visible_area;
	GdkRectangle loc;
	GtkAllocation our_alloc;

	priv = gtk_source_map_get_instance_private (map);

	if (priv->view == NULL)
	{
		return FALSE;
	}

	gtk_widget_get_allocation (GTK_WIDGET (overlay), &our_alloc);

	alloc->x = 0;
	alloc->width = our_alloc.width;

	gtk_widget_get_preferred_height (child, NULL, &alloc->height);

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (priv->view),
	                                &visible_area);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (priv->view),
	                                    &iter, visible_area.x,
	                                    visible_area.y);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (priv->child_view),
	                                 &iter, &loc);
	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (priv->child_view),
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       loc.x, loc.y,
	                                       NULL, &alloc->y);

	return TRUE;
}

static void
gtk_source_map_init (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;
	GtkSourceCompletion *completion;
	GtkStyleContext *context;

	priv = gtk_source_map_get_instance_private (map);

	priv->overlay = GTK_OVERLAY (gtk_overlay_new ());
	gtk_widget_show (GTK_WIDGET (priv->overlay));
	g_signal_connect_object (priv->overlay,
	                         "get-child-position",
	                         G_CALLBACK (gtk_source_map_get_child_position),
	                         map,
	                         0);
	gtk_container_add (GTK_CONTAINER (map), GTK_WIDGET (priv->overlay));

	priv->child_view = g_object_new (GTK_SOURCE_TYPE_VIEW,
	                                 "auto-indent", FALSE,
	                                 "can-focus", FALSE,
	                                 "draw-spaces", 0,
	                                 "editable", FALSE,
	                                 "expand", FALSE,
	                                 "monospace", TRUE,
	                                 "show-line-numbers", FALSE,
	                                 "show-line-marks", FALSE,
	                                 "show-right-margin", FALSE,
	                                 "visible", TRUE,
	                                 NULL);

	g_signal_connect_object (priv->child_view,
	                         "button-press-event",
	                         G_CALLBACK (child_view_button_press_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_events (GTK_WIDGET (priv->child_view), GDK_SCROLL_MASK);
	g_signal_connect_object (priv->child_view,
	                         "scroll-event",
	                         G_CALLBACK (gtk_source_map_do_scroll_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (priv->child_view,
	                         "state-flags-changed",
	                         G_CALLBACK (child_view_state_flags_changed),
	                         map,
	                         G_CONNECT_SWAPPED | G_CONNECT_AFTER);
	g_signal_connect_object (priv->child_view,
	                         "realize",
	                         G_CALLBACK (child_view_realize_after),
	                         map,
	                         G_CONNECT_SWAPPED | G_CONNECT_AFTER);

	priv->view_css_provider = gtk_css_provider_new ();
	context = gtk_widget_get_style_context (GTK_WIDGET (priv->child_view));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (priv->view_css_provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_container_add (GTK_CONTAINER (priv->overlay),
	                   GTK_WIDGET (priv->child_view));

	priv->overlay_box = g_object_new (GTK_TYPE_EVENT_BOX,
	                                  "opacity", 0.5,
	                                  "visible", TRUE,
	                                  "height-request", 10,
	                                  "width-request", 100,
	                                  NULL);
	g_signal_connect_object (priv->overlay_box,
	                         "button-press-event",
	                         G_CALLBACK (overlay_box_button_press_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (priv->overlay_box,
	                         "scroll-event",
	                         G_CALLBACK (gtk_source_map_do_scroll_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (priv->overlay_box,
	                         "button-release-event",
	                         G_CALLBACK (overlay_box_button_release_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (priv->overlay_box,
	                         "motion-notify-event",
	                         G_CALLBACK (overlay_box_motion_notify_event),
	                         map,
	                         G_CONNECT_SWAPPED);
	context = gtk_widget_get_style_context (GTK_WIDGET (priv->overlay_box));
	priv->box_css_provider = gtk_css_provider_new ();
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (priv->box_css_provider),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_overlay_add_overlay (GTK_OVERLAY (priv->overlay),
	                         GTK_WIDGET (priv->overlay_box));

	completion = gtk_source_view_get_completion (priv->child_view);
	gtk_source_completion_block_interactive (completion);

	gtk_source_map_set_font_name (map, "Monospace 1");
}

/**
 * gtk_source_map_new:
 *
 * Creates a new #GtkSourceMap.
 *
 * Returns: a new #GtkSourceMap.
 *
 * Since: 3.18
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
 *
 * Since: 3.18
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
		g_object_remove_weak_pointer (G_OBJECT (priv->view),
		                              (gpointer *)&priv->view);
	}

	priv->view = view;
	if (view != NULL)
	{
		GtkAdjustment *vadj;
		GtkTextBuffer *buffer;

		g_object_add_weak_pointer (G_OBJECT (view),
		                           (gpointer *)&priv->view);

		g_object_bind_property (priv->view, "buffer",
		                        priv->child_view, "buffer",
		                        G_BINDING_SYNC_CREATE);
		g_object_bind_property (priv->view, "indent-width",
		                        priv->child_view, "indent-width",
		                        G_BINDING_SYNC_CREATE);
		g_object_bind_property (priv->view, "tab-width",
		                        priv->child_view, "tab-width",
		                        G_BINDING_SYNC_CREATE);

		g_signal_connect_object (view,
		                         "notify::buffer",
		                         G_CALLBACK (view_notify_buffer),
		                         map,
		                         G_CONNECT_SWAPPED);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		buffer_notify_style_scheme (map, NULL, buffer);

		vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (priv->view));

		g_signal_connect_object (vadj,
		                         "value-changed",
		                         G_CALLBACK (view_vadj_value_changed),
		                         map,
		                         G_CONNECT_SWAPPED);
		g_signal_connect_object (vadj,
		                         "notify::upper",
		                         G_CALLBACK (view_vadj_notify_upper),
		                         map,
		                         G_CONNECT_SWAPPED);

		if ((gtk_widget_get_events (GTK_WIDGET (priv->view)) & GDK_ENTER_NOTIFY_MASK) == 0)
		{
			gtk_widget_add_events (GTK_WIDGET (priv->view),
			                       GDK_ENTER_NOTIFY_MASK);
		}

		if ((gtk_widget_get_events (GTK_WIDGET (priv->view)) & GDK_LEAVE_NOTIFY_MASK) == 0)
		{
			gtk_widget_add_events (GTK_WIDGET (priv->view),
			                       GDK_LEAVE_NOTIFY_MASK);
		}

		gtk_source_map_rebuild_css (map);
	}

	g_object_notify_by_pspec (G_OBJECT (map), pspecs[PROP_VIEW]);
}

/**
 * gtk_source_map_get_view:
 * @map: a #GtkSourceMap.
 *
 * Gets the #GtkSourceMap:view property, which is the view this widget is mapping.
 *
 * Returns: (transfer none) (nullable): a #GtkSourceView or %NULL.
 *
 * Since: 3.18
 */
GtkSourceView *
gtk_source_map_get_view (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;

	g_return_val_if_fail (GTK_SOURCE_IS_MAP (map), NULL);

	priv = gtk_source_map_get_instance_private (map);

	return priv->view;
}

/**
 * gtk_source_map_get_child_view:
 * @map: a #GtkSourceMap.
 *
 * Gets the internal #GtkSourceView in case further tweaking is required.
 *
 * Returns: (transfer none): the internal #GtkSourceView.
 *
 * Since: 3.18
 */
GtkSourceView *
gtk_source_map_get_child_view (GtkSourceMap *map)
{
	GtkSourceMapPrivate *priv;

	g_return_val_if_fail (GTK_SOURCE_IS_MAP (map), NULL);

	priv = gtk_source_map_get_instance_private (map);

	return priv->child_view;
}

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

#include "gtksourcegutterrendererpixbuf.h"
#include "gtksourcepixbufhelper-private.h"

/**
 * GtkSourceGutterRendererPixbuf:
 *
 * Renders a pixbuf in the gutter.
 *
 * A `GtkSourceGutterRendererPixbuf` can be used to render an image in a cell of
 * [class@Gutter].
 */

typedef struct
{
	GtkSourcePixbufHelper *helper;
	GdkPaintable          *paintable;
	GPtrArray             *overlays;
} GtkSourceGutterRendererPixbufPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceGutterRendererPixbuf, gtk_source_gutter_renderer_pixbuf, GTK_SOURCE_TYPE_GUTTER_RENDERER)

enum
{
	PROP_0,
	PROP_PIXBUF,
	PROP_ICON_NAME,
	PROP_GICON,
        PROP_PAINTABLE,
};

static void
clear_overlays (GtkSourceGutterRendererPixbuf *renderer)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	if (priv->overlays != NULL && priv->overlays->len > 0)
	{
		g_ptr_array_remove_range (priv->overlays, 0, priv->overlays->len);
	}
}

static void
gutter_renderer_pixbuf_snapshot_line (GtkSourceGutterRenderer      *renderer,
                                      GtkSnapshot                  *snapshot,
                                      GtkSourceGutterLines         *lines,
                                      guint                         line)
{
	GtkSourceGutterRendererPixbuf *pix = GTK_SOURCE_GUTTER_RENDERER_PIXBUF (renderer);
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (pix);
	GtkWidget *widget = GTK_WIDGET (renderer);
	GdkPaintable *paintable;
	GtkSourceView *view;
	gint width;
	gint height;
	gfloat x = 0;
	gfloat y = 0;
	guint i;
	gint size;

	view = gtk_source_gutter_renderer_get_view (renderer);

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	size = MIN (width, height);

	paintable = gtk_source_pixbuf_helper_render (priv->helper,
	                                             GTK_WIDGET (view),
	                                             size);

	/* Short-circuit if there is nothing to snapshot */
	if (paintable == NULL &&
	    (priv->overlays == NULL || priv->overlays->len == 0))
	{
		return;
	}

	gtk_source_gutter_renderer_align_cell (renderer, line, size, size, &x, &y);

	gtk_snapshot_save (snapshot);
	gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));

	if (paintable != NULL)
	{
		gdk_paintable_snapshot (paintable, snapshot, size, size);
	}
	else if (priv->paintable != NULL)
	{
		gdk_paintable_snapshot (priv->paintable, snapshot, size, size);
	}

	if (priv->overlays != NULL)
	{
		for (i = 0; i < priv->overlays->len; i++)
		{
			GdkPaintable *ele = g_ptr_array_index (priv->overlays, i);

			gdk_paintable_snapshot (ele, snapshot, size, size);
		}
	}

	gtk_snapshot_restore (snapshot);
}

static void
gtk_source_gutter_renderer_pixbuf_finalize (GObject *object)
{
	GtkSourceGutterRendererPixbuf *renderer = GTK_SOURCE_GUTTER_RENDERER_PIXBUF (object);
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

        g_clear_pointer (&priv->helper, gtk_source_pixbuf_helper_free);
        g_clear_pointer (&priv->overlays, g_ptr_array_unref);

	G_OBJECT_CLASS (gtk_source_gutter_renderer_pixbuf_parent_class)->finalize (object);
}

static void
set_pixbuf (GtkSourceGutterRendererPixbuf *renderer,
            GdkPixbuf                     *pixbuf)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

        g_clear_object (&priv->paintable);
        clear_overlays (renderer);
	gtk_source_pixbuf_helper_set_pixbuf (priv->helper, pixbuf);
}

static void
set_gicon (GtkSourceGutterRendererPixbuf *renderer,
           GIcon                         *icon)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

        g_clear_object (&priv->paintable);
        clear_overlays (renderer);
	gtk_source_pixbuf_helper_set_gicon (priv->helper, icon);
}

static void
set_icon_name (GtkSourceGutterRendererPixbuf *renderer,
               const gchar                   *icon_name)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

        g_clear_object (&priv->paintable);
        clear_overlays (renderer);
	gtk_source_pixbuf_helper_set_icon_name (priv->helper, icon_name);
}


static void
gtk_source_gutter_renderer_pixbuf_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
	GtkSourceGutterRendererPixbuf *renderer = GTK_SOURCE_GUTTER_RENDERER_PIXBUF (object);

	switch (prop_id)
	{
		case PROP_PIXBUF:
			set_pixbuf (renderer, g_value_get_object (value));
			break;
		case PROP_ICON_NAME:
			set_icon_name (renderer, g_value_get_string (value));
			break;
		case PROP_GICON:
			set_gicon (renderer, g_value_get_object (value));
			break;
		case PROP_PAINTABLE:
			gtk_source_gutter_renderer_pixbuf_set_paintable (renderer,
			                                                 g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_pixbuf_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
	GtkSourceGutterRendererPixbuf *renderer = GTK_SOURCE_GUTTER_RENDERER_PIXBUF (object);
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	switch (prop_id)
	{
		case PROP_PIXBUF:
			g_value_set_object (value,
			                    gtk_source_pixbuf_helper_get_pixbuf (priv->helper));
			break;
		case PROP_ICON_NAME:
			g_value_set_string (value,
			                    gtk_source_pixbuf_helper_get_icon_name (priv->helper));
			break;
		case PROP_GICON:
			g_value_set_object (value,
			                    gtk_source_pixbuf_helper_get_gicon (priv->helper));
			break;
		case PROP_PAINTABLE:
			g_value_set_object (value, priv->paintable);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_pixbuf_class_init (GtkSourceGutterRendererPixbufClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

	object_class->finalize = gtk_source_gutter_renderer_pixbuf_finalize;
	object_class->get_property = gtk_source_gutter_renderer_pixbuf_get_property;
	object_class->set_property = gtk_source_gutter_renderer_pixbuf_set_property;

	renderer_class->snapshot_line = gutter_renderer_pixbuf_snapshot_line;

	g_object_class_install_property (object_class,
	                                 PROP_PAINTABLE,
	                                 g_param_spec_object ("paintable",
	                                                      "Paintable",
	                                                      "The paintable",
	                                                      GDK_TYPE_PAINTABLE,
	                                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
	                                 PROP_PIXBUF,
	                                 g_param_spec_object ("pixbuf",
	                                                      "Pixbuf",
	                                                      "The pixbuf",
	                                                      GDK_TYPE_PIXBUF,
	                                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
	                                 PROP_ICON_NAME,
	                                 g_param_spec_string ("icon-name",
	                                                      "Icon Name",
	                                                      "The icon name",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
	                                 PROP_GICON,
	                                 g_param_spec_object ("gicon",
	                                                      "GIcon",
	                                                      "The gicon",
	                                                      G_TYPE_ICON,
	                                                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
}

static void
gtk_source_gutter_renderer_pixbuf_init (GtkSourceGutterRendererPixbuf *self)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (self);

	priv->helper = gtk_source_pixbuf_helper_new ();
}

/**
 * gtk_source_gutter_renderer_pixbuf_new:
 *
 * Create a new #GtkSourceGutterRendererPixbuf.
 *
 * Returns: (transfer full): A #GtkSourceGutterRenderer
 *
 **/
GtkSourceGutterRenderer *
gtk_source_gutter_renderer_pixbuf_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_GUTTER_RENDERER_PIXBUF, NULL);
}

/**
 * gtk_source_gutter_renderer_pixbuf_set_pixbuf:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 * @pixbuf: (nullable): the pixbuf, or %NULL.
 */
void
gtk_source_gutter_renderer_pixbuf_set_pixbuf (GtkSourceGutterRendererPixbuf *renderer,
                                              GdkPixbuf                     *pixbuf)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer));
	g_return_if_fail (renderer == NULL || GDK_IS_PIXBUF (pixbuf));

	set_pixbuf (renderer, pixbuf);
}


/**
 * gtk_source_gutter_renderer_pixbuf_get_pixbuf:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 *
 * Get the pixbuf of the renderer.
 *
 * Returns: (transfer none): a #GdkPixbuf
 *
 **/
GdkPixbuf *
gtk_source_gutter_renderer_pixbuf_get_pixbuf (GtkSourceGutterRendererPixbuf *renderer)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer), NULL);

	return gtk_source_pixbuf_helper_get_pixbuf (priv->helper);
}

/**
 * gtk_source_gutter_renderer_pixbuf_set_gicon:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 * @icon: (nullable): the icon, or %NULL.
 */
void
gtk_source_gutter_renderer_pixbuf_set_gicon (GtkSourceGutterRendererPixbuf *renderer,
                                             GIcon                         *icon)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer));
	g_return_if_fail (icon == NULL || G_IS_ICON (icon));

	set_gicon (renderer, icon);
}

/**
 * gtk_source_gutter_renderer_pixbuf_get_gicon:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 *
 * Get the gicon of the renderer
 *
 * Returns: (transfer none): a #GIcon
 *
 **/
GIcon *
gtk_source_gutter_renderer_pixbuf_get_gicon (GtkSourceGutterRendererPixbuf *renderer)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer), NULL);

	return gtk_source_pixbuf_helper_get_gicon (priv->helper);
}

/**
 * gtk_source_gutter_renderer_pixbuf_set_icon_name:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 * @icon_name: (nullable): the icon name, or %NULL.
 */
void
gtk_source_gutter_renderer_pixbuf_set_icon_name (GtkSourceGutterRendererPixbuf *renderer,
                                                 const gchar                   *icon_name)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer));

	set_icon_name (renderer, icon_name);
}

const gchar *
gtk_source_gutter_renderer_pixbuf_get_icon_name (GtkSourceGutterRendererPixbuf *renderer)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer), NULL);

	return gtk_source_pixbuf_helper_get_icon_name (priv->helper);
}

/**
 * gtk_source_gutter_renderer_pixbuf_set_paintable:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 * @paintable: (nullable): the paintable, or %NULL.
 */
void
gtk_source_gutter_renderer_pixbuf_set_paintable (GtkSourceGutterRendererPixbuf *renderer,
                                                 GdkPaintable                  *paintable)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer));
	g_return_if_fail (!paintable || GDK_IS_PAINTABLE (paintable));

	clear_overlays (renderer);
	gtk_source_pixbuf_helper_set_icon_name (priv->helper, NULL);
	g_set_object (&priv->paintable, paintable);
}

/**
 * gtk_source_gutter_renderer_pixbuf_get_paintable:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 *
 * Gets a [iface@Gdk.Paintable] that was set with
 * [method@GutterRendererPixbuf.set_paintable]
 *
 * Returns: (transfer none) (nullable): a #GdkPaintable or %NULL
 */
GdkPaintable *
gtk_source_gutter_renderer_pixbuf_get_paintable (GtkSourceGutterRendererPixbuf *renderer)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer), NULL);

	return priv->paintable;
}

/**
 * gtk_source_gutter_renderer_pixbuf_overlay_paintable:
 * @renderer: a #GtkSourceGutterRendererPixbuf
 * @paintable: a #GdkPaintable
 *
 * Allows overlaying a paintable on top of any other image that
 * has been set for the pixbuf. This will be applied when the
 * widget is next snapshot.
 */
void
gtk_source_gutter_renderer_pixbuf_overlay_paintable (GtkSourceGutterRendererPixbuf *renderer,
                                                     GdkPaintable                  *paintable)
{
	GtkSourceGutterRendererPixbufPrivate *priv = gtk_source_gutter_renderer_pixbuf_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER_PIXBUF (renderer));
	g_return_if_fail (GDK_IS_PAINTABLE (paintable));

	if (priv->overlays == NULL)
	{
		priv->overlays = g_ptr_array_new_with_free_func (g_object_unref);
	}

	g_ptr_array_add (priv->overlays, g_object_ref (paintable));
}

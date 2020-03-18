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

#include <cairo.h>
#include <gdk/gdk.h>

#include "gtksourcepixbufhelper-private.h"

typedef enum _IconType
{
	ICON_TYPE_PIXBUF,
	ICON_TYPE_GICON,
	ICON_TYPE_NAME
} IconType;

struct _GtkSourcePixbufHelper
{
	GdkPaintable *cached_paintable;
	IconType type;

	GdkPixbuf *pixbuf;
	gchar *icon_name;
	GIcon *gicon;
};

GtkSourcePixbufHelper *
gtk_source_pixbuf_helper_new (void)
{
	return g_slice_new0 (GtkSourcePixbufHelper);
}

void
gtk_source_pixbuf_helper_free (GtkSourcePixbufHelper *helper)
{
	g_clear_object (&helper->pixbuf);
	g_clear_object (&helper->cached_paintable);
	g_clear_object (&helper->gicon);
	g_clear_pointer (&helper->icon_name, g_free);

	g_slice_free (GtkSourcePixbufHelper, helper);
}

static void
set_cache (GtkSourcePixbufHelper *helper,
           GdkPaintable          *paintable)
{
	g_clear_object (&helper->cached_paintable);
	helper->cached_paintable = paintable;
}

static GdkTexture *
texture_new_for_surface (cairo_surface_t *surface)
{
  GdkTexture *texture;
  GBytes *bytes;

  g_return_val_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE, NULL);
  g_return_val_if_fail (cairo_image_surface_get_width (surface) > 0, NULL);
  g_return_val_if_fail (cairo_image_surface_get_height (surface) > 0, NULL);

  bytes = g_bytes_new_with_free_func (cairo_image_surface_get_data (surface),
                                      cairo_image_surface_get_height (surface)
                                      * cairo_image_surface_get_stride (surface),
                                      (GDestroyNotify) cairo_surface_destroy,
                                      cairo_surface_reference (surface));

  texture = gdk_memory_texture_new (cairo_image_surface_get_width (surface),
                                    cairo_image_surface_get_height (surface),
                                    GDK_MEMORY_DEFAULT,
                                    bytes,
                                    cairo_image_surface_get_stride (surface));

  g_bytes_unref (bytes);

  return texture;
}

static GdkTexture *
render_paintable_to_texture (GdkPaintable *paintable)
{
	GtkSnapshot *snapshot;
	GskRenderNode *node;
	int width, height;
	cairo_surface_t *surface;
	cairo_t *cr;
	GdkTexture *texture;

	width = gdk_paintable_get_intrinsic_width (paintable);
	height = gdk_paintable_get_intrinsic_height (paintable);

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

	snapshot = gtk_snapshot_new ();
	gdk_paintable_snapshot (paintable, snapshot, width, height);
	node = gtk_snapshot_free_to_node (snapshot);

	cr = cairo_create (surface);
	gsk_render_node_draw (node, cr);
	cairo_destroy (cr);

	gsk_render_node_unref (node);

	texture = texture_new_for_surface (surface);
	cairo_surface_destroy (surface);

	return texture;
}

static void
set_cache_from_icon_paintable (GtkSourcePixbufHelper *helper,
                               GtkIconPaintable      *icon_paintable)
{
	GdkTexture *texture;

	g_assert (helper != NULL);
	g_assert (icon_paintable == NULL || GTK_IS_ICON_PAINTABLE (icon_paintable));

	texture = render_paintable_to_texture (GDK_PAINTABLE (icon_paintable));
	set_cache (helper, GDK_PAINTABLE (g_steal_pointer (&texture)));
}

static void
clear_cache (GtkSourcePixbufHelper *helper)
{
	set_cache (helper, NULL);
}

void
gtk_source_pixbuf_helper_set_pixbuf (GtkSourcePixbufHelper *helper,
                                     const GdkPixbuf       *pixbuf)
{
	helper->type = ICON_TYPE_PIXBUF;

	if (helper->pixbuf)
	{
		g_object_unref (helper->pixbuf);
		helper->pixbuf = NULL;
	}

	if (pixbuf)
	{
		helper->pixbuf = gdk_pixbuf_copy (pixbuf);
	}

	clear_cache (helper);
}

GdkPixbuf *
gtk_source_pixbuf_helper_get_pixbuf (GtkSourcePixbufHelper *helper)
{
	return helper->pixbuf;
}

void
gtk_source_pixbuf_helper_set_icon_name (GtkSourcePixbufHelper *helper,
                                        const gchar           *icon_name)
{
	helper->type = ICON_TYPE_NAME;

	if (helper->icon_name)
	{
		g_free (helper->icon_name);
	}

	helper->icon_name = g_strdup (icon_name);

	clear_cache (helper);
}

const gchar *
gtk_source_pixbuf_helper_get_icon_name (GtkSourcePixbufHelper *helper)
{
	return helper->icon_name;
}

void
gtk_source_pixbuf_helper_set_gicon (GtkSourcePixbufHelper *helper,
                                    GIcon                 *gicon)
{
	helper->type = ICON_TYPE_GICON;

	if (helper->gicon)
	{
		g_object_unref (helper->gicon);
		helper->gicon = NULL;
	}

	if (gicon)
	{
		helper->gicon = g_object_ref (gicon);
	}

	clear_cache (helper);
}

GIcon *
gtk_source_pixbuf_helper_get_gicon (GtkSourcePixbufHelper *helper)
{
	return helper->gicon;
}

static void
from_pixbuf (GtkSourcePixbufHelper *helper,
             GtkWidget             *widget,
             gint                   size)
{
	if (helper->pixbuf != NULL)
	{
		set_cache (helper, GDK_PAINTABLE (gdk_texture_new_for_pixbuf (helper->pixbuf)));
	}
}

static void
from_gicon (GtkSourcePixbufHelper *helper,
            GtkWidget             *widget,
            gint                   size)
{
	GtkIconPaintable *paintable = NULL;
	GtkIconTheme *icon_theme;
	GdkDisplay *display;

	if (helper->gicon == NULL)
	{
		return;
	}

	display = gtk_widget_get_display (widget);
	icon_theme = gtk_icon_theme_get_for_display (display);

	paintable = gtk_icon_theme_lookup_by_gicon (icon_theme,
	                                            helper->gicon,
	                                            size,
	                                            gtk_widget_get_scale_factor (widget),
	                                            gtk_widget_get_direction (widget),
	                                            GTK_ICON_LOOKUP_PRELOAD);

	set_cache_from_icon_paintable (helper, paintable);

	g_object_unref (paintable);
}

static void
from_name (GtkSourcePixbufHelper *helper,
           GtkWidget             *widget,
           gint                   size)
{
	GtkIconPaintable *paintable;
	GtkIconTheme *icon_theme;
	GdkDisplay *display;

	if (helper->icon_name == NULL)
	{
		return;
	}

	display = gtk_widget_get_display (widget);
	icon_theme = gtk_icon_theme_get_for_display (display);

	paintable = gtk_icon_theme_lookup_icon (icon_theme,
	                                        helper->icon_name,
	                                        NULL,
	                                        size,
	                                        gtk_widget_get_scale_factor (widget),
	                                        gtk_widget_get_direction (widget),
	                                        GTK_ICON_LOOKUP_PRELOAD);

	set_cache_from_icon_paintable (helper, paintable);

	g_object_unref (paintable);
}

GdkPaintable *
gtk_source_pixbuf_helper_render (GtkSourcePixbufHelper *helper,
                                 GtkWidget             *widget,
                                 gint                   size)
{
	if (helper->cached_paintable != NULL)
	{
		return helper->cached_paintable;
	}

	switch (helper->type)
	{
		case ICON_TYPE_PIXBUF:
			from_pixbuf (helper, widget, size);
			break;
		case ICON_TYPE_GICON:
			from_gicon (helper, widget, size);
			break;
		case ICON_TYPE_NAME:
			from_name (helper, widget, size);
			break;
		default:
			g_assert_not_reached ();
	}

	return helper->cached_paintable;
}


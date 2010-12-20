/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcemarkcategory.c
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

#include "gtksourcemarkcategory.h"
#include "gtksourceview-i18n.h"
#include "gtksourcepixbufhelper.h"
#include "gtksourceview-marshal.h"

#define GTK_SOURCE_MARK_CATEGORY_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_MARK_CATEGORY, GtkSourceMarkCategoryPrivate))

struct _GtkSourceMarkCategoryPrivate
{
	gchar *id;
	GdkRGBA background;
	gint priority;

	GtkSourcePixbufHelper *helper;

	guint background_set : 1;
};

G_DEFINE_TYPE (GtkSourceMarkCategory, gtk_source_mark_category, G_TYPE_OBJECT)

enum
{
	PROP_0,
	PROP_ID,
	PROP_BACKGROUND,
	PROP_PRIORITY,
	PROP_STOCK_ID,
	PROP_PIXBUF,
	PROP_ICON_NAME,
	PROP_GICON
};

enum
{
	QUERY_TOOLTIP_TEXT,
	QUERY_TOOLTIP_MARKUP,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = {0,};

static void
gtk_source_mark_category_finalize (GObject *object)
{
	GtkSourceMarkCategory *category = GTK_SOURCE_MARK_CATEGORY (object);

	gtk_source_pixbuf_helper_free (category->priv->helper);

	G_OBJECT_CLASS (gtk_source_mark_category_parent_class)->finalize (object);
}

static void
set_background (GtkSourceMarkCategory *category,
                const GdkRGBA         *color)
{
	if (color)
	{
		category->priv->background = *color;
	}

	category->priv->background_set = color != NULL;

	g_object_notify (G_OBJECT (category), "background");
}

static void
set_priority (GtkSourceMarkCategory *category,
              gint                   priority)
{
	if (category->priv->priority == priority)
	{
		return;
	}

	category->priv->priority = priority;
	g_object_notify (G_OBJECT (category), "priority");
}

static void
set_stock_id (GtkSourceMarkCategory *category,
              const gchar           *stock_id)
{
	if (g_strcmp0 (gtk_source_pixbuf_helper_get_stock_id (category->priv->helper),
	                                                      stock_id) == 0)
	{
		return;
	}

	gtk_source_pixbuf_helper_set_stock_id (category->priv->helper,
	                                       stock_id);

	g_object_notify (G_OBJECT (category), "stock-id");
}

static void
set_icon_name (GtkSourceMarkCategory *category,
               const gchar           *icon_name)
{
	if (g_strcmp0 (gtk_source_pixbuf_helper_get_icon_name (category->priv->helper),
	                                                       icon_name) == 0)
	{
		return;
	}

	gtk_source_pixbuf_helper_set_icon_name (category->priv->helper,
	                                        icon_name);

	g_object_notify (G_OBJECT (category), "icon-name");
}

static void
set_pixbuf (GtkSourceMarkCategory *category,
            const GdkPixbuf       *pixbuf)
{
	if (gtk_source_pixbuf_helper_get_pixbuf (category->priv->helper) == pixbuf)
	{
		return;
	}

	gtk_source_pixbuf_helper_set_pixbuf (category->priv->helper,
	                                     pixbuf);

	g_object_notify (G_OBJECT (category), "pixbuf");
}

static void
set_gicon (GtkSourceMarkCategory *category,
           GIcon                 *gicon)
{
	if (gtk_source_pixbuf_helper_get_gicon (category->priv->helper) == gicon)
	{
		return;
	}

	gtk_source_pixbuf_helper_set_gicon (category->priv->helper,
	                                    gicon);

	g_object_notify (G_OBJECT (category), "gicon");
}

static void
gtk_source_mark_category_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	GtkSourceMarkCategory *self = GTK_SOURCE_MARK_CATEGORY (object);

	switch (prop_id)
	{
		case PROP_ID:
			self->priv->id = g_value_dup_string (value);
			break;
		case PROP_BACKGROUND:
			set_background (self, g_value_get_boxed (value));
			break;
		case PROP_PRIORITY:
			set_priority (self, g_value_get_int (value));
			break;
		case PROP_STOCK_ID:
			set_stock_id (self, g_value_get_string (value));
			break;
		case PROP_PIXBUF:
			set_pixbuf (self, g_value_get_object (value));
			break;
		case PROP_ICON_NAME:
			set_icon_name (self, g_value_get_string (value));
			break;
		case PROP_GICON:
			set_gicon (self, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_mark_category_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	GtkSourceMarkCategory *self = GTK_SOURCE_MARK_CATEGORY (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, self->priv->id);
			break;
		case PROP_BACKGROUND:
			if (self->priv->background_set)
			{
				g_value_set_boxed (value, &self->priv->background);
			}
			else
			{
				g_value_set_boxed (value, NULL);
			}
			break;
		case PROP_PRIORITY:
			g_value_set_int (value, self->priv->priority);
			break;
		case PROP_STOCK_ID:
			g_value_set_string (value,
			                    gtk_source_pixbuf_helper_get_stock_id (self->priv->helper));
			break;
		case PROP_PIXBUF:
			g_value_set_object (value,
			                    gtk_source_pixbuf_helper_get_pixbuf (self->priv->helper));
			break;
		case PROP_ICON_NAME:
			g_value_set_string (value,
			                    gtk_source_pixbuf_helper_get_icon_name (self->priv->helper));
			break;
		case PROP_GICON:
			g_value_set_object (value,
			                    gtk_source_pixbuf_helper_get_gicon (self->priv->helper));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_mark_category_class_init (GtkSourceMarkCategoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_mark_category_finalize;

	object_class->get_property = gtk_source_mark_category_get_property;
	object_class->set_property = gtk_source_mark_category_set_property;

	g_type_class_add_private (object_class, sizeof(GtkSourceMarkCategoryPrivate));

	g_object_class_install_property (object_class,
	                                 PROP_ID,
	                                 g_param_spec_string ("id",
	                                                      _("Id"),
	                                                      _("The id"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_BACKGROUND,
	                                 g_param_spec_boxed ("background",
	                                                     _("Background"),
	                                                     _("The background"),
	                                                     GDK_TYPE_RGBA,
	                                                     G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_PRIORITY,
	                                 g_param_spec_int ("priority",
	                                                   _("Priority"),
	                                                   _("The priority"),
	                                                   0,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_STOCK_ID,
	                                 g_param_spec_string ("stock-id",
	                                                      _("Stock Id"),
	                                                      _("The stock id"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_PIXBUF,
	                                 g_param_spec_object ("pixbuf",
	                                                      _("Pixbuf"),
	                                                      _("The pixbuf"),
	                                                      GDK_TYPE_PIXBUF,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_ICON_NAME,
	                                 g_param_spec_string ("icon-name",
	                                                      _("Icon Name"),
	                                                      _("The icon name"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
	                                 PROP_GICON,
	                                 g_param_spec_object ("gicon",
	                                                      _("GIcon"),
	                                                      _("The GIcon"),
	                                                      G_TYPE_ICON,
	                                                      G_PARAM_READWRITE));

	signals[QUERY_TOOLTIP_TEXT] =
		g_signal_new ("query-tooltip-text",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL,
		              NULL,
		              _gtksourceview_marshal_STRING__OBJECT,
		              G_TYPE_STRING,
		              1,
		              GTK_TYPE_SOURCE_MARK);

	signals[QUERY_TOOLTIP_MARKUP] =
		g_signal_new ("query-tooltip-markup",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL,
		              NULL,
		              _gtksourceview_marshal_STRING__OBJECT,
		              G_TYPE_STRING,
		              1,
		              GTK_TYPE_SOURCE_MARK);
}

static void
gtk_source_mark_category_init (GtkSourceMarkCategory *self)
{
	self->priv = GTK_SOURCE_MARK_CATEGORY_GET_PRIVATE (self);

	self->priv->helper = gtk_source_pixbuf_helper_new ();
}

GtkSourceMarkCategory *
gtk_source_mark_category_new (const gchar *id)
{
	return g_object_new (GTK_TYPE_SOURCE_MARK_CATEGORY,
	                     "id", id,
	                     NULL);
}

const gchar *
gtk_source_mark_category_get_id (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return category->priv->id;
}

void
gtk_source_mark_category_set_background (GtkSourceMarkCategory *category,
                                         const GdkRGBA         *background)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_background (category, background);
}

gboolean
gtk_source_mark_category_get_background (GtkSourceMarkCategory *category,
                                         GdkRGBA               *background)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), FALSE);

	if (background)
	{
		*background = category->priv->background;
	}
	return category->priv->background_set;
}

void
gtk_source_mark_category_set_priority (GtkSourceMarkCategory *category,
                                       gint                   priority)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_priority (category, priority);
}

gint
gtk_source_mark_category_get_priority (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), 0);

	return category->priv->priority;
}

void
gtk_source_mark_category_set_stock_id (GtkSourceMarkCategory *category,
                                       const gchar           *stock_id)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_stock_id (category, stock_id);
}

const gchar *
gtk_source_mark_category_get_stock_id (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_stock_id (category->priv->helper);
}

void
gtk_source_mark_category_set_icon_name (GtkSourceMarkCategory *category,
                                        const gchar           *icon_name)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_icon_name (category, icon_name);
}

const gchar *
gtk_source_mark_category_get_stock_icon_name (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_icon_name (category->priv->helper);
}

void
gtk_source_mark_category_set_gicon (GtkSourceMarkCategory *category,
                                    GIcon                 *gicon)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_gicon (category, gicon);
}

/**
 * gtk_source_mark_category_get_gicon:
 * @category: a #GtkSourceMarkCategory
 *
 * Get the #GIcon associated with the mark category.
 *
 * Returns: (transfer none): A #GIcon
 *
 **/
GIcon *
gtk_source_mark_category_get_gicon (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_gicon (category->priv->helper);
}

void
gtk_source_mark_category_set_pixbuf (GtkSourceMarkCategory *category,
                                     const GdkPixbuf       *pixbuf)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_pixbuf (category, pixbuf);
}

const GdkPixbuf *
gtk_source_mark_category_get_pixbuf (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_pixbuf (category->priv->helper);
}

const GdkPixbuf *
gtk_source_mark_category_render_icon (GtkSourceMarkCategory *category,
                                      GtkWidget             *widget,
                                      gint                   size)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (size > 0, NULL);

	return gtk_source_pixbuf_helper_render (category->priv->helper,
	                                        widget,
	                                        size);
}

gchar *
gtk_source_mark_category_get_tooltip_text (GtkSourceMarkCategory *category,
                                           GtkSourceMark         *mark)
{
	gchar *ret;

	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARK (mark), NULL);

	ret = NULL;
	g_signal_emit (category, signals[QUERY_TOOLTIP_TEXT], 0, mark, &ret);

	return ret;
}

gchar *
gtk_source_mark_category_get_tooltip_markup (GtkSourceMarkCategory *category,
                                             GtkSourceMark         *mark)
{
	gchar *ret;

	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_MARK (mark), NULL);

	ret = NULL;
	g_signal_emit (category, signals[QUERY_TOOLTIP_MARKUP], 0, mark, &ret);

	return ret;
}


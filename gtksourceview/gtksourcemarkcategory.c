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

/**
 * SECTION:markcategory
 * @short_description: The mark category object
 * @title: GtkSourceMarkCategory
 * @see_also: #GtkSourceMark
 *
 * GtkSourceMarkCategory is an object specifying a category used by
 * GtkSourceMarks. It allows to define a background color, an icon and
 * a priority.
 *
 * To get a name of a category, use gtk_source_mark_category_get_id().
 *
 * Background color is used as a background of a line where a mark is placed and
 * it can be set with gtk_source_mark_category_set_background(). To check if the
 * category has any custom background color and what color it is, use
 * gtk_source_mark_category_get_background().
 *
 * An icon is a graphic element which is shown in the gutter of a view. An
 * example use is showing a red filled circle in a debugger to show that a
 * breakpoint was set in certain line. To get an icon that will be placed in
 * a gutter first a base for it must be specified and then
 * gtk_source_mark_category_render_icon() must be called.
 * There are several ways to specify a base for an icon:
 * <itemizedlist>
 *  <listitem>
 *   <para>
 *    gtk_source_mark_category_set_icon_name()
 *   </para>
 *  </listitem>
 *  <listitem>
 *   <para>
 *    gtk_source_mark_category_set_stock_id()
 *   </para>
 *  </listitem>
 *  <listitem>
 *   <para>
 *    gtk_source_mark_category_set_gicon()
 *   </para>
 *  </listitem>
 *  <listitem>
 *   <para>
 *    gtk_source_mark_category_set_pixbuf()
 *   </para>
 *  </listitem>
 * </itemizedlist>
 * Using any of the above functions overrides the one used earlier. But note
 * that a getter counterpart of ealier used function can still return some
 * value, but it is just not used when rendering the proper icon.
 *
 * A priority says about importance of a category - the higher the value, the
 * more important the category is. It is used to determine whose category
 * background should be used to fill the line when there are more than one mark
 * in a line. Also icons in the gutter are stacked by priority ascending, i.e.
 * an icon with highest priority is stacked at the top. To set or get a priority
 * gtk_source_mark_category_set_priority() or
 * gtk_source_mark_category_get_priority() should be used.
 *
 * To provide meaningful tooltips for a given mark of a category, you should
 * connect to ::query-tooltip-text or ::query-tooltip-markup where the latter
 * takes precedence.
 */

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

	/**
	 * GtkSourceMarkCategory:id:
	 *
	 * A name of the category.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_ID,
	                                 g_param_spec_string ("id",
	                                                      _("Id"),
	                                                      _("The id"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceMarkCategory:background:
	 *
	 * A color used for background of a line.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_BACKGROUND,
	                                 g_param_spec_boxed ("background",
	                                                     _("Background"),
	                                                     _("The background"),
	                                                     GDK_TYPE_RGBA,
	                                                     G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory:priority:
	 *
	 * A priority of the category.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_PRIORITY,
	                                 g_param_spec_int ("priority",
	                                                   _("Priority"),
	                                                   _("The priority"),
	                                                   0,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory:stock-id:
	 *
	 * A stock id that may be a base of a rendered icon.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_STOCK_ID,
	                                 g_param_spec_string ("stock-id",
	                                                      _("Stock Id"),
	                                                      _("The stock id"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory:pixbuf:
	 *
	 * A #GdkPixbuf that may be a base of a rendered icon.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_PIXBUF,
	                                 g_param_spec_object ("pixbuf",
	                                                      _("Pixbuf"),
	                                                      _("The pixbuf"),
	                                                      GDK_TYPE_PIXBUF,
	                                                      G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory:icon-name:
	 *
	 * An icon name that may be a base of a rendered icon.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_ICON_NAME,
	                                 g_param_spec_string ("icon-name",
	                                                      _("Icon Name"),
	                                                      _("The icon name"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory:gicon:
	 *
	 * A #GIcon that may be a base of a rendered icon.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_GICON,
	                                 g_param_spec_object ("gicon",
	                                                      _("GIcon"),
	                                                      _("The GIcon"),
	                                                      G_TYPE_ICON,
	                                                      G_PARAM_READWRITE));

	/**
	 * GtkSourceMarkCategory::query-tooltip-text:
	 * @category: The #GtkSourceMarkCategory which emits the signal.
	 * @mark: The #GtkSourceMark.
	 *
	 * The code should connect to this signal to provide a tooltip for given
	 * @mark. The tooltip should be just a plain text.
	 *
	 * Returns: (transfer full): A tooltip. The string should be freed with
	 * g_free() when done with it.
	 */
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

	/**
	 * GtkSourceMarkCategory::query-tooltip-markup:
	 * @category: The #GtkSourceMarkCategory which emits the signal.
	 * @mark: The #GtkSourceMark.
	 *
	 * The code should connect to this signal to provide a tooltip for given
	 * @mark. The tooltip can contain a markup.
	 *
	 * Returns: (transfer full): A tooltip. The string should be freed with
	 * g_free() when done with it.
	 */
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

/**
 * gtk_source_mark_category_new:
 * @id: A category name.
 *
 * Creates a new source mark category. @id cannot be NULL.
 *
 * Returns: (transfer full): a new source mark category.
 */
GtkSourceMarkCategory *
gtk_source_mark_category_new (const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (GTK_TYPE_SOURCE_MARK_CATEGORY,
	                     "id", id,
	                     NULL);
}

/**
 * gtk_source_mark_category_get_id:
 * @category: a #GtkSourceMarkCategory.
 *
 * Gets a name of @category.
 *
 * Returns: (transfer none): Name of a category. The returned value is owned
 * by @category and shouldn't be freed.
 */
const gchar *
gtk_source_mark_category_get_id (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return category->priv->id;
}

/**
 * gtk_source_mark_category_set_background:
 * @category: a #GtkSourceMarkCategory.
 * @background: a #GdkRGBA.
 *
 * Sets background color to the one given in @background.
 */
void
gtk_source_mark_category_set_background (GtkSourceMarkCategory *category,
                                         const GdkRGBA         *background)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_background (category, background);
}

/**
 * gtk_source_mark_category_get_background:
 * @category: a #GtkSourceMarkCategory.
 * @background: (out caller-allocates): a #GdkRGBA.
 *
 * Stores background color in @background.
 *
 * Returns: whether background color for @category was set.
 */
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

/**
 * gtk_source_mark_category_set_priority:
 * @category: a #GtkSourceMarkCategory.
 * @priority: a priority.
 *
 * Sets priority of @category to @priority.
 */
void
gtk_source_mark_category_set_priority (GtkSourceMarkCategory *category,
                                       gint                   priority)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_priority (category, priority);
}

/**
 * gtk_source_mark_category_get_priority:
 * @category: a #GtkSourceMarkCategory
 *
 * Gets priority of @category.
 *
 * Returns: priority of @category.
 */
gint
gtk_source_mark_category_get_priority (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), 0);

	return category->priv->priority;
}

/**
 * gtk_source_mark_category_set_stock_id:
 * @category: a #GtkSourceMarkCategory.
 * @stock_id: a stock id.
 *
 * Sets stock id to be used as a base for rendered icon.
 */
void
gtk_source_mark_category_set_stock_id (GtkSourceMarkCategory *category,
                                       const gchar           *stock_id)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_stock_id (category, stock_id);
}

/**
 * gtk_source_mark_category_get_stock_id:
 * @category: a #GtkSourceMarkCategory.
 *
 * Gets a stock id of an icon used by this category. Note that the stock id can
 * be %NULL if it wasn't set earlier.
 *
 * Returns: (transfer none): Stock id. Returned string is owned by @category and
 * shouldn't be freed.
 */
const gchar *
gtk_source_mark_category_get_stock_id (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_stock_id (category->priv->helper);
}

/**
 * gtk_source_mark_category_set_icon_name:
 * @category: a #GtkSourceMarkCategory.
 * @icon_name: name of an icon to be used.
 *
 * Sets a name of an icon to be used as a base for rendered icon.
 */
void
gtk_source_mark_category_set_icon_name (GtkSourceMarkCategory *category,
                                        const gchar           *icon_name)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_icon_name (category, icon_name);
}

/**
 * gtk_source_mark_category_get_icon_name:
 * @category: a #GtkSourceMarkCategory.
 *
 * Gets a name of an icon to be used as a base for rendered icon. Note that the
 * icon name can be %NULL if it wasn't set earlier.
 *
 * Returns: (transfer none): An icon name. The string belongs to @category and
 * should not be freed.
 */
const gchar *
gtk_source_mark_category_get_icon_name (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_icon_name (category->priv->helper);
}

/**
 * gtk_source_mark_category_set_gicon:
 * @category: a #GtkSourceMarkCategory.
 * @gicon: a #GIcon to be used.
 *
 * Sets an icon to be used as a base for rendered icon.
 */
void
gtk_source_mark_category_set_gicon (GtkSourceMarkCategory *category,
                                    GIcon                 *gicon)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_gicon (category, gicon);
}

/**
 * gtk_source_mark_category_get_gicon:
 * @category: a #GtkSourceMarkCategory.
 *
 * Gets a #GIcon to be used as a base for rendered icon. Note that the icon can
 * be %NULL if it wasn't set earlier.
 *
 * Returns: (transfer none): An icon. The icon belongs to @category and should
 * not be unreffed.
 */
GIcon *
gtk_source_mark_category_get_gicon (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_gicon (category->priv->helper);
}

/**
 * gtk_source_mark_category_set_pixbuf:
 * @category: a #GtkSourceMarkCategory.
 * @pixbuf: a #GdkPixbuf to be used.
 *
 * Sets a pixbuf to be used as a base for rendered icon.
 */
void
gtk_source_mark_category_set_pixbuf (GtkSourceMarkCategory *category,
                                     const GdkPixbuf       *pixbuf)
{
	g_return_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category));

	set_pixbuf (category, pixbuf);
}

/**
 * gtk_source_mark_category_get_pixbuf:
 * @category: a #GtkSourceMarkCategory.
 *
 * Gets a #GdkPixbuf to be used as a base for rendered icon. Note that the
 * pixbuf can be %NULL if it wasn't set earlier.
 *
 * Returns: (transfer none): A pixbuf. The pixbuf belongs to @category and
 * should not be unreffed.
 */
const GdkPixbuf *
gtk_source_mark_category_get_pixbuf (GtkSourceMarkCategory *category)
{
	g_return_val_if_fail (GTK_IS_SOURCE_MARK_CATEGORY (category), NULL);

	return gtk_source_pixbuf_helper_get_pixbuf (category->priv->helper);
}

/**
 * gtk_source_mark_category_render_icon:
 * @category: a #GtkSourceMarkCategory.
 * @widget: widget of which style settings may be used.
 * @size: size of the rendered icon.
 *
 * Renders an icon of given size. The base of the icon is set by the last call
 * to one of: gtk_source_mark_category_set_pixbuf(),
 * gtk_source_mark_category_set_gicon(),
 * gtk_source_mark_category_set_icon_name() or
 * gtk_source_mark_category_set_stock_id(). @size cannot be lower than 1.
 *
 * Returns: (transfer none): A rendered pixbuf. The pixbuf belongs to @category
 * and should not be unreffed.
 */
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

/**
 * gtk_source_mark_category_get_tooltip_text:
 * @category: a #GtkSourceMarkCategory.
 * @mark: a #GtkSourceMark.
 *
 * Queries for a tooltip by emitting
 * a GtkSourceMarkCategory::query-tooltip-text signal. The tooltip is a plain
 * text.
 *
 * Returns: (transfer full): A tooltip. The returned string should be freed by
 * using g_free() when done with it.
 */
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

/**
 * gtk_source_mark_category_get_tooltip_markup:
 * @category: a #GtkSourceMarkCategory.
 * @mark: a #GtkSourceMark.
 *
 * Queries for a tooltip by emitting
 * a GtkSourceMarkCategory::query-tooltip-markup signal. The tooltip may contain
 * a markup.
 *
 * Returns: (transfer full): A tooltip. The returned string should be freed by
 * using g_free() when done with it.
 */
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


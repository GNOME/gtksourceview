/*
 * This file is part of GtkSourceView
 *
 * Copyright 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

/**
 * SECTION:completionitem
 * @title: GtkSourceCompletionItem
 * @short_description: Simple implementation of GtkSourceCompletionProposal
 *
 * The #GtkSourceCompletionItem class is a simple implementation of the
 * #GtkSourceCompletionProposal interface.
 */

#include "config.h"

#include "gtksourcecompletionitem.h"
#include "gtksourcecompletionproposal.h"

typedef struct
{
	gchar *label;
	gchar *markup;
	gchar *text;
	GdkTexture *icon;
	gchar *icon_name;
	GIcon *gicon;
	gchar *info;
} GtkSourceCompletionItemPrivate;

enum
{
	PROP_0,
	PROP_LABEL,
	PROP_MARKUP,
	PROP_TEXT,
	PROP_ICON,
	PROP_ICON_NAME,
	PROP_GICON,
	PROP_INFO,
	N_PROPS
};

static void gtk_source_completion_proposal_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionItem,
                         gtk_source_completion_item,
                         G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSourceCompletionItem)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                                gtk_source_completion_proposal_iface_init))

static GParamSpec *properties [N_PROPS];

static gchar *
gtk_source_completion_proposal_get_label_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return g_strdup (priv->label);
}

static gchar *
gtk_source_completion_proposal_get_markup_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return g_strdup (priv->markup);
}

static gchar *
gtk_source_completion_proposal_get_text_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return g_strdup (priv->text);
}

static GdkTexture *
gtk_source_completion_proposal_get_icon_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return priv->icon;
}

static const gchar *
gtk_source_completion_proposal_get_icon_name_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return priv->icon_name;
}

static GIcon *
gtk_source_completion_proposal_get_gicon_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return priv->gicon;
}

static gchar *
gtk_source_completion_proposal_get_info_impl (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (proposal);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	return g_strdup (priv->info);
}

static void
gtk_source_completion_proposal_iface_init (gpointer g_iface,
                                           gpointer iface_data)
{
	GtkSourceCompletionProposalInterface *iface = g_iface;

	/* Interface data getter implementations */
	iface->get_label = gtk_source_completion_proposal_get_label_impl;
	iface->get_markup = gtk_source_completion_proposal_get_markup_impl;
	iface->get_text = gtk_source_completion_proposal_get_text_impl;
	iface->get_icon = gtk_source_completion_proposal_get_icon_impl;
	iface->get_icon_name = gtk_source_completion_proposal_get_icon_name_impl;
	iface->get_gicon = gtk_source_completion_proposal_get_gicon_impl;
	iface->get_info = gtk_source_completion_proposal_get_info_impl;
}

static void
gtk_source_completion_item_dispose (GObject *object)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (object);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_clear_object (&priv->icon);
	g_clear_object (&priv->gicon);

	G_OBJECT_CLASS (gtk_source_completion_item_parent_class)->dispose (object);
}

static void
gtk_source_completion_item_finalize (GObject *object)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (object);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_free (priv->label);
	g_free (priv->markup);
	g_free (priv->text);
	g_free (priv->icon_name);
	g_free (priv->info);

	G_OBJECT_CLASS (gtk_source_completion_item_parent_class)->finalize (object);
}

static void
gtk_source_completion_item_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (object);
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	switch (prop_id)
	{
		case PROP_LABEL:
			g_value_set_string (value, priv->label);
			break;

		case PROP_MARKUP:
			g_value_set_string (value, priv->markup);
			break;

		case PROP_TEXT:
			g_value_set_string (value, priv->text);
			break;

		case PROP_ICON:
			g_value_set_object (value, priv->icon);
			break;

		case PROP_ICON_NAME:
			g_value_set_string (value, priv->icon_name);
			break;

		case PROP_GICON:
			g_value_set_object (value, priv->gicon);
			break;

		case PROP_INFO:
			g_value_set_string (value, priv->info);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
emit_changed (GtkSourceCompletionItem *item)
{
	gtk_source_completion_proposal_changed (GTK_SOURCE_COMPLETION_PROPOSAL (item));
}

static void
gtk_source_completion_item_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceCompletionItem *item = GTK_SOURCE_COMPLETION_ITEM (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			gtk_source_completion_item_set_label (item, g_value_get_string (value));
			break;

		case PROP_MARKUP:
			gtk_source_completion_item_set_markup (item, g_value_get_string (value));
			break;

		case PROP_TEXT:
			gtk_source_completion_item_set_text (item, g_value_get_string (value));
			break;

		case PROP_ICON:
			gtk_source_completion_item_set_icon (item, g_value_get_object (value));
			break;

		case PROP_ICON_NAME:
			gtk_source_completion_item_set_icon_name (item, g_value_get_string (value));
			break;

		case PROP_GICON:
			gtk_source_completion_item_set_gicon (item, g_value_get_object (value));
			break;

		case PROP_INFO:
			gtk_source_completion_item_set_info (item, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_item_class_init (GtkSourceCompletionItemClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_completion_item_dispose;
	object_class->finalize = gtk_source_completion_item_finalize;
	object_class->get_property = gtk_source_completion_item_get_property;
	object_class->set_property = gtk_source_completion_item_set_property;

	/**
	 * GtkSourceCompletionItem:label:
	 *
	 * Label to be shown for this proposal.
	 */
	properties [PROP_LABEL] =
		g_param_spec_string ("label",
		                     "Label",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:markup:
	 *
	 * Label with markup to be shown for this proposal.
	 */
	properties [PROP_MARKUP] =
		g_param_spec_string ("markup",
		                     "Markup",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:text:
	 *
	 * Proposal text.
	 */
	properties [PROP_TEXT] =
		g_param_spec_string ("text",
		                     "Text",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:icon:
	 *
	 * The #GdkTexture for the icon to be shown for this proposal.
	 */
	properties [PROP_ICON] =
		g_param_spec_object ("icon",
		                     "Icon",
		                     "",
		                     GDK_TYPE_TEXTURE,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:icon-name:
	 *
	 * The icon name for the icon to be shown for this proposal.
	 *
	 * Since: 3.18
	 */
	properties [PROP_ICON_NAME] =
		g_param_spec_string ("icon-name",
		                     "Icon Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:gicon:
	 *
	 * The #GIcon for the icon to be shown for this proposal.
	 *
	 * Since: 3.18
	 */
	properties [PROP_GICON] =
		g_param_spec_object ("gicon",
		                     "GIcon",
		                     "",
		                     G_TYPE_ICON,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceCompletionItem:info:
	 *
	 * Optional extra information to be shown for this proposal.
	 */
	properties [PROP_INFO] =
		g_param_spec_string ("info",
		                     "Info",
		                     "",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_completion_item_init (GtkSourceCompletionItem *item)
{
}

/**
 * gtk_source_completion_item_new:
 *
 * Creates a new #GtkSourceCompletionItem. The desired properties need to be set
 * afterwards.
 *
 * Returns: (transfer full): a new #GtkSourceCompletionItem.
 * Since: 4.0
 */
GtkSourceCompletionItem *
gtk_source_completion_item_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_ITEM, NULL);
}

/**
 * gtk_source_completion_item_set_label:
 * @item: a #GtkSourceCompletionItem.
 * @label: (nullable): the label, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_label (GtkSourceCompletionItem *item,
                                      const gchar             *label)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));

	if (g_strcmp0 (priv->label, label) != 0)
	{
		g_free (priv->label);
		priv->label = g_strdup (label);

		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_LABEL]);
	}
}

/**
 * gtk_source_completion_item_set_markup:
 * @item: a #GtkSourceCompletionItem.
 * @markup: (nullable): the markup, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_markup (GtkSourceCompletionItem *item,
                                       const gchar             *markup)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));

	if (g_strcmp0 (priv->markup, markup) != 0)
	{
		g_free (priv->markup);
		priv->markup = g_strdup (markup);

		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_MARKUP]);
	}
}

/**
 * gtk_source_completion_item_set_text:
 * @item: a #GtkSourceCompletionItem.
 * @text: (nullable): the text, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_text (GtkSourceCompletionItem *item,
                                     const gchar             *text)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));

	if (g_strcmp0 (priv->text, text) != 0)
	{
		g_free (priv->text);
		priv->text = g_strdup (text);

		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_TEXT]);
	}
}

/**
 * gtk_source_completion_item_set_icon:
 * @item: a #GtkSourceCompletionItem.
 * @icon: (nullable): the #GdkTexture, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_icon (GtkSourceCompletionItem *item,
                                     GdkTexture              *icon)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));
	g_return_if_fail (icon == NULL || GDK_IS_TEXTURE (icon));

	if (g_set_object (&priv->icon, icon))
	{
		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_ICON]);
	}
}

/**
 * gtk_source_completion_item_set_icon_name:
 * @item: a #GtkSourceCompletionItem.
 * @icon_name: (nullable): the icon name, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_icon_name (GtkSourceCompletionItem *item,
                                          const gchar             *icon_name)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));

	if (g_strcmp0 (priv->icon_name, icon_name) != 0)
	{
		g_free (priv->icon_name);
		priv->icon_name = g_strdup (icon_name);

		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_ICON_NAME]);
	}
}

/**
 * gtk_source_completion_item_set_gicon:
 * @item: a #GtkSourceCompletionItem.
 * @gicon: (nullable): the #GIcon, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_gicon (GtkSourceCompletionItem *item,
                                      GIcon                   *gicon)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));
	g_return_if_fail (gicon == NULL || G_IS_ICON (gicon));

	if (g_set_object (&priv->gicon, gicon))
	{
		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_GICON]);
	}
}

/**
 * gtk_source_completion_item_set_info:
 * @item: a #GtkSourceCompletionItem.
 * @info: (nullable): the info, or %NULL.
 *
 * Since: 3.24
 */
void
gtk_source_completion_item_set_info (GtkSourceCompletionItem *item,
                                     const gchar             *info)
{
	GtkSourceCompletionItemPrivate *priv = gtk_source_completion_item_get_instance_private (item);

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_ITEM (item));

	if (g_strcmp0 (priv->info, info) != 0)
	{
		g_free (priv->info);
		priv->info = g_strdup (info);

		emit_changed (item);
		g_object_notify_by_pspec (G_OBJECT (item), properties [PROP_INFO]);
	}
}

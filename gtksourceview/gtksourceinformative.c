/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtksourceinformative-private.h"
#include "gtksourceview.h"

typedef struct
{
	GtkImage *icon;
	GtkLabel *message;
	GtkMessageType message_type;
} GtkSourceInformativePrivate;

enum {
	PROP_0,
	PROP_ICON_NAME,
	PROP_MESSAGE,
	PROP_MESSAGE_TYPE,
	N_PROPS
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceInformative, gtk_source_informative, GTK_SOURCE_TYPE_ASSISTANT)

static GParamSpec *properties [N_PROPS];

const char *
gtk_source_informative_get_message (GtkSourceInformative *self)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_INFORMATIVE (self), NULL);

	return gtk_label_get_label (priv->message);
}

void
gtk_source_informative_set_message (GtkSourceInformative *self,
                                    const char           *message)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_INFORMATIVE (self));

	gtk_label_set_label (priv->message, message);
	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE]);
}

GtkMessageType
gtk_source_informative_get_message_type (GtkSourceInformative *self)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_INFORMATIVE (self), GTK_MESSAGE_OTHER);

	return priv->message_type;
}

void
gtk_source_informative_set_message_type (GtkSourceInformative *self,
                                         GtkMessageType        message_type)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_assert (GTK_SOURCE_IS_INFORMATIVE (self));

	priv->message_type = message_type;

	gtk_widget_remove_css_class (GTK_WIDGET (self), "error");
	gtk_widget_remove_css_class (GTK_WIDGET (self), "info");
	gtk_widget_remove_css_class (GTK_WIDGET (self), "question");
	gtk_widget_remove_css_class (GTK_WIDGET (self), "warning");
	gtk_widget_remove_css_class (GTK_WIDGET (self), "other");

	switch (priv->message_type)
	{
	case GTK_MESSAGE_INFO:
		gtk_widget_add_css_class (GTK_WIDGET (self), "info");
		break;

	case GTK_MESSAGE_WARNING:
		gtk_widget_add_css_class (GTK_WIDGET (self), "warning");
		break;

	case GTK_MESSAGE_QUESTION:
		gtk_widget_add_css_class (GTK_WIDGET (self), "question");
		break;

	case GTK_MESSAGE_ERROR:
		gtk_widget_add_css_class (GTK_WIDGET (self), "error");
		break;

	case GTK_MESSAGE_OTHER:
		gtk_widget_add_css_class (GTK_WIDGET (self), "other");
		break;

	default:
		break;
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MESSAGE_TYPE]);
}

const char *
gtk_source_informative_get_icon_name (GtkSourceInformative *self)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_assert (GTK_SOURCE_IS_INFORMATIVE (self));

	return gtk_image_get_icon_name (priv->icon);
}

void
gtk_source_informative_set_icon_name (GtkSourceInformative *self,
                                      const char           *icon_name)
{
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);

	g_assert (GTK_SOURCE_IS_INFORMATIVE (self));

	gtk_image_set_from_icon_name (priv->icon, icon_name);
	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ICON_NAME]);
}

static void
gtk_source_informative_get_offset (GtkSourceAssistant *assistant,
                                   int                *x_offset,
                                   int                *y_offset)
{
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS

	GtkSourceInformative *self = GTK_SOURCE_INFORMATIVE (assistant);
	GtkSourceInformativePrivate *priv = gtk_source_informative_get_instance_private (self);
	GtkStyleContext *style_context;
	GtkBorder margin;
	int min_width, min_baseline;

	GTK_SOURCE_ASSISTANT_CLASS (gtk_source_informative_parent_class)->get_offset (assistant, x_offset, y_offset);

	gtk_widget_measure (GTK_WIDGET (priv->icon),
			    GTK_ORIENTATION_HORIZONTAL,
			    -1,
			    &min_width,
			    NULL,
			    &min_baseline,
			    NULL);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (priv->icon));
	gtk_style_context_get_margin (style_context, &margin);

	*x_offset -= min_width;
	*x_offset += margin.right;

	G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_source_informative_get_target_location (GtkSourceAssistant *assistant,
                                            GdkRectangle       *rect)
{
	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_assert (rect != NULL);

	GTK_SOURCE_ASSISTANT_CLASS (gtk_source_informative_parent_class)->get_target_location (assistant, rect);

	/* Align to beginning of the character */
	rect->width = 0;
}

static void
gtk_source_informative_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GtkSourceInformative *self = GTK_SOURCE_INFORMATIVE (object);

	switch (prop_id)
	{
	case PROP_ICON_NAME:
		g_value_set_string (value, gtk_source_informative_get_icon_name (self));
		break;

	case PROP_MESSAGE:
		g_value_set_string (value, gtk_source_informative_get_message (self));
		break;

	case PROP_MESSAGE_TYPE:
		g_value_set_enum (value, gtk_source_informative_get_message_type (self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_informative_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GtkSourceInformative *self = GTK_SOURCE_INFORMATIVE (object);

	switch (prop_id)
	{
	case PROP_ICON_NAME:
		gtk_source_informative_set_icon_name (self, g_value_get_string (value));
		break;

	case PROP_MESSAGE:
		gtk_source_informative_set_message (self, g_value_get_string (value));
		break;

	case PROP_MESSAGE_TYPE:
		gtk_source_informative_set_message_type (self, g_value_get_enum (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_informative_class_init (GtkSourceInformativeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);

	object_class->get_property = gtk_source_informative_get_property;
	object_class->set_property = gtk_source_informative_set_property;

	assistant_class->get_offset = gtk_source_informative_get_offset;
	assistant_class->get_target_location = gtk_source_informative_get_target_location;

	properties [PROP_ICON_NAME] =
		g_param_spec_string ("icon-name",
		                     "Icon Name",
		                     "Icon Name",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_MESSAGE] =
		g_param_spec_string ("message",
		                     "Message",
		                     "The message for the popover",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_MESSAGE_TYPE] =
		g_param_spec_enum ("message-type",
		                   "Message Type",
		                   "The message type for the popover",
		                   GTK_TYPE_MESSAGE_TYPE,
		                   GTK_MESSAGE_INFO,
		                   (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gtksourceview/ui/gtksourceinformative.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GtkSourceInformative, icon);
	gtk_widget_class_bind_template_child_private (widget_class, GtkSourceInformative, message);
}

static void
gtk_source_informative_init (GtkSourceInformative *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

	gtk_popover_set_autohide (GTK_POPOVER (self), FALSE);
}

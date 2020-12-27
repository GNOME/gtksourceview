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

#include "gtksourceassistant-private.h"
#include "gtksourceassistantchild-private.h"
#include "gtksourceview-private.h"

typedef struct
{
	GtkTextMark             *mark;
	GtkSourceAssistantChild *child;
} GtkSourceAssistantPrivate;

static void buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceAssistant, _gtk_source_assistant, GTK_TYPE_POPOVER,
                         G_ADD_PRIVATE (GtkSourceAssistant)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, buildable_iface_init))

static GtkSourceView *
_gtk_source_assistant_get_view (GtkSourceAssistant *assistant)
{
	GtkWidget *widget;

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

	widget = gtk_widget_get_ancestor (GTK_WIDGET (assistant), GTK_SOURCE_TYPE_VIEW);

	g_return_val_if_fail (!widget || GTK_SOURCE_IS_VIEW (widget), NULL);

	return GTK_SOURCE_VIEW (widget);
}

static void
_gtk_source_assistant_hide_action (GtkWidget   *widget,
                                   const gchar *action_name,
                                   GVariant    *parameter)
{
	g_assert (GTK_SOURCE_IS_ASSISTANT (widget));

	gtk_popover_popdown (GTK_POPOVER (widget));
}

static void
_gtk_source_assistant_real_get_target_location (GtkSourceAssistant *assistant,
                                                GdkRectangle       *location)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);
	GtkSourceView *view;

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_assert (location != NULL);

	view = _gtk_source_assistant_get_view (assistant);

	if (view != NULL)
	{
		GtkTextBuffer *buffer;
		GtkTextMark *mark;
		GtkTextIter iter;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		mark = priv->mark ? priv->mark : gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
		gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, location);
	}
	else
	{
		location->x = 0;
		location->y = 0;
		location->width = 0;
		location->height = 0;
	}
}

static void
_gtk_source_assistant_get_target_location (GtkSourceAssistant *assistant,
                                           GdkRectangle       *location)
{
	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_assert (location != NULL);

	GTK_SOURCE_ASSISTANT_GET_CLASS (assistant)->get_target_location (assistant, location);
}

static void
_gtk_source_assistant_update_position (GtkSourceAssistant *assistant)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);
	const GList *children = NULL;
	GtkWidget *parent;
	GdkRectangle rect;
	int x = 0;
	int y = 0;

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

	parent = gtk_widget_get_parent (GTK_WIDGET (assistant));

	if (GTK_SOURCE_IS_VIEW (parent))
	{
		_gtk_source_assistant_get_offset (assistant, &x, &y);
		_gtk_source_assistant_get_target_location (assistant, &rect);

		gtk_popover_set_offset (GTK_POPOVER (assistant), x, y);
		gtk_popover_set_pointing_to (GTK_POPOVER (assistant), &rect);
	}

	gtk_popover_present (GTK_POPOVER (assistant));

	if (priv->child != NULL)
	{
		children = _gtk_source_assistant_child_get_attached (priv->child);
	}

	for (const GList *iter = children; iter; iter = iter->next)
	{
		GtkSourceAssistant *child = iter->data;

		_gtk_source_assistant_get_offset (child, &x, &y);
		gtk_popover_set_offset (GTK_POPOVER (child), x, y);

		if (gtk_widget_get_visible (GTK_WIDGET (child)))
		{
			gtk_popover_present (GTK_POPOVER (child));
		}
	}
}

static void
_gtk_source_assistant_update_size (GtkSourceAssistant *self)
{
	int min_width;
	int nat_width;

	g_assert (GTK_SOURCE_IS_ASSISTANT (self));

	/* Hack to force the sizing request, otherwise the popover does
	 * not shrink as results are reduced.
	 */
	gtk_widget_set_size_request (GTK_WIDGET (self), -1, -1);
	gtk_widget_measure (GTK_WIDGET (self),
	                    GTK_ORIENTATION_HORIZONTAL,
	                    -1,
	                    &min_width, &nat_width, NULL, NULL);
	gtk_widget_set_size_request (GTK_WIDGET (self), min_width, -1);
}

void
_gtk_source_assistant_reposition (GtkSourceAssistant *self)
{
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (self));

	_gtk_source_assistant_update_size (self);
	_gtk_source_assistant_update_position (self);
}

static void
_gtk_source_assistant_show (GtkWidget *widget)
{
	GtkSourceAssistant *assistant = (GtkSourceAssistant *)widget;

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

	_gtk_source_assistant_reposition (assistant);

	GTK_WIDGET_CLASS (_gtk_source_assistant_parent_class)->show (widget);
}

static void
_gtk_source_assistant_hide (GtkWidget *widget)
{
	GtkSourceAssistant *assistant = (GtkSourceAssistant *)widget;
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

	_gtk_source_assistant_child_hide (priv->child);

	GTK_WIDGET_CLASS (_gtk_source_assistant_parent_class)->hide (widget);
}

static void
_gtk_source_assistant_real_get_offset (GtkSourceAssistant *assistant,
                                       int                *x,
                                       int                *y)
{
	GtkStyleContext *style_context;
	GtkBorder margin;
	GtkPositionType pos;

	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_assert (x != NULL);
	g_assert (y != NULL);

	pos = gtk_popover_get_position (GTK_POPOVER (assistant));

	style_context = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	gtk_style_context_get_margin (style_context, &margin);

	*x = -margin.left;

	if (pos != GTK_POS_TOP)
	{
		*y = -margin.top + 1;
	}
	else
	{
		*y = (margin.bottom / 3 * 2) - 1;
	}
}

static void
_gtk_source_assistant_dispose (GObject *object)
{
	GtkSourceAssistant *self = (GtkSourceAssistant *)object;
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (self);

	g_assert (GTK_SOURCE_IS_ASSISTANT (self));

	_gtk_source_assistant_detach (self);
	g_clear_object (&priv->mark);

	G_OBJECT_CLASS (_gtk_source_assistant_parent_class)->dispose (object);
}

static void
_gtk_source_assistant_class_init (GtkSourceAssistantClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = _gtk_source_assistant_dispose;

	widget_class->hide = _gtk_source_assistant_hide;
	widget_class->show = _gtk_source_assistant_show;

	klass->get_offset = _gtk_source_assistant_real_get_offset;
	klass->get_target_location = _gtk_source_assistant_real_get_target_location;

	gtk_widget_class_install_action (widget_class, "assistant.hide", NULL, _gtk_source_assistant_hide_action);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "assistant.hide", NULL);
	gtk_widget_class_set_css_name (widget_class, "GtkSourceAssistant");
}

static void
_gtk_source_assistant_init (GtkSourceAssistant *self)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (self);

	gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_START);
	gtk_widget_set_valign (GTK_WIDGET (self), GTK_ALIGN_START);
	gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_BOTTOM);
	gtk_popover_set_has_arrow (GTK_POPOVER (self), FALSE);
	gtk_popover_set_autohide (GTK_POPOVER (self), TRUE);

	priv->child = _gtk_source_assistant_child_new ();
	gtk_popover_set_child (GTK_POPOVER (self), GTK_WIDGET (priv->child));
}

GtkSourceAssistant *
_gtk_source_assistant_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_ASSISTANT, NULL);
}

void
_gtk_source_assistant_set_mark (GtkSourceAssistant *assistant,
                                GtkTextMark        *mark)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_return_if_fail (GTK_IS_TEXT_MARK (mark));

	if (g_set_object (&priv->mark, mark))
	{
		_gtk_source_assistant_update_position (assistant);
	}
}

GtkTextMark *
_gtk_source_assistant_get_mark (GtkSourceAssistant *assistant)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);

	g_return_val_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant), NULL);

	return priv->mark;
}

void
_gtk_source_assistant_get_offset (GtkSourceAssistant *assistant,
                                  int                *x,
                                  int                *y)
{
	int dummy_x;
	int dummy_y;

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));

	if (x == NULL)
		x = &dummy_x;

	if (y == NULL)
		y = &dummy_y;

	*x = 0;
	*y = 0;

	GTK_SOURCE_ASSISTANT_GET_CLASS (assistant)->get_offset (assistant, x, y);
}

void
_gtk_source_assistant_detach (GtkSourceAssistant *assistant)
{
	GtkWidget *parent;

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));

	parent = gtk_widget_get_parent (GTK_WIDGET (assistant));

	if (GTK_SOURCE_IS_ASSISTANT_CHILD (parent))
	{
		_gtk_source_assistant_child_detach (GTK_SOURCE_ASSISTANT_CHILD (parent),
		                                    assistant);
	}
}

void
_gtk_source_assistant_attach (GtkSourceAssistant *assistant,
                              GtkSourceAssistant *attach_to)
{
	GtkSourceAssistantPrivate *priv;

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_return_if_fail (!attach_to || GTK_SOURCE_IS_ASSISTANT (attach_to));

	if (attach_to == NULL)
	{
		_gtk_source_assistant_detach (assistant);
	}
	else
	{
		priv = _gtk_source_assistant_get_instance_private (attach_to);
		_gtk_source_assistant_child_attach (priv->child, assistant);
	}
}

void
_gtk_source_assistant_set_child (GtkSourceAssistant *assistant,
                                 GtkWidget          *child)
{
	GtkSourceAssistantPrivate *priv = _gtk_source_assistant_get_instance_private (assistant);

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_return_if_fail (!child || GTK_IS_WIDGET (child));

	_gtk_source_assistant_child_set_child (priv->child, child);
}

static void
_gtk_source_assistant_add_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 GObject      *child,
                                 const char   *type)
{
	GtkSourceAssistant *self = (GtkSourceAssistant *)buildable;

	if (GTK_IS_WIDGET (child))
	{
		_gtk_source_assistant_set_child (self, GTK_WIDGET (child));
	}
}

static void
buildable_iface_init (GtkBuildableIface *iface)
{
	iface->add_child = _gtk_source_assistant_add_child;
}

void
_gtk_source_assistant_destroy (GtkSourceAssistant *self)
{
	GtkWidget *parent;

	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (self));

	parent = gtk_widget_get_parent (GTK_WIDGET (self));

	if (parent == NULL)
		return;

	if (GTK_SOURCE_IS_VIEW (parent))
	{
		_gtk_source_view_remove_assistant (GTK_SOURCE_VIEW (parent), self);
	}
	else if (GTK_SOURCE_IS_ASSISTANT_CHILD (parent))
	{
		_gtk_source_assistant_child_detach (GTK_SOURCE_ASSISTANT_CHILD (parent), self);
	}
	else
	{
		g_warning ("Cannot remove assistant from type %s",
			   G_OBJECT_TYPE_NAME (parent));
	}
}

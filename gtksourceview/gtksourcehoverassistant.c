/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "gtksourceassistant-private.h"
#include "gtksourcehoverassistant-private.h"
#include "gtksourcehovercontext-private.h"
#include "gtksourcehoverdisplay-private.h"
#include "gtksourcehover-private.h"
#include "gtksourceview.h"

#define GTK_SOURCE_HOVER_ASSISTANT_MOTION "GTK_SOURCE_HOVER_ASSISTANT_MOTION"

struct _GtkSourceHoverAssistant
{
	GtkSourceAssistant parent_instance;

	GtkEventController *popover_motion;
	GtkEventController *root_motion;

	GtkSourceHoverDisplay *display;

	GCancellable *cancellable;

	GdkRectangle hovered_at;

	double root_x;
	double root_y;

	gulong root_motion_handler;
	gulong root_leave_handler;

	GSource *dismiss_source;

	guint disposed : 1;
};

G_DEFINE_TYPE (GtkSourceHoverAssistant, gtk_source_hover_assistant, GTK_SOURCE_TYPE_ASSISTANT)

static gboolean
gtk_source_hover_assistant_should_dismiss (GtkSourceHoverAssistant *self)
{
	GdkSurface *surface;

	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));

	if (gtk_event_controller_motion_contains_pointer (GTK_EVENT_CONTROLLER_MOTION (self->popover_motion)))
	{
		return FALSE;
	}

	if ((surface = gtk_native_get_surface (GTK_NATIVE (self))))
	{
		GdkRectangle popup_area, root_area;
		double popup_x, popup_y;
		double transform_x, transform_y;
		GtkRoot *root;

		popup_x = gdk_popup_get_position_x (GDK_POPUP (surface));
		popup_y = gdk_popup_get_position_y (GDK_POPUP (surface));
		gtk_native_get_surface_transform (GTK_NATIVE (self), &transform_x, &transform_y);

		popup_area.x = popup_x - transform_x;
		popup_area.y = popup_y - transform_y;
		popup_area.width = gdk_surface_get_width (surface);
		popup_area.height = gdk_surface_get_height (surface);

		root = gtk_widget_get_root (GTK_WIDGET (self));
		root_area.x = 0;
		root_area.y = 0;
		root_area.width = gtk_widget_get_width (GTK_WIDGET (root));
		root_area.height = gtk_widget_get_height (GTK_WIDGET (root));

		if (gdk_rectangle_intersect (&root_area, &popup_area, &popup_area) &&
		    gdk_rectangle_contains_point (&popup_area, self->root_x, self->root_y))
		{
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
gtk_source_hover_assistant_dismiss_cb (GtkSourceHoverAssistant *self)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));

	g_clear_pointer (&self->dismiss_source, g_source_destroy);

	if (gtk_source_hover_assistant_should_dismiss (self))
	{
		_gtk_source_hover_assistant_dismiss (self);
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_hover_assistant_queue_dismiss (GtkSourceHoverAssistant *self)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));

	if (self->dismiss_source != NULL)
	{
		return;
	}

	self->dismiss_source = g_idle_source_new ();
	g_source_set_name (self->dismiss_source, "gtk-source-hover-assistant-timeout");
	g_source_set_callback (self->dismiss_source, (GSourceFunc)gtk_source_hover_assistant_dismiss_cb, self, NULL);
	g_source_attach (self->dismiss_source, NULL);
	g_source_unref (self->dismiss_source);
}

static void
gtk_source_hover_assistant_popover_leave_cb (GtkSourceHoverAssistant  *self,
                                             GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	gtk_source_hover_assistant_queue_dismiss (self);
}

static void
gtk_source_hover_assistant_root_leave_cb (GtkSourceHoverAssistant  *self,
                                          GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	gtk_source_hover_assistant_queue_dismiss (self);
}

static void
gtk_source_hover_assistant_root_motion_cb (GtkSourceHoverAssistant  *self,
                                           double                    x,
                                           double                    y,
                                           GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	self->root_x = x;
	self->root_y = y;

	gtk_source_hover_assistant_queue_dismiss (self);
}

static void
gtk_source_hover_assistant_root (GtkWidget *widget)
{
	GtkSourceHoverAssistant *self = (GtkSourceHoverAssistant *)widget;
	GtkRoot *root;

	GTK_WIDGET_CLASS (gtk_source_hover_assistant_parent_class)->root (widget);

	if ((root = gtk_widget_get_root (widget)))
	{
		GtkEventController *motion;

		if (!(motion = g_object_get_data (G_OBJECT (root), GTK_SOURCE_HOVER_ASSISTANT_MOTION)))
		{
			motion = gtk_event_controller_motion_new ();
			gtk_event_controller_set_name (motion, "gtk-source-hover-assistant-motion");
			g_object_set_data (G_OBJECT (root), GTK_SOURCE_HOVER_ASSISTANT_MOTION, motion);
			gtk_widget_add_controller (GTK_WIDGET (root), motion);
		}

		self->root_motion = g_object_ref (motion);
		self->root_motion_handler =
			g_signal_connect_object (self->root_motion,
			                         "motion",
			                         G_CALLBACK (gtk_source_hover_assistant_root_motion_cb),
			                         self,
			                         G_CONNECT_SWAPPED);
		self->root_leave_handler =
			g_signal_connect_object (self->root_motion,
			                         "leave",
			                         G_CALLBACK (gtk_source_hover_assistant_root_leave_cb),
			                         self,
			                         G_CONNECT_SWAPPED);

		if (!gtk_widget_get_visible (GTK_WIDGET (self)))
		{
			g_signal_handler_block (self->root_motion, self->root_motion_handler);
			g_signal_handler_block (self->root_motion, self->root_leave_handler);
		}
	}
}

static void
gtk_source_hover_assistant_unroot (GtkWidget *widget)
{
	GtkSourceHoverAssistant *self = (GtkSourceHoverAssistant *)widget;
	GtkRoot *root;

	if ((root = gtk_widget_get_root (widget)))
	{
		GtkEventController *motion;

		if ((motion = g_object_get_data (G_OBJECT (root), GTK_SOURCE_HOVER_ASSISTANT_MOTION)))
		{
			g_clear_signal_handler (&self->root_motion_handler, motion);
			g_clear_signal_handler (&self->root_leave_handler, motion);
		}
		else
		{
			self->root_motion_handler = 0;
			self->root_leave_handler = 0;
		}
	}

	GTK_WIDGET_CLASS (gtk_source_hover_assistant_parent_class)->unroot (widget);
}

static void
gtk_source_hover_assistant_show (GtkWidget *widget)
{
	GtkSourceHoverAssistant *self = GTK_SOURCE_HOVER_ASSISTANT (widget);
	GtkRoot *root;

	GTK_WIDGET_CLASS (gtk_source_hover_assistant_parent_class)->show (widget);

	if ((root = gtk_widget_get_root (widget)))
	{
		GtkEventController *motion;

		if ((motion = g_object_get_data (G_OBJECT (root), GTK_SOURCE_HOVER_ASSISTANT_MOTION)))
		{
			g_signal_handler_unblock (motion, self->root_motion_handler);
			g_signal_handler_unblock (motion, self->root_leave_handler);
		}
	}
}

static void
gtk_source_hover_assistant_hide (GtkWidget *widget)
{
	GtkSourceHoverAssistant *self = GTK_SOURCE_HOVER_ASSISTANT (widget);
	GtkRoot *root;

	GTK_WIDGET_CLASS (gtk_source_hover_assistant_parent_class)->hide (widget);

	gtk_popover_set_pointing_to (GTK_POPOVER (widget), NULL);
	gtk_popover_set_offset (GTK_POPOVER (widget), 0, 0);

	if ((root = gtk_widget_get_root (widget)))
	{
		GtkEventController *motion;

		if ((motion = g_object_get_data (G_OBJECT (root), GTK_SOURCE_HOVER_ASSISTANT_MOTION)))
		{
			g_signal_handler_block (motion, self->root_motion_handler);
			g_signal_handler_block (motion, self->root_leave_handler);
		}
	}
}

static void
gtk_source_hover_assistant_get_target_location (GtkSourceAssistant *assistant,
                                                GdkRectangle       *rect)
{
	*rect = GTK_SOURCE_HOVER_ASSISTANT (assistant)->hovered_at;
}

static void
gtk_source_hover_assistant_dispose (GObject *object)
{
	GtkSourceHoverAssistant *self = (GtkSourceHoverAssistant *)object;

	self->disposed = TRUE;

	self->display = NULL;

	g_clear_pointer (&self->dismiss_source, g_source_destroy);

	g_clear_object (&self->root_motion);
	g_clear_object (&self->popover_motion);

	g_clear_object (&self->cancellable);

	G_OBJECT_CLASS (gtk_source_hover_assistant_parent_class)->dispose (object);
}

static void
gtk_source_hover_assistant_class_init (GtkSourceHoverAssistantClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);

	object_class->dispose = gtk_source_hover_assistant_dispose;

	widget_class->root = gtk_source_hover_assistant_root;
	widget_class->unroot = gtk_source_hover_assistant_unroot;
	widget_class->show = gtk_source_hover_assistant_show;
	widget_class->hide = gtk_source_hover_assistant_hide;

	assistant_class->get_target_location = gtk_source_hover_assistant_get_target_location;
}

static void
gtk_source_hover_assistant_init (GtkSourceHoverAssistant *self)
{
	gtk_widget_add_css_class (GTK_WIDGET (self), "hover-assistant");

	gtk_popover_set_autohide (GTK_POPOVER (self), FALSE);
	gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_TOP);

	self->display = g_object_new (GTK_SOURCE_TYPE_HOVER_DISPLAY, NULL);
	_gtk_source_assistant_set_child (GTK_SOURCE_ASSISTANT (self), GTK_WIDGET (self->display));

	self->popover_motion = gtk_event_controller_motion_new ();
	gtk_event_controller_set_name (self->popover_motion, "gkt-source-hover-assistant-motion");
	g_signal_connect_object (self->popover_motion,
	                         "leave",
	                         G_CALLBACK (gtk_source_hover_assistant_popover_leave_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), g_object_ref (self->popover_motion));
}

GtkSourceAssistant *
_gtk_source_hover_assistant_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_HOVER_ASSISTANT, NULL);
}

static void
gtk_source_hover_assistant_populate_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
	GtkSourceHoverContext *context = (GtkSourceHoverContext *)object;
	GtkSourceHoverAssistant *self = user_data;
	GError *error = NULL;

	g_assert (GTK_SOURCE_IS_HOVER_CONTEXT (context));
	g_assert (G_IS_ASYNC_RESULT (result));
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));

	if (_gtk_source_hover_context_populate_finish (context, result, &error))
	{
		if (!self->disposed)
		{
			gtk_widget_set_visible (GTK_WIDGET (self),
			                        !_gtk_source_hover_display_is_empty (self->display));
		}
	}

	g_clear_object (&self);
	g_clear_error (&error);
}

void
_gtk_source_hover_assistant_display (GtkSourceHoverAssistant  *self,
                                     GtkSourceHoverProvider  **providers,
                                     guint                     n_providers,
                                     const GtkTextIter        *begin,
                                     const GtkTextIter        *end,
                                     const GtkTextIter        *location)
{
	GtkSourceHoverContext *context;
	GtkSourceView *view;
	GdkRectangle begin_rect;
	GdkRectangle end_rect;
	GdkRectangle location_rect;
	GdkRectangle visible_rect;

	g_return_if_fail (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_return_if_fail (n_providers == 0 || providers != NULL);
	g_return_if_fail (begin != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (location != NULL);

	memset (&self->hovered_at, 0, sizeof self->hovered_at);

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);

	if (n_providers == 0)
	{
		gtk_widget_hide (GTK_WIDGET (self));
		return;
	}

	view = GTK_SOURCE_VIEW (gtk_widget_get_parent (GTK_WIDGET (self)));

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), begin, &begin_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), end, &end_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), location, &location_rect);
	gdk_rectangle_union (&begin_rect, &end_rect, &location_rect);

	if (!gdk_rectangle_intersect (&location_rect, &visible_rect, &location_rect))
	{
		gtk_widget_hide (GTK_WIDGET (self));
		return;
	}

	self->hovered_at = location_rect;

	context = _gtk_source_hover_context_new (view, begin, end, location);

	for (guint i = 0; i < n_providers; i++)
	{
		_gtk_source_hover_context_add_provider (context, providers[i]);
	}

	_gtk_source_hover_display_clear (self->display);

	self->cancellable = g_cancellable_new ();

	_gtk_source_hover_context_populate_async (context,
	                                          self->display,
	                                          self->cancellable,
	                                          gtk_source_hover_assistant_populate_cb,
	                                          g_object_ref (self));

	g_object_unref (context);
}

void
_gtk_source_hover_assistant_dismiss (GtkSourceHoverAssistant *self)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_ASSISTANT (self));

	g_cancellable_cancel (self->cancellable);
	g_clear_object (&self->cancellable);

	gtk_widget_hide (GTK_WIDGET (self));

	_gtk_source_hover_display_clear (self->display);
}

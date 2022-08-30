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

#include "gtksourceassistant-private.h"
#include "gtksourcehoverassistant-private.h"
#include "gtksourcehovercontext-private.h"
#include "gtksourcehoverdisplay-private.h"
#include "gtksourceview.h"

#define GRACE_X 20
#define GRACE_Y 20

struct _GtkSourceHoverAssistant
{
	GtkSourceAssistant parent_instance;
	GtkSourceHoverDisplay *display;
	GCancellable *cancellable;
	GdkRectangle hovered_at;
};

G_DEFINE_TYPE (GtkSourceHoverAssistant, gtk_source_hover_assistant, GTK_SOURCE_TYPE_ASSISTANT)

static void
gtk_source_hover_assistant_get_target_location (GtkSourceAssistant *assistant,
                                                GdkRectangle       *rect)
{
	*rect = GTK_SOURCE_HOVER_ASSISTANT (assistant)->hovered_at;
}

static void
gtk_source_hover_assistant_motion_cb (GtkSourceHoverAssistant  *self,
                                      double                    x,
                                      double                    y,
                                      GtkEventControllerMotion *controller)
{
	GdkSurface *assistant_surface;
	GtkWidget *parent;
	GtkRoot *root;
	double tx, ty;
	int width, height;

	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	if (gtk_event_controller_motion_contains_pointer (controller))
	{
		return;
	}

	if (!(parent = gtk_widget_get_parent (GTK_WIDGET (self))) ||
	    !(root = gtk_widget_get_root (parent)) ||
	    !(assistant_surface = gtk_native_get_surface (GTK_NATIVE (self))))
	{
		return;
	}

	gtk_widget_translate_coordinates (parent, GTK_WIDGET (root), x, y, &x, &y);
	x -= gdk_popup_get_position_x (GDK_POPUP (assistant_surface));
	y -= gdk_popup_get_position_y (GDK_POPUP (assistant_surface));

	gtk_native_get_surface_transform (GTK_NATIVE (root), &tx, &ty);
	x += tx;
	y += ty;

	width = gdk_surface_get_width (assistant_surface);
	height = gdk_surface_get_height (assistant_surface);

	if (x < -GRACE_X ||
	    x > width + GRACE_Y ||
	    y < -GRACE_Y ||
	    y > height + GRACE_Y)
	{
		GtkWidget *focus = gtk_root_get_focus (root);
		GtkWidget *view = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_SOURCE_TYPE_VIEW);
		gboolean return_focus;

		return_focus = focus != NULL &&
			(focus == GTK_WIDGET (self) ||
			 gtk_widget_is_ancestor (focus, GTK_WIDGET (self)));

		gtk_widget_hide (GTK_WIDGET (self));

		if (return_focus && view != NULL)
		{
			gtk_widget_grab_focus (GTK_WIDGET (view));
		}
	}
}

static gboolean
gtk_source_hover_assistant_scroll_cb (GtkSourceHoverAssistant  *self,
                                      double                    dx,
                                      double                    dy,
                                      GtkEventControllerScroll *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (controller));

	gtk_widget_hide (GTK_WIDGET (self));

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_hover_assistant_dispose (GObject *object)
{
	GtkSourceHoverAssistant *self = (GtkSourceHoverAssistant *)object;

	self->display = NULL;

	g_clear_object (&self->cancellable);

	G_OBJECT_CLASS (gtk_source_hover_assistant_parent_class)->dispose (object);
}

static void
gtk_source_hover_assistant_class_init (GtkSourceHoverAssistantClass *klass)
{
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_hover_assistant_dispose;

	assistant_class->get_target_location = gtk_source_hover_assistant_get_target_location;
}

static void
gtk_source_hover_assistant_init (GtkSourceHoverAssistant *self)
{
	GtkEventController *motion;
	GtkEventController *scroll;

	gtk_widget_add_css_class (GTK_WIDGET (self), "hover-assistant");

	gtk_popover_set_autohide (GTK_POPOVER (self), TRUE);

	self->display = g_object_new (GTK_SOURCE_TYPE_HOVER_DISPLAY, NULL);
	_gtk_source_assistant_set_child (GTK_SOURCE_ASSISTANT (self), GTK_WIDGET (self->display));

	motion = gtk_event_controller_motion_new ();
	gtk_event_controller_set_propagation_phase (motion, GTK_PHASE_CAPTURE);
	g_signal_connect_object (motion,
	                         "motion",
	                         G_CALLBACK (gtk_source_hover_assistant_motion_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), motion);

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect_object (scroll,
	                         "scroll",
	                         G_CALLBACK (gtk_source_hover_assistant_scroll_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), scroll);
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
		gtk_widget_set_visible (GTK_WIDGET (self),
					!_gtk_source_hover_display_is_empty (self->display));
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

	if (n_providers == 0)
	{
		if (gtk_widget_get_visible (GTK_WIDGET (self)))
		{
			gtk_widget_hide (GTK_WIDGET (self));
		}

		return;
	}

	if (self->cancellable != NULL)
	{
		g_cancellable_cancel (self->cancellable);
		g_clear_object (&self->cancellable);
	}

	view = GTK_SOURCE_VIEW (gtk_widget_get_parent (GTK_WIDGET (self)));

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), begin, &begin_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), end, &end_rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), location, &location_rect);

	gdk_rectangle_union (&begin_rect, &end_rect, &location_rect);

	if (!gdk_rectangle_intersect (&location_rect, &visible_rect, &location_rect))
	{
		if (gtk_widget_get_visible (GTK_WIDGET (self)))
		{
			gtk_widget_hide (GTK_WIDGET (self));
		}

		memset (&self->hovered_at, 0, sizeof self->hovered_at);

		return;
	}

	if (gtk_text_iter_equal (begin, end) && gtk_text_iter_starts_line (begin))
	{
		location_rect.width = 1;
		gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_RIGHT);
	}
	else
	{
		gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_TOP);
	}

	self->hovered_at = location_rect;

	context = _gtk_source_hover_context_new (view, begin, end, location);

	for (guint i = 0; i < n_providers; i++)
	{
		_gtk_source_hover_context_add_provider (context, providers[i]);
	}

	self->cancellable = g_cancellable_new ();

	_gtk_source_hover_display_clear (self->display);

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
	gtk_widget_hide (GTK_WIDGET (self));
	_gtk_source_hover_display_clear (self->display);
}

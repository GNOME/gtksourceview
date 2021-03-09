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
#include "gtksourcehover-private.h"
#include "gtksourcehovercontext.h"
#include "gtksourcehoverprovider.h"
#include "gtksourcesignalgroup-private.h"
#include "gtksourceview.h"

struct _GtkSourceHover
{
  GObject             parent_instance;

  GtkSourceView      *view;
  GtkSourceAssistant *assistant;

  GPtrArray          *providers;

  guint               delay_display_source;
  guint               dismiss_source;
};

G_DEFINE_TYPE (GtkSourceHover, gtk_source_hover, G_TYPE_OBJECT)

static gboolean
on_key_pressed_cb (GtkSourceHover        *hover,
                   guint                  keyval,
                   guint                  keycode,
                   GdkModifierType        state,
                   GtkEventControllerKey *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

	return GDK_EVENT_PROPAGATE;
}

static void
on_focus_enter_cb (GtkSourceHover          *hover,
                   GtkEventControllerFocus *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (controller));

}

static void
on_focus_leave_cb (GtkSourceHover          *hover,
                   GtkEventControllerFocus *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_IS_EVENT_CONTROLLER_FOCUS (controller));

}

static void
on_motion_cb (GtkSourceHover           *hover,
              double                    x,
              double                    y,
              GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

}

static gboolean
on_scroll_cb (GtkSourceHover           *hover,
              double                    dx,
              double                    dy,
              GtkEventControllerScroll *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (controller));

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_hover_dispose (GObject *object)
{
	GtkSourceHover *self = (GtkSourceHover *)object;

	g_clear_pointer (&self->assistant, _gtk_source_assistant_destroy);

	g_clear_weak_pointer (&self->view);

	g_clear_handle_id (&self->delay_display_source, g_source_remove);
	g_clear_handle_id (&self->dismiss_source, g_source_remove);

	if (self->providers->len > 0)
	{
		g_ptr_array_remove_range (self->providers, 0, self->providers->len);
	}

	G_OBJECT_CLASS (gtk_source_hover_parent_class)->dispose (object);
}

static void
gtk_source_hover_finalize (GObject *object)
{
	GtkSourceHover *self = (GtkSourceHover *)object;

	g_clear_pointer (&self->providers, g_ptr_array_unref);

	g_assert (self->delay_display_source == 0);
	g_assert (self->dismiss_source == 0);

	G_OBJECT_CLASS (gtk_source_hover_parent_class)->finalize (object);
}

static void
gtk_source_hover_class_init (GtkSourceHoverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_hover_dispose;
	object_class->finalize = gtk_source_hover_finalize;
}

static void
gtk_source_hover_init (GtkSourceHover *self)
{
	self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkSourceHover *
_gtk_source_hover_new (GtkSourceView *view)
{
	GtkSourceHover *self;
	GtkEventController *focus;
	GtkEventController *key;
	GtkEventController *motion;
	GtkEventController *scroll;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	self = g_object_new (GTK_SOURCE_TYPE_HOVER, NULL);

	g_set_weak_pointer (&self->view, view);

	key = gtk_event_controller_key_new ();
	g_signal_connect_object (key,
				 "key-pressed",
				 G_CALLBACK (on_key_pressed_cb),
				 self,
				 G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), key);

	focus = gtk_event_controller_focus_new ();
	g_signal_connect_object (focus,
				 "enter",
				 G_CALLBACK (on_focus_enter_cb),
				 self,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (focus,
				 "leave",
				 G_CALLBACK (on_focus_leave_cb),
				 self,
				 G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), focus);

	motion = gtk_event_controller_motion_new ();
	g_signal_connect_object (motion,
				 "motion",
				 G_CALLBACK (on_motion_cb),
				 self,
				 G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), motion);

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect_object (scroll,
				 "scroll",
				 G_CALLBACK (on_scroll_cb),
				 self,
				 G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), scroll);

	return self;
}

void
gtk_source_hover_add_provider (GtkSourceHover         *self,
                               GtkSourceHoverProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER (self));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_PROVIDER (provider));

	for (guint i = 0; i < self->providers->len; i++)
	{
		if (g_ptr_array_index (self->providers, i) == (gpointer)provider)
		{
			return;
		}
	}

	g_ptr_array_add (self->providers, g_object_ref (provider));
}

void
gtk_source_hover_remove_provider (GtkSourceHover         *self,
                                  GtkSourceHoverProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER (self));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_PROVIDER (provider));

	for (guint i = 0; i < self->providers->len; i++)
	{
		if (g_ptr_array_index (self->providers, i) == (gpointer)provider)
		{
			g_ptr_array_remove_index (self->providers, i);
			return;
		}
	}
}

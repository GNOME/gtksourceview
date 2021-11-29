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
#include "gtksourcehoverassistant-private.h"
#include "gtksourcehovercontext.h"
#include "gtksourcehoverprovider.h"
#include "gtksourceiter-private.h"
#include "gtksourcesignalgroup-private.h"
#include "gtksourceview-private.h"

#define DEFAULT_HOVER_DELAY 500

/**
 * SECTION:hover
 * @Title: GtkSourceHover
 * @Short_description: Interactive tooltips
 * @See_also: #GtkSourceHoverProvider, #GtkSourceHoverDisplay, and
 *   #GtkSourceHoverContext.
 *
 * #GtkSourceHover allows a #GtkSourceView to provide contextual information.
 * When enabled, if the user hovers over a word in the text editor, a series
 * of registered #GtkSourceHoverProvider can populate a #GtkSourceHoverDisplay
 * with useful information.
 *
 * To enable call gtk_source_view_get_hover() and add #GtkSourceHoverProvider
 * using gtk_source_hover_add_provider(). To disable, remove all registered
 * providers with gtk_source_hover_remove_provider().
 *
 * You can change how long to wait to display the interactive tooltip by
 * setting the #GtkSourceHover:hover-delay property in milliseconds.
 */

struct _GtkSourceHover
{
	GObject             parent_instance;

	GtkSourceView      *view;
	GtkSourceAssistant *assistant;

	GPtrArray          *providers;

	double              motion_x;
	double              motion_y;

	guint               hover_delay;
	guint               settle_source;
};

G_DEFINE_TYPE (GtkSourceHover, gtk_source_hover, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_HOVER_DELAY,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gboolean
gtk_source_hover_get_bounds (GtkSourceHover *self,
                             GtkTextIter    *begin,
                             GtkTextIter    *end,
                             GtkTextIter    *location)
{
	GtkTextIter iter;
	int x, y;

	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (!self->view || GTK_SOURCE_IS_VIEW (self->view));
	g_assert (begin != NULL);
	g_assert (end != NULL);
	g_assert (location != NULL);

	memset (begin, 0, sizeof *begin);
	memset (end, 0, sizeof *end);
	memset (location, 0, sizeof *location);

	if (self->view == NULL)
	{
		return FALSE;
	}

	g_assert (GTK_SOURCE_IS_VIEW (self->view));

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self->view),
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       self->motion_x,
	                                       self->motion_y,
	                                       &x, &y);

	if (!gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (self->view), &iter, x, y))
	{
		return FALSE;
	}

	*location = iter;

	if (!_gtk_source_iter_inside_word (&iter))
	{
		*begin = iter;
		gtk_text_iter_set_line_offset (begin, 0);

		*end = iter;
		gtk_text_iter_forward_to_line_end (end);

		return TRUE;
	}

	if (!_gtk_source_iter_starts_full_word (&iter))
	{
		_gtk_source_iter_backward_extra_natural_word_start (&iter);
	}

	*begin = iter;
	*end = iter;

	_gtk_source_iter_forward_extra_natural_word_end (end);

	return TRUE;
}

static gboolean
gtk_source_hover_settled_cb (GtkSourceHover *self)
{
	GtkTextIter begin;
	GtkTextIter end;
	GtkTextIter location;

	g_assert (GTK_SOURCE_IS_HOVER (self));

	self->settle_source = 0;

	if (gtk_source_hover_get_bounds (self, &begin, &end, &location))
	{
		_gtk_source_hover_assistant_display (GTK_SOURCE_HOVER_ASSISTANT (self->assistant),
		                                     (GtkSourceHoverProvider **)self->providers->pdata,
		                                     self->providers->len,
		                                     &begin, &end, &location);
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_hover_queue_settle (GtkSourceHover *self)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));

	g_clear_handle_id (&self->settle_source, g_source_remove);
	self->settle_source = g_timeout_add (self->hover_delay,
	                                     (GSourceFunc) gtk_source_hover_settled_cb,
	                                     self);
}

static gboolean
gtk_source_hover_key_pressed_cb (GtkSourceHover        *self,
                                 guint                  keyval,
                                 guint                  keycode,
                                 GdkModifierType        state,
                                 GtkEventControllerKey *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (controller));

	g_clear_handle_id (&self->settle_source, g_source_remove);
	_gtk_source_hover_assistant_dismiss (GTK_SOURCE_HOVER_ASSISTANT (self->assistant));

	return GDK_EVENT_PROPAGATE;
}


static void
gtk_source_hover_motion_cb (GtkSourceHover           *self,
                            double                    x,
                            double                    y,
                            GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	self->motion_x = x;
	self->motion_y = y;

	gtk_source_hover_queue_settle (self);
}

static void
gtk_source_hover_leave_cb (GtkSourceHover           *self,
                           GtkEventControllerMotion *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_MOTION (controller));

	g_clear_handle_id (&self->settle_source, g_source_remove);
}

static gboolean
gtk_source_hover_scroll_cb (GtkSourceHover           *self,
                            double                    dx,
                            double                    dy,
                            GtkEventControllerScroll *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (controller));

	g_clear_handle_id (&self->settle_source, g_source_remove);
	_gtk_source_hover_assistant_dismiss (GTK_SOURCE_HOVER_ASSISTANT (self->assistant));

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_hover_dispose (GObject *object)
{
	GtkSourceHover *self = (GtkSourceHover *)object;

	g_clear_handle_id (&self->settle_source, g_source_remove);
	g_clear_pointer (&self->assistant, _gtk_source_assistant_destroy);
	g_clear_weak_pointer (&self->view);

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

	g_assert (self->assistant == NULL);
	g_assert (self->settle_source == 0);

	G_OBJECT_CLASS (gtk_source_hover_parent_class)->finalize (object);
}

static void
gtk_source_hover_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
	GtkSourceHover *self = GTK_SOURCE_HOVER (object);

	switch (prop_id) {
	case PROP_HOVER_DELAY:
		g_value_set_uint (value, self->hover_delay);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_hover_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
	GtkSourceHover *self = GTK_SOURCE_HOVER (object);

	switch (prop_id) {
	case PROP_HOVER_DELAY:
		self->hover_delay = g_value_get_uint (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_hover_class_init (GtkSourceHoverClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_hover_dispose;
	object_class->finalize = gtk_source_hover_finalize;
	object_class->get_property = gtk_source_hover_get_property;
	object_class->set_property = gtk_source_hover_set_property;

	/**
	 * GtkSourceHover:hover-delay:
	 *
	 * The "hover-delay" property contains the number of milliseconds to
	 * delay before showing the hover assistant.
	 */
	properties [PROP_HOVER_DELAY] =
		g_param_spec_uint ("hover-delay",
		                   "Hover Delay",
		                   "Number of milliseconds to delay before showing assistant",
		                   1, 5000, DEFAULT_HOVER_DELAY,
		                   (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_hover_init (GtkSourceHover *self)
{
	self->providers = g_ptr_array_new_with_free_func (g_object_unref);
	self->hover_delay = DEFAULT_HOVER_DELAY;
}

GtkSourceHover *
_gtk_source_hover_new (GtkSourceView *view)
{
	GtkSourceHover *self;
	GtkEventController *key;
	GtkEventController *motion;
	GtkEventController *scroll;

	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), NULL);

	self = g_object_new (GTK_SOURCE_TYPE_HOVER, NULL);
	g_set_weak_pointer (&self->view, view);
	self->assistant = _gtk_source_hover_assistant_new ();
	_gtk_source_view_add_assistant (view, self->assistant);

	key = gtk_event_controller_key_new ();
	g_signal_connect_object (key,
	                         "key-pressed",
	                         G_CALLBACK (gtk_source_hover_key_pressed_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (view), key);

	motion = gtk_event_controller_motion_new ();
	g_signal_connect_object (motion,
	                         "leave",
	                         G_CALLBACK (gtk_source_hover_leave_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (motion,
	                         "motion",
	                         G_CALLBACK (gtk_source_hover_motion_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (view), motion);

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect_object (scroll,
	                         "scroll",
	                         G_CALLBACK (gtk_source_hover_scroll_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (view), scroll);

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

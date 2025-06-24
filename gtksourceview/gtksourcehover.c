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

#include "gtksourceannotation-private.h"
#include "gtksourceannotations-private.h"
#include "gtksourceannotationprovider-private.h"
#include "gtksourceassistant-private.h"
#include "gtksourcebuffer.h"
#include "gtksourcehover-private.h"
#include "gtksourcehoverassistant-private.h"
#include "gtksourcehovercontext.h"
#include "gtksourcehoverprovider.h"
#include "gtksourceiter-private.h"
#include "gtksourceview-private.h"

#define DEFAULT_HOVER_DELAY 500

/**
 * GtkSourceHover:
 *
 * Interactive tooltips.
 *
 * `GtkSourceHover` allows a [class@View] to provide contextual information.
 * When enabled, if the user hovers over a word in the text editor, a series
 * of registered [iface@HoverProvider] can populate a [class@HoverDisplay]
 * with useful information.
 *
 * To enable call [method@View.get_hover] and add [iface@HoverProvider]
 * using [method@Hover.add_provider]. To disable, remove all registered
 * providers with [method@Hover.remove_provider].
 *
 * You can change how long to wait to display the interactive tooltip by
 * setting the [property@Hover:hover-delay] property in milliseconds.
 */

struct _GtkSourceHover
{
	GObject             parent_instance;

	GtkSourceView      *view;
	GtkSourceAssistant *assistant;
	GtkTextBuffer      *buffer;

	GPtrArray          *providers;

	double              motion_x;
	double              motion_y;

	guint               hover_delay;

	GSource            *settle_source;

	guint               in_click : 1;
};

G_DEFINE_TYPE (GtkSourceHover, gtk_source_hover, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_HOVER_DELAY,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void
gtk_source_hover_dismiss (GtkSourceHover *self)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));

	g_clear_pointer (&self->settle_source, g_source_destroy);

	if (self->assistant != NULL)
	{
		_gtk_source_hover_assistant_dismiss (GTK_SOURCE_HOVER_ASSISTANT (self->assistant));
	}
}

static void
cursor_moved_cb (GtkSourceHover  *self,
		 GtkSourceBuffer *buffer)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));

	if (!self->in_click)
	{
		gtk_source_hover_dismiss (self);
	}
}

static void
gtk_source_hover_notify_buffer (GtkSourceHover *hover,
                                GParamSpec     *pspec,
                                GtkSourceView  *view)
{
	GtkTextBuffer *buffer;

	g_assert (GTK_SOURCE_IS_HOVER (hover));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (buffer == hover->buffer)
	{
		return;
	}

	if (hover->buffer != NULL)
	{
		g_signal_handlers_disconnect_by_func (hover->buffer,
						      G_CALLBACK (cursor_moved_cb),
						      hover);
		g_clear_weak_pointer (&hover->buffer);
	}

	if (!GTK_SOURCE_IS_BUFFER (buffer))
	{
		return;
	}

	if (g_set_weak_pointer (&hover->buffer, buffer))
	{
		g_signal_connect_object (hover->buffer,
					 "cursor-moved",
					 G_CALLBACK (cursor_moved_cb),
					 hover,
					 G_CONNECT_SWAPPED);
	}
}

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

	if (g_unichar_isspace (gtk_text_iter_get_char (&iter)))
	{
		return FALSE;
	}

	*begin = *end = iter;

	while (!gtk_text_iter_starts_line (begin))
	{
		gtk_text_iter_backward_char (begin);

		if (g_unichar_isspace (gtk_text_iter_get_char (begin)))
		{
			gtk_text_iter_forward_char (begin);
			break;
		}
	}

	while (!gtk_text_iter_ends_line (end))
	{
		if (g_unichar_isspace (gtk_text_iter_get_char (end)))
		{
			break;
		}

		if (!gtk_text_iter_forward_char (end))
		{
			break;
		}
	}

	*location = iter;

	return TRUE;
}

static gboolean
gtk_source_hover_get_annotation (GtkSourceHover               *self,
                                 GtkSourceAnnotationProvider **provider_out,
                                 GtkSourceAnnotation         **annotation_out)
{
	GtkSourceAnnotations *annotations;
	GtkSourceGutter *left;
	GPtrArray *providers;
	guint i, j;
	int gutter_width = 0;

	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (!self->view || GTK_SOURCE_IS_VIEW (self->view));

	if (self->view == NULL || provider_out == NULL || annotation_out == NULL)
	{
		return FALSE;
	}

	left = gtk_source_view_get_gutter (self->view, GTK_TEXT_WINDOW_LEFT);

	if (left != NULL)
	{
		gutter_width = gtk_widget_get_width (GTK_WIDGET (left));
	}

	annotations = gtk_source_view_get_annotations (self->view);
	providers = _gtk_source_annotations_get_providers (annotations);

	for (i = 0; i < providers->len; i++)
	{
		GtkSourceAnnotationProvider *provider = g_ptr_array_index (providers, i);
		GPtrArray *annotations_array = _gtk_source_annotation_provider_get_annotations (provider);

		for (j = 0; j < annotations_array->len; j++)
		{
			GtkSourceAnnotation *annotation = g_ptr_array_index (annotations_array, j);

			if (_gtk_source_annotation_contains_point (annotation, self->motion_x - gutter_width, self->motion_y))
			{
				*provider_out = provider;
				*annotation_out = annotation;
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
gtk_source_hover_settled_cb (GtkSourceHover *self)
{
	GtkTextIter begin;
	GtkTextIter end;
	GtkTextIter location;
	GtkSourceAnnotation *annotation;
	GtkSourceAnnotationProvider *provider;

	g_assert (GTK_SOURCE_IS_HOVER (self));

	g_clear_pointer (&self->settle_source, g_source_destroy);

	if (gtk_source_hover_get_bounds (self, &begin, &end, &location))
	{
		_gtk_source_hover_assistant_display (GTK_SOURCE_HOVER_ASSISTANT (self->assistant),
		                                     (GtkSourceHoverProvider **)self->providers->pdata,
		                                     self->providers->len,
		                                     &begin, &end, &location);
	}
	else if (gtk_source_hover_get_annotation (self, &provider, &annotation))
	{
		_gtk_source_hover_assistant_display_annotation (GTK_SOURCE_HOVER_ASSISTANT (self->assistant),
		                                                provider,
		                                                annotation);
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_hover_queue_settle (GtkSourceHover *self)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));

	if G_LIKELY (self->settle_source != NULL)
	{
		gint64 ready_time = g_get_monotonic_time () + (1000L * self->hover_delay);
		g_source_set_ready_time (self->settle_source, ready_time);
		return;
	}

	self->settle_source = g_timeout_source_new (self->hover_delay);
	g_source_set_callback (self->settle_source, (GSourceFunc)gtk_source_hover_settled_cb, self, NULL);
	g_source_set_name (self->settle_source, "gtk-source-hover-settle");
	g_source_attach (self->settle_source, g_main_context_default ());
	g_source_unref (self->settle_source);
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

	gtk_source_hover_dismiss (self);

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

	/* Ignore synthesized motion events */
	if (self->motion_x == x && self->motion_y == y)
	{
		return;
	}

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

	g_clear_pointer (&self->settle_source, g_source_destroy);
}

static gboolean
gtk_source_hover_scroll_cb (GtkSourceHover           *self,
                            double                    dx,
                            double                    dy,
                            GtkEventControllerScroll *controller)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_SCROLL (controller));

	gtk_source_hover_dismiss (self);

	return GDK_EVENT_PROPAGATE;
}

static void
gtk_source_hover_dispose (GObject *object)
{
	GtkSourceHover *self = (GtkSourceHover *)object;


	if (self->providers->len > 0)
	{
		g_ptr_array_remove_range (self->providers, 0, self->providers->len);
	}

	g_clear_pointer (&self->settle_source, g_source_destroy);
	g_clear_pointer (&self->assistant, _gtk_source_assistant_destroy);

	g_clear_weak_pointer (&self->view);
	g_clear_weak_pointer (&self->buffer);

	G_OBJECT_CLASS (gtk_source_hover_parent_class)->dispose (object);
}

static void
gtk_source_hover_finalize (GObject *object)
{
	GtkSourceHover *self = (GtkSourceHover *)object;

	g_clear_pointer (&self->providers, g_ptr_array_unref);

	g_assert (self->assistant == NULL);
	g_assert (self->settle_source == NULL);

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
	 * Contains the number of milliseconds to delay before showing the hover assistant.
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

static void
gtk_source_hover_click_pressed_cb (GtkSourceHover  *self,
                                   int              n_press,
                                   double           x,
                                   double           y,
                                   GtkGestureClick *click)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	self->in_click = TRUE;
}

static void
gtk_source_hover_click_released_cb (GtkSourceHover  *self,
                                    int              n_press,
                                    double           x,
                                    double           y,
                                    GtkGestureClick *click)
{
	g_assert (GTK_SOURCE_IS_HOVER (self));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	self->in_click = FALSE;
}

GtkSourceHover *
_gtk_source_hover_new (GtkSourceView *view)
{
	GtkSourceHover *self;
	GtkEventController *key;
	GtkEventController *motion;
	GtkEventController *scroll;
	GtkEventController *click;

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

	click = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
	g_signal_connect_object (click,
	                         "pressed",
	                         G_CALLBACK (gtk_source_hover_click_pressed_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (click,
	                         "released",
	                         G_CALLBACK (gtk_source_hover_click_released_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_event_controller_set_propagation_phase (click, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller (GTK_WIDGET (view), click);

	scroll = gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	g_signal_connect_object (scroll,
	                         "scroll",
	                         G_CALLBACK (gtk_source_hover_scroll_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (view), scroll);

	g_signal_connect_object (view,
	                         "notify::buffer",
	                         G_CALLBACK (gtk_source_hover_notify_buffer),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_source_hover_notify_buffer (self, NULL, view);

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

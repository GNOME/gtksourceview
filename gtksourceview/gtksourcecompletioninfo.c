/*
 * This file is part of GtkSourceView
 *
 * Copyright 2007-2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright 2009 Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright 2012 Sébastien Wilmet <swilmet@gnome.org>
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
 * SECTION:completioninfo
 * @title: GtkSourceCompletionInfo
 * @short_description: Calltips object
 *
 * This object can be used to show a calltip or help for the
 * current completion proposal.
 *
 * The info window has always the same size as the natural size of its child
 * widget, added with gtk_container_add().  If you want a fixed size instead, a
 * possibility is to use a scrolled window, as the following example
 * demonstrates.
 *
 * <example>
 *   <title>Fixed size with a scrolled window.</title>
 *   <programlisting>
 * GtkSourceCompletionInfo *info;
 * GtkWidget *your_widget;
 * GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
 *
 * gtk_widget_set_size_request (scrolled_window, 300, 200);
 * gtk_container_add (GTK_CONTAINER (scrolled_window), your_widget);
 * gtk_container_add (GTK_CONTAINER (info), scrolled_window);
 *   </programlisting>
 * </example>
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "gtksourcecompletioninfo-private.h"

struct _GtkSourceCompletionInfo
{
	GtkWindow parent_instance;

	guint idle_resize;

	GtkWidget *attached_to;
	GtkEventController *key;
	gulong focus_out_event_handler;

	gint xoffset;

	guint transient_set : 1;
};

G_DEFINE_TYPE (GtkSourceCompletionInfo, gtk_source_completion_info, GTK_TYPE_WINDOW);

/* Resize the window */

/* Init, dispose, finalize, ... */

static void
set_attached_to (GtkSourceCompletionInfo *info,
                 GtkWidget               *attached_to)
{
	if (info->attached_to == attached_to)
	{
		return;
	}

	if (info->attached_to != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (info->attached_to),
					      (gpointer *) &info->attached_to);

		if (info->focus_out_event_handler != 0)
		{
			g_signal_handler_disconnect (info->key,
						     info->focus_out_event_handler);

			info->focus_out_event_handler = 0;
			info->key = NULL;
		}
	}

	info->attached_to = attached_to;
	info->key = NULL;

	if (attached_to == NULL)
	{
		return;
	}

	g_object_add_weak_pointer (G_OBJECT (attached_to),
				   (gpointer *) &info->attached_to);

	info->key = gtk_event_controller_key_new ();
	gtk_widget_add_controller (GTK_WIDGET (attached_to), info->key);

	info->focus_out_event_handler =
		g_signal_connect_swapped (info->key,
					  "focus-out",
					  G_CALLBACK (gtk_widget_hide),
					  info);

	info->transient_set = FALSE;
}

static void
gtk_source_completion_info_init (GtkSourceCompletionInfo *info)
{
	/* Tooltip style */
	gtk_window_set_title (GTK_WINDOW (info), _("Completion Info"));
	gtk_widget_set_name (GTK_WIDGET (info), "gtk-tooltip");

	g_object_set (info,
	              "margin-top", 1,
	              "margin-bottom", 1,
	              "margin-start", 1,
	              "margin-end", 1,
	              NULL);
}

static void
gtk_source_completion_info_dispose (GObject *object)
{
	GtkSourceCompletionInfo *info = GTK_SOURCE_COMPLETION_INFO (object);

	if (info->idle_resize != 0)
	{
		g_source_remove (info->idle_resize);
		info->idle_resize = 0;
	}

	set_attached_to (info, NULL);

	G_OBJECT_CLASS (gtk_source_completion_info_parent_class)->dispose (object);
}

static void
gtk_source_completion_info_show (GtkWidget *widget)
{
	GtkSourceCompletionInfo *info = GTK_SOURCE_COMPLETION_INFO (widget);

	if (info->attached_to != NULL && !info->transient_set)
	{
		GtkRoot *toplevel;

		toplevel = gtk_widget_get_root (GTK_WIDGET (info->attached_to));

		if (toplevel != NULL)
		{
			gtk_window_set_transient_for (GTK_WINDOW (info),
			                              GTK_WINDOW (toplevel));
			info->transient_set = TRUE;
		}
	}

	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->show (widget);
}

static void
gtk_source_completion_info_class_init (GtkSourceCompletionInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gtk_source_completion_info_dispose;

	widget_class->show = gtk_source_completion_info_show;
}

void
_gtk_source_completion_info_set_xoffset (GtkSourceCompletionInfo *window,
					 gint                     xoffset)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_INFO (window));

	window->xoffset = xoffset;
}

static void
move_to_iter (GtkSourceCompletionInfo *window,
              GtkTextView             *view,
              GtkTextIter             *iter)
{
	GdkRectangle location;
	GdkPopupLayout *layout;
	GdkSurface *surface;
	GtkRoot *root;

	if (!GTK_IS_NATIVE (window))
		return;

	surface = gtk_native_get_surface (GTK_NATIVE (window));
	if (surface == NULL)
		return;

	root = gtk_widget_get_root (GTK_WIDGET (view));
	if (root == NULL)
		return;

	gtk_text_view_get_iter_location (view, iter, &location);
	gtk_text_view_buffer_to_window_coords (view,
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       location.x,
	                                       location.y,
	                                       &location.x,
	                                       &location.y);

	gtk_widget_translate_coordinates (GTK_WIDGET (view),
	                                  GTK_WIDGET (root),
	                                  location.x + window->xoffset,
	                                  location.y,
	                                  &location.x,
	                                  &location.y);

	layout = gdk_popup_layout_new (&location, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST);
	gdk_popup_layout_set_anchor_hints (layout, GDK_ANCHOR_SLIDE | GDK_ANCHOR_FLIP_Y | GDK_ANCHOR_RESIZE);
	gdk_popup_present (GDK_POPUP (surface), window->priv->xoffset, 0, layout);
	gdk_popup_layout_unref (layout);
}

static void
move_to_cursor (GtkSourceCompletionInfo *window,
		GtkTextView             *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter insert;

	buffer = gtk_text_view_get_buffer (view);
	gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert (buffer));

	move_to_iter (window, view, &insert);
}

/* Public functions */

/**
 * gtk_source_completion_info_new:
 *
 * Returns: a new GtkSourceCompletionInfo.
 */
GtkSourceCompletionInfo *
gtk_source_completion_info_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_INFO,
	                     "margin-top", 3,
	                     "margin-bottom", 3,
	                     "margin-start", 3,
	                     "margin-end", 3,
	                     NULL);
}

/**
 * gtk_source_completion_info_move_to_iter:
 * @info: a #GtkSourceCompletionInfo.
 * @view: a #GtkTextView on which the info window should be positioned.
 * @iter: (nullable): a #GtkTextIter.
 *
 * Moves the #GtkSourceCompletionInfo to @iter. If @iter is %NULL @info is
 * moved to the cursor position. Moving will respect the #GdkGravity setting
 * of the info window and will ensure the line at @iter is not occluded by
 * the window.
 */
void
gtk_source_completion_info_move_to_iter (GtkSourceCompletionInfo *info,
                                         GtkTextView             *view,
                                         GtkTextIter             *iter)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_INFO (info));
	g_return_if_fail (GTK_IS_TEXT_VIEW (view));

	if (iter == NULL)
	{
		move_to_cursor (info, view);
	}
	else
	{
		move_to_iter (info, view, iter);
	}
}

/* gtksourcedriver-private.h
 *
 * Copyright 2025 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtksourceview-private.h"

G_BEGIN_DECLS

typedef struct _GtkSourceDriver
{
  GtkSourceView   *view;
  GtkSourceBuffer *buffer;
  GtkAdjustment   *vadjustment;
  gulong           buffer_changed;
  gulong           notify_vadjustment;
  gulong           notify_changed;
  guint            in_snapshot;
} GtkSourceDriver;

static inline void
_gtk_source_driver_update (GtkSourceDriver *driver)
{
	if (driver->in_snapshot == 0)
	{
		//GdkRectangle visible_rect;

		//gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (driver->view), &visible_rect);
		//_gtk_source_view_ensure_redrawn_rect_is_highlighted (driver->view, &visible_rect);

		gtk_widget_queue_resize (GTK_WIDGET (driver->view));
	}
	else
	{
		/* Queue until after this draw completes */
	}
}

static inline void
_gtk_source_driver_notify_value (GtkSourceDriver *driver,
                                 GParamSpec      *pspec,
                                 GtkAdjustment   *adjustment)
{
	_gtk_source_driver_update (driver);
}

static inline void
_gtk_source_driver_notify_vadjustment (GtkSourceDriver *driver,
                                       GParamSpec      *pspec,
                                       GtkSourceView   *view)
{
	GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));

	if (adjustment == driver->vadjustment)
		return;

	if (driver->vadjustment)
	{
		g_clear_signal_handler (&driver->notify_changed, driver->vadjustment);
		g_clear_object (&driver->vadjustment);
	}

	if (g_set_object (&driver->vadjustment, adjustment))
	{
		driver->notify_changed =
			g_signal_connect_swapped (driver->vadjustment,
			                          "notify::value",
			                          G_CALLBACK (_gtk_source_driver_notify_value),
			                          driver);
	}
}

static inline void
_gtk_source_driver_buffer_changed (GtkSourceDriver *driver,
                                   GtkSourceBuffer *buffer)
{
}

static inline void
_gtk_source_driver_init (GtkSourceDriver *driver,
                         GtkSourceView   *view)
{
	memset (driver, 0, sizeof *driver);

	driver->view = view;

	driver->notify_vadjustment =
		g_signal_connect_swapped (view,
		                          "notify::vadjustment",
		                          G_CALLBACK (_gtk_source_driver_notify_vadjustment),
		                          driver);

	_gtk_source_driver_notify_vadjustment (driver, NULL, view);
}

static inline void
_gtk_source_driver_clear (GtkSourceDriver *driver)
{
	/* Must be safe to call multiple times from dispose */

	g_clear_signal_handler (&driver->buffer_changed, driver->buffer);
	g_clear_signal_handler (&driver->notify_changed, driver->vadjustment);
	g_clear_signal_handler (&driver->notify_vadjustment, driver->view);

	g_clear_object (&driver->vadjustment);

	memset (driver, 0, sizeof *driver);
}

static inline void
_gtk_source_driver_set_buffer (GtkSourceDriver *driver,
                               GtkSourceBuffer *buffer)
{
	if (driver->buffer == buffer)
		return;

	if (driver->buffer)
	{
		g_clear_signal_handler (&driver->buffer_changed, driver->buffer);
		driver->buffer = NULL;
	}

	driver->buffer = buffer;

	if (buffer)
	{
		driver->buffer_changed =
			g_signal_connect_swapped (buffer,
			                          "changed",
			                          G_CALLBACK (_gtk_source_driver_buffer_changed),
			                          driver);
	}
}

static inline void
_gtk_source_driver_size_allocated (GtkSourceDriver *driver)
{
	_gtk_source_driver_update (driver);
}

static inline void
_gtk_source_driver_begin_snapshot (GtkSourceDriver *driver)
{
	driver->in_snapshot++;
}

static inline void
_gtk_source_driver_end_snapshot (GtkSourceDriver *driver)
{
	driver->in_snapshot--;
}

G_END_DECLS

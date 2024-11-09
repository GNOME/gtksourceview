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

#include <gtk/gtk.h>

#include "gtksourcescheduler.h"

/**
 * SECTION:scheduler
 * @title: Work Scheduling
 * @short_description: scheduling background work
 *
 */

/* The goal of this GSource is to let us pile on a bunch of background work but
 * only do a small amount of it at a time per-frame cycle. This becomes more
 * important when you have multiple documents open all competing to do
 * background work and potentially stalling the main loop.
 *
 * Instead, we only do 1 msec of work at a time and then wait for the next
 * frame to come in.
 *
 * Since we don't have access to the widgets, we need to base this work off the
 * shortest delay between frames among the available monitors. Some aliasing is
 * still possible depending on per-monitor scan-outs, but we already have that
 * issue without this.
 */

#define _1_MSEC (G_USEC_PER_SEC / 1000L)

typedef struct
{
  GSource source;
  GQueue queue;
  gint64 interval;
  gsize last_handler_id;
} GtkSourceScheduler;

typedef struct
{
  GList link;
  GtkSourceSchedulerCallback callback;
  gpointer user_data;
  GDestroyNotify notify;
  gint64 ready_time;
  gsize id;
} GtkSourceTask;

static GSource *the_source;

static void
gtk_source_task_free (GtkSourceTask *task)
{
	g_assert (task != NULL);
	g_assert (task->link.data == (gpointer)task);
	g_assert (task->link.next == NULL);
	g_assert (task->link.prev == NULL);
	g_assert (task->callback != NULL);

	if (task->notify != NULL)
	{
		task->notify (task->user_data);
	}

	g_slice_free (GtkSourceTask, task);
}

static GtkSourceTask *
gtk_source_task_new (GtkSourceSchedulerCallback callback,
                     gpointer                   user_data,
                     GDestroyNotify             notify)
{
	GtkSourceTask *task;

	g_return_val_if_fail (callback != NULL, NULL);

	task = g_slice_new0 (GtkSourceTask);
	task->link.data = task;
	task->callback = callback;
	task->user_data = user_data;
	task->notify = notify;
	task->ready_time = 0; /* Now */
	task->id = 0;

	return task;
}

static gint64
get_interval (GtkSourceScheduler *self)
{
	if G_UNLIKELY (self->interval == 0)
	{
		GdkDisplay *display = gdk_display_get_default ();
		GListModel *monitors = gdk_display_get_monitors (display);
		guint n_items = g_list_model_get_n_items (monitors);
		gint64 lowest_interval = 60000;

		for (guint i = 0; i < n_items; i++)
		{
			GdkMonitor *monitor = g_list_model_get_item (monitors, i);
			gint64 interval = gdk_monitor_get_refresh_rate (monitor);

			if (interval != 0 && interval < lowest_interval)
				lowest_interval = interval;

			g_object_unref (monitor);
		}

		self->interval = (double)G_USEC_PER_SEC / (double)lowest_interval * 1000.0;
	}

	return self->interval;
}

static gboolean
gtk_source_scheduler_prepare (GSource *source,
                              int     *timeout)
{
	*timeout = -1;
	return FALSE;
}

static gboolean
gtk_source_scheduler_check (GSource *source)
{
	GtkSourceScheduler *self = (GtkSourceScheduler *)source;
	GtkSourceTask *task = g_queue_peek_head (&self->queue);

	return task != NULL && task->ready_time <= g_source_get_time (source);
}

static gboolean
gtk_source_scheduler_dispatch (GSource     *source,
                               GSourceFunc  source_func,
                               gpointer     user_data)
{
	GtkSourceScheduler *self = (GtkSourceScheduler *)source;
	gint64 current = g_source_get_time (source);
	gint64 deadline = current + _1_MSEC;
	gint64 interval = get_interval (self);

	/* Try to process as many items within our quanta if they */
	while (g_get_monotonic_time () < deadline)
	{
		GtkSourceTask *task = g_queue_peek_head (&self->queue);

		if (task == NULL)
			break;

		g_queue_unlink (&self->queue, &task->link);

		if (task->callback (deadline, task->user_data))
		{
			task->ready_time = current + interval;
			g_queue_push_tail_link (&self->queue, &task->link);
			continue;
		}

		gtk_source_task_free (task);
	}

	if (self->queue.head != NULL)
	{
		GtkSourceTask *task = g_queue_peek_head (&self->queue);
		g_source_set_ready_time (source, task->ready_time);
		return G_SOURCE_CONTINUE;
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_scheduler_finalize (GSource *source)
{
#ifndef G_DISABLE_ASSERT
	GtkSourceScheduler *self = (GtkSourceScheduler *)source;

	g_assert (self->queue.length == 0);
	g_assert (self->queue.head == NULL);
	g_assert (self->queue.tail == NULL);
#endif

	if (source == the_source)
	{
		the_source = NULL;
	}
}

static GSourceFuncs source_funcs = {
	gtk_source_scheduler_prepare,
	gtk_source_scheduler_check,
	gtk_source_scheduler_dispatch,
	gtk_source_scheduler_finalize,
};

static GtkSourceScheduler *
get_scheduler (void)
{
	if (the_source == NULL)
	{
		the_source = g_source_new (&source_funcs, sizeof (GtkSourceScheduler));
		g_source_set_name (the_source, "GtkSourceScheduler");
		g_source_set_priority (the_source, G_PRIORITY_LOW);
		g_source_set_ready_time (the_source, 0);
		g_source_attach (the_source, g_main_context_default ());
		g_source_unref (the_source);
	}

	return (GtkSourceScheduler *)the_source;
}

/**
 * gtk_source_scheduler_add:
 * @callback: (scope notified) (closure user_data): the callback to execute
 * @user_data: user data for @callback
 *
 * Simplified version of [func@scheduler_add_full].
 *
 * Since: 5.2
 */
gsize
gtk_source_scheduler_add (GtkSourceSchedulerCallback callback,
                          gpointer                   user_data)
{
	return gtk_source_scheduler_add_full (callback, user_data, NULL);
}

/**
 * gtk_source_scheduler_add_full: (rename-to gtk_source_scheduler_add)
 * @callback: (scope notified) (closure user_data) (destroy notify): the callback to execute
 * @user_data: user data for @callback
 * @notify: closure notify for @user_data
 *
 * Adds a new callback that will be executed as time permits on the main thread.
 *
 * This is useful when you need to do a lot of background work but want to do
 * it incrementally.
 *
 * @callback will be provided a deadline that it should complete it's work by
 * (or near) and can be checked using [func@GLib.get_monotonic_time] for comparison.
 *
 * Use [func@scheduler_remove] to remove the handler.
 *
 * Since: 5.2
 */
gsize
gtk_source_scheduler_add_full (GtkSourceSchedulerCallback callback,
                               gpointer                   user_data,
                               GDestroyNotify             notify)
{
	GtkSourceScheduler *self;
	GtkSourceTask *task;

	g_return_val_if_fail (callback != NULL, 0);

	self = get_scheduler ();
	task = gtk_source_task_new (callback, user_data, notify);
	task->id = ++self->last_handler_id;

	/* Request progress immediately */
	g_queue_push_head_link (&self->queue, &task->link);
	g_source_set_ready_time ((GSource *)self, g_source_get_time ((GSource *)self));

	return task->id;
}

/**
 * gtk_source_scheduler_remove:
 * @handler_id: the handler id
 *
 * Removes a scheduler callback previously registered with
 * [func@scheduler_add] or [func@scheduler_add_full].
 *
 * Since: 5.2
 */
void
gtk_source_scheduler_remove (gsize handler_id)
{
	GtkSourceScheduler *self;

	g_return_if_fail (handler_id != 0);

	self = get_scheduler ();

	for (const GList *iter = self->queue.head; iter != NULL; iter = iter->next)
	{
		GtkSourceTask *task = iter->data;

		if (task->id == handler_id)
		{
			g_queue_unlink (&self->queue, &task->link);
			gtk_source_task_free (task);
			break;
		}
	}

	if (self->queue.head != NULL)
	{
		GtkSourceTask *task = g_queue_peek_head (&self->queue);
		g_source_set_ready_time ((GSource *)self, task->ready_time);
	}
	else
	{
		g_source_destroy ((GSource *)self);
	}
}

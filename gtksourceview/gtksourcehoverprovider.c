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

#include "gtksourcehovercontext.h"
#include "gtksourcehoverdisplay.h"
#include "gtksourcehoverprovider.h"

/**
 * GtkSourceHoverProvider:
 * 
 * Interface to populate interactive tooltips.
 *
 * `GtkSourceHoverProvider` is an interface that should be implemented to extend
 * the contents of a [class@HoverDisplay]. This is typical in editors that
 * interact external tooling such as those utilizing Language Server Protocol.
 *
 * If you can populate the [class@HoverDisplay] synchronously, use
 * [vfunc@HoverProvider.populate]. Otherwise, interface implementations that
 * may take additional time should use [vfunc@HoverProvider.populate_async]
 * to avoid blocking the main loop.
 */

G_DEFINE_INTERFACE (GtkSourceHoverProvider, gtk_source_hover_provider, G_TYPE_OBJECT)

static gboolean
gtk_source_hover_provider_real_populate (GtkSourceHoverProvider  *provider,
                                         GtkSourceHoverContext   *context,
                                         GtkSourceHoverDisplay   *display,
                                         GError                 **error)
{
	return TRUE;
}

static void
gtk_source_hover_provider_real_populate_async (GtkSourceHoverProvider *provider,
                                               GtkSourceHoverContext  *context,
                                               GtkSourceHoverDisplay  *display,
                                               GCancellable           *cancellable,
                                               GAsyncReadyCallback     callback,
                                               gpointer                user_data)
{
	GError *error = NULL;
	GTask *task;

	task = g_task_new (provider, cancellable, callback, user_data);
	g_task_set_source_tag (task, gtk_source_hover_provider_real_populate_async);

	if (!GTK_SOURCE_HOVER_PROVIDER_GET_IFACE (provider)->populate (provider, context, display, &error))
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, TRUE);

	g_object_unref (task);
}

static gboolean
gtk_source_hover_provider_real_populate_finish (GtkSourceHoverProvider  *self,
                                                GAsyncResult            *result,
                                                GError                 **error)
{
	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gtk_source_hover_provider_default_init (GtkSourceHoverProviderInterface *iface)
{
	iface->populate_async = gtk_source_hover_provider_real_populate_async;
	iface->populate_finish = gtk_source_hover_provider_real_populate_finish;
	iface->populate = gtk_source_hover_provider_real_populate;
}

void
gtk_source_hover_provider_populate_async (GtkSourceHoverProvider *provider,
                                          GtkSourceHoverContext  *context,
                                          GtkSourceHoverDisplay  *display,
                                          GCancellable           *cancellable,
                                          GAsyncReadyCallback     callback,
                                          gpointer                user_data)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_PROVIDER (provider));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_CONTEXT (context));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (display));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	GTK_SOURCE_HOVER_PROVIDER_GET_IFACE (provider)->populate_async (provider, context, display, cancellable, callback, user_data);
}

gboolean
gtk_source_hover_provider_populate_finish (GtkSourceHoverProvider  *provider,
                                           GAsyncResult            *result,
                                           GError                 **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_PROVIDER (provider), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	return GTK_SOURCE_HOVER_PROVIDER_GET_IFACE (provider)->populate_finish (provider, result, error);
}

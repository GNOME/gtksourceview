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

#pragma once

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
# error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gio/gio.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_HOVER_PROVIDER (gtk_source_hover_provider_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkSourceHoverProvider, gtk_source_hover_provider, GTK_SOURCE, HOVER_PROVIDER, GObject)

struct _GtkSourceHoverProviderInterface
{
	GTypeInterface parent_iface;

	gboolean (*populate)        (GtkSourceHoverProvider  *self,
	                             GtkSourceHoverContext   *context,
	                             GtkSourceHoverDisplay   *display,
	                             GError                 **error);
	void     (*populate_async)  (GtkSourceHoverProvider  *self,
	                             GtkSourceHoverContext   *context,
	                             GtkSourceHoverDisplay   *display,
	                             GCancellable            *cancellable,
	                             GAsyncReadyCallback      callback,
	                             gpointer                 user_data);
	gboolean (*populate_finish) (GtkSourceHoverProvider  *self,
	                             GAsyncResult            *result,
	                             GError                 **error);
};

GTK_SOURCE_AVAILABLE_IN_ALL
void     gtk_source_hover_provider_populate_async  (GtkSourceHoverProvider  *self,
                                                    GtkSourceHoverContext   *context,
                                                    GtkSourceHoverDisplay   *display,
                                                    GCancellable            *cancellable,
                                                    GAsyncReadyCallback      callback,
                                                    gpointer                 user_data);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean gtk_source_hover_provider_populate_finish (GtkSourceHoverProvider  *self,
                                                    GAsyncResult                 *result,
                                                    GError                      **error);

G_END_DECLS

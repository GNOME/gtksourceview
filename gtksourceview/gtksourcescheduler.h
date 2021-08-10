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

#include <glib-object.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

typedef gboolean (*GtkSourceSchedulerCallback) (gint64   deadline,
                                                gpointer user_data);

GTK_SOURCE_AVAILABLE_IN_5_2
gsize gtk_source_scheduler_add      (GtkSourceSchedulerCallback callback,
                                     gpointer                   user_data);
GTK_SOURCE_AVAILABLE_IN_5_2
gsize gtk_source_scheduler_add_full (GtkSourceSchedulerCallback callback,
                                     gpointer                   user_data,
                                     GDestroyNotify             notify);
GTK_SOURCE_AVAILABLE_IN_5_2
void gtk_source_scheduler_remove    (gsize handler_id);

static inline void
gtk_source_scheduler_clear (gsize *handler_id_ptr)
{
	gsize val = *handler_id_ptr;

	if (val)
	{
		*handler_id_ptr = 0;
		gtk_source_scheduler_remove (val);
	}
}

G_END_DECLS

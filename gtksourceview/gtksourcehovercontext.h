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

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_HOVER_CONTEXT (gtk_source_hover_context_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceHoverContext, gtk_source_hover_context, GTK_SOURCE, HOVER_CONTEXT, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceView   *gtk_source_hover_context_get_view   (GtkSourceHoverContext *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceBuffer *gtk_source_hover_context_get_buffer (GtkSourceHoverContext *self);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean         gtk_source_hover_context_get_iter   (GtkSourceHoverContext *self,
                                                      GtkTextIter           *iter);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean         gtk_source_hover_context_get_bounds (GtkSourceHoverContext *self,
                                                      GtkTextIter           *begin,
                                                      GtkTextIter           *end);

G_END_DECLS

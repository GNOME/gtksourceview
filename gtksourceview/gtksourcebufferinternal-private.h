/*
 * This file is part of GtkSourceView
 *
 * Copyright 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#pragma once

#include <glib-object.h>
#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_BUFFER_INTERNAL (_gtk_source_buffer_internal_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceBufferInternal, _gtk_source_buffer_internal, GTK_SOURCE, BUFFER_INTERNAL, GObject)

G_GNUC_INTERNAL
GtkSourceBufferInternal *_gtk_source_buffer_internal_get_from_buffer   (GtkSourceBuffer         *buffer);
G_GNUC_INTERNAL
void                     _gtk_source_buffer_internal_emit_search_start (GtkSourceBufferInternal *buffer_internal,
                                                                        GtkSourceSearchContext  *search_context);

G_END_DECLS

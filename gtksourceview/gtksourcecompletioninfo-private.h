/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#include <gtk/gtk.h>

#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_INFO (_gtk_source_completion_info_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceCompletionInfo, _gtk_source_completion_info, GTK_SOURCE, COMPLETION_INFO, GtkSourceAssistant)

GtkSourceCompletionInfo *_gtk_source_completion_info_new      (void);
GtkSourceCompletionCell *_gtk_source_completion_info_get_cell (GtkSourceCompletionInfo *self);

G_END_DECLS

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

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_CELL (gtk_source_completion_cell_get_type())

typedef enum _GtkSourceCompletionColumn
{
	GTK_SOURCE_COMPLETION_COLUMN_ICON       = 0,
	GTK_SOURCE_COMPLETION_COLUMN_BEFORE     = 1,
	GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT = 2,
	GTK_SOURCE_COMPLETION_COLUMN_AFTER      = 3,
	GTK_SOURCE_COMPLETION_COLUMN_COMMENT    = 4,
	GTK_SOURCE_COMPLETION_COLUMN_DETAILS    = 5,
} GtkSourceCompletionColumn;

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionCell, gtk_source_completion_cell, GTK_SOURCE, COMPLETION_CELL, GtkWidget)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionColumn gtk_source_completion_cell_get_column               (GtkSourceCompletionCell *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkWidget                *gtk_source_completion_cell_get_widget               (GtkSourceCompletionCell *self);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_widget               (GtkSourceCompletionCell *self,
                                                                               GtkWidget               *child);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_markup               (GtkSourceCompletionCell *self,
                                                                               const char              *markup);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_icon_name            (GtkSourceCompletionCell *self,
                                                                               const char              *icon_name);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_gicon                (GtkSourceCompletionCell *self,
                                                                               GIcon                   *gicon);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_paintable            (GtkSourceCompletionCell *self,
                                                                               GdkPaintable            *paintable);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_text                 (GtkSourceCompletionCell *self,
                                                                               const char              *text);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_cell_set_text_with_attributes (GtkSourceCompletionCell *self,
                                                                               const char              *text,
                                                                               PangoAttrList           *attrs);

G_END_DECLS

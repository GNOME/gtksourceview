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

#define GTK_SOURCE_TYPE_ASSISTANT_CHILD (_gtk_source_assistant_child_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceAssistantChild, _gtk_source_assistant_child, GTK_SOURCE, ASSISTANT_CHILD, GtkWidget)

GtkSourceAssistantChild *_gtk_source_assistant_child_new          (void);
void                     _gtk_source_assistant_child_hide         (GtkSourceAssistantChild *self);
void                     _gtk_source_assistant_child_set_child    (GtkSourceAssistantChild *self,
                                                                   GtkWidget               *child);
void                     _gtk_source_assistant_child_attach       (GtkSourceAssistantChild *self,
                                                                   GtkSourceAssistant      *child);
void                     _gtk_source_assistant_child_detach       (GtkSourceAssistantChild *self,
                                                                   GtkSourceAssistant      *child);
const GList             *_gtk_source_assistant_child_get_attached (GtkSourceAssistantChild *self);

G_END_DECLS

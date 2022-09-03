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

#define GTK_SOURCE_TYPE_COMPLETION_CONTEXT (gtk_source_completion_context_get_type())

typedef enum _GtkSourceCompletionActivation
{
	GTK_SOURCE_COMPLETION_ACTIVATION_NONE = 0,
	GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE = 1,
	GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED = 2,
} GtkSourceCompletionActivation;

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionContext, gtk_source_completion_context, GTK_SOURCE, COMPLETION_CONTEXT, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletion           *gtk_source_completion_context_get_completion             (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionActivation  gtk_source_completion_context_get_activation             (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_context_get_bounds                 (GtkSourceCompletionContext  *self,
                                                                                         GtkTextIter                 *begin,
                                                                                         GtkTextIter                 *end);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_context_get_empty                  (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
char                          *gtk_source_completion_context_get_word                   (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_context_get_busy                   (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceView                 *gtk_source_completion_context_get_view                   (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceBuffer               *gtk_source_completion_context_get_buffer                 (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceLanguage             *gtk_source_completion_context_get_language               (GtkSourceCompletionContext  *self);
GTK_SOURCE_AVAILABLE_IN_ALL
void                           gtk_source_completion_context_set_proposals_for_provider (GtkSourceCompletionContext  *self,
                                                                                         GtkSourceCompletionProvider *provider,
                                                                                         GListModel                  *results);
GTK_SOURCE_AVAILABLE_IN_5_6
GListModel                    *gtk_source_completion_context_get_proposals_for_provider (GtkSourceCompletionContext  *self,
                                                                                         GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_5_6
GListModel                    *gtk_source_completion_context_list_providers             (GtkSourceCompletionContext  *self);

G_END_DECLS

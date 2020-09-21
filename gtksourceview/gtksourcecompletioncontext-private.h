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

#include "gtksourcecompletioncontext.h"

G_BEGIN_DECLS

GtkSourceCompletionContext  *_gtk_source_completion_context_new              (GtkSourceCompletion            *completion);
gboolean                     _gtk_source_completion_context_get_item_full    (GtkSourceCompletionContext     *self,
                                                                              guint                           position,
                                                                              GtkSourceCompletionProvider   **provider,
                                                                              GtkSourceCompletionProposal   **proposal);
void                         _gtk_source_completion_context_add_provider     (GtkSourceCompletionContext     *self,
                                                                              GtkSourceCompletionProvider    *provider);
void                         _gtk_source_completion_context_remove_provider  (GtkSourceCompletionContext     *self,
                                                                              GtkSourceCompletionProvider    *provider);
gboolean                     _gtk_source_completion_context_can_refilter     (GtkSourceCompletionContext     *self,
                                                                              const GtkTextIter              *begin,
                                                                              const GtkTextIter              *end);
void                         _gtk_source_completion_context_refilter         (GtkSourceCompletionContext     *self);
gboolean                     _gtk_source_completion_context_iter_invalidates (GtkSourceCompletionContext     *self,
                                                                              const GtkTextIter              *iter);
void                         _gtk_source_completion_context_complete_async   (GtkSourceCompletionContext     *self,
                                                                              GtkSourceCompletionActivation   activation,
                                                                              const GtkTextIter              *begin,
                                                                              const GtkTextIter              *end,
                                                                              GCancellable                   *cancellable,
                                                                              GAsyncReadyCallback             callback,
                                                                              gpointer                        user_data);
gboolean                     _gtk_source_completion_context_complete_finish  (GtkSourceCompletionContext     *self,
                                                                              GAsyncResult                   *result,
                                                                              GError                        **error);

G_END_DECLS

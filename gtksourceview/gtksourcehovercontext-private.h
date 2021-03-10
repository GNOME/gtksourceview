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

#include "gtksourcehovercontext.h"

G_BEGIN_DECLS

GtkSourceHoverContext *_gtk_source_hover_context_new            (GtkSourceView           *view,
                                                                 const GtkTextIter       *begin,
                                                                 const GtkTextIter       *end,
                                                                 const GtkTextIter       *location);
void                  _gtk_source_hover_context_add_provider    (GtkSourceHoverContext   *self,
                                                                 GtkSourceHoverProvider  *provider);
void                  _gtk_source_hover_context_populate_async  (GtkSourceHoverContext   *self,
                                                                 GtkSourceHoverDisplay   *display,
                                                                 GCancellable            *cancellable,
                                                                 GAsyncReadyCallback      callback,
                                                                 gpointer                 user_data);
gboolean              _gtk_source_hover_context_populate_finish (GtkSourceHoverContext   *self,
                                                                 GAsyncResult            *result,
                                                                 GError                 **error);


G_END_DECLS

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

#include "gtksourcevimimcontext.h"

G_BEGIN_DECLS

typedef void (*GtkSourceVimIMContextObserver) (GtkSourceVimIMContext *im_context,
                                               const char            *string,
                                               gboolean               reset,
                                               gpointer               user_data);

void _gtk_source_vim_im_context_add_observer (GtkSourceVimIMContext         *self,
                                              GtkSourceVimIMContextObserver  observer,
                                              gpointer                       user_data,
                                              GDestroyNotify                 notify);

G_END_DECLS

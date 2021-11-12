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

#include "gtksourcevimstate.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_VIM_CHAR_PENDING (gtk_source_vim_char_pending_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimCharPending, gtk_source_vim_char_pending, GTK_SOURCE, VIM_CHAR_PENDING, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_char_pending_new           (void);
gunichar           gtk_source_vim_char_pending_get_character (GtkSourceVimCharPending *self);
const char        *gtk_source_vim_char_pending_get_string    (GtkSourceVimCharPending *self);

G_END_DECLS

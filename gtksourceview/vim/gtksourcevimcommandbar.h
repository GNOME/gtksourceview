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

#define GTK_SOURCE_TYPE_VIM_COMMAND_BAR (gtk_source_vim_command_bar_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimCommandBar, gtk_source_vim_command_bar, GTK_SOURCE, VIM_COMMAND_BAR, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_command_bar_new          (void);
GtkSourceVimState *gtk_source_vim_command_bar_take_command (GtkSourceVimCommandBar *self);
const char        *gtk_source_vim_command_bar_get_text     (GtkSourceVimCommandBar *self);
void               gtk_source_vim_command_bar_set_text     (GtkSourceVimCommandBar *self,
                                                            const char             *text);

G_END_DECLS

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

#define GTK_SOURCE_TYPE_VIM (gtk_source_vim_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVim, gtk_source_vim, GTK_SOURCE, VIM, GtkSourceVimState)

GtkSourceVim *gtk_source_vim_new                  (GtkSourceView  *view);
void          gtk_source_vim_reset                (GtkSourceVim   *self);
const char   *gtk_source_vim_get_command_text     (GtkSourceVim   *self);
const char   *gtk_source_vim_get_command_bar_text (GtkSourceVim   *self);
gboolean      gtk_source_vim_emit_execute_command (GtkSourceVim   *self,
                                                   const char     *command);
gboolean      gtk_source_vim_emit_filter          (GtkSourceVim   *self,
                                                   GtkTextIter    *begin,
                                                   GtkTextIter    *end);
gboolean      gtk_source_vim_emit_format          (GtkSourceVim   *self,
                                                   GtkTextIter    *begin,
                                                   GtkTextIter    *end);
void          gtk_source_vim_emit_ready           (GtkSourceVim   *self);

G_END_DECLS

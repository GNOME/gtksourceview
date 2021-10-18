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

#include "gtksourcevimmotion.h"
#include "gtksourcevimstate.h"
#include "gtksourcevimtextobject.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_VIM_COMMAND (gtk_source_vim_command_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimCommand, gtk_source_vim_command, GTK_SOURCE, VIM_COMMAND, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_command_new                      (const char              *command);
GtkSourceVimState *gtk_source_vim_command_new_parsed               (GtkSourceVimState       *current,
                                                                    const char              *command_line);
const char        *gtk_source_vim_command_get_command              (GtkSourceVimCommand     *self);
void               gtk_source_vim_command_set_motion               (GtkSourceVimCommand     *self,
                                                                    GtkSourceVimMotion      *motion);
void               gtk_source_vim_command_set_selection_motion     (GtkSourceVimCommand     *self,
                                                                    GtkSourceVimMotion      *selection_motion);
void               gtk_source_vim_command_set_text_object          (GtkSourceVimCommand     *self,
                                                                    GtkSourceVimTextObject  *text_objet);
gboolean           gtk_source_vim_command_parse_search_and_replace (const char              *str,
                                                                    char                   **search,
                                                                    char                   **replace,
                                                                    char                   **options);

G_END_DECLS

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

typedef enum
{

	GTK_SOURCE_VIM_INSERT_HERE,
	GTK_SOURCE_VIM_INSERT_AFTER_CHAR,
	GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_BOF,
	GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_SOL,
} GtkSourceVimInsertAt;

#define GTK_SOURCE_TYPE_VIM_INSERT (gtk_source_vim_insert_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimInsert, gtk_source_vim_insert, GTK_SOURCE, VIM_INSERT, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_insert_new                  (void);
void               gtk_source_vim_insert_set_at               (GtkSourceVimInsert     *self,
                                                               GtkSourceVimInsertAt    at);
void               gtk_source_vim_insert_set_motion           (GtkSourceVimInsert     *self,
                                                               GtkSourceVimMotion     *motion);
void               gtk_source_vim_insert_set_selection_motion (GtkSourceVimInsert     *self,
                                                               GtkSourceVimMotion     *selection_motion);
void               gtk_source_vim_insert_set_text_object      (GtkSourceVimInsert     *self,
                                                               GtkSourceVimTextObject *text_object);
void               gtk_source_vim_insert_set_indent           (GtkSourceVimInsert     *self,
                                                               gboolean                indent);
void               gtk_source_vim_insert_set_prefix           (GtkSourceVimInsert     *self,
                                                               const char             *prefix);
void               gtk_source_vim_insert_set_suffix           (GtkSourceVimInsert     *self,
                                                               const char             *suffix);


G_END_DECLS

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

#define GTK_SOURCE_TYPE_VIM_TEXT_OBJECT (gtk_source_vim_text_object_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimTextObject, gtk_source_vim_text_object, GTK_SOURCE, VIM_TEXT_OBJECT, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_text_object_new_inner_word          (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_WORD          (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_sentence      (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_paragraph     (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_block_paren   (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_block_brace   (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_block_bracket (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_block_lt_gt   (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_quote_double  (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_quote_single  (void);
GtkSourceVimState *gtk_source_vim_text_object_new_inner_quote_grave   (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_word              (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_WORD              (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_sentence          (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_paragraph         (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_block_paren       (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_block_brace       (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_block_bracket     (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_block_lt_gt       (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_quote_double      (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_quote_single      (void);
GtkSourceVimState *gtk_source_vim_text_object_new_a_quote_grave       (void);
gboolean           gtk_source_vim_text_object_is_linewise             (GtkSourceVimTextObject *self);
gboolean           gtk_source_vim_text_object_select                  (GtkSourceVimTextObject *self,
                                                                       GtkTextIter            *begin,
                                                                       GtkTextIter            *end);

G_END_DECLS

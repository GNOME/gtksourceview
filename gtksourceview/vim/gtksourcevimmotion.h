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

#include <gtk/gtk.h>

#include "gtksourcevimstate.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_VIM_MOTION (gtk_source_vim_motion_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimMotion, gtk_source_vim_motion, GTK_SOURCE, VIM_MOTION, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_motion_new                       (void);
GtkSourceVimState *gtk_source_vim_motion_chain                     (GtkSourceVimMotion *self,
                                                                    GtkSourceVimMotion *other);
GtkSourceVimState *gtk_source_vim_motion_new_none                  (void);
GtkSourceVimState *gtk_source_vim_motion_new_first_char            (void);
GtkSourceVimState *gtk_source_vim_motion_new_line_end              (void);
GtkSourceVimState *gtk_source_vim_motion_new_line_end_with_nl      (void);
GtkSourceVimState *gtk_source_vim_motion_new_next_line_end_with_nl (void);
GtkSourceVimState *gtk_source_vim_motion_new_previous_line_end     (void);
GtkSourceVimState *gtk_source_vim_motion_new_forward_char          (void);
GtkSourceVimState *gtk_source_vim_motion_new_down                  (int                 adjust_count);
GtkSourceVimState *gtk_source_vim_motion_new_line_start            (void);
void               gtk_source_vim_motion_set_linewise_keyval       (GtkSourceVimMotion *self,
                                                                    guint               keyval);
gboolean           gtk_source_vim_motion_get_apply_on_leave        (GtkSourceVimMotion *self);
void               gtk_source_vim_motion_set_apply_on_leave        (GtkSourceVimMotion *self,
                                                                    gboolean            apply_on_leave);
void               gtk_source_vim_motion_set_mark                  (GtkSourceVimMotion *self,
                                                                    GtkTextMark        *mark);
gboolean           gtk_source_vim_motion_apply                     (GtkSourceVimMotion *self,
                                                                    GtkTextIter        *iter,
                                                                    gboolean            apply_inclusive);
gboolean           gtk_source_vim_motion_invalidates_visual_column (GtkSourceVimMotion *self);
gboolean           gtk_source_vim_motion_is_linewise               (GtkSourceVimMotion *self);
gboolean           gtk_source_vim_motion_is_jump                   (GtkSourceVimMotion *self);

gboolean gtk_source_vim_iter_backward_block_brace_start   (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_block_bracket_start (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_block_lt_gt_start   (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_block_paren_start   (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_paragraph_start     (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_quote_double        (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_quote_grave         (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_quote_single        (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_sentence_start      (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_word_start          (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_backward_WORD_start          (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_block_brace_end      (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_block_bracket_end    (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_block_lt_gt_end      (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_block_paren_end      (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_paragraph_end        (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_quote_double         (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_quote_grave          (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_quote_single         (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_sentence_end         (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_word_end             (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_forward_WORD_end             (GtkTextIter       *iter);
gboolean gtk_source_vim_iter_ends_word                    (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_ends_WORD                    (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_ends_quote_double            (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_ends_quote_single            (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_ends_quote_grave             (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_starts_word                  (const GtkTextIter *iter);
gboolean gtk_source_vim_iter_starts_WORD                  (const GtkTextIter *iter);

G_END_DECLS

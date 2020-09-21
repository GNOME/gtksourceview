/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014, 2016 - Sébastien Wilmet <swilmet@gnome.org>
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
 */

#pragma once

#include <gtk/gtk.h>

#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

/* Semi-public functions. */

GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_forward_visible_word_end           (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_forward_visible_word_ends          (GtkTextIter       *iter,
                                                              gint               count);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_backward_visible_word_start        (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_backward_visible_word_starts       (GtkTextIter       *iter,
                                                              gint               count);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_extend_selection_word              (const GtkTextIter *location,
                                                              GtkTextIter       *start,
                                                              GtkTextIter       *end);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_starts_extra_natural_word          (const GtkTextIter *iter,
                                                              gboolean           visible);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_ends_extra_natural_word            (const GtkTextIter *iter,
                                                              gboolean           visible);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_get_leading_spaces_end_boundary    (const GtkTextIter *iter,
                                                              GtkTextIter       *leading_end);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_get_trailing_spaces_start_boundary (const GtkTextIter *iter,
                                                              GtkTextIter       *trailing_start);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_forward_full_word_end              (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_backward_full_word_start           (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_starts_full_word                   (const GtkTextIter *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_ends_full_word                     (const GtkTextIter *iter);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_forward_extra_natural_word_end     (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
void     _gtk_source_iter_backward_extra_natural_word_start  (GtkTextIter       *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_starts_word                        (const GtkTextIter *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_ends_word                          (const GtkTextIter *iter);
GTK_SOURCE_INTERNAL
gboolean _gtk_source_iter_inside_word                        (const GtkTextIter *iter);

G_END_DECLS

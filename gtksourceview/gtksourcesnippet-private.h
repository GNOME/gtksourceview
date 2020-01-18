/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

 #include "gtksourcesnippet.h"

 G_BEGIN_DECLS

G_GNUC_INTERNAL
void         _gtk_source_snippet_replace_current_chunk_text (GtkSourceSnippet  *self,
                                                             const gchar       *new_text);
G_GNUC_INTERNAL
gchar       *_gtk_source_snippet_get_edited_text            (GtkSourceSnippet  *self);
G_GNUC_INTERNAL
gboolean     _gtk_source_snippet_begin                      (GtkSourceSnippet  *self,
                                                             GtkSourceBuffer   *buffer,
                                                             GtkTextIter       *iter);
G_GNUC_INTERNAL
void         _gtk_source_snippet_finish                     (GtkSourceSnippet  *self);
G_GNUC_INTERNAL
gboolean     _gtk_source_snippet_move_next                  (GtkSourceSnippet  *self);
G_GNUC_INTERNAL
gboolean     _gtk_source_snippet_move_previous              (GtkSourceSnippet  *self);
G_GNUC_INTERNAL
void         _gtk_source_snippet_after_insert_text          (GtkSourceSnippet  *self,
                                                             GtkTextBuffer     *buffer,
                                                             GtkTextIter       *iter,
                                                             const gchar       *text,
                                                             gint               len);
G_GNUC_INTERNAL
void         _gtk_source_snippet_after_delete_range         (GtkSourceSnippet  *self,
                                                             GtkTextBuffer     *buffer,
                                                             GtkTextIter       *begin,
                                                             GtkTextIter       *end);
G_GNUC_INTERNAL
gboolean     _gtk_source_snippet_insert_set                 (GtkSourceSnippet  *self,
                                                             GtkTextMark       *mark);
G_GNUC_INTERNAL
guint        _gtk_source_snippet_count_affected_chunks      (GtkSourceSnippet  *snippet,
                                                             const GtkTextIter *begin,
                                                             const GtkTextIter *end);
G_GNUC_INTERNAL
gboolean     _gtk_source_snippet_contains_range             (GtkSourceSnippet  *snippet,
                                                             const GtkTextIter *begin,
                                                             const GtkTextIter *end);

G_END_DECLS

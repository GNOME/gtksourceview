/*
 * This file is part of GtkSourceView
 *
 * Copyright 2013, 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"
#include "gtksourcebuffer.h"

G_BEGIN_DECLS

void                       _gtk_source_buffer_update_syntax_highlight   (GtkSourceBuffer        *buffer,
                                                                         const GtkTextIter      *start,
                                                                         const GtkTextIter      *end,
                                                                         gboolean                synchronous);
gboolean                   _gtk_source_buffer_has_search_highlights     (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_update_search_highlight   (GtkSourceBuffer        *buffer,
                                                                         const GtkTextIter      *start,
                                                                         const GtkTextIter      *end,
                                                                         gboolean                synchronous);
GtkSourceMark             *_gtk_source_buffer_source_mark_next          (GtkSourceBuffer        *buffer,
                                                                         GtkSourceMark          *mark,
                                                                         const gchar            *category);
GtkSourceMark             *_gtk_source_buffer_source_mark_prev          (GtkSourceBuffer        *buffer,
                                                                         GtkSourceMark          *mark,
                                                                         const gchar            *category);
GtkTextTag                *_gtk_source_buffer_get_bracket_match_tag     (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_add_search_context        (GtkSourceBuffer        *buffer,
                                                                         GtkSourceSearchContext *search_context);
void                       _gtk_source_buffer_set_as_invalid_character  (GtkSourceBuffer        *buffer,
                                                                         const GtkTextIter      *start,
                                                                         const GtkTextIter      *end);
gboolean                   _gtk_source_buffer_has_invalid_chars         (GtkSourceBuffer        *buffer);
GtkSourceBracketMatchType  _gtk_source_buffer_find_bracket_match        (GtkSourceBuffer        *buffer,
                                                                         const GtkTextIter      *pos,
                                                                         GtkTextIter            *bracket,
                                                                         GtkTextIter            *bracket_match);
void                       _gtk_source_buffer_save_and_clear_selection  (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_restore_selection         (GtkSourceBuffer        *buffer);
gboolean                   _gtk_source_buffer_has_source_marks          (GtkSourceBuffer        *buffer);
GSList                    *_gtk_source_buffer_get_source_marks_in_range (GtkSourceBuffer        *buffer,
                                                                         const GtkTextIter      *start,
                                                                         const GtkTextIter      *end,
                                                                         const char             *category);
gboolean                   _gtk_source_buffer_has_spaces_tag            (GtkSourceBuffer        *buffer);
GtkTextTag                *_gtk_source_buffer_get_snippet_focus_tag     (GtkSourceBuffer        *buffer);
gint64                     _gtk_source_buffer_get_insertion_count       (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_block_cursor_moved        (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_unblock_cursor_moved      (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_begin_loading             (GtkSourceBuffer        *buffer);
void                       _gtk_source_buffer_end_loading               (GtkSourceBuffer        *buffer);

G_END_DECLS

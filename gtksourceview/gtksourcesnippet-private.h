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
#include "gtksourcesnippetchunk.h"
#include "gtksourcesnippetcontext.h"

G_BEGIN_DECLS

struct _GtkSourceSnippet
{
	GObject                  parent_instance;

	GtkSourceSnippetContext *context;
	GtkTextBuffer           *buffer;

	GQueue                   chunks;
	GtkSourceSnippetChunk   *current_chunk;

	GtkTextMark             *begin_mark;
	GtkTextMark             *end_mark;
	gchar                   *trigger;
	const gchar             *language_id;
	gchar                   *description;
	gchar                   *name;

	/* This is used to track the insert position within a snippet
	 * while we make transforms. We don't use marks here because
	 * the gravity of the mark is not enough o assure we end up
	 * at the correct position. So instead we are relative to the
	 * beginning of the snippet.
	 */
	gint                     saved_insert_pos;

	gint                     focus_position;
	gint                     max_focus_position;

	guint                    inserted : 1;
};


void         _gtk_source_snippet_replace_current_chunk_text (GtkSourceSnippet  *self,
                                                             const gchar       *new_text);
gchar       *_gtk_source_snippet_get_edited_text            (GtkSourceSnippet  *self);
gboolean     _gtk_source_snippet_begin                      (GtkSourceSnippet  *self,
                                                             GtkSourceBuffer   *buffer,
                                                             GtkTextIter       *iter);
void         _gtk_source_snippet_finish                     (GtkSourceSnippet  *self);
gboolean     _gtk_source_snippet_move_next                  (GtkSourceSnippet  *self);
gboolean     _gtk_source_snippet_move_previous              (GtkSourceSnippet  *self);
void         _gtk_source_snippet_after_insert_text          (GtkSourceSnippet  *self,
                                                             GtkTextBuffer     *buffer,
                                                             GtkTextIter       *iter,
                                                             const gchar       *text,
                                                             gint               len);
void         _gtk_source_snippet_after_delete_range         (GtkSourceSnippet  *self,
                                                             GtkTextBuffer     *buffer,
                                                             GtkTextIter       *begin,
                                                             GtkTextIter       *end);
gboolean     _gtk_source_snippet_insert_set                 (GtkSourceSnippet  *self,
                                                             GtkTextMark       *mark);
guint        _gtk_source_snippet_count_affected_chunks      (GtkSourceSnippet  *snippet,
                                                             const GtkTextIter *begin,
                                                             const GtkTextIter *end);
gboolean     _gtk_source_snippet_contains_range             (GtkSourceSnippet  *snippet,
                                                             const GtkTextIter *begin,
                                                             const GtkTextIter *end);

G_END_DECLS

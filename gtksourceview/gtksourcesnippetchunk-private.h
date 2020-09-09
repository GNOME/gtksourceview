/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014-2020 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcesnippetchunk.h"

G_BEGIN_DECLS

struct _GtkSourceSnippetChunk
{
	GInitiallyUnowned        parent_instance;

	GList                    link;

	GtkSourceSnippetContext *context;
	char                    *spec;
	char                    *text;
	char                    *tooltip_text;
	GtkTextMark             *begin_mark;
	GtkTextMark             *end_mark;

	gulong                   context_changed_handler;

	gint                     focus_position;

	guint                    text_set : 1;
};

G_GNUC_INTERNAL
void     _gtk_source_snippet_chunk_save_text  (GtkSourceSnippetChunk *chunk);
G_GNUC_INTERNAL
gboolean _gtk_source_snippet_chunk_contains   (GtkSourceSnippetChunk *chunk,
                                               const GtkTextIter     *iter);
G_GNUC_INTERNAL
gboolean _gtk_source_snippet_chunk_get_bounds (GtkSourceSnippetChunk *chunk,
                                               GtkTextIter           *begin,
                                               GtkTextIter           *end);

G_END_DECLS

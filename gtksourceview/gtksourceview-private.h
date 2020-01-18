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

#include "gtksourceview.h"

G_BEGIN_DECLS

typedef struct _GtkSourceViewSnippets
{
	GtkSourceView   *view;
	GtkSourceBuffer *buffer;
	GQueue           queue;
	gulong           buffer_insert_text_handler;
	gulong           buffer_insert_text_after_handler;
	gulong           buffer_delete_range_handler;
	gulong           buffer_delete_range_after_handler;
	gulong           buffer_cursor_moved_handler;
} GtkSourceViewSnippets;

void     _gtk_source_view_snippets_init        (GtkSourceViewSnippets *snippets,
                                                GtkSourceView         *view);
void     _gtk_source_view_snippets_shutdown    (GtkSourceViewSnippets *snippets);
void     _gtk_source_view_snippets_set_buffer  (GtkSourceViewSnippets *snippets,
                                                GtkSourceBuffer       *buffer);
void     _gtk_source_view_snippets_push        (GtkSourceViewSnippets *snippets,
                                                GtkSourceSnippet      *snippet,
                                                GtkTextIter           *iter);
void     _gtk_source_view_snippets_pop         (GtkSourceViewSnippets *snippets);
void     _gtk_source_view_snippets_pop_all     (GtkSourceViewSnippets *snippets);
gboolean _gtk_source_view_snippets_key_pressed (GtkSourceViewSnippets *snippets,
                                                guint                  key,
                                                guint                  keycode,
                                                GdkModifierType        state);

G_END_DECLS

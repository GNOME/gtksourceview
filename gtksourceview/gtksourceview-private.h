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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtksourceview.h"

#include "gtksourceinformative-private.h"
#include "gtksourceassistant-private.h"

G_BEGIN_DECLS

typedef struct _GtkSourceViewAssistants
{
	GtkSourceView *view;
	GQueue         queue;
} GtkSourceViewAssistants;

typedef struct _GtkSourceViewSnippets
{
	GtkSourceView        *view;
	GtkSourceBuffer      *buffer;
	GSignalGroup         *snippet_signals;
	GtkSourceInformative *informative;
	GQueue                queue;
	gulong                buffer_insert_text_handler;
	gulong                buffer_insert_text_after_handler;
	gulong                buffer_delete_range_handler;
	gulong                buffer_delete_range_after_handler;
	gulong                buffer_cursor_moved_handler;
} GtkSourceViewSnippets;

gboolean _gtk_source_view_has_snippet              (GtkSourceView           *view);
void     _gtk_source_view_add_assistant            (GtkSourceView           *view,
                                                    GtkSourceAssistant      *assistant);
void     _gtk_source_view_remove_assistant         (GtkSourceView           *view,
                                                    GtkSourceAssistant      *assistant);
void     _gtk_source_view_assistants_init          (GtkSourceViewAssistants *assistants,
                                                    GtkSourceView           *view);
void     _gtk_source_view_assistants_add           (GtkSourceViewAssistants *assistants,
                                                    GtkSourceAssistant      *assistant);
void     _gtk_source_view_assistants_remove        (GtkSourceViewAssistants *assistants,
                                                    GtkSourceAssistant      *assistant);
void     _gtk_source_view_assistants_remove        (GtkSourceViewAssistants *assistants,
                                                    GtkSourceAssistant      *assistant);
gboolean _gtk_source_view_assistants_hide_all      (GtkSourceViewAssistants *assistants);
void     _gtk_source_view_assistants_shutdown      (GtkSourceViewAssistants *assistants);
void     _gtk_source_view_assistants_size_allocate (GtkSourceViewAssistants *assistants,
                                                    int                      width,
                                                    int                      height,
                                                    int                      baseline);
gboolean _gtk_source_view_assistants_handle_key    (GtkSourceViewAssistants *assistant,
                                                    guint                    keyval,
                                                    GdkModifierType          state);
void     _gtk_source_view_assistants_update_all    (GtkSourceViewAssistants *assistants);

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

void     _gtk_source_view_hide_completion                    (GtkSourceView *view);
gboolean _gtk_source_view_get_current_line_background        (GtkSourceView *view,
                                                              GdkRGBA       *rgba);
gboolean _gtk_source_view_get_current_line_number_background (GtkSourceView *view,
                                                              GdkRGBA       *rgba);
gboolean _gtk_source_view_get_current_line_number_color      (GtkSourceView *view,
                                                              GdkRGBA       *rgba);
gboolean _gtk_source_view_get_current_line_number_bold       (GtkSourceView *view);

G_END_DECLS

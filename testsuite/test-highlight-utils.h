/*
 * This file is part of GtkSourceView
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include <gtksourceview/gtksource.h>

static inline GtkSourceBuffer *
test_highlight_buffer_new (const char *id,
                           const char *text)
{
	const char *paths[] = { TOP_SRCDIR "/testsuite/language-specs",
	                        TOP_SRCDIR "/data/language-specs", NULL };
	GtkSourceLanguageManager *manager = gtk_source_language_manager_new ();
	GtkSourceLanguage *language;
	GtkSourceBuffer *buffer;

	gtk_source_language_manager_set_search_path (manager, paths);
	language = gtk_source_language_manager_get_language (manager, id);
	g_assert_nonnull (language);
	buffer = gtk_source_buffer_new_with_language (language);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
	g_object_unref (manager);

	return buffer;
}

static inline void
test_highlight_buffer_ensure (GtkSourceBuffer *buffer)
{
	GtkTextIter begin, end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
}

static inline void
test_highlight_assert_class (GtkSourceBuffer *buffer,
                             int              offset,
                             const char      *name,
                             gboolean         expected)
{
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
	g_assert_cmpint (gtk_source_buffer_iter_has_context_class (buffer, &iter, name), ==, expected);
}

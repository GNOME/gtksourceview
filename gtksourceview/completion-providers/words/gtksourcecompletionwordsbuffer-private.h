/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>

#include "gtksourcecompletionwordslibrary-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_WORDS_BUFFER (gtk_source_completion_words_buffer_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionWordsBuffer, gtk_source_completion_words_buffer, GTK_SOURCE, COMPLETION_WORDS_BUFFER, GObject)

G_GNUC_INTERNAL
GtkSourceCompletionWordsBuffer *gtk_source_completion_words_buffer_new                   (GtkSourceCompletionWordsLibrary *library,
                                                                                          GtkTextBuffer                   *buffer);
G_GNUC_INTERNAL
GtkTextBuffer                  *gtk_source_completion_words_buffer_get_buffer            (GtkSourceCompletionWordsBuffer  *buffer);
G_GNUC_INTERNAL
void                            gtk_source_completion_words_buffer_set_scan_batch_size   (GtkSourceCompletionWordsBuffer  *buffer,
                                                                                          guint                            size);
G_GNUC_INTERNAL
void                            gtk_source_completion_words_buffer_set_minimum_word_size (GtkSourceCompletionWordsBuffer  *buffer,
                                                                                          guint                            size);

G_END_DECLS

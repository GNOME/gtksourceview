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

#include <gio/gio.h>

#include "gtksourcecompletionwordslibrary-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_WORDS_MODEL (gtk_source_completion_words_model_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceCompletionWordsModel, gtk_source_completion_words_model, GTK_SOURCE, COMPLETION_WORDS_MODEL, GObject)

GListModel *gtk_source_completion_words_model_new        (GtkSourceCompletionWordsLibrary *library,
                                                          guint                            proposals_batch_size,
                                                          guint                            minimum_word_size,
                                                          const char                      *prefix);
gboolean    gtk_source_completion_words_model_can_filter (GtkSourceCompletionWordsModel   *self,
                                                          const char                      *word);
void        gtk_source_completion_words_model_cancel     (GtkSourceCompletionWordsModel   *self);

G_END_DECLS

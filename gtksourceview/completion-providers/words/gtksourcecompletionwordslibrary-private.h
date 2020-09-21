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

#include <glib-object.h>

#include "gtksourcecompletionwordsproposal-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_WORDS_LIBRARY (gtk_source_completion_words_library_get_type())

struct _GtkSourceCompletionWordsLibraryClass
{
	GObjectClass parent_class;
};

GTK_SOURCE_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionWordsLibrary, gtk_source_completion_words_library, GTK_SOURCE, COMPLETION_WORDS_LIBRARY, GObject)

GTK_SOURCE_INTERNAL
GtkSourceCompletionWordsLibrary  *gtk_source_completion_words_library_new          (void);
GTK_SOURCE_INTERNAL
GSequenceIter                    *gtk_source_completion_words_library_find         (GtkSourceCompletionWordsLibrary  *library,
                                                                                    GtkSourceCompletionWordsProposal *proposal);
GTK_SOURCE_INTERNAL
GSequenceIter                    *gtk_source_completion_words_library_find_first   (GtkSourceCompletionWordsLibrary  *library,
                                                                                    const gchar                      *word,
                                                                                    gint                              len);
GTK_SOURCE_INTERNAL
GSequenceIter                    *gtk_source_completion_words_library_find_next    (GSequenceIter                    *iter,
                                                                                    const gchar                      *word,
                                                                                    gint                              len);
GTK_SOURCE_INTERNAL
GtkSourceCompletionWordsProposal *gtk_source_completion_words_library_get_proposal (GSequenceIter                    *iter);
GTK_SOURCE_INTERNAL
GtkSourceCompletionWordsProposal *gtk_source_completion_words_library_add_word     (GtkSourceCompletionWordsLibrary  *library,
                                                                                    const gchar                      *word);
GTK_SOURCE_INTERNAL
void                              gtk_source_completion_words_library_remove_word  (GtkSourceCompletionWordsLibrary  *library,
                                                                                    GtkSourceCompletionWordsProposal *proposal);
GTK_SOURCE_INTERNAL
gboolean                          gtk_source_completion_words_library_is_locked    (GtkSourceCompletionWordsLibrary  *library);
GTK_SOURCE_INTERNAL
void                              gtk_source_completion_words_library_lock         (GtkSourceCompletionWordsLibrary  *library);
GTK_SOURCE_INTERNAL
void                              gtk_source_completion_words_library_unlock       (GtkSourceCompletionWordsLibrary  *library);

G_END_DECLS

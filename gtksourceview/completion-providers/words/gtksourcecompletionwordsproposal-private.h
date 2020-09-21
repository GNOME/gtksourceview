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

#include "../../gtksourcecompletionproposal.h"
#include "../../gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_WORDS_PROPOSAL (gtk_source_completion_words_proposal_get_type())

GTK_SOURCE_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionWordsProposal, gtk_source_completion_words_proposal, GTK_SOURCE, COMPLETION_WORDS_PROPOSAL, GObject)

GTK_SOURCE_INTERNAL
GtkSourceCompletionWordsProposal *gtk_source_completion_words_proposal_new      (const gchar                      *word);
GTK_SOURCE_INTERNAL
const gchar                      *gtk_source_completion_words_proposal_get_word (GtkSourceCompletionWordsProposal *proposal);
GTK_SOURCE_INTERNAL
void                              gtk_source_completion_words_proposal_use      (GtkSourceCompletionWordsProposal *proposal);
GTK_SOURCE_INTERNAL
void                              gtk_source_completion_words_proposal_unuse    (GtkSourceCompletionWordsProposal *proposal);

G_END_DECLS

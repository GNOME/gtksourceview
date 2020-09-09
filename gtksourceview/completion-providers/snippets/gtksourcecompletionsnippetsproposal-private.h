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

#include <gtksourceview/gtksourcetypes.h>
#include <gtksourceview/gtksourcecompletionproposal.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_SNIPPETS_PROPOSAL (gtk_source_completion_snippets_proposal_get_type())

G_GNUC_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionSnippetsProposal, gtk_source_completion_snippets_proposal, GTK_SOURCE, COMPLETION_SNIPPETS_PROPOSAL, GObject)

G_GNUC_INTERNAL
GtkSourceCompletionProposal *gtk_source_completion_snippets_proposal_new         (GtkSourceSnippet                    *snippet);
G_GNUC_INTERNAL
GtkSourceSnippet            *gtk_source_completion_snippets_proposal_get_snippet (GtkSourceCompletionSnippetsProposal *self);

G_END_DECLS

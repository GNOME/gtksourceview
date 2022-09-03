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

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <glib-object.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_PROPOSAL (gtk_source_completion_proposal_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkSourceCompletionProposal, gtk_source_completion_proposal, GTK_SOURCE, COMPLETION_PROPOSAL, GObject)

struct _GtkSourceCompletionProposalInterface
{
	GTypeInterface parent_iface;

  char *(*get_typed_text) (GtkSourceCompletionProposal *proposal);
};

GTK_SOURCE_AVAILABLE_IN_5_6
char *gtk_source_completion_proposal_get_typed_text (GtkSourceCompletionProposal *proposal);

G_END_DECLS

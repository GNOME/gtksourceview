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

#include "config.h"

#include "gtksourcecompletionproposal.h"

/**
 * GtkSourceCompletionProposal:
 *
 * Interface for completion proposals.
 *
 * This interface is used to denote that an object is capable of being
 * a completion proposal for [class@Completion].
 *
 * Currently, no method or functions are required but additional methods
 * may be added in the future. Proposals created by
 * #GtkSourceCompletionProvider can use [func@GObject.IMPLEMENT_INTERFACE] to
 * implement this with %NULL for the interface init function.
 */

G_DEFINE_INTERFACE (GtkSourceCompletionProposal, gtk_source_completion_proposal, G_TYPE_OBJECT)

static void
gtk_source_completion_proposal_default_init (GtkSourceCompletionProposalInterface *iface)
{
}

/**
 * gtk_source_completion_proposal_get_typed_text:
 * @proposal: a #GtkSourceCompletionProposal
 *
 * Gets the typed-text for the proposal, if supported by the implementation.
 *
 * Implementing this virtual-function is optional, but can be useful to allow
 * external tooling to compare results.
 *
 * Returns: (transfer full) (nullable): a newly allocated string, or %NULL
 *
 * Since: 5.6
 */
char *
gtk_source_completion_proposal_get_typed_text (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionProposalInterface *iface;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal), NULL);

	iface = GTK_SOURCE_COMPLETION_PROPOSAL_GET_IFACE (proposal);

	return iface->get_typed_text ? iface->get_typed_text (proposal) : NULL;
}

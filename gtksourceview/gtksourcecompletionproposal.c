/*
 * gtksourcecompletionproposal.c
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2007 - 2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright (C) 2009 Jesse van den Kieboom <jessevdk@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gtksourcecompletionproposal
 * @title: GtkSourceCompletionProposal
 * @short_description: Completion proposal object
 *
 * The proposal interface represents a completion item in the completion window.
 * It provides information on how to display the completion item and what action
 * should be taken when the completion item is activated.
 */

#include <gtksourceview/gtksourcecompletionproposal.h>

#include "gtksourceview-marshal.h"

/* Signals */
enum
{
	CHANGED,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = {0,};

static const gchar *
gtk_source_completion_proposal_get_label_default (GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_reached (NULL);
}

static GdkPixbuf *
gtk_source_completion_proposal_get_icon_default (GtkSourceCompletionProposal *proposal)
{
	return NULL;
}

static const gchar *
gtk_source_completion_proposal_get_info_default (GtkSourceCompletionProposal *proposal)
{
	return NULL;
}

static void 
gtk_source_completion_proposal_init (GtkSourceCompletionProposalIface *iface)
{
	static gboolean initialized = FALSE;
	
	iface->get_label = gtk_source_completion_proposal_get_label_default;
	iface->get_icon = gtk_source_completion_proposal_get_icon_default;
	iface->get_info = gtk_source_completion_proposal_get_info_default;
	
	if (!initialized)
	{
		signals[CHANGED] = 
			g_signal_new ("changed",
			      G_TYPE_FROM_INTERFACE (iface),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionProposalIface, changed),
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);

		initialized = TRUE;
	}
}

GType 
gtk_source_completion_proposal_get_type ()
{
	static GType gtk_source_completion_proposal_type_id = 0;
	
	if (!gtk_source_completion_proposal_type_id)
	{
		static const GTypeInfo g_define_type_info =
		{
			sizeof (GtkSourceCompletionProposalIface),
			(GBaseInitFunc) gtk_source_completion_proposal_init, 
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		gtk_source_completion_proposal_type_id = 
			g_type_register_static (G_TYPE_INTERFACE,
						"GtkSourceCompletionProposal",
						&g_define_type_info,
						0);
	}
	
	return gtk_source_completion_proposal_type_id;
}

/**
 * gtk_source_completion_proposal_get_label:
 * @proposal: The #GtkSourceCompletionProposal
 *
 * Gets the label of @proposal
 *
 * Returns: The label of @proposal
 */
const gchar *
gtk_source_completion_proposal_get_label (GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);	
	return GTK_SOURCE_COMPLETION_PROPOSAL_GET_INTERFACE (proposal)->get_label (proposal);
}

/**
 * gtk_source_completion_proposal_get_icon:
 * @proposal: The #GtkSourceCompletionProposal
 *
 * Gets the icon of @proposal
 *
 * Returns: The icon of @proposal
 */
GdkPixbuf *
gtk_source_completion_proposal_get_icon (GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);
	return GTK_SOURCE_COMPLETION_PROPOSAL_GET_INTERFACE (proposal)->get_icon (proposal);
}

/**
 * gtk_source_completion_proposal_get_info:
 * @proposal: The #GtkSourceCompletionProposal
 *
 * Gets extra information associated to the proposal. This information will be
 * used to present the user with extra, detailed information about the
 * selected proposal.
 *
 * Returns: The extra information of @proposal or %NULL if no extra information
 *          is associated to @proposal
 */
const gchar *
gtk_source_completion_proposal_get_info (GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);
	return GTK_SOURCE_COMPLETION_PROPOSAL_GET_INTERFACE (proposal)->get_info (proposal);
}

/**
 * gtk_source_completion_proposal_changed:
 * @proposal: The #GtkSourceCompletionProposal
 *
 * Emits the "changed" signal on @proposal. This should be called by
 * implementations whenever the name, icon or info of the proposal has
 * changed.
 */
void
gtk_source_completion_proposal_changed (GtkSourceCompletionProposal *proposal)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal));
	g_signal_emit (proposal, signals[CHANGED], 0);
}

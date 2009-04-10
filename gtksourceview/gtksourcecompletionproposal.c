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
	ACTIVATE,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = {0,};

static const gchar *
gtk_source_completion_proposal_get_label_default (GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_reached (NULL);
}

static void 
gtk_source_completion_proposal_init (GtkSourceCompletionProposalIface *iface)
{
	static gboolean initialized = FALSE;
	
	iface->get_label = gtk_source_completion_proposal_get_label_default;
	
	if (!initialized)
	{
		signals[ACTIVATE] = 
			g_signal_new ("activate",
			      G_TYPE_FROM_INTERFACE (iface),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionProposalIface, activate),
			      g_signal_accumulator_true_handled, 
			      NULL,
			      _gtksourceview_marshal_BOOLEAN__OBJECT, 
			      G_TYPE_BOOLEAN,
			      1,
			      GTK_TYPE_SOURCE_BUFFER);
		
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
const GdkPixbuf *
gtk_source_completion_proposal_get_icon (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionProposalIface *iface;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);

	iface = GTK_SOURCE_COMPLETION_PROPOSAL_GET_INTERFACE (proposal);
	
	if (iface->get_icon)
	{
		return iface->get_icon (proposal);
	}
	else
	{
		return NULL;
	}
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
	GtkSourceCompletionProposalIface *iface;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);

	iface = GTK_SOURCE_COMPLETION_PROPOSAL_GET_INTERFACE (proposal);
	
	if (iface->get_info)
	{
		return iface->get_info (proposal);
	}
	else
	{
		return NULL;
	}
}

/**
 * gtk_source_completion_proposal_activate:
 * @proposal: The #GtkSourceCompletionProposal
 * @buffer: The #GtkSourceBuffer
 * 
 * This emits the "activate" signal on @proposal. This function is generally 
 * called when @proposal is activated from the completion window. 
 * Implementations should take action in the default handler of the signal.
 */
gboolean
gtk_source_completion_proposal_activate (GtkSourceCompletionProposal *proposal,
					 GtkSourceBuffer	     *buffer)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), FALSE);
	
	g_signal_emit (proposal, signals[ACTIVATE], 0, buffer, &ret);

	return ret;
}



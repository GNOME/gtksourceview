/*
 * gtksourcecompletionprovider.c
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
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
 * SECTION:gtk_source_completion-provider
 * @title: GtkSourceCompletionProvider
 * @short_description: Completion provider interface
 *
 * You must implement this interface to provide proposals to #GscCompletion
 * 
 */


#include <gtksourceview/gtksourcecompletionprovider.h>

/* Default implementations */
static const gchar *
gtk_source_completion_provider_get_name_default (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_reached (NULL);
}

static GList *
gtk_source_completion_provider_get_proposals_default (GtkSourceCompletionProvider *provider)
{
	return NULL;
}

static gboolean
gtk_source_completion_provider_filter_proposal_default (GtkSourceCompletionProvider *provider,
                                                        GtkSourceCompletionProposal *proposal,
                                                        const gchar                 *criteria)
{
	return TRUE;
}

static gboolean
gtk_source_completion_provider_can_auto_complete_default (GtkSourceCompletionProvider *provider)
{
	return FALSE;
}

static GtkWidget *
gtk_source_completion_provider_get_info_widget_default (GtkSourceCompletionProvider *provider,
                                                        GtkSourceCompletionProposal *proposal)
{
	return NULL;
}

static void
gtk_source_completion_provider_update_info_default (GtkSourceCompletionProvider *provider,
                                                    GtkSourceCompletionProposal *proposal,
                                                    GtkSourceCompletionInfo     *info)
{
}

static void 
gtk_source_completion_provider_base_init (GtkSourceCompletionProviderIface *iface)
{
	static gboolean initialized = FALSE;
	
	iface->get_name = gtk_source_completion_provider_get_name_default;
	iface->get_proposals = gtk_source_completion_provider_get_proposals_default;
	iface->filter_proposal = gtk_source_completion_provider_filter_proposal_default;
	iface->can_auto_complete = gtk_source_completion_provider_can_auto_complete_default;

	iface->get_info_widget = gtk_source_completion_provider_get_info_widget_default;
	iface->update_info = gtk_source_completion_provider_update_info_default;
	
	if (!initialized)
	{
		initialized = TRUE;
	}
}


GType 
gtk_source_completion_provider_get_type ()
{
	static GType gtk_source_completion_provider_type_id = 0;

	if (!gtk_source_completion_provider_type_id)
	{
		static const GTypeInfo g_define_type_info = 
		{ 
			sizeof (GtkSourceCompletionProviderIface), 
			(GBaseInitFunc) gtk_source_completion_provider_base_init, 
			(GBaseFinalizeFunc) NULL, 
			(GClassInitFunc) NULL, 
			(GClassFinalizeFunc) NULL, 
			NULL, 
			0, 
			0, 
			(GInstanceInitFunc) NULL 
		};
						
		gtk_source_completion_provider_type_id = 
				g_type_register_static (G_TYPE_INTERFACE, 
							"GtkSourceCompletionProvider", 
							&g_define_type_info, 
							0);
	}

	return gtk_source_completion_provider_type_id;
}

/**
 * gtk_source_completion_provider_get_name:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get the name of the provider. This should be a translatable name for
 * display to the user. For example: _("Document word completion provider")
 *
 * Returns: The name of the provider
 */
const gchar *
gtk_source_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_name (provider);
}

/**
 * gtk_source_completion_provider_get_proposals:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get proposals from the provider for completion
 *
 * Returns: a list of #GtkSourceViewProposal or NULL if there are no proposals.
 *          The returned list is owned by the caller.
 */
GList * 
gtk_source_completion_provider_get_proposals (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_proposals (provider);
}

/**
 * gtk_source_completion_provider_filter_proposal:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: A #GtkSourceCompletionProposal
 * @criteria: A string representing the filter criteria
 *
 * Determines whether to filter @proposal based on @criteria. It is guaranteed
 * that @criteria is always a valid UTF-8 string (and never %NULL)
 *
 * Returns: %TRUE if @proposal conforms to @criteria and should be available,
 *          or %FALSE if @proposal should be hidden
 */
gboolean
gtk_source_completion_provider_filter_proposal (GtkSourceCompletionProvider *provider,
                                                GtkSourceCompletionProposal *proposal,
                                                const gchar                 *criteria)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), FALSE);
	g_return_val_if_fail (criteria != NULL, FALSE);

	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->filter_proposal (provider, proposal, criteria);
}

/**
 * gtk_source_completion_provider_can_auto_complete:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get whether @provider can be used to auto complete typed words
 *
 * Returns: %TRUE if @provider can be used to auto complete, or %FALSE otherwise
 */
gboolean
gtk_source_completion_provider_can_auto_complete (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->can_auto_complete (provider);
}

/**
 * gtk_source_completion_provider_get_info_widget:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: The currently selected #GtkSourceCompletionProposal
 *
 * Get a customized info widget to show extra information of a proposal with.
 * This allows for customized widgets on a proposal basis, although in general
 * providers will have the same custom widget for all their proposals and
 * @proposal can be ignored. The implementation of this function is optional, 
 * if implemented, #gtk_source_completion_provider_update_info MUST also be
 * implemented. If not implemented, the default 
 * #gtk_source_completion_proposal_get_info will be used to display extra
 * information about a #GtkSourceCompletionProposal.
 *
 * Returns: a custom #GtkWidget to show extra information about @proposal
 */
GtkWidget *
gtk_source_completion_provider_get_info_widget (GtkSourceCompletionProvider *provider,
                                                GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);
	
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_info_widget (provider, proposal);
}

/**
 * gtk_source_completion_provider_update_info:
 * @provider: A #GtkSourceCompletionProvider
 * @proposal: A #GtkSourceCompletionProposal
 * @info: A #GtkSourceCompletionInfo
 *
 * Update extra information shown in @info for @proposal. This should be
 * implemented if your provider sets a custom info widget for @proposal.
 * This function MUST be implemented when 
 * #gtk_source_completion_provider_get_info_widget is implemented.
 */
void 
gtk_source_completion_provider_update_info (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionProposal *proposal,
                                            GtkSourceCompletionInfo     *info)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (info));
	
	GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->update_info (provider, proposal, info);
}

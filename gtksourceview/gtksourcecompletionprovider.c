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

/**
 * gtk_source_completion_provider_get_name:
 * @provider: The #GtkSourceCompletionProvider
 *
 * The provider name. For example: "Document word completion provider"
 *
 * Returns: The provider's name 
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
 * @trigger: The #GtkSourceViewTrigger that triggered the completion
 *
 * Get the proposals from the provider triggered by @trigger for completion.
 *
 * Returns: a list of #GtkSourceViewProposal or NULL if there are no proposals.
 *          The returned list is owned by the caller and is freed when no
 *          longer needed.
 */
GList * 
gtk_source_completion_provider_get_proposals (GtkSourceCompletionProvider *provider,
					      GtkSourceCompletionTrigger  *trigger)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_proposals (provider, trigger);
}

/**
 * gtk_source_completion_provider_get_page:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: The #GtkSourceViewProposal
 *
 * Get the page on which @proposal should be placed
 *
 * Returns: the #GtkSourceCompletionPage on which @proposal should be placed,
 *          or %NULL to put @proposal on the default page
 */
GtkSourceCompletionPage *
gtk_source_completion_provider_get_page (GtkSourceCompletionProvider *provider,
                                         GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), NULL);
	
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_page (provider, proposal);
}

/**
 * gtk_source_completion_provider_get_info_widget:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: The currently selected #GtkSourceCompletionProposal
 *
 * Get a customized info widget to show extra information of a proposal with.
 * This allows for customized widgets on a proposal basis, although in general
 * providers will have the same custom widget for all their proposals and
 * @proposal is ignored. The implementation of this function is optional.
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

/* Default implementations */
static const gchar *
gtk_source_completion_provider_get_name_default (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_reached (NULL);
}

static GList *
gtk_source_completion_provider_get_proposals_default (GtkSourceCompletionProvider *provider,
						      GtkSourceCompletionTrigger  *trigger)
{
	return NULL;
}

static GtkSourceCompletionPage *
gtk_source_completion_provider_get_page_default (GtkSourceCompletionProvider *provider,
                                                 GtkSourceCompletionProposal *proposal)
{
	return NULL;
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
	iface->get_page = gtk_source_completion_provider_get_page_default;

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



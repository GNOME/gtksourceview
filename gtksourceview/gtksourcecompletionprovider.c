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

#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletionprovider.h>
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourceview.h>

#include "gtksourcecompletionutils.h"
#include "gtksourceview-i18n.h"

/* Default implementations */
static const gchar *
gtk_source_completion_provider_get_name_default (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_reached (NULL);
}

static GdkPixbuf *
gtk_source_completion_provider_get_icon_default (GtkSourceCompletionProvider *provider)
{
	return NULL;
}

static GList *
gtk_source_completion_provider_get_proposals_default (GtkSourceCompletionProvider *provider,
                                                      GtkTextIter                 *iter)
{
	return NULL;
}

static gboolean
gtk_source_completion_provider_filter_proposal_default (GtkSourceCompletionProvider *provider,
                                                        GtkSourceCompletionProposal *proposal,
                                                        GtkTextIter                 *iter,
                                                        const gchar                 *criteria)
{
	return TRUE;
}

static gboolean
gtk_source_completion_provider_get_automatic_default (GtkSourceCompletionProvider *provider)
{
	return TRUE;
}

static gboolean
gtk_source_completion_provider_get_interactive_default (GtkSourceCompletionProvider *provider)
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

static gboolean
gtk_source_completion_provider_activate_proposal_default (GtkSourceCompletionProvider *provider,
                                                          GtkSourceCompletionProposal *proposal,
                                                          GtkTextIter                 *iter)
{
	return FALSE;
}

static void 
gtk_source_completion_provider_base_init (GtkSourceCompletionProviderIface *iface)
{
	static gboolean initialized = FALSE;
	
	iface->get_name = gtk_source_completion_provider_get_name_default;
	iface->get_icon = gtk_source_completion_provider_get_icon_default;

	iface->get_proposals = gtk_source_completion_provider_get_proposals_default;
	iface->filter_proposal = gtk_source_completion_provider_filter_proposal_default;
	
	iface->get_automatic = gtk_source_completion_provider_get_automatic_default;
	iface->get_interactive = gtk_source_completion_provider_get_interactive_default;
	
	iface->get_info_widget = gtk_source_completion_provider_get_info_widget_default;
	iface->update_info = gtk_source_completion_provider_update_info_default;
	
	iface->activate_proposal = gtk_source_completion_provider_activate_proposal_default;

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
 * display to the user. For example: _("Document word completion provider").
 *
 * Returns: The name of the provider.
 */
const gchar *
gtk_source_completion_provider_get_name (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_name (provider);
}

/**
 * gtk_source_completion_provider_get_icon:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get the icon of the provider.
 *
 * Returns: The icon to be used for the provider, or %NULL if the provider does
 *          not have a special icon.
 */
GdkPixbuf *
gtk_source_completion_provider_get_icon (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_icon (provider);
}

/**
 * gtk_source_completion_provider_get_proposals:
 * @provider: The #GtkSourceCompletionProvider
 * @iter: A #GtkTextIter
 *
 * Get proposals from the provider for completion.
 *
 * Returns: a list of #GtkSourceViewProposal or NULL if there are no proposals.
 *          The returned list and the contained #GtkSourceViewProposal are 
 *          owned by the caller and will be freed when no longer needed.
 */
GList * 
gtk_source_completion_provider_get_proposals (GtkSourceCompletionProvider *provider,
                                              GtkTextIter                 *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_proposals (provider, iter);
}

/**
 * gtk_source_completion_provider_filter_proposal:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: A #GtkSourceCompletionProposal
 * @iter: A #GtkTextIter
 * @criteria: A string representing the filter criteria
 *
 * Determines whether to filter @proposal based on @criteria. It is guaranteed
 * that @criteria is always a valid UTF-8 string (and never %NULL). 
 * Implementations are not restricted to using @criteria as a filter criteria.
 * They may also use @iter to do their own matching.
 *
 * Returns: %TRUE if @proposal conforms to @criteria and should be show,
 *          or %FALSE if @proposal should be hidden.
 */
gboolean
gtk_source_completion_provider_filter_proposal (GtkSourceCompletionProvider *provider,
                                                GtkSourceCompletionProposal *proposal,
                                                GtkTextIter                 *iter,
                                                const gchar                 *criteria)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), FALSE);
	g_return_val_if_fail (criteria != NULL, FALSE);

	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->filter_proposal (provider, 
	                                                                                 proposal, 
	                                                                                 iter, 
	                                                                                 criteria);
}

/**
 * gtk_source_completion_provider_get_interactive:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get whether @provider responds to interactive completion (e.g. completion
 * when typing).
 *
 * Returns: %TRUE if @provider can be used interactively, or %FALSE otherwise.
 */
gboolean
gtk_source_completion_provider_get_interactive (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_interactive (provider);
}

/**
 * gtk_source_completion_provider_get_automatic:
 * @provider: The #GtkSourceCompletionProvider
 *
 * Get whether @provider is automatically included in the list of completions.
 * This means that it will be shown in the default list of completions as 
 * activated by the "show-completion" keybinding (<Ctrl>space by default) on
 * the #GtkSourceView.
 *
 * Returns: %TRUE if @provider is automatically included, or %FALSE otherwise.
 */
gboolean
gtk_source_completion_provider_get_automatic (GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->get_automatic (provider);
}

/**
 * gtk_source_completion_provider_get_info_widget:
 * @provider: The #GtkSourceCompletionProvider
 * @proposal: The currently selected #GtkSourceCompletionProposal
 *
 * Get a customized info widget to show extra information of a proposal.
 * This allows for customized widgets on a proposal basis, although in general
 * providers will have the same custom widget for all their proposals and
 * @proposal can be ignored. The implementation of this function is optional. 
 * If implemented, #gtk_source_completion_provider_update_info MUST also be
 * implemented. If not implemented, the default 
 * #gtk_source_completion_proposal_get_info will be used to display extra
 * information about a #GtkSourceCompletionProposal.
 *
 * Returns: a custom #GtkWidget to show extra information about @proposal.
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

/**
 * gtk_source_completion_provider_activate_proposal:
 * @provider: A #GtkSourceCompletionProvider
 * @proposal: A #GtkSourceCompletionProposal
 * @iter: A #GtkTextIter
 *
 * Activate @proposal at @iter. When this functions returns %FALSE, the default
 * activation of @proposal will take place which replaces the word at @iter
 * with the label of @proposal.
 *
 * Returns: %TRUE to indicate that the proposal activation has been handled,
 *          %FALSE otherwise.
 */
gboolean
gtk_source_completion_provider_activate_proposal (GtkSourceCompletionProvider *provider,
                                                  GtkSourceCompletionProposal *proposal,
                                                  GtkTextIter                 *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), FALSE);
	
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (provider)->activate_proposal (provider, 
	                                                                                   proposal,
	                                                                                   iter);
}

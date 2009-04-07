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


#include "gtksourcecompletionprovider.h"

/**
 * gtk_source_completion_provider_get_name:
 * @self: The #GtkSourceCompletionProvider
 *
 * The provider name. By example: "Document word completion provider"
 *
 * Returns: The provider's name 
 */
const gchar*
gtk_source_completion_provider_get_name (GtkSourceCompletionProvider *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (self), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (self)->get_name (self);
}

/* Default implementation */
static const gchar *
gtk_source_completion_provider_get_name_default (GtkSourceCompletionProvider *self)
{
	g_return_val_if_reached (NULL);
}

/**
 * gtk_source_completion_provider_get_proposals:
 * @self: The #GtkSourceCompletionProvider
 * @trigger: The #GscTrigger that raise the event
 *
 * The completion call this function when an event is raised.
 * This function may return a list of #GscProposal to be shown
 * in the popup to the user.
 *
 * Returns: a list of #GscProposal or NULL if there are no proposals
 */
GList* 
gtk_source_completion_provider_get_proposals (GtkSourceCompletionProvider* self,
					      GtkSourceCompletionTrigger *trigger)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (self), NULL);
	return GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (self)->get_proposals (self, trigger);
}

/* Default implementation */
static GList *
gtk_source_completion_provider_get_proposals_default (GtkSourceCompletionProvider *self,
						      GtkSourceCompletionTrigger *trigger)
{
	g_return_val_if_reached (NULL);
}

/**
 * gtk_source_completion_provider_finish:
 * @self: The #GtkSourceCompletionProvider
 *
 * The completion call this function when it is going to hide the popup (The 
 * user selects a proposal or hide the completion popup)
 */
void
gtk_source_completion_provider_finish (GtkSourceCompletionProvider* self)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (self));
	GTK_SOURCE_COMPLETION_PROVIDER_GET_INTERFACE (self)->finish (self);
}

/* Default implementation */
static void
gtk_source_completion_provider_finish_default (GtkSourceCompletionProvider *self)
{
	g_return_if_reached ();
}

static void 
gtk_source_completion_provider_base_init (GtkSourceCompletionProviderIface * iface)
{
	static gboolean initialized = FALSE;
	
	iface->get_name = gtk_source_completion_provider_get_name_default;
	iface->get_proposals = gtk_source_completion_provider_get_proposals_default;
	iface->finish = gtk_source_completion_provider_finish_default;
	
	if (!initialized) {
		initialized = TRUE;
	}
}


GType 
gtk_source_completion_provider_get_type ()
{
	static GType gtk_source_completion_provider_type_id = 0;
	if (!gtk_source_completion_provider_type_id) {
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



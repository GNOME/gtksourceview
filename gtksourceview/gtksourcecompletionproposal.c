/*
 * gtksourcecompletionproposal.c
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
 * SECTION:gtksourcecompletionproposal
 * @title: GtkSourceCompletionProposal
 * @short_description: Completion proposal object
 *
 * Every proposal is an item into the popup. It controls the label to be
 * shown, the help (info) and the apply when the user selects the proposal.
 */
  
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionutils.h"
#include "gtksourceview-i18n.h"

#define GTK_SOURCE_COMPLETION_PROPOSAL_DEFAULT_PAGE _("Default")
#define GTK_SOURCE_COMPLETION_PROPOSAL_DEFAULT_PRIORITY 10

#define GTK_SOURCE_COMPLETION_PROPOSAL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), \
							   GTK_TYPE_SOURCE_COMPLETION_PROPOSAL, GtkSourceCompletionProposalPrivate))

G_DEFINE_TYPE(GtkSourceCompletionProposal, gtk_source_completion_proposal, G_TYPE_OBJECT);

struct _GtkSourceCompletionProposalPrivate
{
	gchar *label;
	gchar *info;
	GdkPixbuf *icon;
	gchar *page_name;
};

/* Properties */
enum
{
	PROP_0,
	PROP_LABEL,
	PROP_INFO,
	PROP_ICON,
	PROP_PAGE_NAME
};

static gboolean
gtk_source_completion_proposal_apply_default (GtkSourceCompletionProposal *self,
					      GtkTextView *view)
{
	gtk_source_completion_utils_replace_current_word (view,
							  self->priv->label);
	return FALSE;
}

static const gchar *
gtk_source_completion_proposal_get_info_default (GtkSourceCompletionProposal *self)
{
	return self->priv->info;
}

static void
gtk_source_completion_proposal_init (GtkSourceCompletionProposal *self)
{
	self->priv = GTK_SOURCE_COMPLETION_PROPOSAL_GET_PRIVATE (self);
	
	self->priv->label = NULL;
	self->priv->info = NULL;
	self->priv->icon = NULL;
	self->priv->page_name = g_strdup (GTK_SOURCE_COMPLETION_PROPOSAL_DEFAULT_PAGE);
}

static void
gtk_source_completion_proposal_finalize (GObject *object)
{
	GtkSourceCompletionProposal *self = GTK_SOURCE_COMPLETION_PROPOSAL (object);
	
	g_free (self->priv->label);
	g_free (self->priv->info);
	g_free (self->priv->page_name);
	
	if (self->priv->icon != NULL)
		g_object_unref(self->priv->icon);
	
	G_OBJECT_CLASS (gtk_source_completion_proposal_parent_class)->finalize (object);
}

static void
gtk_source_completion_proposal_get_property (GObject    *object,
					     guint       prop_id,
					     GValue     *value,
					     GParamSpec *pspec)
{
	GtkSourceCompletionProposal *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (object));

	self = GTK_SOURCE_COMPLETION_PROPOSAL (object);

	switch (prop_id)
	{
			
		case PROP_LABEL:
			g_value_set_string (value,self->priv->label);
			break;
		case PROP_INFO:
			g_value_set_string (value,
					    self->priv->info);
			break;
		case PROP_ICON:
			g_value_set_pointer (value,
					     (gpointer)self->priv->icon);
			break;
		case PROP_PAGE_NAME:
			g_value_set_string (value,
					    self->priv->page_name);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_proposal_set_property (GObject      *object,
					     guint         prop_id,
					     const GValue *value,
					     GParamSpec   *pspec)
{
	GtkSourceCompletionProposal *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (object));

	self = GTK_SOURCE_COMPLETION_PROPOSAL (object);

	switch (prop_id)
	{
		case PROP_LABEL:
			self->priv->label = g_value_dup_string (value);
			break;
		case PROP_INFO:
			self->priv->info = g_value_dup_string (value);
			break;
		case PROP_ICON:
			if (self->priv->icon != NULL)
				g_object_unref (self->priv->icon);

			self->priv->icon = g_object_ref ((GdkPixbuf*)g_value_get_pointer (value));
			break;
		case PROP_PAGE_NAME:
			self->priv->page_name = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_proposal_class_init (GtkSourceCompletionProposalClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_completion_proposal_get_property;
	object_class->set_property = gtk_source_completion_proposal_set_property;
	object_class->finalize = gtk_source_completion_proposal_finalize;

	g_type_class_add_private (object_class, sizeof (GtkSourceCompletionProposalPrivate));
	
	klass->apply = gtk_source_completion_proposal_apply_default;
	klass->get_info = gtk_source_completion_proposal_get_info_default;
	
	/* Proposal properties */
	
	/**
	 * GtkSourceCompletionProposal:label:
	 *
	 * Label to be shown for this proposal
	 */
	g_object_class_install_property (object_class,
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      _("Label to be shown for this proposal"),
							      _("Label to be shown for this proposal"),
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * GtkSourceCompletionProposal:info:
	 *
	 * Info to be shown for this proposal
	 */
	g_object_class_install_property (object_class,
					 PROP_INFO,
					 g_param_spec_string ("info",
							      _("Info to be shown for this proposal"),
							      _("Info to be shown for this proposal"),
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * GtkSourceCompletionProposal:icon:
	 *
	 * Icon to be shown for this proposal
	 */
	g_object_class_install_property (object_class,
					 PROP_ICON,
					 g_param_spec_pointer ("icon",
							      _("Icon to be shown for this proposal"),
							      _("Icon to be shown for this proposal"),
							      G_PARAM_READWRITE));

	/**
	 * GtkSourceCompletionProposal:page-name:
	 *
	 * Page name for this proposal
	 */
	g_object_class_install_property (object_class,
					 PROP_PAGE_NAME,
					 g_param_spec_string ("page-name",
							      _("Page name for this proposal"),
							      _("Page name for this proposal"),
							      NULL,
							      G_PARAM_READWRITE));
}

/**
 * gtk_source_completion_proposal_new:
 * @label: Item label that will be shown in the completion popup. 
 * @info: Item info markup that will be shown when the user select to view the item info.
 * @icon: Item icon that will be shown in the completion popup
 *
 * This function creates a new #GtkSourceCompletionProposal. By default, when the user selects 
 * the proposal, the proposal label will be inserted into the GtkTextView.
 * You can overwrite the apply and disply-info functions to overwrite the default.
 *
 * Returns: A new #GtkSourceCompletionProposal
 */
GtkSourceCompletionProposal *
gtk_source_completion_proposal_new (const gchar *label,
				    const gchar *info,
				    GdkPixbuf *icon)
{
	GtkSourceCompletionProposal *self;
	
	self = GTK_SOURCE_COMPLETION_PROPOSAL (g_object_new (GTK_TYPE_SOURCE_COMPLETION_PROPOSAL, NULL));
	
	self->priv->label = g_strdup (label);
	self->priv->info = g_strdup (info);
	if (icon != NULL)
	{
		self->priv->icon = g_object_ref (icon);
	}
	else
	{
		self->priv->icon = NULL;
	}
	
	return self;
}

/**
 * gtk_source_completion_proposal_get_label:
 * @self: The #GtkSourceCompletionProposal
 *
 * Returns: The proposal label that will be shown into the popup
 */
const gchar *
gtk_source_completion_proposal_get_label (GtkSourceCompletionProposal *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self), NULL);
	
	return self->priv->label;
}

/**
 * gtk_source_completion_proposal_get_icon:
 * @self: The #GtkSourceCompletionProposal
 *
 * Gets the icon of this @proposal that will be shown into the popup.
 *
 * Returns: the icon of this @proposal that will be shown into the popup
 */
const GdkPixbuf *
gtk_source_completion_proposal_get_icon (GtkSourceCompletionProposal *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self), NULL);

	return self->priv->icon;
}

/**
 * gtk_source_completion_proposal_set_page_name:
 * @self: The #GtkSourceCompletionProposal
 * @page_name: The name for the page
 *
 * Sets the name of the page where this proposal will be shown.
 * If @page_name is %NULL the default page will be used.
 */
void
gtk_source_completion_proposal_set_page_name (GtkSourceCompletionProposal *self,
					      const gchar *page_name)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self));

	g_free (self->priv->page_name);
	
	if (page_name == NULL)
	{
		self->priv->page_name = g_strdup (GTK_SOURCE_COMPLETION_PROPOSAL_DEFAULT_PAGE);
	}
	else
	{
		self->priv->page_name = g_strdup (page_name);
	}
}

/**
 * gtk_source_completion_proposal_get_page_name:
 * @self: The #GtkSourceCompletionProposal
 *
 * Gets the page name where the @proposal will be placed.
 *
 * Returns: the page name where the @proposal will be placed.
 */
const gchar *
gtk_source_completion_proposal_get_page_name (GtkSourceCompletionProposal *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self), NULL);
	
	return self->priv->page_name;
}

/**
 * gtk_source_completion_proposal_get_info:
 * @self: The #GtkSourceCompletionProposal
 *
 * The completion calls this function when the user wants to see the proposal info.
 * You can overwrite this function if you need to change the default mechanism.
 *
 * Returns: The proposal info markup asigned for this proposal or NULL;
 */
const gchar *
gtk_source_completion_proposal_get_info (GtkSourceCompletionProposal *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self), NULL);

	return GTK_SOURCE_COMPLETION_PROPOSAL_GET_CLASS (self)->get_info (self);
}

/**
 * gtk_source_completion_proposal_apply:
 * @self: The #GtkSourceCompletionProposal
 * @view: The #GtkTextView
 * 
 * The completion calls this function when the user selects the proposal. 
 * The default handler insert the proposal label into the view. 
 * You can overwrite this function.
 */
void
gtk_source_completion_proposal_apply (GtkSourceCompletionProposal *self,
				      GtkTextView *view)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (self));
	
	GTK_SOURCE_COMPLETION_PROPOSAL_GET_CLASS (self)->apply (self, view);
}



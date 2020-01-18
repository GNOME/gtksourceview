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

#include "config.h"

#include <gtksourceview/gtksourcesnippet.h>

#include "gtksourcecompletionsnippetsproposal-private.h"

struct _GtkSourceCompletionSnippetsProposal
{
	GObject parent_instance;
	GtkSourceSnippet *snippet;
};

static void completion_proposal_iface_init (GtkSourceCompletionProposalInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionSnippetsProposal,
                         gtk_source_completion_snippets_proposal,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
                                                completion_proposal_iface_init))

enum {
	PROP_0,
	PROP_SNIPPET,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static gchar *
get_label (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionSnippetsProposal *self = GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (proposal);
	const gchar *label;

	label = gtk_source_snippet_get_name (self->snippet);

	if (label == NULL)
		label = gtk_source_snippet_get_trigger (self->snippet);

	if (label == NULL)
		label = gtk_source_snippet_get_description (self->snippet);

	return g_strdup (label);
}

static const gchar *
get_icon_name (GtkSourceCompletionProposal *proposal)
{
	/* TODO: Add a symbolic icon for GSV to use */
	return NULL;
}

static gchar *
get_info (GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionSnippetsProposal *self = GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (proposal);
	return g_strdup (gtk_source_snippet_get_description (self->snippet));
}

static void
completion_proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
	iface->get_label = get_label;
	iface->get_icon_name = get_icon_name;
	iface->get_info = get_info;
}

static void
gtk_source_completion_snippets_proposal_finalize (GObject *object)
{
	GtkSourceCompletionSnippetsProposal *self = GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (object);

	g_clear_object (&self->snippet);

	G_OBJECT_CLASS (gtk_source_completion_snippets_proposal_parent_class)->finalize (object);
}

static void
gtk_source_completion_snippets_proposal_get_property (GObject    *object,
                                                      guint       prop_id,
                                                      GValue     *value,
                                                      GParamSpec *pspec)
{
	GtkSourceCompletionSnippetsProposal *self = GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (object);

	switch (prop_id)
	{
	case PROP_SNIPPET:
		g_value_set_object (value, self->snippet);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_snippets_proposal_set_property (GObject      *object,
                                                      guint         prop_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec)
{
	GtkSourceCompletionSnippetsProposal *self = GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (object);

	switch (prop_id)
	{
	case PROP_SNIPPET:
		self->snippet = g_value_dup_object (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_snippets_proposal_class_init (GtkSourceCompletionSnippetsProposalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_completion_snippets_proposal_finalize;
	object_class->get_property = gtk_source_completion_snippets_proposal_get_property;
	object_class->set_property = gtk_source_completion_snippets_proposal_set_property;

	properties [PROP_SNIPPET] =
		g_param_spec_object ("snippet",
		                     "snippet",
		                     "The snippet to expand",
		                     GTK_SOURCE_TYPE_SNIPPET,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_completion_snippets_proposal_init (GtkSourceCompletionSnippetsProposal *self)
{
}

GtkSourceCompletionProposal *
gtk_source_completion_snippets_proposal_new (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_SNIPPETS_PROPOSAL,
	                     "snippet", snippet,
	                     NULL);
}

/**
 * gtk_source_completion_snippets_proposal_get_snippet:
 *
 * Returns: (transfer none): a #GtkSourceSnippet
 */
GtkSourceSnippet *
gtk_source_completion_snippets_proposal_get_snippet (GtkSourceCompletionSnippetsProposal *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (self), NULL);

	return self->snippet;
}

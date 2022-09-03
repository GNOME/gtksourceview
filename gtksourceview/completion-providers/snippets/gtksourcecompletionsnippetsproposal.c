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

static char *
gtk_source_completion_snippets_proposal_get_typed_text (GtkSourceCompletionProposal *proposal)
{
  return g_strdup (GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (proposal)->info.trigger);
}

static void
proposal_iface_init (GtkSourceCompletionProposalInterface *iface)
{
  iface->get_typed_text = gtk_source_completion_snippets_proposal_get_typed_text;
}

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionSnippetsProposal,
                         gtk_source_completion_snippets_proposal,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL, proposal_iface_init))

enum {
	PROP_0,
	PROP_SNIPPET,
	PROP_TRIGGER,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

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
		g_value_take_object (value, gtk_source_completion_snippets_proposal_dup_snippet (self));
		break;

	case PROP_TRIGGER:
		g_value_set_string (value, self->info.trigger);
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

	object_class->get_property = gtk_source_completion_snippets_proposal_get_property;

	properties [PROP_SNIPPET] =
		g_param_spec_object ("snippet", NULL, NULL,
		                     GTK_SOURCE_TYPE_SNIPPET,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_TRIGGER] =
		g_param_spec_string ("trigger", NULL, NULL,
		                     NULL,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_completion_snippets_proposal_init (GtkSourceCompletionSnippetsProposal *self)
{
}

GtkSourceCompletionProposal *
gtk_source_completion_snippets_proposal_new (GtkSourceSnippetBundle     *bundle,
					     const GtkSourceSnippetInfo *info)
{
	GtkSourceCompletionSnippetsProposal *ret;

	g_return_val_if_fail (info != NULL, NULL);

	ret = g_object_new (GTK_SOURCE_TYPE_COMPLETION_SNIPPETS_PROPOSAL, NULL);
	g_set_object (&ret->bundle, bundle);
	ret->info = *info;

	return GTK_SOURCE_COMPLETION_PROPOSAL (ret);
}

GtkSourceSnippet *
gtk_source_completion_snippets_proposal_dup_snippet (GtkSourceCompletionSnippetsProposal *self)
{
	return _gtk_source_snippet_bundle_create_snippet (self->bundle, &self->info);
}

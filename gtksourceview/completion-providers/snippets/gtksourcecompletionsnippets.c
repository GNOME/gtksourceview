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

#include <glib/gi18n.h>

#include <gtksourceview/gtksource-enumtypes.h>
#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletioncell.h>
#include <gtksourceview/gtksourcecompletioncontext.h>
#include <gtksourceview/gtksourceiter-private.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcesnippet.h>
#include <gtksourceview/gtksourcesnippetmanager.h>
#include <gtksourceview/gtksourceview.h>

#include "gtksourcecompletionsnippets.h"
#include "gtksourcecompletionsnippetsproposal-private.h"

/**
 * SECTION:completionsnippets
 * @title: GtkSourceCompletionSnippets
 * @short_description: A GtkSourceCompletionProvider for the completion of snippets
 *
 * The #GtkSourceCompletionSnippets is an example of an implementation of
 * the #GtkSourceCompletionProvider interface. The proposals are snippets
 * registered with the #GtkSourceSnippetManager.
 *
 * Since: 5.0
 */

struct _GtkSourceSnippetResults
{
	GObject     parent_instance;
	GListModel *snippets;
};

static void list_model_iface_init (GListModelInterface *iface);

#define GTK_SOURCE_TYPE_SNIPPET_RESULTS (gtk_source_snippet_results_get_type())
G_DECLARE_FINAL_TYPE (GtkSourceSnippetResults, gtk_source_snippet_results, GTK_SOURCE, SNIPPET_RESULTS, GObject)
G_DEFINE_TYPE_WITH_CODE (GtkSourceSnippetResults, gtk_source_snippet_results, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GType
gtk_source_snippet_results_get_item_type (GListModel *model)
{
	return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gtk_source_snippet_results_get_n_items (GListModel *model)
{
	GtkSourceSnippetResults *self = (GtkSourceSnippetResults *)model;
	return g_list_model_get_n_items (self->snippets);
}

static gpointer
gtk_source_snippet_results_get_item (GListModel *model,
                                     guint       position)
{
	GtkSourceSnippetResults *self = (GtkSourceSnippetResults *)model;
	GtkSourceSnippet *snippet = g_list_model_get_item (self->snippets, position);
	return gtk_source_completion_snippets_proposal_new (snippet);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_item_type = gtk_source_snippet_results_get_item_type;
	iface->get_n_items = gtk_source_snippet_results_get_n_items;
	iface->get_item = gtk_source_snippet_results_get_item;
}

static GListModel *
gtk_source_snippet_results_new (GListModel *base_model)
{
	GtkSourceSnippetResults *self = g_object_new (GTK_SOURCE_TYPE_SNIPPET_RESULTS, NULL);
	self->snippets = g_object_ref (base_model);
	return G_LIST_MODEL (self);
}

static void
gtk_source_snippet_results_finalize (GObject *object)
{
	GtkSourceSnippetResults *self = (GtkSourceSnippetResults *)object;

	g_clear_object (&self->snippets);

	G_OBJECT_CLASS (gtk_source_snippet_results_parent_class)->finalize (object);
}

static void
gtk_source_snippet_results_class_init (GtkSourceSnippetResultsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_snippet_results_finalize;
}

static void
gtk_source_snippet_results_init (GtkSourceSnippetResults *self)
{
}

typedef struct
{
	char *title;
	int   priority;
	int   minimum_word_size;
} GtkSourceCompletionSnippetsPrivate;

static void completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionSnippets, gtk_source_completion_snippets, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSourceCompletionSnippets)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                completion_provider_iface_init))

enum {
	PROP_0,
	PROP_TITLE,
	PROP_PRIORITY,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

static GListModel *
gtk_source_completion_snippets_populate (GtkSourceCompletionProvider  *provider,
                                         GtkSourceCompletionContext   *context,
                                         GError                      **error)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);
	GtkSourceCompletionActivation activation;
	GtkSourceSnippetManager *manager;
	GtkSourceLanguage *language;
	GtkSourceBuffer *buffer;
	const gchar *language_id = "";
	GtkTextIter begin, end;
	GListModel *matches;
	GListModel *results = NULL;
	gchar *word;

	if (!gtk_source_completion_context_get_bounds (context, &begin, &end))
	{
		return NULL;
	}

	buffer = gtk_source_completion_context_get_buffer (context);
	word = gtk_source_completion_context_get_word (context);
	activation = gtk_source_completion_context_get_activation (context);
	manager = gtk_source_snippet_manager_get_default ();
	language = gtk_source_buffer_get_language (buffer);

	if (word == NULL ||
	    (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE &&
	     g_utf8_strlen (word, -1) < (glong)priv->minimum_word_size))
	{
		g_free (word);
		return NULL;
	}

	if (language != NULL)
	{
		language_id = gtk_source_language_get_id (language);
	}

	matches = gtk_source_snippet_manager_list_matching (manager, NULL, language_id, word);

	if (matches != NULL)
	{
		results = gtk_source_snippet_results_new (matches);
	}

	g_clear_object (&matches);
	g_free (word);

	if (results == NULL)
	{
		g_set_error (error,
		             G_IO_ERROR,
		             G_IO_ERROR_NOT_SUPPORTED,
		             "No results");
	}

	return results;
}

static gchar *
gtk_source_completion_snippets_get_title (GtkSourceCompletionProvider *self)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (self);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return g_strdup (priv->title);
}

static gint
gtk_source_completion_snippets_get_priority (GtkSourceCompletionProvider *provider,
                                             GtkSourceCompletionContext  *context)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return priv->priority;
}

static void
gtk_source_completion_snippets_activate (GtkSourceCompletionProvider *provider,
                                         GtkSourceCompletionContext  *context,
                                         GtkSourceCompletionProposal *proposal)
{
	GtkSourceCompletionSnippetsProposal *p = (GtkSourceCompletionSnippetsProposal *)proposal;
	GtkTextIter begin, end;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (p));

	if (gtk_source_completion_context_get_bounds (context, &begin, &end))
	{
		GtkTextBuffer *buffer = gtk_text_iter_get_buffer (&begin);
		GtkSourceView *view = gtk_source_completion_context_get_view (context);
		GtkSourceSnippet *snippet, *copy;

		snippet = gtk_source_completion_snippets_proposal_get_snippet (GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (proposal));
		copy = gtk_source_snippet_copy (snippet);

		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_source_view_push_snippet (view, copy, &begin);
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

		g_object_unref (copy);
	}
}

static void
gtk_source_completion_snippets_display (GtkSourceCompletionProvider *provider,
                                        GtkSourceCompletionContext  *context,
                                        GtkSourceCompletionProposal *proposal,
                                        GtkSourceCompletionCell     *cell)
{
	GtkSourceCompletionSnippetsProposal *p = (GtkSourceCompletionSnippetsProposal *)proposal;
	GtkSourceCompletionColumn column;
	GtkSourceSnippet *snippet;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (p));
	g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

	snippet = gtk_source_completion_snippets_proposal_get_snippet (p);
	column = gtk_source_completion_cell_get_column (cell);

	if (column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT)
	{
		const char *trigger = gtk_source_snippet_get_trigger (snippet);
		char *word = gtk_source_completion_context_get_word (context);
		PangoAttrList *highlight = gtk_source_completion_fuzzy_highlight (trigger, word);

		gtk_source_completion_cell_set_text_with_attributes (cell, trigger, highlight);

		pango_attr_list_unref (highlight);
		g_free (word);
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_ICON)
	{
		gtk_source_completion_cell_set_icon_name (cell,
		                                          "completion-snippet-symbolic");
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT)
	{
		gtk_source_completion_cell_set_text (cell,
		                                     gtk_source_snippet_get_trigger (snippet));
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_DETAILS)
	{
		gtk_source_completion_cell_set_text (cell,
		                                     gtk_source_snippet_get_description (snippet));
	}
}

static void
gtk_source_completion_snippets_refilter (GtkSourceCompletionProvider *provider,
                                         GtkSourceCompletionContext  *context,
                                         GListModel                  *model)
{
	GtkFilterListModel *filter_model;
	GtkExpression *expression;
	GtkStringFilter *filter;
	gchar *word;

	g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (G_IS_LIST_MODEL (model));

	word = gtk_source_completion_context_get_word (context);

	if (GTK_IS_FILTER_LIST_MODEL (model))
	{
		model = gtk_filter_list_model_get_model (GTK_FILTER_LIST_MODEL (model));
	}

	if (!word || !word[0])
	{
		gtk_source_completion_context_set_proposals_for_provider (context, provider, model);
		g_free (word);
		return;
	}

	expression = gtk_property_expression_new (GTK_SOURCE_TYPE_COMPLETION_SNIPPETS_PROPOSAL, NULL, "trigger");
	filter = gtk_string_filter_new (g_steal_pointer (&expression));
	gtk_string_filter_set_search (GTK_STRING_FILTER (filter), word);
	filter_model = gtk_filter_list_model_new (g_object_ref (model),
	                                          GTK_FILTER (g_steal_pointer (&filter)));
	gtk_filter_list_model_set_incremental (filter_model, TRUE);
	gtk_source_completion_context_set_proposals_for_provider (context, provider, G_LIST_MODEL (filter_model));

	g_clear_object (&filter_model);
	g_free (word);
}


static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->get_title = gtk_source_completion_snippets_get_title;
	iface->get_priority = gtk_source_completion_snippets_get_priority;
	iface->populate = gtk_source_completion_snippets_populate;
	iface->activate = gtk_source_completion_snippets_activate;
	iface->display = gtk_source_completion_snippets_display;
	iface->refilter = gtk_source_completion_snippets_refilter;
}

static void
gtk_source_completion_snippets_finalize (GObject *object)
{
	GtkSourceCompletionSnippets *provider = GTK_SOURCE_COMPLETION_SNIPPETS (object);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (provider);

	g_clear_pointer (&priv->title, g_free);

	G_OBJECT_CLASS (gtk_source_completion_snippets_parent_class)->finalize (object);
}

static void
gtk_source_completion_snippets_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
	GtkSourceCompletionSnippets *self = GTK_SOURCE_COMPLETION_SNIPPETS (object);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (self);

	switch (prop_id)
	{
	case PROP_TITLE:
		g_value_set_string (value, priv->title);
		break;

	case PROP_PRIORITY:
		g_value_set_int (value, priv->priority);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_snippets_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
	GtkSourceCompletionSnippets *self = GTK_SOURCE_COMPLETION_SNIPPETS (object);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (self);

	switch (prop_id)
	{
	case PROP_TITLE:
		g_free (priv->title);
		priv->title = g_value_dup_string (value);

		if (priv->title == NULL)
		{
			priv->title = g_strdup (_("Snippets"));
		}
		break;

	case PROP_PRIORITY:
		priv->priority = g_value_get_int (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_snippets_class_init (GtkSourceCompletionSnippetsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_completion_snippets_finalize;
	object_class->get_property = gtk_source_completion_snippets_get_property;
	object_class->set_property = gtk_source_completion_snippets_set_property;

	properties[PROP_TITLE] =
		g_param_spec_string ("title",
		                     "Title",
		                     "The provider title",
		                     NULL,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PRIORITY] =
		g_param_spec_int ("priority",
		                  "Priority",
		                  "Provider priority",
		                  G_MININT,
		                  G_MAXINT,
		                  0,
		                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_completion_snippets_init (GtkSourceCompletionSnippets *self)
{
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (self);

	priv->minimum_word_size = 2;
}

GtkSourceCompletionSnippets *
gtk_source_completion_snippets_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_SNIPPETS, NULL);
}

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
 * GtkSourceCompletionSnippets:
 *
 * A [iface@CompletionProvider] for the completion of snippets.
 *
 * The `GtkSourceCompletionSnippets` is an example of an implementation of
 * the [iface@CompletionProvider] interface. The proposals are snippets
 * registered with the [class@SnippetManager].
 */

typedef struct
{
	char *word;
	int minimum_word_size;
	guint filter_all : 1;
} FilterData;

static void
filter_data_finalize (FilterData *fd)
{
	g_clear_pointer (&fd->word, g_free);
}

static void
filter_data_release (FilterData *fd)
{
	g_atomic_rc_box_release_full (fd, (GDestroyNotify)filter_data_finalize);
}

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

	if (self->snippets != NULL)
	{
		return g_list_model_get_n_items (self->snippets);
	}

	return 0;
}

static gpointer
gtk_source_snippet_results_get_item (GListModel *model,
                                     guint       position)
{
	GtkSourceSnippetResults *self = (GtkSourceSnippetResults *)model;

	g_assert (!self->snippets || GTK_SOURCE_IS_SNIPPET_BUNDLE (self->snippets));

	if (self->snippets != NULL)
	{
		GtkSourceSnippetBundle *bundle = GTK_SOURCE_SNIPPET_BUNDLE (self->snippets);
		const GtkSourceSnippetInfo *info = _gtk_source_snippet_bundle_get_info (bundle, position);
		return gtk_source_completion_snippets_proposal_new (bundle, info);
	}

	return NULL;
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
	g_set_object (&self->snippets, base_model);
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
	FilterData *filter_data;
	char       *title;
	int         priority;
} GtkSourceCompletionSnippetsPrivate;

static void completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionSnippets, gtk_source_completion_snippets, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSourceCompletionSnippets)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER, completion_provider_iface_init))

enum {
	PROP_0,
	PROP_TITLE,
	PROP_PRIORITY,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
filter_snippet_func (gpointer item,
		     gpointer user_data)
{
	GtkSourceCompletionSnippetsProposal *proposal = item;
	FilterData *fd = user_data;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (proposal));
	g_assert (fd != NULL);

	if (fd->filter_all)
	{
		return FALSE;
	}

	if (proposal->info.trigger == NULL)
	{
		return FALSE;
	}

	/* We could do fuzzy here, or we could also do case-insensitive (strcasestr),
	 * but generally, having case match can be helpful in it's own. We could always
	 * add more tweaks to this if they become necessary.
	 */

	return strstr (proposal->info.trigger, fd->word) != NULL;
}

static GListModel *
gtk_source_completion_snippets_populate (GtkSourceCompletionProvider  *provider,
                                         GtkSourceCompletionContext   *context,
                                         GError                      **error)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);
	GtkSourceCompletionActivation activation;
	GtkSourceSnippetManager *manager;
	GtkFilterListModel *filter_model;
	GtkSourceLanguage *language;
	GtkCustomFilter *filter;
	GtkSourceBuffer *buffer;
	const char *language_id = "";
	GListModel *matches;
	GListModel *results;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (snippets));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	buffer = gtk_source_completion_context_get_buffer (context);
	activation = gtk_source_completion_context_get_activation (context);
	manager = gtk_source_snippet_manager_get_default ();
	language = gtk_source_buffer_get_language (buffer);

	/* Update state in indirected struct for filters */
	g_free (priv->filter_data->word);
	priv->filter_data->word = gtk_source_completion_context_get_word (context);

	priv->filter_data->filter_all =
		(priv->filter_data->word == NULL ||
		 (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE &&
		  g_utf8_strlen (priv->filter_data->word, -1) < (glong)priv->filter_data->minimum_word_size));

	if (language != NULL)
	{
		language_id = gtk_source_language_get_id (language);
	}

	matches = gtk_source_snippet_manager_list_matching (manager, NULL, language_id, NULL);
	results = gtk_source_snippet_results_new (matches);
	g_clear_object (&matches);

	/* Create a filter we can re-use so we get optimal updates on
	 * change (only refilter the portions either matching or not-matching
	 * when words change).
	 */
	filter = gtk_custom_filter_new (filter_snippet_func,
	                                g_atomic_rc_box_acquire (priv->filter_data),
	                                (GDestroyNotify)filter_data_release);
	filter_model = gtk_filter_list_model_new (results, GTK_FILTER (filter));
	gtk_filter_list_model_set_incremental (filter_model, TRUE);

	return G_LIST_MODEL (filter_model);
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
		GtkSourceSnippet *snippet;

		snippet = gtk_source_completion_snippets_proposal_dup_snippet (p);

		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &end);
		gtk_source_view_push_snippet (view, snippet, &begin);
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

		g_object_unref (snippet);
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

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (p));
	g_assert (GTK_SOURCE_IS_COMPLETION_CELL (cell));

	column = gtk_source_completion_cell_get_column (cell);

	if (column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT)
	{
		const char *trigger = p->info.trigger;
		char *word = gtk_source_completion_context_get_word (context);
		PangoAttrList *highlight = gtk_source_completion_fuzzy_highlight (trigger, word);

		gtk_source_completion_cell_set_text_with_attributes (cell, trigger, highlight);

		pango_attr_list_unref (highlight);
		g_free (word);
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_ICON)
	{
		gtk_source_completion_cell_set_icon_name (cell, "completion-snippet-symbolic");
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT)
	{
		gtk_source_completion_cell_set_text (cell, p->info.trigger);
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_DETAILS)
	{
		gtk_source_completion_cell_set_text (cell, p->info.description);
	}
	else
	{
		gtk_source_completion_cell_set_text (cell, NULL);
	}
}

static void
gtk_source_completion_snippets_refilter (GtkSourceCompletionProvider *provider,
                                         GtkSourceCompletionContext  *context,
                                         GListModel                  *model)
{
	GtkSourceCompletionSnippets *self = (GtkSourceCompletionSnippets *)provider;
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (self);
	GtkFilterChange change;
	GtkFilter *filter;
	char *old_word;
	char *word;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (self));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_assert (GTK_IS_FILTER_LIST_MODEL (model));

	word = gtk_source_completion_context_get_word (context);
	old_word = g_steal_pointer (&priv->filter_data->word);

	if (old_word && g_str_has_prefix (word, old_word))
		change = GTK_FILTER_CHANGE_MORE_STRICT;
	else if (old_word && g_str_has_prefix (old_word, word))
		change = GTK_FILTER_CHANGE_LESS_STRICT;
	else
		change = GTK_FILTER_CHANGE_DIFFERENT;

	if (priv->filter_data->filter_all)
	{
		priv->filter_data->filter_all = FALSE;
		change = GTK_FILTER_CHANGE_LESS_STRICT;
	}

	priv->filter_data->word = word;

	filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (model));
	gtk_filter_changed (filter, change);

	g_free (old_word);
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
	g_clear_pointer (&priv->filter_data, filter_data_release);

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

	priv->filter_data = g_atomic_rc_box_alloc0 (sizeof (FilterData));
	priv->filter_data->minimum_word_size = 2;
}

GtkSourceCompletionSnippets *
gtk_source_completion_snippets_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_SNIPPETS, NULL);
}

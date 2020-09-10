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

#include <gtksourceview/completion-providers/words/gtksourcecompletionwordsutils-private.h>

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
	const gchar *language_id = NULL;
	GtkTextIter begin, end;
	GListModel *matches;
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

	g_free (word);

	return matches;
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
		gtk_source_completion_cell_set_text (cell,
		                                     gtk_source_snippet_get_name (snippet));
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_ICON)
	{
		gtk_source_completion_cell_set_icon_name (cell,
		                                          "completion-snippet-symbolic");
	}
	else if (column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT)
	{
		gtk_source_completion_cell_set_text (cell,
		                                     gtk_source_snippet_get_description (snippet));
	}
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->get_title = gtk_source_completion_snippets_get_title;
	iface->get_priority = gtk_source_completion_snippets_get_priority;
	iface->populate = gtk_source_completion_snippets_populate;
	iface->activate = gtk_source_completion_snippets_activate;
	iface->display = gtk_source_completion_snippets_display;
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

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
	GdkTexture                    *icon;
	gchar                         *name;
	GtkSourceView                 *view;
	gint                           interactive_delay;
	gint                           priority;
	gint                           minimum_word_size;
	GtkSourceCompletionActivation  activation;
} GtkSourceCompletionSnippetsPrivate;

static void completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionSnippets, gtk_source_completion_snippets, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkSourceCompletionSnippets)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
                                                completion_provider_iface_init))

enum {
	PROP_0,
	PROP_ACTIVATION,
	PROP_ICON,
	PROP_INTERACTIVE_DELAY,
	PROP_NAME,
	PROP_PRIORITY,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gchar *
get_word_at_iter (GtkTextIter *iter)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_line;
	gchar *line_text;
	gchar *word;

	buffer = gtk_text_iter_get_buffer (iter);
	start_line = *iter;
	gtk_text_iter_set_line_offset (&start_line, 0);

	line_text = gtk_text_buffer_get_text (buffer, &start_line, iter, FALSE);
	word = _gtk_source_completion_words_utils_get_end_word (line_text);
	g_free (line_text);

	return word;
}

static GtkSourceView *
get_view (GtkSourceCompletionContext *context)
{
	GtkSourceCompletion *completion;
	GtkSourceView *view;

	g_object_get (context, "completion", &completion, NULL);
	view = gtk_source_completion_get_view (completion);
	g_object_unref (completion);

	return view;
}

static void
gtk_source_completion_snippets_populate (GtkSourceCompletionProvider *provider,
                                         GtkSourceCompletionContext  *context)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);
	GtkSourceCompletionActivation activation;
	GtkSourceSnippetManager *manager;
	GtkSourceLanguage *language;
	GtkTextBuffer *buffer;
	const gchar *language_id = NULL;
	GtkTextIter iter;
	GListModel *matches;
	GList *list = NULL;
	gchar *word;
	guint n_items;

	if (!gtk_source_completion_context_get_iter (context, &iter))
	{
		gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
		return;
	}

	priv->view = get_view (context);

	word = get_word_at_iter (&iter);

	activation = gtk_source_completion_context_get_activation (context);

	if (word == NULL ||
	    (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE &&
	     g_utf8_strlen (word, -1) < (glong)priv->minimum_word_size))
	{
		g_free (word);
		gtk_source_completion_context_add_proposals (context, provider, NULL, TRUE);
		return;
	}

	manager = gtk_source_snippet_manager_get_default ();
	buffer = gtk_text_iter_get_buffer (&iter);
	language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (buffer));
	if (language != NULL)
		language_id = gtk_source_language_get_id (language);

	matches = gtk_source_snippet_manager_list_matching (manager, NULL, language_id, word);
	n_items = g_list_model_get_n_items (matches);

	for (guint i = 0; i < n_items; i++)
	{
		GtkSourceSnippet *snippet = g_list_model_get_item (matches, i);
		list = g_list_prepend (list, gtk_source_completion_snippets_proposal_new (snippet));
		g_object_unref (snippet);
	}

	gtk_source_completion_context_add_proposals (context, provider, list, TRUE);

  g_list_free_full (list, g_object_unref);
	g_object_unref (matches);
	g_free (word);
}

static gchar *
gtk_source_completion_snippets_get_name (GtkSourceCompletionProvider *self)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (self);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return g_strdup (priv->name);
}

static GdkTexture *
gtk_source_completion_snippets_get_icon (GtkSourceCompletionProvider *self)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (self);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return priv->icon;
}

static gint
gtk_source_completion_snippets_get_interactive_delay (GtkSourceCompletionProvider *provider)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return priv->interactive_delay;
}

static gint
gtk_source_completion_snippets_get_priority (GtkSourceCompletionProvider *provider)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return priv->priority;
}

static GtkSourceCompletionActivation
gtk_source_completion_snippets_get_activation (GtkSourceCompletionProvider *provider)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);

	return priv->activation;
}

static gboolean
gtk_source_completion_snippets_activate_proposal (GtkSourceCompletionProvider *provider,
                                                  GtkSourceCompletionProposal *proposal,
                                                  GtkTextIter                 *iter)
{
	GtkSourceCompletionSnippets *snippets = GTK_SOURCE_COMPLETION_SNIPPETS (provider);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (snippets);
	GtkSourceSnippet *snippet;
	GtkSourceSnippet *copy;
	GtkTextIter begin;

	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS (snippets));
	g_assert (GTK_SOURCE_IS_COMPLETION_SNIPPETS_PROPOSAL (proposal));

	snippet = gtk_source_completion_snippets_proposal_get_snippet (GTK_SOURCE_COMPLETION_SNIPPETS_PROPOSAL (proposal));

	if (snippet == NULL || priv->view == NULL)
	{
		return FALSE;
	}

	if (_gtk_source_iter_ends_full_word (iter))
	{
		begin = *iter;
		_gtk_source_iter_backward_full_word_start (&begin);
		gtk_text_buffer_delete (gtk_text_iter_get_buffer (&begin), &begin, iter);
	}

	copy = gtk_source_snippet_copy (snippet);
	gtk_source_view_push_snippet (priv->view, copy, iter);
	g_object_unref (copy);

	return TRUE;
}

static void
completion_provider_iface_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->get_activation = gtk_source_completion_snippets_get_activation;
	iface->get_icon = gtk_source_completion_snippets_get_icon;
	iface->get_interactive_delay = gtk_source_completion_snippets_get_interactive_delay;
	iface->get_name = gtk_source_completion_snippets_get_name;
	iface->get_name = gtk_source_completion_snippets_get_name;
	iface->get_priority = gtk_source_completion_snippets_get_priority;
	iface->populate = gtk_source_completion_snippets_populate;
	iface->activate_proposal = gtk_source_completion_snippets_activate_proposal;
}

static void
gtk_source_completion_snippets_finalize (GObject *object)
{
	GtkSourceCompletionSnippets *provider = GTK_SOURCE_COMPLETION_SNIPPETS (object);
	GtkSourceCompletionSnippetsPrivate *priv = gtk_source_completion_snippets_get_instance_private (provider);

	priv->view = NULL;
	g_clear_pointer (&priv->name, g_free);
	g_clear_object (&priv->icon);

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
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;

	case PROP_ICON:
		g_value_set_object (value, priv->icon);
		break;

	case PROP_INTERACTIVE_DELAY:
		g_value_set_int (value, priv->interactive_delay);
		break;

	case PROP_PRIORITY:
		g_value_set_int (value, priv->priority);
		break;

	case PROP_ACTIVATION:
		g_value_set_flags (value, priv->activation);
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
	case PROP_NAME:
		g_free (priv->name);
		priv->name = g_value_dup_string (value);

		if (priv->name == NULL)
		{
			priv->name = g_strdup (_("Snippets"));
		}
		break;

	case PROP_ICON:
		g_clear_object (&priv->icon);
		priv->icon = g_value_dup_object (value);
		break;

	case PROP_INTERACTIVE_DELAY:
		priv->interactive_delay = g_value_get_int (value);
		break;

	case PROP_PRIORITY:
		priv->priority = g_value_get_int (value);
		break;

	case PROP_ACTIVATION:
		priv->activation = g_value_get_flags (value);
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

	properties[PROP_NAME] =
		g_param_spec_string ("name",
				     "Name",
				     "The provider name",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_ICON] =
		g_param_spec_object ("icon",
				     "Icon",
				     "The provider icon",
				     GDK_TYPE_TEXTURE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_INTERACTIVE_DELAY] =
		g_param_spec_int ("interactive-delay",
				  "Interactive Delay",
				  "The delay before initiating interactive completion",
				  -1,
				  G_MAXINT,
				  50,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PRIORITY] =
		g_param_spec_int ("priority",
				  "Priority",
				  "Provider priority",
				  G_MININT,
				  G_MAXINT,
				  0,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_ACTIVATION] =
		g_param_spec_flags ("activation",
				    "Activation",
				    "The type of activation",
				    GTK_SOURCE_TYPE_COMPLETION_ACTIVATION,
				    GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE |
				    GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED,
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

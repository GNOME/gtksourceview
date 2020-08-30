/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 * Copyright (C) 2013 - Sébastien Wilmet
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:completionwords
 * @title: GtkSourceCompletionWords
 * @short_description: A GtkSourceCompletionProvider for the completion of words
 *
 * The #GtkSourceCompletionWords is an example of an implementation of
 * the #GtkSourceCompletionProvider interface. The proposals are words
 * appearing in the registered #GtkTextBuffer<!-- -->s.
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include "gtksourceview/gtksource.h"
#include "gtksourceview/gtksource-enumtypes.h"

#include "gtksourcecompletionwords.h"
#include "gtksourcecompletionwordslibrary-private.h"
#include "gtksourcecompletionwordsbuffer-private.h"
#include "gtksourcecompletionwordsutils-private.h"

#define BUFFER_KEY "GtkSourceCompletionWordsBufferKey"

enum
{
	PROP_0,
	PROP_TITLE,
	PROP_PROPOSALS_BATCH_SIZE,
	PROP_SCAN_BATCH_SIZE,
	PROP_MINIMUM_WORD_SIZE,
	PROP_PRIORITY,
	N_PROPERTIES
};

typedef struct
{
	gchar *title;

	gchar *word;
	gint word_len;
	guint idle_id;

	GtkSourceCompletionContext *context;
	GSequenceIter *populate_iter;

	guint proposals_batch_size;
	guint scan_batch_size;
	guint minimum_word_size;

	GtkSourceCompletionWordsLibrary *library;
	GList *buffers;

	gint priority;
} GtkSourceCompletionWordsPrivate;

typedef struct
{
	GtkSourceCompletionWords *words;
	GtkSourceCompletionWordsBuffer *buffer;
} BufferBinding;

typedef struct
{
	GtkSourceCompletionContext *context;
	GtkSourceCompletionActivation activation;
	GSequenceIter *populate_iter;
	GListStore *ret;
	char *word;
	guint word_len;
} Populate;

static GParamSpec *properties[N_PROPERTIES];

static void gtk_source_completion_words_iface_init (GtkSourceCompletionProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionWords,
			 gtk_source_completion_words,
			 G_TYPE_OBJECT,
			 G_ADD_PRIVATE (GtkSourceCompletionWords)
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
				 		gtk_source_completion_words_iface_init))

static void
populate_free (Populate *p)
{
	g_clear_object (&p->context);
	g_clear_object (&p->ret);
	g_clear_pointer (&p->word, g_free);
	g_slice_free (Populate, p);
}

static gchar *
gtk_source_completion_words_get_title (GtkSourceCompletionProvider *self)
{
	GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (self);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);

	return g_strdup (priv->title);
}

static gboolean
add_in_idle (GTask *task)
{
	GtkSourceCompletionWords *words = g_task_get_source_object (task);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);
	Populate *p = g_task_get_task_data (task);
	guint idx = 0;
	gboolean finished;

	if (g_task_return_error_if_cancelled (task))
	{
		g_clear_object (&task);
		return G_SOURCE_REMOVE;
	}

	if (p->populate_iter == NULL)
	{
		p->populate_iter =
			gtk_source_completion_words_library_find_first (priv->library,
			                                                p->word,
			                                                p->word_len);
	}

	while (idx < priv->proposals_batch_size && p->populate_iter)
	{
		GtkSourceCompletionWordsProposal *proposal =
				gtk_source_completion_words_library_get_proposal (p->populate_iter);

		/* Only add non-exact matches */
		if (strcmp (gtk_source_completion_words_proposal_get_word (proposal), p->word) != 0)
		{
			g_list_store_append (p->ret, proposal);
		}

		g_clear_object (&proposal);

		p->populate_iter = gtk_source_completion_words_library_find_next (p->populate_iter,
		                                                                  p->word,
		                                                                  p->word_len);
		++idx;
	}

	finished = p->populate_iter == NULL;

	if (finished)
	{
		g_task_return_pointer (task, g_steal_pointer (&p->ret), g_object_unref);
		gtk_source_completion_words_library_unlock (priv->library);
		g_clear_object (&task);
		return G_SOURCE_REMOVE;
	}

	return G_SOURCE_CONTINUE;
}

static void
gtk_source_completion_words_populate_async (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionContext  *context,
                                            GCancellable                *cancellable,
                                            GAsyncReadyCallback          callback,
                                            gpointer                     user_data)
{
	GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (provider);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);
	GtkSourceCompletionActivation activation;
	Populate *populate;
	GtkTextIter begin, end;
	gchar *word;
	GTask *task = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_WORDS (words));
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	task = g_task_new (words, cancellable, callback, user_data);
	g_task_set_source_tag (task, gtk_source_completion_words_populate_async);
	g_task_set_priority (task, priv->priority);

	if (!gtk_source_completion_context_get_bounds (context, &begin, &end))
	{
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_NOT_SUPPORTED,
		                         "No word to complete");
		g_clear_object (&task);
		return;
	}

	g_clear_pointer (&priv->word, g_free);

	word = gtk_text_iter_get_slice (&begin, &end);

	g_assert (word != NULL);
	g_assert (word[0] != 0);

	activation = gtk_source_completion_context_get_activation (context);

	if (activation == GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE &&
	     g_utf8_strlen (word, -1) < (glong)priv->minimum_word_size)
	{
		g_free (word);
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_NOT_SUPPORTED,
		                         "Word is too short to complete");
		return;
	}

	populate = g_slice_new0 (Populate);
	populate->context = g_object_ref (context);
	populate->activation = activation;
	populate->word = g_steal_pointer (&word);
	populate->word_len = g_utf8_strlen (word, -1);
	populate->ret = g_list_store_new (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL);

	g_task_set_task_data (task, populate, (GDestroyNotify)populate_free);

	/* Do first right now */
	if (add_in_idle (task))
	{
		gtk_source_completion_words_library_lock (priv->library);
		priv->idle_id = g_idle_add ((GSourceFunc)add_in_idle, task);
	}
}

static GListModel *
gtk_source_completion_words_populate_finish (GtkSourceCompletionProvider  *provider,
                                             GAsyncResult                 *result,
                                             GError                      **error)
{
	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
gtk_source_completion_words_dispose (GObject *object)
{
	GtkSourceCompletionWords *provider = GTK_SOURCE_COMPLETION_WORDS (object);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (provider);

	while (priv->buffers != NULL)
	{
		BufferBinding *binding = priv->buffers->data;
		GtkTextBuffer *buffer = gtk_source_completion_words_buffer_get_buffer (binding->buffer);

		gtk_source_completion_words_unregister (provider, buffer);
	}

	g_clear_pointer (&priv->title, g_free);
	g_clear_object (&priv->library);

	G_OBJECT_CLASS (gtk_source_completion_words_parent_class)->dispose (object);
}

static void
update_buffers_batch_size (GtkSourceCompletionWords *words)
{
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);
	GList *item;

	for (item = priv->buffers; item != NULL; item = g_list_next (item))
	{
		BufferBinding *binding = item->data;
		gtk_source_completion_words_buffer_set_scan_batch_size (binding->buffer,
		                                                        priv->scan_batch_size);
	}
}

static void
update_buffers_minimum_word_size (GtkSourceCompletionWords *words)
{
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);
	GList *item;

	for (item = priv->buffers; item != NULL; item = g_list_next (item))
	{
		BufferBinding *binding = (BufferBinding *)item->data;
		gtk_source_completion_words_buffer_set_minimum_word_size (binding->buffer,
		                                                          priv->minimum_word_size);
	}
}

static void
gtk_source_completion_words_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	GtkSourceCompletionWords *self = GTK_SOURCE_COMPLETION_WORDS (object);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (self);

	switch (prop_id)
	{
		case PROP_TITLE:
			g_free (priv->title);
			priv->title = g_value_dup_string (value);

			if (priv->title == NULL)
			{
				priv->title = g_strdup (_("Document Words"));
			}
			break;

		case PROP_PROPOSALS_BATCH_SIZE:
			priv->proposals_batch_size = g_value_get_uint (value);
			break;

		case PROP_SCAN_BATCH_SIZE:
			priv->scan_batch_size = g_value_get_uint (value);
			update_buffers_batch_size (self);
			break;

		case PROP_MINIMUM_WORD_SIZE:
			priv->minimum_word_size = g_value_get_uint (value);
			update_buffers_minimum_word_size (self);
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
gtk_source_completion_words_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	GtkSourceCompletionWords *self = GTK_SOURCE_COMPLETION_WORDS (object);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (self);

	switch (prop_id)
	{
		case PROP_TITLE:
			g_value_set_string (value, priv->title);
			break;

		case PROP_PROPOSALS_BATCH_SIZE:
			g_value_set_uint (value, priv->proposals_batch_size);
			break;

		case PROP_SCAN_BATCH_SIZE:
			g_value_set_uint (value, priv->scan_batch_size);
			break;

		case PROP_MINIMUM_WORD_SIZE:
			g_value_set_uint (value, priv->minimum_word_size);
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
gtk_source_completion_words_class_init (GtkSourceCompletionWordsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_completion_words_get_property;
	object_class->set_property = gtk_source_completion_words_set_property;
	object_class->dispose = gtk_source_completion_words_dispose;

	properties[PROP_TITLE] =
		g_param_spec_string ("title",
				     "Title",
				     "The provider title",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PROPOSALS_BATCH_SIZE] =
		g_param_spec_uint ("proposals-batch-size",
				   "Proposals Batch Size",
				   "Number of proposals added in one batch",
				   1,
				   G_MAXUINT,
				   300,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_SCAN_BATCH_SIZE] =
		g_param_spec_uint ("scan-batch-size",
				   "Scan Batch Size",
				   "Number of lines scanned in one batch",
				   1,
				   G_MAXUINT,
				   50,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_MINIMUM_WORD_SIZE] =
		g_param_spec_uint ("minimum-word-size",
				   "Minimum Word Size",
				   "The minimum word size to complete",
				   2,
				   G_MAXUINT,
				   2,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PRIORITY] =
		g_param_spec_int ("priority",
				  "Priority",
				  "Provider priority",
				  G_MININT,
				  G_MAXINT,
				  0,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static gint
gtk_source_completion_words_get_priority (GtkSourceCompletionProvider *provider,
                                          GtkSourceCompletionContext  *context)
{
	GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (provider);
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);

	return priv->priority;
}

static void
gtk_source_completion_words_iface_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->get_title = gtk_source_completion_words_get_title;
	iface->populate_async = gtk_source_completion_words_populate_async;
	iface->populate_finish = gtk_source_completion_words_populate_finish;
	iface->get_priority = gtk_source_completion_words_get_priority;
}

static void
gtk_source_completion_words_init (GtkSourceCompletionWords *self)
{
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (self);

	priv->library = gtk_source_completion_words_library_new ();
}

/**
 * gtk_source_completion_words_new:
 * @title: (nullable): The title for the provider, or %NULL.
 *
 * Returns: a new #GtkSourceCompletionWords provider
 */
GtkSourceCompletionWords *
gtk_source_completion_words_new (const gchar *title)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_WORDS,
	                     "title", title,
	                     NULL);
}

static void
buffer_destroyed (BufferBinding *binding)
{
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (binding->words);

	priv->buffers = g_list_remove (priv->buffers, binding);
	g_object_unref (binding->buffer);
	g_slice_free (BufferBinding, binding);
}

/**
 * gtk_source_completion_words_register:
 * @words: a #GtkSourceCompletionWords
 * @buffer: a #GtkTextBuffer
 *
 * Registers @buffer in the @words provider.
 */
void
gtk_source_completion_words_register (GtkSourceCompletionWords *words,
                                      GtkTextBuffer            *buffer)
{
	GtkSourceCompletionWordsPrivate *priv = gtk_source_completion_words_get_instance_private (words);
	GtkSourceCompletionWordsBuffer *buf;
	BufferBinding *binding;

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS (words));
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

	binding = g_object_get_data (G_OBJECT (buffer), BUFFER_KEY);

	if (binding != NULL)
	{
		return;
	}

	buf = gtk_source_completion_words_buffer_new (priv->library,
	                                              buffer);

	gtk_source_completion_words_buffer_set_scan_batch_size (buf,
	                                                        priv->scan_batch_size);

	gtk_source_completion_words_buffer_set_minimum_word_size (buf,
	                                                          priv->minimum_word_size);

	binding = g_slice_new (BufferBinding);
	binding->words = words;
	binding->buffer = buf;

	g_object_set_data_full (G_OBJECT (buffer),
	                        BUFFER_KEY,
	                        binding,
	                        (GDestroyNotify)buffer_destroyed);

	priv->buffers = g_list_prepend (priv->buffers, binding);
}

/**
 * gtk_source_completion_words_unregister:
 * @words: a #GtkSourceCompletionWords
 * @buffer: a #GtkTextBuffer
 *
 * Unregisters @buffer from the @words provider.
 */
void
gtk_source_completion_words_unregister (GtkSourceCompletionWords *words,
                                        GtkTextBuffer            *buffer)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS (words));
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

	g_object_set_data (G_OBJECT (buffer), BUFFER_KEY, NULL);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcecompletionproviderwords.c
 * This file is part of gtksourceview
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * gtksourceview is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gtksourceview; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "gtksourcecompletionwords.h"
#include "gtksourcecompletionwordslibrary.h"
#include "gtksourcecompletionwordsbuffer.h"

#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourceview-i18n.h>
#include <string.h>

#define GTK_SOURCE_COMPLETION_WORDS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_COMPLETION_WORDS, GtkSourceCompletionWordsPrivate))

#define BUFFER_KEY "GtkSourceCompletionWordsBufferKey"

enum
{
	PROP_0,
	
	PROP_NAME,
	PROP_ICON,
	PROP_PROPOSALS_BATCH_SIZE,
	PROP_SCAN_BATCH_SIZE,
	PROP_MINIMUM_WORD_SIZE
};

struct _GtkSourceCompletionWordsPrivate
{
	gchar *name;
	GdkPixbuf *icon;

	gchar *word;
	gint word_len;
	guint idle_id;
	
	GtkSourceCompletionContext *context;
	GSequenceIter *populate_iter;

	guint cancel_id;
	
	guint proposals_batch_size;
	guint scan_batch_size;
	guint minimum_word_size;
	
	GtkSourceCompletionWordsLibrary *library;
	GList *buffers;
};

typedef struct
{
	GObjectClass parent_class;
} GscProposalWordsClass;

typedef struct
{
	GtkSourceCompletionWords *words;
	GtkSourceCompletionWordsBuffer *buffer;
} BufferBinding;

static void gtk_source_completion_words_iface_init (GtkSourceCompletionProviderIface *iface);

GType gsc_proposal_words_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionWords,
			 gtk_source_completion_words,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gtk_source_completion_words_iface_init))

static const gchar * 
gtk_source_completion_words_get_name (GtkSourceCompletionProvider *self)
{
	return GTK_SOURCE_COMPLETION_WORDS (self)->priv->name;
}

static GdkPixbuf * 
gtk_source_completion_words_get_icon (GtkSourceCompletionProvider *self)
{
	return GTK_SOURCE_COMPLETION_WORDS (self)->priv->icon;
}

static gboolean
is_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && 
	       (g_unichar_isalnum (ch) || ch == g_utf8_get_char ("_"));
}

static void
population_finished (GtkSourceCompletionWords *words)
{
	if (words->priv->idle_id != 0)
	{
		g_source_remove (words->priv->idle_id);
		words->priv->idle_id = 0;
	}
	
	g_free (words->priv->word);
	words->priv->word = NULL;
	
	if (words->priv->context != NULL)
	{
		if (words->priv->cancel_id)
		{
			g_signal_handler_disconnect (words->priv->context, 
			                             words->priv->cancel_id);
			words->priv->cancel_id = 0;
		}
	
		g_object_unref (words->priv->context);
		words->priv->context = NULL;
	}
}

static gchar *
get_word_at_iter (GtkSourceCompletionWords *words,
                  GtkTextIter              *iter)
{
	GtkTextIter start = *iter;
	gint line = gtk_text_iter_get_line (iter);
	gboolean went_back = TRUE;
	
	if (!gtk_text_iter_backward_char (&start))
	{
		return NULL;
	}

	while (went_back &&
	       line == gtk_text_iter_get_line (&start) && 
	       is_word_char (gtk_text_iter_get_char (&start)))
	{
		went_back = gtk_text_iter_backward_char (&start);
	}
	
	if (went_back)
	{
		gtk_text_iter_forward_char (&start);
	}
	
	if (gtk_text_iter_equal (iter, &start))
	{
		return NULL;
	}

	return gtk_text_iter_get_text (&start, iter);
}

static gboolean
add_in_idle (GtkSourceCompletionWords *words)
{
	guint idx = 0;
	GList *ret = NULL;
	gboolean finished;
	
	/* Don't complete empty string (when word == NULL) */
	if (words->priv->word == NULL)
	{
		gtk_source_completion_context_add_proposals (words->priv->context,
	                                                     GTK_SOURCE_COMPLETION_PROVIDER (words),
	                                                     NULL,
	                                                     TRUE);
		population_finished (words);
		return FALSE;
	}
	
	if (words->priv->populate_iter == NULL)
	{
		words->priv->populate_iter = 
			gtk_source_completion_words_library_find_first (words->priv->library,
			                                                words->priv->word,
			                                                words->priv->word_len);
	}

	while (idx < words->priv->proposals_batch_size && 
	       words->priv->populate_iter)
	{
		GtkSourceCompletionWordsProposal *proposal = 
				gtk_source_completion_words_library_get_proposal (words->priv->populate_iter);
		
		/* Only add non-exact matches */
		if (strcmp (gtk_source_completion_words_proposal_get_word (proposal),
		            words->priv->word) != 0)
		{
			ret = g_list_prepend (ret, proposal);
		}

		words->priv->populate_iter = 
				gtk_source_completion_words_library_find_next (words->priv->populate_iter,
		                                                               words->priv->word,
		                                                               words->priv->word_len);
		++idx;
	}
	
	ret = g_list_reverse (ret);
	finished = words->priv->populate_iter == NULL;
	
	gtk_source_completion_context_add_proposals (words->priv->context,
	                                             GTK_SOURCE_COMPLETION_PROVIDER (words),
	                                             ret,
	                                             finished);
	
	if (finished)
	{
		gtk_source_completion_words_library_unlock (words->priv->library);
		population_finished (words);
	}

	return !finished;
}

static gboolean
gtk_source_completion_words_match (GtkSourceCompletionProvider *provider,
                                   GtkSourceCompletionContext  *context)
{
	GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (provider);
	GtkTextIter iter;
	gchar *word;
	gboolean ret;

	gtk_source_completion_context_get_iter (context, &iter);
	word = get_word_at_iter (words, &iter);
	
	ret = g_utf8_strlen (word, -1) >= words->priv->minimum_word_size;
	
	g_free (word);
	
	return ret;
}

static void
gtk_source_completion_words_populate (GtkSourceCompletionProvider *provider,
                                      GtkSourceCompletionContext  *context)
{
	GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (provider);
	GtkTextIter iter;
	gchar *word;

	gtk_source_completion_context_get_iter (context, &iter);

	g_free (words->priv->word);
	word = get_word_at_iter (words, &iter);
	
	if (word == NULL)
	{
		gtk_source_completion_context_add_proposals (context,
		                                             provider,
		                                             NULL,
		                                             TRUE);
		return;
	}

	words->priv->cancel_id = 
		g_signal_connect_swapped (context, 
			                  "cancelled", 
			                   G_CALLBACK (population_finished), 
			                   provider);

	words->priv->context = g_object_ref (context);

	words->priv->word = word;
	words->priv->word_len = strlen (word);
	
	/* Do first right now */
	if (add_in_idle (words))
	{
		gtk_source_completion_words_library_lock (words->priv->library);
		words->priv->idle_id = g_idle_add ((GSourceFunc)add_in_idle,
		                                   words);
	}	
}

static void
remove_buffer (BufferBinding *binding)
{
	g_object_set_data (G_OBJECT (gtk_source_completion_words_buffer_get_buffer (binding->buffer)),
	                   BUFFER_KEY,
	                   NULL);
}

static void
gtk_source_completion_words_dispose (GObject *object)
{
	GtkSourceCompletionWords *provider = GTK_SOURCE_COMPLETION_WORDS (object);
	GList *cp;
	
	population_finished (provider);
	
	cp = g_list_copy (provider->priv->buffers);
	g_list_foreach (cp, (GFunc)remove_buffer, NULL);
	
	g_list_free (cp);
	g_list_free (provider->priv->buffers);
	
	g_free (provider->priv->name);
	provider->priv->name = NULL;
	
	if (provider->priv->icon)
	{
		g_object_unref (provider->priv->icon);
		provider->priv->icon = NULL;
	}
	
	if (provider->priv->library)
	{
		g_object_unref (provider->priv->library);
		provider->priv->library = NULL;
	}
	
	G_OBJECT_CLASS (gtk_source_completion_words_parent_class)->dispose (object);
}

static void
update_buffers_batch_size (GtkSourceCompletionWords *words)
{
	GList *item;
	
	for (item = words->priv->buffers; item; item = g_list_next (item))
	{
		BufferBinding *binding = (BufferBinding *)item->data;
		gtk_source_completion_words_buffer_set_scan_batch_size (binding->buffer,
		                                                        words->priv->scan_batch_size);
	}
}

static void
update_buffers_minimum_word_size (GtkSourceCompletionWords *words)
{
	GList *item;
	
	for (item = words->priv->buffers; item; item = g_list_next (item))
	{
		BufferBinding *binding = (BufferBinding *)item->data;
		gtk_source_completion_words_buffer_set_minimum_word_size (binding->buffer,
		                                                          words->priv->minimum_word_size);
	}
}

static void
gtk_source_completion_words_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	GtkSourceCompletionWords *self = GTK_SOURCE_COMPLETION_WORDS (object);
	
	switch (prop_id)
	{
		case PROP_NAME:
			g_free (self->priv->name);
			self->priv->name = g_value_dup_string (value);
			
			if (self->priv->name == NULL)
			{
				self->priv->name = g_strdup (_("Document Words"));
			}
		break;
		case PROP_ICON:
			if (self->priv->icon)
			{
				g_object_unref (self->priv->icon);
			}
			
			self->priv->icon = g_value_dup_object (value);
		break;
		case PROP_PROPOSALS_BATCH_SIZE:
			self->priv->proposals_batch_size = g_value_get_uint (value);
		break;
		case PROP_SCAN_BATCH_SIZE:
			self->priv->scan_batch_size = g_value_get_uint (value);
			update_buffers_batch_size (self);
		break;
		case PROP_MINIMUM_WORD_SIZE:
			self->priv->minimum_word_size = g_value_get_uint (value);
			update_buffers_minimum_word_size (self);
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
	
	switch (prop_id)
	{
		case PROP_NAME:
			g_value_set_string (value, self->priv->name);
		break;
		case PROP_ICON:
			g_value_set_object (value, self->priv->icon);
		break;
		case PROP_PROPOSALS_BATCH_SIZE:
			g_value_set_uint (value, self->priv->proposals_batch_size);
		break;
		case PROP_SCAN_BATCH_SIZE:
			g_value_set_uint (value, self->priv->scan_batch_size);
		break;
		case PROP_MINIMUM_WORD_SIZE:
			g_value_set_uint (value, self->priv->minimum_word_size);
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

	object_class->dispose = gtk_source_completion_words_dispose;

	object_class->set_property = gtk_source_completion_words_set_property;
	object_class->get_property = gtk_source_completion_words_get_property;
	
	g_object_class_install_property (object_class,
	                                 PROP_NAME,
	                                 g_param_spec_string ("name",
	                                                      _("Name"),
	                                                      _("The provider name"),
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	g_object_class_install_property (object_class,
	                                 PROP_ICON,
	                                 g_param_spec_object ("icon",
	                                                      _("Icon"),
	                                                      _("The provider icon"),
	                                                      GDK_TYPE_PIXBUF,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_PROPOSALS_BATCH_SIZE,
	                                 g_param_spec_uint ("proposals-batch-size",
	                                                    _("Proposals Batch Size"),
	                                                    _("Number of proposals added in one batch"),
	                                                    1,
	                                                    G_MAXUINT,
	                                                    300,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_SCAN_BATCH_SIZE,
	                                 g_param_spec_uint ("scan-batch-size",
	                                                    _("Scan Batch Size"),
	                                                    _("Number of lines scanned in one batch"),
	                                                    1,
	                                                    G_MAXUINT,
	                                                    20,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	g_object_class_install_property (object_class,
	                                 PROP_MINIMUM_WORD_SIZE,
	                                 g_param_spec_uint ("minimum-word-size",
	                                                    _("Minimum Word Size"),
	                                                    _("The minimum word size to complete"),
	                                                    2,
	                                                    G_MAXUINT,
	                                                    3,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionWordsPrivate));
}

static gboolean
gtk_source_completion_words_get_start_iter (GtkSourceCompletionProvider *provider,
                                            GtkSourceCompletionProposal *proposal,
                                            GtkTextIter                 *iter)
{
	//GtkSourceCompletionWords *words = GTK_SOURCE_COMPLETION_WORDS (provider);

	/* TODO: implement */
	return FALSE;
}

static void
gtk_source_completion_words_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gtk_source_completion_words_get_name;
	iface->get_icon = gtk_source_completion_words_get_icon;

	iface->populate = gtk_source_completion_words_populate;
	iface->match = gtk_source_completion_words_match;

	iface->get_start_iter = gtk_source_completion_words_get_start_iter;
}

static void 
gtk_source_completion_words_init (GtkSourceCompletionWords *self)
{	
	self->priv = GTK_SOURCE_COMPLETION_WORDS_GET_PRIVATE (self);
	
	self->priv->library = gtk_source_completion_words_library_new ();
}

GtkSourceCompletionWords *
gtk_source_completion_words_new (gchar const *name,
                                 GdkPixbuf   *icon)
{
	return g_object_new (GTK_TYPE_SOURCE_COMPLETION_WORDS, 
	                     "name", name, 
	                     "icon", icon, 
	                     NULL);
}

static void
buffer_destroyed (BufferBinding *binding)
{
	binding->words->priv->buffers = g_list_remove (binding->words->priv->buffers, 
	                                               binding);
	g_object_unref (binding->buffer);
	g_slice_free (BufferBinding, binding);
}

void
gtk_source_completion_words_register (GtkSourceCompletionWords *words,
                                      GtkTextBuffer            *buffer)
{
	GtkSourceCompletionWordsBuffer *buf;
	BufferBinding *binding;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_WORDS (words));
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
	
	binding = g_object_get_data (G_OBJECT (buffer), BUFFER_KEY);
	
	if (binding != NULL)
	{
		return;
	}
	
	buf = gtk_source_completion_words_buffer_new (words->priv->library,
	                                              buffer);
	
	gtk_source_completion_words_buffer_set_scan_batch_size (buf,
	                                                        words->priv->scan_batch_size);

	gtk_source_completion_words_buffer_set_minimum_word_size (buf,
	                                                          words->priv->minimum_word_size);
	
	binding = g_slice_new (BufferBinding);
	binding->words = words;
	binding->buffer = buf;
	
	g_object_set_data_full (G_OBJECT (buffer), 
	                        BUFFER_KEY, 
	                        binding,
	                        (GDestroyNotify)buffer_destroyed);

	words->priv->buffers = g_list_prepend (words->priv->buffers, 
	                                       binding);
}

void
gtk_source_completion_words_unregister (GtkSourceCompletionWords *words,
                                        GtkTextBuffer            *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_WORDS (words));
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
	
	g_object_set_data (G_OBJECT (buffer), BUFFER_KEY, NULL);
}


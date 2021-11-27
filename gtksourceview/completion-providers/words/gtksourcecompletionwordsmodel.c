/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtksourcecompletionwordsmodel-private.h"

struct _GtkSourceCompletionWordsModel
{
	GObject                          parent_instance;
	GPtrArray                       *items;
	GtkSourceCompletionWordsLibrary *library;
	GCancellable                    *cancellable;
	GSequenceIter                   *populate_iter;
	char                            *prefix;
	gsize                            prefix_len;
	guint                            proposals_batch_size;
	guint                            minimum_word_size;
	guint                            idle_id;
};

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionWordsModel, gtk_source_completion_words_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static void
gtk_source_completion_words_model_finalize (GObject *object)
{
	GtkSourceCompletionWordsModel *self = (GtkSourceCompletionWordsModel *)object;

	g_clear_pointer (&self->items, g_ptr_array_unref);
	g_clear_pointer (&self->prefix, g_free);
	g_clear_object (&self->library);
	g_clear_object (&self->cancellable);

	g_assert (self->idle_id == 0);

	G_OBJECT_CLASS (gtk_source_completion_words_model_parent_class)->finalize (object);
}

static void
gtk_source_completion_words_model_dispose (GObject *object)
{
	GtkSourceCompletionWordsModel *self = (GtkSourceCompletionWordsModel *)object;

	if (self->idle_id != 0)
	{
		g_clear_handle_id (&self->idle_id, g_source_remove);
		gtk_source_completion_words_library_unlock (self->library);
	}

	G_OBJECT_CLASS (gtk_source_completion_words_model_parent_class)->dispose (object);
}

static void
gtk_source_completion_words_model_class_init (GtkSourceCompletionWordsModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_completion_words_model_dispose;
	object_class->finalize = gtk_source_completion_words_model_finalize;
}

static void
gtk_source_completion_words_model_init (GtkSourceCompletionWordsModel *self)
{
	self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

static GType
gtk_source_completion_words_model_get_item_type (GListModel *model)
{
	return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gtk_source_completion_words_model_get_n_items (GListModel *model)
{
	GtkSourceCompletionWordsModel *self = (GtkSourceCompletionWordsModel *)model;

	g_assert (GTK_SOURCE_IS_COMPLETION_WORDS_MODEL (self));

	return self->items->len;
}

static gpointer
gtk_source_completion_words_model_get_item (GListModel *model,
					    guint       position)
{
	GtkSourceCompletionWordsModel *self = (GtkSourceCompletionWordsModel *)model;

	g_assert (GTK_SOURCE_IS_COMPLETION_WORDS_MODEL (self));

	if (position < self->items->len)
		return g_object_ref (g_ptr_array_index (self->items, position));

	return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_n_items = gtk_source_completion_words_model_get_n_items;
	iface->get_item = gtk_source_completion_words_model_get_item;
	iface->get_item_type = gtk_source_completion_words_model_get_item_type;
}

static gboolean
add_in_idle (GtkSourceCompletionWordsModel *self)
{
	guint idx = 0;
	guint old_len;

	old_len = self->items->len;

	if (g_cancellable_is_cancelled (self->cancellable))
	{
		goto cleanup;
	}

	if (self->populate_iter == NULL)
	{
		self->populate_iter = gtk_source_completion_words_library_find_first (self->library,
		                                                                      self->prefix,
		                                                                      self->prefix_len);
	}

	while (idx < self->proposals_batch_size && self->populate_iter)
	{
		GtkSourceCompletionWordsProposal *proposal;

		proposal = gtk_source_completion_words_library_get_proposal (self->populate_iter);

		/* Only add non-exact matches */
		if (strcmp (gtk_source_completion_words_proposal_get_word (proposal), self->prefix) != 0)
		{
			g_ptr_array_add (self->items, g_object_ref (proposal));
		}

		self->populate_iter = gtk_source_completion_words_library_find_next (self->populate_iter,
		                                                                     self->prefix,
		                                                                     self->prefix_len);
		++idx;
	}

	if (old_len < self->items->len)
	{
		g_list_model_items_changed (G_LIST_MODEL (self),
		                            old_len,
		                            0,
		                            self->items->len - old_len);
	}

	if (self->populate_iter != NULL)
	{
		return G_SOURCE_CONTINUE;
	}

cleanup:
	gtk_source_completion_words_library_unlock (self->library);
	self->idle_id = 0;

	return G_SOURCE_REMOVE;
}

static void
gtk_source_completion_words_model_populate (GtkSourceCompletionWordsModel *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_WORDS_MODEL (self));
	g_assert (self->prefix != NULL);
	g_assert (self->items != NULL);
	g_assert (self->minimum_word_size >= 2);
	g_assert (self->proposals_batch_size >= 1 || self->proposals_batch_size <= 300);
	g_assert (self->cancellable == NULL);

	/* Short-circuit if the word is too short. We won't allow replaying
	 * these until we reach a large enough word prefix.
	 */
	if (strlen (self->prefix) < self->minimum_word_size)
	{
		return;
	}

	gtk_source_completion_words_library_lock (self->library);

	/* Do first scan immediately */
	if (add_in_idle (self))
	{
		self->idle_id = g_idle_add ((GSourceFunc)add_in_idle, self);
	}
}

GListModel *
gtk_source_completion_words_model_new (GtkSourceCompletionWordsLibrary *library,
                                       guint                            proposals_batch_size,
                                       guint                            minimum_word_size,
                                       const char                      *prefix)
{
	GtkSourceCompletionWordsModel *self;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_LIBRARY (library), NULL);
	g_return_val_if_fail (proposals_batch_size >= 1 || proposals_batch_size <= 300, NULL);
	g_return_val_if_fail (minimum_word_size >= 2, NULL);

	if (prefix == NULL)
		prefix = "";

	self = g_object_new (GTK_SOURCE_TYPE_COMPLETION_WORDS_MODEL, NULL);
	self->library = g_object_ref (library);
	self->proposals_batch_size = proposals_batch_size;
	self->minimum_word_size = minimum_word_size;
	self->prefix = g_strdup (prefix);
	self->prefix_len = strlen (prefix);

	gtk_source_completion_words_model_populate (self);

	return G_LIST_MODEL (self);
}

gboolean
gtk_source_completion_words_model_can_filter (GtkSourceCompletionWordsModel *self,
                                              const char                    *word)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_MODEL (self), FALSE);

	/* If we have an empty word, try to reuse this until we get a real word */
	if (word == NULL || word[0] == 0)
	{
		return self->prefix[0] == 0;
	}

	/* If the word was too short, nothing we can do as we ignored the
	 * populate request.
	 */
	if (strlen (self->prefix) < self->minimum_word_size)
	{
		return FALSE;
	}

	/* If the new word starts with our initial word, then we can simply
	 * refilter outside this model using a GtkFilterListModel.
	 */
	return g_str_has_prefix (word, self->prefix) || g_str_equal (word, self->prefix);
}

/**
 * gtk_source_completion_words_model_cancel:
 * @self: a #GtkSourceCompletionWordsModel
 *
 * Cancels any in-flight population by cancelling the task.
 */
void
gtk_source_completion_words_model_cancel (GtkSourceCompletionWordsModel *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_MODEL (self));

	g_cancellable_cancel (self->cancellable);
}

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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */



#include "config.h"

#include <string.h>

#include "gtksourcebuffer.h"
#include "gtksourcecompletion.h"
#include "gtksourcecompletioncontext-private.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"

/**
 * GtkSourceCompletionContext:
 *
 * The context of a completion.
 *
 * `GtkSourceCompletionContext` contains information about an attept to display
 * completion proposals to the user based on typed text in the [class@View].
 *
 * When typing, [class@Completion] may use registered
 * [iface@CompletionProvider] to determine if there may be results which
 * could be displayed. If so, a `GtkSourceCompletionContext` is created with
 * information that is provided to the [iface@CompletionProvider] to populate
 * results which might be useful to the user.
 *
 * [iface@CompletionProvider] are expected to provide [iface@Gio.ListModel] with
 * [iface@CompletionProposal] which may be joined together in a list of
 * results for the user. They are also responsible for how the contents are
 * displayed using [class@CompletionCell] which allows for some level of
 * customization.
 */

struct _GtkSourceCompletionContext
{
	GObject parent_instance;

	GtkSourceCompletion *completion;

	GArray *providers;

	GtkTextMark *begin_mark;
	GtkTextMark *end_mark;

	GtkSourceCompletionActivation activation;

	guint busy : 1;
	guint has_populated : 1;
	guint empty : 1;
};

typedef struct
{
	GtkSourceCompletionProvider *provider;
	GListModel                  *results;
	GError                      *error;
	gulong                       items_changed_handler;
} ProviderInfo;

typedef struct
{
	guint n_active;
} CompleteTaskData;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionContext, gtk_source_completion_context, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

enum {
	PROP_0,
	PROP_BUSY,
	PROP_COMPLETION,
	PROP_EMPTY,
	N_PROPS
};

enum {
	PROVIDER_MODEL_CHANGED,
	N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
clear_provider_info (gpointer data)
{
	ProviderInfo *info = data;

	if (info->items_changed_handler != 0)
	{
		g_signal_handler_disconnect (info->results, info->items_changed_handler);
		info->items_changed_handler = 0;
	}

	g_clear_object (&info->provider);
	g_clear_object (&info->results);
	g_clear_error (&info->error);
}

static gint
compare_provider_info (gconstpointer a,
                       gconstpointer b,
                       gpointer      user_data)
{
	GtkSourceCompletionContext *self = user_data;
	const ProviderInfo *info_a = a;
	const ProviderInfo *info_b = b;
	int priority_a = gtk_source_completion_provider_get_priority (info_a->provider, self);
	int priority_b = gtk_source_completion_provider_get_priority (info_b->provider, self);

	if (priority_a > priority_b)
		return -1;
	else if (priority_a < priority_b)
		return 1;
	else
		return 0;
}

static void
complete_task_data_free (gpointer data)
{
  CompleteTaskData *task_data = data;

  g_slice_free (CompleteTaskData, task_data);
}

static void
gtk_source_completion_context_update_empty (GtkSourceCompletionContext *self)
{
	gboolean empty = TRUE;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->results != NULL && g_list_model_get_n_items (info->results) > 0)
		{
			empty = FALSE;
			break;
		}
	}

	if (self->empty != empty)
	{
		self->empty = empty;
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_EMPTY]);
	}
}

static void
gtk_source_completion_context_mark_failed (GtkSourceCompletionContext  *self,
                                           GtkSourceCompletionProvider *provider,
                                           const GError                *error)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_assert (error != NULL);

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
	    g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED))
		return;

	for (guint i = 0; i < self->providers->len; i++)
	{
		ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->provider == provider)
		{
			if (error != info->error)
			{
				g_clear_error (&info->error);
				info->error = g_error_copy (error);
			}

			break;
		}
	}
}

static void
gtk_source_completion_context_dispose (GObject *object)
{
	GtkSourceCompletionContext *self = (GtkSourceCompletionContext *)object;

	g_clear_pointer (&self->providers, g_array_unref);
	g_clear_object (&self->completion);

	if (self->begin_mark != NULL)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (self->begin_mark), self->begin_mark);
		g_clear_object (&self->begin_mark);
	}

	if (self->end_mark != NULL)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (self->end_mark), self->end_mark);
		g_clear_object (&self->end_mark);
	}

	G_OBJECT_CLASS (gtk_source_completion_context_parent_class)->dispose (object);
}

static void
gtk_source_completion_context_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
	GtkSourceCompletionContext *self = GTK_SOURCE_COMPLETION_CONTEXT (object);

	switch (prop_id)
	{
	case PROP_BUSY:
		g_value_set_boolean (value, gtk_source_completion_context_get_busy (self));
		break;

	case PROP_COMPLETION:
		g_value_set_object (value, gtk_source_completion_context_get_completion (self));
		break;

	case PROP_EMPTY:
		g_value_set_boolean (value, gtk_source_completion_context_get_empty (self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_context_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
	GtkSourceCompletionContext *self = GTK_SOURCE_COMPLETION_CONTEXT (object);

	switch (prop_id)
	{
	case PROP_COMPLETION:
		self->completion = g_value_dup_object (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_context_class_init (GtkSourceCompletionContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_completion_context_dispose;
	object_class->get_property = gtk_source_completion_context_get_property;
	object_class->set_property = gtk_source_completion_context_set_property;

	/**
	* GtkSourceCompletionContext:busy:
	*
	* The "busy" property is %TRUE while the completion context is
	* populating completion proposals.
	*/
	properties [PROP_BUSY] =
		g_param_spec_boolean ("busy",
		                      "Busy",
		                      "Is the completion context busy populating",
		                      FALSE,
		                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	* GtkSourceCompletionContext:empty:
	*
	* The "empty" property is %TRUE when there are no results.
	*
	* It will be notified when the first result is added or the last
	* result is removed.
	*/
	properties [PROP_EMPTY] =
		g_param_spec_boolean ("empty",
		                      "Empty",
		                      "If the context has no results",
		                      TRUE,
		                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	* GtkSourceCompletionContext:completion:
	*
	* The "completion" is the #GtkSourceCompletion that was used to create the context.
	*/
	properties [PROP_COMPLETION] =
		g_param_spec_object ("completion",
		                     "Completion",
		                     "Completion",
		                     GTK_SOURCE_TYPE_COMPLETION,
		                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceCompletionContext::provider-model-changed:
	 * @self: a #GtkSourceCompletionContext
	 * @provider: a #GtkSourceCompletionProvider
	 * @model: (nullable): a #GListModel
	 *
	 * Emitted when a provider changes a model.
	 *
	 * This signal is primarily useful for #GtkSourceCompletionProvider's
	 * that want to track other providers in context. For example, it can
	 * be used to create a "top results" provider.
	 *
	 * Since: 5.6
	 */
	signals [PROVIDER_MODEL_CHANGED] =
		g_signal_new ("provider-model-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2,
		              GTK_SOURCE_TYPE_COMPLETION_PROVIDER,
		              G_TYPE_LIST_MODEL);
}

static void
gtk_source_completion_context_init (GtkSourceCompletionContext *self)
{
	self->empty = TRUE;

	self->providers = g_array_new (FALSE, FALSE, sizeof (ProviderInfo));
	g_array_set_clear_func (self->providers, clear_provider_info);
}

static GType
gtk_source_completion_context_get_item_type (GListModel *model)
{
	return GTK_SOURCE_TYPE_COMPLETION_PROPOSAL;
}

static guint
gtk_source_completion_context_get_n_items (GListModel *model)
{
	GtkSourceCompletionContext *self = (GtkSourceCompletionContext *)model;
	guint count = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->results != NULL)
		{
			count += g_list_model_get_n_items (info->results);
		}
	}

	return count;
}

gboolean
_gtk_source_completion_context_get_item_full (GtkSourceCompletionContext   *self,
                                              guint                         position,
                                              GtkSourceCompletionProvider **provider,
                                              GtkSourceCompletionProposal **proposal)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);
	g_return_val_if_fail (position < G_MAXUINT, FALSE);

	if (provider != NULL)
		*provider = NULL;

	if (proposal != NULL)
		*proposal = NULL;

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);
		guint n_items;

		if (info->results == NULL)
			continue;

		n_items = g_list_model_get_n_items (info->results);

		if (n_items == 0)
			continue;

		if (position >= n_items)
		{
			position -= n_items;
			continue;
		}

		if (provider != NULL)
		{
			*provider = g_object_ref (info->provider);
		}

		if (proposal != NULL)
		{
			*proposal = g_list_model_get_item (info->results, position);
		}

		return TRUE;
	}

	return FALSE;
}

static gpointer
gtk_source_completion_context_get_item (GListModel *model,
                                        guint       position)
{
	GtkSourceCompletionContext *self = (GtkSourceCompletionContext *)model;
	GtkSourceCompletionProposal *proposal = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));

	_gtk_source_completion_context_get_item_full (self, position, NULL, &proposal);

	return g_steal_pointer (&proposal);
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_item_type = gtk_source_completion_context_get_item_type;
	iface->get_item = gtk_source_completion_context_get_item;
	iface->get_n_items = gtk_source_completion_context_get_n_items;
}

/**
 * gtk_source_completion_context_get_bounds:
 * @self: an #GtkSourceCompletionContext
 * @begin: (out) (optional): a #GtkTextIter
 * @end: (out) (optional): a #GtkTextIter
 *
 * Gets the bounds for the completion, which is the beginning of the
 * current word (taking break characters into account) to the current
 * insertion cursor.
 *
 * If @begin is non-%NULL, it will be set to the start position of the
 * current word being completed.
 *
 * If @end is non-%NULL, it will be set to the insertion cursor for the
 * current word being completed.
 *
 * Returns: %TRUE if the marks are still valid and @begin or @end was set.
 */
gboolean
gtk_source_completion_context_get_bounds (GtkSourceCompletionContext *self,
                                          GtkTextIter                *begin,
                                          GtkTextIter                *end)
{
	GtkSourceBuffer *buffer;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);
	g_return_val_if_fail (self->completion != NULL, FALSE);
	g_return_val_if_fail (begin != NULL || end != NULL, FALSE);

	buffer = gtk_source_completion_get_buffer (self->completion);

	g_return_val_if_fail (buffer != NULL, FALSE);

	if (begin != NULL)
		memset (begin, 0, sizeof *begin);

	if (end != NULL)
		memset (end, 0, sizeof *end);

	if (self->begin_mark == NULL)
	{
		/* Try to give some sort of valid iter */
		gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), begin, end);
		return FALSE;
	}

	g_assert (GTK_IS_TEXT_MARK (self->begin_mark));
	g_assert (GTK_IS_TEXT_MARK (self->end_mark));
	g_assert (GTK_IS_TEXT_BUFFER (buffer));

	if (begin != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), begin, self->begin_mark);
	}

	if (end != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), end, self->end_mark);
	}

	return TRUE;
}

/**
 * gtk_source_completion_context_get_completion:
 * @self: an #GtkSourceCompletionContext
 *
 * Gets the #GtkSourceCompletion that created the context.
 *
 * Returns: (transfer none) (nullable): an #GtkSourceCompletion or %NULL
 */
GtkSourceCompletion *
gtk_source_completion_context_get_completion (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	return self->completion;
}

GtkSourceCompletionContext *
_gtk_source_completion_context_new (GtkSourceCompletion *completion)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION (completion), NULL);

	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_CONTEXT,
	                     "completion", completion,
	                     NULL);
}

void
_gtk_source_completion_context_add_provider (GtkSourceCompletionContext  *self,
                                             GtkSourceCompletionProvider *provider)
{
	ProviderInfo info = {0};

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_return_if_fail (self->has_populated == FALSE);

	info.provider = g_object_ref (provider);
	info.results = NULL;

	g_array_append_val (self->providers, info);
	g_array_sort_with_data (self->providers, compare_provider_info, self);
}

void
_gtk_source_completion_context_remove_provider (GtkSourceCompletionContext  *self,
                                                GtkSourceCompletionProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_return_if_fail (self->has_populated == FALSE);

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->provider == provider)
		{
			g_array_remove_index (self->providers, i);
			return;
		}
	}

	g_warning ("No such provider <%s %p> in context",
	           G_OBJECT_TYPE_NAME (provider), provider);
}

static void
gtk_source_completion_context_items_changed_cb (GtkSourceCompletionContext *self,
                                                guint                       position,
                                                guint                       removed,
                                                guint                       added,
                                                GListModel                 *results)
{
	guint real_position = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_assert (G_IS_LIST_MODEL (results));

	if (removed == 0 && added == 0)
		return;

	for (guint i = 0; i < self->providers->len; i++)
	{
		ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->results == results)
		{
			g_list_model_items_changed (G_LIST_MODEL (self),
			                            real_position + position,
			                            removed,
			                            added);
			break;
		}

		if (info->results != NULL)
			real_position += g_list_model_get_n_items (info->results);
	}

	gtk_source_completion_context_update_empty (self);
}

/**
 * gtk_source_completion_context_get_proposals_for_provider:
 * @self: a #GtkSourceCompletionContext
 * @provider: a #GtkSourceCompletionProvider
 *
 * Gets the #GListModel associated with the provider.
 *
 * You can connect to #GtkSourceCompletionContext::model-changed to receive
 * notifications about when the model has been replaced by a new model.
 *
 * Returns: (transfer none) (nullable): a #GListModel or %NULL
 *
 * Since: 5.6
 */
GListModel *
gtk_source_completion_context_get_proposals_for_provider (GtkSourceCompletionContext  *self,
                                                          GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider), NULL);

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->provider == provider)
		{
			return info->results;
		}
	}

	return NULL;
}

/**
 * gtk_source_completion_context_set_proposals_for_provider:
 * @self: an #GtkSourceCompletionContext
 * @provider: an #GtkSourceCompletionProvider
 * @results: (nullable): a #GListModel or %NULL
 *
 * This function allows providers to update their results for a context
 * outside of a call to [method@CompletionProvider.populate_async].
 *
 * This can be used to immediately return results for a provider while it does
 * additional asynchronous work. Doing so will allow the completions to
 * update while the operation is in progress.
 */
void
gtk_source_completion_context_set_proposals_for_provider (GtkSourceCompletionContext  *self,
                                                          GtkSourceCompletionProvider *provider,
                                                          GListModel                  *results)
{
	guint position = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_assert (!results || G_IS_LIST_MODEL (results));

	for (guint i = 0; i < self->providers->len; i++)
	{
		ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->provider == provider)
		{
			guint n_removed = 0;
			guint n_added = 0;

			if (info->results == results)
				return;

			if (info->results != NULL)
				n_removed = g_list_model_get_n_items (info->results);

			if (results != NULL)
				n_added = g_list_model_get_n_items (results);

			if (info->items_changed_handler != 0)
			{
				g_signal_handler_disconnect (info->results, info->items_changed_handler);
				info->items_changed_handler = 0;
			}

			g_set_object (&info->results, results);

			if (info->results != NULL)
				info->items_changed_handler =
					g_signal_connect_object (info->results,
					                         "items-changed",
					                         G_CALLBACK (gtk_source_completion_context_items_changed_cb),
					                         self,
					                         G_CONNECT_SWAPPED);

			g_list_model_items_changed (G_LIST_MODEL (self), position, n_removed, n_added);

			g_signal_emit (self, signals [PROVIDER_MODEL_CHANGED], 0, provider, results);

			break;
		}

		if (info->results != NULL)
			position += g_list_model_get_n_items (info->results);
	}

	gtk_source_completion_context_update_empty (self);
}

static void
gtk_source_completion_context_populate_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
	GtkSourceCompletionProvider *provider = (GtkSourceCompletionProvider *)object;
	GtkSourceCompletionContext *self;
	CompleteTaskData *task_data;
	GListModel *results = NULL;
	GTask *task = user_data;
	GError *error = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_assert (G_IS_ASYNC_RESULT (result));
	g_assert (G_IS_TASK (task));

	self = g_task_get_source_object (task);
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));

	task_data = g_task_get_task_data (task);
	g_assert (task_data != NULL);

	if (!(results = gtk_source_completion_provider_populate_finish (provider, result, &error)))
	{
		gtk_source_completion_context_mark_failed (self, provider, error);
	}
	else
	{
		gtk_source_completion_context_set_proposals_for_provider (self, provider, results);
	}

	task_data->n_active--;

	gtk_source_completion_context_update_empty (self);

	if (task_data->n_active == 0)
	{
		g_task_return_boolean (task, TRUE);
	}

	g_clear_object (&results);
	g_clear_object (&task);
	g_clear_error (&error);
}

static void
gtk_source_completion_context_notify_complete_cb (GtkSourceCompletionContext *self,
                                                  GParamSpec                 *pspec,
                                                  GTask                      *task)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_assert (G_IS_TASK (task));

	self->busy = FALSE;
	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);
}

/**
 * _gtk_source_completion_context_complete_async:
 * @self: a #GtkSourceCompletionContext
 * @activation: how we are being activated
 * @iter: a #GtkTextIter
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: (nullable): a callback or %NULL
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that the completion context load proposals
 * from the registered providers.
 */
void
_gtk_source_completion_context_complete_async (GtkSourceCompletionContext    *self,
                                               GtkSourceCompletionActivation  activation,
                                               const GtkTextIter             *begin,
                                               const GtkTextIter             *end,
                                               GCancellable                  *cancellable,
                                               GAsyncReadyCallback            callback,
                                               gpointer                       user_data)
{
	GTask *task = NULL;
	CompleteTaskData *task_data;
	GtkSourceBuffer *buffer;
	guint n_items;

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_return_if_fail (self->has_populated == FALSE);
	g_return_if_fail (self->begin_mark == NULL);
	g_return_if_fail (self->end_mark == NULL);
	g_return_if_fail (begin != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	self->activation = activation;
	self->has_populated = TRUE;
	self->busy = TRUE;

	buffer = gtk_source_completion_context_get_buffer (self);

	self->begin_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, begin, TRUE);
	g_object_ref (self->begin_mark);

	self->end_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, end, FALSE);
	g_object_ref (self->end_mark);

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, _gtk_source_completion_context_complete_async);
	g_task_set_priority (task, G_PRIORITY_LOW);

	task_data = g_slice_new0 (CompleteTaskData);
	task_data->n_active = self->providers->len;
	g_task_set_task_data (task, task_data, complete_task_data_free);

	/* Always notify of busy completion, whether we fail or not */
	g_signal_connect_object (task,
	                         "notify::completed",
	                         G_CALLBACK (gtk_source_completion_context_notify_complete_cb),
	                         self,
	                         G_CONNECT_SWAPPED);

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		gtk_source_completion_provider_populate_async (info->provider,
		                                               self,
		                                               cancellable,
		                                               gtk_source_completion_context_populate_cb,
		                                               g_object_ref (task));
	}

	/* Providers may adjust their position based on our new marks */
	n_items = g_list_model_get_n_items (G_LIST_MODEL (self));
	g_array_sort_with_data (self->providers, compare_provider_info, self);
	g_list_model_items_changed (G_LIST_MODEL (self), 0, n_items, n_items);

	if (task_data->n_active == 0)
	{
		g_task_return_boolean (task, TRUE);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_BUSY]);

	g_clear_object (&task);
}

/**
 * _gtk_source_completion_context_complete_finish:
 * @self: an #GtkSourceCompletionContext
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous request to populate proposals.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set
 */
gboolean
_gtk_source_completion_context_complete_finish (GtkSourceCompletionContext  *self,
                                                GAsyncResult                *result,
                                                GError                     **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);
	g_return_val_if_fail (G_IS_TASK (result), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
	g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == _gtk_source_completion_context_complete_async, FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * gtk_source_completion_context_get_busy:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the "busy" property. This is set to %TRUE while the completion
 * context is actively fetching proposals from registered
 * #GtkSourceCompletionProvider's.
 *
 * Returns: %TRUE if the context is busy
 */
gboolean
gtk_source_completion_context_get_busy (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);

	return self->busy;
}

/**
 * gtk_source_completion_context_get_buffer:
 * @self: an #GtkSourceCompletionContext
 *
 * Gets the underlying buffer used by the context.
 *
 * This is a convenience function to get the buffer via the #GtkSourceCompletion
 * property.
 *
 * Returns: (transfer none) (nullable): a #GtkTextBuffer or %NULL
 */
GtkSourceBuffer *
gtk_source_completion_context_get_buffer (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	if (self->completion != NULL)
		return gtk_source_completion_get_buffer (self->completion);

	return NULL;
}

/**
 * gtk_source_completion_context_get_view:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the text view for the context.
 *
 * Returns: (nullable) (transfer none): a #GtkSourceView or %NULL
 */
GtkSourceView *
gtk_source_completion_context_get_view (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	if (self->completion != NULL)
		return gtk_source_completion_get_view (self->completion);

	return NULL;
}

void
_gtk_source_completion_context_refilter (GtkSourceCompletionContext *self)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self));

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		if (info->error != NULL)
			continue;

		if (info->results == NULL)
			continue;

		gtk_source_completion_provider_refilter (info->provider, self, info->results);
	}
}

gboolean
_gtk_source_completion_context_iter_invalidates (GtkSourceCompletionContext *self,
                                                 const GtkTextIter          *iter)
{
	GtkTextIter begin, end;
	GtkTextBuffer *buffer;

	g_assert (!self || GTK_SOURCE_IS_COMPLETION_CONTEXT (self));
	g_assert (iter != NULL);

	if (self == NULL)
		return FALSE;

	buffer = gtk_text_iter_get_buffer (iter);

	gtk_text_buffer_get_iter_at_mark (buffer, &begin, self->begin_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, &end, self->end_mark);

	return gtk_text_iter_compare (&begin, iter) <= 0 &&
	       gtk_text_iter_compare (&end, iter) >= 0;
}

/**
 * gtk_source_completion_context_get_empty:
 * @self: a #GtkSourceCompletionContext
 *
 * Checks if any proposals have been provided to the context.
 *
 * Out of convenience, this function will return %TRUE if @self is %NULL.
 *
 * Returns: %TRUE if there are no proposals in the context
 */
gboolean
gtk_source_completion_context_get_empty (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (!self || GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);

	return self ? self->empty : TRUE;
}

/**
 * gtk_source_completion_context_get_word:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the word that is being completed up to the position of the insert mark.
 *
 * Returns: (transfer full): a string containing the current word
 */
gchar *
gtk_source_completion_context_get_word (GtkSourceCompletionContext *self)
{
	GtkTextIter begin, end;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	gtk_source_completion_context_get_bounds (self, &begin, &end);

	return gtk_text_iter_get_slice (&begin, &end);
}

gboolean
_gtk_source_completion_context_can_refilter (GtkSourceCompletionContext *self,
                                             const GtkTextIter          *begin,
                                             const GtkTextIter          *end)
{
	GtkTextIter old_begin;
	GtkTextIter old_end;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	gtk_source_completion_context_get_bounds (self, &old_begin, &old_end);

	if (gtk_text_iter_equal (&old_begin, begin))
	{
		/*
		 * TODO: We can probably get smarter about this by asking all of
		 * the providers if they can refilter the new word (and only reload
		 * the data for those that cannot.
		 *
		 * Also, we might want to deal with that by copying the context
		 * into a new context and query using that.
		 */
		if (gtk_text_iter_compare (&old_end, end) <= 0)
		{
			GtkTextBuffer *buffer = gtk_text_iter_get_buffer (begin);

			gtk_text_buffer_move_mark (buffer, self->begin_mark, begin);
			gtk_text_buffer_move_mark (buffer, self->end_mark, end);

			return TRUE;
		}
	}

	return FALSE;
}

/**
 * gtk_source_completion_context_get_activation:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the mode for which the context was activated.
 */
GtkSourceCompletionActivation
gtk_source_completion_context_get_activation (GtkSourceCompletionContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), 0);

	return self->activation;
}

/**
 * gtk_source_completion_context_get_language:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the language of the underlying buffer, if any.
 *
 * Returns: (nullable) (transfer none): a #GtkSourceLanguage or %NULL
 */
GtkSourceLanguage *
gtk_source_completion_context_get_language (GtkSourceCompletionContext *self)
{
	GtkSourceBuffer *buffer;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	buffer = gtk_source_completion_context_get_buffer (self);

	if (buffer != NULL)
	{
		return gtk_source_buffer_get_language (buffer);
	}

	return NULL;
}

/**
 * gtk_source_completion_context_list_providers:
 * @self: a #GtkSourceCompletionContext
 *
 * Gets the providers that are associated with the context.
 *
 * Returns: (transfer full): a #GListModel of #GtkSourceCompletionProvider
 *
 * Since: 5.6
 */
GListModel *
gtk_source_completion_context_list_providers (GtkSourceCompletionContext *self)
{
	GListStore *store;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (self), NULL);

	store = g_list_store_new (GTK_SOURCE_TYPE_COMPLETION_PROVIDER);

	for (guint i = 0; i < self->providers->len; i++)
	{
		const ProviderInfo *info = &g_array_index (self->providers, ProviderInfo, i);

		g_list_store_append (store, info->provider);
	}

	return G_LIST_MODEL (store);
}

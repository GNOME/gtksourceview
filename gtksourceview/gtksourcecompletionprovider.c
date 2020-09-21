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

#include "gtksourcecompletioncontext.h"
#include "gtksourcecompletioncell.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"

G_DEFINE_INTERFACE (GtkSourceCompletionProvider, gtk_source_completion_provider, G_TYPE_OBJECT)

static void
fallback_populate_async (GtkSourceCompletionProvider *provider,
                         GtkSourceCompletionContext  *context,
                         GCancellable                *cancellable,
                         GAsyncReadyCallback          callback,
                         gpointer                     user_data)
{
	GListModel *ret;
	GError *error = NULL;
	GTask *task;

	task = g_task_new (provider, cancellable, callback, user_data);
	g_task_set_source_tag (task, fallback_populate_async);

	ret = GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (provider)->populate (provider, context, &error);

	if (ret == NULL)
	{
		if (error != NULL)
		{
			g_task_return_error (task, g_steal_pointer (&error));
		}
		else
		{
			g_task_return_new_error (task,
			                         G_IO_ERROR,
			                         G_IO_ERROR_NOT_SUPPORTED,
			                         "No results");
		}
	}
	else
	{
		g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
	}

	g_clear_object (&task);
}

static GListModel *
fallback_populate_finish (GtkSourceCompletionProvider  *provider,
                          GAsyncResult                 *result,
                          GError                      **error)
{
	return g_task_propagate_pointer (G_TASK (result), error);
}

static GListModel *
fallback_populate (GtkSourceCompletionProvider  *provider,
                   GtkSourceCompletionContext   *context,
                   GError                      **error)
{
	g_set_error (error,
	             G_IO_ERROR,
	             G_IO_ERROR_NOT_SUPPORTED,
	             "Not supported");
	return NULL;
}

static void
fallback_refilter (GtkSourceCompletionProvider *provider,
                   GtkSourceCompletionContext  *context,
                   GListModel                  *model)
{
}

static void
fallback_activate (GtkSourceCompletionProvider *provider,
                   GtkSourceCompletionContext  *context,
                   GtkSourceCompletionProposal *proposal)
{
}

static void
gtk_source_completion_provider_default_init (GtkSourceCompletionProviderInterface *iface)
{
	iface->populate_async = fallback_populate_async;
	iface->populate_finish = fallback_populate_finish;
	iface->populate = fallback_populate;
	iface->refilter = fallback_refilter;
	iface->activate = fallback_activate;
}

char *
gtk_source_completion_provider_get_title (GtkSourceCompletionProvider *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), NULL);

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->get_title)
		return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->get_title (self);

	return NULL;
}

int
gtk_source_completion_provider_get_priority (GtkSourceCompletionProvider *self,
                                             GtkSourceCompletionContext  *context)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), 0);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), 0);

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->get_priority)
		return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->get_priority (self, context);

	return 0;
}

gboolean
gtk_source_completion_provider_is_trigger (GtkSourceCompletionProvider *self,
                                           const GtkTextIter           *iter,
                                           gunichar                     ch)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->is_trigger)
		return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->is_trigger (self, iter, ch);

	return FALSE;
}

gboolean
gtk_source_completion_provider_key_activates (GtkSourceCompletionProvider *self,
                                              GtkSourceCompletionContext  *context,
                                              GtkSourceCompletionProposal *proposal,
                                              guint                         keyval,
                                              GdkModifierType               state)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal), FALSE);

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->key_activates)
		return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->key_activates (self, context, proposal, keyval, state);

	return FALSE;
}

void
gtk_source_completion_provider_populate_async (GtkSourceCompletionProvider *self,
                                               GtkSourceCompletionContext  *context,
                                               GCancellable                *cancellable,
                                               GAsyncReadyCallback          callback,
                                               gpointer                     user_data)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->populate_async (self, context, cancellable, callback, user_data);
}

/**
 * gtk_source_completion_provider_populate_finish:
 * @self: a #GtkSourceCompletionProvider
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes an asynchronous operation to populate a completion provider.
 *
 * Returns: (transfer full): a #GListModel of #GtkSourceCompletionProposal
 *
 * Since: 5.0
 */
GListModel *
gtk_source_completion_provider_populate_finish (GtkSourceCompletionProvider  *self,
                                                GAsyncResult                 *result,
                                                GError                      **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), NULL);

	return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->populate_finish (self, result, error);
}

void
gtk_source_completion_provider_refilter (GtkSourceCompletionProvider *self,
                                         GtkSourceCompletionContext  *context,
                                         GListModel                  *model)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (G_IS_LIST_MODEL (model));

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->refilter)
		GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->refilter (self, context, model);
}

void
gtk_source_completion_provider_display (GtkSourceCompletionProvider *self,
                                        GtkSourceCompletionContext  *context,
                                        GtkSourceCompletionProposal *proposal,
                                        GtkSourceCompletionCell     *cell)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (cell));

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->display)
		GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->display (self, context, proposal, cell);
}

void
gtk_source_completion_provider_activate (GtkSourceCompletionProvider *self,
                                         GtkSourceCompletionContext  *context,
                                         GtkSourceCompletionProposal *proposal)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->activate)
		GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->activate (self, context, proposal);
}

/**
 * gtk_source_completion_provider_list_alternates:
 * @self: a #GtkSourceCompletionProvider
 * @context: a #GtkSourceCompletionContext
 * @proposal: a #GtkSourceCompletionProposal
 *
 * Providers should return a list of alternates to @proposal or %NULL if
 * there are no alternates available. This can be used by the completion
 * view to allow the user to move laterally through similar proposals, such
 * as overrides of methods by the same name.
 *
 * Returns: (nullable) (transfer full) (element-type GtkSourceCompletionProposal):
 *   a #GPtrArray of #GtkSourceCompletionProposal or %NULL.
 *
 * Since: 5.0
 */
GPtrArray *
gtk_source_completion_provider_list_alternates (GtkSourceCompletionProvider *self,
                                                GtkSourceCompletionContext  *context,
                                                GtkSourceCompletionProposal *proposal)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (self), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CONTEXT (context), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal), NULL);

	if (GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->list_alternates)
		return GTK_SOURCE_COMPLETION_PROVIDER_GET_IFACE (self)->list_alternates (self, context, proposal);

	return NULL;
}

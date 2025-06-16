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

#include "gtksourceview-private.h"

void
_gtk_source_view_assistants_init (GtkSourceViewAssistants *assistants,
                                  GtkSourceView           *view)
{
	g_assert (assistants != NULL);
	g_assert (assistants->view == NULL);

	assistants->view = view;
	g_queue_init (&assistants->queue);
}

void
_gtk_source_view_assistants_shutdown (GtkSourceViewAssistants *assistants)
{
	g_assert (assistants != NULL);
	g_assert (assistants->view != NULL);

	while (assistants->queue.length > 0)
	{
		GtkSourceAssistant*assistant = g_queue_peek_head (&assistants->queue);
		_gtk_source_view_assistants_remove (assistants, assistant);
	}

	assistants->view = NULL;

	g_assert (assistants->view == NULL);
	g_assert (g_queue_is_empty (&assistants->queue));
}

void
_gtk_source_view_assistants_add (GtkSourceViewAssistants *assistants,
                                 GtkSourceAssistant      *assistant)
{
	g_assert (assistants != NULL);
	g_assert (assistants->view != NULL);

	if (gtk_widget_get_parent (GTK_WIDGET (assistant)))
	{
		g_warning ("Cannot add assistant, it already has a parent");
		return;
	}

	g_queue_push_tail (&assistants->queue, g_object_ref_sink (assistant));
	gtk_widget_set_parent (GTK_WIDGET (assistant), GTK_WIDGET (assistants->view));
}

void
_gtk_source_view_assistants_remove (GtkSourceViewAssistants *assistants,
                                    GtkSourceAssistant      *assistant)
{
	GList *link;

	g_assert (assistants != NULL);
	g_assert (assistants->view != NULL);
	g_assert (assistants->queue.length > 0);

	link = g_queue_find (&assistants->queue, assistant);

	if (link != NULL)
	{
		g_queue_delete_link (&assistants->queue, link);
		gtk_widget_unparent (GTK_WIDGET (assistant));
		g_object_unref (assistant);
	}
}

void
_gtk_source_view_assistants_size_allocate (GtkSourceViewAssistants *assistants,
                                           int                      width,
                                           int                      height,
                                           int                      baseline)
{
	g_assert (assistants != NULL);

	for (const GList *iter = assistants->queue.head; iter; iter = iter->next)
	{
		GtkSourceAssistant *assistant = iter->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

		if (gtk_widget_get_visible (GTK_WIDGET (assistant)) &&
		    gtk_native_get_surface (GTK_NATIVE (assistant)) != NULL)
		{
			GtkRequisition req;

			gtk_widget_get_preferred_size (GTK_WIDGET (assistant), NULL, &req);
			gtk_popover_present (GTK_POPOVER (assistant));
		}
	}
}

gboolean
_gtk_source_view_assistants_hide_all (GtkSourceViewAssistants *assistants)
{
	gboolean ret = FALSE;

	g_assert (assistants != NULL);

	for (const GList *iter = assistants->queue.head; iter; iter = iter->next)
	{
		GtkSourceAssistant *assistant = iter->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

		if (gtk_widget_get_visible (GTK_WIDGET (assistant)))
		{
			gtk_widget_set_visible (GTK_WIDGET (assistant), FALSE);
			ret = TRUE;
		}
	}

	return ret;
}

gboolean
_gtk_source_view_assistants_handle_key (GtkSourceViewAssistants *assistants,
                                        guint                    keyval,
                                        GdkModifierType          state)
{
	g_assert (assistants != NULL);

	if (keyval == GDK_KEY_Escape)
	{
		if (_gtk_source_view_assistants_hide_all (assistants))
		{
			gtk_widget_grab_focus (GTK_WIDGET (assistants->view));
			return TRUE;
		}
	}

	return FALSE;
}

void
_gtk_source_view_assistants_update_all (GtkSourceViewAssistants *assistants)
{
	g_assert (assistants != NULL);

	for (const GList *iter = assistants->queue.head; iter; iter = iter->next)
	{
		GtkSourceAssistant *assistant = iter->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));

		if (gtk_widget_get_visible (GTK_WIDGET (assistant)))
		{
			_gtk_source_assistant_update_position (assistant);
		}
	}
}

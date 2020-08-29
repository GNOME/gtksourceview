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

#include "gtksourceassistant-private.h"
#include "gtksourceassistantchild-private.h"

struct _GtkSourceAssistantChild
{
	GtkWidget parent_instance;
	GtkWidget *child;
	GQueue attached;
};

G_DEFINE_TYPE (GtkSourceAssistantChild, _gtk_source_assistant_child, GTK_TYPE_WIDGET)

static void
_gtk_source_assistant_child_size_allocate (GtkWidget *widget,
                                           int        width,
                                           int        height,
                                           int        baseline)
{
	GtkSourceAssistantChild *child = (GtkSourceAssistantChild *)widget;

	g_assert (GTK_SOURCE_IS_ASSISTANT_CHILD (child));

	GTK_WIDGET_CLASS (_gtk_source_assistant_child_parent_class)->size_allocate (widget, width, height, baseline);

	for (const GList *iter = child->attached.head; iter; iter = iter->next)
	{
		GtkSourceAssistant *attached = iter->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (attached));
		g_assert (GTK_IS_NATIVE (attached));

		if (gtk_widget_get_visible (GTK_WIDGET (attached)))
		{
			gtk_native_check_resize (GTK_NATIVE (attached));
		}
	}
}

static void
_gtk_source_assistant_child_dispose (GObject *object)
{
	GtkSourceAssistantChild *self = (GtkSourceAssistantChild *)object;

	g_assert (GTK_SOURCE_IS_ASSISTANT_CHILD (self));

	while (self->attached.head != NULL)
	{
		GtkSourceAssistant *attached = self->attached.head->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (attached));

		_gtk_source_assistant_child_detach (self, attached);
	}

  g_clear_pointer (&self->child, gtk_widget_unparent);

	G_OBJECT_CLASS (_gtk_source_assistant_child_parent_class)->dispose (object);
}

static void
_gtk_source_assistant_child_class_init (GtkSourceAssistantChildClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = _gtk_source_assistant_child_dispose;

	widget_class->size_allocate = _gtk_source_assistant_child_size_allocate;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
_gtk_source_assistant_child_init (GtkSourceAssistantChild *self)
{
}

GtkSourceAssistantChild *
_gtk_source_assistant_child_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_ASSISTANT_CHILD, NULL);
}

void
_gtk_source_assistant_child_hide (GtkSourceAssistantChild *self)
{
	g_assert (GTK_SOURCE_IS_ASSISTANT_CHILD (self));

	for (const GList *iter = self->attached.head; iter; iter = iter->next)
	{
		GtkSourceAssistant *attached = iter->data;

		g_assert (GTK_SOURCE_IS_ASSISTANT (attached));
		g_assert (GTK_IS_POPOVER (attached));

		gtk_popover_popdown (GTK_POPOVER (attached));
	}
}

void
_gtk_source_assistant_child_detach (GtkSourceAssistantChild *self,
                                    GtkSourceAssistant      *child)
{
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT_CHILD (self));
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (child));

	if (g_queue_remove (&self->attached, child))
	{
		gtk_widget_unparent (GTK_WIDGET (child));
		g_object_unref (child);
	}
}

void
_gtk_source_assistant_child_attach (GtkSourceAssistantChild *self,
                                    GtkSourceAssistant      *child)
{
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT_CHILD (self));
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT (child));
	g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (child)) == NULL);

	g_queue_push_tail (&self->attached, g_object_ref_sink (child));
	gtk_widget_set_parent (GTK_WIDGET (child), GTK_WIDGET (self));

	if (GTK_IS_NATIVE (child))
	{
		if (gtk_widget_get_visible (GTK_WIDGET (child)))
		{
			gtk_native_check_resize (GTK_NATIVE (child));
		}
	}
}

void
_gtk_source_assistant_child_set_child (GtkSourceAssistantChild *self,
                                       GtkWidget               *child)
{
	g_return_if_fail (GTK_SOURCE_IS_ASSISTANT_CHILD (self));
	g_return_if_fail (GTK_IS_WIDGET (child));

	if (child == self->child)
	{
		return;
	}

	g_clear_pointer (&self->child, gtk_widget_unparent);

	if (child != NULL)
	{
		self->child = child;
		gtk_widget_set_parent (child, GTK_WIDGET (self));
	}

	gtk_widget_queue_resize (GTK_WIDGET (self));
}

const GList *
_gtk_source_assistant_child_get_attached (GtkSourceAssistantChild *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ASSISTANT_CHILD (self), NULL);

	return self->attached.head;
}

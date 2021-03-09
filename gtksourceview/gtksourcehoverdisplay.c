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

#include "gtksourcehoverdisplay.h"

struct _GtkSourceHoverDisplay
{
	GtkWidget parent_instance;

	GtkBox *vbox;
};

G_DEFINE_TYPE (GtkSourceHoverDisplay, gtk_source_hover_display, GTK_TYPE_WIDGET)

static void
gtk_source_hover_display_dispose (GObject *object)
{
	GtkSourceHoverDisplay *self = (GtkSourceHoverDisplay *)object;

	g_clear_pointer ((GtkWidget **)&self->vbox, gtk_widget_unparent);

	G_OBJECT_CLASS (gtk_source_hover_display_parent_class)->dispose (object);
}

static void
gtk_source_hover_display_class_init (GtkSourceHoverDisplayClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gtk_source_hover_display_dispose;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_source_hover_display_init (GtkSourceHoverDisplay *self)
{
	self->vbox = g_object_new (GTK_TYPE_BOX, NULL);
	gtk_widget_set_parent (GTK_WIDGET (self->vbox), GTK_WIDGET (self));
}

void
gtk_source_hover_display_append (GtkSourceHoverDisplay *self,
                                 GtkWidget             *child)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (self));
	g_return_if_fail (GTK_IS_WIDGET (child));
}

void
gtk_source_hover_display_prepend (GtkSourceHoverDisplay *self,
                                  GtkWidget             *child)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (self));
	g_return_if_fail (GTK_IS_WIDGET (child));
}

void
gtk_source_hover_display_insert_after (GtkSourceHoverDisplay *self,
                                       GtkWidget             *child,
                                       GtkWidget             *sibling)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (self));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (!sibling || GTK_IS_WIDGET (sibling));

	if (sibling == NULL)
	{
		gtk_source_hover_display_append (self, child);
	}
}

void
gtk_source_hover_display_remove (GtkSourceHoverDisplay *self,
                                 GtkWidget             *child)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (self));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (gtk_widget_get_parent (child) == (GtkWidget *)self->vbox);
}

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

#include "gtksourceassistant-private.h"
#include "gtksourcehoverassistant-private.h"
#include "gtksourcehoverdisplay.h"

struct _GtkSourceHoverAssistant
{
	GtkSourceAssistant parent_instance;
  GtkSourceHoverDisplay *display;
	GdkRectangle hovered_at;
};

G_DEFINE_TYPE (GtkSourceHoverAssistant, gtk_source_hover_assistant, GTK_SOURCE_TYPE_ASSISTANT)

static void
gtk_source_hover_assistant_get_target_location (GtkSourceAssistant *assistant,
                                                GdkRectangle       *rect)
{
	*rect = GTK_SOURCE_HOVER_ASSISTANT (assistant)->hovered_at;
}

static void
gtk_source_hover_assistant_class_init (GtkSourceHoverAssistantClass *klass)
{
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);

	assistant_class->get_target_location = gtk_source_hover_assistant_get_target_location;
}

static void
gtk_source_hover_assistant_init (GtkSourceHoverAssistant *self)
{
	self->display = g_object_new (GTK_SOURCE_TYPE_HOVER_DISPLAY, NULL);
	_gtk_source_assistant_set_child (GTK_SOURCE_ASSISTANT (self), GTK_WIDGET (self->display));
}

GtkSourceHoverAssistant *
_gtk_source_hover_assistant_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_HOVER_ASSISTANT, NULL);
}

void
_gtk_source_hover_assistant_set_hovered_at (GtkSourceHoverAssistant *self,
                                            const GdkRectangle      *hovered_at)
{
	g_return_if_fail (GTK_SOURCE_IS_HOVER_ASSISTANT (self));
	g_return_if_fail (hovered_at != NULL);

	self->hovered_at = *hovered_at;
}

GtkSourceHoverDisplay *
_gtk_source_hover_assistant_get_display (GtkSourceHoverAssistant *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_HOVER_ASSISTANT (self), NULL);

	return self->display;
}

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
#include "gtksourcecompletioncell-private.h"
#include "gtksourcecompletioninfo-private.h"

struct _GtkSourceCompletionInfo
{
	GtkSourceAssistant       parent_instance;
	GtkSourceCompletionCell *cell;
};

G_DEFINE_TYPE (GtkSourceCompletionInfo, _gtk_source_completion_info, GTK_SOURCE_TYPE_ASSISTANT)

static void
_gtk_source_completion_info_get_offset (GtkSourceAssistant *assistant,
                                        int                *x_offset,
                                        int                *y_offset)
{
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS

	GtkStyleContext *style_context;
	GtkBorder margin;

	g_assert (GTK_SOURCE_IS_COMPLETION_INFO (assistant));

	style_context = gtk_widget_get_style_context (GTK_WIDGET (assistant));
	gtk_style_context_get_margin (style_context, &margin);

	*x_offset = -margin.left + 1;
	*y_offset = -margin.top;

	G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
_gtk_source_completion_info_class_init (GtkSourceCompletionInfoClass *klass)
{
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);

	assistant_class->get_offset = _gtk_source_completion_info_get_offset;
}

static void
_gtk_source_completion_info_init (GtkSourceCompletionInfo *self)
{
	gtk_widget_add_css_class (GTK_WIDGET (self), "completion-info");
	gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_RIGHT);
	gtk_popover_set_autohide (GTK_POPOVER (self), FALSE);

	self->cell = g_object_new (GTK_SOURCE_TYPE_COMPLETION_CELL,
	                           "column", GTK_SOURCE_COMPLETION_COLUMN_DETAILS,
				   "halign", GTK_ALIGN_START,
				   "valign", GTK_ALIGN_START,
	                           NULL);
	_gtk_source_assistant_set_child (GTK_SOURCE_ASSISTANT (self), GTK_WIDGET (self->cell));
}

GtkSourceCompletionInfo *
_gtk_source_completion_info_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_INFO, NULL);
}

GtkSourceCompletionCell *
_gtk_source_completion_info_get_cell (GtkSourceCompletionInfo *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_INFO (self), NULL);

	return self->cell;
}

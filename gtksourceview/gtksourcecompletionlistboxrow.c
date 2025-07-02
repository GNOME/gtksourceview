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

#include "gtksourcecompletioncell-private.h"
#include "gtksourcecompletioncontext-private.h"
#include "gtksourcecompletionlistboxrow-private.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"

struct _GtkSourceCompletionListBoxRow
{
	GtkListBoxRow                parent_instance;

	GtkSourceCompletionProposal *proposal;
	PangoAttrList               *attrs;

	GtkBox                      *box;
	GtkBox                      *more;
	GtkSourceCompletionCell     *icon;
	GtkSourceCompletionCell     *before;
	GtkSourceCompletionCell     *typed_text;
	GtkSourceCompletionCell     *after;
};

G_DEFINE_TYPE (GtkSourceCompletionListBoxRow, gtk_source_completion_list_box_row, GTK_TYPE_LIST_BOX_ROW)

static void
gtk_source_completion_list_box_row_finalize (GObject *object)
{
	GtkSourceCompletionListBoxRow *self = (GtkSourceCompletionListBoxRow *)object;

	g_clear_pointer (&self->attrs, pango_attr_list_unref);

	G_OBJECT_CLASS (gtk_source_completion_list_box_row_parent_class)->finalize (object);
}

static void
gtk_source_completion_list_box_row_class_init (GtkSourceCompletionListBoxRowClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = gtk_source_completion_list_box_row_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gtksourceview/ui/gtksourcecompletionlistboxrow.ui");
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, box);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, icon);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, before);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, typed_text);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, after);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionListBoxRow, more);

	g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_CELL);
}

static void
gtk_source_completion_list_box_row_init (GtkSourceCompletionListBoxRow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
_gtk_source_completion_list_box_row_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_LIST_BOX_ROW, NULL);
}

void
_gtk_source_completion_list_box_row_display (GtkSourceCompletionListBoxRow *self,
                                             GtkSourceCompletionContext    *context,
                                             GtkSourceCompletionProvider   *provider,
                                             GtkSourceCompletionProposal   *proposal,
                                             gboolean                       show_icons,
                                             gboolean                       has_alternates)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (self));
	g_return_if_fail (!context || GTK_SOURCE_IS_COMPLETION_CONTEXT (context));
	g_return_if_fail (!provider || GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_return_if_fail (!proposal || GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

	if (proposal == NULL)
	{
		gtk_source_completion_cell_set_widget (self->icon, NULL);
		gtk_source_completion_cell_set_widget (self->before, NULL);
		gtk_source_completion_cell_set_widget (self->typed_text, NULL);
		gtk_source_completion_cell_set_widget (self->after, NULL);
	}
	else
	{
		gtk_source_completion_provider_display (provider, context, proposal, self->icon);
		gtk_source_completion_provider_display (provider, context, proposal, self->before);
		gtk_source_completion_provider_display (provider, context, proposal, self->typed_text);
		gtk_source_completion_provider_display (provider, context, proposal, self->after);
	}

	gtk_widget_set_visible (GTK_WIDGET (self->icon), show_icons);
	gtk_widget_set_visible (GTK_WIDGET (self->more), has_alternates);
}

void
_gtk_source_completion_list_box_row_attach (GtkSourceCompletionListBoxRow *self,
                                            GtkSizeGroup                  *before,
                                            GtkSizeGroup                  *typed_text,
                                            GtkSizeGroup                  *after)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (self));
	g_return_if_fail (GTK_IS_SIZE_GROUP (before));
	g_return_if_fail (GTK_IS_SIZE_GROUP (typed_text));
	g_return_if_fail (GTK_IS_SIZE_GROUP (after));

	gtk_size_group_add_widget (before, GTK_WIDGET (self->before));
	gtk_size_group_add_widget (typed_text, GTK_WIDGET (self->typed_text));
	gtk_size_group_add_widget (after, GTK_WIDGET (self->after));
}

static void
get_margin_and_border (GtkWidget *widget,
                       GtkBorder *margin,
                       GtkBorder *border)
{
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS

	GtkStyleContext *style_context = gtk_widget_get_style_context (widget);
	gtk_style_context_get_margin (style_context, margin);
	gtk_style_context_get_border (style_context, border);

	G_GNUC_END_IGNORE_DEPRECATIONS
}

gint
_gtk_source_completion_list_box_row_get_x_offset (GtkSourceCompletionListBoxRow *self,
                                                  GtkWidget                     *toplevel)
{
	GtkRequisition min;
	GtkRequisition nat;
	GtkBorder margin;
	GtkBorder border;
	double x = 0;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (self), 0);
	g_return_val_if_fail (GTK_IS_WIDGET (toplevel), 0);

	for (GtkWidget *iter = GTK_WIDGET (self->box);
	     iter != NULL;
	     iter = gtk_widget_get_parent (iter))
	{
		get_margin_and_border (iter, &margin, &border);
		x += margin.left + border.left;

		if (iter == toplevel)
		{
			break;
		}
	}

	get_margin_and_border (GTK_WIDGET (self->icon), &margin, &border);
	gtk_widget_get_preferred_size (GTK_WIDGET (self->icon), &min, &nat);
	x += margin.left + border.left + nat.width + margin.right + border.right;

	get_margin_and_border (GTK_WIDGET (self->before), &margin, &border);
	gtk_widget_get_preferred_size (GTK_WIDGET (self->before), &min, &nat);
	x += margin.left + border.left + nat.width + border.right + margin.right;

	get_margin_and_border (GTK_WIDGET (self->typed_text), &margin, &border);
	gtk_widget_get_preferred_size (GTK_WIDGET (self->typed_text), &min, &nat);
	x += margin.left + border.left;

	return -x;
}

void
_gtk_source_completion_list_box_row_set_attrs (GtkSourceCompletionListBoxRow *self,
                                               PangoAttrList                 *attrs)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (self));

	_gtk_source_completion_cell_set_attrs (self->icon, attrs);
	_gtk_source_completion_cell_set_attrs (self->before, attrs);
	_gtk_source_completion_cell_set_attrs (self->typed_text, attrs);
	_gtk_source_completion_cell_set_attrs (self->after, attrs);
}

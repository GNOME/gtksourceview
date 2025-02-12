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

#include "gtksource-enumtypes.h"
#include "gtksourcecompletioncell-private.h"

/**
 * GtkSourceCompletionCell:
 *
 * Widget for single cell of completion proposal.
 *
 * The `GtkSourceCompletionCell` widget provides a container to display various
 * types of information with the completion display.
 *
 * Each proposal may consist of multiple cells depending on the complexity of
 * the proposal. For example, programming language proposals may contain a cell
 * for the "left-hand-side" of an operation along with the "typed-text" for a
 * function name and "parameters". They may also optionally set an icon to
 * signify the kind of result.
 *
 * A [iface@CompletionProvider] should implement the
 * [vfunc@CompletionProvider.display] virtual function to control
 * how to convert data from their [iface@CompletionProposal] to content for
 * the `GtkSourceCompletionCell`.
 */

struct _GtkSourceCompletionCell
{
	GtkWidget                  widget;
	GtkSourceCompletionColumn  column;
	GtkWidget                 *child;
	PangoAttrList             *attrs;
};

enum {
	PROP_0,
	PROP_COLUMN,
	PROP_MARKUP,
	PROP_PAINTABLE,
	PROP_TEXT,
	PROP_WIDGET,
	N_PROPS
};

G_DEFINE_TYPE (GtkSourceCompletionCell, gtk_source_completion_cell, GTK_TYPE_WIDGET)

static GParamSpec *properties [N_PROPS];

static void
gtk_source_completion_cell_set_column (GtkSourceCompletionCell   *self,
                                       GtkSourceCompletionColumn  column)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_CELL (self));

	self->column = column;

	switch (column)
	{
	case GTK_SOURCE_COMPLETION_COLUMN_ICON:
		gtk_widget_add_css_class (GTK_WIDGET (self), "icon");
		break;

	case GTK_SOURCE_COMPLETION_COLUMN_BEFORE:
		gtk_widget_add_css_class (GTK_WIDGET (self), "before");
		break;

	case GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT:
		gtk_widget_add_css_class (GTK_WIDGET (self), "typed-text");
		break;

	case GTK_SOURCE_COMPLETION_COLUMN_AFTER:
		gtk_widget_add_css_class (GTK_WIDGET (self), "after");
		break;

	case GTK_SOURCE_COMPLETION_COLUMN_COMMENT:
		gtk_widget_add_css_class (GTK_WIDGET (self), "comment");
		break;

	case GTK_SOURCE_COMPLETION_COLUMN_DETAILS:
		gtk_widget_add_css_class (GTK_WIDGET (self), "details");
		break;

	default:
		break;
	}
}

static void
gtk_source_completion_cell_dispose (GObject *object)
{
	GtkSourceCompletionCell *self = (GtkSourceCompletionCell *)object;

	g_clear_pointer (&self->child, gtk_widget_unparent);
	g_clear_pointer (&self->attrs, pango_attr_list_unref);

	G_OBJECT_CLASS (gtk_source_completion_cell_parent_class)->dispose (object);
}

static void
gtk_source_completion_cell_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceCompletionCell *self = GTK_SOURCE_COMPLETION_CELL (object);

	switch (prop_id)
	{
	case PROP_COLUMN:
		g_value_set_enum (value, self->column);
		break;

	case PROP_TEXT:
		if (GTK_IS_LABEL (self->child))
			g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->child)));
		break;

	case PROP_MARKUP:
		if (GTK_IS_LABEL (self->child) &&
		    gtk_label_get_use_markup (GTK_LABEL (self->child)))
			g_value_set_string (value, gtk_label_get_label (GTK_LABEL (self->child)));
		break;

	case PROP_PAINTABLE:
		if (GTK_IS_IMAGE (self->child))
			g_value_set_object (value, gtk_image_get_paintable (GTK_IMAGE (self->child)));
		break;

	case PROP_WIDGET:
		g_value_set_object (value, self->child);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_cell_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceCompletionCell *self = GTK_SOURCE_COMPLETION_CELL (object);

	switch (prop_id)
	{
	case PROP_COLUMN:
		gtk_source_completion_cell_set_column (self, g_value_get_enum (value));
		break;

	case PROP_MARKUP:
		gtk_source_completion_cell_set_markup (self, g_value_get_string (value));
		break;

	case PROP_TEXT:
		gtk_source_completion_cell_set_text (self, g_value_get_string (value));
		break;

	case PROP_PAINTABLE:
		gtk_source_completion_cell_set_paintable (self, g_value_get_object (value));
		break;

	case PROP_WIDGET:
		gtk_source_completion_cell_set_widget (self, g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_cell_class_init (GtkSourceCompletionCellClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gtk_source_completion_cell_dispose;
	object_class->get_property = gtk_source_completion_cell_get_property;
	object_class->set_property = gtk_source_completion_cell_set_property;

	properties [PROP_COLUMN] =
		g_param_spec_enum ("column",
		                   "Column",
		                   "Column",
		                   GTK_SOURCE_TYPE_COMPLETION_COLUMN,
		                   GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT,
		                   (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	properties [PROP_MARKUP] =
		g_param_spec_string ("markup",
		                     "Markup",
		                     "Markup",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_TEXT] =
		g_param_spec_string ("text",
		                     "Text",
		                     "Text",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_PAINTABLE] =
		g_param_spec_object ("paintable",
		                     "Paintable",
		                     "Paintable",
		                     GDK_TYPE_PAINTABLE,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_WIDGET] =
		g_param_spec_object ("widget",
		                     "Widget",
		                     "Widget",
		                     GTK_TYPE_WIDGET,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_css_name (widget_class, "cell");
	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_source_completion_cell_init (GtkSourceCompletionCell *self)
{
	gtk_widget_add_css_class (GTK_WIDGET (self), "cell");
}

void
gtk_source_completion_cell_set_markup (GtkSourceCompletionCell *self,
                                       const char              *markup)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));

	if (markup == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	if (!GTK_IS_LABEL (self->child))
	{
		GtkWidget *child = gtk_label_new (NULL);
		gtk_source_completion_cell_set_widget (self, child);
	}

	gtk_label_set_text (GTK_LABEL (self->child), markup);
	gtk_label_set_use_markup (GTK_LABEL (self->child), TRUE);
}

/**
 * gtk_source_completion_cell_set_text:
 * @self: a #GtkSourceCompletionCell
 * @text: (nullable): the text to set or %NULL
 *
 * Sets the text for the column cell. Use %NULL to unset.
 */
void
gtk_source_completion_cell_set_text (GtkSourceCompletionCell *self,
                                     const char              *text)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));

	if (text == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	if (!GTK_IS_LABEL (self->child))
	{
		GtkWidget *child = gtk_label_new (NULL);
		gtk_source_completion_cell_set_widget (self, child);
	}

	if (gtk_label_get_use_markup (GTK_LABEL (self->child)))
	{
		gtk_label_set_use_markup (GTK_LABEL (self->child), FALSE);
	}

	if (g_strcmp0 (gtk_label_get_label (GTK_LABEL (self->child)), text) != 0)
	{
		gtk_label_set_label (GTK_LABEL (self->child), text);
	}
}

void
gtk_source_completion_cell_set_text_with_attributes (GtkSourceCompletionCell *self,
                                                     const char              *text,
                                                     PangoAttrList           *attrs)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));

	if (text == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	gtk_source_completion_cell_set_text (self, text);

	if (text == NULL)
	{
		return;
	}

	if (attrs != NULL)
	{
		if (self->attrs != NULL)
		{
			PangoAttrList *copy = pango_attr_list_copy (self->attrs);
			pango_attr_list_splice (copy, attrs, 0, g_utf8_strlen (text, -1));
			gtk_label_set_attributes (GTK_LABEL (self->child), copy);
			g_clear_pointer (&copy, pango_attr_list_unref);
		}
		else
		{
			gtk_label_set_attributes (GTK_LABEL (self->child), attrs);
		}
	}
	else
	{
		gtk_label_set_attributes (GTK_LABEL (self->child), self->attrs);
	}
}

void
gtk_source_completion_cell_set_paintable (GtkSourceCompletionCell *self,
                                          GdkPaintable            *paintable)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));
	g_return_if_fail (!paintable || GDK_IS_PAINTABLE (paintable));

	if (paintable == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	gtk_source_completion_cell_set_widget (self, gtk_image_new_from_paintable (paintable));
}

/**
 * gtk_source_completion_cell_get_widget:
 * @self: a #GtkSourceCompletionCell
 *
 * Gets the child #GtkWidget, if any.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget or %NULL
 */
GtkWidget *
gtk_source_completion_cell_get_widget (GtkSourceCompletionCell *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self), NULL);

	return self->child;
}

void
gtk_source_completion_cell_set_widget (GtkSourceCompletionCell *self,
                                       GtkWidget               *widget)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));
	g_return_if_fail (!widget || GTK_IS_WIDGET (widget));
	g_return_if_fail (!widget || gtk_widget_get_parent (widget) == NULL);

	if (widget == self->child)
	{
		return;
	}

	g_clear_pointer (&self->child, gtk_widget_unparent);

	if (widget != NULL)
	{
		self->child = widget;
		gtk_widget_set_parent (widget, GTK_WIDGET (self));

		if (GTK_IS_LABEL (widget))
		{
			gtk_label_set_attributes (GTK_LABEL (widget), self->attrs);

			if (self->column == GTK_SOURCE_COMPLETION_COLUMN_BEFORE)
			{
				gtk_label_set_xalign (GTK_LABEL (widget), 1.0);
			}
			else if (self->column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT)
			{
				gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
				gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
				gtk_widget_set_hexpand (widget, TRUE);
			}
			else if (self->column == GTK_SOURCE_COMPLETION_COLUMN_AFTER ||
			         self->column == GTK_SOURCE_COMPLETION_COLUMN_TYPED_TEXT ||
			         self->column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT ||
			         self->column == GTK_SOURCE_COMPLETION_COLUMN_DETAILS)
			{
				gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
			}

			if (self->column == GTK_SOURCE_COMPLETION_COLUMN_COMMENT)
			{
				gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
				gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
				gtk_label_set_wrap (GTK_LABEL (widget), TRUE);
				gtk_label_set_max_width_chars (GTK_LABEL (widget), 50);
				gtk_widget_set_valign (widget, GTK_ALIGN_BASELINE_FILL);
			}
		}
		else if (GTK_IS_IMAGE (widget))
		{
			if (self->column == GTK_SOURCE_COMPLETION_COLUMN_AFTER)
			{
				gtk_widget_set_halign (widget, GTK_ALIGN_END);
			}
		}
	}
}

GtkSourceCompletionColumn
gtk_source_completion_cell_get_column (GtkSourceCompletionCell *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self), 0);

	return self->column;
}

void
_gtk_source_completion_cell_set_attrs (GtkSourceCompletionCell *self,
                                       PangoAttrList           *attrs)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));

	if (attrs != self->attrs)
	{
		g_clear_pointer (&self->attrs, pango_attr_list_unref);

		if (attrs)
		{
			self->attrs = pango_attr_list_ref (attrs);
		}

		if (GTK_IS_LABEL (self->child))
		{
			gtk_label_set_attributes (GTK_LABEL (self->child), attrs);
		}
	}
}

void
gtk_source_completion_cell_set_icon_name (GtkSourceCompletionCell *self,
                                          const char              *icon_name)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));

	if (icon_name == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	if (!GTK_IS_IMAGE (self->child))
	{
		GtkWidget *image = gtk_image_new ();
		gtk_source_completion_cell_set_widget (self, image);
	}

	if (g_strcmp0 (icon_name, gtk_image_get_icon_name (GTK_IMAGE (self->child))) != 0)
	{
		gtk_image_set_from_icon_name (GTK_IMAGE (self->child), icon_name);
	}
}

void
gtk_source_completion_cell_set_gicon (GtkSourceCompletionCell *self,
                                      GIcon                   *gicon)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self));
	g_return_if_fail (!gicon || G_IS_ICON (gicon));

	if (gicon == NULL && _gtk_source_completion_cell_is_empty (self))
	{
		return;
	}

	if (!GTK_IS_IMAGE (self->child))
	{
		GtkWidget *image = gtk_image_new ();
		gtk_source_completion_cell_set_widget (self, image);
	}

	gtk_image_set_from_gicon (GTK_IMAGE (self->child), gicon);
}

gboolean
_gtk_source_completion_cell_is_empty (GtkSourceCompletionCell *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_CELL (self), FALSE);

	return self->child == NULL;
}

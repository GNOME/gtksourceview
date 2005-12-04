/*  gtksourcefoldlabel.c
 *
 *  Copyright (C) 2005 - Jeroen Zwartepoorte <jeroen.zwartepoorte@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include "gtksourcefoldlabel.h"

/* Properties */
enum {
	PROP_0,
	PROP_SOURCE_VIEW,
	PROP_X,
	PROP_Y
};

#define GTK_SOURCE_FOLD_LABEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_SOURCE_FOLD_LABEL, GtkSourceFoldLabelPrivate))

struct _GtkSourceFoldLabelPrivate
{
	GtkSourceView *source_view;
	int            x;
	int            y;
};

G_DEFINE_TYPE(GtkSourceFoldLabel, gtk_source_fold_label, GTK_TYPE_LABEL)

static void 
gtk_source_fold_label_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	GtkSourceFoldLabel *label;
	
	g_return_if_fail (GTK_IS_SOURCE_FOLD_LABEL (object));

	label = GTK_SOURCE_FOLD_LABEL (object);
    
	switch (prop_id)
	{
		case PROP_SOURCE_VIEW:
			g_value_set_object (value, label->priv->source_view);
			break;
			
		case PROP_X:
			g_value_set_int (value, label->priv->x);
			break;
			
		case PROP_Y:
			g_value_set_int (value, label->priv->y);
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
set_view (GtkSourceFoldLabel *label, GtkSourceView *view)
{
	PangoContext *ctx;
	PangoFontDescription *font_desc;

	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));
	
	ctx = gtk_widget_get_pango_context (GTK_WIDGET (view));
	font_desc = pango_context_get_font_description (ctx);
	gtk_widget_modify_font (GTK_WIDGET (label), font_desc);
}

static void 
gtk_source_fold_label_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	GtkSourceFoldLabel *label;
	
	g_return_if_fail (GTK_IS_SOURCE_FOLD_LABEL (object));

	label = GTK_SOURCE_FOLD_LABEL (object);
    
	switch (prop_id)
	{
		case PROP_SOURCE_VIEW:
			set_view (label, g_value_get_object (value));
			break;
			
		case PROP_X:
			label->priv->x = g_value_get_int (value);
			break;
			
		case PROP_Y:
			label->priv->y = g_value_get_int (value);
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static gint
gtk_source_fold_label_expose (GtkWidget      *widget,
			      GdkEventExpose *event)
{
	(* GTK_WIDGET_CLASS (gtk_source_fold_label_parent_class)->expose_event) (widget, event);

	gdk_draw_rectangle (event->window,
			    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
			    FALSE,
			    widget->allocation.x,
			    widget->allocation.y,
			    widget->allocation.width - 1,
			    widget->allocation.height - 1);

	return TRUE;
}

static void 
gtk_source_fold_label_class_init (GtkSourceFoldLabelClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	
	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = gtk_source_fold_label_get_property;
	object_class->set_property = gtk_source_fold_label_set_property;

	widget_class->expose_event = gtk_source_fold_label_expose;

	g_type_class_add_private (object_class, sizeof (GtkSourceFoldLabelPrivate));

	g_object_class_install_property (object_class,
					 PROP_SOURCE_VIEW,
					 g_param_spec_object ("sourceview",
							      _("Sourceview"),
							      _("The Sourceview this label is shown in"),
							      GTK_TYPE_SOURCE_VIEW,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_X,
					 g_param_spec_int ("x",
							   _("X coordinates for label"),
							   _("The horizontal position where the label is shown"),
							   -1,
							   G_MAXINT,
							   -1,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_Y,
					 g_param_spec_int ("y",
							   _("Y coordinates for label"),
							   _("The vertical position where the label is shown"),
							   -1,
							   G_MAXINT,
							   -1,
							   G_PARAM_READWRITE));
}

static void
gtk_source_fold_label_init (GtkSourceFoldLabel *label)
{
	label->priv = GTK_SOURCE_FOLD_LABEL_GET_PRIVATE (label);
}

GtkWidget *
gtk_source_fold_label_new (GtkSourceView *view)
{
	return GTK_WIDGET (g_object_new (GTK_TYPE_SOURCE_FOLD_LABEL,
					 "label", "..",
					 "sensitive", FALSE,
					 "sourceview", view,
					 "x", -1,
					 "y", -1,
					 NULL));
}

void
gtk_source_fold_label_get_position (GtkSourceFoldLabel *label,
				    int                *x,
				    int                *y)
{
	g_return_if_fail (GTK_IS_SOURCE_FOLD_LABEL (label));
	
	if (x != NULL)
		*x = label->priv->x;
	if (y != NULL)
		*y = label->priv->y;
}

void
gtk_source_fold_label_set_position (GtkSourceFoldLabel *label,
				    int                 x,
				    int                 y)
{
	g_return_if_fail (GTK_IS_SOURCE_FOLD_LABEL (label));
	
	label->priv->x = x;
	label->priv->y = y;
}

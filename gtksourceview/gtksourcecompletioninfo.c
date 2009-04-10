/*
 * gtksourcecompletioninfo.c
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gsc-info
 * @title: GtkSourceCompletionInfo
 * @short_description: Calltips object
 *
 * This object can be used to show a calltip or help. 
 */
  
#include <gtksourceview/gtksourcecompletioninfo.h>
#include "gtksourcecompletionutils.h"
#include "gtksourceview-i18n.h"

struct _GtkSourceCompletionInfoPrivate
{
	GtkWidget *box;
	GtkWidget *info_scroll;
	GtkWidget *label;
	GtkWidget *custom_widget;
	
	gboolean adjust_height;
	gboolean adjust_width;
	gint max_height;
	gint max_width;
};

/* Signals */
enum
{
	SHOW_INFO,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/*Type definition*/
G_DEFINE_TYPE(GtkSourceCompletionInfo, gtk_source_completion_info, GTK_TYPE_WINDOW);

#define GTK_SOURCE_COMPLETION_INFO_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_SOURCE_COMPLETION_INFO, GtkSourceCompletionInfoPrivate))
#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 200


static void
get_max_size (GtkSourceCompletionInfo *self,
	      GtkWidget *widget,
	      gint *w,
	      gint *h)
{
	GtkRequisition req;
	
	gtk_widget_size_request (widget, &req);

	if (self->priv->adjust_height)
	{
		if (req.height > self->priv->max_height)
		{
			*h = self->priv->max_height;
		}
		else
		{
			*h = req.height;
		}
	}
	else
	{
		*h = WINDOW_HEIGHT;
	}
	
	if (self->priv->adjust_width)
	{
		if (req.width > self->priv->max_width)
		{
			*w = self->priv->max_width;
		}
		else
		{
			*w = req.width;
		}
	}
	else
	{
		*w = WINDOW_WIDTH;
	}
}

static void
adjust_resize (GtkSourceCompletionInfo *self)
{
	gint w, h;
	
	if (!self->priv->custom_widget)
	{
		/* Default widget */
		get_max_size (self, self->priv->label, &w, &h);
		w = w + 5 + GTK_WIDGET (self->priv->info_scroll)->style->ythickness * 2;
		h = h + 5 + GTK_WIDGET (self->priv->info_scroll)->style->xthickness * 2;
	}
	else
	{
		/* Custom widget */
		get_max_size (self, self->priv->custom_widget, &w, &h);
	}
	
	gtk_window_resize (GTK_WINDOW (self), w, h);
}

static void
show (GtkWidget *widget)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (widget);
	
	g_signal_emit (self, signals[SHOW_INFO], 0);
	
	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->show (GTK_WIDGET (self));
	
	gtk_label_select_region (GTK_LABEL (self->priv->label), 0, 0);
}

static void
hide (GtkWidget *widget)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (widget);
	
	gtk_label_set_label (GTK_LABEL (self->priv->label), "");
	
	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->hide (GTK_WIDGET (self));
}

static void
gtk_source_completion_info_init (GtkSourceCompletionInfo *self)
{
	self->priv = GTK_SOURCE_COMPLETION_INFO_GET_PRIVATE (self);
	self->priv->adjust_height = FALSE;
	self->priv->adjust_width = FALSE;
	self->priv->max_height = WINDOW_HEIGHT;
	self->priv->max_width = WINDOW_WIDTH;
	self->priv->custom_widget = NULL;
	
	/*
	 * Make it look like a tooltip
	 */
	gtk_widget_set_name (GTK_WIDGET (self), "gtk-tooltip");
	gtk_widget_ensure_style (GTK_WIDGET (self));
	
	gtk_window_set_type_hint (GTK_WINDOW (self),
				  GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (self),
				     WINDOW_WIDTH, WINDOW_HEIGHT);

	self->priv->info_scroll = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->info_scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (self->priv->info_scroll);

	self->priv->label = gtk_label_new (" ");
	/*Top left position*/
	gtk_misc_set_alignment (GTK_MISC (self->priv->label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (self->priv->label), 5, 5);
	gtk_label_set_selectable (GTK_LABEL (self->priv->label), TRUE);
	
	gtk_widget_show (self->priv->label);

	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (self->priv->info_scroll),
					       self->priv->label);

	self->priv->box = gtk_vbox_new (FALSE, 1);
	
	gtk_widget_show (self->priv->box);
	gtk_box_pack_start (GTK_BOX (self->priv->box),
			    self->priv->info_scroll,
			    TRUE, TRUE, 0);
	
	gtk_container_add (GTK_CONTAINER (self), 
			   self->priv->box);
}

static void
gtk_source_completion_info_finalize (GObject *object)
{
	/*GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO(object);*/
	
	G_OBJECT_CLASS (gtk_source_completion_info_parent_class)->finalize (object);
}

static void
gtk_source_completion_info_class_init (GtkSourceCompletionInfoClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (GtkSourceCompletionInfoPrivate));

	object_class->finalize = gtk_source_completion_info_finalize;
	widget_class->show = show;
	widget_class->hide = hide;
	
	/**
	 * GtkSourceCompletionInfo::show-info:
	 * @info: The #GscInf who emits the signal
	 *
	 * This signal is emited before any "show" management. You can connect
	 * to this signal if you want to change some properties or position
	 * before to so the real "show".
	 **/
	signals[SHOW_INFO] =
		g_signal_new ("show-info",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);
}

/**
 * gtk_source_completion_info_new:
 *
 * Returns: The new GtkSourceCompletionInfo.
 *
 */
GtkSourceCompletionInfo*
gtk_source_completion_info_new (void)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (g_object_new (GTK_TYPE_SOURCE_COMPLETION_INFO,
										  "type", GTK_WINDOW_POPUP,
										  NULL));
	return self;
}

/**
 * gtk_source_completion_info_move_to_cursor:
 * @self: The #GtkSourceCompletionInfo
 * @view: The current GtkTextView where we want to show the info
 *
 * Moves the #GtkSourceCompletionInfo to under the @view cursor. If it cannot be shown down,
 * it will be shown up
 *
 */
void
gtk_source_completion_info_move_to_cursor (GtkSourceCompletionInfo* self,
					   GtkTextView *view)
{
	int x,y;
	gboolean resized = FALSE;
	
	g_return_if_fail  (GTK_IS_SOURCE_COMPLETION_INFO (self));

	adjust_resize (self);
	
	gtk_source_completion_utils_get_pos_at_cursor (GTK_WINDOW (self),
						       view,
						       &x,
						       &y,
						       &resized);
	if (resized)
	{
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->info_scroll),
						GTK_POLICY_ALWAYS,
						GTK_POLICY_ALWAYS);
	}
	else
	{
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->info_scroll),
						GTK_POLICY_NEVER,
						GTK_POLICY_NEVER);
	}
	gtk_window_move (GTK_WINDOW (self), x, y);
}

/**
 * gtk_source_completion_info_set_markup:
 * @self: The #GtkSourceCompletionInfo
 * @markup: Text markup to be shown (see <link 
 * linkend="PangoMarkupFormat">Pango markup format</link>). 
 *
 * Sets the text markup to be shown into the GtkSourceCompletionInfo window.
 *
 */
void
gtk_source_completion_info_set_markup (GtkSourceCompletionInfo* self,
				       const gchar* markup)
{
	g_return_if_fail  (GTK_IS_SOURCE_COMPLETION_INFO (self));

	gtk_label_set_markup (GTK_LABEL (self->priv->label), markup);
}

/**
 * gtk_source_completion_info_set_adjust_height:
 * @self: The #GtkSourceCompletionInfo
 * @adjust: TRUE to adjust height to content, FALSE to fixed height
 * @max_height: if adjust = TRUE, set the max height. -1 to preserve the 
 * current value
 *
 * %TRUE adjust height to the content. If the content is only a line, the info
 * will be small and if there are a lot of lines, the info will be large to the 
 * max_height
 *
 */
void
gtk_source_completion_info_set_adjust_height (GtkSourceCompletionInfo* self,
					      gboolean adjust,
					      gint max_height)
{
	g_return_if_fail  (GTK_IS_SOURCE_COMPLETION_INFO (self));

	self->priv->adjust_height = adjust;
	
	if (max_height > 0)
	{
		self->priv->max_height = max_height;
	}
}

/**
 * gtk_source_completion_info_set_adjust_width:
 * @self: The #GtkSourceCompletionInfo
 * @adjust: TRUE to adjust width to content, FALSE to fixed width
 * @max_width: if adjust = TRUE, set the max height. -1 to preserve the 
 * current value
 *
 * %TRUE adjust width to the content. If the content is only a line, the info
 * will be small and if there are a lot of lines, the info will be large to the 
 * max_width
 *
 */
void
gtk_source_completion_info_set_adjust_width (GtkSourceCompletionInfo* self,
					     gboolean adjust,
					     gint max_width)
{
	g_return_if_fail  (GTK_IS_SOURCE_COMPLETION_INFO (self));
	
	self->priv->adjust_width = adjust;
	
	if (max_width > 0)
	{
		self->priv->max_width = max_width;
	}
}

/**
 * gtk_source_completion_info_set_custom:
 * @self: The #GtkSourceCompletionInfo
 * @custom_widget: A #GtkWidget
 *
 * Replaces the widget packed into the window with custom_widget. By default a
 * box with a #GtkScrolledWindow and a #GtkLabel is embedded in the window.
 *
 */
void
gtk_source_completion_info_set_custom (GtkSourceCompletionInfo* self,
				       GtkWidget *custom_widget)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (self));
	
	if (self->priv->custom_widget == custom_widget)
		return;
		
	if (custom_widget)
	{
		g_return_if_fail (GTK_IS_WIDGET (custom_widget));
		
		if (self->priv->custom_widget)
		{
			gtk_container_remove (GTK_CONTAINER (self->priv->box),
					      self->priv->custom_widget);
			g_object_unref (self->priv->custom_widget);
			self->priv->custom_widget = NULL;
		}
		else
		{
			gtk_widget_hide (self->priv->info_scroll);
		}
		
		self->priv->custom_widget = g_object_ref (custom_widget);
		gtk_container_add (GTK_CONTAINER (self->priv->box),
				   custom_widget);
		gtk_widget_show (self->priv->custom_widget);
	}
	else
	{

		if (self->priv->custom_widget)
		{
			gtk_container_remove (GTK_CONTAINER (self->priv->box),
					      self->priv->custom_widget);
			g_object_unref (self->priv->custom_widget);
			self->priv->custom_widget = NULL;
			gtk_widget_show (self->priv->info_scroll);
		}
		
	}
}

/**
 * gtk_source_completion_info_get_custom:
 * @self: The #GtkSourceCompletionInfo
 *
 * Returns: The custom widget setted or NULL.
 *
 */
GtkWidget*
gtk_source_completion_info_get_custom (GtkSourceCompletionInfo* self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (self), NULL);

	return self->priv->custom_widget;
}




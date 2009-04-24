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

#ifndef MIN
#define MIN (a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX (a, b) ((a) > (b) ? (a) : (b))
#endif

struct _GtkSourceCompletionInfoPrivate
{
	GtkWidget *scroll;
	GtkWidget *widget;
	
	gint max_height;
	gint max_width;
	
	gboolean shrink_height;
	gboolean shrink_width;
	
	guint idle_resize;
	guint request_id;
};

/* Signals */
enum
{
	BEFORE_SHOW,
	LAST_SIGNAL
};

/* Properties */
enum
{
	PROP_0,
	PROP_MAX_WIDTH,
	PROP_MAX_HEIGHT,
	PROP_SHRINK_WIDTH,
	PROP_SHRINK_HEIGHT
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GtkSourceCompletionInfo, gtk_source_completion_info, GTK_TYPE_WINDOW);

#define GTK_SOURCE_COMPLETION_INFO_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GTK_TYPE_SOURCE_COMPLETION_INFO, GtkSourceCompletionInfoPrivate))

static void
get_scrolled_window_sizing (GtkSourceCompletionInfo *self,
                            gint                    *border,
                            gint                    *hscroll,
                            gint                    *vscroll)
{
	GtkWidget *scrollbar;

	*border = 0;
	*hscroll = 0;
	*vscroll = 0;

	if (self->priv->scroll != NULL)
	{
		*border = gtk_container_get_border_width (GTK_CONTAINER (self));
		
		scrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (self->priv->scroll));

		if (GTK_WIDGET_VISIBLE (scrollbar))
		{
			*hscroll = scrollbar->allocation.height;
		}

		scrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (self->priv->scroll));

		if (GTK_WIDGET_VISIBLE (scrollbar))
		{
			*vscroll = scrollbar->allocation.height;
		}
	}
}

static void
window_resize (GtkSourceCompletionInfo *self)
{
	GtkRequisition req;
	gint width;
	gint height;
	gint off;
	gint border;
	gint hscroll;
	gint vscroll;
	GtkStyle *style = GTK_WIDGET (self)->style;

	gtk_window_get_default_size (GTK_WINDOW (self), &width, &height);
	
	if (self->priv->widget != NULL)
	{
		/* Try to resize to fit widget, if necessary */
		gtk_widget_size_request (self->priv->widget, &req);
		
		get_scrolled_window_sizing (self, &border, &hscroll, &vscroll);
		off = (gtk_container_get_border_width (GTK_CONTAINER (self)) + border) * 2;

		if (self->priv->shrink_height)
		{
			if (self->priv->max_height == -1)
			{
				height = req.height + style->ythickness * 2;
			}
			else
			{
				height = MIN (req.height + style->ythickness * 2, self->priv->max_height);
			}
			
			height += off + hscroll;
		}
	
		if (self->priv->shrink_width)
		{
			if (self->priv->max_width == -1)
			{
				width = req.width + style->xthickness * 2;
			}
			else
			{
				width = MIN (req.width + style->xthickness * 2, self->priv->max_width);
			}
			
			width += off + vscroll;
		}
	}
	
	gtk_window_resize (GTK_WINDOW (self), width, height);
}

static void
gtk_source_completion_info_init (GtkSourceCompletionInfo *self)
{
	self->priv = GTK_SOURCE_COMPLETION_INFO_GET_PRIVATE (self);
	
	/* Tooltip style */
	gtk_window_set_title (GTK_WINDOW (self), _("Completion Info"));
	gtk_widget_set_name (GTK_WIDGET (self), "gtk-tooltip");
	gtk_widget_ensure_style (GTK_WIDGET (self));
	
	gtk_window_set_type_hint (GTK_WINDOW (self),
				  GDK_WINDOW_TYPE_HINT_NORMAL);

	gtk_window_set_default_size (GTK_WINDOW (self), 300, 200);
	gtk_container_set_border_width (GTK_CONTAINER (self), 1);
}

static gboolean
idle_resize (GtkSourceCompletionInfo *self)
{
	self->priv->idle_resize = 0;
	
	window_resize (self);
	return FALSE;
}

static void
queue_resize (GtkSourceCompletionInfo *self)
{
	if (self->priv->idle_resize == 0)
	{
		self->priv->idle_resize = g_idle_add ((GSourceFunc)idle_resize, self);
	}
}

static void
gtk_source_completion_info_get_property (GObject    *object, 
                                         guint       prop_id, 
                                         GValue     *value, 
                                         GParamSpec *pspec)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (object);
	
	switch (prop_id)
	{
		case PROP_MAX_WIDTH:
			g_value_set_int (value, self->priv->max_width);
			break;
		case PROP_MAX_HEIGHT:
			g_value_set_int (value, self->priv->max_height);
			break;
		case PROP_SHRINK_WIDTH:
			g_value_set_boolean (value, self->priv->shrink_width);
			break;
		case PROP_SHRINK_HEIGHT:
			g_value_set_boolean (value, self->priv->shrink_height);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_info_set_property (GObject      *object, 
                                         guint         prop_id, 
                                         const GValue *value, 
                                         GParamSpec   *pspec)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (object);
	
	switch (prop_id)
	{
		case PROP_MAX_WIDTH:
			self->priv->max_width = g_value_get_int (value);
			queue_resize (self);
			break;
		case PROP_MAX_HEIGHT:
			self->priv->max_height = g_value_get_int (value);
			queue_resize (self);
			break;
		case PROP_SHRINK_WIDTH:
			self->priv->shrink_width = g_value_get_boolean (value);
			queue_resize (self);
			break;
		case PROP_SHRINK_HEIGHT:
			self->priv->shrink_height = g_value_get_boolean (value);
			queue_resize (self);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_completion_info_finalize (GObject *object)
{
	GtkSourceCompletionInfo *self = GTK_SOURCE_COMPLETION_INFO (object);
	
	if (self->priv->idle_resize != 0)
	{
		g_source_remove (self->priv->idle_resize);
	}
	
	G_OBJECT_CLASS (gtk_source_completion_info_parent_class)->finalize (object);
}

static void
gtk_source_completion_info_show (GtkWidget *widget)
{
	/* First emit BEFORE_SHOW and then chain up */
	g_signal_emit (widget, signals[BEFORE_SHOW], 0);
	
	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->show (widget);	
}

static gboolean
gtk_source_completion_info_expose (GtkWidget      *widget,
                                   GdkEventExpose *expose)
{
	GTK_WIDGET_CLASS (gtk_source_completion_info_parent_class)->expose_event (widget, expose);

	gtk_paint_shadow (widget->style,
			  widget->window,
			  GTK_STATE_NORMAL,
			  GTK_SHADOW_OUT,
			  NULL,
			  widget,
			  NULL,
			  widget->allocation.x,
			  widget->allocation.y,
			  widget->allocation.width,
			  widget->allocation.height);

	return FALSE;
}

static void
gtk_source_completion_info_class_init (GtkSourceCompletionInfoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	
	object_class->get_property = gtk_source_completion_info_get_property;
	object_class->set_property = gtk_source_completion_info_set_property;
	object_class->finalize = gtk_source_completion_info_finalize;
	
	widget_class->show = gtk_source_completion_info_show;
	widget_class->expose_event = gtk_source_completion_info_expose;
	
	/**
	 * GtkSourceCompletionInfo::show-info:
	 * @info: The #GscInf who emits the signal
	 *
	 * This signal is emited before any "show" management. You can connect
	 * to this signal if you want to change some properties or position
	 * before to so the real "show".
	 **/
	signals[BEFORE_SHOW] =
		g_signal_new ("before-show",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);

	/* Properties */
	g_object_class_install_property (object_class,
					 PROP_MAX_WIDTH,
					 g_param_spec_int ("max-width",
							    _("Maximum width"),
							    _("The maximum allowed width"),
							    -1,
							    G_MAXINT,
							    -1,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_MAX_HEIGHT,
					 g_param_spec_int ("max-height",
							    _("Maximum height"),
							    _("The maximum allowed height"),
							    -1,
							    G_MAXINT,
							    -1,
							    G_PARAM_READWRITE));
							    

	g_object_class_install_property (object_class,
					 PROP_SHRINK_WIDTH,
					 g_param_spec_boolean ("shrink-width",
							       _("Shrink width"),
							       _("Whether the window should shrink width to fit the contents"),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SHRINK_HEIGHT,
					 g_param_spec_boolean ("shrink-height",
							       _("Shrink height"),
							       _("Whether the window should shrink height to fit the contents"),
							       FALSE,
							       G_PARAM_READWRITE));
						       
	g_type_class_add_private (object_class, sizeof (GtkSourceCompletionInfoPrivate));
}

/**
 * gtk_source_completion_info_new:
 *
 * Returns: The new GtkSourceCompletionInfo.
 *
 */
GtkSourceCompletionInfo *
gtk_source_completion_info_new (void)
{
	return g_object_new (GTK_TYPE_SOURCE_COMPLETION_INFO, 
	                     "type", GTK_WINDOW_POPUP, 
	                     NULL);
}

/**
 * gtk_source_completion_info_move_to_iter:
 * @self: The #GtkSourceCompletionInfo
 * @view: The current GtkTextView where we want to show the info
 * @iter: a #GtkTextIter
 *
 * Moves the #GtkSourceCompletionInfo to under the @iter. If it cannot be shown down,
 * it will be shown up. If @iter is %NULL @self is moved to the cursor position.
 *
 */
void
gtk_source_completion_info_move_to_iter (GtkSourceCompletionInfo *self,
					 GtkTextView             *view,
					 GtkTextIter             *iter)
{
	GtkTextBuffer *buffer;
	GtkTextMark *insert_mark;
	GtkTextIter start;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (self));
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));
	
	if (iter == NULL)
	{
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		insert_mark = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &start, insert_mark);
	}
	else
	{
		start = *iter;
	}
	
	gtk_source_completion_utils_move_to_iter (GTK_WINDOW (self),
						  GTK_SOURCE_VIEW (view),
						  &start);
}

/**
 * gtk_source_completion_info_set_sizing:
 * @self: The #GtkSourceCompletionInfo
 * @width: the maximum/requested width of the window (-1 to default)
 * @height: the maximum/requested height of the window (-1 to default)
 * @shrink_width: whether to shrink the width of the window to fit its contents
 * @shrink_height: whether to shrink the height of the window to fit its
 *                 contents
 *
 * Set sizing information for the info window. If @shrink_width or
 * @shrink_height is %TRUE, the info window will try to resize to fit the
 * window contents, with a maximum size given by @width and @height. Setting
 * @width or @height to -1 removes the maximum size of respectively the width
 * and height of the window.
 *
 */
void
gtk_source_completion_info_set_sizing (GtkSourceCompletionInfo *self,
				       gint                     width,
				       gint                     height,
				       gboolean                 shrink_width,
				       gboolean                 shrink_height)
{
	g_return_if_fail  (GTK_IS_SOURCE_COMPLETION_INFO (self));

	if (self->priv->max_width == width &&
	    self->priv->max_height == height &&
	    self->priv->shrink_width == shrink_width &&
	    self->priv->shrink_height == shrink_height)
	{
		return;
	}

	self->priv->max_width = width;
	self->priv->max_height = height;
	self->priv->shrink_width = shrink_width;
	self->priv->shrink_height = shrink_height;
	
	queue_resize (self);
}

static gboolean
needs_viewport (GtkWidget *widget)
{
	guint id;
	
	id = g_signal_lookup ("set-scroll-adjustments", G_TYPE_FROM_INSTANCE (widget));
	
	return id == 0;
}

static void
widget_size_request_cb (GtkWidget               *widget,
                        GtkRequisition          *requisition,
                        GtkSourceCompletionInfo *info)
{
	queue_resize (info);
}

static gboolean
use_scrolled_window (GtkSourceCompletionInfo *info,
                     GtkWidget               *widget)
{
	GtkRequisition req;
	
	if (!needs_viewport (widget))
	{
		return TRUE;
	}
	
	if (info->priv->max_width == -1 && info->priv->max_height == -1)
	{
		return FALSE;
	}
	
	gtk_widget_size_request (widget, &req);
	
	return (info->priv->max_width < req.width || info->priv->max_height < req.height);
}

static void
create_scrolled_window (GtkSourceCompletionInfo *info)
{
	/* Create scrolled window main widget */
	info->priv->scroll = gtk_scrolled_window_new (NULL, NULL);


	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (info->priv->scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (info->priv->scroll),
	                                     GTK_SHADOW_NONE);
	gtk_widget_show (info->priv->scroll);
	gtk_container_add (GTK_CONTAINER (info), info->priv->scroll);
}

/**
 * gtk_source_completion_info_set_widget:
 * @self: The #GtkSourceCompletionInfo
 * @widget: A #GtkWidget
 *
 * Sets the contents widget of the info window.
 *
 */
void
gtk_source_completion_info_set_widget (GtkSourceCompletionInfo *self,
				       GtkWidget               *widget)
{
	GtkWidget *child;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (self));
	g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

	if (self->priv->widget == widget)
	{
		return;
	}
	
	if (self->priv->widget != NULL)
	{
		g_signal_handler_disconnect (self->priv->widget, self->priv->request_id);
		
		gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (self->priv->widget)),
		                      self->priv->widget);

		if (self->priv->scroll != NULL)
		{
			gtk_widget_destroy (self->priv->scroll);
			self->priv->scroll = NULL;
		}
	}
	
	self->priv->widget = widget;
	
	if (widget != NULL)
	{
		/* Keep it alive */
		if (g_object_is_floating (widget))
		{
			g_object_ref (widget);
		}
		
		self->priv->request_id = g_signal_connect_after (widget, 
                                                                 "size-request", 
                                                                 G_CALLBACK (widget_size_request_cb), 
                                                                 self);
		
		/* See if it needs a viewport */
		if (use_scrolled_window (self, widget))
		{
			create_scrolled_window (self);
			child = widget;
			
			if (needs_viewport (widget))
			{
				child = gtk_viewport_new (NULL, NULL);
				gtk_viewport_set_shadow_type (GTK_VIEWPORT (child), GTK_SHADOW_NONE);
				gtk_widget_show (child);

				gtk_container_add (GTK_CONTAINER (child), widget);
			}
			
			gtk_container_add (GTK_CONTAINER (self->priv->scroll), child);
		}
		else
		{
			gtk_container_add (GTK_CONTAINER (self), widget);
		}
		
		gtk_widget_show (widget);
	}

	window_resize (self);
}

/**
 * gtk_source_completion_info_get_widget:
 * @self: The #GtkSourceCompletionInfo
 *
 * Get the current contents widget
 *
 * Returns: The current contents widget
 *
 */
GtkWidget *
gtk_source_completion_info_get_widget (GtkSourceCompletionInfo* self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_INFO (self), NULL);

	return self->priv->widget;
}




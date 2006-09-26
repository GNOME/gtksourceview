/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourceview.c
 *
 *  Copyright (C) 2001 - Mikael Hermansson <tyan@linux.se> and
 *  Chris Phelps <chicane@reninet.com>
 *
 *  Copyright (C) 2002 - Jeroen Zwartepoorte
 *
 *  Copyright (C) 2003 - Gustavo Giráldez and Paolo Maggi 
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

#include <string.h> /* For strlen */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-tabs.h>

#include "gtksourceview-i18n.h"

#include "gtksourceview-marshal.h"
#include "gtksourceview.h"

/*
#define ENABLE_DEBUG
*/
#undef ENABLE_DEBUG

/*
#define ENABLE_PROFILE
*/
#undef ENABLE_PROFILE

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifdef ENABLE_PROFILE
#define PROFILE(x) (x)
#else
#define PROFILE(x)
#endif

#define COMPOSITE_ALPHA                 225
#define GUTTER_PIXMAP 			16
#define DEFAULT_TAB_WIDTH 		8
#define MIN_NUMBER_WINDOW_WIDTH		20
#define MAX_TAB_WIDTH			32

#define DEFAULT_MARGIN			80
#define MAX_MARGIN			200

/* Signals */
enum {
	UNDO,
	REDO,
	LAST_SIGNAL
};

/* Properties */
enum {
	PROP_0,
	PROP_SHOW_LINE_NUMBERS,
	PROP_SHOW_LINE_MARKERS,
	PROP_TABS_WIDTH,
	PROP_AUTO_INDENT,
	PROP_INSERT_SPACES,
	PROP_SHOW_MARGIN,
	PROP_MARGIN,
	PROP_SMART_HOME_END,
	PROP_HIGHLIGHT_CURRENT_LINE,
	PROP_INDENT_ON_TAB
};

struct _GtkSourceViewPrivate
{
	guint		 tabs_width;
	gboolean 	 show_line_numbers;
	gboolean	 show_line_markers;
	gboolean	 auto_indent;
	gboolean	 insert_spaces;
	gboolean	 show_margin;
	gboolean	 highlight_current_line;
	gboolean	 indent_on_tab;
	guint		 margin;
	gint             cached_margin_width;
	gboolean	 smart_home_end;
	
	GHashTable 	*pixmap_cache;

	GtkSourceBuffer *source_buffer;
	gint		 old_lines;
};

/* Implement DnD for application/x-color drops */
typedef enum {
	TARGET_COLOR = 200
} GtkSourceViewDropTypes;

static const GtkTargetEntry drop_types[] = {
	{"application/x-color", 0, TARGET_COLOR}
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* Prototypes. */
static void	gtk_source_view_class_init 		(GtkSourceViewClass *klass);
static void	gtk_source_view_init 			(GtkSourceView      *view);
static void 	gtk_source_view_finalize 		(GObject            *object);

static void	gtk_source_view_undo 			(GtkSourceView      *view);
static void	gtk_source_view_redo 			(GtkSourceView      *view);

static void 	set_source_buffer 			(GtkSourceView      *view,
			       				 GtkTextBuffer      *buffer);

static void	gtk_source_view_populate_popup 		(GtkTextView        *view,
					    		 GtkMenu            *menu);

static void	gtk_source_view_move_cursor		(GtkTextView        *text_view,
							 GtkMovementStep     step,
							 gint                count,
							 gboolean            extend_selection);

static void 	menu_item_activate_cb 			(GtkWidget          *menu_item,
				  			 GtkTextView        *text_view);

static void 	gtk_source_view_get_lines 		(GtkTextView       *text_view,
				       			 gint               first_y,
				       			 gint               last_y,
				       			 GArray            *buffer_coords,
				       			 GArray            *numbers,
				       			 gint              *countp);
static gint     gtk_source_view_expose 			(GtkWidget         *widget,
							 GdkEventExpose    *event);
static gboolean	gtk_source_view_key_press_event		(GtkWidget         *widget,
							 GdkEventKey       *event);
static gboolean	gtk_source_view_button_press_event	(GtkWidget         *widget,
							 GdkEventButton    *event);
static void 	view_dnd_drop 				(GtkTextView       *view, 
							 GdkDragContext    *context,
							 gint               x,
							 gint               y,
							 GtkSelectionData  *selection_data,
							 guint              info,
							 guint              time,
							 gpointer           data);

static gint	calculate_real_tab_width 		(GtkSourceView     *view, 
							 guint              tab_size,
							 gchar              c);

static void	gtk_source_view_set_property 		(GObject           *object,
							 guint              prop_id,
							 const GValue      *value,
							 GParamSpec        *pspec);
static void	gtk_source_view_get_property		(GObject           *object,
							 guint              prop_id,
							 GValue            *value,
							 GParamSpec        *pspec);

static void     gtk_source_view_style_set               (GtkWidget         *widget,
							 GtkStyle          *previous_style);


/* Private functions. */
static void
gtk_source_view_class_init (GtkSourceViewClass *klass)
{
	GObjectClass	 *object_class;
	GtkTextViewClass *textview_class;
	GtkBindingSet    *binding_set;
	GtkWidgetClass   *widget_class;
	
	object_class 	= G_OBJECT_CLASS (klass);
	textview_class 	= GTK_TEXT_VIEW_CLASS (klass);
	parent_class 	= g_type_class_peek_parent (klass);
	widget_class 	= GTK_WIDGET_CLASS (klass);
	
	object_class->finalize = gtk_source_view_finalize;
	object_class->get_property = gtk_source_view_get_property;
	object_class->set_property = gtk_source_view_set_property;

	widget_class->key_press_event = gtk_source_view_key_press_event;
	widget_class->button_press_event = gtk_source_view_button_press_event;
	widget_class->expose_event = gtk_source_view_expose;
	widget_class->style_set = gtk_source_view_style_set;

	textview_class->populate_popup = gtk_source_view_populate_popup;
	textview_class->move_cursor = gtk_source_view_move_cursor;
	
	klass->undo = gtk_source_view_undo;
	klass->redo = gtk_source_view_redo;

	g_object_class_install_property (object_class,
					 PROP_SHOW_LINE_NUMBERS,
					 g_param_spec_boolean ("show_line_numbers",
							       _("Show Line Numbers"),
							       _("Whether to display line numbers"),
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_SHOW_LINE_MARKERS,
					 g_param_spec_boolean ("show_line_markers",
							       _("Show Line Markers"),
							       _("Whether to display line marker pixbufs"),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_TABS_WIDTH,
					 g_param_spec_uint ("tabs_width",
							    _("Tabs Width"),
							    _("Tabs Width"),
							    1,
							    MAX_TAB_WIDTH,
							    DEFAULT_TAB_WIDTH,
							    G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_AUTO_INDENT,
					 g_param_spec_boolean ("auto_indent",
							       _("Auto Indentation"),
							       _("Whether to enable auto indentation"),
							       FALSE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_INSERT_SPACES,
					 g_param_spec_boolean ("insert_spaces_instead_of_tabs",
							       _("Insert Spaces Instead of Tabs"),
							       _("Whether to insert spaces instead of tabs"),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SHOW_MARGIN,
					 g_param_spec_boolean ("show_margin",
							       _("Show Right Margin"),
							       _("Whether to display the right margin"),
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_MARGIN,
					 g_param_spec_uint ("margin",
							    _("Margin position"),
							    _("Position of the right margin"),
							    1,
							    MAX_MARGIN,
							    DEFAULT_MARGIN,
							    G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_SMART_HOME_END,
					 g_param_spec_boolean ("smart_home_end",
							       _("Use smart home/end"),
							       _("HOME and END keys move to first/last "
								 "non whitespace characters on line before going "
								 "to the start/end of the line"),
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_CURRENT_LINE,
					 g_param_spec_boolean ("highlight_current_line",
							       _("Highlight current line"),
							       _("Whether to highlight the current line"),
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_INDENT_ON_TAB,
					 g_param_spec_boolean ("indent_on_tab",
							       _("Indent on tab"),
							       _("Whether to indent the selected text when the tab key is pressed"),
							       FALSE,
							       G_PARAM_READWRITE));

	/**
	 * GtkSourceView:right-margin-line-alpha:
	 *
	 * The ::right-margin-line-alpha determines the alpha component with
	 * which the vertical line will be drawn. 0 means it is fully transparent
	 * (invisible). 255 means it has full opacity (text under the line won't
	 * be visible).
	 *
	 * Since: 1.6
	 */
	gtk_widget_class_install_style_property (widget_class,
		g_param_spec_int ("right-margin-line-alpha",
				  _("Margin Line Alpha"),
				  _("Transparency of the margin line"),
				  0,
				  255,
				  40,
				  G_PARAM_READABLE));

	/**
	 * GtkSourceView:right-margin-line-color:
	 *
	 * The ::right-margin-line-color property contains the color with
	 * which the right margin line (vertical line indicating the right
	 * margin) will be drawn.
	 *
	 * Since: 1.6
	 */
	gtk_widget_class_install_style_property (widget_class,
		g_param_spec_boxed ("right-margin-line-color",
				    _("Margin Line Color"),
				    _("Color to use for the right margin line"),
				    GDK_TYPE_COLOR,
				    G_PARAM_READABLE));

	/**
	 * GtkSourceView:right-margin-overlay-toggle:
	 *
	 * The ::right-margin-overlay-toggle property determines whether the
	 * widget will draw a transparent overlay on top of the area on the
	 * right side of the right margin line. On some systems, this has a
	 * noticable performance impact, so this property is FALSE by default.
	 *
	 * Since: 1.6
	 */
	gtk_widget_class_install_style_property (widget_class,
		g_param_spec_string ("right-margin-overlay-toggle",
				     _("Margin Overlay Toggle"),
				     _("Whether to draw the right margin overlay"),
				     "FALSE",
				     G_PARAM_READABLE));

	/**
	 * GtkSourceView:right-margin-overlay-alpha:
	 *
	 * The ::right-margin-overlay-alpha determines the alpha component with
	 * which the overlay will be drawn. 0 means it is fully transparent
	 * (invisible). 255 means it has full opacity (text under the overlay
	 * won't be visible).
	 *
	 * Since: 1.6
	 */
	gtk_widget_class_install_style_property (widget_class,
		g_param_spec_int ("right-margin-overlay-alpha",
				  _("Margin Overlay Alpha"),
				  _("Transparency of the margin overlay"),
				  0,
				  255,
				  15,
				  G_PARAM_READABLE));

	/**
	 * GtkSourceView:right-margin-overlay-color:
	 *
	 * The ::right-margin-overlay-color property contains the color with
	 * which the right margin overlay will be drawn. Setting this property
	 * will only have an effect if ::right-margin-overlay-toggle is TRUE.
	 *
	 * Since: 1.6
	 */
	gtk_widget_class_install_style_property (widget_class,
		g_param_spec_boxed ("right-margin-overlay-color",
				    _("Margin Overlay Color"),
				    _("Color to use for drawing the margin overlay"),
				    GDK_TYPE_COLOR,
				    G_PARAM_READABLE));

	signals [UNDO] =
		g_signal_new ("undo",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceViewClass, undo),
			      NULL,
			      NULL,
			      gtksourceview_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
	signals [REDO] =
		g_signal_new ("redo",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceViewClass, redo),
			      NULL,
			      NULL,
			      gtksourceview_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set,
				      GDK_z,
				      GDK_CONTROL_MASK,
				      "undo", 0);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_z,
				      GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				      "redo", 0);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_F14,
				      0,
				      "undo", 0);
}

static void 
gtk_source_view_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	GtkSourceView *view;
	
	g_return_if_fail (GTK_IS_SOURCE_VIEW (object));

	view = GTK_SOURCE_VIEW (object);
    
	switch (prop_id)
	{
		case PROP_SHOW_LINE_NUMBERS:
			gtk_source_view_set_show_line_numbers (view,
							       g_value_get_boolean (value));
			break;
			
		case PROP_SHOW_LINE_MARKERS:
			gtk_source_view_set_show_line_markers (view,
							       g_value_get_boolean (value));
			break;
			
		case PROP_TABS_WIDTH:
			gtk_source_view_set_tabs_width (view, 
							g_value_get_uint (value));
			break;
			
		case PROP_AUTO_INDENT:
			gtk_source_view_set_auto_indent (view,
							 g_value_get_boolean (value));
			break;
			
		case PROP_INSERT_SPACES:
			gtk_source_view_set_insert_spaces_instead_of_tabs (
							view,
							g_value_get_boolean (value));
			break;
			
		case PROP_SHOW_MARGIN:
			gtk_source_view_set_show_margin (view,
							 g_value_get_boolean (value));
			break;
			
		case PROP_MARGIN:
			gtk_source_view_set_margin (view, 
						    g_value_get_uint (value));
			break;
		
		case PROP_SMART_HOME_END:
			gtk_source_view_set_smart_home_end (view,
							    g_value_get_boolean (value));
			break;

		case PROP_HIGHLIGHT_CURRENT_LINE:
			gtk_source_view_set_highlight_current_line (view,
								    g_value_get_boolean (value));
			break;
		case PROP_INDENT_ON_TAB:
			gtk_source_view_set_indent_on_tab (view,
							   g_value_get_boolean (value));
			break;
		
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void 
gtk_source_view_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	GtkSourceView *view;
	
	g_return_if_fail (GTK_IS_SOURCE_VIEW (object));

	view = GTK_SOURCE_VIEW (object);
    
	switch (prop_id)
	{
		case PROP_SHOW_LINE_NUMBERS:
			g_value_set_boolean (value,
					     gtk_source_view_get_show_line_numbers (view));
					     
			break;
			
		case PROP_SHOW_LINE_MARKERS:
			g_value_set_boolean (value,
					     gtk_source_view_get_show_line_markers (view));

			break;
			
		case PROP_TABS_WIDTH:
			g_value_set_uint (value,
					  gtk_source_view_get_tabs_width (view));
			break;
			
		case PROP_AUTO_INDENT:
			g_value_set_boolean (value,
					     gtk_source_view_get_auto_indent (view));

			break;
			
		case PROP_INSERT_SPACES:
			g_value_set_boolean (value,
					     gtk_source_view_get_insert_spaces_instead_of_tabs (view));
	
			break;

		case PROP_SHOW_MARGIN:
			g_value_set_boolean (value,
					     gtk_source_view_get_show_margin (view));

			break;
			
		case PROP_MARGIN:
			g_value_set_uint (value,
					  gtk_source_view_get_margin (view));
			break;

		case PROP_SMART_HOME_END:
			g_value_set_boolean (value,
					     gtk_source_view_get_smart_home_end (view));
			break;
			
		case PROP_HIGHLIGHT_CURRENT_LINE:
			g_value_set_boolean (value,
					     gtk_source_view_get_highlight_current_line (view));
			break;

		case PROP_INDENT_ON_TAB:
			g_value_set_boolean (value,
					     gtk_source_view_get_indent_on_tab (view));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_view_init (GtkSourceView *view)
{
	GtkTargetList *tl;

	view->priv = g_new0 (GtkSourceViewPrivate, 1);

	view->priv->tabs_width = DEFAULT_TAB_WIDTH;
	view->priv->margin = DEFAULT_MARGIN;
	view->priv->cached_margin_width = -1;
	view->priv->smart_home_end = TRUE;
	
	view->priv->pixmap_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
							  (GDestroyNotify) g_free,
							  (GDestroyNotify) g_object_unref);

	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 2);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 2);

	tl = gtk_drag_dest_get_target_list (GTK_WIDGET (view));
	g_return_if_fail (tl != NULL);

	gtk_target_list_add_table (tl, drop_types, G_N_ELEMENTS (drop_types));

	g_signal_connect (G_OBJECT (view), 
			  "drag_data_received", 
			  G_CALLBACK (view_dnd_drop), 
			  NULL);
}

static void
gtk_source_view_finalize (GObject *object)
{
	GtkSourceView *view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (object));

	view = GTK_SOURCE_VIEW (object);

	if (view->priv->pixmap_cache) 
		g_hash_table_destroy (view->priv->pixmap_cache);
	
	set_source_buffer (view, NULL);

	g_free (view->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
highlight_updated_cb (GtkSourceBuffer *buffer,
		      GtkTextIter     *start,
		      GtkTextIter     *end,
		      GtkTextView     *text_view)
{
	GdkRectangle visible_rect;
	GdkRectangle updated_rect;	
	GdkRectangle redraw_rect;
	gint y;
	gint height;
	
	/* get visible area */
	gtk_text_view_get_visible_rect (text_view, &visible_rect);
	
	/* get updated rectangle */
	gtk_text_view_get_line_yrange (text_view, start, &y, &height);
	updated_rect.y = y;
	gtk_text_view_get_line_yrange (text_view, end, &y, &height);
	updated_rect.height = y + height - updated_rect.y;
	updated_rect.x = visible_rect.x;
	updated_rect.width = visible_rect.width;

	/* intersect both rectangles to see whether we need to queue a redraw */
	if (gdk_rectangle_intersect (&updated_rect, &visible_rect, &redraw_rect)) 
	{
		GdkRectangle widget_rect;
		
		gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_WIDGET,
						       redraw_rect.x,
						       redraw_rect.y,
						       &widget_rect.x,
						       &widget_rect.y);
		
		widget_rect.width = redraw_rect.width;
		widget_rect.height = redraw_rect.height;
		
		gtk_widget_queue_draw_area (GTK_WIDGET (text_view),
					    widget_rect.x,
					    widget_rect.y,
					    widget_rect.width,
					    widget_rect.height);
	}
}

static void 
marker_updated_cb (GtkSourceBuffer *buffer,
		   GtkTextIter     *where,
		   GtkTextView     *text_view)
{
	GdkRectangle visible_rect;
	GdkRectangle updated_rect;	
	GdkRectangle redraw_rect;
	gint y, height;
	
	g_return_if_fail (text_view != NULL && GTK_IS_SOURCE_VIEW (text_view));

	if (!GTK_SOURCE_VIEW (text_view)->priv->show_line_markers)
		return;
	
	/* get visible area */
	gtk_text_view_get_visible_rect (text_view, &visible_rect);
	
	/* get updated rectangle */
	gtk_text_view_get_line_yrange (text_view, where, &y, &height);
	updated_rect.y = y;
	updated_rect.height = height;
	updated_rect.x = visible_rect.x;
	updated_rect.width = visible_rect.width;

	/* intersect both rectangles to see whether we need to queue a redraw */
	if (gdk_rectangle_intersect (&updated_rect, &visible_rect, &redraw_rect)) 
	{
		gint y_win, width;
		
		gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_WIDGET,
						       0,
						       redraw_rect.y,
						       NULL,
						       &y_win);
		
		width = gtk_text_view_get_border_window_size (text_view,
							      GTK_TEXT_WINDOW_LEFT);
		
		gtk_widget_queue_draw_area (GTK_WIDGET (text_view),
					    0, y_win, width, height);
	}
}

static void
set_source_buffer (GtkSourceView *view, GtkTextBuffer *buffer)
{
	/* keep our pointer to the source buffer in sync with
	 * textview's, though it would be a lot nicer if GtkTextView
	 * had a "set_buffer" signal */
	/* FIXME: in gtk 2.3 we have a buffer property so we can
	 * connect to the notify signal.  Unfortunately we can't
	 * depend on gtk 2.3 yet (see bug #108353) */
	if (view->priv->source_buffer) 
	{
		g_signal_handlers_disconnect_by_func (view->priv->source_buffer,
						      highlight_updated_cb,
						      view);
		g_signal_handlers_disconnect_by_func (view->priv->source_buffer,
						      marker_updated_cb,
						      view);
		g_object_remove_weak_pointer (G_OBJECT (view->priv->source_buffer),
					      (gpointer *) &view->priv->source_buffer);
	}

	if (buffer && GTK_IS_SOURCE_BUFFER (buffer)) 
	{
		view->priv->source_buffer = GTK_SOURCE_BUFFER (buffer);
		g_object_add_weak_pointer (G_OBJECT (buffer),
					   (gpointer *) &view->priv->source_buffer);
		g_signal_connect (buffer,
				  "highlight_updated",
				  G_CALLBACK (highlight_updated_cb),
				  view);
		g_signal_connect (buffer,
				  "marker_updated",
				  G_CALLBACK (marker_updated_cb),
				  view);
	}
	else 
	{
		view->priv->source_buffer = NULL;
	}
}

static void
gtk_source_view_undo (GtkSourceView *view)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer)))
	{
		gtk_source_buffer_undo (GTK_SOURCE_BUFFER (buffer));
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
						    gtk_text_buffer_get_insert (buffer));
	}
}

static void
gtk_source_view_redo (GtkSourceView *view)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	if (gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer)))
	{
		gtk_source_buffer_redo (GTK_SOURCE_BUFFER (buffer));
		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
						    gtk_text_buffer_get_insert (buffer));
	}
}

static void
gtk_source_view_populate_popup (GtkTextView *text_view,
				GtkMenu     *menu)
{
	GtkTextBuffer *buffer;
	GtkWidget *menu_item;

	buffer = gtk_text_view_get_buffer (text_view);
	if (!buffer && !GTK_IS_SOURCE_BUFFER (buffer))
		return;

	/* separator */
	menu_item = gtk_menu_item_new ();
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_show (menu_item);

	/* create redo menu_item. */
	menu_item = gtk_image_menu_item_new_from_stock ("gtk-redo", NULL);
	g_object_set_data (G_OBJECT (menu_item), "gtk-signal", "redo");
	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (menu_item_activate_cb), text_view);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_set_sensitive (menu_item,
				  gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer)));
	gtk_widget_show (menu_item);

	/* create undo menu_item. */
	menu_item = gtk_image_menu_item_new_from_stock ("gtk-undo", NULL);
	g_object_set_data (G_OBJECT (menu_item), "gtk-signal", "undo");
	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (menu_item_activate_cb), text_view);
	gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
	gtk_widget_set_sensitive (menu_item, 
				  gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer)));
	gtk_widget_show (menu_item);

}

static void
move_cursor (GtkTextView       *text_view,
	     const GtkTextIter *new_location,
	     gboolean           extend_selection)
{
	GtkTextBuffer *buffer = text_view->buffer;

	if (extend_selection)
		gtk_text_buffer_move_mark_by_name (buffer, "insert",
						   new_location);
	else
		gtk_text_buffer_place_cursor (buffer, new_location);

	gtk_text_view_scroll_mark_onscreen (text_view,
					    gtk_text_buffer_get_insert (buffer));
}

static void
gtk_source_view_move_cursor (GtkTextView    *text_view,
			     GtkMovementStep step,
			     gint            count,
			     gboolean        extend_selection)
{
	GtkSourceView *source_view = GTK_SOURCE_VIEW (text_view);
	GtkTextBuffer *buffer = text_view->buffer;
	GtkTextMark *mark;
	GtkTextIter cur, iter;

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &cur, mark);
	iter = cur;

	if (step == GTK_MOVEMENT_DISPLAY_LINE_ENDS &&
	    source_view->priv->smart_home_end && count == -1)
	{
		/* Find the iter of the first character on the line. */
		gtk_text_iter_set_line_offset (&cur, 0);
		while (!gtk_text_iter_ends_line (&cur))
		{
			gunichar c = gtk_text_iter_get_char (&cur);
			if (g_unichar_isspace (c))
				gtk_text_iter_forward_char (&cur);
			else
				break;
		}

		if (gtk_text_iter_starts_line (&iter) ||
		    !gtk_text_iter_equal (&cur, &iter))
		{
			move_cursor (text_view, &cur, extend_selection);
		}
		else
		{
			gtk_text_iter_set_line_offset (&cur, 0);
			move_cursor (text_view, &cur, extend_selection);
		}
	}
	else if (step == GTK_MOVEMENT_DISPLAY_LINE_ENDS &&
		 source_view->priv->smart_home_end && count == 1)
	{
		/* Find the iter of the last character on the line. */
		if (!gtk_text_iter_ends_line (&cur))
			gtk_text_iter_forward_to_line_end (&cur);
		while (!gtk_text_iter_starts_line (&cur))
		{
			gunichar c;
			gtk_text_iter_backward_char (&cur);
			c = gtk_text_iter_get_char (&cur);
			if (!g_unichar_isspace (c))
			{
				/* We've gone one character too far. */
				gtk_text_iter_forward_char (&cur);
				break;
			}
		}

		if (gtk_text_iter_ends_line (&iter) ||
		    !gtk_text_iter_equal (&cur, &iter))
		{
			move_cursor (text_view, &cur, extend_selection);
		}
		else
		{
			gtk_text_iter_forward_to_line_end (&cur);
			move_cursor (text_view, &cur, extend_selection);
		}
	}
	else
	{
		GTK_TEXT_VIEW_CLASS (parent_class)->move_cursor (text_view,
								 step, count,
								 extend_selection);
	}
}

static void
menu_item_activate_cb (GtkWidget   *menu_item,
		       GtkTextView *text_view)
{
	const gchar *signal;

	signal = g_object_get_data (G_OBJECT (menu_item), "gtk-signal");
	g_signal_emit_by_name (G_OBJECT (text_view), signal);
}

/* This function is taken from gtk+/tests/testtext.c */
static void
gtk_source_view_get_lines (GtkTextView  *text_view,
			   gint          first_y,
			   gint          last_y,
			   GArray       *buffer_coords,
			   GArray       *numbers,
			   gint         *countp)
{
	GtkTextIter iter;
	gint count;
	gint size;
      	gint last_line_num = -1;	

	g_array_set_size (buffer_coords, 0);
	g_array_set_size (numbers, 0);
  
	/* Get iter at first y */
	gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

	/* For each iter, get its location and add it to the arrays.
	 * Stop when we pass last_y
	*/
	count = 0;
  	size = 0;

  	while (!gtk_text_iter_is_end (&iter))
    	{
		gint y, height;
      
		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (buffer_coords, y);
		last_line_num = gtk_text_iter_get_line (&iter);
		g_array_append_val (numbers, last_line_num);
      	
		++count;

		if ((y + height) >= last_y)
			break;
      
		gtk_text_iter_forward_line (&iter);
	}

	if (gtk_text_iter_is_end (&iter))
    	{
		gint y, height;
		gint line_num;
      
		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		line_num = gtk_text_iter_get_line (&iter);

		if (line_num != last_line_num)
		{
			g_array_append_val (buffer_coords, y);
			g_array_append_val (numbers, line_num);
			++count;
		}
	}

	*countp = count;
}

static GSList * 
draw_line_markers (GtkSourceView *view,
		   GSList        *current_marker,
		   gint          *line_number,
		   gint           x,
		   gint           y)
{
	GdkPixbuf *pixbuf, *composite;
	GtkSourceMarker *marker;
	gint width, height;
	gint next_line;
	gchar *marker_type;
	
	g_assert (current_marker);
	
	composite = NULL;
	width = height = 0;

	/* composite all the pixbufs for the markers present at the line */
	do
	{
		marker = current_marker->data;
		
		next_line = gtk_source_marker_get_line (marker);
		if (next_line != *line_number)
			break;
		
		marker_type = gtk_source_marker_get_marker_type (marker);
		pixbuf = gtk_source_view_get_marker_pixbuf (view, marker_type);
		
		if (pixbuf)
		{
			if (!composite)
			{
				composite = gdk_pixbuf_copy (pixbuf);
				width = gdk_pixbuf_get_width (composite);
				height = gdk_pixbuf_get_height (composite);
			}
			else
			{
				gint pixbuf_w;
				gint pixbuf_h;

				pixbuf_w = gdk_pixbuf_get_width (pixbuf);
				pixbuf_h = gdk_pixbuf_get_height (pixbuf);
				gdk_pixbuf_composite (pixbuf,
						      composite,
						      0, 0,
						      width, height,
						      0, 0,
						      (double) pixbuf_w / width,
						      (double) pixbuf_h / height,
						      GDK_INTERP_BILINEAR,
						      COMPOSITE_ALPHA);
			}
			g_object_unref (pixbuf);
			
		} else
			g_warning ("Unknown marker '%s' used", marker_type);

		g_free (marker_type);
		current_marker = g_slist_next (current_marker);
	}
	while (current_marker);
	
	*line_number = next_line;
	
	/* render the result to the left window */
	if (composite)
	{
		GdkWindow *window;

		window = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
						   GTK_TEXT_WINDOW_LEFT);

		gdk_draw_pixbuf (GDK_DRAWABLE (window), NULL, composite,
				 0, 0, x, y,
				 width, height,
				 GDK_RGB_DITHER_NORMAL, 0, 0);
		g_object_unref (composite);
	}

	return current_marker;
}

static void
gtk_source_view_paint_margin (GtkSourceView *view,
			      GdkEventExpose *event)
{
	GtkTextView *text_view;
	GdkWindow *win;
	PangoLayout *layout;
	GArray *numbers;
	GArray *pixels;
	GSList *markers, *current_marker;
	gint marker_line = 0;
	gchar str [8];  /* we don't expect more than ten million lines ;-) */
	gint y1, y2;
	gint count;
	gint margin_width;
	gint text_width, x_pixmap;
	gint i;
	GtkTextIter cur;
	gint cur_line;

	text_view = GTK_TEXT_VIEW (view);

	if (!view->priv->show_line_numbers && !view->priv->show_line_markers)
	{
		gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (text_view),
						      GTK_TEXT_WINDOW_LEFT,
						      0);

		return;
	}

	win = gtk_text_view_get_window (text_view,
					GTK_TEXT_WINDOW_LEFT);

	y1 = event->area.y;
	y2 = y1 + event->area.height;

	/* get the extents of the line printing */
	gtk_text_view_window_to_buffer_coords (text_view,
					       GTK_TEXT_WINDOW_LEFT,
					       0,
					       y1,
					       NULL,
					       &y1);

	gtk_text_view_window_to_buffer_coords (text_view,
					       GTK_TEXT_WINDOW_LEFT,
					       0,
					       y2,
					       NULL,
					       &y2);

	numbers = g_array_new (FALSE, FALSE, sizeof (gint));
	pixels = g_array_new (FALSE, FALSE, sizeof (gint));

	/* get the line numbers and y coordinates. */
	gtk_source_view_get_lines (text_view,
				   y1,
				   y2,
				   pixels,
				   numbers,
				   &count);

	/* A zero-lined document should display a "1"; we don't need to worry about
	scrolling effects of the text widget in this special case */
	
	if (count == 0)
	{
		gint y = 0;
		gint n = 0;
		count = 1;
		g_array_append_val (pixels, y);
		g_array_append_val (numbers, n);
	}

	DEBUG ({
		g_message ("Painting line numbers %d - %d",
			   g_array_index (numbers, gint, 0),
			   g_array_index (numbers, gint, count - 1));
	});
	
	/* set size. */
	g_snprintf (str, sizeof (str),
		    "%d", MAX (99, gtk_text_buffer_get_line_count (text_view->buffer)));
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), str);

	pango_layout_get_pixel_size (layout, &text_width, NULL);
	
	pango_layout_set_width (layout, text_width);
	pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

	/* determine the width of the left margin. */
	if (view->priv->show_line_numbers)
		margin_width = text_width + 4;
	else
		margin_width = 0;

	x_pixmap = margin_width;
	
	if (view->priv->show_line_markers)
		margin_width += GUTTER_PIXMAP;

	g_return_if_fail (margin_width != 0);
	
	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (text_view),
					      GTK_TEXT_WINDOW_LEFT,
					      margin_width);

	/* get markers for the exposed region */
	markers = NULL;
	if (view->priv->source_buffer && view->priv->show_line_markers)
	{
		GtkTextIter begin, end;

		/* get markers in the exposed area */
		gtk_text_buffer_get_iter_at_line (text_view->buffer,
						  &begin,
						  g_array_index (numbers, gint, 0));
		gtk_text_buffer_get_iter_at_line (text_view->buffer,
						  &end,
						  g_array_index (numbers, gint, count - 1));
		if (!gtk_text_iter_ends_line (&end))
			gtk_text_iter_forward_to_line_end (&end);

		markers = gtk_source_buffer_get_markers_in_region (
			view->priv->source_buffer, &begin, &end);

		DEBUG ({
			g_message ("Painting markers for lines %d - %d",
				   gtk_text_iter_get_line (&begin),
				   gtk_text_iter_get_line (&end));
		});
	}
	
	i = 0;
	current_marker = markers;
	if (current_marker)
		marker_line = gtk_source_marker_get_line (
			GTK_SOURCE_MARKER (current_marker->data));
	
	
	gtk_text_buffer_get_iter_at_mark (text_view->buffer, 
					  &cur, 
					  gtk_text_buffer_get_insert (text_view->buffer));

	cur_line = gtk_text_iter_get_line (&cur) + 1;
	
	while (i < count) 
	{
		gint pos;
		
		gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_LEFT,
						       0,
						       g_array_index (pixels, gint, i),
						       NULL,
						       &pos);

		if (view->priv->show_line_numbers) 
		{
			gint line_to_paint = g_array_index (numbers, gint, i) + 1;
			
			if (line_to_paint == cur_line)
			{
				gchar *markup;
				markup = g_strdup_printf ("<b>%d</b>", line_to_paint);
				
				pango_layout_set_markup (layout, markup, -1);

				g_free (markup);
			}
			else
			{
				g_snprintf (str, sizeof (str),
					    "%d", line_to_paint);

				pango_layout_set_markup (layout, str, -1);
			}

			gtk_paint_layout (GTK_WIDGET (view)->style,
					  win,
					  GTK_WIDGET_STATE (view),
					  FALSE,
					  NULL,
					  GTK_WIDGET (view),
					  NULL,
					  text_width + 2, 
					  pos,
					  layout);
		}

		if (view->priv->show_line_markers && current_marker) 
		{
			if (marker_line == g_array_index (numbers, gint, i))
			{
				/* draw markers for the line */
				current_marker = draw_line_markers (view,
								    current_marker,
								    &marker_line,
								    x_pixmap,
								    pos);
			}
		}

		++i;
	}

	/* we should have used all markers */
	g_assert (current_marker == NULL);
	
	g_slist_free (markers);
	g_array_free (pixels, TRUE);
	g_array_free (numbers, TRUE);

	g_object_unref (G_OBJECT (layout));
}

static gint
gtk_source_view_expose (GtkWidget      *widget,
			GdkEventExpose *event)
{
	GtkSourceView *view;
	GtkTextView *text_view;
	gboolean event_handled;
	
	view = GTK_SOURCE_VIEW (widget);
	text_view = GTK_TEXT_VIEW (widget);

	event_handled = FALSE;
	
	/* maintain the our source_buffer pointer synchronized */
	if (text_view->buffer != GTK_TEXT_BUFFER (view->priv->source_buffer) &&
	    GTK_IS_SOURCE_BUFFER (text_view->buffer)) 
	{
		set_source_buffer (view, text_view->buffer);
	}
	
	/* check if the expose event is for the text window first, and
	 * make sure the visible region is highlighted */
	if (event->window == gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT) &&
	    view->priv->source_buffer != NULL) 
	{
		GdkRectangle visible_rect;
		GtkTextIter iter1, iter2;
		
		gtk_text_view_get_visible_rect (text_view, &visible_rect);
		gtk_text_view_get_line_at_y (text_view, &iter1,
					     visible_rect.y, NULL);
		gtk_text_iter_backward_line (&iter1);
		gtk_text_view_get_line_at_y (text_view, &iter2,
					     visible_rect.y
					     + visible_rect.height, NULL);
		gtk_text_iter_forward_line (&iter2);

		_gtk_source_buffer_highlight_region (view->priv->source_buffer,
						     &iter1, &iter2, FALSE);
	}

	/* now check for the left window, which contains the margin */
	if (event->window == gtk_text_view_get_window (text_view,
						       GTK_TEXT_WINDOW_LEFT)) 
	{
		gtk_source_view_paint_margin (view, event);
		event_handled = TRUE;
	} 
	else 
	{
		gint lines;

		/* FIXME: could it be a performances problem? - Paolo */
		lines = gtk_text_buffer_get_line_count (text_view->buffer);

		if (view->priv->old_lines != lines)
		{
			GdkWindow *w;
			view->priv->old_lines = lines;

			w = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_LEFT);

			if (w != NULL)
				gdk_window_invalidate_rect (w, NULL, FALSE);
		}

		if (view->priv->highlight_current_line && 
		    (event->window == gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT)))
		{
			GdkRectangle visible_rect;
			GdkRectangle redraw_rect;
			GtkTextIter cur;
			gint y;
			gint height;
			gint win_y;
			
			gtk_text_buffer_get_iter_at_mark (text_view->buffer, 
							  &cur, 
							  gtk_text_buffer_get_insert (text_view->buffer));

			gtk_text_view_get_line_yrange (text_view, &cur, &y, &height);
							
			gtk_text_view_get_visible_rect (text_view, &visible_rect);
			
			gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_TEXT,
						       visible_rect.x,
						       visible_rect.y,
						       &redraw_rect.x,
						       &redraw_rect.y);

			gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_TEXT,
						       0,
						       y,
						       NULL,
						       &win_y);

			redraw_rect.width = visible_rect.width;
			redraw_rect.height = visible_rect.height;

			gdk_draw_rectangle (event->window,
					    widget->style->bg_gc[GTK_WIDGET_STATE (widget)],
					    TRUE,
					    redraw_rect.x + MAX (0, gtk_text_view_get_left_margin (text_view) - 1),
					    win_y,
					    redraw_rect.width,
					    height);
		}
		
		/* Have GtkTextView draw the text first. */
		if (GTK_WIDGET_CLASS (parent_class)->expose_event)
			event_handled = 
				(* GTK_WIDGET_CLASS (parent_class)->expose_event)
				(widget, event);

		/* Draw the right margin vertical line + overlay. */
		if (view->priv->show_margin && 
		    (event->window == gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT)))
		{
			GdkRectangle visible_rect;
			GdkRectangle redraw_rect;
			cairo_t *cr;
			double x;
			GdkColor *line_color;
			gchar *toggle;
			guchar alpha;

#ifdef ENABLE_PROFILE
			static GTimer *timer = NULL;
#endif

			if (view->priv->cached_margin_width < 0)
				view->priv->cached_margin_width =
					calculate_real_tab_width (view, view->priv->margin, '_');

#ifdef ENABLE_PROFILE
			if (timer == NULL)
				timer = g_timer_new ();
				
			g_timer_start (timer);
#endif 
		
			gtk_text_view_get_visible_rect (text_view, &visible_rect);
			
			gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_TEXT,
						       visible_rect.x,
						       visible_rect.y,
						       &redraw_rect.x,
						       &redraw_rect.y);
			
			redraw_rect.width = visible_rect.width;
			redraw_rect.height = visible_rect.height;

			cr = gdk_cairo_create (gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_TEXT));
			
			/* Set a clip region for the expose event. */
			cairo_rectangle (cr, event->area.x, event->area.y,
					 event->area.width, event->area.height);
			cairo_clip (cr);

			/* Offset with 0.5 is needed for a sharp line. */
			x = view->priv->cached_margin_width -
				visible_rect.x + redraw_rect.x + 0.5 +
				gtk_text_view_get_left_margin (text_view);

			/* Default line width is 2.0 which is too wide. */
			cairo_set_line_width (cr, 1.0);

			cairo_move_to (cr, x, redraw_rect.y);
			cairo_line_to (cr, x, redraw_rect.y + redraw_rect.height);
			
			gtk_widget_style_get (widget,
					      "right-margin-line-alpha", &alpha,
					      "right-margin-line-color", &line_color,
					      "right-margin-overlay-toggle", &toggle,
					      NULL);

			if (!line_color)
				line_color = gdk_color_copy (&widget->style->text[GTK_STATE_NORMAL]);
			
			cairo_set_source_rgba (cr,
					       line_color->red / 65535.,
					       line_color->green / 65535.,
					       line_color->blue / 65535.,
					       alpha / 255.);
			gdk_color_free (line_color);
			cairo_stroke (cr);
			
			/* g_strstrip doesn't allocate a new string. */
			toggle = g_strstrip (toggle);
			
			/* Only draw the overlay when the theme explicitly permits it. */
			if (g_ascii_strcasecmp ("TRUE", toggle) == 0 ||
			    strcmp ("1", toggle) == 0)
			{
				GdkColor *overlay_color;
				
				gtk_widget_style_get (widget,
						      "right-margin-overlay-alpha", &alpha,
						      "right-margin-overlay-color", &overlay_color,
						      NULL);

				if (!overlay_color)
					overlay_color = gdk_color_copy (&widget->style->text[GTK_STATE_NORMAL]);

				/* Draw the rectangle next to the line (x+.5). */
				cairo_rectangle (cr,
						 x + .5,
						 redraw_rect.y,
						 redraw_rect.width - x - .5,
						 redraw_rect.y + redraw_rect.height);
				cairo_set_source_rgba (cr,
						       overlay_color->red / 65535.,
						       overlay_color->green / 65535.,
						       overlay_color->blue / 65535.,
						       alpha / 255.);
				gdk_color_free (overlay_color);
				cairo_fill (cr);
			}

			g_free (toggle);
			cairo_destroy (cr);
			
			PROFILE ({
				g_timer_stop (timer);
				g_message ("Time to draw the margin: %g (sec * 1000)", g_timer_elapsed (timer, NULL) * 1000);
			});
		}
	}
	
	return event_handled;	
}

/*
 *This is a pretty important function...we call it when the tab_stop is changed,
 *And when the font is changed.
 *NOTE: You must change this with the default font for now...
 *It may be a good idea to set the tab_width for each GtkTextTag as well
 *based on the font that we set at creation time
 *something like style_cache_set_tabs_from_font or the like.
 *Now, this *may* not be necessary because most tabs wont be inside of another highlight,
 *except for things like multi-line comments (which will usually have an italic font which
 *may or may not be a different size than the standard one), or if some random language
 *definition decides that it would be spiffy to have a bg color for "start of line" whitespace
 *"^\(\t\| \)+" would probably do the trick for that.
 */
static gint
calculate_real_tab_width (GtkSourceView *view, guint tab_size, gchar c)
{
	PangoLayout *layout;
	gchar *tab_string;
	gint tab_width = 0;

	if (tab_size == 0)
		return -1;

	tab_string = g_strnfill (tab_size, c);
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (view), tab_string);
	g_free (tab_string);

	if (layout != NULL) {
		pango_layout_get_pixel_size (layout, &tab_width, NULL);
		g_object_unref (G_OBJECT (layout));
	} else
		tab_width = -1;

	return tab_width;
}


/* ----------------------------------------------------------------------
 * Public interface 
 * ---------------------------------------------------------------------- */

/**
 * gtk_source_view_new:
 *
 * Creates a new #GtkSourceView. An empty default buffer will be
 * created for you. If you want to specify your own buffer, consider
 * gtk_source_view_new_with_buffer().
 *
 * Return value: a new #GtkSourceView
 **/
GtkWidget *
gtk_source_view_new ()
{
	GtkWidget *widget;
	GtkSourceBuffer *buffer;

	buffer = gtk_source_buffer_new (NULL);
	widget = gtk_source_view_new_with_buffer (buffer);
	g_object_unref (buffer);
	return widget;
}

/**
 * gtk_source_view_new_with_buffer:
 * @buffer: a #GtkSourceBuffer.
 *
 * Creates a new #GtkSourceView widget displaying the buffer
 * @buffer. One buffer can be shared among many widgets.
 *
 * Return value: a new #GtkTextView.
 **/
GtkWidget *
gtk_source_view_new_with_buffer (GtkSourceBuffer *buffer)
{
	GtkWidget *view;

	g_return_val_if_fail (buffer != NULL && GTK_IS_SOURCE_BUFFER (buffer), NULL);
	
	view = g_object_new (GTK_TYPE_SOURCE_VIEW, NULL);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), GTK_TEXT_BUFFER (buffer));

	return view;
}

GType
gtk_source_view_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_source_view_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceView),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_view_init
		};

		our_type = g_type_register_static (GTK_TYPE_TEXT_VIEW,
						   "GtkSourceView",
						   &our_info, 0);
	}

	return our_type;
}

/**
 * gtk_source_view_get_show_line_numbers:
 * @view: a #GtkSourceView.
 *
 * Returns whether line numbers are displayed beside the text.
 *
 * Return value: %TRUE if the line numbers are displayed.
 **/
gboolean
gtk_source_view_get_show_line_numbers (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->show_line_numbers;
}

/**
 * gtk_source_view_set_show_line_numbers:
 * @view: a #GtkSourceView.
 * @show: whether line numbers should be displayed.
 *
 * If %TRUE line numbers will be displayed beside the text.
 *
 **/
void
gtk_source_view_set_show_line_numbers (GtkSourceView *view,
				       gboolean       show)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	show = (show != FALSE);

	if (show) 
	{
		if (!view->priv->show_line_numbers) 
		{
			/* Set left margin to minimum width if no margin is 
			   visible yet. Otherwise, just queue a redraw, so the
			   expose handler will automatically adjust the margin. */
			if (!view->priv->show_line_markers)
				gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
								      GTK_TEXT_WINDOW_LEFT,
								      MIN_NUMBER_WINDOW_WIDTH);
			else
				gtk_widget_queue_draw (GTK_WIDGET (view));

			view->priv->show_line_numbers = show;

			g_object_notify (G_OBJECT (view), "show_line_numbers");
		}
	}
	else 
	{
		if (view->priv->show_line_numbers) 
		{
			view->priv->show_line_numbers = show;

			/* force expose event, which will adjust margin. */
			gtk_widget_queue_draw (GTK_WIDGET (view));

			g_object_notify (G_OBJECT (view), "show_line_numbers");
		}
	}
}

/**
 * gtk_source_view_get_show_line_markers:
 * @view: a #GtkSourceView.
 *
 * Returns whether line markers are displayed beside the text.
 *
 * Return value: %TRUE if the line markers are displayed.
 **/
gboolean
gtk_source_view_get_show_line_markers (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->show_line_markers;
}

/**
 * gtk_source_view_set_show_line_markers:
 * @view: a #GtkSourceView.
 * @show: whether line markers should be displayed.
 *
 * If %TRUE line markers will be displayed beside the text.
 *
 **/
void
gtk_source_view_set_show_line_markers (GtkSourceView *view,
				       gboolean       show)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	show = (show != FALSE);

	if (show) 
	{
		if (!view->priv->show_line_markers) 
		{
			/* Set left margin to minimum width if no margin is 
			   visible yet. Otherwise, just queue a redraw, so the
			   expose handler will automatically adjust the margin. */
			if (!view->priv->show_line_numbers)
				gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
								      GTK_TEXT_WINDOW_LEFT,
								      MIN_NUMBER_WINDOW_WIDTH);
			else
				gtk_widget_queue_draw (GTK_WIDGET (view));

			view->priv->show_line_markers = show;

			g_object_notify (G_OBJECT (view), "show_line_markers");
		}
	} 
	else 
	{
		if (view->priv->show_line_markers) 
		{
			view->priv->show_line_markers = show;

			/* force expose event, which will adjust margin. */
			gtk_widget_queue_draw (GTK_WIDGET (view));

			g_object_notify (G_OBJECT (view), "show_line_markers");
		}
	}
}

/**
 * gtk_source_view_get_tabs_width:
 * @view: a #GtkSourceView.
 *
 * Returns the width of tabulation in characters.
 *
 * Return value: width of tab.
 **/
guint
gtk_source_view_get_tabs_width (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->tabs_width;
}

static gboolean
set_tab_stops_internal (GtkSourceView *view)
{
	PangoTabArray *tab_array;
	gint real_tab_width;

	real_tab_width = calculate_real_tab_width (view, view->priv->tabs_width, ' ');

	if (real_tab_width < 0)
		return FALSE;
	
	tab_array = pango_tab_array_new (1, TRUE);
	pango_tab_array_set_tab (tab_array, 0, PANGO_TAB_LEFT, real_tab_width);

	gtk_text_view_set_tabs (GTK_TEXT_VIEW (view), 
				tab_array);

	pango_tab_array_free (tab_array);

	return TRUE;
}

/**
 * gtk_source_view_set_tabs_width:
 * @view: a #GtkSourceView.
 * @width: width of tab in characters.
 *
 * Sets the width of tabulation in characters.
 *
 **/
void
gtk_source_view_set_tabs_width (GtkSourceView *view,
				guint          width)
{
	guint save_width;
	
	g_return_if_fail (GTK_SOURCE_VIEW (view));
	g_return_if_fail (width <= MAX_TAB_WIDTH);
	g_return_if_fail (width > 0);

	if (view->priv->tabs_width == width)
		return;
	
	gtk_widget_ensure_style (GTK_WIDGET (view));
	
	save_width = view->priv->tabs_width;
	view->priv->tabs_width = width;
	if (set_tab_stops_internal (view))
	{
		g_object_notify (G_OBJECT (view), "tabs_width");
	}
	else
	{
		g_warning ("Impossible to set tabs width.");
		view->priv->tabs_width = save_width;
	}
}

/**
 * gtk_source_view_set_marker_pixbuf:
 * @view: a #GtkSourceView.
 * @marker_type: a marker type.
 * @pixbuf: a #GdkPixbuf.
 *
 * Associates a given @pixbuf with a given @marker_type.
 **/
void
gtk_source_view_set_marker_pixbuf (GtkSourceView *view,
				   const gchar   *marker_type,
				   GdkPixbuf     *pixbuf)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));
	g_return_if_fail (marker_type != NULL);
	g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));
	
	if (pixbuf)
	{
		gint width;
		gint height;

		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);
		if (width > GUTTER_PIXMAP || height > GUTTER_PIXMAP)
		{
			if (width > GUTTER_PIXMAP)
				width = GUTTER_PIXMAP;
			if (height > GUTTER_PIXMAP)
				height = GUTTER_PIXMAP;
			pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height,
							  GDK_INTERP_BILINEAR);
		}
		else
		{
			/* we own a reference of the pixbuf */
			g_object_ref (G_OBJECT (pixbuf));
		}
		
		g_hash_table_insert (view->priv->pixmap_cache,
				     g_strdup (marker_type),
				     pixbuf);
	}
	else
	{
		g_hash_table_remove (view->priv->pixmap_cache, marker_type);
	}
}

/**
 * gtk_source_view_get_marker_pixbuf:
 * @view: a #GtkSourceView.
 * @marker_type: a marker type. 
 *
 * Gets the pixbuf which is associated with the given @marker_type.
 *
 * Return value: a #GdkPixbuf if found, or %NULL if not found.
 **/
GdkPixbuf * 
gtk_source_view_get_marker_pixbuf (GtkSourceView *view,
				   const gchar   *marker_type)
{
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (view != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), NULL);
	g_return_val_if_fail (marker_type != NULL, NULL);
	
	pixbuf = g_hash_table_lookup (view->priv->pixmap_cache, marker_type);

	if (pixbuf)
		g_object_ref (pixbuf);
	
	return pixbuf;
}

static gchar *
compute_indentation (GtkSourceView *view, 
		     GtkTextIter   *cur)
{
	GtkTextIter start;
	GtkTextIter end;

	gunichar ch;
	gint line;

	line = gtk_text_iter_get_line (cur);

	gtk_text_buffer_get_iter_at_line (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), 
					  &start, 
					  line);

	end = start;

	ch = gtk_text_iter_get_char (&end);

	while (g_unichar_isspace (ch) && 
	       (ch != '\n') && 
	       (ch != '\r') && 
	       (gtk_text_iter_compare (&end, cur) < 0))
	{
		if (!gtk_text_iter_forward_char (&end))
			break;

		ch = gtk_text_iter_get_char (&end);
	}

	if (gtk_text_iter_equal (&start, &end))
		return NULL;

	return gtk_text_iter_get_slice (&start, &end);
}

static void
indent_lines (GtkSourceView *view, GtkTextIter *start, GtkTextIter *end)
{
	GtkTextBuffer *buf;
	gint start_line, end_line;
	gint i;
	gchar *tab_buffer = NULL;

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	start_line = gtk_text_iter_get_line (start);
	end_line = gtk_text_iter_get_line (end);

	if ((gtk_text_iter_get_visible_line_offset (end) == 0) &&
	    (end_line > start_line))
	{
		end_line--;
	}

	if (gtk_source_view_get_insert_spaces_instead_of_tabs (view))
	{
		gint tabs_size;

		tabs_size = view->priv->tabs_width;
		tab_buffer = g_strnfill (tabs_size, ' ');
	}
	else
	{
		tab_buffer = g_strdup ("\t");
	}

	gtk_text_buffer_begin_user_action (buf);

	for (i = start_line; i <= end_line; i++)
	{
		GtkTextIter iter;

		gtk_text_buffer_get_iter_at_line (buf, &iter, i);

		/* don't add indentation on empty lines */
		if (gtk_text_iter_ends_line (&iter))
			continue;

		gtk_text_buffer_insert (buf, &iter, tab_buffer, -1);
	}

	gtk_text_buffer_end_user_action (buf);

	g_free (tab_buffer);

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
					    gtk_text_buffer_get_insert (buf));
}

static void
unindent_lines (GtkSourceView *view, GtkTextIter *start, GtkTextIter *end)
{
	GtkTextBuffer *buf;
	gint start_line, end_line;
	gint i;

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	start_line = gtk_text_iter_get_line (start);
	end_line = gtk_text_iter_get_line (end);

	if ((gtk_text_iter_get_visible_line_offset (end) == 0) &&
	    (end_line > start_line))
	{
		end_line--;
	}

	gtk_text_buffer_begin_user_action (buf);

	for (i = start_line; i <= end_line; i++)
	{
		GtkTextIter iter, iter2;

		gtk_text_buffer_get_iter_at_line (buf, &iter, i);

		if (gtk_text_iter_get_char (&iter) == '\t')
		{
			iter2 = iter;
			gtk_text_iter_forward_char (&iter2);
			gtk_text_buffer_delete (buf, &iter, &iter2);
		}
		else if (gtk_text_iter_get_char (&iter) == ' ')
		{
			gint spaces = 0;

			iter2 = iter;

			while (!gtk_text_iter_ends_line (&iter2))
			{
				if (gtk_text_iter_get_char (&iter2) == ' ')
					spaces++;
				else
					break;

				gtk_text_iter_forward_char (&iter2);
			}

			if (spaces > 0)
			{
				gint tabs = 0;
				gint tabs_size = view->priv->tabs_width;

				tabs = spaces / tabs_size;
				spaces = spaces - (tabs * tabs_size);

				if (spaces == 0)
					spaces = tabs_size;

				iter2 = iter;

				gtk_text_iter_forward_chars (&iter2, spaces);
				gtk_text_buffer_delete (buf, &iter, &iter2);
			}
		}
	}

	gtk_text_buffer_end_user_action (buf);

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view),
					    gtk_text_buffer_get_insert (buf));
}

static void
insert_tab_or_spaces (GtkSourceView *view, GtkTextIter *start, GtkTextIter *end)
{
	GtkTextBuffer *buf;
	gchar *tab_buf;

	if (view->priv->insert_spaces)
	{
		gint tabs_size;
		GtkTextIter iter;
		gint cur_pos;
		gint tab_pos;
		gint num_of_equivalent_spaces;

		tabs_size = view->priv->tabs_width; 

		iter = *start;

		cur_pos = gtk_text_iter_get_line_offset (start);
		tab_pos = cur_pos;

		/* If there are tabs in the line take them into account */
		while (tab_pos > 0)
		{
			gunichar c;

			gtk_text_iter_backward_char (&iter);
			c = gtk_text_iter_get_char (&iter);
			if (c == '\t')
				break;
			tab_pos--;
		}

		num_of_equivalent_spaces = tabs_size - (cur_pos - tab_pos) % tabs_size; 

		tab_buf = g_strnfill (num_of_equivalent_spaces, ' ');
	}
	else
	{
		tab_buf = g_strdup ("\t");
	}

	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	gtk_text_buffer_begin_user_action (buf);
	gtk_text_buffer_delete (buf, start, end);
	gtk_text_buffer_insert (buf, start, tab_buf, -1);
	gtk_text_buffer_end_user_action (buf);

	g_free (tab_buf);
}

static gboolean
gtk_source_view_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	GtkSourceView *view;
	GtkTextBuffer *buf;
	GtkTextIter cur;
	GtkTextMark *mark;
	guint modifiers;
	gint key;

	view = GTK_SOURCE_VIEW (widget);
	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

	/* Be careful when testing for modifier state equality:
	 * caps lock, num lock,etc need to be taken into account */
	modifiers = gtk_accelerator_get_default_mod_mask ();

	key = event->keyval;

	mark = gtk_text_buffer_get_insert (buf);
	gtk_text_buffer_get_iter_at_mark (buf, &cur, mark);

	if ((key == GDK_Return || key == GDK_KP_Enter) &&
	    !(event->state & GDK_SHIFT_MASK) &&
	    view->priv->auto_indent)
	{
		/* Auto-indent means that when you press ENTER at the end of a
		 * line, the new line is automatically indented at the same
		 * level as the previous line.
		 * SHIFT+ENTER allows to avoid autoindentation.
		 */
		gchar *indent = NULL;

		/* Calculate line indentation and create indent string. */
		indent = compute_indentation (view, &cur);

		if (indent != NULL)
		{
			/* Allow input methods to internally handle a key press event. 
			 * If this function returns TRUE, then no further processing should be done 
			 * for this keystroke. */
			if (gtk_im_context_filter_keypress (GTK_TEXT_VIEW(view)->im_context, event))
				return TRUE;

			/* If an input method has inserted some test while handling the key press event,
			 * the cur iterm may be invalid, so get the iter again */
			gtk_text_buffer_get_iter_at_mark (buf, &cur, mark);
	
			/* Insert new line and auto-indent. */
			gtk_text_buffer_begin_user_action (buf);
			gtk_text_buffer_insert (buf, &cur, "\n", 1);
			gtk_text_buffer_insert (buf, &cur, indent, strlen (indent));
			g_free (indent);
			gtk_text_buffer_end_user_action (buf);
			gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (widget),
							    mark);
			return TRUE;
		}
	}

	/* if tab or shift+tab:
	 * with shift+tab key is GDK_ISO_Left_Tab (depends on X?)
	 */
	if ((key == GDK_Tab || key == GDK_KP_Tab || key == GDK_ISO_Left_Tab) &&
	    ((event->state & modifiers) == 0 ||
	     (event->state & modifiers) == GDK_SHIFT_MASK))
	{
		GtkTextIter s, e;
		gboolean has_selection;

		has_selection = gtk_text_buffer_get_selection_bounds (buf, &s, &e);

		if (view->priv->indent_on_tab)
		{
			/* shift+tab: always unindent */
			if (event->state & GDK_SHIFT_MASK)
			{
				unindent_lines (view, &s, &e);
				return TRUE;
			}

			/* tab: if we have a selection which spans one whole line
			 * or more, we mass indent, if the selection spans less then
			 * the full line just replace the text with \t
			 */
			if (has_selection &&
			    ((gtk_text_iter_starts_line (&s) && gtk_text_iter_ends_line (&e)) ||
			     (gtk_text_iter_get_line (&s) != gtk_text_iter_get_line (&e))))
			{
				indent_lines (view, &s, &e);
				return TRUE;
			}
		}
 
		insert_tab_or_spaces (view, &s, &e);
 		return TRUE;
	}

	return (* GTK_WIDGET_CLASS (parent_class)->key_press_event) (widget, event);
}

static void
extend_selection_to_line (GtkTextBuffer *buf, GtkTextIter *line_start)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter line_end;

	gtk_text_buffer_get_selection_bounds (buf, &start, &end);

	line_end = *line_start;
	gtk_text_iter_forward_to_line_end (&line_end);

	if (gtk_text_iter_compare (&start, line_start) < 0)
	{
		gtk_text_buffer_select_range (buf, &start, &line_end);
	}
	else if (gtk_text_iter_compare (&end, &line_end) < 0)
	{
		/* if the selection is in this line, extend
		 * the selection to the whole line */
		gtk_text_buffer_select_range (buf, &line_end, line_start);
	}
	else
	{
		gtk_text_buffer_select_range (buf, &end, line_start);
	}
}

static void
select_line (GtkTextBuffer *buf, GtkTextIter *line_start)
{
	GtkTextIter iter;

	iter = *line_start;

	if (!gtk_text_iter_ends_line (&iter))
		gtk_text_iter_forward_to_line_end (&iter);

	/* Select the line, put the cursor at the end of the line */
	gtk_text_buffer_select_range (buf, &iter, line_start);
}

static gboolean
gtk_source_view_button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	GtkSourceView *view;
	GtkTextBuffer *buf;
	int y_buf;
	GtkTextIter line_start;

	view = GTK_SOURCE_VIEW (widget);
	buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget));

	if (view->priv->show_line_numbers &&
	    (event->window == gtk_text_view_get_window (GTK_TEXT_VIEW (view),
						       GTK_TEXT_WINDOW_LEFT)))
	{
		gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
						       GTK_TEXT_WINDOW_LEFT,
						       event->x, event->y,
						       NULL, &y_buf);

		gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view),
					     &line_start,
					     y_buf,
					     NULL);
	
		if (event->type == GDK_BUTTON_PRESS && (event->button == 1))
		{
			if ((event->state & GDK_CONTROL_MASK) != 0)
			{
				/* Single click + Ctrl -> select the line */
				select_line (buf, &line_start);
			}
			else if ((event->state & GDK_SHIFT_MASK) != 0)
			{
				/* Single click + Shift -> extended current
				   selection to include the clicked line */
				extend_selection_to_line (buf, &line_start);
			}
			else
			{
				gtk_text_buffer_place_cursor (buf, &line_start);
			}
		}
		else if (event->type == GDK_2BUTTON_PRESS && (event->button == 1))
		{
			select_line (buf, &line_start);
		}

		/* consume the event also on right click etc */
		return TRUE;
	}

	return GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);
}

/**
 * gtk_source_view_get_auto_indent:
 * @view: a #GtkSourceView.
 *
 * Returns whether auto indentation of text is enabled.
 *
 * Return value: %TRUE if auto indentation is enabled.
 **/
gboolean
gtk_source_view_get_auto_indent (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->auto_indent;
}

/**
 * gtk_source_view_set_auto_indent:
 * @view: a #GtkSourceView.
 * @enable: whether to enable auto indentation.
 *
 * If %TRUE auto indentation of text is enabled.
 *
 **/
void
gtk_source_view_set_auto_indent (GtkSourceView *view, gboolean enable)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	enable = (enable != FALSE);

	if (view->priv->auto_indent == enable)
		return;

	view->priv->auto_indent = enable;

	g_object_notify (G_OBJECT (view), "auto_indent");
}

/**
 * gtk_source_view_get_insert_spaces_instead_of_tabs:
 * @view: a #GtkSourceView.
 *
 * Returns whether when inserting a tabulator character it should
 * be replaced by a group of space characters.
 *
 * Return value: %TRUE if spaces are inserted instead of tabs.
 **/
gboolean
gtk_source_view_get_insert_spaces_instead_of_tabs (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->insert_spaces;
}

/**
 * gtk_source_view_set_insert_spaces_instead_of_tabs:
 * @view: a #GtkSourceView.
 * @enable: whether to insert spaces instead of tabs.
 *
 * If %TRUE any tabulator character inserted is replaced by a group
 * of space characters.
 *
 **/
void
gtk_source_view_set_insert_spaces_instead_of_tabs (GtkSourceView *view, gboolean enable)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	enable = (enable != FALSE);
	
	if (view->priv->insert_spaces == enable)
		return;
	
	view->priv->insert_spaces = enable;

	g_object_notify (G_OBJECT (view), "insert_spaces_instead_of_tabs");
}

/**
 * gtk_source_view_get_indent_on_tab:
 * @view: a #GtkSourceView.
 *
 * Returns whether when the tab key is pressed the current selection
 * should get indented instead of replaced with the \t character.
 *
 * Return value: %TRUE if the selection is indented when tab is pressed.
 *
 * Since: 1.8
 **/
gboolean
gtk_source_view_get_indent_on_tab (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->indent_on_tab;
}

/**
 * gtk_source_view_set_indent_on_tab:
 * @view: a #GtkSourceView.
 * @enable: whether to indent a block when tab is pressed.
 *
 * If %TRUE, when the tab key is pressed and there is a selection, the
 * selected text is indented of one level instead of being replaced with
 * the \t characters. Shift+Tab unindents the selection.
 *
 * Since: 1.8
 **/
void
gtk_source_view_set_indent_on_tab (GtkSourceView *view, gboolean enable)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	enable = (enable != FALSE);

	if (view->priv->indent_on_tab == enable)
		return;

	view->priv->indent_on_tab = enable;

	g_object_notify (G_OBJECT (view), "indent_on_tab");
}

static void 
view_dnd_drop (GtkTextView *view, 
	       GdkDragContext *context,
	       gint x,
	       gint y,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint time,
	       gpointer data)
{

	GtkTextIter iter;

	if (info == TARGET_COLOR) 
	{
		guint16 *vals;
		gchar string[] = "#000000";
		gint buffer_x;
		gint buffer_y;
		
		if (selection_data->length < 0)
			return;

		if ((selection_data->format != 16) || (selection_data->length != 8)) 
		{
			g_warning ("Received invalid color data\n");
			return;
		}

		vals = (guint16 *) selection_data->data;

		vals[0] /= 256;
	        vals[1] /= 256;
		vals[2] /= 256;
		
		g_snprintf (string, sizeof (string), "#%02X%02X%02X", vals[0], vals[1], vals[2]);
		
		gtk_text_view_window_to_buffer_coords (view, 
						       GTK_TEXT_WINDOW_TEXT, 
						       x, 
						       y, 
						       &buffer_x, 
						       &buffer_y);
		gtk_text_view_get_iter_at_location (view, &iter, buffer_x, buffer_y);

		if (gtk_text_view_get_editable (view))
		{
			gtk_text_buffer_insert (gtk_text_view_get_buffer (view), 
						&iter, 
						string, 
						strlen (string));
			gtk_text_buffer_place_cursor (gtk_text_view_get_buffer (view),
						&iter);
		}

		/*
		 * FIXME: Check if the iter is inside a selection
		 * If it is, remove the selection and then insert at
		 * the cursor position - Paolo
		 */

		return;
	}
}

/**
 * gtk_source_view_get_show_margin:
 * @view: a #GtkSourceView.
 *
 * Returns whether a margin is displayed.
 *
 * Return value: %TRUE if the margin is showed.
 **/
gboolean 
gtk_source_view_get_show_margin (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->show_margin;
}

/**
 * gtk_source_view_set_show_margin:
 * @view: a #GtkSourceView.
 * @show: whether to show a margin.
 *
 * If %TRUE a margin is displayed
 **/
void 
gtk_source_view_set_show_margin (GtkSourceView *view, gboolean show)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	show = (show != FALSE);

	if (view->priv->show_margin == show)
		return;

	view->priv->show_margin = show;

	gtk_widget_queue_draw (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "show_margin");
}

/**
 * gtk_source_view_get_highlight_current_line:
 * @view: a #GtkSourceView
 *
 * Returns whether the current line is highlighted
 *
 * Return value: TRUE if the current line is highlighted
 **/
gboolean 
gtk_source_view_get_highlight_current_line (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->highlight_current_line;
}

/**
 * gtk_source_view_set_highlight_current_line:
 * @view: a #GtkSourceView
 * @show: whether to highlight the current line
 *
 * If TRUE the current line is highlighted
 **/
void 
gtk_source_view_set_highlight_current_line (GtkSourceView *view, gboolean hl)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	hl = (hl != FALSE);

	if (view->priv->highlight_current_line == hl)
		return;

	view->priv->highlight_current_line = hl;

	gtk_widget_queue_draw (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "highlight_current_line");
}

/**
 * gtk_source_view_get_margin:
 * @view: a #GtkSourceView.
 *
 * Gets the position of the right margin in the given @view.
 *
 * Return value: the position of the right margin.
 **/
guint
gtk_source_view_get_margin  (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), DEFAULT_MARGIN);

	return view->priv->margin;

}

/**
 * gtk_source_view_set_margin:
 * @view: a #GtkSourceView.
 * @margin: the position of the margin to set.
 *
 * Sets the position of the right margin in the given @view.
 *
 **/
void 
gtk_source_view_set_margin (GtkSourceView *view, guint margin)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));
	g_return_if_fail (margin >= 1);
	g_return_if_fail (margin <= MAX_MARGIN);

	if (view->priv->margin == margin)
		return;

	view->priv->margin = margin;
	view->priv->cached_margin_width = -1;

	gtk_widget_queue_draw (GTK_WIDGET (view));

	g_object_notify (G_OBJECT (view), "margin");
}

/**
 * gtk_source_view_set_smart_home_end:
 * @view: a #GtkSourceView.
 * @enable: whether to enable smart behavior for HOME and END keys.
 *
 * If %TRUE HOME and END keys will move to the first/last non-space
 * character of the line before moving to the start/end.
 *
 **/
void
gtk_source_view_set_smart_home_end (GtkSourceView *view, gboolean enable)
{
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	enable = (enable != FALSE);

	if (view->priv->smart_home_end == enable)
		return;

	view->priv->smart_home_end = enable;

	g_object_notify (G_OBJECT (view), "smart_home_end");
}

/**
 * gtk_source_view_get_smart_home_end:
 * @view: a #GtkSourceView.
 *
 * Returns whether HOME and END keys will move to the first/last non-space
 * character of the line before moving to the start/end.
 *
 * Return value: %TRUE if smart behavior for HOME and END keys is enabled.
 **/
gboolean
gtk_source_view_get_smart_home_end (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->priv->smart_home_end;
}

/**
 * gtk_source_view_style_set:
 * @widget: a #GtkSourceView.
 * @previous_style:
 *
 *
 **/
static void 
gtk_source_view_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	GtkSourceView *view;
	
	g_return_if_fail (GTK_IS_SOURCE_VIEW (widget));
	
	/* call default handler first */
	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);

	view = GTK_SOURCE_VIEW (widget);
	if (previous_style)
	{
		/* If previous_style is NULL this is the initial
		 * emission and we can't set the tab array since the
		 * text view doesn't have a default style yet */
		
		/* re-set tab stops */
		set_tab_stops_internal (view);
		/* make sure the margin width is recalculated on next expose */
		view->priv->cached_margin_width = -1;
	}
}


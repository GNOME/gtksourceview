/*  gtksourceview.c
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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
 *  You should have received a copy of the GNU Library General Public License*  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango-tabs.h>

#include "gtksourceview.h"

#define GUTTER_PIXMAP 16
#define TAB_STOP 4
#define MIN_NUMBER_WINDOW_WIDTH 20

enum {
	UNDO,
	REDO,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* Prototypes. */
static void gtk_source_view_class_init (GtkSourceViewClass *klass);
static void gtk_source_view_init (GtkSourceView *view);
static void gtk_source_view_finalize (GObject *object);

static void gtk_source_view_pixbuf_foreach_unref (gpointer key,
						  gpointer value,
						  gpointer user_data);

static void gtk_source_view_undo (GtkSourceView *view);
static void gtk_source_view_redo (GtkSourceView *view);

static void gtk_source_view_populate_popup (GtkTextView *view,
					    GtkMenu     *menu);
static void menuitem_activate_cb (GtkWidget   *menuitem,
				  GtkTextView *text_view);

static void gtk_source_view_draw_line_markers (GtkSourceView *view,
					       gint           line,
					       gint           x,
					       gint           y);
static GdkPixbuf *gtk_source_view_get_line_marker (GtkSourceView *view,
						   GList *list);

static void gtk_source_view_get_lines (GtkTextView  *text_view,
				       gint          first_y,
				       gint          last_y,
				       GArray       *buffer_coords,
				       GArray       *numbers,
				       gint         *countp);
static gint gtk_source_view_expose (GtkWidget      *widget,
				    GdkEventExpose *event,
				    gpointer        user_data);


static gint gtk_source_view_calculate_tab_stop_width (GtkWidget *widget,
						      gint       tab_stop);


/* Private functions. */
static void
gtk_source_view_class_init (GtkSourceViewClass *klass)
{
	GObjectClass   *object_class;
	GtkTextViewClass *textview_class;
	GtkBindingSet    *binding_set;

	object_class = (GObjectClass *) klass;
	textview_class = GTK_TEXT_VIEW_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = gtk_source_view_finalize;
	textview_class->populate_popup = gtk_source_view_populate_popup;
	klass->undo = gtk_source_view_undo;
	klass->redo = gtk_source_view_redo;

	signals[UNDO] =
		gtk_signal_new ("undo",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GtkSourceViewClass, undo),
				gtk_marshal_VOID__VOID, GTK_TYPE_NONE, 0);
	signals[REDO] =
		gtk_signal_new ("redo",
				GTK_RUN_LAST | GTK_RUN_ACTION,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GtkSourceViewClass, redo),
				gtk_marshal_VOID__VOID, GTK_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_z,
				      GDK_CONTROL_MASK,
				      "undo", 0);
	gtk_binding_entry_add_signal (binding_set,
				      GDK_r,
				      GDK_CONTROL_MASK,
				      "redo", 0);
}

static void
gtk_source_view_init (GtkSourceView *view)
{
	view->pixmap_cache = g_hash_table_new (g_str_hash, g_str_equal);

	gtk_source_view_set_show_line_numbers (view, TRUE);
	gtk_source_view_set_show_line_pixmaps (view, TRUE);
}

static void
gtk_source_view_finalize (GObject *object)
{
	GtkSourceView *view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (object));

	view = GTK_SOURCE_VIEW (object);

	if (view->pixmap_cache) {
		g_hash_table_foreach_remove (view->pixmap_cache,
					     (GHRFunc) gtk_source_view_pixbuf_foreach_unref,
					     NULL);
		g_hash_table_destroy (view->pixmap_cache);
	}
}

static void
gtk_source_view_pixbuf_foreach_unref (gpointer key,
				      gpointer value,
				      gpointer user_data)
{
	g_object_unref (G_OBJECT (value));
}

static void
gtk_source_view_undo (GtkSourceView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	gtk_source_buffer_undo (GTK_SOURCE_BUFFER
				(gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))));
}

static void
gtk_source_view_redo (GtkSourceView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	gtk_source_buffer_redo (GTK_SOURCE_BUFFER
				(gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))));
}

static void
gtk_source_view_populate_popup (GtkTextView *text_view,
				GtkMenu     *menu)
{
	GtkTextBuffer *buffer;
	GtkWidget *menuitem;

	buffer = gtk_text_view_get_buffer (text_view);
	if (!buffer && !GTK_IS_SOURCE_BUFFER (buffer))
		return;

	/* what is this for? */
	menuitem = gtk_menu_item_new ();
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 3);
	gtk_widget_show (menuitem);

	/* create undo menuitem. */
	menuitem = gtk_menu_item_new_with_label ("Undo");
	g_object_set_data (G_OBJECT (menuitem), "gtk-signal", "undo");
	g_signal_connect (G_OBJECT (menuitem),
			  "activate",
			  G_CALLBACK (menuitem_activate_cb),
			  text_view);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 4);
	gtk_widget_set_sensitive (menuitem,
				  gtk_source_buffer_can_undo (GTK_SOURCE_BUFFER (buffer)));
	gtk_widget_show (menuitem);

	/* create redo menuitem. */
	menuitem = gtk_menu_item_new_with_label ("Redo");
	g_object_set_data (G_OBJECT (menuitem), "gtk-signal", "redo");
	g_signal_connect (G_OBJECT (menuitem),
			  "activate",
			  G_CALLBACK (menuitem_activate_cb),
			  text_view);
	gtk_menu_shell_insert (GTK_MENU_SHELL (menu), menuitem, 5);
	gtk_widget_set_sensitive (menuitem,
				  gtk_source_buffer_can_redo (GTK_SOURCE_BUFFER (buffer)));
	gtk_widget_show (menuitem);
}

static void
menuitem_activate_cb (GtkWidget   *menuitem,
		      GtkTextView *text_view)
{
	const gchar *signal;

	signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
	g_signal_emit_by_name (G_OBJECT (text_view), signal);
}

static GdkPixbuf *
gtk_source_view_get_line_marker (GtkSourceView *view,
				 GList         *list)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *composite;
	GList *iter;

	pixbuf = gtk_source_view_get_pixbuf (view, (const gchar *) list->data);
	if (!pixbuf) {
		g_warning ("Unknown marker '%s' used.", (char*)list->data);
		return NULL;
	}

	if (!list->next)
		g_object_ref (pixbuf);
	else {
		pixbuf = gdk_pixbuf_copy (pixbuf);
		for (iter = list->next; iter; iter = iter->next) {
			composite = gtk_source_view_get_pixbuf (view,
								(const gchar *) iter->data);
			if (composite) {
				gint width;
				gint height;
				gint comp_width;
				gint comp_height;

				width = gdk_pixbuf_get_width (pixbuf);
				height = gdk_pixbuf_get_height (pixbuf);
				comp_width = gdk_pixbuf_get_width (composite);
				comp_height = gdk_pixbuf_get_height (composite);
				gdk_pixbuf_composite ((const GdkPixbuf *) composite,
						      pixbuf,
						      0, 0,
						      width, height,
						      0, 0,
						      width / comp_width,
						      height / comp_height,
						      GDK_INTERP_BILINEAR,
						      225);
			} else
				g_warning ("Unknown marker '%s' used", (char*)iter->data);
		}
	}

	return pixbuf;
}

static void
gtk_source_view_draw_line_markers (GtkSourceView *view,
				   gint           line,
				   gint           x,
				   gint           y)
{
	GList *list;
	GdkPixbuf *pixbuf;
	GdkWindow *win = gtk_text_view_get_window (GTK_TEXT_VIEW (view),
						   GTK_TEXT_WINDOW_LEFT);

	list = (GList *)
		gtk_source_buffer_line_get_markers (GTK_SOURCE_BUFFER
						    (GTK_TEXT_VIEW (view)->buffer), line);
	if (list) {
		if ((pixbuf = gtk_source_view_get_line_marker (view, list))) {
			gdk_pixbuf_render_to_drawable_alpha (pixbuf, GDK_DRAWABLE (win), 0, 0,
							     x, y,
							     gdk_pixbuf_get_width (pixbuf),
							     gdk_pixbuf_get_height (pixbuf),
							     GDK_PIXBUF_ALPHA_BILEVEL,
							     127, GDK_RGB_DITHER_NORMAL, 0, 0);
			g_object_unref (pixbuf);
		}
	}
}

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
	gint last_line_num;

	g_array_set_size (buffer_coords, 0);
	g_array_set_size (numbers, 0);

	/* get iter at first y */
	gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

	/* For each iter, get its location and add it to the arrays.
	 * Stop when we pass last_y. */
	count = 0;
	size = 0;

	while (!gtk_text_iter_is_end (&iter)) {
		gint y, height;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (buffer_coords, y);
		last_line_num = gtk_text_iter_get_line (&iter);
		g_array_append_val (numbers, last_line_num);

		++count;

		if ((y + height) > last_y)
			break;

		gtk_text_iter_forward_line (&iter);
	}

	if (gtk_text_iter_is_end (&iter)) {
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

static gint
gtk_source_view_expose (GtkWidget      *widget,
			GdkEventExpose *event,
			gpointer        user_data)
{
	GtkSourceView *view;
	GtkTextView *text_view;
	GdkWindow *win;
	PangoLayout *layout;
	GArray *numbers;
	GArray *pixels;
	gchar *str;
	gint y1;
	gint y2;
	gint count;
	gint margin_width;
	gint text_width;
	gint i;

	view = GTK_SOURCE_VIEW (user_data);
	text_view = GTK_TEXT_VIEW (widget);

	win = gtk_text_view_get_window (text_view,
					GTK_TEXT_WINDOW_LEFT);

	if (event->window != win)
		return FALSE;

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

	layout = gtk_widget_create_pango_layout (widget, "");

	/* set size. */
	str = g_strdup_printf ("%d", MAX (999,
					  gtk_text_buffer_get_line_count (text_view->buffer)));
	pango_layout_set_text (layout, str, -1);
	g_free (str);

	pango_layout_get_pixel_size (layout, &text_width, NULL);
	pango_layout_set_width (layout, text_width);
	pango_layout_set_alignment (layout, PANGO_ALIGN_RIGHT);

	/* determine the width of the left margin. */
	if (view->show_line_numbers && view->show_line_pixmaps)
		margin_width = text_width + 4 + GUTTER_PIXMAP;
	else if (view->show_line_numbers)
		margin_width = text_width + 4;
	else if (view->show_line_pixmaps)
		margin_width = GUTTER_PIXMAP;
	else
		margin_width = 0;

	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (text_view),
					      GTK_TEXT_WINDOW_LEFT,
					      margin_width);

	if (margin_width == 0) {
		g_signal_handlers_disconnect_by_func (G_OBJECT (view),
						      G_CALLBACK (gtk_source_view_expose),
						      view);
		return;
	}

	i = 0;
	while (i < count) {
		gint pos;

		gtk_text_view_buffer_to_window_coords (text_view,
						       GTK_TEXT_WINDOW_LEFT,
						       0,
						       g_array_index (pixels, gint, i),
						       NULL,
						       &pos);

		if (view->show_line_numbers ) {
			str = g_strdup_printf ("%d", g_array_index (numbers, gint, i) + 1);

			pango_layout_set_text (layout, str, -1);

			gtk_paint_layout (widget->style,
					  win,
					  GTK_WIDGET_STATE (widget),
					  FALSE,
					  NULL,
					  widget,
					  NULL,
					  text_width + 2, pos,
					  layout);

			g_free (str);
		}

		if (view->show_line_pixmaps) {
			gint x;

			if (view->show_line_numbers)
				x = text_width + 4;
			else
				x = 0;
			gtk_source_view_draw_line_markers (view,
							   g_array_index (numbers, gint, i) + 1,
							   x,
							   pos);
		}

		++i;
	}

	g_array_free (pixels, TRUE);
	g_array_free (numbers, TRUE);

	g_object_unref (G_OBJECT (layout));

	return TRUE;
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
gtk_source_view_calculate_tab_stop_width (GtkWidget *widget,
					  gint       tab_stop)
{
	PangoLayout *layout;
	gchar *tab_string;
	int counter = 0;
	int tab_width = 0;

	if (tab_stop == 0)
		return 0;

	tab_string = g_malloc (tab_stop + 1);

	while (counter < tab_stop) {
		tab_string[counter] = ' ';
		counter++;
	}
	tab_string[tab_stop] = '\0';

	layout = gtk_widget_create_pango_layout (widget, tab_string);
	g_free (tab_string);

	if (layout) {
		pango_layout_get_pixel_size (layout, &tab_width, NULL);
		g_object_unref (G_OBJECT (layout));
	} else
		tab_width = tab_stop * 8;

	return tab_width;
}

/* ----------------------------------------------------------------------
 * Public interface 
 * ---------------------------------------------------------------------- */

GtkWidget *
gtk_source_view_new ()
{
	GtkWidget *widget;
	GtkTextBuffer *buffer;

	buffer = gtk_source_buffer_new (NULL);
	widget = gtk_source_view_new_with_buffer (GTK_SOURCE_BUFFER (buffer));
	return widget;
}

GtkWidget *
gtk_source_view_new_with_buffer (GtkSourceBuffer *buffer)
{
	GtkWidget *view;

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

gboolean
gtk_source_view_get_show_line_numbers (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->show_line_numbers;
}

void
gtk_source_view_set_show_line_numbers (GtkSourceView *view,
				       gboolean       visible)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	if (visible) {
		if (!view->show_line_numbers) {
			/* Set left margin to minimum width if no margin is 
			   visible yet. Otherwise, just queue a redraw, so the
			   expose handler will automatically adjust the margin. */
			if (!view->show_line_pixmaps)
				gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
								      GTK_TEXT_WINDOW_LEFT,
								      MIN_NUMBER_WINDOW_WIDTH);
			else
				gtk_widget_queue_draw (GTK_WIDGET (view));

			if (!view->show_line_pixmaps)
				g_signal_connect (G_OBJECT (view),
						  "expose_event",
						  G_CALLBACK (gtk_source_view_expose),
						  view);

			view->show_line_numbers = visible;		}	} else {
		if (view->show_line_numbers) {
			view->show_line_numbers = visible;

			/* force expose event, which will adjust margin. */
			gtk_widget_queue_draw (GTK_WIDGET (view));
		}	}
}

gboolean
gtk_source_view_get_show_line_pixmaps (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->show_line_pixmaps;
}

void
gtk_source_view_set_show_line_pixmaps (GtkSourceView *view,
				       gboolean       visible)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	if (visible) {
		if (!view->show_line_pixmaps) {
			/* Set left margin to minimum width if no margin is 
			   visible yet. Otherwise, just queue a redraw, so the
			   expose handler will automatically adjust the margin. */
			if (!view->show_line_numbers)
				gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (view),
								      GTK_TEXT_WINDOW_LEFT,
								      MIN_NUMBER_WINDOW_WIDTH);
			else
				gtk_widget_queue_draw (GTK_WIDGET (view));

			if (!view->show_line_numbers)
				g_signal_connect (G_OBJECT (view),
						  "expose_event",
						  G_CALLBACK (gtk_source_view_expose),
						  view);

			view->show_line_pixmaps = visible;		}	} else {
		if (view->show_line_pixmaps) {
			view->show_line_pixmaps = visible;

			/* force expose event, which will adjust margin. */
			gtk_widget_queue_draw (GTK_WIDGET (view));
		}
	}
}

gint
gtk_source_view_get_tab_stop (GtkSourceView *view)
{
	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	return view->tab_stop;
}

void
gtk_source_view_set_tab_stop (GtkSourceView *view,
			      gint           tab_stop)
{
	PangoTabArray *tabs;

	g_return_if_fail (view != NULL);
	g_return_if_fail (GTK_IS_SOURCE_VIEW (view));

	view->tab_stop = tab_stop;
	tabs = pango_tab_array_new (1, TRUE);
	pango_tab_array_set_tab (tabs, 0, PANGO_TAB_LEFT,
				 gtk_source_view_calculate_tab_stop_width (GTK_WIDGET (view),
									   tab_stop));
	gtk_text_view_set_tabs (GTK_TEXT_VIEW (view), tabs);
	pango_tab_array_free (tabs);
}

gint
gtk_source_view_get_tab_stop_width (GtkSourceView *view)
{
	PangoTabArray *tabs;
	PangoTabAlign alignment;
	gint tabstop;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	tabs = gtk_text_view_get_tabs (GTK_TEXT_VIEW (view));
	pango_tab_array_get_tab (tabs, 0, &alignment, &tabstop);
	return tabstop;
}

gboolean
gtk_source_view_add_pixbuf (GtkSourceView *view,
			    const gchar   *key,
			    GdkPixbuf     *pixbuf,
			    gboolean       overwrite)
{
	gpointer data = NULL;
	gboolean replaced = FALSE;

	g_return_val_if_fail (view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), FALSE);

	data = g_hash_table_lookup (view->pixmap_cache, key);
	if (data && !overwrite)
		return (FALSE);

	if (data) {
		g_hash_table_remove (view->pixmap_cache, key);
		g_object_unref (G_OBJECT (data));
		replaced = TRUE;
	}
	if (pixbuf && GDK_IS_PIXBUF (pixbuf)) {
		gint width;
		gint height;

		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);
		if (width > GUTTER_PIXMAP || height > GUTTER_PIXMAP) {
			if (width > GUTTER_PIXMAP)
				width = GUTTER_PIXMAP;
			if (height > GUTTER_PIXMAP)
				height = GUTTER_PIXMAP;
			pixbuf = gdk_pixbuf_scale_simple (pixbuf, width, height,
							  GDK_INTERP_BILINEAR);
		}
		g_object_ref (G_OBJECT (pixbuf));
		g_hash_table_insert (view->pixmap_cache,
				     (gchar *) key,
				     (gpointer) pixbuf);
	}

	return replaced;
}

GdkPixbuf *
gtk_source_view_get_pixbuf (GtkSourceView *view,
			    const gchar   *key)
{
	return g_hash_table_lookup (view->pixmap_cache, key);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  test-widget.c
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagesmanager.h>


/* Private data structures */

typedef struct {
	GtkSourceBuffer *buffer;
	GList           *windows;
	gboolean         show_markers;
	gboolean         show_numbers;
	gboolean	 auto_indent;
	gboolean	 insert_spaces;
	gboolean	 show_margin;
	guint            tab_stop;
	GtkItemFactory  *item_factory;
	GtkWidget       *pos_label;
} ViewsData;

#define READ_BUFFER_SIZE   4096

#define MARKER_TYPE_1      "one"
#define MARKER_TYPE_2      "two"


/* Private prototypes */

static void       open_file_cb                   (ViewsData       *vd,
						  guint            callback_action,
						  GtkWidget       *widget);
static void       new_view_cb                    (ViewsData       *vd,
						  guint            callback_action,
						  GtkWidget       *widget);
static void       view_toggled_cb                (ViewsData       *vd,
						  guint            callback_action,
						  GtkWidget       *widget);
static void       tabs_toggled_cb                (ViewsData       *vd,
						  guint            callback_action,
						  GtkWidget       *widget);

/* Menu definition */

#define SHOW_NUMBERS_PATH "/View/Show _Line Numbers"
#define SHOW_MARKERS_PATH "/View/Show _Markers"
#define SHOW_MARGIN_PATH "/View/Show M_argin"

#define ENABLE_AUTO_INDENT_PATH "/View/Enable _Auto Indent"
#define INSERT_SPACES_PATH "/View/Insert _Spaces Instead of Tabs"


static GtkItemFactoryEntry menu_items[] = {
	{ "/_File",                   NULL,         0,               0, "<Branch>" },
	{ "/File/_Open",              "<control>O", open_file_cb,    0, "<StockItem>", GTK_STOCK_OPEN },
	{ "/File/sep1",               NULL,         0,               0, "<Separator>" },
	{ "/File/_Quit",              "<control>Q", gtk_main_quit,   0, "<StockItem>", GTK_STOCK_QUIT },
	
	{ "/_View",                   NULL,         0,               0, "<Branch>" },
	{ "/View/_New View",          NULL,         new_view_cb,     0, "<StockItem>", GTK_STOCK_NEW },
	{ "/View/sep1",               NULL,         0,               0, "<Separator>" },
	{ SHOW_NUMBERS_PATH,          NULL,         view_toggled_cb, 1, "<CheckItem>" },
	{ SHOW_MARKERS_PATH,          NULL,         view_toggled_cb, 2, "<CheckItem>" },
	{ SHOW_MARGIN_PATH,           NULL,         view_toggled_cb, 5, "<CheckItem>" },

	{ "/View/sep2",               NULL,         0,               0, "<Separator>" },
	{ ENABLE_AUTO_INDENT_PATH,    NULL,         view_toggled_cb, 3, "<CheckItem>" },
	{ INSERT_SPACES_PATH,         NULL,         view_toggled_cb, 4, "<CheckItem>" },

	{ "/View/sep3",               NULL,         0,               0, "<Separator>" },
	{ "/_View/_Tabs Width",	      NULL,         0,	             0, "<Branch>" },
	{ "/View/Tabs Width/4",	      NULL,         tabs_toggled_cb, 4, "<RadioItem>" },
	{ "/View/Tabs Width/6",	      NULL,         tabs_toggled_cb, 6, "/View/Tabs Width/4" },
	{ "/View/Tabs Width/8",	      NULL,         tabs_toggled_cb, 8, "/View/Tabs Width/4" },
	{ "/View/Tabs Width/10",      NULL,         tabs_toggled_cb, 10, "/View/Tabs Width/4" },
	{ "/View/Tabs Width/12",      NULL,         tabs_toggled_cb, 12, "/View/Tabs Width/4" }
};

/* Implementation */
static void 
view_toggled_cb (ViewsData *vd,
		 guint      callback_action,
		 GtkWidget *widget)
{
	gboolean active;
	void (*set_func) (GtkSourceView *, gboolean);

	set_func = NULL;
	active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget));
	
	switch (callback_action)
	{
		case 1:
			vd->show_numbers = active;
			set_func = gtk_source_view_set_show_line_numbers;
			break;
		case 2:
			vd->show_markers = active;
			set_func = gtk_source_view_set_show_line_markers;
			break;
		case 3:
			vd->auto_indent = active;
			set_func = gtk_source_view_set_auto_indent;
			break;
		case 4:
			vd->insert_spaces = active;
			set_func = gtk_source_view_set_insert_spaces_instead_of_tabs;
			break;
		case 5: 
			vd->show_margin = active;
			set_func = gtk_source_view_set_show_margin;
		default:
			break;
	}

	if (set_func)
	{
		GList *l;
		for (l = vd->windows; l; l = l->next)
		{
			GtkWidget *window = l->data;
			GtkWidget *view = g_object_get_data (G_OBJECT (window), "view");
			set_func (GTK_SOURCE_VIEW (view), active);
		}
	}
}

static void 
tabs_toggled_cb (ViewsData *vd,
	         guint      callback_action,
	         GtkWidget *widget)
{
	GList *l;

	vd->tab_stop = callback_action;
	
	for (l = vd->windows; l; l = l->next)
	{
		GtkWidget *window = l->data;
		GtkWidget *view = g_object_get_data (G_OBJECT (window), "view");
		g_object_set (G_OBJECT (view), "tabs_width", vd->tab_stop, NULL);
	}
}


static void
error_dialog (GtkWindow *parent, const gchar *msg, ...)
{
	va_list ap;
	gchar *tmp;
	GtkWidget *dialog;
	
	va_start (ap, msg);
	tmp = g_strdup_vprintf (msg, ap);
	va_end (ap);
	
	dialog = gtk_message_dialog_new (parent,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 tmp);
	g_free (tmp);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean 
gtk_source_buffer_load_with_encoding (GtkSourceBuffer *source_buffer,
				      const gchar     *filename,
				      const gchar     *encoding,
				      GError         **error)
{
	GIOChannel *io;
	GtkTextIter iter;
	gchar *buffer;
	gboolean reading;
	
	g_return_val_if_fail (source_buffer != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (source_buffer), FALSE);

	*error = NULL;

	io = g_io_channel_new_file (filename, "r", error);
	if (!io)
	{
		error_dialog (NULL, "%s\nFile %s", (*error)->message, filename);
		return FALSE;
	}

	if (g_io_channel_set_encoding (io, encoding, error) != G_IO_STATUS_NORMAL)
	{
		error_dialog (NULL, "Failed to set encoding:\n%s\n%s",
			      filename, (*error)->message);
		return FALSE;
	}

	gtk_source_buffer_begin_not_undoable_action (source_buffer);

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (source_buffer), "", 0);
	buffer = g_malloc (READ_BUFFER_SIZE);
	reading = TRUE;
	while (reading)
	{
		gsize bytes_read;
		GIOStatus status;
		
		status = g_io_channel_read_chars (io, buffer,
						  READ_BUFFER_SIZE, &bytes_read,
						  error);
		switch (status)
		{
			case G_IO_STATUS_EOF:
				reading = FALSE;
				/* fall through */
				
			case G_IO_STATUS_NORMAL:
				if (bytes_read == 0)
				{
					continue;
				}
				
				gtk_text_buffer_get_end_iter (
					GTK_TEXT_BUFFER (source_buffer), &iter);
				gtk_text_buffer_insert (GTK_TEXT_BUFFER (source_buffer),
							&iter, buffer, bytes_read);
				break;
				
			case G_IO_STATUS_AGAIN:
				continue;

			case G_IO_STATUS_ERROR:
			default:
				error_dialog (NULL, "%s\nFile %s", (*error)->message, filename);

				/* because of error in input we clear already loaded text */
				gtk_text_buffer_set_text (GTK_TEXT_BUFFER (source_buffer), "", 0);
				
				reading = FALSE;
				break;
		}
	}
	g_free (buffer);
	
	gtk_source_buffer_end_not_undoable_action (source_buffer);

	g_io_channel_unref (io);

	if (*error)
		return FALSE;

	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (source_buffer), FALSE);

	/* move cursor to the beginning */
	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (source_buffer), &iter);
	gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (source_buffer), &iter);

	return TRUE;
}

static void
remove_all_markers (GtkSourceBuffer *buffer)
{
	GSList *markers;
	GtkTextIter begin, end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	markers = gtk_source_buffer_get_markers_in_region (buffer, &begin, &end);
	while (markers)
	{
		GtkSourceMarker *marker = markers->data;
		
		gtk_source_buffer_delete_marker (buffer, marker);
		markers = g_slist_delete_link (markers, markers);
	}
}

static gboolean
open_file (GtkSourceBuffer *buffer, const gchar *filename)
{
	GtkSourceLanguagesManager *manager;
	GtkSourceLanguage *language = NULL;
	gchar *mime_type;
	GError *err = NULL;
	gchar *uri;
		
	/* get the new language for the file mimetype */
	manager = g_object_get_data (G_OBJECT (buffer), "languages-manager");

	/* I hate this! */
	if (g_path_is_absolute (filename))
	{
		uri = gnome_vfs_get_uri_from_local_path (filename);
	}
	else
	{
		gchar *curdir, *path;
		
		curdir = g_get_current_dir ();
		path = g_strconcat (curdir, "/", filename, NULL);
		g_free (curdir);
		uri = gnome_vfs_get_uri_from_local_path (path);
		g_free (path);
	}

	mime_type = gnome_vfs_get_mime_type (uri);
	g_free (uri);
	if (mime_type)
	{
		language = gtk_source_languages_manager_get_language_from_mime_type (manager,
										     mime_type);

		if (language == NULL)
		{
			g_print ("No language found for mime type `%s'\n", mime_type);
			g_object_set (G_OBJECT (buffer), "highlight", FALSE, NULL);
		}
		else
		{	
			g_object_set (G_OBJECT (buffer), "highlight", TRUE, NULL);

			gtk_source_buffer_set_language (buffer, language);
		}
			
		g_free (mime_type);
	}
	else
	{
		g_object_set (G_OBJECT (buffer), "highlight", FALSE, NULL);

		g_warning ("Couldn't get mime type for file `%s'", filename);
	}

	remove_all_markers (buffer);
	gtk_source_buffer_load_with_encoding (buffer, filename, "utf-8", &err);
	
	if (err != NULL)
	{
		g_error_free (err);
		return FALSE;
	}
	return TRUE;
}

static void
file_selected_cb (GtkWidget *widget, GtkFileSelection *file_sel)
{
	ViewsData *vd;
	const gchar *filename;
	
	vd = g_object_get_data (G_OBJECT (file_sel), "viewsdata");
	filename = gtk_file_selection_get_filename (file_sel);
	open_file (vd->buffer, filename);
}

static void 
open_file_cb (ViewsData *vd,
	      guint      callback_action,
	      GtkWidget *widget)
{
	GtkWidget *file_sel;

	file_sel = gtk_file_selection_new ("Open file...");
	g_object_set_data (G_OBJECT (file_sel), "viewsdata", vd);

	g_signal_connect (GTK_FILE_SELECTION (file_sel)->ok_button,
			  "clicked",
			  G_CALLBACK (file_selected_cb),
			  file_sel);

	g_signal_connect_swapped (GTK_FILE_SELECTION (file_sel)->ok_button, 
				  "clicked", G_CALLBACK (gtk_widget_destroy),
				  file_sel);
	g_signal_connect_swapped (GTK_FILE_SELECTION (file_sel)->cancel_button, 
				  "clicked", G_CALLBACK (gtk_widget_destroy),
				  file_sel);

	gtk_widget_show (file_sel);
}

/* Stolen from gedit */

static void
update_cursor_position (GtkTextBuffer *buffer, ViewsData *vd)
{
	gchar *msg;
	gint row, col, chars;
	GtkTextIter iter, start;
	
	gtk_text_buffer_get_iter_at_mark (buffer,
					  &iter,
					  gtk_text_buffer_get_insert (buffer));
	
	chars = gtk_text_iter_get_offset (&iter);
	row = gtk_text_iter_get_line (&iter) + 1;
	
	start = iter;
	gtk_text_iter_set_line_offset (&start, 0);
	col = 0;

	while (!gtk_text_iter_equal (&start, &iter))
	{
		if (gtk_text_iter_get_char (&start) == '\t')
		{
			col += (vd->tab_stop - (col % vd->tab_stop));
		}
		else
			++col;
		
		gtk_text_iter_forward_char (&start);
	}
	
	msg = g_strdup_printf ("char: %d, line: %d, column: %d", chars, row, col);
	gtk_label_set_text (GTK_LABEL (vd->pos_label), msg);
      	g_free (msg);
}

static void 
move_cursor_cb (GtkTextBuffer *buffer,
		GtkTextIter   *cursoriter,
		GtkTextMark   *mark,
		gpointer       data)
{
	if (mark != gtk_text_buffer_get_insert (buffer))
		return;
	
	update_cursor_position (buffer, data);
}

static gboolean
window_deleted_cb (GtkWidget *widget, GdkEvent *ev, ViewsData *vd)
{
	if (g_list_nth_data (vd->windows, 0) == widget)
	{
		/* Main (first in the list) window was closed, so exit
		 * the application */
		gtk_main_quit ();
	}
	else
	{
		vd->windows = g_list_remove (vd->windows, widget);

		/* we return FALSE since we want the window destroyed */
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
button_press_cb (GtkWidget *widget, GdkEventButton *ev, gpointer user_data)
{
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (widget), FALSE);
	
	view = GTK_SOURCE_VIEW (widget);
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	
	if (!gtk_source_view_get_show_line_markers (view))
		return FALSE;
	
	/* check that the click was on the left gutter */
	if (ev->window == gtk_text_view_get_window (GTK_TEXT_VIEW (view),
						    GTK_TEXT_WINDOW_LEFT))
	{
		gint y_buf;
		GtkTextIter line_start, line_end;
		GSList *marker_list, *list_iter;
		GtkSourceMarker *marker;
		const gchar *marker_type;
		
		if (ev->button == 1)
			marker_type = MARKER_TYPE_1;
		else
			marker_type = MARKER_TYPE_2;
		
		gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
						       GTK_TEXT_WINDOW_LEFT,
						       ev->x, ev->y,
						       NULL, &y_buf);

		/* get line bounds */
		gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view),
					     &line_start,
					     y_buf,
					     NULL);
		
		line_end = line_start;
		gtk_text_iter_forward_to_line_end (&line_end);

		/* get the markers already in the line */
		marker_list = gtk_source_buffer_get_markers_in_region (buffer,
								       &line_start,
								       &line_end);

		/* search for the marker corresponding to the button pressed */
		marker = NULL;
		for (list_iter = marker_list;
		     list_iter && !marker;
		     list_iter = g_slist_next (list_iter))
		{
			GtkSourceMarker *tmp = list_iter->data;
			gchar *tmp_type = gtk_source_marker_get_marker_type (tmp);
			
			if (tmp_type && !strcmp (tmp_type, marker_type))
			{
				marker = tmp;
			}
			g_free (tmp_type);
		}
		g_slist_free (marker_list);
		
		if (marker)
		{
			/* a marker was found, so delete it */
			gtk_source_buffer_delete_marker (buffer, marker);
		}
		else
		{
			/* no marker found -> create one */
			marker = gtk_source_buffer_create_marker (buffer, NULL,
								  marker_type, &line_start);
		}
	}
	
	return FALSE;
}

static GtkWidget *
create_window (ViewsData *vd)
{
	GtkWidget *window, *sw, *view, *vbox;
	PangoFontDescription *font_desc = NULL;
	GdkPixbuf *pixbuf;
	
	/* window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	gtk_window_set_title (GTK_WINDOW (window), "GtkSourceView Demo");
	g_signal_connect (window, "delete-event", (GCallback) window_deleted_cb, vd);
	vd->windows = g_list_append (vd->windows, window);

	/* vbox */
	vbox = gtk_vbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	g_object_set_data (G_OBJECT (window), "vbox", vbox);
	gtk_widget_show (vbox);

	/* scrolled window */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_end (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_IN);
	gtk_widget_show (sw);
	
	/* view */
	view = gtk_source_view_new_with_buffer (vd->buffer);
	g_object_set_data (G_OBJECT (window), "view", view);
	gtk_container_add (GTK_CONTAINER (sw), view);
	gtk_widget_show (view);

	/* setup view */
	font_desc = pango_font_description_from_string ("monospace 10");
	if (font_desc != NULL)
	{
		gtk_widget_modify_font (view, font_desc);
		pango_font_description_free (font_desc);
	}
	
	g_object_set (G_OBJECT (view), 
		      "tabs_width", vd->tab_stop, 
		      "show_line_numbers", vd->show_numbers,
		      "show_line_markers", vd->show_markers,
		      "show_margin", vd->show_margin,
		      "auto_indent", vd->auto_indent,
		      "insert_spaces_instead_of_tabs", vd->insert_spaces,
		      NULL);

	g_signal_connect (view, "button-press-event", (GCallback) button_press_cb, NULL);
	
	/* add marker pixbufs */
	pixbuf = gdk_pixbuf_new_from_file ("/usr/share/pixmaps/apple-green.png", NULL);
	gtk_source_view_set_marker_pixbuf (GTK_SOURCE_VIEW (view), MARKER_TYPE_1, pixbuf);
	g_object_unref (pixbuf);
	
	pixbuf = gdk_pixbuf_new_from_file ("/usr/share/pixmaps/apple-red.png", NULL);
	gtk_source_view_set_marker_pixbuf (GTK_SOURCE_VIEW (view), MARKER_TYPE_2, pixbuf);
	g_object_unref (pixbuf);
	
	return window;
}

static void
new_view_cb (ViewsData *vd, guint callback_action, GtkWidget *widget)
{
	GtkWidget *window;
	
	window = create_window (vd);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
	gtk_widget_show (window);
}

static GtkWidget *
create_main_window (ViewsData *vd)
{
	GtkWidget *window;
	GtkAccelGroup *accel_group;
	GtkWidget *vbox;
	GtkWidget *menu;
	GtkWidget *radio_item;
	gchar *radio_path;
	
	window = create_window (vd);
	vbox = g_object_get_data (G_OBJECT (window), "vbox");

	/* item factory/menu */
	accel_group = gtk_accel_group_new ();
	vd->item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
	g_object_unref (accel_group);
	gtk_item_factory_create_items (vd->item_factory,
				       G_N_ELEMENTS (menu_items),
				       menu_items,
				       vd);
	menu = gtk_item_factory_get_widget (vd->item_factory, "<main>");
	gtk_box_pack_start (GTK_BOX (vbox), menu, FALSE, FALSE, 0);
	gtk_widget_show (menu);
	
	/* preselect menu checkitems */
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (vd->item_factory,
								"/View/Show Line Numbers")),
		vd->show_numbers);
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (vd->item_factory,
								"/View/Show Markers")),
		vd->show_markers);
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (vd->item_factory,
								"/View/Show Margin")),
		vd->show_margin);

	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (vd->item_factory,
								"/View/Enable Auto Indent")),
		vd->auto_indent);
	gtk_check_menu_item_set_active (
		GTK_CHECK_MENU_ITEM (gtk_item_factory_get_item (vd->item_factory,
								"/View/Insert Spaces Instead of Tabs")),
		vd->insert_spaces);

	radio_path = g_strdup_printf ("/View/Tabs Width/%d", vd->tab_stop);
	radio_item = gtk_item_factory_get_item (vd->item_factory, radio_path);
	if (radio_item)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (radio_item), TRUE);
	g_free (radio_path);
	
	/* cursor position label */
	vd->pos_label = gtk_label_new ("label");
	gtk_box_pack_start (GTK_BOX (vbox), vd->pos_label, FALSE, FALSE, 0);
	g_signal_connect (vd->buffer, "mark_set",
			  (GCallback) move_cursor_cb, vd);
	g_signal_connect (vd->buffer, "changed",
			  (GCallback) update_cursor_position, vd);
	gtk_widget_show (vd->pos_label);
	
	return window;
}

static GtkSourceBuffer *
create_source_buffer (GtkSourceLanguagesManager *manager)
{
	GtkSourceBuffer *buffer;
	
	buffer = GTK_SOURCE_BUFFER (gtk_source_buffer_new (NULL));
	g_object_ref (manager);
	g_object_set_data_full (G_OBJECT (buffer), "languages-manager",
				manager, (GDestroyNotify) g_object_unref);
	
	return buffer;
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkSourceLanguagesManager *lm;
	ViewsData *vd;
	
	/* initialization */
	gtk_init (&argc, &argv);
	gnome_vfs_init ();
	
	lm = gtk_source_languages_manager_new ();
	
	/* setup... */
	vd = g_new0 (ViewsData, 1);
	vd->buffer = create_source_buffer (lm);
	g_object_unref (lm);
	vd->windows = NULL;

	vd->show_numbers = TRUE;
	vd->show_markers = TRUE;
	vd->show_margin = TRUE;
	vd->tab_stop = 8;
	vd->auto_indent = TRUE;
	vd->insert_spaces = FALSE;

	window = create_main_window (vd);
	if (argc > 1)
		open_file (vd->buffer, argv [1]);
	else
		open_file (vd->buffer, "../gtksourceview/gtksourcebuffer.c");

	gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
	gtk_widget_show (window);

	/* ... and action! */
	gtk_main ();

	/* cleanup */
	g_object_unref (vd->buffer);
	g_object_unref (vd->item_factory);
	g_list_foreach (vd->windows, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (vd->windows);
	g_free (vd);

	gnome_vfs_shutdown ();
	
	return 0;
}

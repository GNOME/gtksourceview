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
#include <libgnomeprintui/gnome-print-job-preview.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagesmanager.h>
#include <gtksourceview/gtksourceprintjob.h>


/* Global list of open windows */

static GList *windows = NULL;


/* Private data structures */

#define READ_BUFFER_SIZE   4096

#define MARKER_TYPE_1      "one"
#define MARKER_TYPE_2      "two"


/* Private prototypes -------------------------------------------------------- */

static void       open_file_cb                   (GtkAction       *action,
						  gpointer         user_data);

static void       new_view_cb                    (GtkAction       *action,
						  gpointer         user_data);
static void       numbers_toggled_cb             (GtkAction       *action,
						  gpointer         user_data);
static void       markers_toggled_cb             (GtkAction       *action,
						  gpointer         user_data);
static void       margin_toggled_cb              (GtkAction       *action,
						  gpointer         user_data);
static void       auto_indent_toggled_cb         (GtkAction       *action,
						  gpointer         user_data);
static void       insert_spaces_toggled_cb       (GtkAction       *action,
						  gpointer         user_data);
static void       tabs_toggled_cb                (GtkAction       *action,
						  GtkAction       *current,
						  gpointer         user_data);
static void       print_preview_cb               (GtkAction       *action,
						  gpointer         user_data);

static GtkWidget *create_view_window             (GtkSourceBuffer *buffer,
						  GtkSourceView   *from);


/* Actions & UI definition ---------------------------------------------------- */

static GtkActionEntry buffer_action_entries[] = {
	{ "Open", GTK_STOCK_OPEN, "_Open", "<control>O",
	  "Open a file", G_CALLBACK (open_file_cb) },
	{ "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q",
	  "Exit the application", G_CALLBACK (gtk_main_quit) },
};

static GtkActionEntry view_action_entries[] = {
	{ "FileMenu", NULL, "_File" },
	{ "ViewMenu", NULL, "_View" },
	{ "PrintPreview", GTK_STOCK_PRINT, "_Print Preview", "<control>P",
	  "Preview printing of the file", G_CALLBACK (print_preview_cb) },
	{ "NewView", GTK_STOCK_NEW, "_New View", NULL,
	  "Create a new view of the file", G_CALLBACK (new_view_cb) },
	{ "TabsWidth", NULL, "_Tabs Width" },
};

static GtkToggleActionEntry toggle_entries[] = {
	{ "ShowNumbers", NULL, "Show _Line Numbers", NULL,
	  "Toggle visibility of line numbers in the left margin",
	  G_CALLBACK (numbers_toggled_cb), FALSE },
	{ "ShowMarkers", NULL, "Show _Markers", NULL,
	  "Toggle visibility of markers in the left margin",
	  G_CALLBACK (markers_toggled_cb), FALSE },
	{ "ShowMargin", NULL, "Show M_argin", NULL,
	  "Toggle visibility of right margin indicator",
	  G_CALLBACK (margin_toggled_cb), FALSE },
	{ "AutoIndent", NULL, "Enable _Auto Indent", NULL,
	  "Toggle automatic auto indentation of text",
	  G_CALLBACK (auto_indent_toggled_cb), FALSE },
	{ "InsertSpaces", NULL, "Insert _Spaces Instead of Tabs", NULL,
	  "Whether to insert space characters when inserting tabulations",
	  G_CALLBACK (insert_spaces_toggled_cb), FALSE }
};

static GtkRadioActionEntry radio_entries[] = {
	{ "TabsWidth4", NULL, "4", NULL, "Set tabulation width to 4 spaces", 4 },
	{ "TabsWidth6", NULL, "6", NULL, "Set tabulation width to 6 spaces", 6 },
	{ "TabsWidth8", NULL, "8", NULL, "Set tabulation width to 8 spaces", 8 },
	{ "TabsWidth10", NULL, "10", NULL, "Set tabulation width to 10 spaces", 10 },
	{ "TabsWidth12", NULL, "12", NULL, "Set tabulation width to 12 spaces", 12 }
};

static const gchar *view_ui_description =
"<ui>"
"  <menubar name=\"MainMenu\">"
"    <menu action=\"FileMenu\">"
"      <menuitem action=\"PrintPreview\"/>"
"    </menu>"
"    <menu action=\"ViewMenu\">"
"      <menuitem action=\"NewView\"/>"
"      <separator/>"
"      <menuitem action=\"ShowNumbers\"/>"
"      <menuitem action=\"ShowMarkers\"/>"
"      <menuitem action=\"ShowMargin\"/>"
"      <separator/>"
"      <menuitem action=\"AutoIndent\"/>"
"      <menuitem action=\"InsertSpaces\"/>"
"      <separator/>"
"      <menu action=\"TabsWidth\">"
"        <menuitem action=\"TabsWidth4\"/>"
"        <menuitem action=\"TabsWidth6\"/>"
"        <menuitem action=\"TabsWidth8\"/>"
"        <menuitem action=\"TabsWidth10\"/>"
"        <menuitem action=\"TabsWidth12\"/>"
"      </menu>"
"    </menu>"
"  </menubar>"
"</ui>";

static const gchar *buffer_ui_description =
"<ui>"
"  <menubar name=\"MainMenu\">"
"    <menu action=\"FileMenu\">"
"      <menuitem action=\"Open\"/>"
"      <separator/>"
"      <menuitem action=\"Quit\"/>"
"    </menu>"
"    <menu action=\"ViewMenu\">"
"    </menu>"
"  </menubar>"
"</ui>";


/* File loading code ----------------------------------------------------------------- */

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
	g_object_set_data_full (G_OBJECT (buffer),
				"filename", g_strdup (filename),
				(GDestroyNotify) g_free);

	if (err != NULL)
	{
		g_error_free (err);
		return FALSE;
	}
	return TRUE;
}


/* Printing callbacks --------------------------------------------------------- */

static void
page_cb (GtkSourcePrintJob *job, gpointer user_data)
{
	g_print ("Printing %.2f%%    \r",
		 100.0 * gtk_source_print_job_get_page (job) /
		 gtk_source_print_job_get_page_count (job));
}

static void
finished_cb (GtkSourcePrintJob *job, gpointer user_data)
{
	GnomePrintJob *gjob;
	GtkWidget *preview;

	g_print ("\n");
	gjob = gtk_source_print_job_get_print_job (job);
	preview = gnome_print_job_preview_new (gjob, "test-widget print preview");
 	g_object_unref (gjob); 
 	g_object_unref (job);
	
	gtk_widget_show (preview);
}


/* View action callbacks -------------------------------------------------------- */

static void
numbers_toggled_cb (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_show_line_numbers (
		GTK_SOURCE_VIEW (user_data),
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
markers_toggled_cb (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_show_line_markers (
		GTK_SOURCE_VIEW (user_data),
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
margin_toggled_cb (GtkAction *action, gpointer user_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_show_margin (
		GTK_SOURCE_VIEW (user_data),
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void 
auto_indent_toggled_cb (GtkAction *action,
			gpointer   user_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_auto_indent (
		GTK_SOURCE_VIEW (user_data),
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void 
insert_spaces_toggled_cb (GtkAction *action,
			  gpointer   user_data)
{
	g_return_if_fail (GTK_IS_TOGGLE_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_insert_spaces_instead_of_tabs (
		GTK_SOURCE_VIEW (user_data),
		gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void 
tabs_toggled_cb (GtkAction *action,
		 GtkAction *current,
		 gpointer user_data)
{
	g_return_if_fail (GTK_IS_RADIO_ACTION (action) && GTK_IS_SOURCE_VIEW (user_data));
	gtk_source_view_set_tabs_width (
		GTK_SOURCE_VIEW (user_data),
		gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action)));
}

static void
new_view_cb (GtkAction *action, gpointer user_data)
{
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkWidget *window;
	
	g_return_if_fail (GTK_IS_SOURCE_VIEW (user_data));

	view = GTK_SOURCE_VIEW (user_data);
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	window = create_view_window (buffer, view);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
	gtk_widget_show (window);
}

static void
print_preview_cb (GtkAction *action, gpointer user_data)
{
	GtkSourcePrintJob *job;
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkTextIter start, end;
	gchar *filename;

	g_return_if_fail (GTK_IS_SOURCE_VIEW (user_data));
	
	view = GTK_SOURCE_VIEW (user_data);
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (user_data)));
	
	job = gtk_source_print_job_new (NULL);
	gtk_source_print_job_setup_from_view (job, view);
	gtk_source_print_job_set_wrap_mode (job, GTK_WRAP_CHAR);
	gtk_source_print_job_set_highlight (job, TRUE);
	gtk_source_print_job_set_print_numbers (job, 5);

	gtk_source_print_job_set_header_format (job,
						"Printed on %A",
						NULL,
						"%F",
						TRUE);

	filename = g_object_get_data (G_OBJECT (buffer), "filename");
	
	gtk_source_print_job_set_footer_format (job,
						"%T",
						filename,
						"Page %N/%Q",
						TRUE);

	gtk_source_print_job_set_print_header (job, TRUE);
	gtk_source_print_job_set_print_footer (job, TRUE);
	
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	if (gtk_source_print_job_print_range_async (job, &start, &end))
	{
		g_signal_connect (job, "begin_page", (GCallback) page_cb, NULL);
		g_signal_connect (job, "finished", (GCallback) finished_cb, NULL);
	}
	else
	{
		g_warning ("Async print failed");
	}
}


/* Buffer action callbacks ------------------------------------------------------------ */

static void 
open_file_cb (GtkAction *action, gpointer user_data)
{
	GtkWidget *chooser;
	gint response;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (user_data));
	
	chooser = gtk_file_chooser_dialog_new ("Open file...",
					       NULL,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					       NULL);

	response = gtk_dialog_run (GTK_DIALOG (chooser));
	if (response == GTK_RESPONSE_OK)
	{
		gchar *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		if (filename != NULL)
		{
			open_file (GTK_SOURCE_BUFFER (user_data), filename);
			g_free (filename);
		}
	}
	
	gtk_widget_destroy (chooser);
}


/* View UI callbacks ------------------------------------------------------------------ */

static void
update_cursor_position (GtkTextBuffer *buffer, gpointer user_data)
{
	gchar *msg;
	gint row, col, chars, tabwidth;
	GtkTextIter iter, start;
	GtkSourceView *view;
	GtkLabel *pos_label;

	g_return_if_fail (GTK_IS_SOURCE_VIEW (user_data));
	
	view = GTK_SOURCE_VIEW (user_data);
	tabwidth = gtk_source_view_get_tabs_width (view);
	pos_label = GTK_LABEL (g_object_get_data (G_OBJECT (view), "pos_label"));
	
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
			col += (tabwidth - (col % tabwidth));
		}
		else
			++col;
		
		gtk_text_iter_forward_char (&start);
	}
	
	msg = g_strdup_printf ("char: %d, line: %d, column: %d", chars, row, col);
	gtk_label_set_text (pos_label, msg);
      	g_free (msg);
}

static void 
move_cursor_cb (GtkTextBuffer *buffer,
		GtkTextIter   *cursoriter,
		GtkTextMark   *mark,
		gpointer       user_data)
{
	if (mark != gtk_text_buffer_get_insert (buffer))
		return;

	update_cursor_position (buffer, user_data);
}

static gboolean
window_deleted_cb (GtkWidget *widget, GdkEvent *ev, gpointer user_data)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (user_data), TRUE);

	if (g_list_nth_data (windows, 0) == widget)
	{
		/* Main (first in the list) window was closed, so exit
		 * the application */
		gtk_main_quit ();
	}
	else
	{
		GtkSourceView *view = GTK_SOURCE_VIEW (user_data);
		GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
		
		windows = g_list_remove (windows, widget);

		/* deinstall buffer motion signal handlers */
		g_signal_handlers_disconnect_matched (buffer,
						      G_SIGNAL_MATCH_DATA,
						      0, /* signal_id */
						      0, /* detail */
						      NULL, /* closure */
						      NULL, /* func */
						      user_data);

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


/* Window creation functions -------------------------------------------------------- */

static GtkWidget *
create_view_window (GtkSourceBuffer *buffer, GtkSourceView *from)
{
	GtkWidget *window, *sw, *view, *vbox, *pos_label, *menu;
	PangoFontDescription *font_desc = NULL;
	GdkPixbuf *pixbuf;
	GtkAccelGroup *accel_group;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GError *error;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (from == NULL || GTK_IS_SOURCE_VIEW (from), NULL);
	
	/* window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width (GTK_CONTAINER (window), 0);
	gtk_window_set_title (GTK_WINDOW (window), "GtkSourceView Demo");
	windows = g_list_append (windows, window);

	/* view */
	view = gtk_source_view_new_with_buffer (buffer);
	
	g_signal_connect (buffer, "mark_set", G_CALLBACK (move_cursor_cb), view);
	g_signal_connect (buffer, "changed", G_CALLBACK (update_cursor_position), view);
	g_signal_connect (view, "button-press-event", G_CALLBACK (button_press_cb), NULL);
	g_signal_connect (window, "delete-event", (GCallback) window_deleted_cb, view);

	/* action group and UI manager */
	action_group = gtk_action_group_new ("ViewActions");
	gtk_action_group_add_actions (action_group, view_action_entries,
				      G_N_ELEMENTS (view_action_entries), view);
	gtk_action_group_add_toggle_actions (action_group, toggle_entries,
					     G_N_ELEMENTS (toggle_entries), view);
	gtk_action_group_add_radio_actions (action_group, radio_entries,
					    G_N_ELEMENTS (radio_entries),
					    -1, G_CALLBACK (tabs_toggled_cb), view);
	
	ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group);

	/* save a reference to the ui manager in the window for later use */
	g_object_set_data_full (G_OBJECT (window), "ui_manager",
				ui_manager, (GDestroyNotify) g_object_unref);
	
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);
	gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, view_ui_description, -1, &error))
	{
		g_message ("building view ui failed: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	/* misc widgets */
	vbox = gtk_vbox_new (0, FALSE);
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                             GTK_SHADOW_IN);
	pos_label = gtk_label_new ("Position");
	g_object_set_data (G_OBJECT (view), "pos_label", pos_label);
	menu = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");

	/* layout widgets */
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gtk_box_pack_start (GTK_BOX (vbox), menu, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (sw), view);
	gtk_box_pack_start (GTK_BOX (vbox), pos_label, FALSE, FALSE, 0);

	/* setup view */
	font_desc = pango_font_description_from_string ("monospace 10");
	if (font_desc != NULL)
	{
		gtk_widget_modify_font (view, font_desc);
		pango_font_description_free (font_desc);
	}

	/* change view attributes to match those of from */
	if (from)
	{
		gchar *tmp;
		GtkAction *action;

		action = gtk_action_group_get_action (action_group, "ShowNumbers");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_source_view_get_show_line_numbers (from));

		action = gtk_action_group_get_action (action_group, "ShowMarkers");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_source_view_get_show_line_markers (from));
		
		action = gtk_action_group_get_action (action_group, "ShowMargin");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_source_view_get_show_margin (from));
		
		action = gtk_action_group_get_action (action_group, "AutoIndent");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      gtk_source_view_get_auto_indent (from));
		
		action = gtk_action_group_get_action (action_group, "InsertSpaces");
		gtk_toggle_action_set_active (
			GTK_TOGGLE_ACTION (action),
			gtk_source_view_get_insert_spaces_instead_of_tabs (from));
		

		tmp = g_strdup_printf ("TabsWidth%d", gtk_source_view_get_tabs_width (from));
		action = gtk_action_group_get_action (action_group, tmp);
		if (action)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		g_free (tmp);
	}

	/* add marker pixbufs */
	error = NULL;	
	if ((pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apple-green.png", &error)))
	{
		gtk_source_view_set_marker_pixbuf (GTK_SOURCE_VIEW (view), MARKER_TYPE_1, pixbuf);
		g_object_unref (pixbuf);
	}
	else
	{
		g_message ("could not load marker 1 image.  Spurious messages might get triggered: %s",
		error->message);
		g_error_free (error);
	} 
	
	error = NULL;
	if ((pixbuf = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/apple-red.png", &error)))
	{
		gtk_source_view_set_marker_pixbuf (GTK_SOURCE_VIEW (view), MARKER_TYPE_2, pixbuf);
		g_object_unref (pixbuf);
	}
	else
	{
		g_message ("could not load marker 2 image.  Spurious messages might get triggered: %s",
		error->message);
		g_error_free (error);		
	}
	
	gtk_widget_show_all (vbox);

	return window;
}

static GtkWidget *
create_main_window (GtkSourceBuffer *buffer)
{
	GtkWidget *window;
	GtkAction *action;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GList *groups;
	GError *error;
	
	window = create_view_window (buffer, NULL);
	ui_manager = g_object_get_data (G_OBJECT (window), "ui_manager");
	
	/* buffer action group */
	action_group = gtk_action_group_new ("BufferActions");
	gtk_action_group_add_actions (action_group, buffer_action_entries,
				      G_N_ELEMENTS (buffer_action_entries), buffer);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 1);
	g_object_unref (action_group);
	
	/* merge buffer ui */
	error = NULL;
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, buffer_ui_description, -1, &error))
	{
		g_message ("building buffer ui failed: %s", error->message);
		g_error_free (error);
		exit (1);
	}

	/* preselect menu checkitems */
	groups = gtk_ui_manager_get_action_groups (ui_manager);
	/* retrieve the view action group at position 0 in the list */
	action_group = g_list_nth_data (groups, 0);

	action = gtk_action_group_get_action (action_group, "ShowNumbers");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	action = gtk_action_group_get_action (action_group, "ShowMarkers");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	action = gtk_action_group_get_action (action_group, "ShowMargin");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	action = gtk_action_group_get_action (action_group, "AutoIndent");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	action = gtk_action_group_get_action (action_group, "InsertSpaces");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);

	action = gtk_action_group_get_action (action_group, "TabsWidth8");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	
	return window;
}


/* Buffer creation -------------------------------------------------------------- */

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


/* Program entry point ------------------------------------------------------------ */

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkSourceLanguagesManager *lm;
	GtkSourceBuffer *buffer;
	
	/* initialization */
	gtk_init (&argc, &argv);
	gnome_vfs_init ();
	
	/* create buffer */
	lm = gtk_source_languages_manager_new ();
	buffer = create_source_buffer (lm);
	g_object_unref (lm);

	if (argc > 1)
		open_file (buffer, argv [1]);
	else
		open_file (buffer, "../gtksourceview/gtksourcebuffer.c");

	/* create first window */
	window = create_main_window (buffer);
	gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
	gtk_widget_show (window);

	/* ... and action! */
	gtk_main ();

	/* cleanup */
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (windows);
	g_object_unref (buffer);

	gnome_vfs_shutdown ();
	
	return 0;
}

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourcevimimcontext-private.h>

static GMainLoop *main_loop;
static GString *sequence;

static gboolean
execute_command (GtkSourceVimIMContext *context,
                 const char            *command)
{
	if (g_str_equal (command, ":q") || g_str_equal (command, "^Wc"))
	{
		g_main_loop_quit (main_loop);
		return TRUE;
	}

	return FALSE;
}

static void
load_cb (GObject      *object,
         GAsyncResult *result,
         gpointer      user_data)
{
	GtkTextBuffer *buffer = user_data;
	GtkTextIter iter;

	gtk_text_buffer_get_start_iter (buffer, &iter);
	gtk_text_buffer_select_range (buffer, &iter, &iter);
	gtk_text_buffer_set_enable_undo (buffer, TRUE);
}

static void
open_file (GtkSourceBuffer *buffer,
           GFile           *file)
{
	GtkSourceFileLoader *loader;
	GtkSourceFile *sfile;

	sfile = gtk_source_file_new ();
	gtk_source_file_set_location (sfile, file);
	loader = gtk_source_file_loader_new (buffer, sfile);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
					   NULL, NULL, NULL, NULL, load_cb, buffer);

	g_object_unref (sfile);
	g_object_unref (loader);
}

static gboolean
on_close_request (GtkWindow *window)
{
	g_main_loop_quit (main_loop);
	return FALSE;
}

static void
observe_key (GtkSourceVimIMContext *self,
             const char            *str,
             gboolean               reset_observer,
             gpointer               data)
{
	GtkLabel *label = data;

	if (reset_observer)
		g_string_truncate (sequence, 0);

	g_string_append (sequence, str);
	gtk_label_set_label (label, sequence->str);
}

int
main (int argc,
      char *argv[])
{
	GtkWindow *window;
	GtkSourceStyleSchemeManager *schemes;
	GtkSourceLanguageManager *languages;
	GtkScrolledWindow *scroller;
	GtkSourceView *view;
	GtkIMContext *im_context;
	GtkEventController *key;
	GtkSourceBuffer *buffer;
	GtkLabel *command_bar;
	GtkLabel *command;
	GtkLabel *observe;
	GtkBox *vbox;
	GtkBox *box;
	GFile *file;

	gtk_init ();
	gtk_source_init ();

	sequence = g_string_new (NULL);
	schemes = gtk_source_style_scheme_manager_get_default ();
	languages = gtk_source_language_manager_get_default ();

	main_loop = g_main_loop_new (NULL, FALSE);
	window = g_object_new (GTK_TYPE_WINDOW,
	                       "default-width", 800,
	                       "default-height", 600,
	                       NULL);
	scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
	                         "vexpand", TRUE,
	                         NULL);
	buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_language (buffer, gtk_source_language_manager_get_language (languages, "c"));
	gtk_source_buffer_set_style_scheme (buffer, gtk_source_style_scheme_manager_get_scheme (schemes, "Adwaita"));
	view = g_object_new (GTK_SOURCE_TYPE_VIEW,
	                     "auto-indent", TRUE,
	                     "buffer", buffer,
	                     "monospace", TRUE,
	                     "show-line-numbers", TRUE,
	                     "top-margin", 6,
	                     "left-margin", 6,
	                     NULL);
	vbox = g_object_new (GTK_TYPE_BOX,
	                     "orientation", GTK_ORIENTATION_VERTICAL,
	                     "vexpand", TRUE,
	                     NULL);
	box = g_object_new (GTK_TYPE_BOX,
			    "margin-start", 12,
			    "margin-top", 6,
			    "margin-bottom", 6,
			    "margin-end", 12,
	                    "orientation", GTK_ORIENTATION_HORIZONTAL,
	                    "hexpand", TRUE,
	                    NULL);
	command_bar = g_object_new (GTK_TYPE_LABEL,
	                            "hexpand", TRUE,
	                            "xalign", 0.0f,
	                            "margin-top", 6,
	                            "margin-bottom", 6,
	                            "margin-end", 12,
	                            NULL);
	command = g_object_new (GTK_TYPE_LABEL,
	                        "xalign", 0.0f,
	                        "margin-top", 6,
	                        "margin-bottom", 6,
	                        "margin-end", 12,
	                        "width-chars", 8,
	                        NULL);
	observe = g_object_new (GTK_TYPE_LABEL,
	                        "margin-start", 24,
	                        "width-chars", 12,
	                        "wrap", TRUE,
	                        "xalign", 1.0f,
	                        NULL);

	gtk_window_set_child (window, GTK_WIDGET (vbox));
	gtk_box_append (vbox, GTK_WIDGET (scroller));
	gtk_box_append (vbox, GTK_WIDGET (box));
	gtk_scrolled_window_set_child (scroller, GTK_WIDGET (view));
	gtk_box_append (box, GTK_WIDGET (command_bar));
	gtk_box_append (box, GTK_WIDGET (command));
	gtk_box_append (box, GTK_WIDGET (observe));

	im_context = gtk_source_vim_im_context_new ();
	g_object_bind_property (im_context, "command-bar-text", command_bar, "label", G_BINDING_SYNC_CREATE);
	g_object_bind_property (im_context, "command-text", command, "label", G_BINDING_SYNC_CREATE);
	g_signal_connect (im_context, "execute-command", G_CALLBACK (execute_command), NULL);
	_gtk_source_vim_im_context_add_observer (GTK_SOURCE_VIM_IM_CONTEXT (im_context), observe_key, observe, NULL);
	gtk_im_context_set_client_widget (im_context, GTK_WIDGET (view));

	key = gtk_event_controller_key_new ();
	gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (key), im_context);
	gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
	gtk_widget_add_controller (GTK_WIDGET (view), key);

	g_signal_connect (window, "close-request", G_CALLBACK (on_close_request), NULL);
	gtk_window_present (window);

	file = g_file_new_for_path (TOP_SRCDIR "/gtksourceview/gtksourcebuffer.c");
	open_file (buffer, file);

	g_main_loop_run (main_loop);

	gtk_source_finalize ();

	return 0;
}

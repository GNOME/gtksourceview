/*
 * Copyright (C) 2004  Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

#include "eggregex.h"

typedef struct {
  GtkWidget     *window;
  GtkWidget     *regex;
  GtkWidget     *subst;
  GtkTextBuffer *buffer;  
} RegexData;

static void show_error (GtkWidget   *widget,
			const gchar *error_message)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (widget),
				   GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_ERROR,
				   GTK_BUTTONS_OK,
				   "%s", error_message);

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
 
  g_signal_connect_swapped (dialog, "response",
			    G_CALLBACK (gtk_widget_destroy),
			    dialog);

  gtk_widget_show (dialog);
}

static void
search_cb (GtkWidget *widget,
	   RegexData *data)
{
  gchar *pattern;
  gchar *text;
  GtkTextIter start, end;
  EggRegex *regex;
  GError *error = NULL;
   
  pattern = g_strdup (gtk_entry_get_text (GTK_ENTRY (data->regex)));
  gtk_text_buffer_get_start_iter (data->buffer, &start);
  gtk_text_buffer_get_end_iter (data->buffer, &end);
  text = gtk_text_buffer_get_text (data->buffer, &start, &end, FALSE);

  regex = egg_regex_new (pattern, 0, 0, &error);
  if (error)
    {
      show_error (data->window, error->message);
      g_error_free (error);
      g_free (pattern);
      g_free (text);
      return;
    }
  
  if (egg_regex_match (regex, text, -1, 0) > 0)
    {
      gint start_pos, end_pos;

      egg_regex_fetch_pos (regex, text, 0, &start_pos, &end_pos);
      gtk_text_buffer_get_iter_at_offset (data->buffer, &start, start_pos);
      gtk_text_buffer_get_iter_at_offset (data->buffer, &end, end_pos);

      gtk_text_buffer_select_range (data->buffer, &start, &end);
    }

  egg_regex_free (regex);
  
  g_free (pattern);
  g_free (text);
}

static void
replace_cb (GtkWidget   *widget,
	      RegexData *data)
{
  gchar *pattern, *replacement;
  gchar *text, *new_text;
  GtkTextIter start, end;
  EggRegex *regex;
  GError *error = NULL;
   
  pattern = g_strdup (gtk_entry_get_text (GTK_ENTRY (data->regex)));
  replacement = g_strdup (gtk_entry_get_text (GTK_ENTRY (data->subst)));
  gtk_text_buffer_get_start_iter (data->buffer, &start);
  gtk_text_buffer_get_end_iter (data->buffer, &end);
  text = gtk_text_buffer_get_text (data->buffer, &start, &end, FALSE);

  regex = egg_regex_new (pattern, 0, 0, &error);
  if (error)
    {
      show_error (data->window, error->message);
      g_error_free (error);
      g_free (pattern);
      g_free (replacement);
      g_free (text);
      return;
    }
  
  new_text = egg_regex_replace (regex, text, -1, replacement, 0, &error);

  if (error)
    {
      show_error (data->window, error->message);
      g_error_free (error);
      g_free (pattern);
      g_free (replacement);
      g_free (text);
      g_free (new_text);
      return;
    }
  gtk_text_buffer_set_text (data->buffer, new_text, -1);
  
  egg_regex_free (regex);
  
  g_free (replacement);
  g_free (pattern);
  g_free (text);
  g_free (new_text);
}

int
main (int argc, char *argv[])
{
  GtkWidget *vbox, *hbox, *label;
  GtkWidget *quit, *search, *replace, *text;
  RegexData data;

  gtk_init (&argc, &argv);

  data.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (data.window, 400, 200);
  g_signal_connect (data.window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

  vbox = gtk_vbox_new (0, 0);
  gtk_container_add (GTK_CONTAINER (data.window), vbox);

  hbox = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  
  label = gtk_label_new ("Regex:");
  data.regex = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), data.regex, TRUE, TRUE, 0);

  label = gtk_label_new ("Replacement:");
  data.subst = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), data.subst, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (0, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  search = gtk_button_new_with_label ("Search");
  replace = gtk_button_new_with_label ("Replace");
  quit = gtk_button_new_with_label ("Quit");
  gtk_container_add (GTK_CONTAINER (hbox), search);  
  gtk_container_add (GTK_CONTAINER (hbox), replace);
  gtk_container_add (GTK_CONTAINER (hbox), quit);
  g_signal_connect (search, "clicked", G_CALLBACK (search_cb), &data);
  g_signal_connect (replace, "clicked", G_CALLBACK (replace_cb), &data);
  g_signal_connect (quit, "clicked", G_CALLBACK (gtk_main_quit), NULL);
  
  data.buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (data.buffer, 
			    "The quick brown fox jumps over "
			    "the lazy dog.", -1);
  text = gtk_text_view_new_with_buffer (data.buffer);

  gtk_container_add (GTK_CONTAINER (vbox), text);

  gtk_widget_show_all (data.window);
  
  gtk_main ();

  return 0;
}

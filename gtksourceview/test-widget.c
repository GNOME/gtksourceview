/*  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
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
#include "gtktextsearch.h"
#include "gtksourceview.h"

static GtkTextBuffer *test_source (GtkSourceBuffer *buf);
static void cb_move_cursor (GtkTextBuffer *buf, GtkTextIter *cursoriter, GtkTextMark *mark,
			    gpointer data);

GtkTextBuffer *
test_source (GtkSourceBuffer *buffer)
{
	GtkTextTag *tag;
	GtkTextTagTable *table;
	GList *list = NULL;
	GError *err = NULL;

	if (!buffer)
		buffer = GTK_SOURCE_BUFFER (gtk_source_buffer_new (NULL));

	table = GTK_TEXT_BUFFER (buffer)->tag_table;

	tag = gtk_pattern_tag_new ("gnu_typedef", "\\b\\(Gtk\\|Gdk\\|Gnome\\)[a-zA-Z0-9_]+");
	g_object_set (G_OBJECT (tag), "foreground", "blue", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("defs",
				   "^#[ \t]*\\(include\\|if\\|ifdef\\|ifndef\\|else\\|elif\\|define\\|endif\\|pragma\\)\\b");
	g_object_set (G_OBJECT (tag), "foreground", "tomato3", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("numbers", "\\b[0-9]+\\.?\\b");
	g_object_set (G_OBJECT (tag), "weight", PANGO_WEIGHT_BOLD, NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("types",
				   "\\b\\(int\\|float\\|enum\\|bool\\|char\\|void\\|gint\\|"
				   "gchar\\|gpointer\\|guint\\|guchar\\|static\\|const\\|"
				   "typedef\\|struct\\|class\\|gboolean\\|sizeof\\)\\b");
	g_object_set (G_OBJECT (tag), "foreground", "blue", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("gtk_functions", "\\b\\(gtk\\|gdk\\|g\\|gnome\\)_[a-zA-Z0-9_]+");
	g_object_set (G_OBJECT (tag), "foreground", "brown", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("functions", "^[a-zA-Z_]*\\:");
	g_object_set (G_OBJECT (tag), "foreground", "navy", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("macro", "\\b[A-Z_][A-Z0-9_\\-]+\\b");
	g_object_set (G_OBJECT (tag), "foreground", "red", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("keywords",
				   "\\b\\(do\\|while\\|for\\|if\\|else\\|switch\\|case\\|"
				   "return\\|public\\|protected\\|private\\|false\\|"
				   "true\\|break\\|extern\\|inline\\|this\\|dynamic_cast\\|"
				   "static_cast\\|template\\|cin\\|cout\\)\\b");
	g_object_set (G_OBJECT (tag), "foreground", "blue", "weight", PANGO_WEIGHT_BOLD, NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("operators",
				   "\\(\\*\\|\\*\\*\\|->\\|::\\|<<\\|>>\\|>\\|<\\|=\\|==\\|!=\\|<=\\|>=\\|++\\|--\\|%\\|+\\|-\\|||\\|&&\\|!\\|+=\\|-=\\|\\*=\\|/=\\|%=\\)");
	g_object_set (G_OBJECT (tag), "foreground", "green", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_pattern_tag_new ("char_string", "'\\?[a-zA-Z0-9_\\()#@!$%^&*-=+\"{}<)]'");
	g_object_set (G_OBJECT (tag), "foreground", "orange", NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_syntax_tag_new ("comment", "//", "\n");
	g_object_set (G_OBJECT (tag), "foreground", "gray", "style", PANGO_STYLE_ITALIC, NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_syntax_tag_new ("comment_multiline", "/\\*", "\\*/");
	g_object_set (G_OBJECT (tag), "foreground", "gray", "style", PANGO_STYLE_ITALIC, NULL);
	list = g_list_append (list, (gpointer) tag);

	tag = gtk_syntax_tag_new ("string", "\"", "\"");
	g_object_set (G_OBJECT (tag), "foreground", "forest green", NULL);
	list = g_list_append (list, (gpointer) tag);

	gtk_source_buffer_install_regex_tags (buffer, list);
	g_list_free (list);

	gtk_source_buffer_load (buffer, "test-widget.c", &err);
#ifdef OLD
	if (g_file_get_contents ("gtksourcebuffer.c", &txt, &len, &error)) {
		gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), txt, len);
	} else {
		GtkWidget *w;

		w = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					    error->message);
		gtk_dialog_run (GTK_DIALOG (w));
		gtk_widget_destroy (w);
		g_error_free (error);
	}
#endif

	return GTK_TEXT_BUFFER (buffer);
}
gboolean
cb_func(GtkTextIter *iter1, GtkTextIter *iter2, gpointer data)
{
  gtk_text_buffer_delete(GTK_TEXT_BUFFER(gtk_text_iter_get_buffer(iter1)), iter1, iter2);

  gtk_text_buffer_insert (GTK_TEXT_BUFFER(gtk_text_iter_get_buffer(iter1)), iter1, "FUCK", 4);



  return FALSE;
}

void
cb_entry_activate (GtkWidget *widget, gpointer data)
{
  GtkTextSearch *search;
  char *txt;
  txt = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
 

  search = gtk_text_search_new (GTK_TEXT_BUFFER(data), NULL, txt, GTK_ETEXT_SEARCH_TEXT_ONLY | GTK_ETEXT_SEARCH_CASE_INSENSITIVE, NULL);  
  gtk_text_search_forward_foreach(search, cb_func, NULL);
/*
  if (gtk_text_search_forward (search, &iter1, &iter2))  {
     gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER(data), &iter1); 
     gtk_text_buffer_move_mark (GTK_TEXT_BUFFER(data), gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(data)), &iter2); 
  }
*/

  g_free(txt);
}

void
cb_convert (GtkWidget *widget, gpointer data)
{
	FILE *hf;
	gchar *txt;

	txt = gtk_source_buffer_convert_to_html (GTK_SOURCE_BUFFER (data), "This is a test");
	if (txt) {
		hf = fopen ("test.html", "w+");
		fwrite (txt, strlen (txt), 1, hf);
		fclose (hf);
	}
	g_free (txt);
}

void
cb_toggle (GtkWidget *widget, gpointer data)
{
	gtk_source_view_set_show_line_pixmaps (GTK_SOURCE_VIEW (data),
					       !GTK_SOURCE_VIEW (data)->show_line_pixmaps);
}

void
cb_line_numbers_toggle (GtkWidget *widget, gpointer data)
{
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (data),
					       !GTK_SOURCE_VIEW (data)->show_line_numbers);
}

void
cb_move_cursor (GtkTextBuffer *b, GtkTextIter *cursoriter, GtkTextMark *mark, gpointer data)
{
	char buf[64];

	if (mark != gtk_text_buffer_get_insert (gtk_text_iter_get_buffer (cursoriter)))
		return;
	g_snprintf (buf, 64, "char pos %d line %d column %d",
		    gtk_text_iter_get_offset (cursoriter),
		    gtk_text_iter_get_line (cursoriter),
		    gtk_text_iter_get_line_offset (cursoriter));
	gtk_label_set_text (GTK_LABEL (data), buf);
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GtkWidget *scrolled;
	GtkWidget *vbox;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *button;
	GtkTextBuffer *buf;
	GtkWidget *tw;
	GdkPixbuf *pixbuf;
	int i;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect (GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

	buf = test_source (NULL);

	vbox = gtk_vbox_new (0, FALSE);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 2);

	button = gtk_button_new_with_label ("convert to html (example is saved as test.html)");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (cb_convert), buf);

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

	label = gtk_label_new ("label");
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	g_signal_connect_closure (G_OBJECT (entry), "activate",
				  g_cclosure_new ((GCallback) cb_entry_activate, buf, NULL), TRUE);
	g_signal_connect_closure (G_OBJECT (buf), "mark_set",
				  g_cclosure_new ((GCallback) cb_move_cursor, label, NULL), TRUE);
	tw = gtk_source_view_new_with_buffer (GTK_SOURCE_BUFFER (buf));
	gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (tw), TRUE);
	gtk_source_view_set_show_line_pixmaps (GTK_SOURCE_VIEW (tw), TRUE);
	gtk_source_view_set_tab_stop (GTK_SOURCE_VIEW (tw), 8);
	gtk_container_add (GTK_CONTAINER (scrolled), tw);

	button = gtk_button_new_with_label ("Toggle line pixmaps");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (cb_toggle), tw);

	button = gtk_button_new_with_label ("Toggle line numbers");
	gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (cb_line_numbers_toggle), tw);

	pixbuf = gdk_pixbuf_new_from_file ("/usr/share/pixmaps/apple-green.png", NULL);
	gtk_source_view_add_pixbuf (GTK_SOURCE_VIEW (tw), "one", pixbuf, FALSE);
	pixbuf = gdk_pixbuf_new_from_file ("/usr/share/pixmaps/no.xpm", NULL);
	gtk_source_view_add_pixbuf (GTK_SOURCE_VIEW (tw), "two", pixbuf, FALSE);
	pixbuf = gdk_pixbuf_new_from_file ("/usr/share/pixmaps/detach-menu.xpm", NULL);
	gtk_source_view_add_pixbuf (GTK_SOURCE_VIEW (tw), "three", pixbuf, FALSE);

	for (i = 1; i < 200; i += 20) {
		gtk_source_buffer_line_set_marker (GTK_SOURCE_BUFFER (GTK_TEXT_VIEW (tw)->buffer),
						   i, "one");
	}
	for (i = 1; i < 200; i += 40) {
		gtk_source_buffer_line_add_marker (GTK_SOURCE_BUFFER (GTK_TEXT_VIEW (tw)->buffer),
						   i, "two");
	}
	for (i = 1; i < 200; i += 80) {
		gtk_source_buffer_line_add_marker (GTK_SOURCE_BUFFER (GTK_TEXT_VIEW (tw)->buffer),
						   i, "three");
	}


	gtk_widget_set_usize (window, 400, 500);
	gtk_widget_show_all (window);
	gtk_main ();
	return (0);
}

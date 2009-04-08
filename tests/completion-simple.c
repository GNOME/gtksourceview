/*
 * main.c
 * Copyright (C) perriman 2007 <chuchiperriman@gmail.com>
 * 
 * main.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * main.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with main.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletioninfo.h>
#include <gtksourceview/gtksourcecompletiontriggerkey.h>

#include "gsc-utils-test.h"
#include "gsc-provider-test.h"

#define TEST_PAGE "Page 3"
#define FIXED_PAGE "Fixed"

static GtkWidget *view;
static GtkSourceCompletion *comp;
static GtkSourceCompletionInfo *info;

static const gboolean change_keys = FALSE;

static void
show_completion_cb (GtkWidget *w, gpointer user_data)
{
	gint n;
	gint pos;
	
	n = gtk_source_completion_get_n_pages (comp);
	pos = gtk_source_completion_get_page_pos (comp, TEST_PAGE);
	
	if (pos == n -1)
		pos = 1;
	else
		pos++;
	
	gtk_source_completion_set_page_pos (comp, TEST_PAGE, pos);
	g_debug ("pos: %d, urpos: %d", pos, 
		gtk_source_completion_get_page_pos (comp, TEST_PAGE));
	g_assert (gtk_source_completion_get_page_pos (comp, TEST_PAGE) == pos);
}

static void
hide_completion_cb (GtkWidget *w, gpointer user_data)
{
	
}

static gboolean
filter_func (GtkSourceCompletionProposal *proposal,
	     gpointer user_data)
{
	const gchar *label = gtk_source_completion_proposal_get_label (proposal);
	return g_str_has_prefix (label, "sp");
}

static void
destroy_cb(GtkObject *object,gpointer   user_data)
{
	gtk_main_quit ();
}

static void
activate_toggled_cb (GtkToggleButton *button,
		     gpointer user_data)
{
	g_object_set (comp, "active", gtk_toggle_button_get_active (button), NULL);
}

static void
remember_toggled_cb (GtkToggleButton *button,
		     gpointer user_data)
{
	g_object_set (comp, "remember-info-visibility", gtk_toggle_button_get_active (button), NULL);
}

static void
select_on_show_toggled_cb (GtkToggleButton *button,
			   gpointer user_data)
{
	g_object_set (comp, "select-on-show", gtk_toggle_button_get_active (button), NULL);
}

static gboolean
key_press(GtkWidget   *widget,
	GdkEventKey *event,
	gpointer     user_data)
{
	if (event->keyval == GDK_F9)
	{
		gtk_source_completion_filter_proposals (comp,
					  filter_func,
					  NULL);
		return TRUE;
	} else if (event->keyval == GDK_F8)
	{
		GtkSourceCompletionInfo *gsc_info;
		
		gsc_info = gtk_source_completion_get_info_widget (comp);
		
		if (GTK_WIDGET_VISIBLE (GTK_WIDGET (gsc_info)))
			gtk_widget_hide (GTK_WIDGET (gsc_info));
		else
			gtk_widget_show (GTK_WIDGET (gsc_info));
	}
	
	guint key = 0;
	GdkModifierType mod;
	gtk_accelerator_parse ("<Control>b", &key, &mod);
	
	guint s = event->state & gtk_accelerator_get_default_mod_mask();
	
	if (s == mod && gdk_keyval_to_lower(event->keyval) == key)
	{
		if (!GTK_WIDGET_VISIBLE (info))
		{
			gchar *text;
			gchar *word = gsc_get_last_word (GTK_TEXT_VIEW (view));
			text = g_strdup_printf ("<b>Calltip</b>: %s", word);
			
			gtk_source_completion_info_set_markup (info, text);
			g_free (text);
			gtk_source_completion_info_move_to_cursor (info, GTK_TEXT_VIEW (view));
			gtk_widget_show (GTK_WIDGET (info));
		}
		else
		{
			gtk_widget_hide (GTK_WIDGET (info));
		}
	}
	
	return FALSE;
}

static GtkWidget*
create_window (void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *activate;
	GtkWidget *remember;
	GtkWidget *select_on_show;
	GtkWidget *label;
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_resize(GTK_WINDOW(window),600,400);
	
	vbox = gtk_vbox_new (FALSE,1);
	hbox = gtk_hbox_new (FALSE,1);
	
	view = gtk_source_view_new();
	GtkWidget *scroll = gtk_scrolled_window_new(NULL,NULL);
	gtk_container_add(GTK_CONTAINER(scroll),view);
	
	activate = gtk_check_button_new_with_label ("Active");
	remember = gtk_check_button_new_with_label ("Remember info visibility");
	select_on_show = gtk_check_button_new_with_label ("Select first on show");
	label = gtk_label_new ("F9 filter by \"sp\"\n<Control>b to show a calltip\nF8 show/hide info");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (activate), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (remember), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox),label, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),activate, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),remember, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox),select_on_show, FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(vbox),scroll, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox),hbox, FALSE, FALSE, 0);
	
	gtk_container_add(GTK_CONTAINER(window),vbox);
	
	g_signal_connect(view, "key-release-event", G_CALLBACK(key_press), NULL);
	
	g_signal_connect(window, "destroy", G_CALLBACK(destroy_cb), NULL);
	
	g_signal_connect(activate,"toggled",G_CALLBACK(activate_toggled_cb),NULL);
	g_signal_connect(remember,"toggled",G_CALLBACK(remember_toggled_cb),NULL);
	g_signal_connect(select_on_show,"toggled",G_CALLBACK(select_on_show_toggled_cb),NULL);
	
	return window;
}

static void
create_completion(void)
{
	GscProviderTest *prov_test;
	GtkSourceCompletionTriggerKey *ur_trigger;
	
	prov_test = gsc_provider_test_new ();
	
	comp = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
	
	ur_trigger = gtk_source_completion_trigger_key_new (comp,
							    "User Request Trigger",
							    "<Control>Return");
	
	gtk_source_completion_register_trigger (comp, GTK_SOURCE_COMPLETION_TRIGGER (ur_trigger));

	gtk_source_completion_register_provider (comp, GTK_SOURCE_COMPLETION_PROVIDER (prov_test),
						 GTK_SOURCE_COMPLETION_TRIGGER (ur_trigger));

	gtk_source_completion_set_active (comp, TRUE);
	
	g_signal_connect (comp, "show", G_CALLBACK (show_completion_cb), NULL);
	g_signal_connect (comp, "hide", G_CALLBACK (hide_completion_cb), NULL);
	
}

static void
create_info ()
{
	info = gtk_source_completion_info_new ();
	gtk_source_completion_info_set_adjust_height (info,
				    TRUE,
				    -1);
	gtk_source_completion_info_set_adjust_width (info,
				   TRUE,
				   -1);
}

int
main (int argc, char *argv[])
{
 	GtkWidget *window;
	
	gtk_set_locale ();
	gtk_init (&argc, &argv);

	window = create_window ();
	create_completion ();
	create_info ();
	
	g_assert (gtk_source_completion_get_n_pages (comp) == 1);
	gtk_source_completion_set_page_pos (comp, FIXED_PAGE, 0);
	
	gtk_widget_show_all (window);

	gtk_main ();
	return 0;
}



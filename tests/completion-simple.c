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

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourcecompletioninfo.h>
#include <gtksourceview/gtksourcelanguagemanager.h>

#include "gsc-provider-words.h"

#ifdef HAVE_DEVHELP
#include <devhelp/dh-base.h>
#include "gsc-provider-devhelp.h"
#endif

static GtkWidget *view;
static GtkSourceCompletion *comp;

static const gboolean change_keys = FALSE;

typedef struct 
{
	GtkWidget *box;
	GtkWidget *header;
	GtkWidget *content;
	GtkWidget *foot;
} CustomWidget;

static void
destroy_cb (GtkObject *object,
	    gpointer   user_data)
{
	gtk_main_quit ();
}

static void
remember_toggled_cb (GtkToggleButton *button,
		     gpointer user_data)
{
	g_object_set (comp, "remember-info-visibility",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static void
select_on_show_toggled_cb (GtkToggleButton *button,
			   gpointer user_data)
{
	g_object_set (comp, "select-on-show",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static void
show_headers_toggled_cb (GtkToggleButton *button,
			   gpointer user_data)
{
	g_object_set (comp, "show-headers",
		      gtk_toggle_button_get_active (button),
		      NULL);
}

static gboolean
key_press (GtkWidget   *widget,
	   GdkEventKey *event,
	   gpointer     user_data)
{
	if (event->keyval == GDK_F8)
	{
		GtkSourceCompletionInfo *gsc_info;
		
		gsc_info = gtk_source_completion_get_info_window (comp);
		
		if (GTK_WIDGET_VISIBLE (GTK_WIDGET (gsc_info)))
			gtk_widget_hide (GTK_WIDGET (gsc_info));
		else
			gtk_widget_show (GTK_WIDGET (gsc_info));
	}
	
	return FALSE;
}

static void
toggle_active_property (gpointer     source,
                        gpointer     dest,
                        const gchar *name)
{
	gboolean value;
	
	g_object_get (source, name, &value, NULL);
	g_object_set (dest, "active", value, NULL);
}

static GtkWidget*
create_window (void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *remember;
	GtkWidget *select_on_show;
	GtkWidget *show_headers;
	GtkSourceCompletion *completion;
		
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_resize (GTK_WINDOW (window), 600, 400);
	
	vbox = gtk_vbox_new (FALSE, 1);
	hbox = gtk_hbox_new (FALSE, 1);
	
	view = gtk_source_view_new ();
	GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroll), view);
	
	remember = gtk_check_button_new_with_label ("Remember info visibility");
	select_on_show = gtk_check_button_new_with_label ("Select first on show");
	show_headers = gtk_check_button_new_with_label ("Show headers");
	
	completion = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
	
	toggle_active_property (completion, remember, "remember-info-visibility");
	toggle_active_property (completion, select_on_show, "select-on-show");
	toggle_active_property (completion, show_headers, "show-headers");
	
	gtk_box_pack_start (GTK_BOX (hbox), remember, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), select_on_show, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), show_headers, FALSE, FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	g_signal_connect (view, "key-release-event",
			  G_CALLBACK (key_press),
			  NULL);
	g_signal_connect (window, "destroy",
			  G_CALLBACK (destroy_cb),
			   NULL);
	g_signal_connect (remember, "toggled",
			  G_CALLBACK (remember_toggled_cb),
			  NULL);
	g_signal_connect (select_on_show, "toggled",
			  G_CALLBACK (select_on_show_toggled_cb),
			  NULL);
	g_signal_connect (show_headers, "toggled",
			  G_CALLBACK (show_headers_toggled_cb),
			  NULL);

	return window;
}

static void
create_completion(void)
{
	GscProviderWords *prov_words;
	
	comp = gtk_source_view_get_completion (GTK_SOURCE_VIEW (view));
	prov_words = gsc_provider_words_new (GTK_SOURCE_VIEW (view));
	
	gtk_source_completion_add_provider (comp, 
	                                    GTK_SOURCE_COMPLETION_PROVIDER (prov_words), 
	                                    NULL);

#ifdef HAVE_DEVHELP
	GscProviderDevhelp *prov_devhelp;

	prov_devhelp = gsc_provider_devhelp_new ();
	
	gtk_source_completion_add_provider (comp, 
	                                    GTK_SOURCE_COMPLETION_PROVIDER (prov_devhelp), 
	                                    NULL);
#endif
}

int
main (int argc, char *argv[])
{
 	GtkWidget *window;
	
	gtk_set_locale ();
	gtk_init (&argc, &argv);

	window = create_window ();
	create_completion ();
	
	gtk_widget_show_all (window);

	gtk_main ();
	return 0;
}

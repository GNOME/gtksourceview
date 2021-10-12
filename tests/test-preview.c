#include <gtksourceview/gtksource.h>

static gboolean
close_request (GtkWidget *window,
               GMainLoop *main_loop)
{
  g_main_loop_quit (main_loop);
  return FALSE;
}

static void
on_activate (GtkSourceStyleSchemePreview *preview,
             GtkBox                      *box)
{
	GtkSourceStyleScheme *scheme;

	g_assert (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (preview));
	g_assert (GTK_IS_BOX (box));

	scheme = gtk_source_style_scheme_preview_get_scheme (preview);

	g_print ("Selected: %s\n", gtk_source_style_scheme_get_name (scheme));

	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (box));
	     child;
	     child = gtk_widget_get_next_sibling (child))
	{
		gtk_source_style_scheme_preview_set_selected (GTK_SOURCE_STYLE_SCHEME_PREVIEW (child), FALSE);
	}

	gtk_source_style_scheme_preview_set_selected (preview, TRUE);
}

int
main (int argc,
      char *argv[])
{
	GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
	GtkSourceStyleSchemeManager *manager;
	const char * const *ids;
	GtkScrolledWindow *scroller;
	GtkWindow *window;
	GtkBox *box;

	gtk_init ();
	gtk_source_init ();

	manager = gtk_source_style_scheme_manager_get_default ();
	ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

	window = g_object_new (GTK_TYPE_WINDOW,
	                       "default-width", 120,
	                       "default-height", 500,
	                       NULL);
	scroller = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
	                         "propagate-natural-width", TRUE,
	                         "hscrollbar-policy", GTK_POLICY_NEVER,
	                         "min-content-height", 250,
	                         NULL);
	box = g_object_new (GTK_TYPE_BOX,
	                    "margin-top", 12,
	                    "margin-bottom", 12,
	                    "margin-start", 12,
	                    "margin-end", 12,
	                    "orientation", GTK_ORIENTATION_VERTICAL,
	                    "spacing", 12,
	                    NULL);

	for (guint i = 0; ids[i]; i++)
	{
		GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, ids[i]);
		GtkWidget *preview = gtk_source_style_scheme_preview_new (scheme);

		g_signal_connect (preview, "activate", G_CALLBACK (on_activate), box);
		gtk_box_append (box, preview);
	}

	gtk_window_set_child (window, GTK_WIDGET (scroller));
	gtk_scrolled_window_set_child (scroller, GTK_WIDGET (box));

	gtk_window_present (window);
	g_signal_connect (window, "close-request", G_CALLBACK (close_request), main_loop);
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	return 0;
}

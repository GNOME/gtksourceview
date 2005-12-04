/*
 * This is a test of gtksourceview's capabilities. Notice the new highlighting
 * engine that supports sub-languages like GTK+-like @ comments.
 */

#include <gtk/gtk.h>

#if 0

#if GTK_SOURCE_VIEW_IS_SMART_ABOUT_NESTED_IFDEFS
#define HOORAY
#else
#define GET_A_REAL_EDITOR
#endif

#endif /* #if 0 */

static void
doit (void)
{
	g_message ("Just do it!");
}

/**
 * main:
 * @argc: the number of command-line arguments given to the program + 1.
 * @argv: the arguments specified on the command-line.
 *
 * Displays a basic #GtkWindow.
 *
 * Return value: 0 if the program completed succesfully.
 **/
int
main (int argc, char *argv[])
{
	GtkWidget *window;
	gchar *title;
	
	gtk_init (&argc, &argv);
	
	title = "GtkSourceView test";
	
	/* There should be a breakpoint on the next line. */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	/* Try hovering over "title" to see it's value (tooltip). */
	gtk_window_set_title (GTK_WINDOW (window), title);
	
	gtk_window_set_default_size (GTK_WINDOW (window), 500, 500);
	
	if (DEBUG) {
		if (argc > 1) {
			g_message ("%d arguments where given at startup", argc - 1);
		}
	}
	
	/* The next line is bookmarked. */
	gtk_widget_show_all (window);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	gtk_main ();
	
	return 0;
}

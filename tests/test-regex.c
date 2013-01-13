#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourceregex.h"

static void
test_slash_c_pattern (void)
{
	GtkSourceRegex *regex;
	GError *error = NULL;

	regex = _gtk_source_regex_new ("\\C", 0, &error);
	g_assert_error (error, G_REGEX_ERROR, G_REGEX_ERROR_COMPILE);
	g_assert (regex == NULL);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Regex/slash-c", test_slash_c_pattern);

	return g_test_run();
}

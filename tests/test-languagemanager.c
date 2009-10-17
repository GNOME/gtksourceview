#include "config.h"
#include <string.h>

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcelanguagemanager.h>

static void
test_guess_language (void)
{
	GtkSourceLanguageManager *lm;
	GtkSourceLanguage *l;

	lm = gtk_source_language_manager_get_default ();

	if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
	{
		l = gtk_source_language_manager_guess_language (lm, NULL, NULL);
	}
	g_test_trap_assert_failed ();

	l = gtk_source_language_manager_guess_language (lm, "foo.abcdef", NULL);
	g_assert (l == NULL);

	l = gtk_source_language_manager_guess_language (lm, NULL, "image/png");
	g_assert (l == NULL);

	l = gtk_source_language_manager_guess_language (lm, "foo.c", NULL);
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, NULL, "text/x-csrc");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	l = gtk_source_language_manager_guess_language (lm, "foo.c", "text/x-csrc");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	/* when in disagreement, glob wins */
	l = gtk_source_language_manager_guess_language (lm, "foo.c", "text/x-fortran");
	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "c");

	/* when content type is a descendent of the mime matched by the glob, mime wins */
//	FIXME: this fails with the current logic
//	l = gtk_source_language_manager_guess_language (lm, "foo.xml", "application/xslt+xml");
//	g_assert_cmpstr (gtk_source_language_get_id (l), ==, "xslt");
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/LanguageManager/guess-language", test_guess_language);

	return g_test_run();
}

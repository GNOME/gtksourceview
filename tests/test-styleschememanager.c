#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

static void
test_get_default (void)
{
	GtkSourceStyleSchemeManager *sm1, *sm2;

	sm1 = gtk_source_style_scheme_manager_get_default ();
	sm2 = gtk_source_style_scheme_manager_get_default ();
	g_assert (sm1 == sm2);
}

static void
test_prepend_search_path (void)
{
	GtkSourceStyleSchemeManager *sm;
	gchar *style_dir;
	GtkSourceStyleScheme *scheme;
	const gchar *fname;
	gchar *expected;

	sm = gtk_source_style_scheme_manager_get_default ();

	style_dir = g_build_filename (TOP_SRCDIR, "tests", "styles", NULL);
	gtk_source_style_scheme_manager_prepend_search_path (sm, style_dir);

	scheme = gtk_source_style_scheme_manager_get_scheme (sm, "classic");
	fname = gtk_source_style_scheme_get_filename (scheme);
	expected = g_build_filename (style_dir, "classic.xml", NULL);
	g_assert_cmpstr (fname, ==, expected);

	g_free (expected);
	g_free (style_dir);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/StyleSchemeManager/get-default", test_get_default);
	g_test_add_func ("/StyleSchemeManager/prepend-search-path", test_prepend_search_path);

	return g_test_run();
}

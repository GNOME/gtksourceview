#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

typedef struct _TestFixture TestFixture;

struct _TestFixture {
	GtkSourceLanguageManager *manager;
};

static void
test_fixture_setup (TestFixture   *fixture,
                    gconstpointer  data)
{
	gchar **lang_dirs;

	fixture->manager = gtk_source_language_manager_get_default ();

	lang_dirs = g_new0 (gchar *, 3);
	lang_dirs[0] = g_build_filename (TOP_SRCDIR, "tests", "language-specs", NULL);
	lang_dirs[1] = g_build_filename (TOP_SRCDIR, "data", "language-specs", NULL);

	gtk_source_language_manager_set_search_path (fixture->manager, lang_dirs);
	g_strfreev (lang_dirs);
}

static void
test_fixture_teardown (TestFixture   *fixture,
                       gconstpointer  data)
{
}

static void
compare_strv (gchar **strv,
              gchar **expected_strv)
{
	if (expected_strv != NULL)
	{
		guint n, i;

		n = g_strv_length (expected_strv);
		for (i = 0; i < n; i++)
		{
			g_assert_cmpstr (strv[i], ==, expected_strv[i]);
		}
	}
	else
	{
		g_assert (strv == NULL);
	}
}

static void
check_language (GtkSourceLanguage  *language,
                const char         *id,
                const char         *expected_name,
                const char         *expected_section,
                gboolean            expected_hidden,
                const char         *expected_extra_meta,
                gchar             **expected_mime,
                gchar             **expected_glob,
                gchar             **expected_styles,
		const char         *style_id,
                const char         *expected_style_name)
{
	gchar **mime;
	gchar **glob;
	gchar **styles;

	g_assert_cmpstr (gtk_source_language_get_id (language), ==, id);
	g_assert_cmpstr (gtk_source_language_get_name (language), ==, expected_name);
	g_assert_cmpstr (gtk_source_language_get_section (language), ==, expected_section);
	g_assert (gtk_source_language_get_hidden (language) == expected_hidden);
	g_assert_cmpstr (gtk_source_language_get_metadata (language, "extra-meta"), ==, expected_extra_meta);

	mime = gtk_source_language_get_mime_types (language);
	compare_strv (mime, expected_mime);
	g_strfreev (mime);

	glob = gtk_source_language_get_globs (language);
	compare_strv (glob, expected_glob);
	g_strfreev (glob);

	styles = gtk_source_language_get_style_ids (language);
	compare_strv (styles, expected_styles);
	g_strfreev (styles);

	if (expected_style_name != NULL)
	{
		g_assert_cmpstr (gtk_source_language_get_style_name (language, style_id), ==, expected_style_name);
	}
}

static void
test_language (TestFixture   *fixture,
               gconstpointer  data)
{
	GtkSourceLanguage *language;

	language = gtk_source_language_manager_get_language (fixture->manager, "test-full");
	gchar *mime[] = { "text/x-test", "application/x-test", NULL};
	gchar *glob[] = { "*.test", "*.tst", NULL};
	gchar *styles[] = { "test-full:keyword", "test-full:string", NULL};
	check_language (language, "test-full", "Test Full", "Sources", FALSE, "extra", mime, glob, styles, "test-full:string", "String");

	language = gtk_source_language_manager_get_language (fixture->manager, "test-empty");
	check_language (language, "test-empty", "Test Empty", "Others", TRUE, NULL, NULL, NULL, NULL, NULL, NULL);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add ("/Language/language-properties", TestFixture, NULL, test_fixture_setup, test_language, test_fixture_teardown);

	return g_test_run();
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

static const char *c_snippet =
	"#include <foo.h>\n"
	"\n"
	"/* this is a comment */\n"
	"int main() {\n"
	"}\n";

/* If we are running from the source dir (e.g. during make check)
 * we override the path to read from the data dir
 */
static void
init_default_manager (void)
{
	gchar *dir;

	dir = g_build_filename (TOP_SRCDIR, "data", "language-specs", NULL);

	if (g_file_test (dir, G_FILE_TEST_IS_DIR))
	{
		GtkSourceLanguageManager *lm;
		gchar **lang_dirs;

		lm = gtk_source_language_manager_get_default ();

		lang_dirs = g_new0 (gchar *, 2);
		lang_dirs[0] = dir;

		gtk_source_language_manager_set_search_path (lm, lang_dirs);
		g_strfreev (lang_dirs);
	}
	else
	{
		g_free (dir);
	}
}

static void
test_get_buffer (void)
{
	GtkWidget *view;
	GtkSourceBuffer *buffer;

	view = gtk_source_view_new ();

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	g_assert (buffer != NULL);
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));

	if (g_object_is_floating (view))
	{
		g_object_ref_sink (view);
	}

	/* Here we check if notify_buffer recreates the buffer while view is being
	 * destroyed, which causes assertion failure in GtkTextView's finalize ()
	 * function.
	 * Please see: https://bugzilla.gnome.org/show_bug.cgi?id=634510 */
	g_object_unref (view);
}

static void
test_get_context_classes (void)
{
	GtkSourceLanguageManager *lm;
	GtkSourceBuffer *buffer;
	GtkSourceLanguage *lang;
	GtkTextIter start, end, i;
	char **classes;

	/* test plain text */
	buffer = gtk_source_buffer_new (NULL);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "some text", -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_source_buffer_ensure_highlight (buffer, &start, &end);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &i);
	classes = gtk_source_buffer_get_context_classes_at_iter (buffer, &i);
	g_assert_cmpuint (g_strv_length (classes), ==, 0);
	g_strfreev (classes);

	g_object_unref (buffer);

	/* test C */
	lm = gtk_source_language_manager_get_default ();
	lang = gtk_source_language_manager_get_language (lm, "c");
	g_assert (GTK_SOURCE_IS_LANGUAGE (lang));
	buffer = gtk_source_buffer_new_with_language (lang);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), c_snippet, -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_source_buffer_ensure_highlight (buffer, &start, &end);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &i);
	classes = gtk_source_buffer_get_context_classes_at_iter (buffer, &i);
	g_assert_cmpuint (g_strv_length (classes), ==, 1);
	g_assert_cmpstr (classes[0], ==, "no-spell-check");
	g_strfreev (classes);

	gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &i, 2, 5);
	classes = gtk_source_buffer_get_context_classes_at_iter (buffer, &i);
	g_assert_cmpuint (g_strv_length (classes), ==, 1);
	g_assert_cmpstr (classes[0], ==, "comment");
	g_strfreev (classes);

	g_object_unref (buffer);
}

static void
do_test_change_case (GtkSourceBuffer         *buffer,
		     GtkSourceChangeCaseType  case_type,
		     const gchar             *text,
		     const gchar             *expected)
{
	GtkTextIter start;
	GtkTextIter end;
	gchar *changed;
	gchar *changed_normalized;
	gchar *expected_normalized;

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_source_buffer_change_case (buffer, case_type, &start, &end);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	changed = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);

	changed_normalized = g_utf8_normalize (changed, -1, G_NORMALIZE_DEFAULT);
	expected_normalized = g_utf8_normalize (expected, -1, G_NORMALIZE_DEFAULT);

	g_assert_cmpstr (changed_normalized, ==, expected_normalized);

	g_free (changed);
	g_free (changed_normalized);
	g_free (expected_normalized);
}

static void
test_change_case (void)
{
	GtkSourceBuffer *buffer;

	buffer = gtk_source_buffer_new (NULL);

	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "some TEXT", "some text");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_UPPER, "some TEXT", "SOME TEXT");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, "some TEXT", "SOME text");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "some TEXT", "Some Text");

	/* https://bugzilla.gnome.org/show_bug.cgi?id=416390 */
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "T̈OME", "ẗome");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_UPPER, "ẗome", "T̈OME");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, "ẗome", "T̈OME");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, "T̈OME", "ẗome");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "ẗome", "T̈ome");

	/* test g_unichar_totitle */
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "\307\261adzíki", "\307\263adzíki");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "\307\262adzíki", "\307\263adzíki");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "\307\263adzíki", "\307\263adzíki");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_UPPER, "\307\263adzíki", "\307\261ADZÍKI");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_UPPER, "\307\262adzíki", "\307\261ADZÍKI");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, "\307\263adzíki", "\307\261ADZÍKI");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "\307\263adzíki", "\307\262adzíki");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "\307\261ADZÍKI", "\307\262adzíki");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "\307\262ADZÍKI", "\307\262adzíki");

	g_object_unref (buffer);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	init_default_manager ();

	g_test_add_func ("/Buffer/bug-634510", test_get_buffer);
	g_test_add_func ("/Buffer/get-context-classes", test_get_context_classes);
	g_test_add_func ("/Buffer/change-case", test_change_case);

	return g_test_run();
}

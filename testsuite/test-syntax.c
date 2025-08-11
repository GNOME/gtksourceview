/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourcebuffer-private.h>

static void
setup_search_paths (const char *basedir)
{
	GtkSourceLanguageManager *languages;
	GtkSourceStyleSchemeManager *styles;
	gchar *langs_path[2] = { NULL };
	gchar *styles_path;

	styles_path = g_build_filename (basedir, "data", "style", NULL);
	styles = gtk_source_style_scheme_manager_get_default ();
	gtk_source_style_scheme_manager_prepend_search_path (styles, styles_path);
	g_free (styles_path);

	langs_path[0] = g_build_filename (basedir, "data", "language-specs", NULL);
	languages = gtk_source_language_manager_get_default ();
	gtk_source_language_manager_set_search_path (languages, (const char * const *)langs_path);
	g_free (langs_path[0]);
}

static void
test_syntax (gconstpointer data)
{
	const char *filename = data;
	GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default ();
	GtkSourceStyleSchemeManager *sm = gtk_source_style_scheme_manager_get_default ();
	GtkSourceStyleScheme *s = gtk_source_style_scheme_manager_get_scheme (sm, "Adwaita");
	GtkSourceLanguage *l;
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;
	char *content_type;
	GError *error = NULL;
	char *contents;
	gsize len;

	g_file_get_contents (filename, &contents, &len, &error);
	g_assert_no_error (error);
	g_assert_nonnull (contents);
	g_assert_cmpint (len, >, 0);

	g_print ("%s\n", filename);
	content_type = g_content_type_guess (filename, (const guint8 *)contents, len, NULL);
	l = gtk_source_language_manager_guess_language (lm, filename, content_type);

	if (l == NULL)
	{
		char *message = g_strdup_printf ("Skipping because cannot guess language for %s", filename);
		g_test_skip (message);
		g_free (message);
		goto cleanup;
	}

	buffer = gtk_source_buffer_new (NULL);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), contents, len);
	gtk_source_buffer_set_language (buffer, l);
	gtk_source_buffer_set_highlight_syntax (buffer, TRUE);
	gtk_source_buffer_set_style_scheme (buffer, s);

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	_gtk_source_buffer_update_syntax_highlight (buffer, &begin, &end, TRUE);

	g_object_unref (buffer);

	g_assert_nonnull (l);

cleanup:
	g_free (content_type);
	g_free (contents);
}

gint
main (gint   argc,
      gchar *argv[])
{
	const char *srcdir = g_getenv ("G_TEST_SRCDIR");
	char *path = g_build_filename (srcdir, "..", "tests", "syntax-highlighting", NULL);
	GDir *dir;
	GError *error = NULL;
	const char *name;

	g_assert_nonnull (srcdir);

	g_test_init (&argc, &argv, NULL);

	setup_search_paths (srcdir);

	dir = g_dir_open (path, 0, &error);
	g_assert_no_error (error);
	g_assert_nonnull (dir);

	while ((name = g_dir_read_name (dir)))
	{
		if (g_str_has_prefix (name, "file."))
		{
			char *testpath = g_strdup_printf ("/syntax-highlighting/%s", name);
			char *filename = g_build_filename (path, name, NULL);
			g_test_add_data_func_full (testpath, filename, test_syntax, g_free);
			g_free (testpath);
		}
	}

	g_dir_close (dir);
	g_free (path);

	return g_test_run ();
}

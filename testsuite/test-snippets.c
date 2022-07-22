/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#include "config.h"

#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourceinit.h>

static const gchar *data_search_path[] = {
	TOP_SRCDIR"/data/snippets",
	NULL
};
static const char *testsuite_search_path[] = {
	TOP_SRCDIR"/testsuite/snippets",
	NULL
};

static void
test_simple (void)
{
	GtkSourceSnippetManager *mgr;
	GtkSourceSnippet *snippet;
	const gchar **groups;

	mgr = g_object_new (GTK_SOURCE_TYPE_SNIPPET_MANAGER, NULL);
	gtk_source_snippet_manager_set_search_path (mgr, data_search_path);

	/* Update if you add new groups to data/snippets/ */
	groups = gtk_source_snippet_manager_list_groups (mgr);
	g_assert_cmpint (1, ==, g_strv_length ((gchar **)groups));
	g_assert_cmpstr (groups[0], ==, "Licenses");
	g_free (groups);

	/* Make sure we can get gpl3 snippet for C language */
	snippet = gtk_source_snippet_manager_get_snippet (mgr, NULL, "c", "gpl3");
	g_assert_nonnull (snippet);
	g_assert_finalize_object (snippet);

	g_assert_finalize_object (mgr);
}

static void
test_snippet_fetching (void)
{
	GtkSourceSnippetManager *mgr;
	GListModel *model;
	guint n_items;

	mgr = g_object_new (GTK_SOURCE_TYPE_SNIPPET_MANAGER, NULL);
	gtk_source_snippet_manager_set_search_path (mgr, testsuite_search_path);

	model = gtk_source_snippet_manager_list_all (mgr);
	n_items = g_list_model_get_n_items (model);

	/* Test language id for snippets */
	for (guint i = 0; i < n_items; i++)
	{
		GtkSourceSnippet *snippet = g_list_model_get_item (model, i);
		const char *language_id = gtk_source_snippet_get_language_id (snippet);

		g_assert_nonnull (language_id);
		g_assert_cmpstr (language_id, !=, "");

		g_object_unref (snippet);
	}

	g_assert_finalize_object (mgr);
}

static void
test_snippet_parse (void)
{
	GtkSourceSnippet *snippet;
	GtkSourceSnippetChunk *chunk;
	GError *error = NULL;

	snippet = gtk_source_snippet_new_parsed ("${1:test} ${2:$1}$0", &error);
	g_assert_no_error (error);
	g_assert_nonnull (snippet);

	g_assert_cmpint (4, ==, gtk_source_snippet_get_n_chunks (snippet));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 0);
	g_assert_nonnull (chunk);
	g_assert_cmpint (1, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("test", ==, gtk_source_snippet_chunk_get_spec (chunk));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 1);
	g_assert_nonnull (chunk);
	g_assert_cmpint (-1, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr (" ", ==, gtk_source_snippet_chunk_get_spec (chunk));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 2);
	g_assert_nonnull (chunk);
	g_assert_cmpint (2, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("$1", ==, gtk_source_snippet_chunk_get_spec (chunk));
	/* Unset until user types */
	g_assert_cmpstr ("", ==, gtk_source_snippet_chunk_get_text (chunk));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 3);
	g_assert_nonnull (chunk);
	g_assert_cmpint (0, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("", ==, gtk_source_snippet_chunk_get_spec (chunk));

	g_assert_finalize_object (snippet);
}

static void
test_snippet_parse_issue_252 (void)
{
	GtkSourceSnippet *snippet;
	GtkSourceSnippetChunk *chunk;
	GError *error = NULL;

	snippet = gtk_source_snippet_new_parsed ("a\n$0\nb", &error);
	g_assert_no_error (error);
	g_assert_nonnull (snippet);

	g_assert_cmpint (3, ==, gtk_source_snippet_get_n_chunks (snippet));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 0);
	g_assert_nonnull (chunk);
	g_assert_cmpint (-1, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("a\n", ==, gtk_source_snippet_chunk_get_spec (chunk));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 1);
	g_assert_nonnull (chunk);
	g_assert_cmpint (0, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("", ==, gtk_source_snippet_chunk_get_spec (chunk));

	chunk = gtk_source_snippet_get_nth_chunk (snippet, 2);
	g_assert_nonnull (chunk);
	g_assert_cmpint (-1, ==, gtk_source_snippet_chunk_get_focus_position (chunk));
	g_assert_cmpstr ("\nb", ==, gtk_source_snippet_chunk_get_spec (chunk));

	g_assert_finalize_object (snippet);
}

gint
main (gint argc,
      gchar *argv[])
{
	int ret;

	gtk_init ();
	gtk_source_init ();
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/SourceView/Snippets/parse-bundle", test_simple);
	g_test_add_func ("/SourceView/Snippets/new-parsed", test_snippet_parse);
	g_test_add_func ("/SourceView/Snippets/$0-in-middle", test_snippet_parse_issue_252);
	g_test_add_func ("/SourceView/Snippets/snippet-fetching", test_snippet_fetching);
	ret = g_test_run ();

	gtk_source_finalize ();

	return ret;
}

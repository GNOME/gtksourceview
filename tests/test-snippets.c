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

static const gchar *search_path[] = {
	TOP_SRCDIR"/data/snippets",
	NULL
};

gint
main (gint argc,
      gchar *argv[])
{
	GtkSourceSnippetManager *mgr;
	GtkSourceSnippet *snippet;
	const gchar **groups;

	gtk_init ();
	gtk_source_init ();

	mgr = gtk_source_snippet_manager_get_default ();
	gtk_source_snippet_manager_set_search_path (mgr, search_path);

	/* Update if you add new groups to data/snippets/ */
	groups = gtk_source_snippet_manager_list_groups (mgr);
	g_assert_cmpint (1, ==, g_strv_length ((gchar **)groups));
	g_assert_cmpstr (groups[0], ==, "Licenses");
	g_free (groups);

	/* Make sure we can get gpl3 snippet for C language */
	snippet = gtk_source_snippet_manager_get_snippet (mgr, NULL, "c", "gpl3");
	g_assert_nonnull (snippet);
	g_assert_finalize_object (snippet);

	gtk_source_finalize ();

	return 0;
}

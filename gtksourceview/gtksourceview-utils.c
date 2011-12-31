/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourceviewutils.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 - Gustavo Giráldez and Paolo Maggi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include "gtksourceview-utils.h"

#define SOURCEVIEW_DIR "gtksourceview-3.0"


gchar **
_gtk_source_view_get_default_dirs (const char *basename,
				   gboolean    compat)
{
	const gchar * const *xdg_dirs;
	GPtrArray *dirs;

	dirs = g_ptr_array_new ();

	/* user dir */
	g_ptr_array_add (dirs, g_build_filename (g_get_user_data_dir (),
						 SOURCEVIEW_DIR,
						 basename,
						 NULL));

#ifdef G_OS_UNIX
	/* Legacy gtsourceview 1 user dir, for backward compatibility */
	if (compat)
	{
		const gchar *home;

		home = g_get_home_dir ();
		if (home != NULL)
		{
			g_ptr_array_add (dirs,
					 g_strdup_printf ("%s/%s",
							  home,
							  ".gnome2/gtksourceview-1.0/language-specs"));
		}
	}
#endif

	/* system dir */
	for (xdg_dirs = g_get_system_data_dirs (); xdg_dirs && *xdg_dirs; ++xdg_dirs)
		g_ptr_array_add (dirs, g_build_filename (*xdg_dirs,
							 SOURCEVIEW_DIR,
							 basename,
							 NULL));

	g_ptr_array_add (dirs, NULL);

	return (gchar**) g_ptr_array_free (dirs, FALSE);
}

static GSList *
build_file_listing (const gchar *item,
		    GSList      *filenames,
		    const gchar *suffix,
		    gboolean     only_dirs)
{
	GDir *dir;
	const gchar *name;

	if (!only_dirs && g_file_test (item, G_FILE_TEST_IS_REGULAR))
	{
		filenames = g_slist_prepend (filenames, g_strdup(item));
		return filenames;

	}
	dir = g_dir_open (item, 0, NULL);

	if (dir == NULL)
		return filenames;

	while ((name = g_dir_read_name (dir)) != NULL)
	{
		gchar *full_path = g_build_filename (item, name, NULL);

		if (!g_file_test (full_path, G_FILE_TEST_IS_DIR) &&
		    g_str_has_suffix (name, suffix))
		{
			filenames = g_slist_prepend (filenames, full_path);
		}
		else
		{
			g_free (full_path);
		}
	}

	g_dir_close (dir);

	return filenames;
}

GSList *
_gtk_source_view_get_file_list (gchar       **path,
				const gchar  *suffix,
				gboolean      only_dirs)
{
	GSList *files = NULL;

	for ( ; path && *path; ++path)
		files = build_file_listing (*path, files, suffix, only_dirs);

	return g_slist_reverse (files);
}

/* Wrapper around strtoull for easier use: tries
 * to convert @str to a number and return -1 if it is not.
 * Used to check if references in subpattern contexts
 * (e.g. \%{1@start} or \%{blah@start}) are named or numbers.
 */
gint
_gtk_source_string_to_int (const gchar *str)
{
	guint64 number;
	gchar *end_str;

	if (str == NULL || *str == '\0')
		return -1;

	errno = 0;
	number = g_ascii_strtoull (str, &end_str, 10);

	if (errno != 0 || number > G_MAXINT || *end_str != '\0')
		return -1;

	return number;
}


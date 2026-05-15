/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#include <gtksourceview/gtksource.h>

#include "gtksourceview/gtksourceutils-private.h"

static void
test_unescape_search_text (void)
{
	gchar *unescaped_text;

	unescaped_text = gtk_source_utils_unescape_search_text ("\\n");
	g_assert_cmpstr (unescaped_text, ==, "\n");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\r");
	g_assert_cmpstr (unescaped_text, ==, "\r");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\t");
	g_assert_cmpstr (unescaped_text, ==, "\t");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\\\");
	g_assert_cmpstr (unescaped_text, ==, "\\");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("foo\\n bar\\r ß\\t hello\\\\blah");
	g_assert_cmpstr (unescaped_text, ==, "foo\n bar\r ß\t hello\\blah");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("foo\n bar\r ß\t hello\\blah");
	g_assert_cmpstr (unescaped_text, ==, "foo\n bar\r ß\t hello\\blah");
	g_free (unescaped_text);

	unescaped_text = gtk_source_utils_unescape_search_text ("\\n \\1");
	g_assert_cmpstr (unescaped_text, ==, "\n \\1");
	g_free (unescaped_text);
}

static void
test_escape_search_text (void)
{
	gchar *escaped_text;

	escaped_text = gtk_source_utils_escape_search_text ("\n");
	g_assert_cmpstr (escaped_text, ==, "\\n");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\r");
	g_assert_cmpstr (escaped_text, ==, "\\r");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\t");
	g_assert_cmpstr (escaped_text, ==, "\\t");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("\\");
	g_assert_cmpstr (escaped_text, ==, "\\\\");
	g_free (escaped_text);

	escaped_text = gtk_source_utils_escape_search_text ("foo\n bar\r ß\t hello\\blah");
	g_assert_cmpstr (escaped_text, ==, "foo\\n bar\\r ß\\t hello\\\\blah");
	g_free (escaped_text);
}

static void
test_checked_add_gsize (void)
{
	gsize result;

	g_assert_true (_gtk_source_utils_checked_add_gsize (1, 2, &result));
	g_assert_cmpuint (result, ==, 3);

	g_assert_true (_gtk_source_utils_checked_add_gsize (G_MAXSIZE, 0, &result));
	g_assert_cmpuint (result, ==, G_MAXSIZE);

	g_assert_false (_gtk_source_utils_checked_add_gsize (G_MAXSIZE, 1, &result));
}

static void
test_checked_mul_gsize (void)
{
	gsize result;

	g_assert_true (_gtk_source_utils_checked_mul_gsize (2, 3, &result));
	g_assert_cmpuint (result, ==, 6);

	g_assert_true (_gtk_source_utils_checked_mul_gsize (0, G_MAXSIZE, &result));
	g_assert_cmpuint (result, ==, 0);

	g_assert_false (_gtk_source_utils_checked_mul_gsize (G_MAXSIZE, 2, &result));
}

static void
test_checked_add_goffset (void)
{
	goffset result;

	g_assert_true (_gtk_source_utils_checked_add_goffset (1, 2, &result));
	g_assert_cmpint (result, ==, 3);

	g_assert_true (_gtk_source_utils_checked_add_goffset (G_MAXINT64, 0, &result));
	g_assert_cmpint (result, ==, G_MAXINT64);

	g_assert_false (_gtk_source_utils_checked_add_goffset (G_MAXINT64, 1, &result));
	g_assert_false (_gtk_source_utils_checked_add_goffset (-1, 1, &result));
	g_assert_false (_gtk_source_utils_checked_add_goffset (1, -1, &result));
}

static void
test_checked_signed_to_unsigned (void)
{
	gsize size_result;
	goffset offset_result;

	g_assert_true (_gtk_source_utils_checked_gssize_to_gsize (0, &size_result));
	g_assert_cmpuint (size_result, ==, 0);

	g_assert_true (_gtk_source_utils_checked_gssize_to_goffset (G_MAXSSIZE, &offset_result));
	g_assert_cmpint (offset_result, ==, G_MAXSSIZE);

	g_assert_false (_gtk_source_utils_checked_gssize_to_gsize (-1, &size_result));
	g_assert_false (_gtk_source_utils_checked_gssize_to_goffset (-1, &offset_result));
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Utils/unescape_search_text", test_unescape_search_text);
	g_test_add_func ("/Utils/escape_search_text", test_escape_search_text);
	g_test_add_func ("/Utils/checked_add_gsize", test_checked_add_gsize);
	g_test_add_func ("/Utils/checked_mul_gsize", test_checked_mul_gsize);
	g_test_add_func ("/Utils/checked_add_goffset", test_checked_add_goffset);
	g_test_add_func ("/Utils/checked_signed_to_unsigned", test_checked_signed_to_unsigned);

	return g_test_run ();
}

/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Paolo Borelli
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

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourceregex-private.h"

#include "gtksourceview/implregex-private.h"

static void
test_slash_c_pattern (void)
{
	GtkSourceRegex *regex;
	GError *error = NULL;

	regex = _gtk_source_regex_new ("\\C", 0, &error);
	g_assert_error (error, G_REGEX_ERROR, G_REGEX_ERROR_COMPILE);
	g_assert_null (regex);
	g_error_free (error);
}

static void
assert_iterations (GMatchInfo    *mi1,
                   ImplMatchInfo *mi2)
{
	gboolean r1;
	gboolean r2;
	gboolean next1;
	gboolean next2;
	GError *err1 = NULL;
	GError *err2 = NULL;

	for (;;)
	{
		gboolean matches1 = g_match_info_matches (mi1);
		gboolean matches2 = impl_match_info_matches (mi2);
		int count1, count2;

		g_assert_cmpint (matches1, ==, matches2);

		if (!matches1)
			break;

		count1 = g_match_info_get_match_count (mi1);
		count2 = impl_match_info_get_match_count (mi2);
		g_assert_cmpint (count1, ==, count2);

		/* Check past boundaries for correctness */
		for (int i = 0; i < count1 + 2; i++)
		{
			int p1_begin = -123, p2_begin = -123;
			int p1_end = -123, p2_end = -123;
			char *str1, *str2;

			r1 = g_match_info_fetch_pos (mi1, i, &p1_begin, &p1_end);
			r2 = impl_match_info_fetch_pos (mi2, i, &p2_begin, &p2_end);
			g_assert_cmpint (r1, ==, r2);
			g_assert_cmpint (p1_begin, ==, p2_begin);
			g_assert_cmpint (p1_end, ==, p2_end);

			str1 = g_match_info_fetch (mi1, i);
			str2 = impl_match_info_fetch (mi2, i);
			g_assert_cmpstr (str1, ==, str2);

			g_free (str1);
			g_free (str2);
		}

		g_assert_cmpint (g_match_info_is_partial_match (mi1),
		                 ==,
		                 impl_match_info_is_partial_match (mi2));

		next1 = g_match_info_next (mi1, &err1);
		next2 = impl_match_info_next (mi2, &err2);
		g_assert_cmpint (next1, ==, next2);
		g_assert_true (err1 == NULL || err2 != NULL);
	}

	g_assert_false (g_match_info_matches (mi1));
	g_assert_false (impl_match_info_matches (mi2));
}

static void
compare_impl_regex_to_g_regex (const char         *subject,
                               const char         *pattern,
                               GRegexCompileFlags  compile_flags,
                               GRegexMatchFlags    match_flags)
{
	GError *err1 = NULL;
	GError *err2 = NULL;
	GRegex *reg1 = g_regex_new (pattern, compile_flags, 0, &err1);
	/* Disable JIT for ImplRegex, as it is not as flexible for search */
	ImplRegex *reg2 = impl_regex_new (pattern, compile_flags & ~G_REGEX_OPTIMIZE, 0, &err2);
	GMatchInfo *mi1 = NULL;
	ImplMatchInfo *mi2 = NULL;
	gboolean r1, r2;
	int subject_len = strlen (subject);

	g_assert_true ((reg1 == NULL && reg2 == NULL) ||
	               (reg1 != NULL && reg2 != NULL));
	g_assert_cmpstr (g_regex_get_pattern (reg1),
	                 ==,
	                 impl_regex_get_pattern (reg2));
	g_assert_cmpint (g_regex_get_max_lookbehind (reg1),
	                 ==,
	                 impl_regex_get_max_lookbehind (reg2));

	r1 = g_regex_match (reg1, subject, match_flags, &mi1);
	r2 = impl_regex_match (reg2, subject, match_flags, &mi2);
	g_assert_cmpint (r1, ==, r2);
	g_assert_true (err1 == NULL || err2 != NULL);
	assert_iterations (mi1, mi2);
	g_clear_pointer (&mi1, g_match_info_free);
	g_clear_pointer (&mi2, impl_match_info_free);

	if (compile_flags & G_REGEX_RAW)
	{
		for (int i = 0; i <= subject_len; i++)
		{
			r1 = g_regex_match_full (reg1, subject, subject_len, i, match_flags, &mi1, &err1);
			r2 = impl_regex_match_full (reg2, subject, subject_len, i, match_flags, &mi2, &err2);
			g_assert_cmpint (r1, ==, r2);
			g_assert_true (err1 == NULL || err2 != NULL);
			assert_iterations (mi1, mi2);
			g_clear_pointer (&mi1, g_match_info_free);
			g_clear_pointer (&mi2, impl_match_info_free);
		}
	}
	else
	{
		for (const char *iter = subject; *iter; iter = g_utf8_next_char (iter))
		{
			gsize i = iter - subject;
			r1 = g_regex_match_full (reg1, subject, subject_len, i, match_flags, &mi1, &err1);
			r2 = impl_regex_match_full (reg2, subject, subject_len, i, match_flags, &mi2, &err2);
			g_assert_cmpint (r1, ==, r2);
			g_assert_true (err1 == NULL || err2 != NULL);
			assert_iterations (mi1, mi2);
			g_clear_pointer (&mi1, g_match_info_free);
			g_clear_pointer (&mi2, impl_match_info_free);
		}
	}

	g_clear_pointer (&reg1, g_regex_unref);
	g_clear_pointer (&reg2, impl_regex_unref);
}

static void
test_compare (void)
{
	/* Flags match what we see from search context */
	GRegexCompileFlags compile = 0x2003;
	GRegexMatchFlags match = 0x400;

	compare_impl_regex_to_g_regex ("aaa\n", "aa", compile, match);
	compare_impl_regex_to_g_regex ("aaa\n", "aa", compile, match);

	compare_impl_regex_to_g_regex ("aaaa", "aa", compile, match);
	compare_impl_regex_to_g_regex ("aaaa", "aa", compile, match);

	compare_impl_regex_to_g_regex ("aaaa\n", "aa", compile, match);
	compare_impl_regex_to_g_regex ("aaaa\n", "aa", compile, match);

	compare_impl_regex_to_g_regex ("", "aa", compile, match);
	compare_impl_regex_to_g_regex ("", "aa", compile, match);

	compare_impl_regex_to_g_regex ("hello\n", "\\w+", compile, match);
	compare_impl_regex_to_g_regex ("hello\n", "\\w+", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*)*", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "\\w+", compile, match);

	compare_impl_regex_to_g_regex ("aa#bb", "(\\w+)#(\\w+)", compile, match);
	compare_impl_regex_to_g_regex ("aa#bb", "(\\w+)#(\\w+)", compile, match);

	compare_impl_regex_to_g_regex ("aa#bb cc#dd", "(\\w+)#(\\w+)", compile, match);
	compare_impl_regex_to_g_regex ("aa#bb cc#dd", "(\\w+)#(\\w+)", compile, match);
	compare_impl_regex_to_g_regex ("aa#bb cc#dd", "(\\w+)#(\\w+)", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\\n)*", compile, match);

	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\n)*", compile, match);
	compare_impl_regex_to_g_regex ("hello\nworld\n", "(.*\\n)*", compile, match);

	compare_impl_regex_to_g_regex ("&aa", "\\baa\\b", compile, match);
	compare_impl_regex_to_g_regex ("\342\200\223aa", "\\baa\\b", compile, match);
	/* this can be a invalid UTF-8 string if substring-ed, make glib think it's a raw string */
	compare_impl_regex_to_g_regex ("\342\200\223aa", "\\baa\\b", compile | G_REGEX_RAW, match);

	compare_impl_regex_to_g_regex ("12\n", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("\n23\n", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("\n123\n", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("\n23\n", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("\n12", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("3", "(?<=1)23", compile, match);
	compare_impl_regex_to_g_regex ("\n123 123\n", "(?<=1)23", compile, match);

	compare_impl_regex_to_g_regex ("12\n", "12(?=3)", compile, match);
	compare_impl_regex_to_g_regex ("123\n", "12(?=3)", compile, match);
	compare_impl_regex_to_g_regex ("\n123", "12(?=3)", compile, match);
	compare_impl_regex_to_g_regex ("\n123 123\n", "12(?=3)", compile, match);
}

static void
test_issue_198 (void)
{
	GError *error = NULL;
	ImplRegex *re = impl_regex_new ("(a)*", 0, 0, &error);
	ImplMatchInfo *mi = NULL;
	char *aaa = g_malloc (8192);
	gboolean r;

	g_assert_no_error (error);
	g_assert_nonnull (re);

	memset (aaa, 'a', 8191);
	aaa[8191] = 0;
	r = impl_regex_match_full (re, aaa, 8191, 0, 0, &mi, &error);
	g_assert_no_error (error);
	g_assert_nonnull (mi);
	g_assert_true (r);

	g_free (aaa);
	g_clear_pointer (&mi, impl_match_info_free);
	g_clear_pointer (&re, impl_regex_unref);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Regex/slash-c", test_slash_c_pattern);
	g_test_add_func ("/Regex/compare-g-regex", test_compare);
	g_test_add_func ("/Regex/issue_198", test_issue_198);

	return g_test_run();
}

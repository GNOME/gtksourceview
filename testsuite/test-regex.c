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

static void
test_sized_subject (void)
{
	const char subject[] = { 'a', '\0', 'b' };
	ImplRegex *regex;
	ImplMatchInfo *match = NULL;
	GError *error = NULL;
	int start, end;

	regex = impl_regex_new ("a\\x00b", 0, 0, &error);
	g_assert_no_error (error);
	g_assert_true (impl_regex_match_full (regex, subject, sizeof subject, 0, 0, &match, &error));
	g_assert_no_error (error);
	g_assert_true (impl_match_info_fetch_pos (match, 0, &start, &end));
	g_assert_cmpint (start, ==, 0);
	g_assert_cmpint (end, ==, sizeof subject);
	impl_match_info_free (match);
	impl_regex_unref (regex);

	for (guint optimize = 0; optimize < 2; optimize++)
	{
		char *exact = g_malloc (3);

		memcpy (exact, "abc", 3);
		regex = impl_regex_new ("$", optimize ? G_REGEX_OPTIMIZE : 0, 0, &error);
		g_assert_no_error (error);
		g_assert_true (impl_regex_match_full (regex, exact, 3, 0, 0, &match, &error));
		g_assert_no_error (error);
		g_assert_true (impl_match_info_fetch_pos (match, 0, &start, &end));
		g_assert_cmpint (start, ==, 3);
		g_assert_cmpint (end, ==, 3);
		g_assert_false (impl_match_info_next (match, &error));
		g_assert_no_error (error);
		impl_match_info_free (match);
		impl_regex_unref (regex);
		g_free (exact);
	}
}

static void
assert_match_info_equal (ImplMatchInfo *actual,
                         ImplMatchInfo *expected)
{
	g_assert_cmpint (impl_match_info_matches (actual), ==, impl_match_info_matches (expected));
	g_assert_cmpint (impl_match_info_is_partial_match (actual), ==, impl_match_info_is_partial_match (expected));
	g_assert_cmpint (impl_match_info_get_match_count (actual), ==, impl_match_info_get_match_count (expected));

	if (impl_match_info_matches (actual))
	{
		for (guint i = 0; i < 6; i++)
		{
			int actual_start = -123, actual_end = -123;
			int expected_start = -123, expected_end = -123;
			gboolean actual_found, expected_found;

			actual_found = impl_match_info_fetch_pos (actual, i, &actual_start, &actual_end);
			expected_found = impl_match_info_fetch_pos (expected, i, &expected_start, &expected_end);
			g_assert_cmpint (actual_found, ==, expected_found);
			g_assert_cmpint (actual_start, ==, expected_start);
			g_assert_cmpint (actual_end, ==, expected_end);
		}
	}
}

static void
test_reuse (void)
{
	static const struct {
		const char *subject;
		gssize length;
		gsize start;
		GRegexMatchFlags flags;
	} cases[] = {
		{ "abc123", -1, 0, 0 },
		{ "abc", -1, 0, 0 },
		{ "!", -1, 0, 0 },
		{ "", -1, 0, 0 },
		{ "é🙂abc123", -1, 6, 0 },
		{ "a\0b", 3, 0, 0 },
		{ "abc123", 3, 0, 0 },
		{ "abc123", 6, 6, 0 },
		{ "abc123", 6, 7, 0 },
		{ "abc123", 6, 0, G_REGEX_MATCH_NOTBOL },
	};
	static const char *patterns[] = {
		"(?<word>[a-z]+)(?<digits>[0-9]+)?",
		"^abc",
		"(?=a)|$",
		"",
		"(?<=é🙂)(abc)(123)?",
	};
	ImplMatchInfo *cached = NULL;
	GError *error = NULL;

	/* Reuse one result across different patterns as well as subjects. */
	for (guint optimize = 0; optimize < 2; optimize++)
	{
		for (guint p = 0; p < G_N_ELEMENTS (patterns); p++)
		{
			ImplRegex *regex = impl_regex_new (patterns[p], optimize ? G_REGEX_OPTIMIZE : 0, 0, &error);
			ImplMatchInfo *saved = NULL;
			int saved_start = -1, saved_end = -1;

			g_assert_no_error (error);
			impl_regex_match_full (regex, "abc123", -1, 0, 0, &saved, &error);
			g_assert_no_error (error);
			if (impl_match_info_matches (saved))
				impl_match_info_fetch_pos (saved, 0, &saved_start, &saved_end);

			for (guint iteration = 0; iteration < 3; iteration++)
			{
				for (guint c = 0; c < G_N_ELEMENTS (cases); c++)
				{
					ImplMatchInfo *fresh = NULL;
					gboolean expected, actual;

					expected = impl_regex_match_full (regex, cases[c].subject, cases[c].length,
					                                  cases[c].start, cases[c].flags, &fresh, &error);
					g_assert_no_error (error);
					actual = impl_regex_match_full_reuse (regex, cases[c].subject, cases[c].length,
					                                      cases[c].start, cases[c].flags, &cached, &error);
					g_assert_no_error (error);
					g_assert_cmpint (actual, ==, expected);
					assert_match_info_equal (cached, fresh);

					while (expected)
					{
						expected = impl_match_info_next (fresh, &error);
						g_assert_no_error (error);
						actual = impl_match_info_next (cached, &error);
						g_assert_no_error (error);
						g_assert_cmpint (actual, ==, expected);
						assert_match_info_equal (cached, fresh);
					}

					impl_match_info_free (fresh);
				}
			}

			if (impl_match_info_matches (saved))
			{
				int start, end;

				impl_match_info_fetch_pos (saved, 0, &start, &end);
				g_assert_cmpint (start, ==, saved_start);
				g_assert_cmpint (end, ==, saved_end);
			}

			impl_match_info_free (saved);
			impl_regex_unref (regex);
		}
	}

	impl_match_info_free (cached);
}

static void
test_reuse_partial (void)
{
	ImplRegex *regex;
	ImplMatchInfo *cached = NULL;
	GError *error = NULL;

	regex = impl_regex_new ("(?<first>abc)(?<last>def)", 0, 0, &error);
	g_assert_no_error (error);

	for (guint i = 0; i < 3; i++)
	{
		int start, end;
		char *capture;

		g_assert_true (impl_regex_match_full_reuse (regex, "abcdef", -1, 0, 0, &cached, &error));
		g_assert_no_error (error);
		g_assert_true (impl_match_info_fetch_named_pos (cached, "last", &start, &end));
		g_assert_cmpint (start, ==, 3);
		g_assert_cmpint (end, ==, 6);
		capture = impl_match_info_fetch_named (cached, "last");
		g_assert_cmpstr (capture, ==, "def");
		g_free (capture);

		g_assert_false (impl_regex_match_full_reuse (regex, "abc", -1, 0,
		                                             G_REGEX_MATCH_PARTIAL_SOFT, &cached, &error));
		g_assert_no_error (error);
		g_assert_true (impl_match_info_is_partial_match (cached));
		g_assert_false (impl_regex_match_full_reuse (regex, "no", -1, 0, 0, &cached, &error));
		g_assert_no_error (error);
		g_assert_false (impl_match_info_is_partial_match (cached));
	}

	impl_regex_unref (regex);
	impl_match_info_free (cached);
}

static void
test_source_reuse_captures (void)
{
	GtkSourceRegex *start;
	GtkSourceRegex *end;
	GtkSourceRegex *resolved;
	GError *error = NULL;
	int begin, finish;

	start = _gtk_source_regex_new ("(?<delimiter>[A-Z]+):", G_REGEX_ANCHORED, &error);
	g_assert_no_error (error);
	end = _gtk_source_regex_new ("\\%{delimiter@start}$", G_REGEX_ANCHORED, &error);
	g_assert_no_error (error);
	g_assert_false (_gtk_source_regex_is_resolved (end));
	g_assert_true (_gtk_source_regex_match (start, "END:", 4, 0));
	resolved = _gtk_source_regex_resolve (end, start, "END:");
	g_assert_true (_gtk_source_regex_is_resolved (resolved));
	g_assert_true (_gtk_source_regex_match (resolved, "END", 3, 0));

	/* Matching another regex must not overwrite the opener's captures. */
	_gtk_source_regex_fetch_pos_bytes (start, 1, &begin, &finish);
	g_assert_cmpint (begin, ==, 0);
	g_assert_cmpint (finish, ==, 3);
	g_assert_false (_gtk_source_regex_match (start, "!", 1, 0));
	g_assert_true (_gtk_source_regex_match (start, "FINISH:", 7, 0));
	_gtk_source_regex_unref (resolved);
	resolved = _gtk_source_regex_resolve (end, start, "FINISH:");
	g_assert_true (_gtk_source_regex_match (resolved, "FINISH", 6, 0));
	g_assert_false (_gtk_source_regex_match (resolved, "END", 3, 0));

	_gtk_source_regex_unref (resolved);
	_gtk_source_regex_unref (start);
	_gtk_source_regex_unref (end);
}

static void
test_source_named_bytes (void)
{
	const char *subject = "é🙂abc";
	GtkSourceRegex *regex;
	GError *error = NULL;
	int begin, end;

	regex = _gtk_source_regex_new ("(?<emoji>🙂)(?<word>[a-z]+)(?<digits>[0-9]+)?", 0, &error);
	g_assert_no_error (error);
	g_assert_true (_gtk_source_regex_match (regex, subject, strlen (subject), 0));
	_gtk_source_regex_fetch_named_pos_bytes (regex, "emoji", &begin, &end);
	g_assert_cmpint (begin, ==, 2);
	g_assert_cmpint (end, ==, 6);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "word", &begin, &end);
	g_assert_cmpint (begin, ==, 6);
	g_assert_cmpint (end, ==, 9);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "digits", &begin, &end);
	g_assert_cmpint (begin, ==, -1);
	g_assert_cmpint (end, ==, -1);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "unknown", &begin, &end);
	g_assert_cmpint (begin, ==, -1);
	g_assert_cmpint (end, ==, -1);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "word", NULL, &end);
	g_assert_cmpint (end, ==, 9);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "word", &begin, NULL);
	g_assert_cmpint (begin, ==, 6);
	_gtk_source_regex_fetch_named_pos_bytes (regex, "word", NULL, NULL);
	g_assert_false (_gtk_source_regex_match (regex, "!", 1, 0));
	_gtk_source_regex_fetch_named_pos_bytes (regex, "word", &begin, &end);
	g_assert_cmpint (begin, ==, -1);
	g_assert_cmpint (end, ==, -1);
	_gtk_source_regex_unref (regex);
}

static void
test_first_byte (void)
{
	static const struct {
		const char *pattern;
		int byte;
	} cases[] = {
		{ "\\{", '{' },
		{ "\\\\$", '\\' },
		{ "(?:\\{a|\\{b)", '{' },
		{ "^\\{", '{' },
		{ "(?<=a)\\{", '{' },
		{ "[{}]", -1 },
		{ "[[:punct:]]", -1 },
		{ "(?=\\{)", -1 },
		{ "(?:\\{)?", -1 },
		{ "", -1 },
		{ "a", -1 },
		{ "(?i)K", -1 },
		{ "é", -1 },
		{ "\\{\\Kfoo", -1 },
		{ "\\{(*SKIP)(*FAIL)|abc", -1 },
	};
	const char *subject = "a{ab}\\\n3Kéfoo";
	GError *error = NULL;

	for (guint caseless = 0; caseless < 2; caseless++)
	{
		for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
		{
			GtkSourceRegex *regex;
			int first;

			regex = _gtk_source_regex_new (cases[i].pattern,
			                               G_REGEX_ANCHORED | (caseless ? G_REGEX_CASELESS : 0), &error);
			g_assert_no_error (error);
			first = _gtk_source_regex_get_first_byte (regex);
			g_assert_cmpint (first, ==, cases[i].byte);

			if (first >= 0)
			{
				for (const char *p = subject; *p; p = g_utf8_next_char (p))
				{
					if (_gtk_source_regex_match (regex, subject, strlen (subject), p - subject))
					{
						int begin, end;

						_gtk_source_regex_fetch_pos_bytes (regex, 0, &begin, &end);
						g_assert_cmpint (begin, ==, p - subject);
						g_assert_cmpint (end, >, begin);
						g_assert_cmpint ((guchar)subject[begin], ==, first);
					}
				}
			}

			_gtk_source_regex_unref (regex);
		}
	}
}

static void
test_source_independent_matcher (void)
{
	GtkSourceRegex *original;
	GtkSourceRegex *matcher;
	GError *error = NULL;
	char *capture;
	int begin, end;

	original = _gtk_source_regex_new ("(?<name>[a-z]+)?$", 0, &error);
	g_assert_no_error (error);
	matcher = _gtk_source_regex_new_matcher (original);
	g_assert_true (_gtk_source_regex_match (original, "first", 5, 0));
	g_assert_true (_gtk_source_regex_match (matcher, "second", 6, 0));
	capture = _gtk_source_regex_fetch (original, 1);
	g_assert_cmpstr (capture, ==, "first");
	g_free (capture);
	_gtk_source_regex_fetch_named_pos_bytes (matcher, "name", &begin, &end);
	g_assert_cmpint (begin, ==, 0);
	g_assert_cmpint (end, ==, 6);

	g_assert_true (_gtk_source_regex_match (matcher, "", 0, 0));
	capture = _gtk_source_regex_fetch (original, 1);
	g_assert_cmpstr (capture, ==, "first");
	g_free (capture);
	_gtk_source_regex_unref (original);
	g_assert_true (_gtk_source_regex_match (matcher, "", 0, 0));
	g_assert_true (_gtk_source_regex_match (matcher, "third", 5, 0));
	capture = _gtk_source_regex_fetch (matcher, 1);
	g_assert_cmpstr (capture, ==, "third");
	g_free (capture);
	_gtk_source_regex_unref (matcher);
}

static void
test_duplicate_named_captures (void)
{
	static const struct {
		const char *pattern;
		const char *subject;
	} cases[] = {
		{ "(?:(?<tag>alpha)|(?<tag>beta)|x)(?<optional>z)?", "alpha" },
		{ "(?:(?<tag>alpha)|(?<tag>beta)|x)(?<optional>z)?", "beta" },
		{ "(?:(?<tag>alpha)|(?<tag>beta)|x)(?<optional>z)?", "x" },
		{ "(?:(?<tag>alpha)|(?<tag>beta)|x)(?<optional>z)?", "alphaz" },
		{ "(?<tag>)(?<tag>abc)", "abc" },
		{ "(?<tag>abc)?(?<tag>def)", "def" },
		{ "(?<tag>a)(?<tag>b)", "ab" },
		{ "(?<tag>é)(?<tag>🙂)", "é🙂" },
		{ "(?<tag>ordinary)(?<optional>z)?", "ordinary" },
	};
	const char *names[] = { "tag", "optional", "unknown" };
	GError *error = NULL;

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
	{
		GRegex *reference = g_regex_new (cases[i].pattern, G_REGEX_DUPNAMES | G_REGEX_ANCHORED, 0, &error);
		GMatchInfo *expected = NULL;
		ImplRegex *regex;
		ImplMatchInfo *actual = NULL;

		g_assert_no_error (error);
		regex = impl_regex_new (cases[i].pattern, G_REGEX_DUPNAMES | G_REGEX_ANCHORED | G_REGEX_OPTIMIZE, 0, &error);
		g_assert_no_error (error);
		g_assert_true (g_regex_match (reference, cases[i].subject, 0, &expected));

		for (guint repeat = 0; repeat < 3; repeat++)
		{
			g_assert_true (impl_regex_match_full_reuse (regex, cases[i].subject, -1, 0, 0, &actual, &error));
			g_assert_no_error (error);

			for (guint n = 0; n < G_N_ELEMENTS (names); n++)
			{
				int begin = -123, end = -123, expected_begin = -123, expected_end = -123;
				gboolean found, expected_found;
				char *value, *expected_value;

				found = impl_match_info_fetch_named_pos (actual, names[n], &begin, &end);
				expected_found = g_match_info_fetch_named_pos (expected, names[n], &expected_begin, &expected_end);
				g_assert_cmpint (found, ==, expected_found);
				if (found)
				{
					g_assert_cmpint (begin, ==, expected_begin);
					g_assert_cmpint (end, ==, expected_end);
				}
				value = impl_match_info_fetch_named (actual, names[n]);
				expected_value = g_match_info_fetch_named (expected, names[n]);
				g_assert_cmpstr (value, ==, expected_value);
				g_free (value);
				g_free (expected_value);
			}
		}

		impl_match_info_free (actual);
		impl_regex_unref (regex);
		g_match_info_free (expected);
		g_regex_unref (reference);
	}
}

static void
test_duplicate_named_delimiter (void)
{
	const char *subjects[] = { "<<'EOF.a'", "<<\"END.b\"" };
	const char *delimiters[] = { "EOF.a", "END.b" };
	GtkSourceRegex *start;
	GtkSourceRegex *end;
	GError *error = NULL;

	start = _gtk_source_regex_new ("<<(?:'(?<HDB>[^']+)'|\"(?<HDB>[^\"]+)\")",
	                               G_REGEX_DUPNAMES | G_REGEX_ANCHORED, &error);
	g_assert_no_error (error);
	end = _gtk_source_regex_new ("\\%{HDB@start}$", G_REGEX_ANCHORED, &error);
	g_assert_no_error (error);

	for (guint i = 0; i < G_N_ELEMENTS (subjects); i++)
	{
		GtkSourceRegex *resolved;

		g_assert_true (_gtk_source_regex_match (start, subjects[i], strlen (subjects[i]), 0));
		resolved = _gtk_source_regex_resolve (end, start, subjects[i]);
		g_assert_true (_gtk_source_regex_match (resolved, delimiters[i], strlen (delimiters[i]), 0));
		g_assert_false (_gtk_source_regex_match (resolved, "EOFxa", 5, 0));
		_gtk_source_regex_unref (resolved);
	}

	_gtk_source_regex_unref (start);
	_gtk_source_regex_unref (end);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Regex/slash-c", test_slash_c_pattern);
	g_test_add_func ("/Regex/compare-g-regex", test_compare);
	g_test_add_func ("/Regex/issue_198", test_issue_198);
	g_test_add_func ("/Regex/sized-subject", test_sized_subject);
	g_test_add_func ("/Regex/reuse", test_reuse);
	g_test_add_func ("/Regex/reuse-partial", test_reuse_partial);
	g_test_add_func ("/Regex/source-reuse-captures", test_source_reuse_captures);
	g_test_add_func ("/Regex/source-named-bytes", test_source_named_bytes);
	g_test_add_func ("/Regex/first-byte", test_first_byte);
	g_test_add_func ("/Regex/source-independent-matcher", test_source_independent_matcher);
	g_test_add_func ("/Regex/duplicate-named-captures", test_duplicate_named_captures);
	g_test_add_func ("/Regex/duplicate-named-delimiter", test_duplicate_named_delimiter);

	return g_test_run();
}

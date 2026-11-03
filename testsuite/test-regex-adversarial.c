/*
 * This file is part of GtkSourceView
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include <string.h>

#include "gtksourceview/implregex-private.h"

static const char *patterns[] = {
"!", "(!)", "(?i:!)", "(?=!)!", "(?<=x)!", "(?<!x)!", "(?:!abc|!def)",
"[!]", "[!][^!]*", "!{1,3}", "!+", "(?:!)*", "(?=!)", "(?=!).", "(?=!)\\D",
"!|", "!|x", "(?>!x|!y)", "(?<one>!)?(?(one)x|!)", "(?i:k)", "(?i:s)",
"(?i:K)", "(?i:ſ)", "(?i:µ)", "(?i:!)", "\\G!", "\\A!", "^!", "\\b!",
"!\\Kx", "x\\K!", "(?:!|x\\K!)", "!\\K", "(*NO_START_OPT)!", "(*NO_AUTO_POSSESS)!",
"(?!x)!", "(?!.)!", "(?:!)(?=x)", "\\x{21}", "\\Q!\\E",
"(?:!xx|!x)*", "(?<recursive>!(?&recursive)?)", "(?(DEFINE)(?<p>!))(?&p)", "(?'p'!)(?1)?",
"(*MARK:first)!", "(*FAIL)|!", "(?:(*SKIP)(*F)|!)", "(?:(?=x)(*SKIP)(*F)|!)",
"(?|(!)|(!!))", "(?J)(?<p>!)|(?<p>!!)", "\\x{00}", "(?<=🙂)!", "(?<!🙂)!",
"(.)?(!)?", "(?<a>[a-z]*)(?<b>!)?", "(?=.)|$", "", "x*", "(?:!|x)*", "!$"
};
static const char *subjects[] = { "", "!", "!!", "!x", "x!", "xx!", "!xx!", "!def", "!abc", "\n!", "x\n!", "k", "K", "K", "s", "ſ", "µ", "μ", "🙂!", "!🙂", "a\0!x" };

/* Reuse and capture lifetime are covered in test-regex.c. Here the
 * question is whether rejecting a subject by its first byte can lose a match.
 */
static void
test_first_byte_filters (void)
{
	guint filters = 0;

	for (guint flags = 0; flags < 4; flags++)
	{
		for (guint p = 0; p < G_N_ELEMENTS (patterns); p++)
		{
			GError *error = NULL;
			ImplRegex *regex;
			int first;

			regex = impl_regex_new (patterns[p],
			                        G_REGEX_ANCHORED |
			                        ((flags & 1) ? G_REGEX_OPTIMIZE : 0) |
			                        ((flags & 2) ? G_REGEX_CASELESS : 0),
			                        0, &error);
			g_assert_no_error (error);
			first = impl_regex_get_first_byte (regex);

			if (first >= 0)
			{
				filters++;
				for (guint s = 0; s < G_N_ELEMENTS (subjects); s++)
				{
					gsize length = s == G_N_ELEMENTS (subjects) - 1 ? 4 : strlen (subjects[s]);

					for (gsize position = 0; position <= length; position++)
					{
						ImplMatchInfo *match = NULL;

						if (position < length && (((guchar)subjects[s][position] & 0xc0) == 0x80))
							continue;

						if (impl_regex_match_full (regex, subjects[s], length, position, 0, &match, &error))
						{
							g_assert_cmpuint (position, <, length);
							g_assert_cmpint ((guchar)subjects[s][position], ==, first);
						}

						g_assert_no_error (error);
						impl_match_info_free (match);
					}
				}
			}
			impl_regex_unref (regex);
		}
	}

	g_assert_cmpuint (filters, >, 0);
}

int
main (int    argc,
      char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/Regex/adversarial-first-byte", test_first_byte_filters);
	return g_test_run ();
}

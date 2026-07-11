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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtksourceview/gtksource.h>
#include <gtksourceview/vim/gtksourcevim.h>
#include <gtksourceview/vim/gtksourcevimcommand.h>
#include <gtksourceview/vim/gtksourceviminsert.h>
#include <gtksourceview/vim/gtksourcevimnormal.h>
#include <gtksourceview/vim/gtksourcevimregisters.h>
#include <gtksourceview/vim/gtksourcevimstate.h>

#include "gtksourceview/gtksourcelanguagemanager-private.h"

static void
run_test (const char *text,
          const char *input,
          const char *expected)
{
	GtkSourceView *view = GTK_SOURCE_VIEW (g_object_ref_sink (gtk_source_view_new ()));
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	GtkSourceStyleSchemeManager *schemes = gtk_source_style_scheme_manager_get_default ();
	GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (schemes, "Adwaita");
	GtkSourceVim *vim = gtk_source_vim_new (view);
	GtkSourceVimState *registers = gtk_source_vim_state_get_registers (GTK_SOURCE_VIM_STATE (vim));
	GtkTextIter begin, end;
	GtkTextIter insert;
	GString *expected_text = NULL;
	int cursor_offset = -1;
	char *ret;

	/* Registers are shared per-process, so they need to be reset between runs */
	gtk_source_vim_registers_reset (GTK_SOURCE_VIM_REGISTERS (registers));

	gtk_source_buffer_set_style_scheme (buffer, scheme);

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &begin, &begin);

	for (const char *c = input; *c; c = g_utf8_next_char (c))
	{
		GtkSourceVimState *current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (vim));
		gunichar ch = g_utf8_get_char (c);
		char string[16] = {0};
		GdkModifierType mods = 0;
		guint keyval;

		/* It would be nice to send GdkEvent, but we have to rely on
		 * the fact that our engine knows key-presses pretty much
		 * everywhere so that we can send keypresses based on chars.
		 */
		string[g_unichar_to_utf8 (ch, string)] = 0;

		if (ch == '\033')
		{
			string[0] = '^';
			string[1] = '[';
			string[2] = 0;
			keyval = GDK_KEY_Escape;
		}
		else if (ch == '\n')
		{
			string[0] = '\n';
			string[1] = 0;
			keyval = GDK_KEY_Return;
		}
		else if (ch == '\001')
		{
			string[0] = '^';
			string[1] = 'A';
			string[2] = 0;
			keyval = GDK_KEY_a;
			mods = GDK_CONTROL_MASK;
		}
		else if (ch == '\030')
		{
			string[0] = '^';
			string[1] = 'X';
			string[2] = 0;
			keyval = GDK_KEY_x;
			mods = GDK_CONTROL_MASK;
		}
		else
		{
			keyval = gdk_unicode_to_keyval (ch);
		}

		if (!GTK_SOURCE_VIM_STATE_GET_CLASS (current)->handle_keypress (current, keyval, 0, mods, string))
		{
			gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer), string, -1);
		}
	}

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	ret = gtk_text_iter_get_slice (&begin, &end);
	expected_text = g_string_new (NULL);
	for (const char *c = expected; *c; c = g_utf8_next_char (c))
	{
		gunichar ch = g_utf8_get_char (c);

		if (ch == '|')
		{
			g_assert_cmpint (cursor_offset, ==, -1);
			cursor_offset = g_utf8_strlen (expected_text->str, expected_text->len);
		}
		else
		{
			g_string_append_unichar (expected_text, ch);
		}
	}

	if (g_strcmp0 (ret, expected_text->str) != 0)
	{
		g_error ("input '%s' produced text '%s', expected '%s'",
		         input, ret, expected_text->str);
	}
	g_free (ret);

	if (cursor_offset >= 0)
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
		                                  &insert,
		                                  gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer)));
		if (gtk_text_iter_get_offset (&insert) != cursor_offset)
		{
			g_error ("input '%s' placed cursor at %d, expected %d",
			         input, gtk_text_iter_get_offset (&insert), cursor_offset);
		}
	}

	g_string_free (expected_text, TRUE);

	g_assert_finalize_object (G_OBJECT (vim));
	g_assert_finalize_object (G_OBJECT (view));
}

static void
test_yank (void)
{
	run_test ("1\n2\n3", "yGP", "1\n2\n3\n1\n2\n3");
	run_test ("1\n2\n3", "yGp", "1\n1\n2\n3\n2\n3");
	run_test ("1\n2\n3", "\"zyGP", "1\n2\n3");
	run_test ("1\n2\n3", "\"zyG\"zP", "1\n2\n3\n1\n2\n3");
}

static void
test_insert (void)
{
	run_test ("line1", "o\033", "line1\n");
	run_test ("line1", "O\033", "\nline1");
	run_test ("", "itesting\033" "a this.\033", "testing this.");
	run_test ("", "3iz\033", "zzz");
	run_test ("\tPROP_0,\n", "3IPROP\033", "\tPROPPROPPROPPROP_0,\n");
}

static void
test_change (void)
{
	run_test ("word here", "ciwnot\033", "not here");
	run_test ("word here", "wc$\033", "word ");
	run_test ("alpha beta gamma", "wceBETA\033", "alpha BETA gamma");
	run_test ("alpha beta gamma", "wcwBETA\033", "alpha BETA gamma");
	run_test ("one two three", "Ctail\033", "tail");
	run_test ("one\ntwo\nthree\n", "ccreplacement\033", "replacement\ntwo\nthree\n");
	run_test ("one\ntwo\nthree\n", "Sreplacement\033", "replacement\ntwo\nthree\n");
	run_test ("foo(bar) baz", "f(ci(paren\033", "foo(paren) baz");
	run_test ("foo <tag> baz", "f<ci<angle\033", "foo <angle> baz");
}

static void
test_delete (void)
{
	run_test ("a word here.", "v$x", "");
	run_test ("t\nt\n", "Vx", "t\n");
	run_test ("a word here.", "vex", " here.");
	run_test ("line1", "dd", "");
	run_test ("line1\n", "dj", "");
	run_test ("line1\n\n", "dj", "");
	run_test ("1\n2\n", "d2j", "");
	run_test ("1\n2\n", "d10j", "");
	run_test ("1\n2\n3\n42", "vjjjx", "2");
	run_test ("1\n2\n3\n42", "vjjjVx", "");
	run_test ("1\n2\n3\n4", "dG", "");
	run_test ("1\n2\n3\n42", "jmzjjd'z", "1");
	run_test ("1\n2\n3\n4\n5", "4Gd1G", "5");
	run_test ("1\n2\n3\n4\n5", ":4\nd1G", "5");
	run_test ("alpha beta gamma", "dw", "beta gamma");
	run_test ("alpha beta gamma", "de", " beta gamma");
	run_test ("alpha beta gamma", "d2w", "gamma");
	run_test ("alpha beta gamma", "d$", "");
	run_test ("alpha beta gamma", "lld0", "pha beta gamma");
	run_test ("foo(bar) baz", "f(di(", "foo() baz");
	run_test ("foo(bar) baz", "f(da(", "foo baz");
	run_test ("foo <tag> baz", "f<di<", "foo <> baz");
	run_test ("foo <tag> baz", "f<da<", "foo  baz");
	run_test ("one\ntwo\nthree\n", "jdd", "one\nthree\n");
	run_test ("one\ntwo\nthree\n", "jdk", "three\n");
	run_test ("one\ntwo\nthree\n", "jD", "one\n\nthree\n");
	run_test ("one\ntwo\nthree\n", "j0x", "one\nwo\nthree\n");

#if 0
	/* somehow VIM ignores \n before 4. */
	run_test ("1\n22\n3\n4", "jlmzjjd`z", "1\n2\n4");
#endif
}

static void
test_motion (void)
{
	run_test ("abc", "", "|abc");
	run_test ("abc", "l", "a|bc");
	run_test ("abc", "2l", "ab|c");
	run_test ("abc", "10l", "ab|c");
	run_test ("abc", "lh", "|abc");
	run_test ("  abc", "$", "  ab|c");
	run_test ("  abc", "$0", "|  abc");
	run_test ("  abc", "$^", "  |abc");
	run_test ("one two-three", "w", "one |two-three");
	run_test ("one two-three", "e", "on|e two-three");
	run_test ("one two-three", "E", "on|e two-three");
	run_test ("one two-three", "wwb", "one |two-three");
	run_test ("one two-three", "wge", "on|e two-three");
	run_test ("one\ntwo\nthree", "G", "one\ntwo\n|three");
	run_test ("one\ntwo\n  three\n", "Ggg", "|one\ntwo\n  three\n");
	run_test ("one\ntwo\n  three\n", "3G", "one\ntwo\n  |three\n");
	run_test ("one\ntwo\n  three\n", "+", "one\n|two\n  three\n");
	run_test ("one\ntwo\n  three\n", "2+", "one\ntwo\n  |three\n");
	run_test ("one\ntwo\nthree\n", "j", "one\n|two\nthree\n");
	run_test ("one\ntwo\nthree\n", "2j", "one\ntwo\n|three\n");
	run_test ("one\ntwo\nthree\n", "2jk", "one\n|two\nthree\n");
	run_test ("abcd\nx\nabcd", "$j", "abcd\n|x\nabcd");
	run_test ("abcd\nx\nabcd", "$jk", "abc|d\nx\nabcd");
	run_test ("abcd\nx\nabcd", "$jj", "abcd\nx\nabc|d");
	run_test ("abcd\nx\nabcd", "$jkh", "ab|cd\nx\nabcd");
	run_test ("abc def ghi", "fdfgFd", "abc |def ghi");
	run_test ("abc(def)ghi", "f(%", "abc(def|)ghi");
	run_test ("abc(def)ghi", "f(%%", "abc|(def)ghi");
	run_test ("a. b! c?", ")", "a. |b! c?");
	run_test ("a. b! c?", "))", "a. b! |c?");
	run_test ("first\n\nsecond\n", "}", "first\n|\nsecond\n");
	run_test ("first\n\nsecond\n", "}j{", "first\n|\nsecond\n");
}

static void
test_search (void)
{
	run_test ("one two one two", "/two\n", "one |two one two");
	run_test ("one two one two", "/two\nn", "one two one |two");
	run_test ("one two one two", "/two\nnN", "one |two one two");
	run_test ("one two one two", "?two\n", "one two one |two");
	run_test ("one two one two", "w*", "one two one |two");
}

static void
test_mark (void)
{
	run_test ("one\ntwo\nthree\n", "jmaG'a", "one\n|two\nthree\n");
	run_test ("one\ntwo\nthree\n", "jmaG`a", "one\n|two\nthree\n");
	run_test ("  one\n  two\n  three\n", "jllmaG'a", "  one\n  |two\n  three\n");
	run_test ("  one\n  two\n  three\n", "jllmaG`a", "  one\n  |two\n  three\n");
}

static void
test_operator (void)
{
	run_test ("abc", "x", "bc");
	run_test ("abc", "2x", "c");
	run_test ("abc", "lX", "bc");
	run_test ("abcd", "2l2X", "cd");
	run_test ("abc", "rx", "xbc");
	run_test ("abc", "2rx", "xxc");
	run_test ("one\ntwo\nthree\n", "J", "one two\nthree\n");
	run_test ("abc", "~", "Abc");
	run_test ("abc", "3~", "ABC");
	run_test ("abc", "vll~", "ABC");
	run_test ("abc def", "gUw", "ABC def");
	run_test ("ABC DEF", "guw", "abc DEF");
	run_test ("abc DEF", "g~w", "ABC DEF");
	run_test ("n = 41;", "\001", "n = 42;");
	run_test ("n = 41;", "5\001", "n = 46;");
	run_test ("n = 41;", "\030", "n = 40;");
	run_test ("n = 1;", "\001.", "n = 3;");
	run_test ("abc", "sX\033", "Xbc");
	run_test ("abc", "2sX\033", "Xc");
	run_test ("abc", "x.", "c");
	run_test ("", "ia\033.", "aa");
	run_test ("abcd", "rxl.", "xxcd");
	run_test ("abcd", "sX\033l.", "XXcd");
	run_test ("one two three", "cwONE\033w.", "ONE ONE three");
	run_test ("foo(bar) baz(qux)", "f(ci(X\033f(.", "foo(X) baz(X)");
}

static void
test_search_and_replace (void)
{
	static const struct {
		const char *command;
		gboolean success;
		const char *search;
		const char *replace;
		const char *options;
	} parse_s_and_r[] = {
		{ "s/", TRUE, NULL, NULL, NULL },
		{ "s/a", TRUE, "a", NULL, NULL },
		{ "s/a/", TRUE, "a", NULL, NULL },
		{ "s/a/b", TRUE, "a", "b", NULL },
		{ "s/a/b/", TRUE, "a", "b", NULL },
		{ "s/a/b/c", TRUE, "a", "b", "c" },
		{ "s#a#b#c", TRUE, "a", "b", "c" },
		{ "s/^ \\//", TRUE, "^ /", NULL, NULL },
		{ "s/\\/\\/", TRUE, "//", NULL, NULL },
		{ "s/^$//gI", TRUE, "^$", "", "gI" },
	};

	for (guint i = 0; i < G_N_ELEMENTS (parse_s_and_r); i++)
	{
		const char *str = parse_s_and_r[i].command;
		char *search = NULL;
		char *replace = NULL;
		char *options = NULL;
		gboolean ret;

		g_assert_true (*str == 's');

		str++;
		ret = gtk_source_vim_command_parse_search_and_replace (str, &search, &replace, &options);

		if (!parse_s_and_r[i].success && ret)
		{
			g_error ("expected %s to fail, but it succeeded",
			         parse_s_and_r[i].command);
		}
		else if (parse_s_and_r[i].success && !ret)
		{
			g_error ("expected %s to pass, but it failed",
			         parse_s_and_r[i].command);
		}

		g_assert_cmpstr (search, ==, parse_s_and_r[i].search);
		g_assert_cmpstr (replace, ==, parse_s_and_r[i].replace);
		g_assert_cmpstr (options, ==, parse_s_and_r[i].options);

		g_free (search);
		g_free (replace);
		g_free (options);
	}

	run_test ("test test test test", ":s/test\n", " test test test");
	run_test ("test test test test", ":s/test/bar\n", "bar test test test");
	run_test ("test test test test", ":s/test/bar/g\n", "bar bar bar bar");
	run_test ("test test test test", ":s/TEST/bar/gi\n", "bar bar bar bar");
	run_test ("test test test test", ":s/TEST/bar\n", "test test test test");
	run_test ("t t t t\nt t t t\n", ":s/t/f\n", "f t t t\nt t t t\n");
	run_test ("t t t t\nt t t t\n", ":%s/t/f\n", "f t t t\nf t t t\n");
	run_test ("t t t t\nt t t t\n", ":%s/t/f/g\n", "f f f f\nf f f f\n");
	run_test ("t t t t\nt t t t\n", ":.,$s/t/f\n", "f t t t\nf t t t\n");
	run_test ("t t\nt t\nt t\n", ":.,+1s/t/f\n", "f t\nf t\nt t\n");
	run_test ("t t t t\nt t t t\n", "V:s/t/f\n", "f t t t\nt t t t\n");
	run_test ("/ / / /", ":s/\\//#/g\n", "# # # #");
}

static void
test_command_bar (void)
{
	run_test ("", ":set sw=0\n", "");
	run_test ("", ":set sw=-2\n", "");
	run_test ("", ":set sw=33\n", "");
	run_test ("", ":set ts=0\n", "");
	run_test ("", ":set ts=-2\n", "");
	run_test ("", ":set ts=33\n", "");
	run_test ("", ":set tw=100\n", "");
	run_test ("", ":set ft=c\n", "");
	run_test ("", ":set hlsearch nohls nohlsearch\n", "");
	run_test ("", ":set number nonumber cursorline nocursorline\n", "");
	run_test ("", ":set syntax=on syntax=off\n", "");
	run_test ("", ":noh\n", "");
	run_test ("", ":nohlsearch\n", "");
}

static void
test_visual (void)
{
	run_test ("0123456789", "3lvllohhx", "06789");
}

int
main (int argc,
      char *argv[])
{
	const char *srcdir = g_getenv ("G_TEST_SRCDIR");
	char *schemes_search_path[2] = { NULL };
	GtkSourceStyleSchemeManager *schemes;
	char *rng;
	int ret;

	g_assert_true (g_file_test (srcdir, G_FILE_TEST_IS_DIR));

	gtk_init ();
	gtk_source_init ();

	rng = g_build_filename (TOP_SRCDIR, "data", "language-specs", "language2.rng", NULL);
	_gtk_source_language_manager_set_rng_file (rng);
	g_free (rng);

	schemes = gtk_source_style_scheme_manager_get_default ();
	schemes_search_path[0] = g_build_filename (srcdir, "..", "data", "styles", NULL);
	gtk_source_style_scheme_manager_set_search_path (schemes, (const char * const *)schemes_search_path);
	g_free (schemes_search_path[0]);

	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/GtkSourceView/vim-input/yank", test_yank);
	g_test_add_func ("/GtkSourceView/vim-input/insert", test_insert);
	g_test_add_func ("/GtkSourceView/vim-input/change", test_change);
	g_test_add_func ("/GtkSourceView/vim-input/delete", test_delete);
	g_test_add_func ("/GtkSourceView/vim-input/motion", test_motion);
	g_test_add_func ("/GtkSourceView/vim-input/search", test_search);
	g_test_add_func ("/GtkSourceView/vim-input/mark", test_mark);
	g_test_add_func ("/GtkSourceView/vim-input/operator", test_operator);
	g_test_add_func ("/GtkSourceView/vim-input/search-and-replace", test_search_and_replace);
	g_test_add_func ("/GtkSourceView/vim-input/command-bar", test_command_bar);
	g_test_add_func ("/GtkSourceView/vim-input/visual", test_visual);
	ret = g_test_run ();
	gtk_source_finalize ();
	return ret;
}

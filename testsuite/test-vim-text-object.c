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
#include <gtksourceview/vim/gtksourcevimtextobject.h>

static void
run_test (GtkSourceVimState *text_object,
          const char        *text,
          guint              position,
          const char        *expect_selection)
{
	GtkSourceBuffer *buffer;
	GtkTextIter begin, end;

	g_assert_true (GTK_SOURCE_IS_VIM_TEXT_OBJECT (text_object));
	g_assert_nonnull (text);

	buffer = gtk_source_buffer_new (NULL);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &begin, position);
	end = begin;

	if (!gtk_source_vim_text_object_select (GTK_SOURCE_VIM_TEXT_OBJECT (text_object), &begin, &end))
	{
		char *text_escape;
		char *exp_escape;

		if (expect_selection == NULL)
			goto cleanup;

		text_escape = g_strescape (text, NULL);
		exp_escape = g_strescape (expect_selection, NULL);

		g_error ("Selection Failed: '%s' at position %u expected '%s'",
			 text_escape, position, exp_escape);

		g_free (text_escape);
		g_free (exp_escape);
	}

	if (expect_selection == NULL)
	{
		char *out = gtk_text_iter_get_slice (&begin, &end);
		char *escaped = g_strescape (out, NULL);
		g_error ("Expected to fail selection but got '%s'", escaped);
		g_free (escaped);
		g_free (out);
	}
	else
	{
		char *selected_text = gtk_text_iter_get_slice (&begin, &end);
		g_assert_cmpstr (selected_text, ==, expect_selection);
		g_free (selected_text);
	}

cleanup:
	g_clear_object (&buffer);

	g_assert_finalize_object (text_object);
}

static void
test_word (void)
{
	run_test (gtk_source_vim_text_object_new_inner_word (), "", 0, "");
	run_test (gtk_source_vim_text_object_new_inner_word (), "this is some- text to modify\n", 8, "some");
	run_test (gtk_source_vim_text_object_new_inner_word (), "something  here\n", 10, "  ");
	run_test (gtk_source_vim_text_object_new_inner_word (), "something  here", 9, "  ");
	run_test (gtk_source_vim_text_object_new_inner_word (), "a", 0, "a");
	run_test (gtk_source_vim_text_object_new_inner_word (), "a b", 1, " ");
	run_test (gtk_source_vim_text_object_new_inner_word (), "+ -", 1, " ");
	run_test (gtk_source_vim_text_object_new_inner_word (), "z a", 2, "a");
	run_test (gtk_source_vim_text_object_new_a_word (), "a b", 1, " b");
	run_test (gtk_source_vim_text_object_new_a_word (), "+ -", 1, " -");
	run_test (gtk_source_vim_text_object_new_a_word (), "a b", 2, "b");
	run_test (gtk_source_vim_text_object_new_a_word (), "a b c", 2, "b ");
	run_test (gtk_source_vim_text_object_new_inner_word (), "\n    \n\n", 2, "    ");
	run_test (gtk_source_vim_text_object_new_a_word (), "\n    \n\n", 2, "    ");
}

static void
test_WORD (void)
{
	run_test (gtk_source_vim_text_object_new_inner_WORD (), "this is some- text to modify\n", 8, "some-");
	run_test (gtk_source_vim_text_object_new_inner_WORD (), "something  here\n", 10, "  ");
	run_test (gtk_source_vim_text_object_new_inner_WORD (), "something  here", 9, "  ");
	run_test (gtk_source_vim_text_object_new_inner_WORD (), "\n    \n\n", 2, "    ");
	run_test (gtk_source_vim_text_object_new_a_WORD (), "\n    \n\n", 2, "    ");
}

static void
test_block (void)
{
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "this_is_a_function (some stuff\n  and some more)\ntrailing", 23, "(some stuff\n  and some more)");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "this_is_a_function (some stuff\n  and some more)\ntrailing", 23, "some stuff\n  and some more");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(should not match\n", 5, NULL);
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(m)", 0, "m");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(m)", 1, "m");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(m)", 2, "m");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(m)", 3, NULL);
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "(m)", 0, "(m)");
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "(m)", 1, "(m)");
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "(m)", 2, "(m)");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "(m)", 3, NULL);
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "()", 2, NULL);
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "()", 1, "");
	run_test (gtk_source_vim_text_object_new_inner_block_paren (), "()", 0, "");
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "() ", 1, "()");
	run_test (gtk_source_vim_text_object_new_a_block_paren (), "() ", 0, "()");
	run_test (gtk_source_vim_text_object_new_a_block_lt_gt (), "<a></a>", 0, "<a>");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a>", 0, "a");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a>", 2, "a");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a></a>", 0, "a");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a></a>", 1, "a");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a></a>", 2, "a");
	run_test (gtk_source_vim_text_object_new_inner_block_lt_gt (), "<a></a>", 3, "/a");

	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 0, "a[b[c]]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 1, "a[b[c]]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 2, "b[c]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 3, "b[c]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 4, "c");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 5, "c");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 6, "c");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 7, "b[c]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 8, "a[b[c]]");
	run_test (gtk_source_vim_text_object_new_inner_block_bracket (), "[a[b[c]]]", 9, NULL);
}

static void
test_quote (void)
{
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"this is a string.\"", 0, "this is a string.");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"this is a string.\"", 0, "\"this is a string.\"");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"this is a string.\n", 0, NULL);
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"this \"is a string.\"", 6, "this ");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"this \"is a string.\"", 6, "\"this \"");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"this \"is a string.\"", 7, "is a string.");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"this \"is a string.", 7, NULL);
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"\"", 0, "");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"\"", 1, "");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), " \"\"", 2, "");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"\" ", 1, "");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"\" \"", 1, "");
	run_test (gtk_source_vim_text_object_new_inner_quote_double (), "\"a\" \"", 1, "a");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"\"", 0, "\"\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"\"", 1, "\"\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), " \"\"", 2, "\"\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"\" ", 1, "\"\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"\" \"", 1, "\"\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"a\"b\"", 2, "\"a\"");
	run_test (gtk_source_vim_text_object_new_a_quote_double (), "\"a\"b\"", 3, "\"b\"");
}

static void
test_sentence (void)
{
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 0, "a.");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 1, "a.");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 2, "b!");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 3, "b!");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 4, "b!");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 5, "c?");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "a. b! c?", 6, "c?");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "\n a. b! c?", 1, "a.");
	run_test (gtk_source_vim_text_object_new_inner_sentence (), "\n a. b! c?", 2, "a.");

	run_test (gtk_source_vim_text_object_new_a_sentence (), "a. b! c?", 0, "a. ");
	run_test (gtk_source_vim_text_object_new_a_sentence (), " a. b! c?", 0, " a. ");
	run_test (gtk_source_vim_text_object_new_a_sentence (), "\n a. b! c?", 1, "a. ");
	run_test (gtk_source_vim_text_object_new_a_sentence (), "\n a. b! c?", 2, "a. ");
}

static void
test_paragraph (void)
{
	GtkSourceVimState *temp;

	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "testing this.\n\n\n", 0, "testing this.");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "testing this.\n", 5, "testing this.");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\n\n", 0, "\n\n");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\n\n", 1, "\n\n");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\n\n\n", 1, "\n\n\n");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "what\nwill\n we\n\nfind\nhere.", 1, "what\nwill\n we");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\tword;\n\n\tanother;\n\n\tthird;\n", 9, "\tanother;");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\tword;\n\n\tanother;\n", 7, "");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\t1\n\n\t2\n\n\t3", 8, "");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\n", 0, "\n");
	run_test (gtk_source_vim_text_object_new_inner_paragraph (), "\n\na\nb\nc\n", 0, "\n");

	run_test (gtk_source_vim_text_object_new_a_paragraph (), "testing this.\n\n\n", 0, "testing this.\n\n\n");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "testing this.\n", 5, "testing this.\n");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\n", 0, NULL);
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\n\n", 0, NULL);
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\n\n", 1, NULL);
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\n\n\n", 1, NULL);
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "what\nwill\n we\n\nfind\nhere.", 1, "what\nwill\n we\n");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\tword;\n\n\tanother;\n\n\tthird;\n", 9, "\tanother;\n");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\tword;\n\n\tanother;\n", 7, "\n\tanother;");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\t1\n\n\t2\n\n\t3\n", 7, "\n\t3");
	run_test (gtk_source_vim_text_object_new_a_paragraph (), "\t1\n\n\t2\n\n\t3\n", 8, "\t3\n");

	temp = gtk_source_vim_text_object_new_inner_paragraph ();
	gtk_source_vim_state_set_count (temp, 2);
	run_test (temp, "t\n\nt", 0, "t\n");
}

int
main (int argc,
      char *argv[])
{
	int ret;

	gtk_init ();
	gtk_source_init ();
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/GtkSourceView/vim-text-object/word", test_word);
	g_test_add_func ("/GtkSourceView/vim-text-object/WORD", test_WORD);
	g_test_add_func ("/GtkSourceView/vim-text-object/block", test_block);
	g_test_add_func ("/GtkSourceView/vim-text-object/quote", test_quote);
	g_test_add_func ("/GtkSourceView/vim-text-object/sentence", test_sentence);
	g_test_add_func ("/GtkSourceView/vim-text-object/paragraph", test_paragraph);
	ret = g_test_run ();
	gtk_source_finalize ();
	return ret;
}

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

#include <gtksourceview/gtksource.h>
#include "gtksourceview/gtksourcetagplan-private.h"

typedef struct
{
	guint apply;
	guint remove;
} MutationCount;

static void
assert_tag_change (GtkTextTag        *tag,
                   const GtkTextIter *start,
                   const GtkTextIter *end,
                   gboolean           adding)
{
	GtkTextIter iter = *start;

	g_assert_cmpint (gtk_text_iter_compare (start, end), <, 0);

	while (gtk_text_iter_compare (&iter, end) < 0)
	{
		g_assert_cmpint (gtk_text_iter_has_tag (&iter, tag), ==, !adding);
		gtk_text_iter_forward_char (&iter);
	}
}

static void
count_apply (GtkTextBuffer *buffer,
             GtkTextTag    *tag,
             GtkTextIter   *start,
             GtkTextIter   *end,
             MutationCount *count)
{
	assert_tag_change (tag, start, end, TRUE);
	count->apply++;
}

static void
count_remove (GtkTextBuffer *buffer,
              GtkTextTag    *tag,
              GtkTextIter   *start,
              GtkTextIter   *end,
              MutationCount *count)
{
	assert_tag_change (tag, start, end, FALSE);
	count->remove++;
}

static GtkTextBuffer *
new_buffer (int length)
{
	GtkTextBuffer *buffer = gtk_text_buffer_new (NULL);
	char *text = g_strnfill (length, 'x');

	gtk_text_buffer_set_text (buffer, text, length);
	g_free (text);

	return buffer;
}

static void
set_tag (GtkTextBuffer *buffer,
         GtkTextTag    *tag,
         int            start,
         int            end,
         gboolean       enabled)
{
	GtkTextIter a;
	GtkTextIter b;

	gtk_text_buffer_get_iter_at_offset (buffer, &a, start);
	b = a;
	gtk_text_iter_forward_chars (&b, end - start);

	if (enabled)
		gtk_text_buffer_apply_tag (buffer, tag, &a, &b);
	else
		gtk_text_buffer_remove_tag (buffer, tag, &a, &b);
}

static gboolean
has_tag (GtkTextBuffer *buffer,
         GtkTextTag    *tag,
         int            offset)
{
	GtkTextIter iter;

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
	return gtk_text_iter_has_tag (&iter, tag);
}

static void
test_coalesce_and_unchanged (void)
{
	GtkTextBuffer *buffer = new_buffer (1000);
	GtkSourceTagPlan *plan = _gtk_source_tag_plan_new (buffer, 0, 1000);
	GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, NULL, NULL);
	MutationCount count = { 0, 0 };

	g_signal_connect (buffer, "apply-tag", G_CALLBACK (count_apply), &count);
	g_signal_connect (buffer, "remove-tag", G_CALLBACK (count_remove), &count);

	for (int i = 0; i < 1000; i++)
		_gtk_source_tag_plan_add (plan, tag, i, i + 1, TRUE);

	_gtk_source_tag_plan_apply (plan);
	g_assert_cmpuint (count.apply, ==, 1);
	g_assert_cmpuint (count.remove, ==, 0);

	for (int i = 0; i < 1000; i++)
		g_assert_true (has_tag (buffer, tag, i));

	count = (MutationCount) { 0, 0 };
	_gtk_source_tag_plan_apply (plan);
	g_assert_cmpuint (count.apply, ==, 0);
	g_assert_cmpuint (count.remove, ==, 0);

	g_clear_pointer (&plan, _gtk_source_tag_plan_free);
	plan = _gtk_source_tag_plan_new (buffer, 0, 1000);
	_gtk_source_tag_plan_add_tag (plan, tag);
	_gtk_source_tag_plan_apply (plan);
	g_assert_cmpuint (count.apply, ==, 0);
	g_assert_cmpuint (count.remove, ==, 1);

	_gtk_source_tag_plan_free (plan);
	g_object_unref (buffer);
}

static void
test_ordered_overrides (void)
{
	GtkTextBuffer *buffer = new_buffer (20);
	GtkSourceTagPlan *plan = _gtk_source_tag_plan_new (buffer, 0, 20);
	GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, "class", NULL);
	GtkTextTag *other = gtk_text_buffer_create_tag (buffer, "other", NULL);

	_gtk_source_tag_plan_add (plan, tag, 0, 20, TRUE);
	_gtk_source_tag_plan_add (plan, tag, 2, 14, FALSE);
	_gtk_source_tag_plan_add (plan, tag, 4, 18, TRUE);
	_gtk_source_tag_plan_add (plan, tag, 6, 10, FALSE);
	_gtk_source_tag_plan_add (plan, tag, 8, 9, TRUE);
	_gtk_source_tag_plan_add (plan, other, 0, 20, TRUE);
	_gtk_source_tag_plan_apply (plan);

	for (int i = 0; i < 20; i++)
	{
		gboolean expected = !(i >= 2 && i < 4) && !(i >= 6 && i < 8) && i != 9;

		g_assert_cmpint (has_tag (buffer, tag, i), ==, expected);
		g_assert_true (has_tag (buffer, other, i));
	}

	/* Reapplying a plan must also leave mixed enable/disable plans intact. */
	_gtk_source_tag_plan_apply (plan);
	g_assert_false (has_tag (buffer, tag, 2));
	g_assert_true (has_tag (buffer, tag, 8));

	_gtk_source_tag_plan_free (plan);
	g_object_unref (buffer);
}

static void
test_partial_range_and_identity (void)
{
	GtkTextBuffer *buffer = gtk_text_buffer_new (NULL);
	GtkSourceTagPlan *plan = NULL;
	GtkTextTag *a = gtk_text_buffer_create_tag (buffer, "a", "foreground", "red", NULL);
	GtkTextTag *b = gtk_text_buffer_create_tag (buffer, "b", "foreground", "red", NULL);
	GtkTextTag *user = gtk_text_buffer_create_tag (buffer, "user", "weight", 700, NULL);
	int user_priority = gtk_text_tag_get_priority (user);

	gtk_text_buffer_set_text (buffer, "abé🙂cdλefg", -1);
	set_tag (buffer, a, 0, 10, TRUE);
	set_tag (buffer, b, 0, 10, TRUE);
	set_tag (buffer, user, 0, 10, TRUE);

	plan = _gtk_source_tag_plan_new (buffer, 2, 8);
	_gtk_source_tag_plan_add_tag (plan, a);
	_gtk_source_tag_plan_add_tag (plan, b);
	_gtk_source_tag_plan_add (plan, b, 4, 6, TRUE);
	_gtk_source_tag_plan_add (plan, a, 0, 3, TRUE);
	_gtk_source_tag_plan_add (plan, a, 7, 10, TRUE);
	_gtk_source_tag_plan_apply (plan);

	for (int i = 0; i < 10; i++)
	{
		g_assert_cmpint (has_tag (buffer, a, i), ==, i < 3 || i >= 7);
		g_assert_cmpint (has_tag (buffer, b, i), ==, i < 2 || i >= 8 || (i >= 4 && i < 6));
		g_assert_true (has_tag (buffer, user, i));
	}

	g_assert_cmpint (gtk_text_tag_get_priority (user), ==, user_priority);

	_gtk_source_tag_plan_free (plan);
	g_object_unref (buffer);
}

static void
test_random_overlays (void)
{

	for (int trial = 0; trial < 300; trial++)
	{
		GtkTextBuffer *buffer = new_buffer (128);
		GtkTextTag *tags[3];
		MutationCount count = { 0, 0 };
		gboolean expected[3][128] = { { FALSE } };
		GtkSourceTagPlan *plan = NULL;
		int start = g_test_rand_int_range (0, 128);
		int end = g_test_rand_int_range (start, 129);

		for (int t = 0; t < 3; t++)
		{
			tags[t] = gtk_text_buffer_create_tag (buffer, NULL, NULL);

			for (int i = 0; i < 20; i++)
			{
				int a = g_test_rand_int_range (0, 128);
				int b = g_test_rand_int_range (a, 129);

				set_tag (buffer, tags[t], a, b, TRUE);
				for (int k = a; k < b; k++)
					expected[t][k] = TRUE;
			}
		}

		plan = _gtk_source_tag_plan_new (buffer, start, end);
		for (int t = 0; t < 2; t++)
		{
			_gtk_source_tag_plan_add_tag (plan, tags[t]);
			for (int k = start; k < end; k++)
				expected[t][k] = FALSE;
		}

		for (int i = 0; i < 100; i++)
		{
			int t = g_test_rand_int_range (0, 2);
			int a = g_test_rand_int_range (0, 128);
			int b = g_test_rand_int_range (a, 129);
			gboolean enabled = g_test_rand_bit ();

			_gtk_source_tag_plan_add (plan, tags[t], a, b, enabled);
			for (int k = MAX (a, start); k < MIN (b, end); k++)
				expected[t][k] = enabled;
		}

		g_signal_connect (buffer, "apply-tag", G_CALLBACK (count_apply), &count);
		g_signal_connect (buffer, "remove-tag", G_CALLBACK (count_remove), &count);
		_gtk_source_tag_plan_apply (plan);

		for (int t = 0; t < 3; t++)
		{
			for (int k = 0; k < 128; k++)
				g_assert_cmpint (has_tag (buffer, tags[t], k), ==, expected[t][k]);
		}

		_gtk_source_tag_plan_free (plan);
		g_object_unref (buffer);
	}

}

static void
test_overlay_existing_tag (void)
{
	GtkTextBuffer *buffer = new_buffer (10);
	GtkSourceTagPlan *plan = _gtk_source_tag_plan_new (buffer, 0, 10);
	GtkTextTag *tag = gtk_text_buffer_create_tag (buffer, "external-class", NULL);

	set_tag (buffer, tag, 0, 10, TRUE);
	_gtk_source_tag_plan_add (plan, tag, 2, 8, FALSE);
	_gtk_source_tag_plan_add (plan, tag, 4, 6, TRUE);
	_gtk_source_tag_plan_apply (plan);

	for (int i = 0; i < 10; i++)
		g_assert_cmpint (has_tag (buffer, tag, i), ==, i < 2 || i >= 8 || (i >= 4 && i < 6));

	set_tag (buffer, tag, 0, 1, FALSE);
	_gtk_source_tag_plan_apply (plan);
	g_assert_false (has_tag (buffer, tag, 0));
	g_assert_true (has_tag (buffer, tag, 1));

	_gtk_source_tag_plan_free (plan);
	g_object_unref (buffer);
}

static void
ensure_highlight (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_source_buffer_ensure_highlight (buffer, &start, &end);
}

static GtkSourceBuffer *
new_source_buffer (GtkSourceLanguageManager **manager_out)
{
	GtkSourceLanguageManager *manager = gtk_source_language_manager_new ();
	char *path = g_test_build_filename (G_TEST_DIST, "language-specs", NULL);
	char *spec_path = g_build_filename (TOP_SRCDIR, "data", "language-specs", NULL);
	const char *paths[] = { path, spec_path, NULL };
	GtkSourceLanguage *language;
	GtkSourceBuffer *buffer;

	gtk_source_language_manager_set_search_path (manager, paths);
	language = gtk_source_language_manager_get_language (manager, "test-tag-plan");
	g_assert_nonnull (language);
	buffer = gtk_source_buffer_new_with_language (language);

	g_free (spec_path);
	g_free (path);

	*manager_out = manager;

	return buffer;
}

static void
test_engine_classes_and_edit (void)
{
	GtkSourceLanguageManager *manager = NULL;
	GtkSourceBuffer *source_buffer = NULL;
	GtkTextBuffer *buffer;
	GtkTextTag *outer;
	GtkTextTag *inner;
	GtkTextTag *base;
	GtkTextTag *boundary;
	GtkTextTag *token;
	GtkTextTag *user;
	GtkTextTagTable *table;
	MutationCount count = { 0, 0 };
	GtkTextIter iter;
	GSList *before[13];

	source_buffer = new_source_buffer (&manager);
	buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_text_buffer_set_text (buffer, "[ab{xy}cd] z\n", -1);
	user = gtk_text_buffer_create_tag (buffer, "user", "weight", 700, NULL);
	set_tag (buffer, user, 0, 13, TRUE);
	ensure_highlight (source_buffer);
	table = gtk_text_buffer_get_tag_table (buffer);
	outer = gtk_text_tag_table_lookup (table, "gtksourceview:context-classes:outer");
	inner = gtk_text_tag_table_lookup (table, "gtksourceview:context-classes:inner");
	base = gtk_text_tag_table_lookup (table, "gtksourceview:context-classes:base");
	boundary = gtk_text_tag_table_lookup (table, "gtksourceview:context-classes:boundary");
	token = gtk_text_tag_table_lookup (table, "gtksourceview:context-classes:token");

	g_assert_nonnull (outer);
	g_assert_nonnull (inner);
	g_assert_nonnull (base);
	g_assert_nonnull (boundary);
	g_assert_nonnull (token);

	for (int i = 0; i < 12; i++)
	{
		g_assert_cmpint (has_tag (buffer, outer, i), ==, (i >= 1 && i < 3) || (i >= 7 && i < 9));
		g_assert_cmpint (has_tag (buffer, inner, i), ==, i >= 3 && i < 7);
		g_assert_cmpint (has_tag (buffer, base, i), ==, !(i >= 3 && i < 7));
		g_assert_cmpint (has_tag (buffer, boundary, i), ==, i == 0 || i == 9);
		g_assert_cmpint (has_tag (buffer, token, i), ==, i == 4);
		g_assert_true (has_tag (buffer, user, i));
	}

	for (int i = 0; i < 13; i++)
	{
		gtk_text_buffer_get_iter_at_offset (buffer, &iter, i);
		before[i] = gtk_text_iter_get_tags (&iter);
	}

	gtk_text_buffer_get_iter_at_offset (buffer, &iter, 2);
	gtk_text_buffer_insert (buffer, &iter, "é", -1);
	g_signal_connect (buffer, "apply-tag", G_CALLBACK (count_apply), &count);
	g_signal_connect (buffer, "remove-tag", G_CALLBACK (count_remove), &count);
	ensure_highlight (source_buffer);
	g_assert_cmpuint (count.apply, ==, 0);
	g_assert_cmpuint (count.remove, ==, 0);

	for (int i = 0; i < 14; i++)
	{
		GSList *after;
		int old = i > 2 ? i - 1 : i;

		gtk_text_buffer_get_iter_at_offset (buffer, &iter, i);
		after = gtk_text_iter_get_tags (&iter);
		g_assert_cmpuint (g_slist_length (after), ==, g_slist_length (before[old]));
		for (GSList *l = before[old]; l != NULL; l = l->next)
			g_assert_nonnull (g_slist_find (after, l->data));
		g_slist_free (after);
	}

	for (int i = 0; i < 13; i++)
		g_slist_free (before[i]);

	g_object_unref (source_buffer);
	g_object_unref (manager);
}

static void
test_engine_external_class (void)
{
	GtkSourceLanguageManager *manager = NULL;
	GtkSourceBuffer *source_buffer = NULL;
	GtkTextBuffer *buffer;
	GtkTextTag *external;

	source_buffer = new_source_buffer (&manager);
	buffer = GTK_TEXT_BUFFER (source_buffer);
	gtk_text_buffer_set_text (buffer, "[ab{xy}cd] z\n", -1);

	external = gtk_text_buffer_create_tag (buffer, "gtksourceview:context-classes:outer", NULL);
	set_tag (buffer, external, 11, 12, TRUE);
	ensure_highlight (source_buffer);

	for (int i = 0; i < 12; i++)
		g_assert_cmpint (has_tag (buffer, external, i), ==,
		                 (i >= 1 && i < 3) || (i >= 7 && i < 9) || i == 11);

	g_object_unref (source_buffer);
	g_object_unref (manager);
}

int
main (int   argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/tag-plan/coalesce-and-unchanged", test_coalesce_and_unchanged);
	g_test_add_func ("/tag-plan/ordered-overrides", test_ordered_overrides);
	g_test_add_func ("/tag-plan/partial-range-and-identity", test_partial_range_and_identity);
	g_test_add_func ("/tag-plan/random-overlays", test_random_overlays);
	g_test_add_func ("/tag-plan/overlay-existing-tag", test_overlay_existing_tag);
	g_test_add_func ("/tag-plan/engine-classes-and-edit", test_engine_classes_and_edit);
	g_test_add_func ("/tag-plan/engine-external-class", test_engine_external_class);

	return g_test_run ();
}

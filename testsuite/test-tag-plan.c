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
test_scan_boundaries (void)
{
	GtkTextBuffer *buffer = new_buffer (8);
	GtkSourceTagPlan *plan = _gtk_source_tag_plan_new (buffer, 2, 6);
	GtkTextTag *tags[4];
	const int ranges[][2] = { { 0, 4 }, { 4, 8 }, { 2, 6 }, { 3, 5 } };
	const char *expected[] = { "11100000", "00000111", "00100100", "00011000" };

	for (int t = 0; t < 4; t++)
	{
		tags[t] = gtk_text_buffer_create_tag (buffer, NULL, NULL);
		set_tag (buffer, tags[t], ranges[t][0], ranges[t][1], TRUE);

		if (t < 3)
			_gtk_source_tag_plan_add (plan, tags[t], 3, 5, FALSE);
	}

	_gtk_source_tag_plan_apply (plan);

	for (int t = 0; t < 4; t++)
	{
		for (int i = 0; i < 8; i++)
		{
			g_assert_cmpint (has_tag (buffer, tags[t], i), ==, expected[t][i] == '1');
		}
	}

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

typedef enum
{
	NOTIFY_ENSURE,
	NOTIFY_EDIT_ENSURE,
	NOTIFY_EDIT,
	NOTIFY_DETACH,
} NotifyAction;

typedef struct
{
	NotifyAction action;
	guint outer_calls;
	guint nested_calls;
	gboolean in_callback;
	gboolean synchronous;
} NotifyState;

static void
highlight_updated_reentrant (GtkSourceBuffer *buffer,
                             GtkTextIter     *start,
                             GtkTextIter     *end,
                             NotifyState     *state)
{
	GtkTextIter iter;

	if (state->in_callback)
	{
		state->nested_calls++;
		g_assert_cmpint (state->action, ==, NOTIFY_EDIT_ENSURE);
		g_assert_cmpint (gtk_text_iter_get_line (start), ==, 0);
		g_assert_cmpint (gtk_text_iter_get_line (end), ==, 0);
		return;
	}

	g_assert_cmpint (gtk_text_iter_get_line (end), ==, gtk_text_iter_get_line (start));

	state->outer_calls++;
	if (state->outer_calls == 1)
	{
		g_assert_cmpint (gtk_text_iter_get_line (start), ==, 20);
		gtk_text_buffer_get_iter_at_line_offset (GTK_TEXT_BUFFER (buffer), &iter,
		                                         state->synchronous ? 700 : 20, 3);
		g_assert_true (gtk_source_buffer_iter_has_context_class (buffer, &iter, "inner"));
	}
	else
	{
		g_assert_cmpint (state->action, ==, NOTIFY_ENSURE);
		g_assert_cmpuint (state->outer_calls, ==, 2);
		g_assert_cmpint (gtk_text_iter_get_line (start), ==, 700);
	}

	state->in_callback = TRUE;
	if (state->action == NOTIFY_DETACH)
	{
		gtk_source_buffer_set_language (buffer, NULL);
	}
	else
	{
		if (state->action == NOTIFY_EDIT_ENSURE || state->action == NOTIFY_EDIT)
		{
			gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, " ", 1);
		}

		if (state->action != NOTIFY_EDIT)
			ensure_highlight (buffer);
	}
	state->in_callback = FALSE;
}

static void
test_notification_reentrancy (gconstpointer data)
{
	GtkSourceLanguageManager *manager = NULL;
	GtkSourceBuffer *source_buffer = NULL;
	GString *text = g_string_new (NULL);
	GtkTextBuffer *buffer;
	GtkTextIter end;
	const int lines[] = { 700, 20 };
	NotifyState state = { .action = GPOINTER_TO_INT (data) / 2 };
	gboolean synchronous = GPOINTER_TO_INT (data) % 2;
	gulong handler;

	state.synchronous = synchronous;

	source_buffer = new_source_buffer (&manager);
	buffer = GTK_TEXT_BUFFER (source_buffer);

	for (int i = 0; i < 1000; i++)
		g_string_append (text, " [ab{xy}cd] z\n");
	gtk_text_buffer_set_text (buffer, text->str, text->len);
	ensure_highlight (source_buffer);

	for (guint i = 0; i < G_N_ELEMENTS (lines); i++)
	{
		if (!synchronous && i == 0)
			continue;

		gtk_text_buffer_get_iter_at_line_offset (buffer, &end, lines[i], 2);
		gtk_text_buffer_insert (buffer, &end, i == 0 || !synchronous ? "{x}" : " ", -1);
	}

	handler = g_signal_connect (buffer,
	                            "highlight-updated",
	                            G_CALLBACK (highlight_updated_reentrant),
	                            &state);

	if (synchronous)
	{
		ensure_highlight (source_buffer);
	}
	else
	{
		while (state.outer_calls == 0)
			g_assert_true (g_main_context_iteration (NULL, FALSE));
	}

	g_signal_handler_disconnect (buffer, handler);
	g_assert_cmpuint (state.outer_calls, ==, state.action == NOTIFY_ENSURE && synchronous ? 2 : 1);
	g_assert_cmpuint (state.nested_calls, ==, state.action == NOTIFY_EDIT_ENSURE ? 1 : 0);

	if (state.action == NOTIFY_DETACH)
	{
		g_assert_null (gtk_source_buffer_get_language (source_buffer));
	}
	else
	{
		ensure_highlight (source_buffer);
		gtk_text_buffer_get_iter_at_line_offset (buffer, &end, synchronous ? 700 : 20, 3);
		g_assert_true (gtk_source_buffer_iter_has_context_class (source_buffer, &end, "inner"));
	}

	g_string_free (text, TRUE);
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
	g_test_add_func ("/tag-plan/scan-boundaries", test_scan_boundaries);
	g_test_add_func ("/tag-plan/random-overlays", test_random_overlays);
	g_test_add_func ("/tag-plan/overlay-existing-tag", test_overlay_existing_tag);
	g_test_add_func ("/tag-plan/engine-classes-and-edit", test_engine_classes_and_edit);
	g_test_add_func ("/tag-plan/engine-external-class", test_engine_external_class);
	g_test_add_data_func ("/tag-plan/notification/ensure-idle", GINT_TO_POINTER (2 * NOTIFY_ENSURE), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/ensure-synchronous", GINT_TO_POINTER (2 * NOTIFY_ENSURE + 1), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/edit-ensure-idle", GINT_TO_POINTER (2 * NOTIFY_EDIT_ENSURE), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/edit-ensure-synchronous", GINT_TO_POINTER (2 * NOTIFY_EDIT_ENSURE + 1), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/edit-idle", GINT_TO_POINTER (2 * NOTIFY_EDIT), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/edit-synchronous", GINT_TO_POINTER (2 * NOTIFY_EDIT + 1), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/detach-idle", GINT_TO_POINTER (2 * NOTIFY_DETACH), test_notification_reentrancy);
	g_test_add_data_func ("/tag-plan/notification/detach-synchronous", GINT_TO_POINTER (2 * NOTIFY_DETACH + 1), test_notification_reentrancy);

	return g_test_run ();
}

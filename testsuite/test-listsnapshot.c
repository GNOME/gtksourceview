/* test-listsnapshot.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gtksourceview/gtksourcelistsnapshot-private.h>

typedef struct
{
	guint call_count;
	guint position;
	guint removed;
	guint added;
} ItemsChanged;

static void
items_changed_cb (GListModel   *model,
		  guint         position,
		  guint         removed,
		  guint         added,
		  ItemsChanged *state)
{
#if 0
	g_printerr ("%d %d %d\n", position, removed, added);
#endif

	state->call_count++;
	state->position = position;
	state->removed = removed;
	state->added = added;
}

static void
test_list_snapshot (void)
{
	GtkSourceListSnapshot *list_snapshot;
	GListStore *store;
	GMenu *menu;
	ItemsChanged state = {0};

	list_snapshot = gtk_source_list_snapshot_new ();
	g_signal_connect (list_snapshot, "items-changed", G_CALLBACK (items_changed_cb), &state);
	g_assert_cmpint (0, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (G_TYPE_OBJECT, ==, g_list_model_get_item_type (G_LIST_MODEL (list_snapshot)));

	store = g_list_store_new (G_TYPE_MENU_MODEL);

	menu = g_menu_new ();
	g_list_store_append (store, menu);

	/* initial model set (with items) */
	gtk_source_list_snapshot_set_model (list_snapshot, G_LIST_MODEL (store));
	g_assert_cmpint (1, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (G_TYPE_MENU_MODEL, ==, g_list_model_get_item_type (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (state.call_count, ==, 1);
	g_assert_cmpint (state.position, ==, 0);
	g_assert_cmpint (state.removed, ==, 0);
	g_assert_cmpint (state.added, ==, 1);

	/* try to reset, no changes/emission */
	gtk_source_list_snapshot_set_model (list_snapshot, G_LIST_MODEL (store));
	g_assert_cmpint (1, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (G_TYPE_MENU_MODEL, ==, g_list_model_get_item_type (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (state.call_count, ==, 1);

	/* clear model */
	gtk_source_list_snapshot_set_model (list_snapshot, NULL);
	g_assert_cmpint (0, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (G_TYPE_OBJECT, ==, g_list_model_get_item_type (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (state.call_count, ==, 2);
	g_assert_cmpint (state.position, ==, 0);
	g_assert_cmpint (state.removed, ==, 1);
	g_assert_cmpint (state.added, ==, 0);

	/* set model again */
	gtk_source_list_snapshot_set_model (list_snapshot, G_LIST_MODEL (store));
	g_assert_cmpint (1, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (G_TYPE_MENU_MODEL, ==, g_list_model_get_item_type (G_LIST_MODEL (list_snapshot)));
	g_assert_cmpint (state.call_count, ==, 3);
	g_assert_cmpint (state.position, ==, 0);
	g_assert_cmpint (state.removed, ==, 0);
	g_assert_cmpint (state.added, ==, 1);

	/* Add some more items so we can hold a range */
	for (guint i = 0; i < 100; i++)
	{
		GMenu *m = g_menu_new ();
		state.call_count = 0;
		g_list_store_append (store, m);
		g_assert_cmpint (state.call_count, ==, 1);
		g_assert_cmpint (state.position, ==, 1+i);
		g_assert_cmpint (state.removed, ==, 0);
		g_assert_cmpint (state.added, ==, 1);
		g_object_unref (m);
	}
	g_assert_cmpint (101, ==, g_list_model_get_n_items (G_LIST_MODEL (store)));
	g_assert_cmpint (101, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));

	/* Hold a range so we can test changing things arround */
	gtk_source_list_snapshot_hold (list_snapshot, 10, 20);
	g_assert_cmpint (101, ==, g_list_model_get_n_items (G_LIST_MODEL (list_snapshot)));
	for (guint i = 0; i <= 100; i++)
	{
		GMenu *item = g_list_model_get_item (G_LIST_MODEL (list_snapshot), i);

		if (i < 10 || i >= 30)
		{
			g_assert_null (item);
		}
		else
		{
			g_assert_nonnull (item);
			g_assert_true (G_IS_MENU (item));
			g_object_unref (item);
		}
	}

	/* Now remove everything and ensure we're still good */
	state.call_count = 0;
	while (g_list_model_get_n_items (G_LIST_MODEL (store)))
	{
		g_list_store_remove (store, 0);
		g_assert_cmpint (state.call_count, ==, 0);
	}

	/* Now release our hold */
	gtk_source_list_snapshot_release (list_snapshot);
	g_assert_cmpint (state.call_count, ==, 1);
	g_assert_cmpint (state.position, ==, 0);
	g_assert_cmpint (state.removed, ==, 101);
	g_assert_cmpint (state.added, ==, 0);

	g_assert_finalize_object (list_snapshot);
	g_assert_finalize_object (store);
	g_assert_finalize_object (menu);
}

int
main (int argc,
      char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/GtkSource/ListSnapshot/basic", test_list_snapshot);
	return g_test_run ();
}

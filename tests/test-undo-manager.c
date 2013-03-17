/*
 * test-undo-manager.c
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

static void
insert_text (GtkSourceBuffer *buffer,
	     gchar           *text)
{
	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer), text, -1);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static void
delete_first_line (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &end, 1);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static void
delete_char_at_offset (GtkSourceBuffer *buffer,
		       gint             offset)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &start, offset);
	end = start;
	gtk_text_iter_forward_char (&end);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

static gchar *
get_contents (GtkSourceBuffer *buffer)
{
	GtkTextIter start;
	GtkTextIter end;

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);
	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer), &end);

	return gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);
}

static void
check_max_undo_levels (GtkSourceBuffer *buffer)
{
	gint max_levels = gtk_source_buffer_get_max_undo_levels (buffer);
	gint nb_redos = 0;
	gint nb_undos = 0;
	gint i = 0;

	g_assert (max_levels >= 0);

	/* Redo all actions */
	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
		nb_redos++;
		g_assert_cmpint (nb_redos, <=, max_levels);
	}

	/* Undo all actions */
	while (gtk_source_buffer_can_undo (buffer))
	{
		gtk_source_buffer_undo (buffer);
		nb_undos++;
		g_assert_cmpint (nb_undos, <=, max_levels);
	}

	/* Add max_levels+1 actions */
	for (i = 0; i <= max_levels; i++)
	{
		insert_text (buffer, "foobar\n");
	}

	/* Check number of possible undos */
	nb_undos = 0;
	while (gtk_source_buffer_can_undo (buffer))
	{
		gtk_source_buffer_undo (buffer);
		nb_undos++;
		g_assert_cmpint (nb_undos, <=, max_levels);
	}

	g_assert_cmpint (nb_undos, ==, max_levels);
}

static void
test_get_set_max_undo_levels (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);

	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), >=, -1);

	gtk_source_buffer_set_max_undo_levels (buffer, -1);
	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), ==, -1);

	gtk_source_buffer_set_max_undo_levels (buffer, 3);
	g_assert_cmpint (gtk_source_buffer_get_max_undo_levels (buffer), ==, 3);

	g_object_unref (buffer);
}

static void
test_single_action (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "foo");
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_redo (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_lose_redo_actions (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "baz\n");
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_max_undo_levels (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
#if 0
	gint min = 0;
#else
	gint min = 1;
#endif
	gint max = 5;
	gint i;

	/* Increase */
	for (i = min; i <= max; i++)
	{
		gtk_source_buffer_set_max_undo_levels (buffer, i);
		check_max_undo_levels (buffer);
	}

	/* Decrease */
	for (i = max; i >= min; i--)
	{
		gtk_source_buffer_set_max_undo_levels (buffer, i);
		check_max_undo_levels (buffer);
	}

	/* can redo: TRUE -> FALSE */
	gtk_source_buffer_set_max_undo_levels (buffer, 3);
	check_max_undo_levels (buffer);

	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
	}

	gtk_source_buffer_undo (buffer);
	g_assert (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, 2);
	g_assert (!gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
test_not_undoable_action (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	/* On empty buffer */
	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "foo\n", -1);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	/* begin_user_action inside */
	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer), "bar\n", -1);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	/* In the middle of an action history */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_undo (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "new text\n", -1);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	/* Empty undoable action */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_begin_not_undoable_action (buffer);
	gtk_source_buffer_end_not_undoable_action (buffer);

	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	/* Behavior _during_ a not undoable action */

	/* FIXME: the API doesn't explain what should be the behaviors in the
	 * following situations (also for nested).
	 * What is certain is that after the last end_not_undoable_action() (if
	 * the calls are nested), it is not possible to undo or redo.
	 */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);

	gtk_source_buffer_begin_not_undoable_action (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (gtk_source_buffer_can_redo (buffer));

	gtk_source_buffer_redo (buffer);
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "new text\n", -1);

	gtk_source_buffer_end_not_undoable_action (buffer);
	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	/* Nested */
	insert_text (buffer, "foo\n");
	insert_text (buffer, "bar\n");
	gtk_source_buffer_undo (buffer);

	gtk_source_buffer_begin_not_undoable_action (buffer);
	insert_text (buffer, "foo\n");

	gtk_source_buffer_begin_not_undoable_action (buffer);
	insert_text (buffer, "inserted text\n");

	gtk_source_buffer_end_not_undoable_action (buffer);
	insert_text (buffer, "blah\n");

	gtk_source_buffer_end_not_undoable_action (buffer);
	g_assert (!gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	insert_text (buffer, "blah\n");
	g_assert (gtk_source_buffer_can_undo (buffer));
	g_assert (!gtk_source_buffer_can_redo (buffer));

	g_object_unref (buffer);
}

static void
check_contents_history (GtkSourceBuffer *buffer,
			GList           *contents_history)
{
	GList *l;

	/* Go to the end */
	while (gtk_source_buffer_can_redo (buffer))
	{
		gtk_source_buffer_redo (buffer);
	}

	/* Check all the undo's */
	for (l = g_list_last (contents_history); l != NULL; l = l->prev)
	{
		gchar *cur_contents = get_contents (buffer);
		g_assert_cmpstr (cur_contents, ==, l->data);
		g_free (cur_contents);

		if (gtk_source_buffer_can_undo (buffer))
		{
			gtk_source_buffer_undo (buffer);
		}
		else
		{
			g_assert (l->prev == NULL);
		}
	}

	/* Check all the redo's */
	for (l = contents_history; l != NULL; l = l->next)
	{
		gchar *cur_contents = get_contents (buffer);
		g_assert_cmpstr (cur_contents, ==, l->data);
		g_free (cur_contents);

		if (gtk_source_buffer_can_redo (buffer))
		{
			gtk_source_buffer_redo (buffer);
		}
		else
		{
			g_assert (l->next == NULL);
		}
	}
}

static void
test_contents (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GList *contents_history = g_list_append (NULL, get_contents (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	insert_text (buffer, "hello\n");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	insert_text (buffer, "world\n");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	delete_first_line (buffer);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	delete_first_line (buffer);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	g_object_unref (buffer);
}

static void
test_merge_actions (void)
{
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GList *contents_history = g_list_append (NULL, get_contents (buffer));

	gtk_source_buffer_set_max_undo_levels (buffer, -1);

	/* Different action types (an insert followed by a delete) */
	insert_text (buffer, "a");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Mergeable inserts */
	insert_text (buffer, "b");
	insert_text (buffer, "c");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Mergeable deletes */
	delete_char_at_offset (buffer, 1);
	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Non-mergeable deletes */
	insert_text (buffer, "def");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 2);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0);
	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Insert two words */
	insert_text (buffer, "g");
	insert_text (buffer, "h");
	insert_text (buffer, " ");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	insert_text (buffer, "i");
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Delete the two words (with backspace) */
	delete_char_at_offset (buffer, 3);

	/* FIXME when testing with gedit, the deletions of 'i' followed by ' ' are
	 * merged. Here they are not merged... */
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 2);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 1);
	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	/* Delete two words (with delete) */
	insert_text (buffer, "jk l");
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0);
	delete_char_at_offset (buffer, 0);
	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));

	delete_char_at_offset (buffer, 0);
	contents_history = g_list_append (contents_history, get_contents (buffer));
	check_contents_history (buffer, contents_history);

	g_list_free_full (contents_history, g_free);
	g_object_unref (buffer);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/UndoManager/test-get-set-max-undo-levels",
			 test_get_set_max_undo_levels);

	g_test_add_func ("/UndoManager/test-single-action",
			 test_single_action);

	g_test_add_func ("/UndoManager/test-lose-redo-actions",
			 test_lose_redo_actions);

	g_test_add_func ("/UndoManager/test-max-undo-levels",
			 test_max_undo_levels);

	g_test_add_func ("/UndoManager/test-not-undoable-action",
			 test_not_undoable_action);

	g_test_add_func ("/UndoManager/test-contents",
			 test_contents);

	g_test_add_func ("/UndoManager/test-merge-actions",
			 test_merge_actions);

	return g_test_run ();
}

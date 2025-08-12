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
#include <gtksourceview/vim/gtksourcevimstate.h>

static void
test_parents (void)
{
	GtkWidget *view = g_object_ref_sink (gtk_source_view_new ());
	GtkSourceVim *vim = gtk_source_vim_new (GTK_SOURCE_VIEW (view));
	GtkSourceVimState *normal = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (vim));
	GtkSourceVimState *insert = gtk_source_vim_insert_new ();
	GtkSourceVimState *command = gtk_source_vim_command_new (":join");

	gtk_source_vim_state_push (normal, g_object_ref (insert));
	g_assert_true (normal == gtk_source_vim_state_get_parent (insert));

	gtk_source_vim_state_pop (insert);
	g_assert_true (normal == gtk_source_vim_state_get_parent (insert));

	gtk_source_vim_state_push (normal, g_object_ref (command));
	gtk_source_vim_state_pop (command);

	/* Now insert should be released */
	g_assert_finalize_object (insert);

	g_assert_true (GTK_SOURCE_IS_VIM_NORMAL (normal));
	g_object_ref (normal);

	g_assert_finalize_object (vim);
	g_assert_finalize_object (normal);
	g_assert_finalize_object (command);
	g_assert_finalize_object (view);
}

int
main (int argc,
      char *argv[])
{
	int ret;

	gtk_init ();
	gtk_source_init ();
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/GtkSourceView/vim-state/set-parent", test_parents);
	ret = g_test_run ();
	gtk_source_finalize ();
	return ret;
}

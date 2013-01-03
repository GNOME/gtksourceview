/*
 * test-completion-model.c
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
#include "gtksourceview/gtksourcecompletionmodel.h"

static void
test_is_empty (void)
{
	GtkSourceCompletionModel *model;

	model = gtk_source_completion_model_new ();
	g_assert (gtk_source_completion_model_is_empty (model, FALSE));
	g_assert (gtk_source_completion_model_is_empty (model, TRUE));

	g_object_unref (model);
}

int
main (int argc, char **argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/CompletionModel/is-empty", test_is_empty);

	return g_test_run ();
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcecompletioncontainer.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013, 2014 - Sébastien Wilmet <swilmet@gnome.org>
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

/*
 * Custom sizing of the scrolled window containing the GtkTreeView containing
 * the completion proposals. If the GtkTreeView is small enough, the scrolled
 * window returns the natural size of the GtkTreeView. If it exceeds a certain
 * size, the scrolled window returns a smaller size, with the height at a row
 * boundary of the GtkTreeView (plus the size of the scrollbar(s) if needed).
 *
 * The purpose is to have a compact completion window, with a certain size
 * limit.
 */

#include "gtksourcecompletioncontainer.h"

#define UNREALIZED_WIDTH  350
#define MAX_HEIGHT        180

G_DEFINE_TYPE (GtkSourceCompletionContainer,
	       _gtk_source_completion_container,
	       GTK_TYPE_SCROLLED_WINDOW);

static gint
get_max_width (GtkSourceCompletionContainer *container)
{
	if (gtk_widget_get_realized (GTK_WIDGET (container)))
	{
		GtkWidget *toplevel;
		GdkWindow *window;
		GdkScreen *screen;
		gint max_width;
		gint xorigin;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
		window = gtk_widget_get_window (toplevel);
		screen = gdk_window_get_screen (window);

		gdk_window_get_origin (window, &xorigin, NULL);
		max_width = gdk_screen_get_width (screen) - xorigin;

		return MAX (max_width, UNREALIZED_WIDTH);
	}

	return UNREALIZED_WIDTH;
}

static gint
get_vertical_scrollbar_width (void)
{
	gint width;
	GtkWidget *scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, NULL);
	g_object_ref_sink (scrollbar);
	gtk_widget_show (scrollbar);

	gtk_widget_get_preferred_width (scrollbar, NULL, &width);

	g_object_unref (scrollbar);
	return width;
}

static gint
get_horizontal_scrollbar_height (void)
{
	gint height;
	GtkWidget *scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, NULL);
	g_object_ref_sink (scrollbar);
	gtk_widget_show (scrollbar);

	gtk_widget_get_preferred_height (scrollbar, NULL, &height);

	g_object_unref (scrollbar);
	return height;
}

/* This condition is used at several places, and it is important that it is the
 * same condition. So a function is better.
 */
static gboolean
needs_vertical_scrollbar (gint child_natural_height)
{
	return MAX_HEIGHT < child_natural_height;
}

static void
get_width (GtkSourceCompletionContainer *container,
	   gint                         *container_width,
	   gint                         *child_available_width)
{
	GtkWidget *child;
	GtkRequisition nat_size;
	gint width;
	gint scrollbar_width = 0;

	child = gtk_bin_get_child (GTK_BIN (container));
	gtk_widget_get_preferred_size (child, NULL, &nat_size);

	width = nat_size.width;

	if (needs_vertical_scrollbar (nat_size.height))
	{
		scrollbar_width = get_vertical_scrollbar_width ();
		width += scrollbar_width;
	}

	width = MIN (width, get_max_width (container));

	if (container_width != NULL)
	{
		*container_width = width;
	}

	if (child_available_width != NULL)
	{
		*child_available_width = width - scrollbar_width;
	}
}

static void
_gtk_source_completion_container_get_preferred_width (GtkWidget *widget,
						      gint      *min_width,
						      gint      *nat_width)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (widget);
	gint width;

	get_width (container, &width, NULL);

	if (min_width != NULL)
	{
		*min_width = width;
	}

	if (nat_width != NULL)
	{
		*nat_width = width;
	}
}

static gint
get_row_height (GtkSourceCompletionContainer *container)
{
	GtkWidget *tree_view;
	GList *columns;
	GList *l;
	gint max_row_height = 0;
	gint vertical_separator = 0;

	tree_view = gtk_bin_get_child (GTK_BIN (container));
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (tree_view));

	for (l = columns; l != NULL; l = l->next)
	{
		GtkTreeViewColumn *column = l->data;
		gint row_height;

		gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &row_height);

		if (row_height > max_row_height)
		{
			max_row_height = row_height;
		}
	}

	gtk_widget_style_get (tree_view,
			      "vertical-separator", &vertical_separator,
			      NULL);

	max_row_height += vertical_separator;

	g_list_free (columns);
	return max_row_height;
}

/* Return a height at a row boundary of the GtkTreeView. */
static void
_gtk_source_completion_container_get_preferred_height (GtkWidget *widget,
						       gint	 *min_height,
						       gint	 *nat_height)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (widget);
	GtkWidget *child;
	GtkRequisition nat_size;
	gint total_height;
	gint scrollbar_height = 0;
	gint child_available_width;
	gint ret_height;

	child = gtk_bin_get_child (GTK_BIN (container));
	gtk_widget_get_preferred_size (child, NULL, &nat_size);

	total_height = nat_size.height;

	get_width (container, NULL, &child_available_width);

	/* Needs horizontal scrollbar */
	if (child_available_width < nat_size.width)
	{
		scrollbar_height = get_horizontal_scrollbar_height ();
		total_height += scrollbar_height;
	}

	if (needs_vertical_scrollbar (nat_size.height))
	{
		gint row_height = get_row_height (container);
		gint nb_rows_allowed = MAX_HEIGHT / row_height;

		ret_height = nb_rows_allowed * row_height + scrollbar_height;
	}
	else
	{
		ret_height = total_height;
	}

	if (min_height != NULL)
	{
		*min_height = ret_height;
	}

	if (nat_height != NULL)
	{
		*nat_height = ret_height;
	}
}

static void
_gtk_source_completion_container_class_init (GtkSourceCompletionContainerClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->get_preferred_width = _gtk_source_completion_container_get_preferred_width;
	widget_class->get_preferred_height = _gtk_source_completion_container_get_preferred_height;
}

static void
_gtk_source_completion_container_init (GtkSourceCompletionContainer *container)
{
}

GtkSourceCompletionContainer *
_gtk_source_completion_container_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_CONTAINER, NULL);
}

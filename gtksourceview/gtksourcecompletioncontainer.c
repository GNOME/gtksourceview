/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcecompletioncontainer.c
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

/*
 * Custom container for the sizing of the GtkTreeView containing the completion
 * proposals. When the GtkTreeView exceeds a certain size, the container adds
 * the GtkTreeView inside a scrolled window. When the GtkTreeView is small
 * enough, no scrolled window is needed, and the container fits the natural size
 * of the GtkTreeView.
 *
 * If the tree view is always in the scrolled window, there are sometimes some
 * sizing issues.
 *
 * The purpose is to have a compact completion window, with a certain size
 * limit.
 */

#include "gtksourcecompletioncontainer.h"

#define MAX_WIDTH  350
#define MAX_HEIGHT 180

struct _GtkSourceCompletionContainerPrivate
{
	GtkTreeView *tree_view;
	GtkWidget *scrolled_window;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceCompletionContainer, _gtk_source_completion_container, GTK_TYPE_BIN);

/* gtk_container_add() is overridden. This function calls the GtkBin's add(). */
static void
bin_container_add (GtkContainer *container,
		   GtkWidget    *widget)
{
	GTK_CONTAINER_CLASS (_gtk_source_completion_container_parent_class)->add (container, widget);
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

static void
remove_scrolled_window (GtkSourceCompletionContainer *container)
{
	GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (container->priv->tree_view));

	if (parent != GTK_WIDGET (container))
	{
		gtk_container_remove (GTK_CONTAINER (container),
				      container->priv->scrolled_window);

		gtk_container_remove (GTK_CONTAINER (container->priv->scrolled_window),
				      GTK_WIDGET (container->priv->tree_view));

		bin_container_add (GTK_CONTAINER (container),
				   GTK_WIDGET (container->priv->tree_view));
	}
}

static void
add_scrolled_window (GtkSourceCompletionContainer *container)
{
	GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (container->priv->tree_view));

	if (parent != container->priv->scrolled_window)
	{
		gtk_container_remove (GTK_CONTAINER (container),
				      GTK_WIDGET (container->priv->tree_view));

		gtk_container_add (GTK_CONTAINER (container->priv->scrolled_window),
				   GTK_WIDGET (container->priv->tree_view));

		bin_container_add (GTK_CONTAINER (container),
				   container->priv->scrolled_window);
	}
}

static void
check_scrolled_window (GtkSourceCompletionContainer *container,
		       GtkRequisition                child_size)
{
	if (child_size.width <= MAX_WIDTH &&
	    child_size.height <= MAX_HEIGHT)
	{
		remove_scrolled_window (container);
	}
	else
	{
		add_scrolled_window (container);
	}
}

static GtkSizeRequestMode
_gtk_source_completion_container_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
_gtk_source_completion_container_get_preferred_width (GtkWidget *widget,
						      gint      *min_width,
						      gint      *nat_width)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (widget);
	GtkRequisition nat_size;
	gint width;

	/* FIXME sometimes the returned height (nat_size.height) is 0. Maybe a
	 * bug in GtkTreeView. If there are sizing problems, or if there are
	 * problems with the scrolled window, a workaround would be to compute
	 * the height manually, like it is done in get_preferred_height() below.
	 */
	gtk_widget_get_preferred_size (GTK_WIDGET (container->priv->tree_view),
				       NULL,
				       &nat_size);

	check_scrolled_window (container, nat_size);

	width = nat_size.width;

	if (MAX_HEIGHT < nat_size.height)
	{
		width += get_vertical_scrollbar_width ();
	}

	width = MIN (width, MAX_WIDTH);

	if (min_width != NULL)
	{
		*min_width = width;
	}

	if (nat_width != NULL)
	{
		*nat_width = width;
	}
}

/* Return a height at a row boundary of the GtkTreeView. */
static void
_gtk_source_completion_container_get_preferred_height (GtkWidget *widget,
						       gint	 *min_height,
						       gint	 *nat_height)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (widget);
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GtkRequisition nat_size;
	gint nb_rows = 0;
	gint row_height = 0;
	gint vertical_separator = 0;
	gint scrollbar_height = 0;
	gint total_height = 0;
	gint ret_height = 0;

	if (min_height != NULL)
	{
		*min_height = 0;
	}

	if (nat_height != NULL)
	{
		*nat_height = 0;
	}

	column = gtk_tree_view_get_column (container->priv->tree_view, 0);

	if (column == NULL)
	{
		return;
	}

	gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &row_height);

	gtk_widget_style_get (GTK_WIDGET (container->priv->tree_view),
			      "vertical-separator", &vertical_separator,
			      NULL);

	row_height += vertical_separator;

	model = gtk_tree_view_get_model (container->priv->tree_view);

	if (model == NULL)
	{
		return;
	}

	nb_rows = gtk_tree_model_iter_n_children (model, NULL);

	total_height = nb_rows * row_height;

	gtk_widget_get_preferred_size (GTK_WIDGET (container->priv->tree_view),
				       NULL,
				       &nat_size);

	check_scrolled_window (container, nat_size);

	if (MAX_WIDTH < nat_size.width)
	{
		scrollbar_height = get_horizontal_scrollbar_height ();
		total_height += scrollbar_height;
	}

	if (total_height <= MAX_HEIGHT)
	{
		ret_height = total_height;
	}
	else
	{
		gint height_available_for_rows = MAX_HEIGHT - scrollbar_height;
		gint nb_rows_allowed = height_available_for_rows / row_height;

		ret_height = nb_rows_allowed * row_height + scrollbar_height;
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
_gtk_source_completion_container_add (GtkContainer *gtk_container,
				      GtkWidget    *widget)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (gtk_container);

	g_return_if_fail (GTK_IS_TREE_VIEW (widget));

	g_clear_object (&container->priv->tree_view);
	container->priv->tree_view = GTK_TREE_VIEW (widget);
	g_object_ref (container->priv->tree_view);

	bin_container_add (gtk_container, widget);
}

static void
_gtk_source_completion_container_init (GtkSourceCompletionContainer *container)
{
	container->priv = _gtk_source_completion_container_get_instance_private (container);

	container->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	g_object_ref_sink (container->priv->scrolled_window);
	gtk_widget_show (container->priv->scrolled_window);
}

static void
_gtk_source_completion_container_finalize (GObject *object)
{
	GtkSourceCompletionContainer *container = GTK_SOURCE_COMPLETION_CONTAINER (object);

	g_clear_object (&container->priv->scrolled_window);
	g_clear_object (&container->priv->tree_view);

	G_OBJECT_CLASS (_gtk_source_completion_container_parent_class)->finalize (object);
}

static void
_gtk_source_completion_container_class_init (GtkSourceCompletionContainerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	object_class->finalize = _gtk_source_completion_container_finalize;

	widget_class->get_request_mode = _gtk_source_completion_container_get_request_mode;
	widget_class->get_preferred_width = _gtk_source_completion_container_get_preferred_width;
	widget_class->get_preferred_height = _gtk_source_completion_container_get_preferred_height;

	container_class->add = _gtk_source_completion_container_add;
}

GtkSourceCompletionContainer *
_gtk_source_completion_container_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_CONTAINER, NULL);
}

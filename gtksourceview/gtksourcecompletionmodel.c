/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 * gtksourcecompletionmodel.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
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

#include "gtksourcecompletionmodel.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourcecompletionproposal.h"

#define GTK_SOURCE_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_SOURCE_TYPE_COMPLETION_MODEL, GtkSourceCompletionModelPrivate))

struct _GtkSourceCompletionModelPrivate
{
	GType column_types[GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS];
};

enum
{
	PROVIDERS_CHANGED,
	BEGIN_DELETE,
	END_DELETE,
	NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = {0,};

static void tree_model_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionModel,
                         gtk_source_completion_model,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                tree_model_iface_init))

/* Interface implementation */

static GtkTreeModelFlags
tree_model_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), 0);

	return GTK_TREE_MODEL_LIST_ONLY | GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
tree_model_get_n_columns (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), 0);

	return GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS;
}

static GType
tree_model_get_column_type (GtkTreeModel *tree_model,
			    gint          index)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (0 <= index && index < GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS, G_TYPE_INVALID);

	return GTK_SOURCE_COMPLETION_MODEL (tree_model)->priv->column_types[index];
}

static gboolean
tree_model_get_iter (GtkTreeModel *tree_model,
		     GtkTreeIter  *iter,
		     GtkTreePath  *path)
{
	GtkSourceCompletionModel *model;
	gint *indices;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	model = GTK_SOURCE_COMPLETION_MODEL (tree_model);
	indices = gtk_tree_path_get_indices (path);

	/* TODO */

	return FALSE;
}

static GtkTreePath *
tree_model_get_path (GtkTreeModel *tree_model,
		     GtkTreeIter  *iter)
{
	GtkSourceCompletionModel *model;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	model = GTK_SOURCE_COMPLETION_MODEL (tree_model);

	/* TODO */

	return NULL;
}

static void
tree_model_get_value (GtkTreeModel *tree_model,
		      GtkTreeIter  *iter,
		      gint          column,
		      GValue       *value)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->user_data != NULL);
	g_return_if_fail (0 <= column && column < GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS);

	/* TODO */
}

static gboolean
tree_model_iter_next (GtkTreeModel *tree_model,
		      GtkTreeIter  *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	/* TODO */
	return FALSE;
}

static gboolean
tree_model_iter_children (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  GtkTreeIter  *parent)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);

	if (parent != NULL)
	{
		return FALSE;
	}
	else
	{
		/* TODO */
		return FALSE;
	}
}

static gboolean
tree_model_iter_has_child (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	return FALSE;
}

static gint
tree_model_iter_n_children (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), 0);
	g_return_val_if_fail (iter == NULL || iter->user_data != NULL, 0);

	if (iter == NULL)
	{
		/* TODO */
		return 0;
	}
	else
	{
		return 0;
	}
}

static gboolean
tree_model_iter_nth_child (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter,
			   GtkTreeIter  *parent,
			   gint          child_num)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);

	if (parent != NULL)
	{
		return FALSE;
	}
	else
	{
		/* TODO */
		return FALSE;
	}
}

static gboolean
tree_model_iter_parent (GtkTreeModel *tree_model,
			GtkTreeIter  *iter,
			GtkTreeIter  *child)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (child != NULL, FALSE);

	iter->user_data = NULL;
	return FALSE;
}

static void
tree_model_iface_init (gpointer g_iface,
                       gpointer iface_data)
{
	GtkTreeModelIface *iface = (GtkTreeModelIface *)g_iface;

	iface->get_flags = tree_model_get_flags;
	iface->get_n_columns = tree_model_get_n_columns;
	iface->get_column_type = tree_model_get_column_type;
	iface->get_iter = tree_model_get_iter;
	iface->get_path = tree_model_get_path;
	iface->get_value = tree_model_get_value;
	iface->iter_next = tree_model_iter_next;
	iface->iter_children = tree_model_iter_children;
	iface->iter_has_child = tree_model_iter_has_child;
	iface->iter_n_children = tree_model_iter_n_children;
	iface->iter_nth_child = tree_model_iter_nth_child;
	iface->iter_parent = tree_model_iter_parent;
}

/* Construction and destruction */

static void
gtk_source_completion_model_dispose (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_completion_model_parent_class)->dispose (object);
}

static void
gtk_source_completion_model_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_completion_model_parent_class)->finalize (object);
}

static void
gtk_source_completion_model_class_init (GtkSourceCompletionModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_completion_model_finalize;
	object_class->dispose = gtk_source_completion_model_dispose;

	signals[PROVIDERS_CHANGED] =
		g_signal_new ("providers-changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GtkSourceCompletionModelClass, providers_changed),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	signals[BEGIN_DELETE] =
		g_signal_new ("begin-delete",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GtkSourceCompletionModelClass, begin_delete),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	signals[END_DELETE] =
		g_signal_new ("end-delete",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (GtkSourceCompletionModelClass, end_delete),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionModelPrivate));
}

static void
gtk_source_completion_model_init (GtkSourceCompletionModel *self)
{
	self->priv = GTK_SOURCE_COMPLETION_MODEL_GET_PRIVATE (self);

	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER] = G_TYPE_OBJECT;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROPOSAL] = G_TYPE_OBJECT;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_LABEL] = G_TYPE_STRING;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_MARKUP] = G_TYPE_STRING;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON] = GDK_TYPE_PIXBUF;
}

/* Public functions */

GtkSourceCompletionModel*
gtk_source_completion_model_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_MODEL, NULL);
}

void
gtk_source_completion_model_begin_populate (GtkSourceCompletionModel *model,
					    GList                    *providers)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
}

void
gtk_source_completion_model_cancel (GtkSourceCompletionModel *model)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
}

void
gtk_source_completion_model_add_proposals (GtkSourceCompletionModel    *model,
					   GtkSourceCompletionProvider *provider,
					   GList                       *proposals)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
}

void
gtk_source_completion_model_end_populate (GtkSourceCompletionModel    *model,
					  GtkSourceCompletionProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
}

void
gtk_source_completion_model_clear (GtkSourceCompletionModel *model)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
}

/* If @only_visible is %TRUE, only the visible providers are taken into account. */
gboolean
gtk_source_completion_model_is_empty (GtkSourceCompletionModel *model,
                                      gboolean                  only_visible)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), FALSE);

	return FALSE;
}

guint
gtk_source_completion_model_n_proposals (GtkSourceCompletionModel    *model,
                                         GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), 0);
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider), 0);

	return 0;
}

void
gtk_source_completion_model_set_show_headers (GtkSourceCompletionModel *model,
                                              gboolean                  show_headers)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
}

gboolean
gtk_source_completion_model_iter_is_header (GtkSourceCompletionModel *model,
                                            GtkTreeIter              *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	return FALSE;
}

gboolean
gtk_source_completion_model_iter_previous (GtkSourceCompletionModel *model,
                                           GtkTreeIter              *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	return FALSE;
}

gboolean
gtk_source_completion_model_iter_last (GtkSourceCompletionModel *model,
                                       GtkTreeIter              *iter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	return FALSE;
}

/* Get all the providers (visible and hidden), sorted by priority in descending
 * order (the highest priority first).
 */
GList *
gtk_source_completion_model_get_providers (GtkSourceCompletionModel *model)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), NULL);

	return NULL;
}

void
gtk_source_completion_model_set_visible_providers (GtkSourceCompletionModel *model,
                                                   GList                    *providers)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model));
}

GList *
gtk_source_completion_model_get_visible_providers (GtkSourceCompletionModel *model)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), NULL);

	return NULL;
}

gboolean
gtk_source_completion_model_iter_equal (GtkSourceCompletionModel *model,
                                        GtkTreeIter              *iter1,
                                        GtkTreeIter              *iter2)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter1 != NULL, FALSE);
	g_return_val_if_fail (iter2 != NULL, FALSE);

	return iter1->user_data == iter2->user_data;
}

/*
 * gtksourcecompletionmodel.c
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "gtksourcecompletionmodel.h"

#define ITEMS_PER_CALLBACK 500
#define FILTER_PER_CALLBACK 1000

#define GTK_SOURCE_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_COMPLETION_MODEL, GtkSourceCompletionModelPrivate))

typedef struct
{
	GtkSourceCompletionModel *model;

	GtkSourceCompletionProvider *provider;
	GtkSourceCompletionProposal *proposal;
	
	GtkSourceCompletionModelFilterFlag filtered;
	gulong changed_id;
} ProposalNode;

typedef struct
{
	GList *item;
	guint num;
	guint visible_items;
} HeaderInfo;

struct _GtkSourceCompletionModelPrivate
{
	GType column_types[GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS];
	GList *store;
	GList *last;
	
	guint num;
	GHashTable *num_per_provider;
	
	GtkSourceCompletionModelVisibleFunc filter;
	gpointer filter_data;
	
	gboolean show_headers;
	
	guint idle_id;
	GQueue *item_queue;
	
	guint idle_filter_id;

	GList *next_filter_item;
	GtkTreeRowReference *next_filter_path;
};

/* Signals */
enum
{
	ITEMS_ADDED,
	FILTER_DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void tree_model_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionModel, 
                         gtk_source_completion_model, 
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                tree_model_iface_init))


/* Interface implementation */
static ProposalNode *
node_from_iter (GtkTreeIter *iter)
{
	return (ProposalNode *)(((GList *)iter->user_data)->data);
}

static GtkTreePath *
path_from_list (GtkSourceCompletionModel *model,
                GList                    *item)
{
	gint index = 0;
	GList *ptr = model->priv->store;
	ProposalNode *node;
	
	while (ptr && ptr != item)
	{
		node = (ProposalNode *)ptr->data;
		
		if (!node->filtered)
		{
			++index;
		}
		
		ptr = g_list_next (ptr);
	}
	
	if (ptr != item)
	{
		return NULL;
	}
	else
	{
		return gtk_tree_path_new_from_indices (index, -1);
	}
}

static GtkTreeModelFlags
tree_model_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), 0);

	return 0;
}

static gint
tree_model_get_n_columns (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), 0);

	return GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS;
}

static GType
tree_model_get_column_type (GtkTreeModel *tree_model,
			    gint          index)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), G_TYPE_INVALID);
	g_return_val_if_fail (index >= 0 && index < GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS, G_TYPE_INVALID);

	return GTK_SOURCE_COMPLETION_MODEL (tree_model)->priv->column_types[index];
}

static gboolean
get_iter_from_index (GtkSourceCompletionModel *model,
                     GtkTreeIter              *iter,
                     gint                      index)
{
	GList *item;
	ProposalNode *node;

	if (index < 0 || index >= model->priv->num)
	{
		return FALSE;
	}
	
	item = model->priv->store;
	
	while (item != NULL && index >= 0)
	{
		node = (ProposalNode *)item->data;
		
		if (!node->filtered)
		{
			--index;
		}

		if (index != -1)
		{
			item = g_list_next (item);
		}
	}
	
	if (item != NULL)
	{
		iter->user_data = item;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static gboolean
tree_model_get_iter (GtkTreeModel *tree_model,
		     GtkTreeIter  *iter, 
		     GtkTreePath  *path)
{
	GtkSourceCompletionModel *model;
	gint *indices;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	
	model = GTK_SOURCE_COMPLETION_MODEL (tree_model);
	indices = gtk_tree_path_get_indices (path);
	
	return get_iter_from_index (model, iter, indices[0]);
}

static GtkTreePath *
tree_model_get_path (GtkTreeModel *tree_model,
		     GtkTreeIter  *iter)
{
	GtkSourceCompletionModel *model;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	model = GTK_SOURCE_COMPLETION_MODEL (tree_model);
	
	return path_from_list (model, (GList *)iter->user_data);
}

static void
tree_model_get_value (GtkTreeModel *tree_model,
		      GtkTreeIter  *iter, 
		      gint          column,
		      GValue       *value)
{
	ProposalNode *node;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (iter->user_data != NULL);
	g_return_if_fail (column >= 0 && column < GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS);

	node = node_from_iter (iter);

	g_value_init (value, GTK_SOURCE_COMPLETION_MODEL (tree_model)->priv->column_types[column]);

	switch (column)
	{
		case GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER:
			g_value_set_object (value, node->provider);
			break;
		case GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROPOSAL:
			g_value_set_object (value, node->proposal);
			break;
		case GTK_SOURCE_COMPLETION_MODEL_COLUMN_LABEL:
			g_value_set_string (value, gtk_source_completion_proposal_get_label (node->proposal));
			break;
		case GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON:
			if (node->proposal == NULL)
			{
				g_value_set_object (value, 
				                    (gpointer)gtk_source_completion_provider_get_icon (
				                    	node->provider));
			}
			else
			{
				g_value_set_object (value, 
				                    (gpointer)gtk_source_completion_proposal_get_icon (
				                    	node->proposal));
			}
			break;
	}
}

static gboolean
find_first_not_filtered (GList       *item,
                         GtkTreeIter *iter)
{
	ProposalNode *node;

	while (item)
	{
		node = (ProposalNode *)item->data;
		
		if (!node->filtered)
		{
			break;
		}
		
		item = g_list_next (item);
	}
	
	if (item != NULL)
	{
		iter->user_data = item;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static gboolean
tree_model_iter_next (GtkTreeModel *tree_model,
		      GtkTreeIter  *iter)
{
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	
	item = g_list_next ((GList *)iter->user_data);
	
	return find_first_not_filtered (item, iter);
}

static gboolean
tree_model_iter_children (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  GtkTreeIter  *parent)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);
	
	if (parent != NULL)
	{
		return FALSE;
	}
	else
	{
		return find_first_not_filtered (GTK_SOURCE_COMPLETION_MODEL (tree_model)->priv->store, iter);
	}
}

static gboolean
tree_model_iter_has_child (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	
	return FALSE;
}

static gint
tree_model_iter_n_children (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), 0);
	g_return_val_if_fail (iter == NULL || iter->user_data != NULL, 0);
	
	if (iter == NULL)
	{
		return GTK_SOURCE_COMPLETION_MODEL (tree_model)->priv->num;
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
			   gint          n)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);

	if (parent != NULL)
	{
		return FALSE;
	}
	else
	{
		return get_iter_from_index (GTK_SOURCE_COMPLETION_MODEL (tree_model), 
		                            iter, 
		                            n);
	}
}

static gboolean
tree_model_iter_parent (GtkTreeModel *tree_model,
			GtkTreeIter  *iter,
			GtkTreeIter  *child)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (tree_model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	
	iter->user_data = NULL;
	return FALSE;
}

static void
tree_model_row_inserted (GtkTreeModel *tree_model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter)
{
}

static void
tree_model_row_deleted (GtkTreeModel *tree_model,
			 GtkTreePath  *path)
{
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
	
	iface->row_inserted = tree_model_row_inserted;
	iface->row_deleted = tree_model_row_deleted;
}

static void
free_node (ProposalNode *node)
{
	g_object_unref (node->provider);
	
	if (node->proposal != NULL)
	{
		if (node->changed_id != 0)
		{
			g_signal_handler_disconnect (node->proposal,
						     node->changed_id);
		}
		g_object_unref (node->proposal);
	}
	
	g_slice_free (ProposalNode, node);
}

static void
cancel_append (GtkSourceCompletionModel *model)
{
	if (model->priv->item_queue != NULL)
	{
		g_queue_foreach (model->priv->item_queue,
				 (GFunc)free_node, NULL);
		g_queue_clear (model->priv->item_queue);
	}

	if (model->priv->idle_id != 0)
	{
		g_source_remove (model->priv->idle_id);
		model->priv->idle_id = 0;
	}	
}

static void
cancel_refilter (GtkSourceCompletionModel *model)
{
	if (model->priv->next_filter_path != NULL)
	{
		gtk_tree_row_reference_free (model->priv->next_filter_path);
		model->priv->next_filter_path = NULL;
	}
	
	if (model->priv->idle_filter_id != 0)
	{
		g_source_remove (model->priv->idle_filter_id);
		model->priv->idle_filter_id = 0;
		
		g_signal_emit (model, signals[FILTER_DONE], 0);
	}
}

static void
gtk_source_completion_model_dispose (GObject *object)
{
	GtkSourceCompletionModel *model = GTK_SOURCE_COMPLETION_MODEL (object);

	cancel_append (model);
	cancel_refilter (model);
	
	if (model->priv->item_queue != NULL)
	{
		g_queue_free (model->priv->item_queue);
		model->priv->item_queue = NULL;
	}
	
	if (model->priv->num_per_provider != NULL)
	{
		g_hash_table_destroy (model->priv->num_per_provider);
		model->priv->num_per_provider = NULL;
	}
	
	g_list_foreach (model->priv->store, (GFunc)free_node, NULL);
	g_list_free (model->priv->store);
	model->priv->store = NULL;

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

	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionModelPrivate));
	
	signals[ITEMS_ADDED] =
		g_signal_new ("items-added",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionModelClass, items_added),
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);

	signals[FILTER_DONE] =
		g_signal_new ("filter-done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionModelClass, filter_done),
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);

}

static void
free_num (gpointer data)
{
	g_slice_free (HeaderInfo, data);
}

static void
gtk_source_completion_model_init (GtkSourceCompletionModel *self)
{
	self->priv = GTK_SOURCE_COMPLETION_MODEL_GET_PRIVATE (self);
	
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER] = G_TYPE_OBJECT;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROPOSAL] = G_TYPE_OBJECT;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_LABEL] = G_TYPE_STRING;
	self->priv->column_types[GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON] = GDK_TYPE_PIXBUF;
	
	self->priv->num_per_provider = g_hash_table_new_full (g_direct_hash,
	                                                      g_direct_equal,
	                                                      NULL,
	                                                      free_num);
	
	self->priv->idle_id = 0;
	self->priv->item_queue = g_queue_new ();
	g_queue_init (self->priv->item_queue);
}

static void
num_inc (GtkSourceCompletionModel           *model,
         GtkSourceCompletionProvider        *provider,
         gboolean                            inc_local,
         gboolean                            inc_global)
{
	HeaderInfo *info;
	
	info = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (inc_global)
	{
		++model->priv->num;
		
		if (info != NULL)
		{
			++info->visible_items;
		}
	}
	
	if (inc_local && info != NULL)
	{
		++(info->num);
	}
}

static void
num_dec (GtkSourceCompletionModel           *model,
         GtkSourceCompletionProvider        *provider,
         gboolean                            dec_local,
         gboolean                            dec_global)
{
	HeaderInfo *info;
	
	info = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (dec_global)
	{
		--model->priv->num;
		
		if (info != NULL)
		{
			--info->visible_items;
		}
	}

	if (dec_local && info != NULL && info->num > 0)
	{
		--(info->num);
	}
}

static GtkSourceCompletionModelFilterFlag
node_update_filter_state (GtkSourceCompletionModel *model, 
                          ProposalNode             *node)
{
	GtkSourceCompletionModelFilterFlag ret;
	
	if (node->proposal == NULL)
	{
		return node->filtered;
	}
	
	ret = node->filtered;
	
	node->filtered = model->priv->filter (model, 
	                                      node->provider, 
	                                      node->proposal, 
	                                      model->priv->filter_data);

	return ret;
}

static void
update_show_headers (GtkSourceCompletionModel *model,
                     gboolean                  show)
{
	GtkSourceCompletionProvider *provider;
	HeaderInfo *info;
	guint num = 0;
	GList *items = NULL;
	GList *item;
	GHashTableIter hiter;
	
	ProposalNode *node;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!model->priv->show_headers)
	{
		return;
	}
	
	/* Check headers */
	g_hash_table_iter_init (&hiter, model->priv->num_per_provider);
	
	while (g_hash_table_iter_next (&hiter, (gpointer *)&provider, (gpointer *)&info))
	{
		if (info->visible_items > 0)
		{
			node = (ProposalNode *)info->item->data;
			++num;
			
			if (show && node->filtered)
			{
				items = g_list_append (items, info);
			}
			
			if (!show && !node->filtered)
			{
				items = g_list_append (items, info);
			}
		}
	}

	if (show && num > 1 && items != NULL)
	{
		for (item = items; item; item = g_list_next (item))
		{
			info = (HeaderInfo *)item->data;
			node = (ProposalNode *)info->item->data;
			
			node->filtered = GTK_SOURCE_COMPLETION_MODEL_NONE;
			iter.user_data = info->item;
			
			num_inc (model, node->provider, FALSE, TRUE);

			path = path_from_list (model, info->item);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (model),
			                             path,
			                             &iter);
			gtk_tree_path_free (path);
		}
	}
	
	if (!show && num <= 1 && items)
	{
		info = (HeaderInfo *)items->data;
		node = (ProposalNode *)info->item->data;
		
		num_dec (model, node->provider, FALSE, TRUE);

		node->filtered = GTK_SOURCE_COMPLETION_MODEL_FILTERED;
		path = path_from_list (model, info->item);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		gtk_tree_path_free (path);
	}

	g_list_free (items);
}

static void
refilter_headers (GtkSourceCompletionModel *model)
{
	GtkSourceCompletionProvider *provider;
	HeaderInfo *info;
	GHashTableIter hiter;
	ProposalNode *node;
	GtkTreePath *path;

	g_hash_table_iter_init (&hiter, model->priv->num_per_provider);
	
	while (g_hash_table_iter_next (&hiter, (gpointer *)&provider, (gpointer *)&info))
	{
		node = (ProposalNode *)info->item->data;
		
		if (!node->filtered)
		{
			node->filtered = GTK_SOURCE_COMPLETION_MODEL_FILTERED;
			num_dec (model, provider, FALSE, TRUE);
			
			path = path_from_list (model, info->item);
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (model),
			                            path);
			gtk_tree_path_free (path);
		}
	}

	if (model->priv->show_headers)
	{
		update_show_headers (model, TRUE);
		return;
	}
}

/* Public */
GtkSourceCompletionModel*
gtk_source_completion_model_new (GtkSourceCompletionModelVisibleFunc func,
                                 gpointer                            userdata)
{
	GtkSourceCompletionModel *model = g_object_new (GTK_TYPE_SOURCE_COMPLETION_MODEL, NULL);
	
	model->priv->filter = func;
	model->priv->filter_data = userdata;

	return model;
}

static void
append_list (GtkSourceCompletionModel *model,
             ProposalNode             *node)
{
	GList *item;
	
	item = g_list_append (model->priv->last, node);
	
	if (model->priv->store == NULL)
	{
		model->priv->store = item;
	}
	else
	{
		item = item->next;
	}
	
	model->priv->last = item;
}

static void
on_proposal_changed (GtkSourceCompletionProposal *proposal,
                     GList                       *item)
{
	GtkTreeIter iter;
	ProposalNode *node = (ProposalNode *)item->data;
	GtkTreePath *path;

	if (!node->filtered)
	{
		iter.user_data = node;
		path = path_from_list (node->model, item);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (node->model),
		                            path,
		                            &iter);
		gtk_tree_path_free (path);
	}
}

static gboolean
idle_append (gpointer data)
{
	GtkSourceCompletionModel *model = GTK_SOURCE_COMPLETION_MODEL (data);
	HeaderInfo *info;
	GtkTreePath *path;
	GList *item;
	gint i = 0;
	
	while (i < ITEMS_PER_CALLBACK)
	{
		ProposalNode *node = (ProposalNode *)g_queue_pop_head (model->priv->item_queue);
		ProposalNode *header = NULL;
		GtkTreeIter iter;
		
		if (node == NULL)
		{
			/* If we are here we added all elements of the queue */
			g_signal_emit (model, signals[ITEMS_ADDED], 0);
			
			model->priv->idle_id = 0;
			
			return FALSE;
		}
		
		/* Check if it is a header */
		if (g_hash_table_lookup (model->priv->num_per_provider, node->provider) == NULL)
		{
			header = g_slice_new (ProposalNode);
			header->provider = g_object_ref (node->provider);
			header->proposal = NULL;
			header->filtered = GTK_SOURCE_COMPLETION_MODEL_FILTERED;
			
			append_list (model, header);
			
			info = g_slice_new (HeaderInfo);
			info->item = model->priv->last;
			info->num = 0;
			info->visible_items = 0;
			
			g_hash_table_insert (model->priv->num_per_provider, node->provider, info);
		}
		
		node_update_filter_state (model, node);
		
		append_list (model, node);
		
		item = model->priv->last;
		iter.user_data = item;

		num_inc (model, 
			 node->provider, 
			 !node->filtered || (node->filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
			 !node->filtered);

		if (!node->filtered)
		{
			path = path_from_list (model, item);
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
			gtk_tree_path_free (path);
			
			if (header != NULL)
			{
				update_show_headers (model, TRUE);
			}
		}
		
		node->changed_id = g_signal_connect (node->proposal, 
	                                             "changed", 
	                                             G_CALLBACK (on_proposal_changed),
	                                             item);
		
		i++;
	}
	
	return TRUE;
}

void
gtk_source_completion_model_run_add_proposals (GtkSourceCompletionModel *model)
{
	if (idle_append (model))
	{
		model->priv->idle_id =
			g_idle_add ((GSourceFunc)idle_append,
				    model);
	}
}

void
gtk_source_completion_model_append (GtkSourceCompletionModel    *model,
                                    GtkSourceCompletionProvider *provider,
                                    GtkSourceCompletionProposal *proposal)
{
	ProposalNode *node;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal));
	

	node = g_slice_new (ProposalNode);
	node->provider = g_object_ref (provider);
	node->proposal = g_object_ref (proposal);
	node->changed_id = 0;
	
	g_queue_push_tail (model->priv->item_queue, node);
}

void
gtk_source_completion_model_clear (GtkSourceCompletionModel *model)
{
	GtkTreePath *path;
	ProposalNode *node;
	GList *list;
	GtkSourceCompletionModelFilterFlag filtered;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	
	/* Clear the queue of missing elements to append */
	cancel_append (model);
	cancel_refilter (model);
	
	path = gtk_tree_path_new_first ();
	list = model->priv->store;
	
	while (model->priv->store)
	{
		node = (ProposalNode *)model->priv->store->data;
		filtered = node->filtered;

		free_node (node);
		
		model->priv->store = model->priv->store->next;
		
		if (model->priv->store == NULL)
		{
			model->priv->last = NULL;
		}
		
		num_dec (model, 
		         node->provider, 
		         (!filtered || (filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT)) && node->proposal != NULL,
		         !filtered);
		
		if (!filtered)
		{
			gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		}
	}
	
	g_list_free (list);
	gtk_tree_path_free (path);
	
	g_hash_table_remove_all (model->priv->num_per_provider);
}

static gboolean
idle_refilter (GtkSourceCompletionModel *model)
{
	guint i = 0;
	GtkTreePath *path;
	GtkTreeIter iter;
	ProposalNode *node;
	GtkSourceCompletionModelFilterFlag filtered;

	if (model->priv->next_filter_path)
	{
		path = gtk_tree_row_reference_get_path (model->priv->next_filter_path);
		gtk_tree_row_reference_free (model->priv->next_filter_path);
		model->priv->next_filter_path = NULL;
	}
	else
	{
		path = gtk_tree_path_new_first ();
	}
	
	while (i < FILTER_PER_CALLBACK && model->priv->next_filter_item != NULL)
	{
		iter.user_data = model->priv->next_filter_item;

		node = (ProposalNode *)model->priv->next_filter_item->data;
		filtered = node_update_filter_state (model, node);
		
		if ((filtered != 0) == (node->filtered != 0))
		{
			/* Keep the same, so increase path */
			if (!filtered)
			{
				gtk_tree_path_next (path);
			}
		}
		else if (filtered)
		{
			/* Was filtered, but not any more, so insert it */
			num_inc (model, 
			         node->provider,
			         !(filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
			         TRUE);
			         
			gtk_tree_model_row_inserted (GTK_TREE_MODEL (model),
			                             path,
			                             &iter);
			gtk_tree_path_next (path);
		}
		else
		{
			/* Was not filtered, but is now, so remove it */
			num_dec (model, 
			         node->provider,
			         !(node->filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
			         TRUE);

			gtk_tree_model_row_deleted (GTK_TREE_MODEL (model),
			                            path);
		}
		
		model->priv->next_filter_item = g_list_next (model->priv->next_filter_item);
		++i;
	}

	refilter_headers (model);

	if (model->priv->next_filter_item == NULL)
	{
		model->priv->idle_filter_id = 0;
		gtk_tree_path_free (path);
		
		g_signal_emit (model, signals[FILTER_DONE], 0);

		return FALSE;
	}
	
	if (gtk_tree_path_prev (path))
	{
		model->priv->next_filter_path = gtk_tree_row_reference_new (GTK_TREE_MODEL (model),
		                                                            path);
	}
	
	gtk_tree_path_free (path);

	return TRUE;
}

void
gtk_source_completion_model_refilter (GtkSourceCompletionModel *model)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	
	/* Cancel any running filter */
	cancel_refilter (model);
	
	model->priv->next_filter_item = model->priv->store;
	
	if (idle_refilter (model))
	{
		model->priv->idle_filter_id = g_idle_add ((GSourceFunc)idle_refilter, 
		                                          model);
	}
}

gboolean
gtk_source_completion_model_is_empty (GtkSourceCompletionModel *model,
                                      gboolean                  invisible)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	
	if (invisible)
	{
		return model->priv->num == 0;
	}
	else
	{
		return model->priv->num == 0 || 
		       gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL) == 0;
	}
}

guint
gtk_source_completion_model_n_proposals (GtkSourceCompletionModel    *model,
                                         GtkSourceCompletionProvider *provider)
{
	HeaderInfo *info;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), 0);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), 0);
	
	info = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (info == NULL)
	{
		return 0;
	}
	else
	{
		return info->num;
	}
}

void 
gtk_source_completion_model_set_show_headers (GtkSourceCompletionModel *model,
					      gboolean                  show_headers)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	
	if (model->priv->show_headers != show_headers)
	{
		model->priv->show_headers = show_headers;
		refilter_headers (model);
	}
}

gboolean
gtk_source_completion_model_iter_is_header (GtkSourceCompletionModel *model,
                                            GtkTreeIter              *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	return node_from_iter (iter)->proposal == NULL;
}

gboolean
gtk_source_completion_model_iter_previous (GtkSourceCompletionModel *model,
                                           GtkTreeIter              *iter)
{
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	
	item = iter->user_data;
	
	do
	{
		item = g_list_previous (item);
	} while (item && ((ProposalNode *)item->data)->filtered);

	
	if (item != NULL)
	{
		iter->user_data = item;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

gboolean
gtk_source_completion_model_iter_last (GtkSourceCompletionModel *model,
                                       GtkTreeIter              *iter)
{
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	
	item = model->priv->last;
	iter->user_data = item;

	if (!((ProposalNode *)item->data)->filtered)
	{
		return TRUE;
	}
	else if (item != NULL)
	{
		return gtk_source_completion_model_iter_previous (model, iter);
	}
	else
	{
		return FALSE;
	}
}

#include "gtksourcecompletionmodel.h"

#define GTK_SOURCE_COMPLETION_MODEL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_COMPLETION_MODEL, GtkSourceCompletionModelPrivate))

typedef struct
{
	GtkSourceCompletionProvider *provider;
	GtkSourceCompletionProposal *proposal;
	
	GtkSourceCompletionModelFilterFlag filtered;
} ProposalNode;

struct _GtkSourceCompletionModelPrivate
{
	GType column_types[GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS];
	GList *store;
	GList *last;
	
	guint num;
	GHashTable *num_per_provider;
	
	GtkSourceCompletionModelVisibleFunc filter;
	gpointer filter_data;
};

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
			g_value_set_object (value, (gpointer)gtk_source_completion_proposal_get_icon (node->proposal));
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
gtk_source_completion_model_finalize (GObject *object)
{
	GtkSourceCompletionModel *model = GTK_SOURCE_COMPLETION_MODEL (object);
	
	g_hash_table_destroy (model->priv->num_per_provider);

	G_OBJECT_CLASS (gtk_source_completion_model_parent_class)->finalize (object);
}

static void
gtk_source_completion_model_class_init (GtkSourceCompletionModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gtk_source_completion_model_finalize;

	g_type_class_add_private (object_class, sizeof(GtkSourceCompletionModelPrivate));
}

static void
free_num (gpointer data)
{
	g_slice_free (guint, data);
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
}

static void
num_inc (GtkSourceCompletionModel           *model,
         GtkSourceCompletionProvider        *provider,
         gboolean                            inc_local,
         gboolean                            inc_global)
{
	guint *num;
	
	if (inc_global)
	{
		++model->priv->num;
	}
	
	if (!inc_local)
	{
		return;
	}

	num = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (num != NULL)
	{
		++(*num);
	}
	else
	{
		num = g_slice_new (guint);
		*num = 1;

		g_hash_table_insert (model->priv->num_per_provider, provider, num);
	}
	
}

static void
num_dec (GtkSourceCompletionModel           *model,
         GtkSourceCompletionProvider        *provider,
         gboolean                            dec_local,
         gboolean                            dec_global)
{
	guint *num;
	
	if (dec_global)
	{
		--model->priv->num;
	}

	if (!dec_local)
	{
		return;
	}
	
	num = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (num != NULL && *num > 0)
	{
		--(*num);
	}
}

static GtkSourceCompletionModelFilterFlag
node_update_filter_state (GtkSourceCompletionModel *model, 
                          ProposalNode             *node)
{
	GtkSourceCompletionModelFilterFlag ret;
	
	ret = node->filtered;
	
	node->filtered = model->priv->filter (model, 
	                                      node->provider, 
	                                      node->proposal, 
	                                      model->priv->filter_data);

	return ret;
}

static void
free_node (ProposalNode *node)
{
	g_object_unref (node->provider);
	g_object_unref (node->proposal);
	
	g_slice_free (ProposalNode, node);
}

static void
remove_node (GtkSourceCompletionModel *model,
             GList                    *item)
{
	ProposalNode *node;
	GtkTreePath *path;
	GtkSourceCompletionModelFilterFlag filtered;
	GtkSourceCompletionProvider *provider;
	
	path = path_from_list (model, item);

	node = (ProposalNode *)item->data;
	provider = g_object_ref (node->provider);

	filtered = node->filtered;
	free_node (node);

	if (item == model->priv->last)
	{
		model->priv->last = item->prev;
	}

	model->priv->store = g_list_remove_link (model->priv->store, item);
	
	num_dec (model, 
	         provider, 
	         !filtered || (filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
	         !filtered);
	
	if (!filtered)
	{
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	}

	gtk_tree_path_free (path);
	g_object_unref (provider);
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

gboolean 
gtk_source_completion_model_append (GtkSourceCompletionModel    *model,
                                    GtkSourceCompletionProvider *provider,
                                    GtkSourceCompletionProposal *proposal,
                                    GtkTreeIter                 *iter)
{
	ProposalNode *node;
	GList *item;
	GtkTreePath *path;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROPOSAL (proposal), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	
	node = g_slice_new (ProposalNode);
	node->provider = g_object_ref (provider);
	node->proposal = g_object_ref (proposal);

	node_update_filter_state (model, node);
	
	item = model->priv->last;
	item = g_list_append (item, node);
	
	if (model->priv->store == NULL)
	{
		model->priv->store = item;
	}
	else
	{
		item = item->next;
	}
	
	model->priv->last = item;
	iter->user_data = item;
	
	num_inc (model, 
	         node->provider, 
	         !node->filtered || (node->filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
	         !node->filtered);

	if (!node->filtered)
	{
		path = path_from_list (model, item);
		
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, iter);
		gtk_tree_path_free (path);
	}
	
	return TRUE;
}

gboolean 
gtk_source_completion_model_remove (GtkSourceCompletionModel    *model,
                                    GtkTreeIter                 *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);
	
	remove_node (model, (GList *)iter->user_data);
	return TRUE;
}

void
gtk_source_completion_model_clear (GtkSourceCompletionModel *model)
{
	GtkTreePath *path;
	ProposalNode *node;
	GList *list;
	GtkSourceCompletionModelFilterFlag filtered;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	
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
		         !filtered || (filtered & GTK_SOURCE_COMPLETION_MODEL_COUNT),
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

void
gtk_source_completion_model_refilter (GtkSourceCompletionModel *model)
{
	GList *item;
	GtkSourceCompletionModelFilterFlag filtered;
	ProposalNode *node;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model));
	
	path = gtk_tree_path_new_first ();
	
	for (item = model->priv->store; item != NULL; item = g_list_next (item))
	{
		iter.user_data = item;

		node = (ProposalNode *)item->data;
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
	}

	gtk_tree_path_free (path);
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
	guint *num;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_MODEL (model), 0);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), 0);
	
	num = g_hash_table_lookup (model->priv->num_per_provider, provider);
	
	if (num == NULL)
	{
		return 0;
	}
	else
	{
		return *num;
	}
}


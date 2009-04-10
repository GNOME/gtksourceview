/*
 * gtksourcecompletion.c
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */
 
/**
 * SECTION:gtksourcecompletion
 * @title: GtkSourceCompletion
 * @short_description: Main Completion Object
 *
 * This is the main completion object. It manages all providers (#GtkSourceProvider) and 
 * triggers (#GtkSourceTrigger) for a #GtkTextView.
 */

#include <gdk/gdkkeysyms.h> 
#include "gtksourcecompletionutils.h"
#include "gtksourceview-marshal.h"
#include <gtksourceview/gtksourcecompletion.h>
#include "gtksourceview-i18n.h"
#include <string.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 200

#define GTK_SOURCE_COMPLETION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
						  GTK_TYPE_SOURCE_COMPLETION,           \
						  GtkSourceCompletionPriv))

enum
{
	COLUMN_PIXBUF,
	COLUMN_NAME,
	COLUMN_DATA,
	N_COLUMNS
};

/* Signals */
enum
{
	PROPOSAL_SELECTED,
	DISPLAY_INFO,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_MANAGE_KEYS,
	PROP_REMEMBER_INFO_VISIBILITY,
	PROP_SELECT_ON_SHOW
};

enum
{
	TEXT_VIEW_KP,
	TEXT_VIEW_FOCUS_OUT,
	TEXT_VIEW_BUTTON_PRESS,
	LAST_EXTERNAL_SIGNAL
};

typedef struct 
{
	GtkSourceCompletionTrigger *trigger;
	GtkSourceCompletionProvider *provider;
} PTPair;

typedef struct _GtkSourceCompletionPage GtkSourceCompletionPage;
struct _GtkSourceCompletionPage
{
	gchar *name;
	
	GtkScrolledWindow *scroll;
	GtkTreeView *treeview;
	GtkListStore *list_store;
	GtkTreeModelFilter *model_filter;
	
	gboolean filter_active;
	gpointer filter_data;
	GtkSourceCompletionFilterFunc filter_func;
};

struct _GtkSourceCompletionPriv
{
	/* Widget and popup variables*/
	GtkWidget *info_window;
	GtkWidget *info_button;
	GtkWidget *notebook;
	GtkWidget *tab_label;
	GtkWidget *next_page_button;
	GtkWidget *prev_page_button;
	GtkWidget *bottom_bar;
	
	GList *pages;
	GtkSourceCompletionPage *active_page;
	gboolean destroy_has_run;
	gboolean manage_keys;
	gboolean remember_info_visibility;
	gboolean info_visible;
	gboolean select_on_show;
	
	/* Completion management */
	GtkTextView *view;
	GList *triggers;
	GList *prov_trig;
	GtkSourceCompletionTrigger *active_trigger;
	
	gulong signals_ids[LAST_EXTERNAL_SIGNAL];
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GtkSourceCompletion, gtk_source_completion, GTK_TYPE_WINDOW);

static gboolean
get_selected_proposal (GtkSourceCompletionPage *page,
		       GtkSourceCompletionProposal **proposal)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	
	selection = gtk_tree_view_get_selection (page->treeview);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
	{
		model = gtk_tree_view_get_model (page->treeview);
		
		gtk_tree_model_get (model, &iter,
				    COLUMN_DATA,
				    proposal, -1);
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
select_current_proposal (GtkSourceCompletion *self)
{
	gboolean selected = TRUE;
	GtkSourceCompletionProposal *prop = NULL;
	
	if (get_selected_proposal (self->priv->active_page, &prop))
	{
		g_signal_emit (G_OBJECT (self), signals[PROPOSAL_SELECTED],
			       0, prop, &selected);
		selected = TRUE;
	}
	else
	{
		selected = FALSE;
	}
	
	return selected;
}

static gboolean
select_first_proposal (GtkSourceCompletionPage *page)
{
	GtkTreeIter iter;
	GtkTreePath* path;
	GtkTreeModel* model;
	GtkTreeSelection* selection;
	
	selection = gtk_tree_view_get_selection (page->treeview);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
		return FALSE;

	model = gtk_tree_view_get_model (page->treeview);
		
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		gtk_tree_selection_select_iter (selection, &iter);
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_scroll_to_cell (page->treeview,
					      path, 
					      NULL, 
					      FALSE, 
					      0, 
					      0);
		gtk_tree_path_free (path);
		return TRUE;
	}
	
	return FALSE;
}

static gboolean 
select_last_proposal (GtkSourceCompletionPage *page)
{
	GtkTreeIter iter;
	GtkTreeModel* model;
	GtkTreeSelection* selection;
	GtkTreePath* path;
	gint children;
	
	if (!GTK_WIDGET_VISIBLE (page->treeview))
		return FALSE;
	
	selection = gtk_tree_view_get_selection (page->treeview);
	model = gtk_tree_view_get_model (page->treeview);
	
	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
		return FALSE;
	
	children = gtk_tree_model_iter_n_children (model, NULL);
	if (children > 0)
	{
		gtk_tree_model_iter_nth_child (model, &iter,
					       NULL, children - 1);
	
		gtk_tree_selection_select_iter (selection, &iter);
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_scroll_to_cell (page->treeview,
					      path, 
					      NULL, 
					      FALSE, 
					      0, 
					      0);
		gtk_tree_path_free (path);
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
select_previous_proposal (GtkSourceCompletionPage *page,
			  gint rows)
{
	GtkTreeIter iter;
	GtkTreePath* path;
	GtkTreeModel* model;
	GtkTreeSelection* selection;
	
	if (!GTK_WIDGET_VISIBLE (page->treeview))
		return FALSE;
	
	selection = gtk_tree_view_get_selection (page->treeview);
	
	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
		return FALSE;
	
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		gint i;
		
		path = gtk_tree_model_get_path (model, &iter);
		
		for (i=0; i  < rows; i++)
			gtk_tree_path_prev (path);
		
		if (gtk_tree_model_get_iter(model, &iter, path))
		{
			gtk_tree_selection_select_iter (selection, &iter);
			gtk_tree_view_scroll_to_cell (page->treeview,
						      path, 
						      NULL, 
						      FALSE, 
						      0, 
						      0);
		}
		gtk_tree_path_free (path);
	}
	else
	{
		return select_first_proposal (page);
	}
	
	return TRUE;
}

static gboolean
select_next_proposal (GtkSourceCompletionPage *page,
		      gint rows)
{
	GtkTreeIter iter;
	GtkTreeModel* model;
	GtkTreeSelection* selection;
	
	if (!GTK_WIDGET_VISIBLE (page->treeview))
		return FALSE;
	
	selection = gtk_tree_view_get_selection (page->treeview);
	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
		return FALSE;
	
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		GtkTreePath* path;
		gint i;
		
		for (i = 0; i < rows; i++)
		{
			if (!gtk_tree_model_iter_next (model, &iter))
				return select_last_proposal (page);
		}
		gtk_tree_selection_select_iter (selection, &iter);
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_scroll_to_cell (page->treeview,
					      path, 
					      NULL, 
					      FALSE, 
					      0, 
					      0);
		gtk_tree_path_free (path);
	}
	else
	{
		return select_first_proposal (page);
	}
	
	return TRUE;
}

static void
update_info_pos (GtkSourceCompletion *self)
{
	GtkSourceCompletionProposal *proposal = NULL;
	gint y, x, sw, sh;
	
	if (get_selected_proposal (self->priv->active_page, &proposal))
	{
		gboolean ret = TRUE;
		g_signal_emit (self, signals[DISPLAY_INFO], 0, proposal, &ret);
	}
	
	gtk_window_get_position (GTK_WINDOW (self), &x, &y);
	sw = gdk_screen_width ();
	sh = gdk_screen_height ();
	x += WINDOW_WIDTH;

	if (x + WINDOW_WIDTH >= sw)
	{
		x -= (WINDOW_WIDTH * 2);
	}
	gtk_window_move (GTK_WINDOW (self->priv->info_window), x, y);
	gtk_window_set_transient_for (GTK_WINDOW (self->priv->info_window),
				      gtk_window_get_transient_for (GTK_WINDOW (self)));
}

static gboolean
filter_func (GtkTreeModel *model,
	     GtkTreeIter *iter,
	     gpointer data)
{
	GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)data;
	GtkSourceCompletionProposal *proposal = NULL;
	
	if (!page->filter_active || !page->filter_func)
		return TRUE;
		
	gtk_tree_model_get (model,
			    iter,
			    COLUMN_DATA,
			    &proposal,
			    -1);
	
	if (proposal == NULL)
		return TRUE;
	
	return page->filter_func (proposal, page->filter_data);
}

static void
row_activated_cb (GtkTreeView *tree_view,
		  GtkTreePath *path,
		  GtkTreeViewColumn *column,
		  GtkSourceCompletion *self)
{
	select_current_proposal (self);
}

static void 
selection_changed_cd (GtkTreeSelection *selection, 
		      GtkSourceCompletion *self)
{
	if (GTK_WIDGET_VISIBLE (self->priv->info_window))
	{
		update_info_pos (self);
	}
}

static void
add_proposal (GtkSourceCompletionPage *page,
	      GtkSourceCompletionProposal *data)
{
	GtkTreeIter iter;

	g_assert (data != NULL);
	
	gtk_list_store_append (page->list_store, &iter);
			
	gtk_list_store_set (page->list_store, 
			    &iter,
			    COLUMN_PIXBUF, gtk_source_completion_proposal_get_icon (data),
			    COLUMN_NAME, gtk_source_completion_proposal_get_label (data),
			    COLUMN_DATA, data,
			    -1);
}

static void
clear_treeview (GtkSourceCompletionPage *page)
{
	GtkTreeModel *model = GTK_TREE_MODEL (page->list_store);
	GtkTreeIter iter;
	GtkSourceCompletionProposal *data;
	
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			gtk_tree_model_get (model, &iter,
					    COLUMN_DATA, &data, -1);
			g_object_unref (data);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
	
	gtk_list_store_clear (page->list_store);
}

static gint 
get_num_proposals (GtkSourceCompletionPage *page)
{
	GtkTreeModel *model = gtk_tree_view_get_model (page->treeview);
	
	return gtk_tree_model_iter_n_children (model, NULL);
}

static void
filter_visible (GtkSourceCompletionPage *page,
		GtkSourceCompletionFilterFunc func,
		gpointer user_data)
{
	page->filter_active = TRUE;
	page->filter_data = user_data;
	page->filter_func = func;
	gtk_tree_model_filter_refilter (page->model_filter);
	page->filter_active = FALSE;
	page->filter_data = NULL;
	page->filter_func = NULL;
}

static void
free_pair (gpointer data)
{
	PTPair *ptp = (PTPair *)data;
	g_object_unref (ptp->provider);
	g_slice_free (PTPair, ptp);
}

static void
end_completion (GtkSourceCompletion *self)
{
	if (GTK_WIDGET_VISIBLE (self))
	{
		gtk_widget_hide (GTK_WIDGET (self));
		/*
		* We are connected to the hide signal too. Then end_completion 
		* will be called again but the popup will not be visible
		*/
	}
	else
	{
		GList *l;
		
		for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
		{
			PTPair *ptp = (PTPair *)l->data;
			if (ptp->trigger == self->priv->active_trigger)
				gtk_source_completion_provider_finish (GTK_SOURCE_COMPLETION_PROVIDER (ptp->provider));
		}
		
		self->priv->active_trigger = NULL;
	}
}

static void
gtk_source_completion_clear (GtkSourceCompletion *self)
{
	GList *l;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));
	
	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		
		clear_treeview (page);
	}
}

static gboolean
update_pages_visibility (GtkSourceCompletion *self)
{
	GList *l;
	gboolean first_set = FALSE;
	guint num_pages_with_data = 0;
	gint i = 0;
	GtkAdjustment *ad;
	
	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		
		if (get_num_proposals (page) > 0)
		{
			/*Selects the first page with data*/
			if (!first_set)
			{
				gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook),
							       i);
				ad = gtk_tree_view_get_vadjustment (page->treeview);
				gtk_adjustment_set_value (ad, 0);
				ad = gtk_tree_view_get_hadjustment (page->treeview);
				gtk_adjustment_set_value (ad, 0);
				first_set = TRUE;
			}
			num_pages_with_data++;
		}
		i++;
	}
	if (num_pages_with_data > 1)
	{
		gtk_widget_show (self->priv->next_page_button);
		gtk_widget_show (self->priv->prev_page_button);
	}
	else
	{
		gtk_widget_hide (self->priv->next_page_button);
		gtk_widget_hide (self->priv->prev_page_button);
	}
	
	return num_pages_with_data > 0;
}

static void
gtk_source_completion_show_or_update (GtkWidget *widget)
{
	gboolean data;
	
	/*Only show the popup, the positions is set before this function*/
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (widget);
	
	data = update_pages_visibility (self);
	
	if (data && !GTK_WIDGET_VISIBLE (self))
	{
		GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->show (widget);
		
		if (!self->priv->remember_info_visibility)
			self->priv->info_visible = FALSE;
		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->info_button),
					      self->priv->info_visible);
	}
}

static void
gtk_source_completion_page_next (GtkSourceCompletion *self)
{
	gint pages;
	gint current_page;
	gint page;
	GtkSourceCompletionPage *popup_page;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));

	pages = g_list_length (self->priv->pages);
	current_page = g_list_index (self->priv->pages, self->priv->active_page);
	page = current_page;
	
	do
	{
		if (page == pages - 1)
		{
			page = 0;
		}
		else
		{
			page++;
		}
		popup_page = (GtkSourceCompletionPage *)g_list_nth_data (self->priv->pages, page);
	}
	while (get_num_proposals (popup_page) == 0 &&
	       page != current_page);

	if (page != current_page)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook),
					       page);
	
		/*
		 * After setting the page the active_page was updated
		 * so we can update the tree
		 */
		select_first_proposal (popup_page);
	
		if (GTK_WIDGET_VISIBLE (self->priv->info_window))
		{
			update_info_pos (self);
		}
	}
}

static void
gtk_source_completion_page_previous (GtkSourceCompletion *self)
{
	gint pages;
	gint current_page;
	gint page;
	GtkSourceCompletionPage *popup_page;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));
	
	pages = g_list_length (self->priv->pages);
	current_page = g_list_index (self->priv->pages,
				     self->priv->active_page);
	page = current_page;
	
	do
	{
		if (page == 0)
		{
			page = pages - 1;
		}
		else
		{
			page--;
		}
		popup_page = (GtkSourceCompletionPage *)g_list_nth_data (self->priv->pages,
									 page);
	}
	while (get_num_proposals (popup_page) == 0 &&
	       page != current_page);
	
	if (page != current_page)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook),
					       page);
	
		/*
		 * After setting the page the active_page was updated
		 * so we can update the tree
		 */
		select_first_proposal (popup_page);
	
		if (GTK_WIDGET_VISIBLE (self->priv->info_window))
		{
			update_info_pos (self);
		}
	}
}

static GtkSourceCompletionPage *
gtk_source_completion_page_new (GtkSourceCompletion *self,
				const gchar *tree_name)
{
	/*Creates the new trees*/
	GtkSourceCompletionPage *page;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkCellRenderer* renderer_pixbuf;
	GtkTreeViewColumn* column_pixbuf;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkWidget *label;
	
	page = g_slice_new (GtkSourceCompletionPage);
	
	page->name = g_strdup (tree_name);
	
	page->treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_widget_show (GTK_WIDGET (page->treeview));
	
	page->filter_data = NULL;
	page->filter_active = FALSE;
	page->filter_func = NULL;

	g_object_set (page->treeview, "can-focus", FALSE, NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (page->treeview),
					   FALSE);
	
	/* Create the Tree */
	renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
	column_pixbuf = gtk_tree_view_column_new_with_attributes ("Pixbuf",
								  renderer_pixbuf, 
								  "pixbuf", 
								  COLUMN_PIXBUF, 
								  NULL);
	
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", COLUMN_NAME, NULL);

	gtk_tree_view_append_column (page->treeview, column_pixbuf);
	gtk_tree_view_append_column (page->treeview, column);
	
	/* Create the model */
	page->list_store = gtk_list_store_new (N_COLUMNS,
					       GDK_TYPE_PIXBUF, 
					       G_TYPE_STRING, 
					       G_TYPE_POINTER);
	
	model = gtk_tree_model_filter_new (GTK_TREE_MODEL (page->list_store),
					   NULL);

	page->model_filter = GTK_TREE_MODEL_FILTER (model);
	
	gtk_tree_model_filter_set_visible_func (page->model_filter,
						filter_func,
						page,
						NULL);

	gtk_tree_view_set_model (page->treeview,
				 model);
	
	g_signal_connect (page->treeview,
			  "row-activated",
			  G_CALLBACK (row_activated_cb),
			  self);
	
	selection = gtk_tree_view_get_selection (page->treeview);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (selection_changed_cd),
			  self);
	
	self->priv->pages = g_list_append (self->priv->pages,
					   page);
	
	page->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_widget_show (GTK_WIDGET (page->scroll));
	
	gtk_scrolled_window_set_policy (page->scroll,
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_container_add (GTK_CONTAINER (page->scroll),
			   GTK_WIDGET (page->treeview));
	
	label = gtk_label_new (tree_name);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
				  GTK_WIDGET (page->scroll),
				  label);
	
	return page;
}

static GtkSourceCompletionPage *
get_page_by_name (GtkSourceCompletion *self,
		  const gchar* tree_name)
{
	GtkSourceCompletionPage *page;
	GList *l;
	
	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		page = (GtkSourceCompletionPage *)l->data;
	
		if (strcmp (page->name, tree_name) == 0)
			return page;
	}
	
	page = gtk_source_completion_page_new (self, tree_name);

	return page;
}

static void
info_toggled_cb (GtkToggleButton *widget,
		 gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	if (gtk_toggle_button_get_active (widget))
	{
		gtk_widget_show (self->priv->info_window);
	}
	else
	{
		gtk_widget_hide (self->priv->info_window);
	}
}

static void
next_page_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	gtk_source_completion_page_next (self);
}

static void
prev_page_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	gtk_source_completion_page_previous (self);
}

static gboolean
switch_page_cb (GtkNotebook *notebook, 
		GtkNotebookPage *n_page,
		gint page_num, 
		gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	/* Update the active page */
	self->priv->active_page = (GtkSourceCompletionPage *)g_list_nth_data (self->priv->pages,
									      page_num);
	
	gtk_label_set_label (GTK_LABEL (self->priv->tab_label),
			     self->priv->active_page->name);

	return FALSE;
}

static void
show_info_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	g_return_if_fail (GTK_WIDGET_VISIBLE (GTK_WIDGET (self)));
	
	update_info_pos (self);
	self->priv->info_visible = TRUE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->info_button),
				      TRUE);
}

static void
hide_info_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	self->priv->info_visible = FALSE;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->info_button),
				      FALSE);
}

static gboolean
gtk_source_completion_display_info_default (GtkSourceCompletion *self,
					    GtkSourceCompletionProposal *proposal)
{
	if (proposal)
	{
		const gchar *info;
		
		info = gtk_source_completion_proposal_get_info (proposal);
		
		if (info != NULL)
		{
			gtk_source_completion_info_set_markup (GTK_SOURCE_COMPLETION_INFO (self->priv->info_window),
							       info);
		}
		else
		{
			gtk_source_completion_info_set_markup (GTK_SOURCE_COMPLETION_INFO (self->priv->info_window),
							       _("There is no info for the current proposal"));
		}
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
gtk_source_completion_proposal_selected_default (GtkSourceCompletion *self,
						 GtkSourceCompletionProposal *proposal)
{
	gtk_source_completion_proposal_apply (proposal, self->priv->view);
	end_completion (self);
	
	return FALSE;
}

static void
gtk_source_completion_hide (GtkWidget *widget)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (widget);
	gboolean info_visible = self->priv->info_visible;
	
	GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->hide (widget);
	
	//setting to FALSE, hide the info window
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->info_button),
				      FALSE);
	
	self->priv->info_visible = info_visible;
}

static void
gtk_source_completion_realize (GtkWidget *widget)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (widget);
	
	gtk_container_set_border_width (GTK_CONTAINER (self), 1);
	gtk_widget_set_size_request (GTK_WIDGET (self),
				     WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE);
	
	GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->realize (widget);
}

static gboolean
gtk_source_completion_delete_event_cb (GtkWidget *widget,
				       GdkEvent  *event,
				       gpointer   user_data) 
{
	/*Prevent the alt+F4 keys*/
	return TRUE;
}

static void
free_page (gpointer data,
	   gpointer user_data)
{
	GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)data;
	
	g_free (page->name);
	g_slice_free (GtkSourceCompletionPage, page);
}

static gboolean
gtk_source_completion_configure_event (GtkWidget *widget,
				       GdkEventConfigure *event)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (widget);
	gboolean ret;
	
	ret = GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->configure_event (widget, event);
	
	if (GTK_WIDGET_VISIBLE (self->priv->info_window))
		update_info_pos (self);
	
	return ret;
}

static gboolean
view_focus_out_event_cb (GtkWidget *widget,
			 GdkEventFocus *event,
			 gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (self)
	    && !GTK_WIDGET_HAS_FOCUS (self))
		end_completion (self);
	
	return FALSE;
}

static gboolean
view_button_press_event_cb (GtkWidget *widget,
			    GdkEventButton *event,
			    gpointer user_data)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (self))
		end_completion (self);

	return FALSE;
}

static void
gtk_source_completion_finalize (GObject *object)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (object);
	
	if (self->priv->pages != NULL)
	{
		g_list_foreach (self->priv->pages, (GFunc) free_page, NULL);
		g_list_free (self->priv->pages);
	}
	
	if (self->priv->triggers != NULL)
	{
		g_list_foreach (self->priv->triggers, (GFunc) g_object_unref,
				NULL);
		g_list_free (self->priv->triggers);
	}
	
	if (self->priv->prov_trig != NULL)
	{
		g_list_foreach (self->priv->prov_trig, (GFunc) free_pair,
				NULL);
		g_list_free (self->priv->prov_trig);
	}
	
	g_signal_handler_disconnect (self->priv->view,
				     self->priv->signals_ids[TEXT_VIEW_FOCUS_OUT]);
	g_signal_handler_disconnect (self->priv->view,
				     self->priv->signals_ids[TEXT_VIEW_BUTTON_PRESS]);
	
	G_OBJECT_CLASS (gtk_source_completion_parent_class)->finalize (object);
}

static void
gtk_source_completion_destroy (GtkObject *object)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (object);
	
	if (!self->priv->destroy_has_run)
	{
		gtk_source_completion_clear (self);
		self->priv->destroy_has_run = TRUE;
	}
	GTK_OBJECT_CLASS (gtk_source_completion_parent_class)->destroy (object);
}

static gboolean
view_key_press_event_cb (GtkWidget *view,
			 GdkEventKey *event, 
			 gpointer user_data)
{
	gboolean ret = FALSE;
	GtkSourceCompletion *self;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (user_data), FALSE);
	
	self = GTK_SOURCE_COMPLETION (user_data);
	
	if (!GTK_WIDGET_VISIBLE (self))
		return FALSE;
	
	switch (event->keyval)
 	{
		case GDK_Escape:
		{
			gtk_widget_hide (GTK_WIDGET (self));
			ret = TRUE;
			break;
		}
 		case GDK_Down:
		{
			ret = select_next_proposal (self->priv->active_page, 1);
			break;
		}
		case GDK_Page_Down:
		{
			ret = select_next_proposal (self->priv->active_page, 5);
			break;
		}
		case GDK_Up:
		{
			ret = select_previous_proposal (self->priv->active_page, 1);
			if (!ret)
				ret = select_first_proposal (self->priv->active_page);
			break;
		}
		case GDK_Page_Up:
		{
			ret = select_previous_proposal (self->priv->active_page, 5);
			break;
		}
		case GDK_Home:
		{
			ret = select_first_proposal (self->priv->active_page);
			break;
		}
		case GDK_End:
		{
			ret = select_last_proposal (self->priv->active_page);
			break;
		}
		case GDK_Return:
		case GDK_Tab:
		{
			ret = select_current_proposal (self);
			gtk_widget_hide (GTK_WIDGET (self));
			break;
		}
		case GDK_Right:
		{
			gtk_source_completion_page_next (self);
			ret = TRUE;
			break;
		}
		case GDK_Left:
		{
			gtk_source_completion_page_previous (self);
			ret = TRUE;
			break;
		}
		case GDK_i:
		{
			if ((event->state & GDK_CONTROL_MASK) != 0)
			{
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->info_button),
					!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->info_button)));
				ret = TRUE;
			}
		}
	}
	return ret;
}

static void
set_manage_keys (GtkSourceCompletion *self)
{
	if (self->priv->manage_keys && self->priv->signals_ids[TEXT_VIEW_KP] == 0)
	{
		self->priv->signals_ids[TEXT_VIEW_KP] = 
			g_signal_connect (self->priv->view,
					  "key-press-event",
					  G_CALLBACK (view_key_press_event_cb),
					  self);
	}
	
	if (!self->priv->manage_keys && self->priv->signals_ids[TEXT_VIEW_KP] != 0)
	{
		g_signal_handler_disconnect (self->priv->view,
					     self->priv->signals_ids[TEXT_VIEW_KP]);
		self->priv->signals_ids[TEXT_VIEW_KP] = 0;
	}
}

static void
gtk_source_completion_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	GtkSourceCompletion *self;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (object));
	
	self = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
		case PROP_MANAGE_KEYS:
			g_value_set_boolean (value, self->priv->manage_keys);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			g_value_set_boolean (value, self->priv->remember_info_visibility);
			break;
		case PROP_SELECT_ON_SHOW:
			g_value_set_boolean (value, self->priv->select_on_show);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	GtkSourceCompletion *self;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (object));
	
	self = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
		case PROP_MANAGE_KEYS:
			self->priv->manage_keys = g_value_get_boolean (value);
			set_manage_keys (self);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			self->priv->remember_info_visibility = g_value_get_boolean (value);
			break;
		case PROP_SELECT_ON_SHOW:
			self->priv->select_on_show = g_value_get_boolean (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_class_init (GtkSourceCompletionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (GtkSourceCompletionPriv));
	
	object_class->get_property = gtk_source_completion_get_property;
	object_class->set_property = gtk_source_completion_set_property;
	object_class->finalize = gtk_source_completion_finalize;
	gtkobject_class->destroy = gtk_source_completion_destroy;
	widget_class->show = gtk_source_completion_show_or_update;
	widget_class->hide = gtk_source_completion_hide;
	widget_class->realize = gtk_source_completion_realize;
	widget_class->configure_event = gtk_source_completion_configure_event;
	klass->display_info = gtk_source_completion_display_info_default;
	klass->proposal_selected = gtk_source_completion_proposal_selected_default;
	
	/**
	 * GtkSourceCompletion:manage-completion-keys:
	 *
	 * %TRUE if this object must control the completion keys pressed into the
	 * #GtkTextView associated (up next proposal, down previous proposal etc.)
	 */
	g_object_class_install_property (object_class,
					 PROP_MANAGE_KEYS,
					 g_param_spec_boolean ("manage-completion-keys",
							      _("Manage Up, Down etc. keys when the completion is visible"),
							      _("Manage Up, Down etc. keys when the completion is visible"),
							      TRUE,
							      G_PARAM_READWRITE));
	/**
	 * GtkSourceCompletion:remember-info-visibility:
	 *
	 * %TRUE if the completion must remember the last info state
	 * (visible or hidden)
	 */
	g_object_class_install_property (object_class,
					 PROP_REMEMBER_INFO_VISIBILITY,
					 g_param_spec_boolean ("remember-info-visibility",
							      _("Remember the last info state (visible or hidden)"),
							      _("Remember the last info state (visible or hidden)"),
							      FALSE,
							      G_PARAM_READWRITE));
	/**
	 * GtkSourceCompletion:select-on-show:
	 *
	 * %TRUE if the completion must to mark as selected the first proposal
	 * on show (and when the filter is updated)
	 */
	g_object_class_install_property (object_class,
					 PROP_SELECT_ON_SHOW,
					 g_param_spec_boolean ("select-on-show",
							      _("Completion mark as selected the first proposal on show"),
							      _("Completion mark as selected the first proposal on show"),
							      FALSE,
							      G_PARAM_READWRITE));
	
	/**
	 * GtkSourceCompletion::proposal-selected:
	 * @completion: The #GtkSourceCompletion who emits the signal
	 * @proposal: The selected #GtkSourceCompletionProposal
	 *
	 * When the user selects a proposal
	 **/
	signals[PROPOSAL_SELECTED] =
		g_signal_new ("proposal-selected",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionClass, proposal_selected),
			      g_signal_accumulator_true_handled, 
			      NULL,
			      _gtksourceview_marshal_BOOLEAN__POINTER, 
			      G_TYPE_BOOLEAN,
			      1,
			      GTK_TYPE_POINTER);
	
	/**
	 * GtkSourceCompletion::display-info:
	 * @completion: The #GtkSourceCompletion who emits the signal
	 * @proposal: The #GtkSourceCompletionProposal the user wants to see the information
	 *
	 * When the user want to see the information of a proposal
	 **/	      
	signals[DISPLAY_INFO] =
		g_signal_new ("display-info",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionClass, display_info),
			      g_signal_accumulator_true_handled, 
			      NULL,
			      _gtksourceview_marshal_BOOLEAN__POINTER,
			      G_TYPE_BOOLEAN,
			      1,
			      GTK_TYPE_POINTER);
}

static void
gtk_source_completion_init (GtkSourceCompletion *self)
{
	GtkWidget *info_icon;
	GtkWidget *info_button;
	GtkWidget *next_page_icon;
	GtkWidget *prev_page_icon;
	GtkWidget *vbox;

	self->priv = GTK_SOURCE_COMPLETION_GET_PRIVATE (self);
	self->priv->destroy_has_run = FALSE;
	self->priv->active_trigger = NULL;
	self->priv->manage_keys = TRUE;
	self->priv->remember_info_visibility = FALSE;
	self->priv->info_visible = FALSE;
	self->priv->select_on_show = FALSE;

	gtk_window_set_type_hint (GTK_WINDOW (self),
				  GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_focus_on_map (GTK_WINDOW (self), FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (self),
				     WINDOW_WIDTH,WINDOW_HEIGHT);
	gtk_window_set_decorated (GTK_WINDOW (self), FALSE);
	
	/*Notebook*/
	self->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (self->priv->notebook);
	g_object_set (G_OBJECT (self->priv->notebook), "can-focus",
		      FALSE, NULL);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (self->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (self->priv->notebook),
				      FALSE);
	
	/* Add default page */
	self->priv->active_page = gtk_source_completion_page_new (self, DEFAULT_PAGE);
	
	/*Icon list*/
	info_icon = gtk_image_new_from_stock (GTK_STOCK_INFO,
					      GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (info_icon);
	gtk_widget_set_tooltip_text (info_icon, _("Show Proposal Info"));
	
	info_button = gtk_toggle_button_new ();
	gtk_widget_show (info_button);
	g_object_set (G_OBJECT (info_button), "can-focus", FALSE, NULL);
	
	self->priv->info_button = info_button;
	gtk_button_set_focus_on_click (GTK_BUTTON (info_button), FALSE);
	gtk_container_add (GTK_CONTAINER (info_button), info_icon);
	g_signal_connect (G_OBJECT (info_button),
			  "toggled",
			  G_CALLBACK (info_toggled_cb),
			  self);

	self->priv->bottom_bar = gtk_hbox_new (FALSE, 1);
	gtk_widget_show (self->priv->bottom_bar);
	gtk_box_pack_start (GTK_BOX (self->priv->bottom_bar), info_button,
			    FALSE, FALSE, 0);

	/*Next/Previous page buttons*/
	
	next_page_icon = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD,
						   GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (next_page_icon);
	prev_page_icon = gtk_image_new_from_stock (GTK_STOCK_GO_BACK,
						   GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_widget_show (prev_page_icon);
	
	self->priv->next_page_button = gtk_button_new ();
	gtk_widget_show (self->priv->next_page_button);
	g_object_set (G_OBJECT (self->priv->next_page_button),
		      "can-focus", FALSE,
		      "focus-on-click", FALSE,
		      NULL);
	gtk_widget_set_tooltip_text (self->priv->next_page_button,
				     _("Next page"));
	gtk_container_add (GTK_CONTAINER (self->priv->next_page_button),
			   next_page_icon);
	g_signal_connect (G_OBJECT (self->priv->next_page_button),
			  "clicked",
			  G_CALLBACK (next_page_cb),
			  self);
	
	self->priv->prev_page_button = gtk_button_new ();
	gtk_widget_show (self->priv->prev_page_button);
	g_object_set (G_OBJECT (self->priv->prev_page_button),
		      "can-focus", FALSE,
		      "focus-on-click", FALSE,
		      NULL);
	gtk_widget_set_tooltip_text (self->priv->prev_page_button,
				     _("Previous page"));
	gtk_container_add (GTK_CONTAINER (self->priv->prev_page_button),
			   prev_page_icon);
	g_signal_connect (G_OBJECT (self->priv->prev_page_button),
			  "clicked",
			  G_CALLBACK (prev_page_cb),
			  self);

	gtk_box_pack_end (GTK_BOX (self->priv->bottom_bar),
			  self->priv->next_page_button,
			  FALSE, FALSE, 1);
	gtk_box_pack_end (GTK_BOX (self->priv->bottom_bar),
			  self->priv->prev_page_button,
			  FALSE, FALSE, 1);
	/*Page label*/
	self->priv->tab_label = gtk_label_new (DEFAULT_PAGE);
	gtk_widget_show (self->priv->tab_label);
	gtk_box_pack_end (GTK_BOX (self->priv->bottom_bar),
			  self->priv->tab_label,
			  FALSE, TRUE, 10);
	
	/*Main vbox*/
	vbox = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (vbox), self->priv->notebook,
			    TRUE, TRUE, 0);

	gtk_box_pack_end (GTK_BOX (vbox), self->priv->bottom_bar,
			  FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (self), vbox);

	/*Info window*/
	self->priv->info_window = GTK_WIDGET (gtk_source_completion_info_new ());

	/* Connect signals */
			
	g_signal_connect (self->priv->notebook, 
			  "switch-page",
			  G_CALLBACK (switch_page_cb),
			  self);
			
	g_signal_connect (self,
			  "delete-event",
			  G_CALLBACK (gtk_source_completion_delete_event_cb),
			  NULL);

	g_signal_connect (self->priv->info_window,
			  "show-info",
			  G_CALLBACK (show_info_cb),
			  self);
			  
	g_signal_connect (self->priv->info_window,
			  "hide",
			  G_CALLBACK (hide_info_cb),
			  self);

	self->priv->triggers = NULL;
	self->priv->prov_trig = NULL;
}

static void
trigger_activate_cb (GtkSourceCompletionTrigger *trigger,
		     GtkSourceCompletion *self)
{
	GList *l;
	GList *data_list;
	GList *final_list = NULL;
	GtkSourceCompletionProposal *last_proposal = NULL;
	gint x, y;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER (trigger));
	
	/*
	 * If the completion is visble and there is a trigger active, you cannot
	 * raise a different trigger until the current trigger finish o_O
	 */
	if (GTK_WIDGET_VISIBLE (self)
	    && self->priv->active_trigger != trigger)
	{
		return;
	}
	
	end_completion (self);
	gtk_source_completion_clear (self);
	
	for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
	{
		PTPair *ptp = (PTPair *)l->data;
		
		if (ptp->trigger == trigger)
		{
			data_list = gtk_source_completion_provider_get_proposals (ptp->provider,
										  trigger);
			if (data_list != NULL)
			{
				final_list = g_list_concat (final_list,
							    data_list);
			}
		}
	}
	
	if (final_list == NULL)
	{
		if (GTK_WIDGET_VISIBLE (self))
			end_completion (self);
		return;
	}
	
	data_list = final_list;
	/* Insert the data into the model */
	do
	{
		GtkSourceCompletionPage *page;
		
		last_proposal = GTK_SOURCE_COMPLETION_PROPOSAL (data_list->data);
		page = get_page_by_name (self,
					 gtk_source_completion_proposal_get_page_name (last_proposal));
		
		add_proposal (page,
			      last_proposal);
	} while ((data_list = g_list_next (data_list)) != NULL);
	
	g_list_free (final_list);
	
	if (!GTK_WIDGET_HAS_FOCUS (self->priv->view))
		return;
	
	/*
	 *FIXME Do it supports only cursor position? We can 
	 *add a new "position-type": cursor, center_screen,
	 *center_window, custom etc.
	 */
	gtk_source_completion_utils_get_pos_at_cursor (GTK_WINDOW (self),
						       self->priv->view,
						       &x, &y, NULL);

	gtk_window_move (GTK_WINDOW (self),
			 x, y);
	
	/*
	* We must call to gtk_widget_show and not to gsc_completion_show_or_update
	* because if we don't call to gtk_widget_show, the show signal is not emitted
	*/
	gtk_widget_show (GTK_WIDGET (self));

	gtk_widget_grab_focus (GTK_WIDGET (self->priv->view));

	self->priv->active_trigger = trigger;
	
	if (self->priv->select_on_show)
		select_first_proposal (self->priv->active_page);
}

/**
 * _gtk_source_completion_new:
 *
 * Returns: The new #GtkSourceCompletion
 */
GtkSourceCompletion *
_gtk_source_completion_new (GtkTextView *view)
{
	GtkSourceCompletion *self = GTK_SOURCE_COMPLETION (g_object_new (GTK_TYPE_SOURCE_COMPLETION,
									 "type", GTK_WINDOW_POPUP,
									 NULL));
	self->priv->view = view;
	
	self->priv->signals_ids[TEXT_VIEW_FOCUS_OUT] = 
		g_signal_connect (self->priv->view,
				  "focus-out-event",
				  G_CALLBACK (view_focus_out_event_cb),
				  self);
	
	self->priv->signals_ids[TEXT_VIEW_BUTTON_PRESS] =
		g_signal_connect (self->priv->view,
				  "button-press-event",
				  G_CALLBACK (view_button_press_event_cb),
				  self);
	
	set_manage_keys (self);
	
	return self;
}

/**
 * gtk_source_completion_add_trigger:
 * @self: The #GtkSourceCompletion
 * @trigger: The trigger to register
 *
 * This function register a completion trigger. If the completion is actived
 * then this method activate the trigger. This function reference the trigger
 * object
 *
 * Returns: %TRUE if the trigger has been registered
 */
gboolean
gtk_source_completion_add_trigger (GtkSourceCompletion *self,
				   GtkSourceCompletionTrigger *trigger)
{
	GList *l;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), FALSE);
	
	l = g_list_find (self->priv->triggers, trigger);
	/*Only register the trigger if it has not been registered yet*/
	if (l == NULL)
	{
	
		self->priv->triggers = g_list_append (self->priv->triggers,
						      trigger);
		g_object_ref (trigger);
		
		g_signal_connect (trigger, "activate",
				  G_CALLBACK (trigger_activate_cb),
				  self);
		
		return TRUE;
	}
	
	return FALSE;
}

/**
 * gtk_source_completion_add_provider:
 * @self: the #GtkSourceCompletion
 * @provider: The #GtkSourceCompletionProvider.
 * @trigger: The trigger name what you want to register this provider
 *
 * This function register the provider into the completion and reference it. When 
 * an event is raised, completion call to the provider to get the data. When the user
 * select a proposal, it call the provider to tell it this action and the provider do
 * that it want (normally inserts some text)
 * 
 * Returns: %TRUE if it was registered or %FALSE if not (because it has been already registered,
 * or the trigger doesn't exists)
 **/
gboolean
gtk_source_completion_add_provider (GtkSourceCompletion *self,
				    GtkSourceCompletionProvider *provider,
				    GtkSourceCompletionTrigger *trigger)
{
	PTPair *ptp;
	GList *l;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER (trigger), FALSE);
	g_return_val_if_fail (g_list_find (self->priv->triggers, trigger) != NULL,
			      FALSE);

	/*Check if the provider-trigger pair has been registered*/
	for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
	{
		ptp = (PTPair *)l->data;
		if (ptp->trigger == trigger && ptp->provider == provider)
			return FALSE;
	}

	ptp = g_slice_new (PTPair);
	ptp->provider = provider;
	ptp->trigger = trigger;
	self->priv->prov_trig = g_list_append (self->priv->prov_trig, ptp);
	
	g_object_ref (provider);

	return TRUE;
}

/**
 * gtk_source_completion_remove_trigger:
 * @self: The #GtkSourceCompletion
 * @trigger: The trigger to unregister
 *
 * This function unregister a completion trigger. If the completion is actived
 * then this method deactivate the trigger. This function unreference the trigger
 * object
 *
 * Returns: %TRUE if the trigger has been unregistered or %FALSE if not (because
 * the trigger has not been registered)
 */
gboolean
gtk_source_completion_remove_trigger (GtkSourceCompletion *self,
				      GtkSourceCompletionTrigger *trigger)
{
	GList *l;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), FALSE);
	g_return_val_if_fail (g_list_find (self->priv->triggers, trigger) != NULL,
			      FALSE);
	
	self->priv->triggers = g_list_remove (self->priv->triggers, trigger);
	
	for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
	{
		PTPair *ptp = (PTPair *)l->data;
		
		if (ptp->trigger == trigger)
		{
			free_pair (ptp);
			self->priv->prov_trig = g_list_remove_link (self->priv->prov_trig, l);
			//TODO we need start the list again
		}
	}

	g_signal_handlers_disconnect_by_func (trigger, trigger_activate_cb,
					      self);
	g_object_unref (trigger);
	
	return TRUE;
}

/**
 * gtk_source_completion_remove_provider:
 * @self: the #GtkSourceCompletion
 * @provider: The #GtkSourceCompletionProvider.
 * @trigger: The trigger what you want to unregister this provider
 *
 * This function unregister the provider.
 * 
 * Returns: %TRUE if it was unregistered or %FALSE if not (because it doesn't exists,
 * or the trigger don't exists)
 **/
gboolean
gtk_source_completion_remove_provider (GtkSourceCompletion *self,
				       GtkSourceCompletionProvider *provider,
				       GtkSourceCompletionTrigger *trigger)
{
	GList *l;
	gboolean ret = FALSE;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), FALSE);
	g_return_val_if_fail (g_list_find (self->priv->triggers, trigger) != NULL,
			      FALSE);
	
	for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
	{
		PTPair *ptp = (PTPair *)l->data;
		
		if (ptp->provider == provider)
		{
			free_pair (ptp);
			self->priv->prov_trig = g_list_remove_link (self->priv->prov_trig, l);
			//TODO we need start the list again
			ret = TRUE;
		}
	}
		
	return ret;
}

/**
 * gtk_source_completion_get_active_trigger:
 * @self: The #GtkSourceCompletion
 *
 * This function return the active trigger. The active trigger is the last
 * trigger raised if the completion is active. If the completion is not visible then
 * there is no an active trigger.
 *
 * Returns: The trigger or NULL if completion is not active
 */
GtkSourceCompletionTrigger*
gtk_source_completion_get_active_trigger (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), NULL);

	return self->priv->active_trigger;
}


/**
 * gtk_source_completion_get_view:
 * @self: The #GtkSourceCompletion
 *
 * Returns: The view associated with this completion.
 */
GtkTextView*
gtk_source_completion_get_view (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_SOURCE_COMPLETION (self), NULL);
	
	return self->priv->view;
}

/**
 * gtk_source_completion_finish_completion:
 * @self: The #GtkSourceCompletion
 *
 * This function finish the completion if it is active (visible).
 */
void
gtk_source_completion_finish_completion (GtkSourceCompletion *self)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));

	if (GTK_WIDGET_VISIBLE (self))
	{
		end_completion (self);
	}
}

/**
 * gtk_source_completion_filter_proposals:
 * @self: the #GtkSourceCompletion
 * @func: function to filter the proposals visibility
 * @user_data: user data to pass to func
 *
 * This function call to @func for all proposal of all pages. @func must
 * return %TRUE if the proposal is visible or %FALSE if the completion must to 
 * hide the proposal.
 * 
 **/
void
gtk_source_completion_filter_proposals (GtkSourceCompletion *self,
					GtkSourceCompletionFilterFunc func,
					gpointer user_data)
{
	GList *l;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));
	g_return_if_fail (func);
	
	if (!GTK_WIDGET_VISIBLE (self))
		return;
	
	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		if (get_num_proposals (page) > 0)
		{
			filter_visible (page,
					(GtkSourceCompletionFilterFunc) func,
					user_data);
		}
	}

	if (!update_pages_visibility (self))
	{
		end_completion (self);
	}
	else
	{
		if (self->priv->select_on_show)
			select_first_proposal (self->priv->active_page);
	}
}

/**
 * gtk_source_completion_get_bottom_bar:
 * @self: The #GtkSourceCompletion
 *
 * The bottom bar is the widgetwith the info button, the page name etc.
 *
 * Returns: The bottom bar widget (it is a #GtkBox by now)
 */
GtkWidget*
gtk_source_completion_get_bottom_bar (GtkSourceCompletion *self)
{
	return self->priv->bottom_bar;
}

/**
 * gtk_source_completion_get_info_widget:
 * @self: The #GtkSourceCompletion
 *
 * The info widget is the window where the completion shows the
 * proposal info or help. DO NOT USE IT (only to connect to some signal
 * or set the content in a custom widget).
 *
 * Returns: The internal #GtkSourceCompletionInfo widget.
 */
GtkSourceCompletionInfo *
gtk_source_completion_get_info_widget (GtkSourceCompletion *self)
{
	return GTK_SOURCE_COMPLETION_INFO (self->priv->info_window);
}

/**
 * gtk_source_completion_get_trigger:
 * @self: The #GtkSourceCompletion
 * @trigger_name: The trigger name to find
 *
 * Returns: The #GtkSourceCompletionTrigger registered with @trigger_name, %NULL if it cannot
 * be found
 */
GtkSourceCompletionTrigger*
gtk_source_completion_get_trigger (GtkSourceCompletion *self,
				   const gchar *trigger_name)
{

	GList *l;
	GtkSourceCompletionTrigger *trigger;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), NULL);

	for (l = self->priv->triggers; l != NULL; l = g_list_next (l))
	{
		trigger =  GTK_SOURCE_COMPLETION_TRIGGER (l->data);
		
		if (g_strcmp0 (gtk_source_completion_trigger_get_name (trigger), trigger_name) == 0)
			return trigger;
	}
	
	return NULL;
}

/**
 * gtk_source_completion_get_provider:
 * @self: The #GtkSourceCompletion
 * @prov_name: The provider name to find
 *
 * Returns: The #GtkSourceCompletionProvider registered with @prov_name
 */
GtkSourceCompletionProvider*
gtk_source_completion_get_provider (GtkSourceCompletion *self,
				    const gchar *prov_name)
{

	GList *l;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), NULL);

	for (l = self->priv->prov_trig; l != NULL; l = g_list_next (l))
	{
		PTPair *ptp = (PTPair *)l->data;
		if (g_strcmp0 (gtk_source_completion_provider_get_name (ptp->provider), prov_name) == 0)
			return ptp->provider;
	}
		
	return NULL;
}

/**
 * gtk_source_completion_get_page_pos:
 * @self: The #GtkSourceCompletion
 * @page_name: The page name to search
 *
 * Search the page position with the given name
 *
 * Returns: the page position or -1 if it is not found
 */
gint
gtk_source_completion_get_page_pos (GtkSourceCompletion *self,
				    const gchar *page_name)
{
	GList *l;
	gint pos = 0;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), -1);
	
	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		if (g_strcmp0 (page_name, page->name) == 0)
			return pos;
		
		pos++;
	}
	
	return -1;
}

/**
 * gtk_source_completion_get_n_pages:
 * @self: The #GtkSourceCompletion
 * 
 * Returns: The number of pages
 */
gint
gtk_source_completion_get_n_pages (GtkSourceCompletion *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (self), -1);
	
	return g_list_length (self->priv->pages);
}

/**
 * gtk_source_completion_set_page_pos:
 * @self: The #GtkSourceCompletion
 * @page_name: The page name you want to set the position (stating by 1 because 
 * 0 is the default page)
 * @position: The new position of the page. If this is negative, or is larger
 * than the number of elements in the list, the new element is added on to
 * the end of the list
 * 
 */
void
gtk_source_completion_set_page_pos (GtkSourceCompletion *self,
				    const gchar *page_name,
				    gint position)
{
	GList *l;
	GtkSourceCompletionPage *page;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (self));

	for (l = self->priv->pages; l != NULL; l = g_list_next (l))
	{
		page = (GtkSourceCompletionPage *)l->data;
		if (g_strcmp0 (page_name, page->name) == 0)
		{
			break;
		}
		page = NULL;
	}
	
	if (page == NULL)
	{
		page = gtk_source_completion_page_new (self, page_name);
	}
	
	self->priv->pages = g_list_remove (self->priv->pages, page);
	self->priv->pages = g_list_insert (self->priv->pages, page, position);
	gtk_notebook_reorder_child (GTK_NOTEBOOK (self->priv->notebook),
				    GTK_WIDGET (page->scroll),
				    position);
	if (GTK_WIDGET_VISIBLE (self))
		update_pages_visibility (self);
}


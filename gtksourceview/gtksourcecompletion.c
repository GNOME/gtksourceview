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
						  GtkSourceCompletionPrivate))

enum
{
	COLUMN_PIXBUF,
	COLUMN_NAME,
	COLUMN_PROPOSAL,
	COLUMN_PROVIDER,
	N_COLUMNS
};

/* Signals */
enum
{
	PROPOSAL_ACTIVATED,
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
} ProviderTriggerPair;

struct _GtkSourceCompletionPage
{
	gchar *label;
	
	GtkScrolledWindow *scroll;
	GtkTreeView *treeview;
	GtkListStore *list_store;
	GtkTreeModelFilter *model_filter;
	
	gboolean filter_active;
	gpointer filter_data;
	GtkSourceCompletionFilterFunc filter_func;
};

struct _GtkSourceCompletionPrivate
{
	/* Widget and popup variables*/
	GtkWidget *info_window;
	GtkWidget *info_button;
	GtkWidget *notebook;
	GtkWidget *tab_label;
	GtkWidget *next_page_button;
	GtkWidget *prev_page_button;
	GtkWidget *bottom_bar;
	GtkWidget *default_info;
	
	GList *pages;
	GtkSourceCompletionPage *active_page;
	
	gboolean destroy_has_run;
	gboolean manage_keys;
	gboolean remember_info_visibility;
	gboolean info_visible;
	gboolean select_on_show;
	
	/* Completion management */
	GtkTextView *view;
	GHashTable *triggers;
	GtkSourceCompletionTrigger *active_trigger;
	
	gulong signals_ids[LAST_EXTERNAL_SIGNAL];
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GtkSourceCompletion, gtk_source_completion, GTK_TYPE_WINDOW);

static gboolean
get_selected_proposal (GtkSourceCompletion          *completion,
                       GtkTreeIter                  *iter,
		       GtkSourceCompletionProposal **proposal)
{
	GtkTreeIter piter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkSourceCompletionPage *page;
	
	page = completion->priv->active_page;
	
	selection = gtk_tree_view_get_selection (page->treeview);
	
	if (gtk_tree_selection_get_selected (selection, NULL, &piter))
	{
		model = gtk_tree_view_get_model (page->treeview);
		
		gtk_tree_model_get (model, &piter,
				    COLUMN_PROPOSAL,
				    proposal, -1);
		
		if (iter != NULL)
		{
			*iter = piter;
		}

		return TRUE;
	}
	
	return FALSE;
}

static gboolean
activate_current_proposal (GtkSourceCompletion *completion)
{
	gboolean activated = FALSE;
	GtkSourceCompletionProposal *prop = NULL;
	
	if (get_selected_proposal (completion, NULL, &prop))
	{
		g_signal_emit (G_OBJECT (completion), signals[PROPOSAL_ACTIVATED],
			       0, prop, &activated);
		
		g_object_unref (prop);
	}
	
	return activated;
}

typedef gboolean (*ProposalSelector)(GtkTreeModel *model, GtkTreeIter *iter, gboolean hasselection, gpointer userdata);

static gboolean
select_proposal (GtkSourceCompletion *completion,
                 ProposalSelector     selector,
                 gpointer             userdata)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkSourceCompletionPage *page;
	gboolean hasselection;
	
	page = completion->priv->active_page;

	if (!GTK_WIDGET_VISIBLE (page->treeview))
	{
		return FALSE;
	}
	
	selection = gtk_tree_view_get_selection (page->treeview);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
	{
		return FALSE;
	}

	model = gtk_tree_view_get_model (page->treeview);
	
	hasselection = gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	if (selector (model, &iter, hasselection, userdata))
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
	}
	
	/* Always return TRUE to consume the key press event */
	return TRUE;
}

static gboolean
selector_first (GtkTreeModel *model,
                GtkTreeIter  *iter,
                gboolean      hasselection,
                gpointer      userdata)
{
	return gtk_tree_model_get_iter_first (model, iter);
}

static gboolean
selector_last (GtkTreeModel *model,
               GtkTreeIter  *iter,
               gboolean      hasselection,
               gpointer      userdata)
{
	gint num = gtk_tree_model_iter_n_children (model, NULL);
	return gtk_tree_model_iter_nth_child (model, iter, NULL, num - 1);
}

static gboolean
selector_previous (GtkTreeModel *model,
                   GtkTreeIter  *iter,
                   gboolean      hasselection,
                   gpointer      userdata)
{
	gint num = GPOINTER_TO_INT (userdata);
	GtkTreePath *path;
	gboolean ret = FALSE;
	
	if (!hasselection)
	{
		return selector_last (model, iter, hasselection, userdata);
	}
	
	path = gtk_tree_model_get_path (model, iter);
	
	while (num > 0 && gtk_tree_path_prev (path))
	{
		ret = TRUE;
		--num;
	}
	
	ret = ret && gtk_tree_model_get_iter (model, iter, path);
	gtk_tree_path_free (path);
	
	return ret;
}

static gboolean
selector_next (GtkTreeModel *model,
               GtkTreeIter  *iter,
               gboolean      hasselection,
               gpointer      userdata)
{
	gint num = GPOINTER_TO_INT (userdata);
	gboolean ret = FALSE;
	GtkTreeIter next;

	if (!hasselection)
	{
		return selector_first (model, iter, hasselection, userdata);
	}
	
	next = *iter;

	while (num > 0 && gtk_tree_model_iter_next (model, &next))
	{
		ret = TRUE;
		
		*iter = next;
		--num;
	}
	
	return ret;
}

static gboolean
select_first_proposal (GtkSourceCompletion *completion)
{
	return select_proposal (completion, selector_first, NULL);
}

static gboolean 
select_last_proposal (GtkSourceCompletion *completion)
{
	return select_proposal (completion, selector_last, NULL);
}

static gboolean
select_previous_proposal (GtkSourceCompletion *completion,
			  gint                 rows)
{
	return select_proposal (completion, selector_previous, GINT_TO_POINTER (rows));
}

static gboolean
select_next_proposal (GtkSourceCompletion *completion,
		      gint                 rows)
{
	return select_proposal (completion, selector_next, GINT_TO_POINTER (rows));
}

static void
update_info_position (GtkSourceCompletion *completion)
{
	gint x, y;
	gint width, height;
	gint sw, sh;
	gint info_width;
	GdkScreen *screen;
	
	gtk_window_get_position (GTK_WINDOW (completion), &x, &y);
	gtk_window_get_size (GTK_WINDOW (completion), &width, &height);
	gtk_window_get_size (GTK_WINDOW (completion->priv->info_window), &info_width, NULL);

	screen = gtk_window_get_screen (GTK_WINDOW (completion));
	sw = gdk_screen_get_width (screen);
	sh = gdk_screen_get_height (screen);
	
	/* Determine on which side to place it */
	if (x + width + info_width >= sw)
	{
		x -= info_width;
	}
	else
	{
		x += width;
	}

	gtk_window_move (GTK_WINDOW (completion->priv->info_window), x, y);
}

static gboolean
filter_func (GtkTreeModel *model,
	     GtkTreeIter *iter,
	     gpointer data)
{
	GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)data;
	GtkSourceCompletionProposal *proposal = NULL;
	gboolean ret;
	
	if (!page->filter_active || !page->filter_func)
		return TRUE;
		
	gtk_tree_model_get (model,
			    iter,
			    COLUMN_PROPOSAL,
			    &proposal,
			    -1);
	
	if (proposal == NULL)
		return TRUE;
	
	ret = page->filter_func (proposal, page->filter_data);
	g_object_unref (proposal);
	
	return ret;
}

static void
row_activated_cb (GtkTreeView         *tree_view,
		  GtkTreePath         *path,
		  GtkTreeViewColumn   *column,
		  GtkSourceCompletion *completion)
{
	activate_current_proposal (completion);
}

static void
update_proposal_info_real (GtkSourceCompletion         *completion,
                           GtkSourceCompletionProvider *provider,
                           GtkSourceCompletionProposal *proposal)
{
	GtkWidget *info_widget;
	const gchar *text;
	
	if (proposal == NULL)
	{
		/* Set to default widget */
		info_widget = completion->priv->default_info;
		gtk_label_set_markup (GTK_LABEL (info_widget), "");
	}
	else
	{
		info_widget = gtk_source_completion_provider_get_info_widget (provider, 
		                                                              proposal);

		/* If there is no special custom widget, use the default */
		if (info_widget == NULL)
		{
			info_widget = completion->priv->default_info;
			text = gtk_source_completion_proposal_get_info (proposal);
			
			gtk_label_set_markup (GTK_LABEL (info_widget), text != NULL ? text : "");
		}
	}

	gtk_source_completion_info_set_widget (GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window),
	                                       info_widget);
}

static void
update_proposal_info (GtkSourceCompletion *completion)
{
	GtkSourceCompletionProposal *proposal = NULL;
	GtkSourceCompletionProvider *provider;
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (get_selected_proposal (completion, &iter, &proposal))
	{
		model = gtk_tree_view_get_model (completion->priv->active_page->treeview);
		gtk_tree_model_get (model, &iter, COLUMN_PROVIDER, &provider, -1);
		
		update_proposal_info_real (completion, provider, proposal);
		
		g_object_unref (proposal);
		g_object_unref (provider);
	}
	else
	{
		update_proposal_info_real (completion, NULL, NULL);
	}
}

static void 
selection_changed_cb (GtkTreeSelection    *selection, 
		      GtkSourceCompletion *completion)
{
	if (GTK_WIDGET_VISIBLE (completion->priv->info_window))
	{
		update_proposal_info (completion);
	}
}

static void
add_proposal (GtkSourceCompletion         *completion,
              GtkSourceCompletionProvider *provider,
              GtkSourceCompletionProposal *proposal,
              GtkSourceCompletionPage     *page)
{
	GtkTreeIter iter;

	if (page == NULL || g_list_find (completion->priv->pages, page) == NULL)
	{
		page = (GtkSourceCompletionPage *)completion->priv->pages->data;
	}

	gtk_list_store_append (page->list_store, &iter);
	gtk_list_store_set (page->list_store, 
			    &iter,
			    COLUMN_PIXBUF, gtk_source_completion_proposal_get_icon (proposal),
			    COLUMN_NAME, gtk_source_completion_proposal_get_label (proposal),
			    COLUMN_PROPOSAL, proposal,
			    COLUMN_PROVIDER, provider,
			    -1);

	g_object_unref (proposal);
}

static void
clear_page (GtkSourceCompletionPage *page)
{
	gtk_list_store_clear (page->list_store);
}

static gboolean 
page_has_proposals (GtkSourceCompletionPage *page)
{
	GtkTreeIter iter;
	return gtk_tree_model_iter_children (gtk_tree_view_get_model (page->treeview), 
	                                     &iter, 
	                                     NULL);
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
gtk_source_completion_clear (GtkSourceCompletion *completion)
{
	GList *l;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (completion));
	
	for (l = completion->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		
		clear_page (page);
	}
}

static gboolean
update_pages_visibility (GtkSourceCompletion *completion)
{
	GList *l;
	gboolean first_set = FALSE;
	gboolean more_pages = FALSE;

	gint i = 0;
	GtkAdjustment *ad;
	
	for (l = completion->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		
		if (page_has_proposals (page))
		{
			/* Selects the first page with data */
			if (!first_set)
			{
				gtk_notebook_set_current_page (GTK_NOTEBOOK (completion->priv->notebook),
							       i);
							       
				ad = gtk_tree_view_get_vadjustment (page->treeview);
				gtk_adjustment_set_value (ad, 0);
				
				ad = gtk_tree_view_get_hadjustment (page->treeview);
				gtk_adjustment_set_value (ad, 0);

				first_set = TRUE;
			}
			else
			{
				more_pages = TRUE;
				break;
			}
		}

		i++;
	}
	
	if (more_pages)
	{
		gtk_widget_show (completion->priv->next_page_button);
		gtk_widget_show (completion->priv->prev_page_button);
	}
	else
	{
		gtk_widget_hide (completion->priv->next_page_button);
		gtk_widget_hide (completion->priv->prev_page_button);
	}
	
	return first_set;
}

static void
gtk_source_completion_show (GtkWidget *widget)
{
	gboolean data;
	
	/* Only show the popup, the positions is set before this function */
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (widget);

	/* Call update_pages_visibility selects first page */
	data = update_pages_visibility (completion);

	if (data && !GTK_WIDGET_VISIBLE (completion))
	{
		GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->show (widget);
		
		if (!completion->priv->remember_info_visibility)
			completion->priv->info_visible = FALSE;
		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
					      completion->priv->info_visible);
	}
}

static void
advance_page (GtkSourceCompletion  *completion,
              GList                *(*advance)(GList *),
              GList                *(*cycle)(GList *))
{
	GList *page;
	GList *orig;
	
	orig = g_list_find (completion->priv->pages, completion->priv->active_page);
	page = orig;

	/* Find next page with any proposals in them */
	do
	{
		page = advance (page);
	
		if (!page)
			page = cycle (completion->priv->pages);
	} while (page_has_proposals ((GtkSourceCompletionPage *)page->data) == 0 && page != orig);
	
	if (page != orig)
	{
		gtk_notebook_set_current_page (GTK_NOTEBOOK (completion->priv->notebook),
					       g_list_position (completion->priv->pages, page));

		select_first_proposal (completion);
	}
}

static GList *
list_wrap_next (GList *current)
{
	return g_list_next (current);
}

static GList *
list_wrap_prev (GList *current)
{
	return g_list_previous (current);
}

static void
gtk_source_completion_page_next (GtkSourceCompletion *completion)
{
	advance_page (completion, list_wrap_next, g_list_first);
}

static void
gtk_source_completion_page_previous (GtkSourceCompletion *completion)
{
	advance_page (completion, list_wrap_prev, g_list_last);
}

static GtkSourceCompletionPage *
gtk_source_completion_page_new (GtkSourceCompletion *completion,
				const gchar         *label)
{
	GtkSourceCompletionPage *page;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	
	page = g_slice_new (GtkSourceCompletionPage);

	page->label = g_strdup (label);

	page->treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
	
	gtk_widget_show (GTK_WIDGET (page->treeview));
	
	page->filter_data = NULL;
	page->filter_active = FALSE;
	page->filter_func = NULL;

	g_object_set (page->treeview, "can-focus", FALSE, NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (page->treeview),
					   FALSE);
	
	/* Create the tree */
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", COLUMN_PIXBUF, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_NAME, NULL);
	gtk_tree_view_append_column (page->treeview, column);
	
	/* Create the model */
	page->list_store = gtk_list_store_new (N_COLUMNS,
					       GDK_TYPE_PIXBUF, 
					       G_TYPE_STRING, 
					       G_TYPE_OBJECT,
					       G_TYPE_OBJECT);
	
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
			  completion);
	
	selection = gtk_tree_view_get_selection (page->treeview);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (selection_changed_cb),
			  completion);
	
	page->scroll = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_widget_show (GTK_WIDGET (page->scroll));
	
	gtk_scrolled_window_set_policy (page->scroll,
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_container_add (GTK_CONTAINER (page->scroll),
			   GTK_WIDGET (page->treeview));
	
	return page;
}

static void
info_toggled_cb (GtkToggleButton     *widget,
		 GtkSourceCompletion *completion)
{
	if (gtk_toggle_button_get_active (widget))
	{
		gtk_widget_show (completion->priv->info_window);
	}
	else
	{
		gtk_widget_hide (completion->priv->info_window);
	}
}

static void
next_page_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	gtk_source_completion_page_next (completion);
}

static void
prev_page_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	gtk_source_completion_page_previous (completion);
}

static gboolean
switch_page_cb (GtkNotebook         *notebook, 
		GtkNotebookPage     *n_page,
		gint                 page_num, 
		GtkSourceCompletion *completion)
{
	/* Update the active page */
	completion->priv->active_page = g_list_nth_data (completion->priv->pages, page_num);

	gtk_label_set_label (GTK_LABEL (completion->priv->tab_label),
			     completion->priv->active_page->label);

	update_proposal_info (completion);
	return FALSE;
}

static void
show_info_cb (GtkWidget           *widget,
	      GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_WIDGET_VISIBLE (GTK_WIDGET (completion)));
	
	update_info_position (completion);
	update_proposal_info (completion);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
				      TRUE);
}

static void
show_info_after_cb (GtkWidget           *widget,
	            GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_WIDGET_VISIBLE (GTK_WIDGET (completion)));
	
	/* We do this here because GtkLabel does not properly handle
	 * can-focus = FALSE and selects all the text when it gets focus from
	 * showing the info window for the first time */
	gtk_label_select_region (GTK_LABEL (completion->priv->default_info), 0, 0);
}

static void
hide_info_cb (GtkWidget *widget,
	      gpointer user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
				      FALSE);
}

static gboolean
gtk_source_completion_proposal_activated_default (GtkSourceCompletion         *completion,
						  GtkSourceCompletionProposal *proposal)
{
	gboolean ret;
	
	ret = gtk_source_completion_proposal_activate (proposal, 
	                                               GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (
	                                                                  completion->priv->view)));
	gtk_source_completion_finish (completion);
	return ret;
}

static void
gtk_source_completion_finish_real (GtkSourceCompletion *completion)
{
	gboolean info_visible = GTK_WIDGET_VISIBLE (completion->priv->info_window);
	
	GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->hide (GTK_WIDGET (completion));
	
	/* setting to FALSE, hide the info window */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
				      FALSE);
	
	completion->priv->info_visible = info_visible;
	completion->priv->active_trigger = NULL;
	
	gtk_label_set_markup (GTK_LABEL (completion->priv->default_info), "");
}

static void
gtk_source_completion_hide (GtkWidget *widget)
{
	gtk_source_completion_finish_real (GTK_SOURCE_COMPLETION (widget));	
}

static void
gtk_source_completion_realize (GtkWidget *widget)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (widget);
	
	gtk_container_set_border_width (GTK_CONTAINER (completion), 1);
	gtk_widget_set_size_request (GTK_WIDGET (completion),
				     WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (completion), TRUE);
	
	GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->realize (widget);
}

static void
free_page (GtkSourceCompletionPage *page)
{
	g_free (page->label);
	g_slice_free (GtkSourceCompletionPage, page);
}

static gboolean
gtk_source_completion_configure_event (GtkWidget *widget,
				       GdkEventConfigure *event)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (widget);
	gboolean ret;
	
	ret = GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->configure_event (widget, event);
	
	if (ret && GTK_WIDGET_VISIBLE (completion->priv->info_window))
		update_info_position (completion);
	
	return ret;
}

static gboolean
view_focus_out_event_cb (GtkWidget *widget,
			 GdkEventFocus *event,
			 gpointer user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (completion) && !GTK_WIDGET_HAS_FOCUS (completion))
	{
		gtk_source_completion_finish (completion);
	}
	
	return FALSE;
}

static gboolean
view_button_press_event_cb (GtkWidget      *widget,
			    GdkEventButton *event,
			    gpointer        user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (completion))
	{
		gtk_source_completion_finish (completion);
	}

	return FALSE;
}

static void
gtk_source_completion_finalize (GObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	g_list_foreach (completion->priv->pages, (GFunc)free_page, NULL);
	g_list_free (completion->priv->pages);

	g_hash_table_destroy (completion->priv->triggers);
	
	G_OBJECT_CLASS (gtk_source_completion_parent_class)->finalize (object);
}

static gboolean
remove_from_hash (GtkSourceCompletionTrigger *trigger,
                  GList                      *providers)
{
	g_list_foreach (providers, (GFunc)g_object_unref, NULL);
	g_list_free (providers);
	
	g_object_unref (trigger);
	
	return TRUE;
}

static void
gtk_source_completion_destroy (GtkObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	if (!completion->priv->destroy_has_run)
	{
		gtk_source_completion_clear (completion);
		g_hash_table_foreach_remove (completion->priv->triggers, (GHRFunc)remove_from_hash, NULL);
	
		g_signal_handler_disconnect (completion->priv->view,
				     completion->priv->signals_ids[TEXT_VIEW_FOCUS_OUT]);
		g_signal_handler_disconnect (completion->priv->view,
					     completion->priv->signals_ids[TEXT_VIEW_BUTTON_PRESS]);

		g_object_unref (completion->priv->view);

		completion->priv->destroy_has_run = TRUE;
	}
	GTK_OBJECT_CLASS (gtk_source_completion_parent_class)->destroy (object);
}

static gboolean
view_key_press_event_cb (GtkWidget *view,
			 GdkEventKey *event, 
			 gpointer user_data)
{
	gboolean ret = FALSE;
	GtkSourceCompletion *completion;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (user_data), FALSE);
	
	completion = GTK_SOURCE_COMPLETION (user_data);
	
	if (!GTK_WIDGET_VISIBLE (completion))
		return FALSE;
	
	switch (event->keyval)
 	{
		case GDK_Escape:
		{
			gtk_widget_hide (GTK_WIDGET (completion));
			ret = TRUE;
			break;
		}
 		case GDK_Down:
		{
			select_next_proposal (completion, 1);
			break;
		}
		case GDK_Page_Down:
		{
			ret = select_next_proposal (completion, 5);
			break;
		}
		case GDK_Up:
		{
			ret = select_previous_proposal (completion, 1);
			if (!ret)
				ret = select_first_proposal (completion);
			break;
		}
		case GDK_Page_Up:
		{
			ret = select_previous_proposal (completion, 5);
			break;
		}
		case GDK_Home:
		{
			ret = select_first_proposal (completion);
			break;
		}
		case GDK_End:
		{
			ret = select_last_proposal (completion);
			break;
		}
		case GDK_Return:
		case GDK_Tab:
		{
			ret = activate_current_proposal (completion);
			gtk_widget_hide (GTK_WIDGET (completion));
			break;
		}
		case GDK_Right:
		{
			gtk_source_completion_page_next (completion);
			ret = TRUE;
			break;
		}
		case GDK_Left:
		{
			gtk_source_completion_page_previous (completion);
			ret = TRUE;
			break;
		}
		case GDK_i:
		{
			if ((event->state & GDK_CONTROL_MASK) != 0)
			{
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
					!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (completion->priv->info_button)));
				ret = TRUE;
			}
		}
	}
	return ret;
}

static void
set_manage_keys (GtkSourceCompletion *completion)
{
	if (completion->priv->view == NULL)
	{
		/* This happens on gobject construction, its ok */
		return;
	}

	if (completion->priv->manage_keys && completion->priv->signals_ids[TEXT_VIEW_KP] == 0)
	{
		completion->priv->signals_ids[TEXT_VIEW_KP] = 
			g_signal_connect (completion->priv->view,
					  "key-press-event",
					  G_CALLBACK (view_key_press_event_cb),
					  completion);
	}
	
	if (!completion->priv->manage_keys && completion->priv->signals_ids[TEXT_VIEW_KP] != 0)
	{
		g_signal_handler_disconnect (completion->priv->view,
					     completion->priv->signals_ids[TEXT_VIEW_KP]);
		completion->priv->signals_ids[TEXT_VIEW_KP] = 0;
	}
}

static void
gtk_source_completion_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	GtkSourceCompletion *completion;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (object));
	
	completion = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
		case PROP_MANAGE_KEYS:
			g_value_set_boolean (value, completion->priv->manage_keys);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			g_value_set_boolean (value, completion->priv->remember_info_visibility);
			break;
		case PROP_SELECT_ON_SHOW:
			g_value_set_boolean (value, completion->priv->select_on_show);
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
	GtkSourceCompletion *completion;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (object));
	
	completion = GTK_SOURCE_COMPLETION (object);

	switch (prop_id)
	{
		case PROP_MANAGE_KEYS:
			completion->priv->manage_keys = g_value_get_boolean (value);
			set_manage_keys (completion);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			completion->priv->remember_info_visibility = g_value_get_boolean (value);
			break;
		case PROP_SELECT_ON_SHOW:
			completion->priv->select_on_show = g_value_get_boolean (value);
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
	
	g_type_class_add_private (klass, sizeof (GtkSourceCompletionPrivate));
	
	object_class->get_property = gtk_source_completion_get_property;
	object_class->set_property = gtk_source_completion_set_property;
	object_class->finalize = gtk_source_completion_finalize;
	
	gtkobject_class->destroy = gtk_source_completion_destroy;
	
	widget_class->show = gtk_source_completion_show;
	widget_class->hide = gtk_source_completion_hide;
	widget_class->realize = gtk_source_completion_realize;
	widget_class->configure_event = gtk_source_completion_configure_event;

	klass->proposal_activated = gtk_source_completion_proposal_activated_default;
	
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
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
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
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
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
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	
	/**
	 * GtkSourceCompletion::proposal-selected:
	 * @completion: The #GtkSourceCompletion who emits the signal
	 * @proposal: The selected #GtkSourceCompletionProposal
	 *
	 * When the user selects a proposal
	 **/
	signals[PROPOSAL_ACTIVATED] =
		g_signal_new ("proposal-activated",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionClass, proposal_activated),
			      g_signal_accumulator_true_handled, 
			      NULL,
			      _gtksourceview_marshal_BOOLEAN__OBJECT, 
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
}

static void
update_transient_for_info (GtkSourceCompletion *completion,
                           GParamSpec          *spec)
{
	gtk_window_set_transient_for (GTK_WINDOW (completion->priv->info_window),
				      gtk_window_get_transient_for (GTK_WINDOW (completion)));

}

static void
initialize_ui (GtkSourceCompletion *completion)
{
	GtkWidget *info_icon;
	GtkWidget *info_button;
	GtkWidget *next_page_icon;
	GtkWidget *prev_page_icon;
	GtkWidget *vbox;

	gtk_window_set_type_hint (GTK_WINDOW (completion), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_focus_on_map (GTK_WINDOW (completion), FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (completion), WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_decorated (GTK_WINDOW (completion), FALSE);

	/* Notebook */
	completion->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (completion->priv->notebook);

	g_object_set (G_OBJECT (completion->priv->notebook), "can-focus", FALSE, NULL);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (completion->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (completion->priv->notebook), FALSE);

	/* Bottom bar */
	completion->priv->bottom_bar = gtk_hbox_new (FALSE, 1);
	gtk_widget_show (completion->priv->bottom_bar);

	/* Info button */
	info_icon = gtk_image_new_from_stock (GTK_STOCK_INFO, GTK_ICON_SIZE_MENU);
	gtk_widget_show (info_icon);
	gtk_widget_set_tooltip_text (info_icon, _("Show Proposal Info"));
	
	info_button = gtk_toggle_button_new ();
	gtk_widget_show (info_button);
	g_object_set (G_OBJECT (info_button), "can-focus", FALSE, NULL);
	
	gtk_button_set_focus_on_click (GTK_BUTTON (info_button), FALSE);
	gtk_container_add (GTK_CONTAINER (info_button), info_icon);
	g_signal_connect (G_OBJECT (info_button),
			  "toggled",
			  G_CALLBACK (info_toggled_cb),
			  completion);

	completion->priv->info_button = info_button;

	gtk_box_pack_start (GTK_BOX (completion->priv->bottom_bar), 
	                    info_button,
	                    FALSE, 
	                    FALSE, 
	                    0);

	/* Next/Previous page buttons */
	next_page_icon = gtk_image_new_from_stock (GTK_STOCK_GO_FORWARD,
						   GTK_ICON_SIZE_MENU);
	gtk_widget_show (next_page_icon);

	prev_page_icon = gtk_image_new_from_stock (GTK_STOCK_GO_BACK,
						   GTK_ICON_SIZE_MENU);
	gtk_widget_show (prev_page_icon);
	
	completion->priv->next_page_button = gtk_button_new ();
	gtk_widget_show (completion->priv->next_page_button);
	g_object_set (G_OBJECT (completion->priv->next_page_button),
		      "can-focus", FALSE,
		      "focus-on-click", FALSE,
		      NULL);

	gtk_widget_set_tooltip_text (completion->priv->next_page_button, _("Next page"));
	gtk_container_add (GTK_CONTAINER (completion->priv->next_page_button), next_page_icon);
	g_signal_connect (G_OBJECT (completion->priv->next_page_button),
			  "clicked",
			  G_CALLBACK (next_page_cb),
			  completion);
	
	completion->priv->prev_page_button = gtk_button_new ();
	gtk_widget_show (completion->priv->prev_page_button);
	g_object_set (G_OBJECT (completion->priv->prev_page_button),
		      "can-focus", FALSE,
		      "focus-on-click", FALSE,
		      NULL);

	gtk_widget_set_tooltip_text (completion->priv->prev_page_button, _("Previous page"));
	gtk_container_add (GTK_CONTAINER (completion->priv->prev_page_button), prev_page_icon);
	g_signal_connect (G_OBJECT (completion->priv->prev_page_button),
			  "clicked",
			  G_CALLBACK (prev_page_cb),
			  completion);

	gtk_box_pack_end (GTK_BOX (completion->priv->bottom_bar),
			  completion->priv->next_page_button,
			  FALSE, FALSE, 1);

	gtk_box_pack_end (GTK_BOX (completion->priv->bottom_bar),
			  completion->priv->prev_page_button,
			  FALSE, FALSE, 1);
	/* Page label */
	completion->priv->tab_label = gtk_label_new (NULL);
	gtk_widget_show (completion->priv->tab_label);
	gtk_box_pack_end (GTK_BOX (completion->priv->bottom_bar),
			  completion->priv->tab_label,
			  FALSE, 
			  TRUE, 
			  10);
	
	/* Main vbox */
	vbox = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (vbox), 
	                    completion->priv->notebook,
	                    TRUE,
	                    TRUE, 
	                    0);

	gtk_box_pack_end (GTK_BOX (vbox), 
	                  completion->priv->bottom_bar,
	                  FALSE, 
	                  FALSE, 
	                  0);

	gtk_container_add (GTK_CONTAINER (completion), vbox);

	/* Info window */
	completion->priv->info_window = GTK_WIDGET (gtk_source_completion_info_new ());
	g_signal_connect (completion, 
	                  "notify::transient-for",
	                  G_CALLBACK (update_transient_for_info),
	                  NULL);

	/* Default info widget */
	completion->priv->default_info = gtk_label_new (NULL);
	
	gtk_misc_set_alignment (GTK_MISC (completion->priv->default_info), 0, 0.5);
	gtk_label_set_selectable (GTK_LABEL (completion->priv->default_info), TRUE);
	gtk_widget_show (completion->priv->default_info);
	
	gtk_source_completion_info_set_widget (GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window), 
	                                       completion->priv->default_info);

	/* Connect signals */
	g_signal_connect (completion->priv->notebook, 
			  "switch-page",
			  G_CALLBACK (switch_page_cb),
			  completion);
			
	g_signal_connect (completion,
			  "delete-event",
			  G_CALLBACK (gtk_widget_hide_on_delete),
			  NULL);

	g_signal_connect (completion->priv->info_window,
			  "before-show",
			  G_CALLBACK (show_info_cb),
			  completion);

	g_signal_connect (completion->priv->info_window,
			  "show",
			  G_CALLBACK (show_info_after_cb),
			  completion);
			  
	g_signal_connect (completion->priv->info_window,
			  "hide",
			  G_CALLBACK (hide_info_cb),
			  completion);
			  
	/* Add default page */
	completion->priv->active_page = gtk_source_completion_add_page (completion, _("Default"));
}

static void
gtk_source_completion_init (GtkSourceCompletion *completion)
{
	completion->priv = GTK_SOURCE_COMPLETION_GET_PRIVATE (completion);

	completion->priv->triggers = g_hash_table_new (g_direct_hash,
	                                               g_direct_equal);

	initialize_ui (completion);
}

static void
add_proposals (GtkSourceCompletion         *completion,
               GtkSourceCompletionProvider *provider,
               GtkSourceCompletionTrigger  *trigger)
{
	GList *proposals;
	GList *item;
	GtkSourceCompletionPage *page;
	GtkSourceCompletionProposal *proposal;

	proposals = gtk_source_completion_provider_get_proposals (provider, trigger);
	
	for (item = proposals; item; item = g_list_next (item))
	{
		if (GTK_IS_SOURCE_COMPLETION_PROPOSAL (item->data))
		{
			proposal = GTK_SOURCE_COMPLETION_PROPOSAL (item->data);
			page = gtk_source_completion_provider_get_page (provider, proposal);
			
			add_proposal (completion, provider, proposal, page);
		}
	}
	
	g_list_free (proposals);
}

static void
trigger_activate_cb (GtkSourceCompletionTrigger *trigger,
		     GtkSourceCompletion        *completion)
{
	GList *l;
	gint x, y;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (completion));
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER (trigger));
	
	/*
	 * If the completion is visble and there is a trigger active, you cannot
	 * raise a different trigger until the current trigger finished
	 */
	if (GTK_WIDGET_VISIBLE (completion) && completion->priv->active_trigger != trigger)
	{
		return;
	}
	
	/* End any ongoing completion */
	gtk_source_completion_finish (completion);
	gtk_source_completion_clear (completion);
	
	/* Collect all the proposals */
	l = g_hash_table_lookup (completion->priv->triggers, trigger);
	
	while (l != NULL)
	{
		add_proposals (completion, GTK_SOURCE_COMPLETION_PROVIDER (l->data), trigger);
		l = g_list_next (l);
	}
	
	if (!GTK_WIDGET_HAS_FOCUS (completion->priv->view))
		return;
	
	/*
	 *FIXME Do it supports only cursor position? We can 
	 *add a new "position-type": cursor, center_screen,
	 *center_window, custom etc.
	 */
	gtk_source_completion_utils_get_pos_at_cursor (GTK_WINDOW (completion),
						       GTK_SOURCE_VIEW (completion->priv->view),
						       &x, &y, NULL);

	gtk_window_move (GTK_WINDOW (completion), x, y);
	
	gtk_widget_show (GTK_WIDGET (completion));
	gtk_widget_grab_focus (GTK_WIDGET (completion->priv->view));

	completion->priv->active_trigger = trigger;
	
	if (completion->priv->select_on_show)
	{
		select_first_proposal (completion);
	}
}

/**
 * _gtk_source_completion_new:
 *
 * Returns: The new #GtkSourceCompletion
 */
GtkSourceCompletion *
_gtk_source_completion_new (GtkTextView *view)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (g_object_new (GTK_TYPE_SOURCE_COMPLETION,
									 "type", GTK_WINDOW_POPUP,
									 NULL));
	completion->priv->view = g_object_ref (view);
	
	completion->priv->signals_ids[TEXT_VIEW_FOCUS_OUT] = 
		g_signal_connect (completion->priv->view,
				  "focus-out-event",
				  G_CALLBACK (view_focus_out_event_cb),
				  completion);
	
	completion->priv->signals_ids[TEXT_VIEW_BUTTON_PRESS] =
		g_signal_connect (completion->priv->view,
				  "button-press-event",
				  G_CALLBACK (view_button_press_event_cb),
				  completion);
	
	set_manage_keys (completion);
	
	return completion;
}

/**
 * gtk_source_completion_add_provider:
 * @completion: the #GtkSourceCompletion
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
gtk_source_completion_add_provider (GtkSourceCompletion         *completion,
				    GtkSourceCompletionProvider *provider,
				    GtkSourceCompletionTrigger  *trigger)
{
	GList *providers = NULL;
	gboolean found;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER (trigger), FALSE);
	
	/* Check if provider/trigger is already registered */
	found = g_hash_table_lookup_extended (completion->priv->triggers, 
	                                      trigger,
	                                      NULL,
	                                      (gpointer *)&providers);

	if (g_list_find (providers, provider) != NULL)
	{
		return FALSE;
	}

	if (!found)
	{
		g_object_ref (trigger);

		g_signal_connect (trigger, 
		                  "activate", 
		                  G_CALLBACK (trigger_activate_cb), 
		                  completion);
	}
	
	providers = g_list_prepend (providers, g_object_ref (provider));
	g_hash_table_insert (completion->priv->triggers, trigger, providers);

	return TRUE;
}

/**
 * gtk_source_completion_remove_provider:
 * @completion: the #GtkSourceCompletion
 * @provider: The #GtkSourceCompletionProvider.
 * @trigger: The trigger what you want to unregister this provider
 *
 * This function unregister the provider.
 * 
 * Returns: %TRUE if @provider was unregistered for @trigger, or %FALSE 
 *          otherwise
 **/
gboolean
gtk_source_completion_remove_provider (GtkSourceCompletion         *completion,
				       GtkSourceCompletionProvider *provider,
				       GtkSourceCompletionTrigger  *trigger)
{
	GList *providers = NULL;
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER (trigger), FALSE);
	
	providers = g_hash_table_lookup (completion->priv->triggers, trigger);
	item = g_list_find (providers, provider);

	if (item != NULL)
	{
		providers = g_list_remove_link (providers, item);
		g_hash_table_insert (completion->priv->triggers, trigger, providers);

		return TRUE;
	}
	else
	{		
		return FALSE;
	}
}

/**
 * gtk_source_completion_get_active_trigger:
 * @completion: The #GtkSourceCompletion
 *
 * This function return the active trigger. The active trigger is the last
 * trigger raised if the completion is active. If the completion is not visible then
 * there is no an active trigger.
 *
 * Returns: The trigger or NULL if completion is not active
 */
GtkSourceCompletionTrigger*
gtk_source_completion_get_active_trigger (GtkSourceCompletion *completion)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);

	return completion->priv->active_trigger;
}


/**
 * gtk_source_completion_get_view:
 * @completion: The #GtkSourceCompletion
 *
 * Returns: The view associated with this completion.
 */
GtkTextView*
gtk_source_completion_get_view (GtkSourceCompletion *completion)
{
	g_return_val_if_fail (GTK_SOURCE_COMPLETION (completion), NULL);
	
	return completion->priv->view;
}

/**
 * gtk_source_completion_finish:
 * @completion: a #GtkSourceCompletion
 * 
 * Finishes the completion if it is active (visible).
 */
void
gtk_source_completion_finish (GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (completion));
	
	/* Hiding the completion window will trigger the actual finish */
	gtk_widget_hide (GTK_WIDGET (completion));
}

/**
 * gtk_source_completion_filter_proposals:
 * @completion: the #GtkSourceCompletion
 * @func: function to filter the proposals visibility
 * @user_data: user data to pass to func
 *
 * This function call to @func for all proposal of all pages. @func must
 * return %TRUE if the proposal is visible or %FALSE if the completion must to 
 * hide the proposal.
 * 
 **/
void
gtk_source_completion_filter_proposals (GtkSourceCompletion *completion,
					GtkSourceCompletionFilterFunc func,
					gpointer user_data)
{
	GList *l;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (completion));
	g_return_if_fail (func);
	
	if (!GTK_WIDGET_VISIBLE (completion))
		return;
	
	for (l = completion->priv->pages; l != NULL; l = g_list_next (l))
	{
		GtkSourceCompletionPage *page = (GtkSourceCompletionPage *)l->data;
		if (page_has_proposals (page))
		{
			filter_visible (page,
					func,
					user_data);
		}
	}

	if (!update_pages_visibility (completion))
	{
		gtk_source_completion_finish (completion);
	}
	else
	{
		if (completion->priv->select_on_show)
			select_first_proposal (completion);
	}
}

/**
 * gtk_source_completion_get_info_widget:
 * @completion: The #GtkSourceCompletion
 *
 * The info widget is the window where the completion shows the
 * proposal info or help. DO NOT USE IT (only to connect to some signal
 * or set the content in a custom widget).
 *
 * Returns: The internal #GtkSourceCompletionInfo widget.
 */
GtkSourceCompletionInfo *
gtk_source_completion_get_info_window (GtkSourceCompletion *completion)
{
	return GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window);
}

GtkSourceCompletionPage *
gtk_source_completion_add_page (GtkSourceCompletion *completion,
                                const gchar         *label)
{
	GtkSourceCompletionPage *page;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);
	g_return_val_if_fail (label != NULL, NULL);
	
	page = gtk_source_completion_page_new (completion, label);

	completion->priv->pages = g_list_append (completion->priv->pages, page);
	
	gtk_notebook_append_page (GTK_NOTEBOOK (completion->priv->notebook),
				  GTK_WIDGET (page->scroll),
				  gtk_label_new (page->label));

	return page;
}

gboolean
gtk_source_completion_remove_page (GtkSourceCompletion     *completion,
                                   GtkSourceCompletionPage *page)
{
	gint idx;
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (page != NULL, FALSE);
	
	item = g_list_find (completion->priv->pages, page);
	
	if (item == NULL || item == completion->priv->pages)
	{
		return FALSE;
	}
	
	idx = g_list_index (completion->priv->pages, item);
	completion->priv->pages = g_list_remove_link (completion->priv->pages, item);
	
	gtk_notebook_remove_page (GTK_NOTEBOOK (completion->priv->notebook),
	                          idx);
	
	free_page (page);
	return TRUE;
}

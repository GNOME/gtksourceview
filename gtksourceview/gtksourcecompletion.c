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
 * TODO
 */

#include <gdk/gdkkeysyms.h> 
#include "gtksourcecompletionutils.h"
#include "gtksourceview-marshal.h"
#include <gtksourceview/gtksourcecompletion.h>
#include "gtksourceview-i18n.h"
#include "gtksourcecompletionmodel.h"
#include <string.h>
#include <gtksourceview/gtksourceview.h>
#include "gtksourcecompletion-private.h"
#include <stdarg.h>

#define WINDOW_WIDTH 350
#define WINDOW_HEIGHT 200

#define GTK_SOURCE_COMPLETION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
						  GTK_TYPE_SOURCE_COMPLETION,           \
						  GtkSourceCompletionPrivate))

/* Signals */
enum
{
	SHOW,
	HIDE,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_VIEW,
	PROP_MANAGE_KEYS,
	PROP_REMEMBER_INFO_VISIBILITY,
	PROP_SELECT_ON_SHOW,
	PROP_SHOW_HEADERS,
	
	PROP_MINIMUM_AUTO_COMPLETE_LENGTH,
	PROP_AUTO_COMPLETE_DELAY
};

enum
{
	TEXT_VIEW_KEY_PRESS,
	TEXT_VIEW_FOCUS_OUT,
	TEXT_VIEW_BUTTON_PRESS,
	TEXT_BUFFER_DELETE_RANGE,
	TEXT_BUFFER_INSERT_TEXT,
	LAST_EXTERNAL_SIGNAL
};

struct _GtkSourceCompletionPrivate
{
	/* Widget and popup variables*/
	GtkWidget *window;
	GtkWidget *info_window;
	GtkWidget *info_button;
	GtkWidget *selection_label;
	GtkWidget *bottom_bar;
	GtkWidget *default_info;
	GtkWidget *selection_image;
	
	GtkWidget *tree_view_proposals;
	GtkSourceCompletionModel *model_proposals;
	
	gboolean destroy_has_run;
	gboolean manage_keys;
	gboolean remember_info_visibility;
	gboolean info_visible;
	gboolean select_on_show;
	gboolean show_headers;
	
	/* Completion management */
	GtkSourceView *view;

	GList *providers;
	GHashTable *capability_map;
	GList *active_providers;
	
	guint show_timed_out_id;
	guint auto_complete_delay;
	
	gint typing_line;
	gint typing_line_offset;
	
	GtkSourceCompletionProvider *filter_provider;
	
	gboolean inserting_data;
	gboolean is_interactive;
	gulong signals_ids[LAST_EXTERNAL_SIGNAL];
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GtkSourceCompletion, gtk_source_completion, G_TYPE_OBJECT);

static gboolean
get_selected_proposal (GtkSourceCompletion          *completion,
                       GtkTreeIter                  *iter,
		       GtkSourceCompletionProposal **proposal)
{
	GtkTreeIter piter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view_proposals));
	
	if (gtk_tree_selection_get_selected (selection, NULL, &piter))
	{
		model = GTK_TREE_MODEL (completion->priv->model_proposals);
		
		gtk_tree_model_get (model, &piter,
				    GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROPOSAL,
				    proposal, -1);
		
		if (iter != NULL)
		{
			*iter = piter;
		}

		return TRUE;
	}
	
	return FALSE;
}

static void
get_iter_at_insert (GtkSourceCompletion *completion,
                    GtkTextIter         *iter)
{
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));
	gtk_text_buffer_get_iter_at_mark (buffer,
	                                  iter,
	                                  gtk_text_buffer_get_insert (buffer));
}

static gboolean
activate_current_proposal (GtkSourceCompletion *completion)
{
	gboolean activated;
	GtkTreeIter iter;
	GtkTextIter titer;
	GtkSourceCompletionProposal *proposal = NULL;
	GtkSourceCompletionProvider *provider = NULL;
	GtkTextBuffer *buffer;
	const gchar *text;
	
	if (!get_selected_proposal (completion, &iter, &proposal))
	{
		gtk_source_completion_hide (completion);
		return TRUE;
	}
	
	gtk_tree_model_get (GTK_TREE_MODEL (completion->priv->model_proposals),
	                    &iter,
	                    GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER, &provider,
	                    -1);
	
	/* Get insert iter */
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));
	get_iter_at_insert (completion, &titer);

	activated = gtk_source_completion_provider_activate_proposal (provider, proposal, &titer);

	if (!activated)
	{
		text = gtk_source_completion_proposal_get_text (proposal);
		gtk_source_completion_utils_replace_current_word (GTK_SOURCE_BUFFER (buffer),
				                                  text ? text : NULL,
				                                  -1);
	}

	g_object_unref (provider);
	g_object_unref (proposal);
	
	gtk_source_completion_hide (completion);

	return TRUE;
}

typedef gboolean (*ProposalSelector)(GtkSourceCompletion *completion,
                                     GtkTreeModel        *model, 
                                     GtkTreeIter         *iter, 
                                     gboolean             hasselection, 
                                     gpointer             userdata);

static gboolean
select_proposal (GtkSourceCompletion *completion,
                 ProposalSelector     selector,
                 gpointer             userdata)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	gboolean hasselection;
	
	if (!GTK_WIDGET_VISIBLE (completion->priv->tree_view_proposals))
	{
		return FALSE;
	}
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (completion->priv->tree_view_proposals));

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_NONE)
	{
		return FALSE;
	}

	model = GTK_TREE_MODEL (completion->priv->model_proposals);
	
	hasselection = gtk_tree_selection_get_selected (selection, NULL, &iter);
	
	if (selector (completion, model, &iter, hasselection, userdata))
	{
		gtk_tree_selection_select_iter (selection, &iter);
		
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (completion->priv->tree_view_proposals),
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

static void
scroll_to_iter (GtkSourceCompletion *completion,
                GtkTreeModel        *model,
                GtkTreeIter         *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (model, iter);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (completion->priv->tree_view_proposals),
				      path, 
				      NULL, 
				      FALSE, 
				      0, 
				      0);
	gtk_tree_path_free (path);
}

static gboolean
selector_first (GtkSourceCompletion *completion,
                GtkTreeModel        *model,
                GtkTreeIter         *iter,
                gboolean             hasselection,
                gpointer             userdata)
{
	gboolean ret;
	gboolean hasfirst;
	GtkTreeIter first;
	
	ret = gtk_tree_model_get_iter_first (model, iter);
	hasfirst = ret;
	first = *iter;
	
	while (ret && gtk_source_completion_model_iter_is_header (
			GTK_SOURCE_COMPLETION_MODEL (model), iter))
	{
		ret = gtk_tree_model_iter_next (model, iter);
	}
	
	if (hasfirst && !ret)
	{
		scroll_to_iter (completion, model, &first);
	}
	
	return ret;
}

static gboolean
selector_last (GtkSourceCompletion *completion,
               GtkTreeModel        *model,
               GtkTreeIter         *iter,
               gboolean             hasselection,
               gpointer             userdata)
{
	gboolean ret;
	gboolean haslast;
	GtkTreeIter last;

	ret = gtk_source_completion_model_iter_last (GTK_SOURCE_COMPLETION_MODEL (model),
	                                             iter);
	
	haslast = ret;
	last = *iter;

	while (ret && gtk_source_completion_model_iter_is_header (
			GTK_SOURCE_COMPLETION_MODEL (model), iter))
	{
		ret = gtk_source_completion_model_iter_previous (GTK_SOURCE_COMPLETION_MODEL (model), 
		                                                 iter);
	}
	
	if (haslast && !ret)
	{
		scroll_to_iter (completion, model, &last);
	}
	
	return ret;
}

static gboolean
selector_previous (GtkSourceCompletion *completion,
                   GtkTreeModel        *model,
                   GtkTreeIter         *iter,
                   gboolean             hasselection,
                   gpointer             userdata)
{
	gint num = GPOINTER_TO_INT (userdata);
	gboolean ret = FALSE;
	GtkTreeIter next;
	GtkTreeIter last;

	if (!hasselection)
	{
		return selector_last (completion, model, iter, hasselection, userdata);
	}
	
	next = *iter;
	last = *iter;

	while (num > 0 && gtk_source_completion_model_iter_previous (
				GTK_SOURCE_COMPLETION_MODEL (model), &next))
	{
		if (!gtk_source_completion_model_iter_is_header (GTK_SOURCE_COMPLETION_MODEL (model),
		                                                 &next))
		{
			ret = TRUE;
			*iter = next;
			--num;
		}

		last = next;
	}
	
	if (!ret)
	{
		scroll_to_iter (completion, model, &last);
	}
	
	return ret;
}

static gboolean
selector_next (GtkSourceCompletion *completion,
               GtkTreeModel        *model,
               GtkTreeIter         *iter,
               gboolean             hasselection,
               gpointer             userdata)
{
	gint num = GPOINTER_TO_INT (userdata);
	gboolean ret = FALSE;
	GtkTreeIter next;
	GtkTreeIter last;
	
	if (!hasselection)
	{
		return selector_first (completion, model, iter, hasselection, userdata);
	}
	
	next = *iter;
	last = *iter;

	while (num > 0 && gtk_tree_model_iter_next (model, &next))
	{
		if (!gtk_source_completion_model_iter_is_header (GTK_SOURCE_COMPLETION_MODEL (model),
		                                                 &next))
		{
			ret = TRUE;
			*iter = next;
			--num;
		}
		
		last = next;
	}
	
	if (!ret)
	{
		scroll_to_iter (completion, model, &last);
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
get_num_visible_providers (GtkSourceCompletion *completion,
                           guint               *num,
                           guint               *current)
{
	GList *item;
	*num = 0;
	*current = 0;
	
	for (item = completion->priv->active_providers; item; item = g_list_next (item))
	{
		if (item->data == completion->priv->filter_provider)
		{
			*current = ++*num;
		}
		else
		{
			/* See if it has anything */
			if (gtk_source_completion_model_n_proposals (completion->priv->model_proposals,
			                                             GTK_SOURCE_COMPLETION_PROVIDER (item->data)))
			{
				++*num;
			}
		}
	}
}

static void
update_selection_label (GtkSourceCompletion *completion)
{
	guint pos;
	guint num;
	gchar *name;
	gchar *tmp;
	
	get_num_visible_providers (completion, &num, &pos);
	
	if (completion->priv->filter_provider == NULL)
	{
		name = g_strdup_printf("[<i>%s</i>]", _("All"));
		
		gtk_image_clear (GTK_IMAGE (completion->priv->selection_image));
	}
	else
	{
		name = g_markup_escape_text (
			gtk_source_completion_provider_get_name (completion->priv->filter_provider),
								 -1);

		gtk_image_set_from_pixbuf (GTK_IMAGE (completion->priv->selection_image),
                           (GdkPixbuf *)gtk_source_completion_provider_get_icon (completion->priv->filter_provider));
	}
	
	if (num > 1)
	{
		tmp = g_strdup_printf ("%s (%d/%d)", name, pos + 1, num + 1);
		gtk_label_set_markup (GTK_LABEL (completion->priv->selection_label),
		                      tmp);
		g_free (tmp);
	}
	else
	{
		gtk_label_set_markup (GTK_LABEL (completion->priv->selection_label),
		                      name);
	}
	
	g_free (name);
}

typedef GList * (*ListSelector)(GList *);

static gboolean
select_provider (GtkSourceCompletion *completion,
                 ListSelector         advance,
                 ListSelector         cycle_first,
                 ListSelector         cycle_last)
{
	GList *first;
	GList *last;
	GList *orig;
	GList *current;
	GtkSourceCompletionProvider *provider;
	guint num;
	guint pos;
	
	/* If there is only one provider, then there is no other selection */
	if (completion->priv->active_providers->next == NULL)
	{
		return FALSE;
	}
	
	get_num_visible_providers (completion, &num, &pos);
	
	if (num <= 1)
	{
		if (completion->priv->filter_provider != NULL)
		{
			completion->priv->filter_provider = NULL;
			
			update_selection_label (completion);
			//do_refilter (completion, FALSE);
			
			return TRUE;
		}

		return FALSE;
	}

	if (completion->priv->filter_provider != NULL)
	{
		orig = g_list_find (completion->priv->active_providers,
		                    completion->priv->filter_provider);
	}
	else
	{
		orig = NULL;
	}
	
	first = cycle_first (completion->priv->active_providers);
	last = cycle_last (completion->priv->active_providers);
	current = orig;
	
	do
	{
		if (current == NULL)
		{
			current = first;
		}
		else if (current == last)
		{
			current = NULL;
		}
		else
		{
			current = advance (current);
		}
		
		if (current != NULL)
		{
			provider = GTK_SOURCE_COMPLETION_PROVIDER (current->data);
	
			if (gtk_source_completion_model_n_proposals (completion->priv->model_proposals,
			                                             provider) != 0)
			{
				break;
			}
		}
		else if (!gtk_source_completion_model_is_empty (completion->priv->model_proposals, TRUE))
		{
			break;
		}
	} while (orig != current);
	
	if (orig == current)
	{
		return FALSE;
	}
	
	if (current != NULL)
	{
		completion->priv->filter_provider = current->data;
	}
	else
	{
		completion->priv->filter_provider = NULL;
	}
	
	update_selection_label (completion);
	//do_refilter (completion, FALSE);
	
	return TRUE;
}

static GList *
wrap_g_list_next (GList *list)
{
	return g_list_next (list);
}

static GList *
wrap_g_list_previous (GList *list)
{
	return g_list_previous (list);
}

static gboolean
select_next_provider (GtkSourceCompletion *completion)
{
	return select_provider (completion, wrap_g_list_next, g_list_first, g_list_last);
}

static gboolean
select_previous_provider (GtkSourceCompletion *completion)
{
	return select_provider (completion, wrap_g_list_previous, g_list_last, g_list_first);
}

static void
update_info_position (GtkSourceCompletion *completion)
{
	gint x, y;
	gint width, height;
	gint sw, sh;
	gint info_width;
	GdkScreen *screen;
	
	gtk_window_get_position (GTK_WINDOW (completion->priv->window), &x, &y);
	gtk_window_get_size (GTK_WINDOW (completion->priv->window),
			     &width, &height);
	gtk_window_get_size (GTK_WINDOW (completion->priv->info_window), &info_width, NULL);

	screen = gtk_window_get_screen (GTK_WINDOW (completion->priv->window));
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
	gboolean prov_update_info = FALSE;
	GtkSourceCompletionInfo *info_window;
	
	info_window = GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window);
	
	gtk_source_completion_info_set_sizing (info_window,
	                                       -1, -1, TRUE, TRUE);

	if (proposal == NULL)
	{
		/* Set to default widget */
		info_widget = completion->priv->default_info;
		gtk_label_set_markup (GTK_LABEL (info_widget), _("No extra information available"));
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
			
			gtk_label_set_markup (GTK_LABEL (info_widget), text != NULL ? text : _("No extra information available"));
		}
		else
		{
			prov_update_info = TRUE;
		}
	}
	
	gtk_source_completion_info_set_widget (info_window,
	                                       info_widget);

	if (prov_update_info)
	{
		gtk_source_completion_provider_update_info (provider, 
			                                    proposal,
			                                    info_window);
	}
	
	gtk_source_completion_info_process_resize (info_window);
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
		model = GTK_TREE_MODEL (completion->priv->model_proposals);
		gtk_tree_model_get (model, &iter, GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER, &provider, -1);
		
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
show_info_cb (GtkWidget           *widget,
	      GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_WIDGET_VISIBLE (GTK_WIDGET (completion->priv->window)));
	
	update_info_position (completion);
	update_proposal_info (completion);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
				      TRUE);
}

static void
show_info_after_cb (GtkWidget           *widget,
	            GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_WIDGET_VISIBLE (GTK_WIDGET (completion->priv->window)));
	
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

static void
info_size_allocate_cb (GtkWidget           *widget,
                       GtkAllocation       *allocation,
                       GtkSourceCompletion *completion)
{
	/* Update window position */
	update_info_position (completion);
}

static void
gtk_source_completion_realize (GtkWidget           *widget,
                               GtkSourceCompletion *completion)
{
	gtk_container_set_border_width (GTK_CONTAINER (completion->priv->window), 1);
	gtk_widget_set_size_request (GTK_WIDGET (completion->priv->window),
				     WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_resizable (GTK_WINDOW (completion->priv->window), TRUE);
}

static gboolean
gtk_source_completion_configure_event (GtkWidget           *widget,
                                       GdkEventConfigure   *event,
                                       GtkSourceCompletion *completion)
{
	if (GTK_WIDGET_VISIBLE (completion->priv->info_window))
		update_info_position (completion);
	
	return FALSE;
}

static gboolean
view_focus_out_event_cb (GtkWidget     *widget,
                         GdkEventFocus *event,
                         gpointer       user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (completion->priv->window)
	    && !GTK_WIDGET_HAS_FOCUS (completion->priv->window))
	{
		gtk_source_completion_hide (completion);
	}
	
	return FALSE;
}

static gboolean
view_button_press_event_cb (GtkWidget      *widget,
			    GdkEventButton *event,
			    gpointer        user_data)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (user_data);
	
	if (GTK_WIDGET_VISIBLE (completion->priv->window))
	{
		gtk_source_completion_hide (completion);
	}

	return FALSE;
}

static gboolean
view_key_press_event_cb (GtkSourceView       *view,
			 GdkEventKey         *event, 
			 GtkSourceCompletion *completion)
{
	gboolean ret = FALSE;
	GdkModifierType mod;
	
	mod = gtk_accelerator_get_default_mod_mask () & event->state;
	
	if (!GTK_WIDGET_VISIBLE (completion->priv->window)
	    || !completion->priv->manage_keys)
	{
		return FALSE;
	}
	
	switch (event->keyval)
 	{
		case GDK_Escape:
		{
			gtk_source_completion_hide (completion);
			ret = TRUE;
			break;
		}
 		case GDK_Down:
		{
			ret = select_next_proposal (completion, 1);
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
			gtk_source_completion_hide (completion);
			break;
		}
		case GDK_i:
		{
			if (mod == GDK_CONTROL_MASK)
			{
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
					!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (completion->priv->info_button)));
				ret = TRUE;
			}
			break;
		}
		case GDK_Left:
		{
			if (mod == GDK_CONTROL_MASK)
			{
				ret = select_previous_provider (completion);
			}
			break;
		}
		case GDK_Right:
		{
			if (mod == GDK_CONTROL_MASK)
			{
				ret = select_next_provider (completion);
			}
			break;
		}
	}
	return ret;
}

static void
update_typing_offsets (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	gchar *word;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));
	word = gtk_source_completion_utils_get_word_iter (GTK_SOURCE_BUFFER (buffer),
							  NULL,
							  &start,
							  &end);
	g_free (word);

	completion->priv->typing_line = gtk_text_iter_get_line (&start);
	completion->priv->typing_line_offset = gtk_text_iter_get_line_offset (&start);
}

static gboolean
show_auto_completion (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter start;
	GtkTextIter end;
	gchar *word;
	GList *providers;
	
	completion->priv->show_timed_out_id = 0;
	
	providers = g_hash_table_lookup (completion->priv->capability_map, 
	                                 GTK_SOURCE_COMPLETION_CAPABILITY_INTERACTIVE);
	
	if (!providers)
	{
		return FALSE;
	}

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));

	/* Check if the user has changed the cursor position.If yes, we don't complete */
	get_iter_at_insert (completion, &iter);
	
	if ((gtk_text_iter_get_line (&iter) != completion->priv->typing_line))
	{
		return FALSE;
	}
	
	word = gtk_source_completion_utils_get_word_iter (GTK_SOURCE_BUFFER (buffer),
	                                                  &iter,
							  &start,
							  &end);
	
	/* Check minimum amount of characters */
	if (g_utf8_strlen (word, -1) >= 1)
	{
		gtk_source_completion_show (completion, providers, &start);
		completion->priv->is_interactive = TRUE;
	}

	g_free (word);
	
	return FALSE;
}

static void
interactive_do_show (GtkSourceCompletion *completion)
{
	update_typing_offsets (completion);

	if (completion->priv->show_timed_out_id != 0)
	{
		g_source_remove (completion->priv->show_timed_out_id);
		completion->priv->show_timed_out_id = 0;
	}

	completion->priv->show_timed_out_id = 
		g_timeout_add (completion->priv->auto_complete_delay,
			       (GSourceFunc)show_auto_completion,
			       completion);
}

static gboolean
buffer_delete_range_cb (GtkTextBuffer       *buffer,
                        GtkTextIter         *start,
                        GtkTextIter         *end,
                        GtkSourceCompletion *completion)
{
	if (!GTK_WIDGET_VISIBLE (completion->priv->window))
	{
		interactive_do_show (completion);
	}
	else
	{
		if (gtk_text_iter_get_line (start) != completion->priv->typing_line ||
		    (completion->priv->is_interactive && 
		     gtk_text_iter_get_line_offset (start) < completion->priv->typing_line_offset + 1))
		{
			gtk_source_completion_hide (completion);
		}
		else
		{
			//FIXME create the context, remove items, and add new ones
			//refilter_proposals_with_word (completion);
		}
	}
	
	return FALSE;
}

static void
buffer_insert_text_cb (GtkTextBuffer       *buffer,
                       GtkTextIter         *location,
                       gchar               *text,
                       gint                 len,
                       GtkSourceCompletion *completion)
{
	/* Only handle typed text */
	if (len > 1 && completion->priv->is_interactive)
	{
		gtk_source_completion_hide (completion);
		return;
	}
	
	if (!GTK_WIDGET_VISIBLE (completion->priv->window))
	{
		interactive_do_show (completion);
	}
	else
	{
		if ((completion->priv->is_interactive && 
		     gtk_source_completion_utils_is_separator (g_utf8_get_char (text))) ||
		    gtk_text_iter_get_line (location) != completion->priv->typing_line)
		{
			gtk_source_completion_hide (completion);
		}
		else
		{
			//FIXME create the context, remove items, and add new ones
			//refilter_proposals_with_word (completion);
		}
	}
}

static void
disconnect_view (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;
	
	g_signal_handler_disconnect (completion->priv->view,
	                             completion->priv->signals_ids[TEXT_VIEW_FOCUS_OUT]);

	g_signal_handler_disconnect (completion->priv->view,
	                             completion->priv->signals_ids[TEXT_VIEW_BUTTON_PRESS]);

	g_signal_handler_disconnect (completion->priv->view,
	                             completion->priv->signals_ids[TEXT_VIEW_KEY_PRESS]);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));

	g_signal_handler_disconnect (buffer,
	                             completion->priv->signals_ids[TEXT_BUFFER_DELETE_RANGE]);

	g_signal_handler_disconnect (buffer,
	                             completion->priv->signals_ids[TEXT_BUFFER_INSERT_TEXT]);
}

static void
connect_view (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;

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

	completion->priv->signals_ids[TEXT_VIEW_KEY_PRESS] = 
		g_signal_connect (completion->priv->view,
				  "key-press-event",
				  G_CALLBACK (view_key_press_event_cb),
				  completion);

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));

	completion->priv->signals_ids[TEXT_BUFFER_DELETE_RANGE] =
		g_signal_connect_after (buffer,
					"delete-range",
					G_CALLBACK (buffer_delete_range_cb),
					completion);

	completion->priv->signals_ids[TEXT_BUFFER_INSERT_TEXT] =
		g_signal_connect_after (buffer,
					"insert-text",
					G_CALLBACK (buffer_insert_text_cb),
					completion);
}

static void
gtk_source_completion_dispose (GObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	if (completion->priv->view != NULL)
	{
		disconnect_view (completion);
		g_object_unref (completion->priv->view);
		
		completion->priv->view = NULL;
		
		g_list_foreach (completion->priv->providers, (GFunc)g_object_unref, NULL);
	}
	
	G_OBJECT_CLASS (gtk_source_completion_parent_class)->dispose (object);
}

static void
gtk_source_completion_finalize (GObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	if (completion->priv->show_timed_out_id != 0)
	{
		g_source_remove (completion->priv->show_timed_out_id);
	}
	
	g_hash_table_destroy (completion->priv->capability_map);
	g_list_free (completion->priv->providers);
	
	G_OBJECT_CLASS (gtk_source_completion_parent_class)->finalize (object);
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
		case PROP_VIEW:
			g_value_set_object (value, completion->priv->view);
			break;
		case PROP_MANAGE_KEYS:
			g_value_set_boolean (value, completion->priv->manage_keys);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			g_value_set_boolean (value, completion->priv->remember_info_visibility);
			break;
		case PROP_SELECT_ON_SHOW:
			g_value_set_boolean (value, completion->priv->select_on_show);
			break;
		case PROP_SHOW_HEADERS:
			g_value_set_boolean (value, completion->priv->show_headers);
			break;
		case PROP_AUTO_COMPLETE_DELAY:
			g_value_set_uint (value, completion->priv->auto_complete_delay);
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
		case PROP_VIEW:
			/* On construction only */
			completion->priv->view = g_value_dup_object (value);
			connect_view (completion);
			break;
		case PROP_MANAGE_KEYS:
			completion->priv->manage_keys = g_value_get_boolean (value);
			break;
		case PROP_REMEMBER_INFO_VISIBILITY:
			completion->priv->remember_info_visibility = g_value_get_boolean (value);
			break;
		case PROP_SELECT_ON_SHOW:
			completion->priv->select_on_show = g_value_get_boolean (value);
			break;
		case PROP_SHOW_HEADERS:
			completion->priv->show_headers = g_value_get_boolean (value);
			
			if (completion->priv->model_proposals != NULL)
			{
				gtk_source_completion_model_set_show_headers (completion->priv->model_proposals,
				                                              completion->priv->show_headers);
			}
			break;
		case PROP_AUTO_COMPLETE_DELAY:
			completion->priv->auto_complete_delay = g_value_get_uint (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_hide_default (GtkSourceCompletion *completion)
{
	completion->priv->filter_provider = NULL;

	gtk_label_set_markup (GTK_LABEL (completion->priv->default_info), "");

	gtk_source_completion_model_clear (completion->priv->model_proposals);

	g_list_free (completion->priv->active_providers);
	completion->priv->active_providers = NULL;
	
	completion->priv->info_visible = GTK_WIDGET_VISIBLE (completion->priv->info_window);
	
	gtk_widget_hide (completion->priv->info_window);
	gtk_widget_hide (completion->priv->window);
}

static void
gtk_source_completion_show_default (GtkSourceCompletion *completion)
{
	gtk_widget_show (GTK_WIDGET (completion->priv->window));
	gtk_widget_grab_focus (GTK_WIDGET (completion->priv->view));

	if (completion->priv->select_on_show)
	{
		select_first_proposal (completion);
	}
}

static void
gtk_source_completion_class_init (GtkSourceCompletionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (GtkSourceCompletionPrivate));
	
	object_class->get_property = gtk_source_completion_get_property;
	object_class->set_property = gtk_source_completion_set_property;
	object_class->finalize = gtk_source_completion_finalize;
	object_class->dispose = gtk_source_completion_dispose;

	klass->show = gtk_source_completion_show_default;
	klass->hide = gtk_source_completion_hide_default;

	/**
	 * GtkSourceCompletion:view:
	 *
	 * The #GtkSourceView bound to the completion object.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_VIEW,
					 g_param_spec_object ("view",
							      _("View"),
							      _("The GtkSourceView bound to the completion"),
							      GTK_TYPE_SOURCE_VIEW,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	/**
	 * GtkSourceCompletion:manage-completion-keys:
	 *
	 * Determines whether the completion object should manage key presses
	 * for navigating and activating proposals.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_MANAGE_KEYS,
					 g_param_spec_boolean ("manage-completion-keys",
							      _("Manage Completion Keys"),
							      _("Manage keys to navigate proposal selection"),
							      TRUE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * GtkSourceCompletion:remember-info-visibility:
	 *
	 * Determines whether the visibility of the info window should be 
	 * saved when the completion is hidden, and restored when the completion
	 * is shown again.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_REMEMBER_INFO_VISIBILITY,
					 g_param_spec_boolean ("remember-info-visibility",
							      _("Remeber Info Visibility"),
							      _("Remember the last info window visibility state"),
							      FALSE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * GtkSourceCompletion:select-on-show:
	 *
	 * Determines whether the first proposal should be selected when the 
	 * completion is first shown.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_SELECT_ON_SHOW,
					 g_param_spec_boolean ("select-on-show",
							      _("Select on Show"),
							      _("Select first proposal when completion is shown"),
							      TRUE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceCompletion:show-headers:
	 *
	 * Determines whether provider headers should be shown in the proposal
	 * list if there is more than one provider with proposals.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_SHOW_HEADERS,
					 g_param_spec_boolean ("show-headers",
							      _("Show Headers"),
							      _("Show provider headers when proposals from multiple providers are available"),
							      TRUE,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceCompletion:auto-complete-delay:
	 *
	 * Determines the popup delay (in milliseconds) at which the completion
	 * will be shown for interactive completion.
	 *
	 */
	g_object_class_install_property (object_class,
					 PROP_AUTO_COMPLETE_DELAY,
					 g_param_spec_uint ("auto-complete-delay",
							    _("Auto Complete Delay"),
							    _("Completion popup delay for interactive completion"),
							    0,
							    G_MAXUINT,
							    250,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceCompletion::show:
	 * @completion: The #GtkSourceCompletion who emits the signal
	 *
	 * Emitted when the completion window is shown. The default handler
	 * will actually show the window.
	 *
	 */
	signals[SHOW] =
		g_signal_new ("show",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionClass, show),
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);


	/**
	 * GtkSourceCompletion::hide:
	 * @completion: The #GtkSourceCompletion who emits the signal
	 *
	 * Emitted when the completion window is hidden. The default handler
	 * will actually hide the window.
	 *
	 */
	signals[HIDE] =
		g_signal_new ("hide",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (GtkSourceCompletionClass, hide),
			      NULL, 
			      NULL,
			      g_cclosure_marshal_VOID__VOID, 
			      G_TYPE_NONE,
			      0);
}

static void
update_transient_for_info (GObject             *window,
                           GParamSpec          *spec,
                           GtkSourceCompletion *completion)
{
	gtk_window_set_transient_for (GTK_WINDOW (completion->priv->info_window),
				      gtk_window_get_transient_for (GTK_WINDOW (completion->priv->window)));

}

static void
render_proposal_icon_func (GtkTreeViewColumn   *column,
                           GtkCellRenderer     *cell,
                           GtkTreeModel        *model,
                           GtkTreeIter         *iter,
                           GtkSourceCompletion *completion)
{
	gboolean isheader;
	GdkPixbuf *icon;
	GtkStyle *style;
	
	isheader = gtk_source_completion_model_iter_is_header (completion->priv->model_proposals, 
	                                                       iter);
	
	style = gtk_widget_get_style (GTK_WIDGET (completion->priv->tree_view_proposals));
	
	if (isheader)
	{
		g_object_set (cell, 
		              "cell-background-gdk", &(style->bg[GTK_STATE_INSENSITIVE]), 
		              NULL);
	}
	else
	{
		g_object_set (cell,
		              "cell-background-set", FALSE,
		              NULL);
	}
	
	gtk_tree_model_get (model, 
	                    iter, 
	                    GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON,
	                    &icon,
	                    -1);

	g_object_set (cell, "pixbuf", icon, NULL);
	
	if (icon)
	{
		g_object_unref (icon);
	}
}

static void
render_proposal_text_func (GtkTreeViewColumn   *column,
                           GtkCellRenderer     *cell,
                           GtkTreeModel        *model,
                           GtkTreeIter         *iter,
                           GtkSourceCompletion *completion)
{
	gchar *label;
	gchar *markup;
	GtkSourceCompletionProvider *provider;
	gboolean isheader;
	GtkStyle *style;
	
	isheader = gtk_source_completion_model_iter_is_header (completion->priv->model_proposals, 
		                                               iter);

	if (isheader)
	{
		gtk_tree_model_get (model, 
		                    iter, 
		                    GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER, 
		                    &provider, 
		                    -1);
		
		label = g_strdup_printf ("<b>%s</b>", 
		                        g_markup_escape_text (gtk_source_completion_provider_get_name (provider),
		                                              -1));

		style = gtk_widget_get_style (GTK_WIDGET (completion->priv->tree_view_proposals));

		g_object_set (cell, 
		              "markup", label,
		              "background-gdk", &(style->bg[GTK_STATE_INSENSITIVE]), 
		              "foreground-gdk", &(style->fg[GTK_STATE_INSENSITIVE]), 
		              NULL);
		g_free (label);
		
		g_object_unref (provider);
	}
	else
	{
		gtk_tree_model_get (model, 
		                    iter, 
		                    GTK_SOURCE_COMPLETION_MODEL_COLUMN_LABEL, 
		                    &label, 
		                    GTK_SOURCE_COMPLETION_MODEL_COLUMN_MARKUP, 
		                    &markup,
		                    -1);

		if (!markup)
		{
			markup = g_markup_escape_text (label ? label : "", -1);
		}

		g_object_set (cell, 
		              "markup", markup, 
		              "background-set", FALSE, 
		              "foreground-set", FALSE,
		              NULL);

		g_free (label);
		g_free (markup);
	}
}

static gboolean
selection_func (GtkTreeSelection    *selection,
                GtkTreeModel        *model,
                GtkTreePath         *path,
                gboolean             path_currently_selected,
                GtkSourceCompletion *completion)
{
	GtkTreeIter iter;
	
	gtk_tree_model_get_iter (model, &iter, path);
	
	if (gtk_source_completion_model_iter_is_header (completion->priv->model_proposals,
	                                                &iter))
	{
		return path_currently_selected;
	}
	else
	{
		return TRUE;
	}
}

static void
on_row_inserted_cb (GtkTreeModel *tree_model,
		    GtkTreePath  *path,
		    GtkTreeIter  *iter,
		    GtkSourceCompletion *completion)
{
	if (!GTK_WIDGET_VISIBLE (completion->priv->window))
	{
		update_selection_label (completion);
	
		if (!completion->priv->remember_info_visibility)
			completion->priv->info_visible = FALSE;
		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
					      completion->priv->info_visible);
	
		g_signal_emit (completion, signals[SHOW], 0);
	}
	
	/* FIXME this fixes the header visibility but produces the problem of
	* scrolling */
	/*gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (completion->priv->tree_view_proposals),
	                               0,
	                               0);*/
}

static void
on_items_added_cb (GtkSourceCompletionModel *model,
		   GtkSourceCompletion      *completion)
{
	completion->priv->inserting_data = FALSE;

	/* Check if there are any completions */
	if (gtk_source_completion_model_is_empty (model, FALSE))
	{
		gtk_source_completion_hide (completion);
	}
}

static GtkWidget *
initialize_proposals_ui (GtkSourceCompletion *completion)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkWidget *scrolled_window;
	GtkWidget *tree_view;
	
	completion->priv->model_proposals = gtk_source_completion_model_new ();
	g_warning ("1");
	g_signal_connect (completion->priv->model_proposals,
			  "items-added",
			  G_CALLBACK (on_items_added_cb),
			  completion);

	gtk_source_completion_model_set_show_headers (completion->priv->model_proposals,
				                      completion->priv->show_headers);
	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (completion->priv->model_proposals));
	completion->priv->tree_view_proposals = tree_view;

	g_signal_connect_after (completion->priv->model_proposals,
	                        "row-inserted",
	                        G_CALLBACK (on_row_inserted_cb),
	                        completion);
	
	gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (tree_view), FALSE);
	
	gtk_widget_show (tree_view);
	g_object_set (tree_view, "can-focus", FALSE, NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	
	/* Create the tree */
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_set_cell_data_func (column,
	                                         renderer,
	                                         (GtkTreeCellDataFunc)render_proposal_icon_func,
	                                         completion,
	                                         NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	
	gtk_tree_view_column_set_cell_data_func (column,
	                                         renderer,
	                                         (GtkTreeCellDataFunc)render_proposal_text_func,
	                                         completion,
	                                         NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	g_signal_connect (tree_view,
			  "row-activated",
			  G_CALLBACK (row_activated_cb),
			  completion);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_select_function (selection,
	                                        (GtkTreeSelectionFunc)selection_func,
	                                        completion,
	                                        NULL);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (selection_changed_cb),
			  completion);
	
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled_window);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	
	gtk_container_add (GTK_CONTAINER (scrolled_window),
			   tree_view);
	g_warning ("2");
	return scrolled_window;
}

static void
initialize_ui (GtkSourceCompletion *completion)
{
	GtkWidget *info_icon;
	GtkWidget *info_button;
	GtkWidget *vbox;
	GtkWidget *container;

	/* Window */
	completion->priv->window = gtk_window_new (GTK_WINDOW_POPUP);

	gtk_window_set_type_hint (GTK_WINDOW (completion->priv->window),
				  GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_focus_on_map (GTK_WINDOW (completion->priv->window),
				     FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (completion->priv->window),
				     WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_decorated (GTK_WINDOW (completion->priv->window), FALSE);

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

	/* Selection label */
	completion->priv->selection_label = gtk_label_new (NULL);
	gtk_widget_show (completion->priv->selection_label);
	gtk_box_pack_end (GTK_BOX (completion->priv->bottom_bar),
			  completion->priv->selection_label,
			  FALSE, 
			  TRUE, 
			  10);
			  
	completion->priv->selection_image = gtk_image_new ();
	gtk_widget_show (completion->priv->selection_image);
	gtk_box_pack_end (GTK_BOX (completion->priv->bottom_bar),
	                  completion->priv->selection_image,
	                  FALSE,
	                  TRUE,
	                  0);

	container = initialize_proposals_ui (completion);

	/* Main vbox */
	vbox = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (vbox), 
	                    container,
	                    TRUE,
	                    TRUE, 
	                    0);

	gtk_box_pack_end (GTK_BOX (vbox), 
	                  completion->priv->bottom_bar,
	                  FALSE, 
	                  FALSE, 
	                  0);

	gtk_container_add (GTK_CONTAINER (completion->priv->window), vbox);

	/* Info window */
	completion->priv->info_window = GTK_WIDGET (gtk_source_completion_info_new ());
	                             
	g_signal_connect (completion->priv->window, 
	                  "notify::transient-for",
	                  G_CALLBACK (update_transient_for_info),
	                  completion);

	/* Default info widget */
	completion->priv->default_info = gtk_label_new (NULL);
	
	gtk_misc_set_alignment (GTK_MISC (completion->priv->default_info), 0.5, 0.5);
	gtk_label_set_selectable (GTK_LABEL (completion->priv->default_info), TRUE);
	gtk_widget_show (completion->priv->default_info);
	
	gtk_source_completion_info_set_widget (GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window), 
	                                       completion->priv->default_info);

	/* Connect signals */
	g_signal_connect_after (completion->priv->window,
				"configure-event",
				G_CALLBACK (gtk_source_completion_configure_event),
				completion);
	
	g_signal_connect (completion->priv->window,
			  "realize",
			  G_CALLBACK (gtk_source_completion_realize),
			  completion);
	
	g_signal_connect (completion->priv->window,
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

	g_signal_connect (completion->priv->info_window,
	                  "size-allocate",
	                  G_CALLBACK(info_size_allocate_cb),
	                  completion);
}

static void
gtk_source_completion_init (GtkSourceCompletion *completion)
{
	completion->priv = GTK_SOURCE_COMPLETION_GET_PRIVATE (completion);

	completion->priv->capability_map = g_hash_table_new_full (g_str_hash, 
	                                                          g_str_equal,
	                                                          (GDestroyNotify)g_free,
	                                                          (GDestroyNotify)g_list_free);

	initialize_ui (completion);
}

static void
add_proposals (GtkSourceCompletion         *completion,
               GtkSourceCompletionProvider *provider)
{
	GList *proposals;
	GList *item;
	GtkSourceCompletionProposal *proposal;
	GtkTextIter iter;
g_warning ("3");
	get_iter_at_insert (completion, &iter);
	
	proposals = gtk_source_completion_provider_get_proposals (provider, &iter);
	
	completion->priv->inserting_data = TRUE;
	
	for (item = proposals; item; item = g_list_next (item))
	{
		if (GTK_IS_SOURCE_COMPLETION_PROPOSAL (item->data))
		{
			proposal = GTK_SOURCE_COMPLETION_PROPOSAL (item->data);
			gtk_source_completion_model_append (completion->priv->model_proposals,
	                                                    provider,
	                                                    proposal);
			g_object_unref (proposal);
		}
	}

	gtk_source_completion_model_run_add_proposals (completion->priv->model_proposals);
	g_warning ("4");
	g_list_free (proposals);
}

static gchar **
get_separate_capabilities (GtkSourceCompletionProvider *provider)
{
	const gchar *capabilities;
	capabilities = gtk_source_completion_provider_get_capabilities (provider);
	
	return g_strsplit_set (capabilities, " ,", -1);
}

static void
add_capabilities (GtkSourceCompletion          *completion,
                  GtkSourceCompletionProvider  *provider)
{
	gchar **caps = get_separate_capabilities (provider);
	gchar **orig = caps;
	
	while (caps && *caps)
	{
		GList *ptr = g_hash_table_lookup (completion->priv->capability_map,
		                                  *caps);

		ptr = g_list_copy (ptr);
		ptr = g_list_append (ptr, provider);
		
		g_hash_table_insert (completion->priv->capability_map,
		                     g_strdup (*caps),
		                     ptr);
		
		++caps;
	}
	
	if (orig)
	{
		g_strfreev (orig);
	}
}

static void
remove_capabilities (GtkSourceCompletion          *completion,
                     GtkSourceCompletionProvider  *provider)
{
	gchar **caps = get_separate_capabilities (provider);
	gchar **orig = caps;
	
	while (caps && *caps)
	{
		GList *ptr = g_hash_table_lookup (completion->priv->capability_map,
		                                  *caps);

		ptr = g_list_copy (ptr);
		
		if (ptr)
		{
			ptr = g_list_remove (ptr, provider);
			g_hash_table_insert (completion->priv->capability_map,
			                     g_strdup (*caps),
			                     ptr);
		}
		
		++caps;
	}
	
	if (orig)
	{
		g_strfreev (orig);
	}
}

/**
 * gtk_source_completion_show:
 * @completion: A #GtkSourceCompletion
 * @providers: A list of #GtkSourceCompletionProvider
 * @place: The place where you want to position the popup window, or %NULL
 *
 * Shows the show completion window. If @place if %NULL the popup window will
 * be placed on the cursor position.
 *
 * Returns: %TRUE if it was possible to the show completion window.
 */
gboolean
gtk_source_completion_show (GtkSourceCompletion *completion,
                            GList               *providers,
                            GtkTextIter         *place)
{
	GList *l;

	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	
	/* Make sure to clear any active completion */
	gtk_source_completion_hide_default (completion);
	
	if (providers == NULL)
	{
		gtk_source_completion_hide (completion);
		return FALSE;
	}
	
	update_typing_offsets (completion);

	if (place == NULL)
	{
		gtk_source_completion_utils_move_to_cursor (GTK_WINDOW (completion->priv->window),
							    GTK_SOURCE_VIEW (completion->priv->view));
	}
	else
	{
		gtk_source_completion_utils_move_to_iter (GTK_WINDOW (completion->priv->window),
							  GTK_SOURCE_VIEW (completion->priv->view),
							  place);
	}

	/* Make sure all providers are ours */
	for (l = providers; l; l = g_list_next (l))
	{
		if (g_list_find (completion->priv->providers,
		                 l->data) != NULL)
		{
			completion->priv->active_providers =
				g_list_prepend (completion->priv->active_providers,
				                l->data);

			add_proposals (completion, GTK_SOURCE_COMPLETION_PROVIDER (l->data));
		}
	}
g_warning ("5");
	completion->priv->active_providers = 
		g_list_reverse (completion->priv->active_providers);

	completion->priv->is_interactive = FALSE;

	update_selection_label (completion);
	
	return TRUE;
}

GList *
gtk_source_completion_get_providers (GtkSourceCompletion  *completion,
                                     const gchar          *capabilities)
{
	gchar **caps = NULL;
	gchar **orig;
	GList *ret = NULL;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);

	if (capabilities)
	{
		caps = g_strsplit_set (capabilities, " ,", -1);
	}
	
	if (caps)
	{
		orig = caps;
		
		while (*caps)
		{
			GList *ptr = g_hash_table_lookup (completion->priv->capability_map,
			                                  *caps);

			ret = g_list_concat (ret, g_list_copy (ptr));
			
			++caps;
		}
		
		g_strfreev (orig);
	}
	else
	{
		ret = g_list_copy (completion->priv->providers);
	}
		
	g_list_foreach (ret, (GFunc)g_object_ref, NULL);

	
	return ret;
}

GQuark
gtk_source_completion_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark) == 0)
	{
		quark = g_quark_from_static_string ("gtk-source-completion-error-quark");
	}
	
	return quark;
}

/**
 * gtk_source_completion_new:
 * @view: A #GtkSourceView
 *
 * Create a new #GtkSourceCompletion associated with @view.
 *
 * Returns: The new #GtkSourceCompletion.
 */
GtkSourceCompletion *
gtk_source_completion_new (GtkSourceView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), NULL);

	return g_object_new (GTK_TYPE_SOURCE_COMPLETION,
	                     "view", view,
	                     NULL);
}

/**
 * gtk_source_completion_add_provider:
 * @completion: A #GtkSourceCompletion
 * @provider: A #GtkSourceCompletionProvider
 * @error: A #GError
 *
 * Add a new #GtkSourceCompletionProvider to the completion object. This will
 * add a reference @provider, so make sure to unref your own copy when you
 * no longer need it.
 *
 * Returns: %TRUE if @provider was successfully added, otherwise if @error
 *          is provided, it will be set with the error and %FALSE is returned.
 **/
gboolean
gtk_source_completion_add_provider (GtkSourceCompletion          *completion,
				    GtkSourceCompletionProvider  *provider,
				    GError                      **error)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	
	if (g_list_find (completion->priv->providers, provider) != NULL)
	{
		if (error)
		{
			g_set_error (error, 
			             GTK_SOURCE_COMPLETION_ERROR, 
			             GTK_SOURCE_COMPLETION_ERROR_ALREADY_BOUND,
			             "Provider is already bound to this completion object");
		}

		return FALSE;
	}

	completion->priv->providers = g_list_append (completion->priv->providers, 
	                                             g_object_ref (provider));

	add_capabilities (completion, provider);

	if (error)
	{
		*error = NULL;
	}

	return TRUE;
}

/**
 * gtk_source_completion_remove_provider:
 * @completion: A #GtkSourceCompletion
 * @provider: A #GtkSourceCompletionProvider
 * @error: A #GError
 *
 * Remove @provider from the completion.
 * 
 * Returns: %TRUE if @provider was successfully removed, otherwise if @error
 *          is provided, it will be set with the error and %FALSE is returned.
 **/
gboolean
gtk_source_completion_remove_provider (GtkSourceCompletion          *completion,
				       GtkSourceCompletionProvider  *provider,
				       GError                      **error)
{
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);

	item = g_list_find (completion->priv->providers, provider);

	if (item != NULL)
	{
		remove_capabilities (completion, provider);

		completion->priv->providers = g_list_remove_link (completion->priv->providers, item);
		g_object_unref (provider);

		if (error)
		{
			*error = NULL;
		}

		return TRUE;
	}
	else
	{
		if (error)
		{
			g_set_error (error,
			             GTK_SOURCE_COMPLETION_ERROR,
			             GTK_SOURCE_COMPLETION_ERROR_NOT_BOUND,
			             "Provider is not bound to this completion object");
		}
		
		return FALSE;
	}
}

/**
 * gtk_source_completion_hide:
 * @completion: A #GtkSourceCompletion
 * 
 * Hides the completion if it is active (visible).
 */
void
gtk_source_completion_hide (GtkSourceCompletion *completion)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION (completion));
	
	/* Hiding the completion window will trigger the actual hide */
	if (GTK_WIDGET_VISIBLE (completion->priv->window))
	{
		g_signal_emit (completion, signals[HIDE], 0);
	}
}

/**
 * gtk_source_completion_get_info_window:
 * @completion: A #GtkSourceCompletion
 *
 * The info widget is the window where the completion displays optional extra
 * information of the proposal.
 *
 * Returns: The #GtkSourceCompletionInfo window.
 */
GtkSourceCompletionInfo *
gtk_source_completion_get_info_window (GtkSourceCompletion *completion)
{
	return GTK_SOURCE_COMPLETION_INFO (completion->priv->info_window);
}

/**
 * gtk_source_completion_get_view:
 * @completion: A #GtkSourceCompletion
 *
 * The #GtkSourceView associated with @completion.
 *
 * Returns: The #GtkSourceView associated with @completion.
 */
GtkSourceView *
gtk_source_completion_get_view (GtkSourceCompletion *completion)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);
	
	return completion->priv->view;
}

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
	PROPOSAL_ACTIVATED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_VIEW,
	PROP_MANAGE_KEYS,
	PROP_REMEMBER_INFO_VISIBILITY,
	PROP_SELECT_ON_SHOW,
	
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
	GtkWidget *info_window;
	GtkWidget *info_button;
	GtkWidget *selection_label;
	GtkWidget *bottom_bar;
	GtkWidget *default_info;
	
	GtkWidget *tree_view_proposals;
	GtkSourceCompletionModel *model_proposals;
	
	gboolean destroy_has_run;
	gboolean manage_keys;
	gboolean remember_info_visibility;
	gboolean info_visible;
	gboolean select_on_show;
	
	/* Completion management */
	GtkSourceView *view;

	GList *providers;
	GList *auto_providers;
	GList *active_providers;
	
	guint show_timed_out_id;
	guint auto_complete_delay;
	guint minimum_auto_complete_length;
	
	gint typing_line;
	gint typing_line_offset;
	
	GtkSourceCompletionProvider *filter_provider;
	gchar *filter_criteria;
	
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
	
	if (selector (model, &iter, hasselection, userdata))
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
update_selection_label (GtkSourceCompletion *completion)
{
	if (completion->priv->filter_provider == NULL)
	{
		gtk_label_set_text (GTK_LABEL (completion->priv->selection_label),
		                    _("All"));
	}
	else
	{
		gtk_label_set_text (GTK_LABEL (completion->priv->selection_label),
		                    gtk_source_completion_provider_get_name (completion->priv->filter_provider));	
	}
}

static void
do_refilter (GtkSourceCompletion *completion,
             gboolean             finish_if_empty)
{
	gtk_source_completion_model_refilter (completion->priv->model_proposals);
	
	/* Check if there are any proposals left */
	if (finish_if_empty && 
	    gtk_source_completion_model_is_empty (completion->priv->model_proposals, FALSE))
	{
		gtk_source_completion_finish (completion);
	}
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

	/* If there is only one provider, then there is no other selection */
	if (completion->priv->active_providers->next == NULL)
	{
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
	do_refilter (completion, FALSE);	
	
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

static GtkSourceCompletionModelFilterFlag
proposals_filter_func (GtkSourceCompletionModel    *model,
                       GtkSourceCompletionProvider *provider,
                       GtkSourceCompletionProposal *proposal,
                       GtkSourceCompletion         *completion)
{
	GtkSourceCompletionModelFilterFlag ret = GTK_SOURCE_COMPLETION_MODEL_NONE;
	gboolean visible;
	gboolean count;
	
	/* Filter on provider */
	if (completion->priv->filter_provider != NULL && completion->priv->filter_provider != provider)
	{
		visible = FALSE;
		count = TRUE;
	}
	else if (completion->priv->filter_criteria == NULL)
	{
		visible = TRUE;
		count = FALSE;
	}
	else
	{
		visible = gtk_source_completion_provider_filter_proposal (provider,
	                                                                  proposal,
	                                                                  completion->priv->filter_criteria);

		count = FALSE;
	}
	
	if (!visible)
	{
		ret |= GTK_SOURCE_COMPLETION_MODEL_FILTERED;
		
		if (count)
		{
			ret |= GTK_SOURCE_COMPLETION_MODEL_COUNT;
		}
	}
	
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
add_proposal (GtkSourceCompletion         *completion,
              GtkSourceCompletionProvider *provider,
              GtkSourceCompletionProposal *proposal)
{
	GtkTreeIter iter;

	gtk_source_completion_model_append (completion->priv->model_proposals,
	                                    provider,
	                                    proposal,
	                                    &iter);
}

static void
gtk_source_completion_show (GtkWidget *widget)
{
	/* Only show the popup, the positions is set before this function */
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (widget);

	if (!GTK_WIDGET_VISIBLE (completion))
	{
		GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->show (widget);
		
		if (!completion->priv->remember_info_visibility)
			completion->priv->info_visible = FALSE;
		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (completion->priv->info_button),
					      completion->priv->info_visible);
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
	                                                                  GTK_TEXT_VIEW (completion->priv->view))));
	gtk_source_completion_finish (completion);
	return ret;
}

static void
gtk_source_completion_finish_real (GtkSourceCompletion *completion)
{
	completion->priv->filter_provider = NULL;

	gtk_label_set_markup (GTK_LABEL (completion->priv->default_info), "");

	gtk_source_completion_model_clear (completion->priv->model_proposals);

	g_list_free (completion->priv->active_providers);
	completion->priv->active_providers = NULL;
	
	g_free (completion->priv->filter_criteria);
	completion->priv->filter_criteria = NULL;
	
	gtk_widget_hide (GTK_WIDGET (completion->priv->info_window));
}

static void
gtk_source_completion_hide (GtkWidget *widget)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (widget);

	completion->priv->info_visible = GTK_WIDGET_VISIBLE (completion->priv->info_window);

	GTK_WIDGET_CLASS (gtk_source_completion_parent_class)->hide (widget);	

	gtk_source_completion_finish_real (completion);
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

static gboolean
view_key_press_event_cb (GtkSourceView       *view,
			 GdkEventKey         *event, 
			 GtkSourceCompletion *completion)
{
	gboolean ret = FALSE;
	GdkModifierType mod;
	
	mod = gtk_accelerator_get_default_mod_mask () & event->state;
	
	if (!GTK_WIDGET_VISIBLE (completion) || !completion->priv->manage_keys)
	{
		return FALSE;
	}
	
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
refilter_proposals_with_word (GtkSourceCompletion *completion)
{
	GtkTextView *view;
	
	g_free (completion->priv->filter_criteria);
	view = GTK_TEXT_VIEW (completion->priv->view);
	
	completion->priv->filter_criteria = 
		gtk_source_completion_utils_get_word (GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (view)));
	
	do_refilter (completion, TRUE);
}

static void
update_typing_offsets (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));
	mark = gtk_text_buffer_get_insert (buffer);

	/* Check if the user has changed the cursor position.If yes, we don't complete */
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

	completion->priv->typing_line = gtk_text_iter_get_line (&iter);
	completion->priv->typing_line_offset = gtk_text_iter_get_line_offset (&iter);
}

static gboolean
buffer_delete_range_cb (GtkTextBuffer       *buffer,
                        GtkTextIter         *start,
                        GtkTextIter         *end,
                        GtkSourceCompletion *completion)
{
	if (!GTK_WIDGET_VISIBLE (completion))
	{
		return FALSE;
	}

	if (gtk_text_iter_get_line (start) != completion->priv->typing_line ||
	    gtk_text_iter_get_line_offset (start) < completion->priv->typing_line_offset)
	{
		gtk_source_completion_finish (completion);
	}
	else
	{
		refilter_proposals_with_word (completion);
	}
	
	return FALSE;
}

static gboolean
show_auto_completion (GtkSourceCompletion *completion)
{
	GtkTextBuffer *buffer;
	GtkTextMark *insert_mark;
	GtkTextIter iter;
	gchar *word;
	
	completion->priv->show_timed_out_id = 0;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (completion->priv->view));
	insert_mark = gtk_text_buffer_get_insert (buffer);

	/* Check if the user has changed the cursor position.If yes, we don't complete */
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert_mark);
	
	if ((gtk_text_iter_get_line (&iter) != completion->priv->typing_line) ||
	    (gtk_text_iter_get_line_offset (&iter) != completion->priv->typing_line_offset))
	{
		return FALSE;
	}
	
	word = gtk_source_completion_utils_get_word (GTK_SOURCE_BUFFER (buffer));
	
	/* Check minimum amount of characters */
	if (g_utf8_strlen (word, -1) >= completion->priv->minimum_auto_complete_length)
	{
		gtk_source_completion_popup (completion, completion->priv->auto_providers, word);
	}

	g_free (word);
	
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
	if (len > 1)
	{
		gtk_source_completion_finish (completion);		
		return;
	}
	
	if (!GTK_WIDGET_VISIBLE (completion))
	{
		if (completion->priv->auto_providers != NULL)
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
	}
	else
	{
		if (gtk_source_completion_utils_is_separator (g_utf8_get_char (text)) ||
		    gtk_text_iter_get_line (location) != completion->priv->typing_line ||
		    gtk_text_iter_get_line_offset (location) < completion->priv->typing_line_offset)
		{
			gtk_source_completion_finish (completion);
		}
		else
		{
			refilter_proposals_with_word (completion);
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
gtk_source_completion_finalize (GObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	if (completion->priv->show_timed_out_id != 0)
	{
		g_source_remove (completion->priv->show_timed_out_id);
	}
	
	g_free (completion->priv->filter_criteria);
	
	G_OBJECT_CLASS (gtk_source_completion_parent_class)->finalize (object);
}

static void
gtk_source_completion_destroy (GtkObject *object)
{
	GtkSourceCompletion *completion = GTK_SOURCE_COMPLETION (object);
	
	if (!completion->priv->destroy_has_run)
	{
		g_list_foreach (completion->priv->providers, (GFunc)g_object_unref, NULL);
		g_list_free (completion->priv->providers);
		
		disconnect_view (completion);
		g_object_unref (completion->priv->view);

		completion->priv->destroy_has_run = TRUE;
	}

	GTK_OBJECT_CLASS (gtk_source_completion_parent_class)->destroy (object);
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
		case PROP_AUTO_COMPLETE_DELAY:
			g_value_set_uint (value, completion->priv->auto_complete_delay);
			break;
		case PROP_MINIMUM_AUTO_COMPLETE_LENGTH:
			g_value_set_uint (value, completion->priv->minimum_auto_complete_length);
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
		case PROP_AUTO_COMPLETE_DELAY:
			completion->priv->auto_complete_delay = g_value_get_uint (value);
			break;
		case PROP_MINIMUM_AUTO_COMPLETE_LENGTH:
			completion->priv->minimum_auto_complete_length = g_value_get_uint (value);
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
					 PROP_VIEW,
					 g_param_spec_object ("view",
							      _("The completion view"),
							      _("The GtkSourceView bound to the completion"),
							      GTK_TYPE_SOURCE_VIEW,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
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
	 * GtkSourceCompletion:auto-complete-delay:
	 *
	 * The auto completion delay when typing
	 */
	g_object_class_install_property (object_class,
					 PROP_AUTO_COMPLETE_DELAY,
					 g_param_spec_uint ("auto-complete-delay",
							    _("Auto Complete Delay"),
							    _("Auto completion delay when typing"),
							    0,
							    G_MAXUINT,
							    500,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * GtkSourceCompletion:min-len:
	 *
	 * The minimum word length to initiate auto completion
	 */
	g_object_class_install_property (object_class,
					 PROP_MINIMUM_AUTO_COMPLETE_LENGTH,
					 g_param_spec_uint ("minimum-auto-complete-length",
							    _("Minimum Auto Complete Length"),
							    _("Minimum word length to initiate auto completion"),
							    0,
							    G_MAXUINT,
							    3,
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

static GtkWidget *
initialize_proposals_ui (GtkSourceCompletion *completion)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkWidget *scrolled_window;
	GtkWidget *tree_view;
	
	completion->priv->model_proposals = 
		gtk_source_completion_model_new ((GtkSourceCompletionModelVisibleFunc)proposals_filter_func, 
		                                 completion);

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (completion->priv->model_proposals));
	completion->priv->tree_view_proposals = tree_view;
	
	gtk_widget_show (tree_view);
	g_object_set (tree_view, "can-focus", FALSE, NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	
	/* Create the tree */
	column = gtk_tree_view_column_new ();	
	renderer = gtk_cell_renderer_pixbuf_new ();
	
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);

	gtk_tree_view_column_set_attributes (column, renderer, "text", GTK_SOURCE_COMPLETION_MODEL_COLUMN_LABEL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	g_signal_connect (tree_view,
			  "row-activated",
			  G_CALLBACK (row_activated_cb),
			  completion);
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
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
	
	return scrolled_window;
}

static void
initialize_ui (GtkSourceCompletion *completion)
{
	GtkWidget *info_icon;
	GtkWidget *info_button;
	GtkWidget *vbox;
	GtkWidget *container;

	gtk_window_set_type_hint (GTK_WINDOW (completion), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_focus_on_map (GTK_WINDOW (completion), FALSE);
	gtk_widget_set_size_request (GTK_WIDGET (completion), WINDOW_WIDTH, WINDOW_HEIGHT);
	gtk_window_set_decorated (GTK_WINDOW (completion), FALSE);

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
}

static void
gtk_source_completion_init (GtkSourceCompletion *completion)
{
	completion->priv = GTK_SOURCE_COMPLETION_GET_PRIVATE (completion);

	initialize_ui (completion);
}

static void
add_proposals (GtkSourceCompletion         *completion,
               GtkSourceCompletionProvider *provider)
{
	GList *proposals;
	GList *item;
	GtkSourceCompletionProposal *proposal;

	proposals = gtk_source_completion_provider_get_proposals (provider);
	
	for (item = proposals; item; item = g_list_next (item))
	{
		if (GTK_IS_SOURCE_COMPLETION_PROPOSAL (item->data))
		{
			proposal = GTK_SOURCE_COMPLETION_PROPOSAL (item->data);
			add_proposal (completion, provider, proposal);
			g_object_unref (proposal);
		}
	}
	
	g_list_free (proposals);
}

gboolean
gtk_source_completion_popup (GtkSourceCompletion *completion,
                             GList               *providers,
                             const gchar         *criteria)
{
	GList *l;
	gint x, y;

	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	
	/* Make sure to clear any active completion */
	gtk_source_completion_finish_real (completion);
	
	/* Use all registered providers if no providers were specified */
	if (providers == NULL)
	{
		providers = completion->priv->providers;
	}
	
	if (providers == NULL)
	{
		gtk_source_completion_finish (completion);
		return FALSE;
	}
	
	completion->priv->filter_criteria = g_strdup (criteria);
	update_typing_offsets (completion);
	
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
	
	completion->priv->active_providers = 
		g_list_reverse (completion->priv->active_providers);
	
	/* Check if there are any completions */
	if (gtk_source_completion_model_is_empty (completion->priv->model_proposals, FALSE))
	{
		gtk_source_completion_finish (completion);
		return FALSE;
	}

	update_selection_label (completion);	

	/* FIXME: Maybe support are types of positioning */
	gtk_source_completion_utils_get_pos_at_cursor (GTK_WINDOW (completion),
						       GTK_SOURCE_VIEW (completion->priv->view),
						       &x, &y, NULL);

	gtk_window_move (GTK_WINDOW (completion), x, y);
	
	gtk_widget_show (GTK_WIDGET (completion));
	gtk_widget_grab_focus (GTK_WIDGET (completion->priv->view));

	if (completion->priv->select_on_show)
	{
		select_first_proposal (completion);
	}
	
	return TRUE;
}

/**
 * gtk_source_completion_new:
 * @view: the #GtkSourceView to create the completion for
 *
 * Create a new #GtkSourceCompletion associated with @view
 *
 * Returns: The new #GtkSourceCompletion
 */
GtkSourceCompletion *
gtk_source_completion_new (GtkTextView *view)
{
	g_return_val_if_fail (GTK_IS_SOURCE_VIEW (view), NULL);

	return g_object_new (GTK_TYPE_SOURCE_COMPLETION,
	                    "type", GTK_WINDOW_POPUP,
	                    "view", view,
	                    NULL);
}

/**
 * gtk_source_completion_add_provider:
 * @completion: the #GtkSourceCompletion
 * @provider: The #GtkSourceCompletionProvider.
 *
 * Add a new #GtkSourceCompletionProvider to the completion object. This will
 * add a reference @provider, so make sure to unref your own copy when you
 * no longer need it.
 *
 * Returns: %TRUE if @provider was successfully added to @completion, or %FALSE
 *          if @provider was already added to @completion before
 **/
gboolean
gtk_source_completion_add_provider (GtkSourceCompletion         *completion,
				    GtkSourceCompletionProvider *provider)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);
	
	if (g_list_find (completion->priv->providers, provider) != NULL)
	{
		return FALSE;
	}

	completion->priv->providers = g_list_append (completion->priv->providers, 
	                                              g_object_ref (provider));

	if (gtk_source_completion_provider_can_auto_complete (provider))
	{
		completion->priv->auto_providers = 
			g_list_append (completion->priv->auto_providers,
			               provider);
	}
	                                              
	return TRUE;
}

/**
 * gtk_source_completion_remove_provider:
 * @completion: the #GtkSourceCompletion
 * @provider: The #GtkSourceCompletionProvider.
 *
 * Remove @provider from the completion.
 * 
 * Returns: %TRUE if @provider was successfully removed
 **/
gboolean
gtk_source_completion_remove_provider (GtkSourceCompletion         *completion,
				       GtkSourceCompletionProvider *provider)
{
	GList *item;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_PROVIDER (provider), FALSE);

	item = g_list_find (completion->priv->providers, provider);

	if (item != NULL)
	{
		if (gtk_source_completion_provider_can_auto_complete (provider))
		{
			completion->priv->auto_providers = 
				g_list_remove (completion->priv->auto_providers,
				               provider);
		}

		completion->priv->providers = g_list_remove_link (completion->priv->providers, item);
		g_object_unref (provider);

		return TRUE;
	}
	else
	{		
		return FALSE;
	}
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
	if (GTK_WIDGET_VISIBLE (completion))
	{
		gtk_widget_hide (GTK_WIDGET (completion));
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

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gtksourcecompletioncell-private.h"
#include "gtksourcecompletioncontext-private.h"
#include "gtksourcecompletionlist-private.h"
#include "gtksourcecompletionlistbox-private.h"
#include "gtksourcecompletionlistboxrow-private.h"
#include "gtksourcecompletioninfo-private.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourceutils-private.h"
#include "gtksourceview.h"

struct _GtkSourceCompletionList
{
	GtkSourceAssistant          parent_instance;

	GtkSourceCompletionContext *context;
	GtkSourceCompletionInfo    *info;

	/* Template Widgets */
	GtkSourceCompletionListBox *listbox;
	GtkScrolledWindow          *scroller;
	GtkToggleButton            *show_details;
	GtkBox                     *details;
	GtkSourceCompletionCell    *comments;
	GtkLabel                   *alternate_label;

	GtkEventController         *key;
	gulong                      key_press_handler;
	gulong                      key_release_handler;

	guint                       remember_info_visibility : 1;
};

enum {
	PROP_0,
	PROP_CONTEXT,
	PROP_SHOW_DETAILS,
	N_PROPS
};

G_DEFINE_TYPE (GtkSourceCompletionList, _gtk_source_completion_list, GTK_SOURCE_TYPE_ASSISTANT)

static GParamSpec *properties [N_PROPS];

static void
_gtk_source_completion_list_unroot (GtkWidget *widget)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)widget;
	GtkWidget *view;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	if ((view = gtk_widget_get_ancestor (widget, GTK_SOURCE_TYPE_VIEW)))
	{
		gtk_widget_remove_controller (view, self->key);
	}

	GTK_WIDGET_CLASS (_gtk_source_completion_list_parent_class)->unroot (widget);
}

static void
_gtk_source_completion_list_root (GtkWidget *widget)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)widget;
	GtkWidget *view;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	GTK_WIDGET_CLASS (_gtk_source_completion_list_parent_class)->root (widget);

	if ((view = gtk_widget_get_ancestor (widget, GTK_SOURCE_TYPE_VIEW)))
	{
		gtk_widget_add_controller (view, g_object_ref (self->key));
	}
}

static void
_gtk_source_completion_list_hide (GtkWidget *widget)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)widget;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	g_signal_handler_block (self->key, self->key_press_handler);
	g_signal_handler_block (self->key, self->key_release_handler);

	GTK_WIDGET_CLASS (_gtk_source_completion_list_parent_class)->hide (widget);

	if (!self->remember_info_visibility)
	{
		_gtk_source_completion_list_set_show_details (self, FALSE);
	}
}

static void
_gtk_source_completion_list_show (GtkWidget *widget)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)widget;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	GTK_WIDGET_CLASS (_gtk_source_completion_list_parent_class)->show (widget);

	if (_gtk_source_completion_list_get_show_details (self))
	{
		gtk_widget_set_visible (GTK_WIDGET (self->info), TRUE);
	}

	g_signal_handler_unblock (self->key, self->key_press_handler);
	g_signal_handler_unblock (self->key, self->key_release_handler);
}

static void
_gtk_source_completion_list_show_details_notify_active_cb (GtkSourceCompletionList *self,
                                                           GParamSpec              *pspec,
                                                           GtkToggleButton         *toggle)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (pspec != NULL);
	g_assert (GTK_IS_TOGGLE_BUTTON (toggle));

	if (gtk_widget_get_visible (GTK_WIDGET (self)) &&
	    _gtk_source_completion_list_get_show_details (self))
	{
		gtk_widget_set_visible (GTK_WIDGET (self->info), TRUE);
	}
	else
	{
		gtk_widget_set_visible (GTK_WIDGET (self->info), FALSE);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SHOW_DETAILS]);
}

static void
_gtk_source_completion_list_update_comment (GtkSourceCompletionList *self)
{
	GtkSourceCompletionCell *info = NULL;
	GtkSourceCompletionProposal *proposal = NULL;
	GtkSourceCompletionProvider *provider = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	if (self->info != NULL)
	{
		info = _gtk_source_completion_info_get_cell (self->info);
	}

        gtk_source_completion_cell_set_widget (self->comments, NULL);

	if (_gtk_source_completion_list_box_get_selected (self->listbox, &provider, &proposal))
	{
		gtk_source_completion_provider_display (provider, self->context, proposal, self->comments);

		if (info != NULL)
		{
			gtk_source_completion_provider_display (provider, self->context, proposal, info);
		}
	}
	else
	{
		if (info != NULL)
		{
			gtk_source_completion_cell_set_widget (info, NULL);
		}
	}

	if (_gtk_source_completion_cell_is_empty (self->comments) &&
	    (info == NULL || _gtk_source_completion_cell_is_empty (info)))
	{
		gtk_widget_set_visible (GTK_WIDGET (self->details), FALSE);
	}
	else
	{
		gtk_widget_set_visible (GTK_WIDGET (self->details), TRUE);
	}

	if (info != NULL)
	{
		if (_gtk_source_completion_cell_is_empty (info))
		{
			gtk_widget_set_visible (GTK_WIDGET (self->info), FALSE);
		}
		else if (_gtk_source_completion_list_get_show_details (self))
		{
			if (gtk_widget_get_visible (GTK_WIDGET (self)))
			{
				gtk_widget_set_visible (GTK_WIDGET (self->info), TRUE);
			}
		}
	}

	g_clear_object (&proposal);
	g_clear_object (&provider);
}

static void
_gtk_source_completion_list_notify_alternates_cb (GtkSourceCompletionList    *self,
                                                  GParamSpec                 *pspec,
                                                  GtkSourceCompletionListBox *listbox)
{
	int n_alternates;
	int alternate;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (listbox));

	n_alternates = _gtk_source_completion_list_box_get_n_alternates (listbox);
	alternate = _gtk_source_completion_list_box_get_alternate (listbox);

	if (n_alternates == 0)
	{
		gtk_label_set_label (self->alternate_label, NULL);
	}
	else
	{
		char *str;

		n_alternates++;
		alternate = _gtk_source_completion_list_box_get_alternate (self->listbox);

		if (alternate == -1)
		{
			str = g_strdup_printf (_("%d of %u"), 1, n_alternates);
		}
		else
		{
			str = g_strdup_printf (_("%d of %u"), alternate + 1, n_alternates);
		}

		gtk_label_set_label (self->alternate_label, str);

		g_free (str);
	}
}

static void
_gtk_source_completion_list_reposition_cb (GtkSourceCompletionList *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));

	_gtk_source_assistant_update_position (GTK_SOURCE_ASSISTANT (self));
}

static void
_gtk_source_completion_list_notify_proposal_cb (GtkSourceCompletionList    *self,
                                                GParamSpec                 *pspec,
                                                GtkSourceCompletionListBox *listbox)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (listbox));

	_gtk_source_completion_list_update_comment (self);
	_gtk_source_completion_list_notify_alternates_cb (self, NULL, listbox);
}

static void
_gtk_source_completion_list_get_offset (GtkSourceAssistant *assistant,
                                        int                *x_offset,
                                        int                *y_offset)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)assistant;
	GtkSourceCompletionListBoxRow *row;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (x_offset != NULL);
	g_assert (y_offset != NULL);

	GTK_SOURCE_ASSISTANT_CLASS (_gtk_source_completion_list_parent_class)->get_offset (assistant, x_offset, y_offset);

	if ((row = _gtk_source_completion_list_box_get_first (self->listbox)))
	{
		*x_offset = _gtk_source_completion_list_box_row_get_x_offset (row, GTK_WIDGET (self));
	}
}

static gboolean
key_press_propagate_cb (GtkSourceCompletionList *self,
                        guint                    keyval,
                        guint                    keycode,
                        GdkModifierType          modifiers,
                        GtkEventControllerKey   *key)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

	if (keyval == GDK_KEY_Escape)
	{
		/* Still propagate after hiding */
		gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
	}
	else if (gtk_event_controller_key_forward (key, GTK_WIDGET (self->listbox)))
	{
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static gboolean
key_release_propagate_cb (GtkSourceCompletionList *self,
                          guint                    keyval,
                          guint                    keycode,
                          GdkModifierType          modifiers,
                          GtkEventControllerKey   *key)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

	if (gtk_event_controller_key_forward (key, GTK_WIDGET (self->listbox)))
	{
		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static void
_gtk_source_completion_list_get_target_location (GtkSourceAssistant *assistant,
                                                 GdkRectangle       *rect)
{
	g_assert (GTK_SOURCE_IS_ASSISTANT (assistant));
	g_assert (rect != NULL);

	GTK_SOURCE_ASSISTANT_CLASS (_gtk_source_completion_list_parent_class)->get_target_location (assistant, rect);

	/* We want to align to the beginning of the character, so set the
	 * width to one to ensure that. We do not use zero here just to help
	 * ensure math is stable but also because GtkPopover's
	 * gtk_popover_set_pointing_to() would convert width to one anyway.
	 * That way the value can be compared for changes.
	 */
	rect->width = 1;
}

static GtkSizeRequestMode
_gtk_source_completion_list_get_request_mode (GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
_gtk_source_completion_list_dispose (GObject *object)
{
	GtkSourceCompletionList *self = (GtkSourceCompletionList *)object;

	g_clear_object (&self->context);

	if (self->key)
	{
		g_clear_signal_handler (&self->key_press_handler, self->key);
		g_clear_signal_handler (&self->key_release_handler, self->key);
		g_clear_object (&self->key);
	}

	G_OBJECT_CLASS (_gtk_source_completion_list_parent_class)->dispose (object);
}

static void
_gtk_source_completion_list_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	GtkSourceCompletionList *self = GTK_SOURCE_COMPLETION_LIST (object);

	switch (prop_id)
	{
	case PROP_CONTEXT:
		g_value_set_object (value, self->context);
		break;

	case PROP_SHOW_DETAILS:
		g_value_set_boolean (value, _gtk_source_completion_list_get_show_details (self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
_gtk_source_completion_list_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	GtkSourceCompletionList *self = GTK_SOURCE_COMPLETION_LIST (object);

	switch (prop_id)
	{
	case PROP_CONTEXT:
		_gtk_source_completion_list_set_context (self, g_value_get_object (value));
		break;

	case PROP_SHOW_DETAILS:
		_gtk_source_completion_list_set_show_details (self, g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
_gtk_source_completion_list_class_init (GtkSourceCompletionListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkSourceAssistantClass *assistant_class = GTK_SOURCE_ASSISTANT_CLASS (klass);

	object_class->dispose = _gtk_source_completion_list_dispose;
	object_class->get_property = _gtk_source_completion_list_get_property;
	object_class->set_property = _gtk_source_completion_list_set_property;

	widget_class->get_request_mode = _gtk_source_completion_list_get_request_mode;
	widget_class->show = _gtk_source_completion_list_show;
	widget_class->hide = _gtk_source_completion_list_hide;
	widget_class->root = _gtk_source_completion_list_root;
	widget_class->unroot = _gtk_source_completion_list_unroot;

	assistant_class->get_offset = _gtk_source_completion_list_get_offset;
	assistant_class->get_target_location = _gtk_source_completion_list_get_target_location;

	properties [PROP_CONTEXT] =
		g_param_spec_object ("context",
		                     "Context",
		                     "The context containing results",
		                     GTK_SOURCE_TYPE_COMPLETION_CONTEXT,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	properties [PROP_SHOW_DETAILS] =
		g_param_spec_boolean ("show-details",
		                      "Show Details",
		                      "Show the details assistant",
		                      FALSE,
		                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gtksourceview/ui/gtksourcecompletionlist.ui");
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, alternate_label);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, comments);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, details);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, listbox);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, scroller);
	gtk_widget_class_bind_template_child (widget_class, GtkSourceCompletionList, show_details);
	gtk_widget_class_bind_template_callback (widget_class, _gtk_source_completion_list_notify_proposal_cb);
	gtk_widget_class_bind_template_callback (widget_class, _gtk_source_completion_list_reposition_cb);

	g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_LIST_BOX);
}

static void
_gtk_source_completion_list_init (GtkSourceCompletionList *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));
	gtk_widget_add_css_class (GTK_WIDGET (self), "completion");

	gtk_popover_set_position (GTK_POPOVER (self), GTK_POS_BOTTOM);
	gtk_popover_set_autohide (GTK_POPOVER (self), FALSE);

	self->key = gtk_event_controller_key_new ();
	gtk_event_controller_set_name (self->key, "gtk-source-completion");
	gtk_event_controller_set_propagation_phase (self->key, GTK_PHASE_CAPTURE);
	self->key_press_handler =
		g_signal_connect_object (self->key,
					 "key-pressed",
					 G_CALLBACK (key_press_propagate_cb),
					 self,
					 G_CONNECT_SWAPPED);
	self->key_release_handler =
		g_signal_connect_object (self->key,
					 "key-released",
					 G_CALLBACK (key_release_propagate_cb),
					 self,
					 G_CONNECT_SWAPPED);
	g_signal_handler_block (self->key, self->key_press_handler);
	g_signal_handler_block (self->key, self->key_release_handler);

	self->info = GTK_SOURCE_COMPLETION_INFO (_gtk_source_completion_info_new ());
	_gtk_source_assistant_attach (GTK_SOURCE_ASSISTANT (self->info),
	                              GTK_SOURCE_ASSISTANT (self));

	g_signal_connect_object (self->show_details,
	                         "notify::active",
	                         G_CALLBACK (_gtk_source_completion_list_show_details_notify_active_cb),
	                         self,
	                         G_CONNECT_SWAPPED);

	g_signal_connect_object (self->listbox,
	                         "notify::alternate",
	                         G_CALLBACK (_gtk_source_completion_list_notify_alternates_cb),
	                         self,
	                         G_CONNECT_SWAPPED);

	g_signal_connect_object (self->listbox,
	                         "notify::n-alternates",
	                         G_CALLBACK (_gtk_source_completion_list_notify_alternates_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
}

GtkSourceCompletionList *
_gtk_source_completion_list_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_LIST, NULL);
}

GtkSourceCompletionContext *
_gtk_source_completion_list_get_context (GtkSourceCompletionList *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self), NULL);

	return self->context;
}

void
_gtk_source_completion_list_set_context (GtkSourceCompletionList    *self,
                                         GtkSourceCompletionContext *context)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));
	g_return_if_fail (!context || GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	if (g_set_object (&self->context, context))
	{
		_gtk_source_completion_list_box_set_context (self->listbox, context);
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
	}
}

gboolean
_gtk_source_completion_list_get_show_details (GtkSourceCompletionList *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self), FALSE);

	return gtk_toggle_button_get_active (self->show_details);
}

void
_gtk_source_completion_list_set_show_details (GtkSourceCompletionList *self,
                                              gboolean                 show_details)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	gtk_toggle_button_set_active (self->show_details, show_details);
}

guint
_gtk_source_completion_list_get_n_rows (GtkSourceCompletionList *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self), 0);

	return _gtk_source_completion_list_box_get_n_rows (self->listbox);
}

void
_gtk_source_completion_list_set_n_rows (GtkSourceCompletionList *self,
                                        guint                    n_rows)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	_gtk_source_completion_list_box_set_n_rows (self->listbox, n_rows);
}

void
_gtk_source_completion_list_set_font_desc (GtkSourceCompletionList    *self,
                                           const PangoFontDescription *font_desc)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	_gtk_source_completion_list_box_set_font_desc (self->listbox, font_desc);
}

void
_gtk_source_completion_list_set_show_icons (GtkSourceCompletionList *self,
                                            gboolean                 show_icons)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	_gtk_source_completion_list_box_set_show_icons (self->listbox, show_icons);
}

void
_gtk_source_completion_list_set_remember_info_visibility (GtkSourceCompletionList *self,
                                                          gboolean                 remember_info_visibility)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	self->remember_info_visibility = !!remember_info_visibility;
}

void
_gtk_source_completion_list_move_cursor (GtkSourceCompletionList *self,
					 GtkMovementStep          step,
					 int                      direction)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST (self));

	if (self->listbox != NULL)
	{
		_gtk_source_completion_list_box_move_cursor (self->listbox, step, direction);
	}
}

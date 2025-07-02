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

#include "gtksourcecompletion-private.h"
#include "gtksourcecompletioncontext-private.h"
#include "gtksourcecompletionlistbox-private.h"
#include "gtksourcecompletionlistboxrow-private.h"
#include "gtksourcecompletionproposal.h"
#include "gtksourcecompletionprovider.h"
#include "gtksourceview-private.h"

struct _GtkSourceCompletionListBox
{
	GtkWidget parent_instance;

	/* The box containing the rows. */
	GtkBox *box;

	/* Font styling for rows */
	PangoAttrList *font_attrs;

	/* The completion context that is being displayed. */
	GtkSourceCompletionContext *context;

	/* The handler for GtkSourceCompletionContext::items-chaged which should
	 * be disconnected when no longer needed.
	 */
	gulong items_changed_handler;

	/* The number of rows we expect to have visible to the user. */
	guint n_rows;

	/* The currently selected index within the result set. Signed to
	 * ensure our math in various places allows going negative to catch
	 * lower edge.
	 */
	int selected;

	/* The alternate proposal for the current selection. This references
	 * something from GtkSourceCompletionProvider.list_alternates().
	 */
	GPtrArray *alternates;
	int alternate;

	/* This is set whenever we make a change that requires updating the
	 * row content. We delay the update until the next frame callback so
	 * that we only update once right before we draw the frame. This helps
	 * reduce duplicate work when reacting to ::items-changed in the model.
	 */
	guint queued_update;

	/* These size groups are used to keep each portion of the proposal row
	 * aligned with each other. Since we only have a fixed number of visible
	 * rows, the overhead here is negligible by introducing the size cycle.
	 */
	GtkSizeGroup *before_size_group;
	GtkSizeGroup *typed_text_size_group;
	GtkSizeGroup *after_size_group;

	/* The adjustments for scrolling the GtkScrollable. */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	/* Gesture to handle button press/touch events. */
	GtkGesture *click_gesture;

	/* If icons are visible */
	guint show_icons : 1;
};

typedef struct
{
	GtkSourceCompletionListBox *self;
	GtkSourceCompletionContext *context;
	guint n_items;
	guint position;
	int selected;
} UpdateState;

enum {
	PROP_0,
	PROP_ALTERNATE,
	PROP_CONTEXT,
	PROP_PROPOSAL,
	PROP_N_ROWS,
	PROP_HADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_N_ALTERNATES,
	PROP_VADJUSTMENT,
	PROP_VSCROLL_POLICY,
	N_PROPS
};

enum {
	REPOSITION,
	N_SIGNALS
};

static void     gtk_source_completion_list_box_queue_update (GtkSourceCompletionListBox *self);
static gboolean gtk_source_completion_list_box_update_cb    (GtkWidget                  *widget,
                                                             GdkFrameClock              *frame_clock,
                                                             gpointer                    user_data);
static void     gtk_source_completion_list_box_do_update    (GtkSourceCompletionListBox *self,
                                                             gboolean                    update_selection);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionListBox, gtk_source_completion_list_box, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static void
gtk_source_completion_list_box_set_selected (GtkSourceCompletionListBox *self,
                                             int                         selected)
{
	GtkSourceCompletionProposal *proposal = NULL;
	GtkSourceCompletionProvider *provider = NULL;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (selected == -1 && self->context != NULL)
	{
		GtkSourceCompletion *completion;
		gboolean select_on_show;

		completion = gtk_source_completion_context_get_completion (self->context);
		select_on_show = _gtk_source_completion_get_select_on_show (completion);

		if (select_on_show)
		{
			selected = 0;
		}
	}

	self->selected = selected;
	self->alternate = -1;
	g_clear_pointer (&self->alternates, g_ptr_array_unref);

	if (_gtk_source_completion_list_box_get_selected (self, &provider, &proposal))
	{
		self->alternates = gtk_source_completion_provider_list_alternates (provider, self->context, proposal);

		if (self->alternates != NULL)
		{
			g_ptr_array_set_free_func (self->alternates, g_object_unref);
		}
	}

	gtk_source_completion_list_box_queue_update (self);

	g_clear_object (&proposal);
	g_clear_object (&provider);
}

static gboolean
move_next_alternate (GtkWidget *widget,
                     GVariant  *param,
                     gpointer   user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->alternates == NULL || self->alternates->len == 0)
	{
		return FALSE;
	}

	if ((guint)(self->alternate + 1) < self->alternates->len)
	{
		self->alternate++;
	}
	else
	{
		self->alternate = -1;
	}

	gtk_source_completion_list_box_do_update (self, FALSE);

	return TRUE;
}

static void
move_next_alternate_action (GtkWidget  *widget,
                            const char *action_name,
                            GVariant   *param)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (widget));

	move_next_alternate (widget, param, NULL);
}

static gboolean
move_previous_alternate (GtkWidget *widget,
                         GVariant  *param,
                         gpointer   user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->alternates == NULL || self->alternates->len == 0)
	{
		return FALSE;
	}

	if (self->alternate < 0)
	{
		self->alternate = self->alternates->len - 1;
	}
	else
	{
		self->alternate--;
	}

	gtk_source_completion_list_box_do_update (self, FALSE);

	return TRUE;
}

static void
move_previous_alternate_action (GtkWidget  *widget,
                                const char *action_name,
                                GVariant   *param)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (widget));

	move_previous_alternate (widget, param, NULL);
}

static guint
gtk_source_completion_list_box_get_offset (GtkSourceCompletionListBox *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	return gtk_adjustment_get_value (self->vadjustment);
}

static void
gtk_source_completion_list_box_set_offset (GtkSourceCompletionListBox *self,
                                           guint                       offset)
{
	double value = offset;
	double page_size;
	double upper;
	double lower;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	lower = gtk_adjustment_get_lower (self->vadjustment);
	upper = gtk_adjustment_get_upper (self->vadjustment);
	page_size = gtk_adjustment_get_page_size (self->vadjustment);

	if (value > (upper - page_size))
	{
		value = upper - page_size;
	}

	if (value < lower)
	{
		value = lower;
	}

	gtk_adjustment_set_value (self->vadjustment, value);
}

static void
gtk_source_completion_list_box_value_changed (GtkSourceCompletionListBox *self,
                                              GtkAdjustment              *vadj)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (GTK_IS_ADJUSTMENT (vadj));

	gtk_source_completion_list_box_queue_update (self);
}

static void
gtk_source_completion_list_box_set_hadjustment (GtkSourceCompletionListBox *self,
                                                GtkAdjustment              *hadjustment)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (!hadjustment || GTK_IS_ADJUSTMENT (hadjustment));

	if (g_set_object (&self->hadjustment, hadjustment))
	{
		gtk_source_completion_list_box_queue_update (self);
	}
}

static void
gtk_source_completion_list_box_set_vadjustment (GtkSourceCompletionListBox *self,
                                                GtkAdjustment        *vadjustment)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (!vadjustment || GTK_IS_ADJUSTMENT (vadjustment));

	if (self->vadjustment == vadjustment)
		return;

	if (self->vadjustment)
	{
		g_signal_handlers_disconnect_by_func (self->vadjustment,
						      G_CALLBACK (gtk_source_completion_list_box_value_changed),
						      self);
		g_clear_object (&self->vadjustment);
	}

	if (vadjustment)
	{
		self->vadjustment = g_object_ref (vadjustment);

		gtk_adjustment_set_lower (self->vadjustment, 0);
		gtk_adjustment_set_upper (self->vadjustment, 0);
		gtk_adjustment_set_value (self->vadjustment, 0);
		gtk_adjustment_set_step_increment (self->vadjustment, 1);
		gtk_adjustment_set_page_size (self->vadjustment, self->n_rows);
		gtk_adjustment_set_page_increment (self->vadjustment, self->n_rows);

		g_signal_connect_object (self->vadjustment,
		                         "value-changed",
		                         G_CALLBACK (gtk_source_completion_list_box_value_changed),
		                         self,
		                         G_CONNECT_SWAPPED);
	}

	gtk_source_completion_list_box_queue_update (self);
}

static guint
get_row_at_y (GtkSourceCompletionListBox *self,
              double                      y)
{
	guint offset;
	guint n_items;
	int height;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (G_IS_LIST_MODEL (self->context));

	height = gtk_widget_get_height (GTK_WIDGET (self));
	offset = gtk_source_completion_list_box_get_offset (self);

	n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));
	n_items = MAX (1, MIN (self->n_rows, n_items));

	return offset + (y / (height / n_items));
}

static void
click_gesture_pressed (GtkGestureClick            *gesture,
                       guint                       n_press,
                       double                      x,
                       double                      y,
                       GtkSourceCompletionListBox *self)
{
	int selected;

	g_assert (GTK_IS_GESTURE_CLICK (gesture));
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->context == NULL)
	{
		return;
	}

	selected = get_row_at_y (self, y);

	if (selected != self->selected)
	{
		gtk_source_completion_list_box_set_selected (self, selected);
	}
	else
	{
		GtkSourceCompletionProvider *provider = NULL;
		GtkSourceCompletionProposal *proposal = NULL;

		if (self->selected >= 0 &&
		    self->selected < (int)g_list_model_get_n_items (G_LIST_MODEL (self->context)) &&
		    _gtk_source_completion_context_get_item_full (self->context, self->selected, &provider, &proposal))
		{
			_gtk_source_completion_activate (gtk_source_completion_context_get_completion (self->context),
			                                 self->context, provider, proposal);
			g_clear_object (&provider);
			g_clear_object (&proposal);
		}
	}
}

static gboolean
move_binding_cb (GtkWidget *widget,
                 GVariant  *param,
                 gpointer   user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;
	int direction = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	g_variant_get (param, "(i)", &direction);

	if (ABS (direction) == 1)
	{
		_gtk_source_completion_list_box_move_cursor (self, GTK_MOVEMENT_DISPLAY_LINES, direction);
	}
	else
	{
		_gtk_source_completion_list_box_move_cursor (self, GTK_MOVEMENT_PAGES, direction > 0 ? 1 : -1);
	}

	return TRUE;
}

static gboolean
activate_nth_cb (GtkWidget *widget,
                 GVariant  *param,
                 gpointer   user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;
	GtkSourceCompletionProvider *provider = NULL;
	GtkSourceCompletionProposal *proposal = NULL;
	int nth = 0;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->context == NULL)
	{
		return FALSE;
	}

	g_variant_get (param, "(i)", &nth);

	if (nth == 0 && self->selected >= 0)
	{
		nth = self->selected;
	}
	else
	{
		nth--;
	}

	if (nth < 0 || (guint)nth >= g_list_model_get_n_items (G_LIST_MODEL (self->context)))
	{
		return FALSE;
	}

	if (!_gtk_source_completion_context_get_item_full (self->context, nth, &provider, &proposal))
	{
		return FALSE;
	}

	g_assert (GTK_SOURCE_IS_COMPLETION_PROVIDER (provider));
	g_assert (GTK_SOURCE_IS_COMPLETION_PROPOSAL (proposal));

	_gtk_source_completion_activate (gtk_source_completion_context_get_completion (self->context),
	                                 self->context,
	                                 provider,
	                                 proposal);

	g_clear_object (&proposal);
	g_clear_object (&provider);

	return TRUE;
}

static gboolean
activate_nth_tab_cb (GtkWidget *widget,
                     GVariant  *param,
                     gpointer   user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;
	GtkSourceView *view;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->context == NULL)
	{
		return FALSE;
	}

	/* If Tab was pressed by we have a snippet active, that takes precidence
	 * and we should ignore this completion request.
	 */
	view = gtk_source_completion_context_get_view (self->context);
	if (!view || _gtk_source_view_has_snippet (view))
	{
		return FALSE;
	}

	return activate_nth_cb (widget, param, user_data);
}

static gboolean
_gtk_source_completion_list_box_key_pressed_cb (GtkSourceCompletionListBox *self,
                                                guint                       keyval,
                                                guint                       keycode,
                                                GdkModifierType             state,
                                                GtkEventControllerKey      *key)
{
	GtkSourceCompletionProvider *provider = NULL;
	GtkSourceCompletionProposal *proposal = NULL;
	gboolean ret = FALSE;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (GTK_IS_EVENT_CONTROLLER_KEY (key));

	if (self->context == NULL)
	{
		return FALSE;
	}

	if (_gtk_source_completion_list_box_get_selected (self, &provider, &proposal))
	{
		if (gtk_source_completion_provider_key_activates (provider, self->context, proposal, keyval, state))
		{
			_gtk_source_completion_activate (gtk_source_completion_context_get_completion (self->context),
			                                 self->context,
			                                 provider,
			                                 proposal);
			ret = TRUE;
		}
	}

	g_clear_object (&proposal);
	g_clear_object (&provider);

	return ret;
}

static void
gtk_source_completion_list_box_constructed (GObject *object)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)object;

	G_OBJECT_CLASS (gtk_source_completion_list_box_parent_class)->constructed (object);

	if (self->hadjustment == NULL)
	{
		self->hadjustment = gtk_adjustment_new (0, 0, 0, 0, 0, 0);
	}

	if (self->vadjustment == NULL)
	{
		self->vadjustment = gtk_adjustment_new (0, 0, 0, 0, 0, 0);
	}

	gtk_adjustment_set_lower (self->hadjustment, 0);
	gtk_adjustment_set_upper (self->hadjustment, 0);
	gtk_adjustment_set_value (self->hadjustment, 0);

	gtk_source_completion_list_box_queue_update (self);
}

static void
gtk_source_completion_list_box_dispose (GObject *object)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)object;

	if (self->box != NULL)
	{
		gtk_widget_unparent (GTK_WIDGET (self->box));
		self->box = NULL;
	}

	g_clear_object (&self->before_size_group);
	g_clear_object (&self->typed_text_size_group);
	g_clear_object (&self->after_size_group);
	g_clear_object (&self->hadjustment);
	g_clear_object (&self->vadjustment);
	g_clear_pointer (&self->font_attrs, pango_attr_list_unref);

	G_OBJECT_CLASS (gtk_source_completion_list_box_parent_class)->dispose (object);
}

static void
gtk_source_completion_list_box_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
	GtkSourceCompletionListBox *self = GTK_SOURCE_COMPLETION_LIST_BOX (object);

	switch (prop_id)
	{
	case PROP_N_ALTERNATES:
		g_value_set_int (value, self->alternates ? self->alternates->len : 0);
		break;

	case PROP_ALTERNATE:
		g_value_set_int (value, self->alternate);
		break;

	case PROP_CONTEXT:
		g_value_set_object (value, _gtk_source_completion_list_box_get_context (self));
		break;

	case PROP_PROPOSAL:
		g_value_take_object (value, _gtk_source_completion_list_box_get_proposal (self));
		break;

	case PROP_N_ROWS:
		g_value_set_uint (value, _gtk_source_completion_list_box_get_n_rows (self));
		break;

	case PROP_HADJUSTMENT:
		g_value_set_object (value, self->hadjustment);
		break;

	case PROP_VADJUSTMENT:
		g_value_set_object (value, self->vadjustment);
		break;

	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, GTK_SCROLL_NATURAL);
		break;

	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, GTK_SCROLL_NATURAL);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_list_box_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
	GtkSourceCompletionListBox *self = GTK_SOURCE_COMPLETION_LIST_BOX (object);

	switch (prop_id)
	{
	case PROP_CONTEXT:
		_gtk_source_completion_list_box_set_context (self, g_value_get_object (value));
		break;

	case PROP_N_ROWS:
		_gtk_source_completion_list_box_set_n_rows (self, g_value_get_uint (value));
		break;

	case PROP_HADJUSTMENT:
		gtk_source_completion_list_box_set_hadjustment (self, g_value_get_object (value));
		break;

	case PROP_VADJUSTMENT:
		gtk_source_completion_list_box_set_vadjustment (self, g_value_get_object (value));
		break;

	case PROP_HSCROLL_POLICY:
		/* Do nothing */
		break;

	case PROP_VSCROLL_POLICY:
		/* Do nothing */
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_completion_list_box_class_init (GtkSourceCompletionListBoxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = gtk_source_completion_list_box_constructed;
	object_class->dispose = gtk_source_completion_list_box_dispose;
	object_class->get_property = gtk_source_completion_list_box_get_property;
	object_class->set_property = gtk_source_completion_list_box_set_property;

	properties [PROP_ALTERNATE] =
		g_param_spec_int ("alternate",
		                  "Alternate",
		                  "The alternate to choose",
		                  -1, G_MAXINT, -1,
		                  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_N_ALTERNATES] =
		g_param_spec_int ("n-alternates",
		                  "N Alternates",
		                  "The number of alternates",
		                  0, G_MAXINT, 0,
		                  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_CONTEXT] =
		g_param_spec_object ("context",
				     "Context",
				     "The context being displayed",
				     GTK_SOURCE_TYPE_COMPLETION_CONTEXT,
				     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	properties [PROP_HADJUSTMENT] =
		g_param_spec_object ("hadjustment", NULL, NULL,
				     GTK_TYPE_ADJUSTMENT,
				     (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

	properties [PROP_HSCROLL_POLICY] =
		g_param_spec_enum ("hscroll-policy", NULL, NULL,
				   GTK_TYPE_SCROLLABLE_POLICY,
				   GTK_SCROLL_NATURAL,
				   (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_VADJUSTMENT] =
		g_param_spec_object ("vadjustment", NULL, NULL,
				     GTK_TYPE_ADJUSTMENT,
				     (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

	properties [PROP_VSCROLL_POLICY] =
		g_param_spec_enum ("vscroll-policy", NULL, NULL,
				   GTK_TYPE_SCROLLABLE_POLICY,
				   GTK_SCROLL_NATURAL,
				   (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_PROPOSAL] =
		g_param_spec_object ("proposal",
				     "Proposal",
				     "The proposal that is currently selected",
				     GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
				     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_N_ROWS] =
		g_param_spec_uint ("n-rows",
				   "N Rows",
				   "The number of visible rows",
				   1, 32, 5,
				   (G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	signals [REPOSITION] =
		g_signal_new_class_handler ("reposition",
					    G_TYPE_FROM_CLASS (klass),
					    G_SIGNAL_RUN_LAST,
					    NULL, NULL, NULL,
					    g_cclosure_marshal_VOID__VOID,
					    G_TYPE_NONE, 0);
	g_signal_set_va_marshaller (signals [REPOSITION],
				    G_TYPE_FROM_CLASS (klass),
				    g_cclosure_marshal_VOID__VOIDv);

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_css_name (widget_class, "list");

	gtk_widget_class_install_action (widget_class, "proposal.move-next-alternate", NULL, move_next_alternate_action);
	gtk_widget_class_install_action (widget_class, "proposal.move-previous-alternate", NULL, move_previous_alternate_action);

	gtk_widget_class_add_binding (widget_class, GDK_KEY_Down, 0, move_binding_cb, "(i)", 1);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Up, 0, move_binding_cb, "(i)", -1);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Up, 0, move_binding_cb, "(i)", -2);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Page_Down, 0, move_binding_cb, "(i)", 2);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_1, GDK_ALT_MASK, activate_nth_cb, "(i)", 1);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_2, GDK_ALT_MASK, activate_nth_cb, "(i)", 2);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_3, GDK_ALT_MASK, activate_nth_cb, "(i)", 3);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_4, GDK_ALT_MASK, activate_nth_cb, "(i)", 4);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_5, GDK_ALT_MASK, activate_nth_cb, "(i)", 5);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_6, GDK_ALT_MASK, activate_nth_cb, "(i)", 6);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_7, GDK_ALT_MASK, activate_nth_cb, "(i)", 7);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_8, GDK_ALT_MASK, activate_nth_cb, "(i)", 8);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_9, GDK_ALT_MASK, activate_nth_cb, "(i)", 9);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Return, 0, activate_nth_cb, "(i)", 0);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_KP_Enter, 0, activate_nth_cb, "(i)", 0);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Tab, 0, activate_nth_tab_cb, "(i)", 0);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Right, 0, move_next_alternate, NULL);
	gtk_widget_class_add_binding (widget_class, GDK_KEY_Left, 0, move_previous_alternate, NULL);
	gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "assistant.hide", NULL);

	g_type_ensure (GTK_SOURCE_TYPE_COMPLETION_LIST_BOX_ROW);
}

static void
gtk_source_completion_list_box_init (GtkSourceCompletionListBox *self)
{
	GtkEventController *key;

	key = gtk_event_controller_key_new ();
	g_signal_connect_swapped (key,
	                          "key-pressed",
	                          G_CALLBACK (_gtk_source_completion_list_box_key_pressed_cb),
	                          self);
	gtk_widget_add_controller (GTK_WIDGET (self), key);

	self->box = g_object_new (GTK_TYPE_BOX,
				  "orientation", GTK_ORIENTATION_VERTICAL,
				  "visible", TRUE,
				  NULL);
	gtk_widget_set_parent (GTK_WIDGET (self->box), GTK_WIDGET (self));

	self->selected = -1;
	self->alternate = -1;
	self->before_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->typed_text_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->after_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	self->click_gesture = gtk_gesture_click_new ();
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (self->click_gesture), GTK_PHASE_BUBBLE);
	gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (self->click_gesture), FALSE);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (self->click_gesture), GDK_BUTTON_PRIMARY);
	g_signal_connect_object (self->click_gesture, "pressed",
	                         G_CALLBACK (click_gesture_pressed), self, 0);
	gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (self->click_gesture));
}

static void
gtk_source_completion_list_box_do_update (GtkSourceCompletionListBox *self,
                                          gboolean                    update_selection)
{
	GtkSourceCompletionProvider *last_provider = NULL;
	UpdateState state = {0};

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	state.self = self;
	state.context = self->context;
	state.position = gtk_source_completion_list_box_get_offset (self);
	state.selected = self->selected;

	if (self->context != NULL)
	{
		state.n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));
	}

	state.position = MIN (state.position, MAX (state.n_items, self->n_rows) - self->n_rows);
	state.selected = MIN (self->selected, state.n_items ? (int)state.n_items - 1 : 0);

	if (gtk_adjustment_get_upper (self->vadjustment) != state.n_items)
	{
		gtk_adjustment_set_upper (self->vadjustment, state.n_items);
	}

	for (GtkWidget *iter = gtk_widget_get_first_child (GTK_WIDGET (self->box));
	     iter != NULL;
	     iter = gtk_widget_get_next_sibling (iter))
	{
		GtkSourceCompletionProposal *proposal = NULL;
		GtkSourceCompletionProvider *provider = NULL;
		gboolean has_alternates = FALSE;

		if (!GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (iter))
		{
			continue;
		}

		if (state.selected >= 0 && state.position == (guint)state.selected)
		{
			gtk_widget_set_state_flags (iter, GTK_STATE_FLAG_SELECTED, FALSE);
		}
		else
		{
			gtk_widget_unset_state_flags (iter, GTK_STATE_FLAG_SELECTED);
		}

		if (state.context != NULL && state.position < state.n_items)
		{
			_gtk_source_completion_context_get_item_full (state.context,
			                                              state.position,
			                                              &provider,
			                                              &proposal);

			if (state.selected == (int)state.position)
			{
				if (self->alternate >= 0 && self->alternate < (int)self->alternates->len)
				{
					g_clear_object (&proposal);
					proposal = g_object_ref (g_ptr_array_index (self->alternates, self->alternate));
				}

				has_alternates = self->alternates != NULL && self->alternates->len > 0;
			}

			_gtk_source_completion_list_box_row_display (GTK_SOURCE_COMPLETION_LIST_BOX_ROW (iter),
			                                             state.context,
			                                             provider,
			                                             proposal,
			                                             self->show_icons,
			                                             has_alternates);

			if (last_provider != NULL && provider != last_provider)
			{
				gtk_widget_add_css_class (GTK_WIDGET (iter), "group-leader");
			}
			else
			{
				gtk_widget_remove_css_class (GTK_WIDGET (iter), "group-leader");
			}

			gtk_widget_set_visible (iter, TRUE);
		}
		else
		{
			gtk_widget_set_visible (iter, FALSE);
			_gtk_source_completion_list_box_row_display (GTK_SOURCE_COMPLETION_LIST_BOX_ROW (iter),
			                                             NULL, NULL, NULL, self->show_icons, FALSE);
		}

		state.position++;

		last_provider = provider;

		g_clear_object (&proposal);
		g_clear_object (&provider);
	}

	if (update_selection)
	{
		gtk_source_completion_list_box_set_selected (self, state.selected);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PROPOSAL]);
	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ALTERNATES]);
	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_ALTERNATE]);

	g_signal_emit (self, signals [REPOSITION], 0);
}

static gboolean
gtk_source_completion_list_box_update_cb (GtkWidget     *widget,
                                          GdkFrameClock *frame_clock,
                                          gpointer       user_data)
{
	GtkSourceCompletionListBox *self = (GtkSourceCompletionListBox *)widget;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	self->queued_update = 0;

	gtk_source_completion_list_box_do_update (self, TRUE);

	/* There is a chance that the update sequence could cause us to need
	 * to queue another update. But we don't actually need it. Just cancel
	 * any additional request immediately.
	 */
	if (self->queued_update != 0)
	{
		gtk_widget_remove_tick_callback (GTK_WIDGET (self), self->queued_update);
		self->queued_update = 0;
	}

	return G_SOURCE_REMOVE;
}

static void
gtk_source_completion_list_box_queue_update (GtkSourceCompletionListBox *self)
{
	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	/* See gtk_source_completion_list_box_set_selected() for why this needs
	 * to be set here to avoid re-entrancy.
	 */
	if (self->queued_update == 0)
	{
		self->queued_update = gtk_widget_add_tick_callback (GTK_WIDGET (self),
		                                                    gtk_source_completion_list_box_update_cb,
		                                                    NULL, NULL);
	}
}

GtkWidget *
_gtk_source_completion_list_box_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_COMPLETION_LIST_BOX, NULL);
}

guint
_gtk_source_completion_list_box_get_n_rows (GtkSourceCompletionListBox *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), 0);

	return self->n_rows;
}

void
_gtk_source_completion_list_box_set_n_rows (GtkSourceCompletionListBox *self,
                                            guint                       n_rows)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_return_if_fail (n_rows > 0);
	g_return_if_fail (n_rows <= 32);

	if (n_rows != self->n_rows)
	{
		GtkWidget *tmp;

		while ((tmp = gtk_widget_get_first_child (GTK_WIDGET (self->box))))
		{
			gtk_box_remove (self->box, tmp);
		}

		self->n_rows = n_rows;

		if (self->vadjustment != NULL)
		{
			gtk_adjustment_set_page_size (self->vadjustment, n_rows);
		}

		for (guint i = 0; i < n_rows; i++)
		{
			GtkWidget *row;

			row = _gtk_source_completion_list_box_row_new ();

			gtk_widget_set_can_focus (GTK_WIDGET (row), FALSE);

			_gtk_source_completion_list_box_row_attach (GTK_SOURCE_COMPLETION_LIST_BOX_ROW (row),
			                                            self->before_size_group,
			                                            self->typed_text_size_group,
			                                            self->after_size_group);
			_gtk_source_completion_list_box_row_set_attrs (GTK_SOURCE_COMPLETION_LIST_BOX_ROW (row),
			                                               self->font_attrs);

			gtk_box_append (self->box, row);
		}

		gtk_source_completion_list_box_queue_update (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_N_ROWS]);
	}
}

/**
 * gtk_source_completion_list_box_get_proposal:
 * @self: a #GtkSourceCompletionListBox
 *
 * Gets the currently selected proposal, or %NULL if no proposal is selected
 *
 * Returns: (nullable) (transfer full): a #GtkSourceCompletionProposal or %NULL
 */
GtkSourceCompletionProposal *
_gtk_source_completion_list_box_get_proposal (GtkSourceCompletionListBox *self)
{
	GtkSourceCompletionProposal *ret = NULL;

	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), NULL);

	if (self->context != NULL &&
	    self->selected < (int)g_list_model_get_n_items (G_LIST_MODEL (self->context)))
		ret = g_list_model_get_item (G_LIST_MODEL (self->context), self->selected);

	g_return_val_if_fail (!ret || GTK_SOURCE_IS_COMPLETION_PROPOSAL (ret), NULL);

	return ret;
}

/**
 * _gtk_source_completion_list_box_get_selected:
 * @self: an #GtkSourceCompletionListBox
 * @provider: (out) (transfer full) (optional): a location for the provider
 * @proposal: (out) (transfer full) (optional): a location for the proposal
 *
 * Gets the selected item if there is any.
 *
 * If there is a selection, %TRUE is returned and @provider and @proposal
 * are set to the selected provider/proposal.
 *
 * Returns: %TRUE if there is a selection
 */
gboolean
_gtk_source_completion_list_box_get_selected (GtkSourceCompletionListBox   *self,
                                              GtkSourceCompletionProvider **provider,
                                              GtkSourceCompletionProposal **proposal)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), FALSE);

	if (self->context != NULL)
	{
		guint n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context));

		if (n_items > 0)
		{
			if (self->selected >= 0)
			{
				gint selected = MIN (self->selected, (int)n_items - 1);
				_gtk_source_completion_context_get_item_full (self->context, selected, provider, proposal);
				return TRUE;
			}
		}
	}

	return FALSE;
}

/**
 * gtk_source_completion_list_box_get_context:
 * @self: a #GtkSourceCompletionListBox
 *
 * Gets the context that is being displayed in the list box.
 *
 * Returns: (transfer none) (nullable): an #GtkSourceCompletionContext or %NULL
 */
GtkSourceCompletionContext *
_gtk_source_completion_list_box_get_context (GtkSourceCompletionListBox *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), NULL);

	return self->context;
}

static void
gtk_source_completion_list_box_items_changed_cb (GtkSourceCompletionListBox *self,
                                                 guint                       position,
                                                 guint                       removed,
                                                 guint                       added,
                                                 GListModel                 *model)
{
	guint offset;

	g_assert (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_assert (G_IS_LIST_MODEL (model));

	offset = gtk_source_completion_list_box_get_offset (self);

	/* Skip widget resize if results are out of view and wont force our
	 * current results down.
	 */
	if ((position >= offset + self->n_rows) ||
	    (removed == added && (position + added) < offset))
	{
		return;
	}

	gtk_source_completion_list_box_queue_update (self);
}

/**
 * gtk_source_completion_list_box_set_context:
 * @self: a #GtkSourceCompletionListBox
 * @context: the #GtkSourceCompletionContext
 *
 * Sets the context to be displayed.
 */
void
_gtk_source_completion_list_box_set_context (GtkSourceCompletionListBox *self,
                                             GtkSourceCompletionContext *context)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));
	g_return_if_fail (!context || GTK_SOURCE_IS_COMPLETION_CONTEXT (context));

	if (self->context == context)
		return;

	if (self->context != NULL && self->items_changed_handler != 0)
	{
		g_signal_handler_disconnect (self->context, self->items_changed_handler);
		self->items_changed_handler = 0;
	}

	g_set_object (&self->context, context);

	if (self->context != NULL)
	{
		self->items_changed_handler =
			g_signal_connect_object (self->context,
			                         "items-changed",
			                         G_CALLBACK (gtk_source_completion_list_box_items_changed_cb),
			                         self,
			                         G_CONNECT_SWAPPED);
	}

	gtk_source_completion_list_box_set_selected (self, -1);
	gtk_adjustment_set_value (self->vadjustment, 0);

	g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_CONTEXT]);
}

GtkSourceCompletionListBoxRow *
_gtk_source_completion_list_box_get_first (GtkSourceCompletionListBox *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), NULL);

	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->box));
	     child != NULL;
	     child = gtk_widget_get_next_sibling (child))
	{
		if (GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (child))
		{
			return GTK_SOURCE_COMPLETION_LIST_BOX_ROW (child);
		}
	}

	return NULL;
}

void
_gtk_source_completion_list_box_move_cursor (GtkSourceCompletionListBox *self,
                                             GtkMovementStep             step,
                                             int                         direction)
{
	int n_items;
	int offset;

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	if (self->context == NULL || direction == 0)
		return;

	if (!(n_items = g_list_model_get_n_items (G_LIST_MODEL (self->context))))
		return;

	/* n_items is signed so that we can do negative comparison */
	if (n_items < 0)
		return;

	if (step == GTK_MOVEMENT_BUFFER_ENDS)
	{
		if (direction > 0)
		{
			gtk_source_completion_list_box_set_offset (self, n_items);
			gtk_source_completion_list_box_set_selected (self, n_items - 1);
		}
		else
		{
			gtk_source_completion_list_box_set_offset (self, 0);
			gtk_source_completion_list_box_set_selected (self, -1);
		}

		gtk_source_completion_list_box_queue_update (self);

		return;
	}

	if ((direction < 0 && self->selected == 0) ||
	    (direction > 0 && self->selected == n_items - 1))
	{
		return;
	}

	if (step == GTK_MOVEMENT_PAGES)
	{
		direction *= self->n_rows;
	}

	if ((self->selected + direction) > n_items)
	{
		gtk_source_completion_list_box_set_selected (self, n_items - 1);
	}
	else if ((self->selected + direction) < 0)
	{
		gtk_source_completion_list_box_set_selected (self, 0);
	}
	else
	{
		gtk_source_completion_list_box_set_selected (self, self->selected + direction);
	}

	offset = gtk_source_completion_list_box_get_offset (self);

	if (self->selected < offset)
	{
		gtk_source_completion_list_box_set_offset (self, self->selected);
	}
	else if (self->selected >= (int)(offset + self->n_rows))
	{
		gtk_source_completion_list_box_set_offset (self, self->selected - self->n_rows + 1);
	}

	gtk_source_completion_list_box_queue_update (self);
}

void
_gtk_source_completion_list_box_set_font_desc (GtkSourceCompletionListBox *self,
                                               const PangoFontDescription *font_desc)
{
	GtkWidget *iter;

	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	g_clear_pointer (&self->font_attrs, pango_attr_list_unref);

	if (font_desc)
	{
		self->font_attrs = pango_attr_list_new ();

		if (font_desc != NULL)
		{
			pango_attr_list_insert (self->font_attrs, pango_attr_font_desc_new (font_desc));
			pango_attr_list_insert (self->font_attrs, pango_attr_font_features_new ("tnum"));
		}
	}

	for ((iter = gtk_widget_get_first_child (GTK_WIDGET (self->box)));
	     iter != NULL;
	     iter = gtk_widget_get_next_sibling (iter))
	{
		if (!GTK_SOURCE_IS_COMPLETION_LIST_BOX_ROW (iter))
		{
			continue;
		}

		_gtk_source_completion_list_box_row_set_attrs (GTK_SOURCE_COMPLETION_LIST_BOX_ROW (iter),
		                                               self->font_attrs);
	}
}

int
_gtk_source_completion_list_box_get_alternate (GtkSourceCompletionListBox *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), 0);

	return self->alternate + 1;
}

guint
_gtk_source_completion_list_box_get_n_alternates (GtkSourceCompletionListBox *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self), 0);

	return self->alternates ? self->alternates->len : 0;
}

void
_gtk_source_completion_list_box_set_show_icons (GtkSourceCompletionListBox *self,
                                                gboolean                    show_icons)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_LIST_BOX (self));

	self->show_icons = !!show_icons;

	gtk_source_completion_list_box_queue_update (self);
}

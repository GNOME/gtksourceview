/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include <string.h>

#include "gtksourcebuffer.h"
#include "gtksourcesearchcontext.h"
#include "gtksourcesearchsettings.h"
#include "gtksourceutils-private.h"
#include "gtksourceview.h"

#include "gtksourcevimjumplist.h"
#include "gtksourcevimregisters.h"
#include "gtksourcevimmarks.h"
#include "gtksourcevimstate.h"

typedef struct
{
	/* Owned reference to marks/registers (usually set low in the stack) */
	GtkSourceVimState *registers;
	GtkSourceVimState *marks;
	GtkSourceVimState *jumplist;

	/* Owned reference to the view (usually set low in the stack) */
	GtkSourceView *view;

	/* The name of the register set with "<name> */
	const char *current_register;

	/* Unowned parent pointer. The parent owns a reference to this
	 * instance of GtkSourceVimState.
	 */
	GtkSourceVimState *parent;

	/* Unowned pointer to a child that has been pushed onto our
	 * stack of states. @child must exist within @children.
	 */
	GtkSourceVimState *child;

	/* We have a custom search context/settings just for our VIM */
	GtkSourceSearchContext *search_context;
	GtkSourceSearchSettings *search_settings;

	/* A queue of all our children, using @link of the children nodes
	 * to insert into the queue without extra allocations.
	 */
	GQueue children;

	/* The GList to be inserted into @children of the parent. */
	GList link;

	/* A count if one has been associated with the state object */
	int count;

	/* The column we last were on. Usually this is just set by the
	 * Normal state but could also be set in others (like Visual).
	 */
	guint column;

	/* Various flags */
	guint count_set : 1;
	guint can_repeat : 1;
	guint column_set : 1;
	guint reverse_search : 1;
} GtkSourceVimStatePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkSourceVimState, gtk_source_vim_state, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_PARENT,
	PROP_VIEW,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

void
gtk_source_vim_state_keyval_unescaped (guint           keyval,
                                       GdkModifierType mods,
                                       char            str[16])
{
#define return_str(v) \
	G_STMT_START { g_strlcpy (str, v, 16); return; } G_STMT_END

	str[0] = 0;

	if (keyval == GDK_KEY_Escape)
		return_str ("\033");

	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_l:
				return_str ("\f");

			case GDK_KEY_a:
				return_str ("\a");

			default:
				break;
		}
	}

	switch (keyval)
	{
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
		case GDK_KEY_ISO_Left_Tab:
			return_str ("\t");

		case GDK_KEY_BackSpace:
			return_str ("\b");

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			return_str ("\n");

		default:
			break;
	}

	gtk_source_vim_state_keyval_to_string (keyval, mods, str);

#undef return_str
}

void
gtk_source_vim_state_keyval_to_string (guint           keyval,
                                       GdkModifierType mods,
                                       char            str[16])
{
	int pos = 0;

	if (keyval && (mods & GDK_CONTROL_MASK) != 0)
	{
		str[pos++] = '^';
	}

	switch (keyval)
	{
		case GDK_KEY_Escape:
			str[pos++] = '^';
			str[pos++] = '[';
			break;

		case GDK_KEY_BackSpace:
			str[pos++] = '^';
			str[pos++] = 'H';
			break;

		case GDK_KEY_ISO_Left_Tab:
		case GDK_KEY_Tab:
			str[pos++] = '\\';
			str[pos++] = 't';
			break;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			str[pos++] = '\\';
			str[pos++] = 'n';
			break;

		default:
		{
			gunichar ch;

			/* ctrl things like ^M ^L are all uppercase */
			if ((mods & GDK_CONTROL_MASK) != 0)
				ch = gdk_keyval_to_unicode (gdk_keyval_to_upper (keyval));
			else
				ch = gdk_keyval_to_unicode (keyval);

			pos += g_unichar_to_utf8 (ch, &str[pos]);

			break;
		}
	}

	str[pos] = 0;
}

static gboolean
gtk_source_vim_state_real_handle_event (GtkSourceVimState *self,
                                        GdkEvent          *event)
{
	g_assert (GTK_SOURCE_IS_VIM_STATE (self));
	g_assert (event != NULL);

	if (gdk_event_get_event_type (event) != GDK_KEY_PRESS)
	{
		return FALSE;
	}

	/* Ignore shift/control/etc keyvals */
	switch (gdk_key_event_get_keyval (event))
	{
		case GDK_KEY_Shift_L:
		case GDK_KEY_Shift_R:
		case GDK_KEY_Shift_Lock:
		case GDK_KEY_Caps_Lock:
		case GDK_KEY_ISO_Lock:
		case GDK_KEY_Control_L:
		case GDK_KEY_Control_R:
		case GDK_KEY_Meta_L:
		case GDK_KEY_Meta_R:
		case GDK_KEY_Alt_L:
		case GDK_KEY_Alt_R:
		case GDK_KEY_Super_L:
		case GDK_KEY_Super_R:
		case GDK_KEY_Hyper_L:
		case GDK_KEY_Hyper_R:
		case GDK_KEY_ISO_Level3_Shift:
		case GDK_KEY_ISO_Next_Group:
		case GDK_KEY_ISO_Prev_Group:
		case GDK_KEY_ISO_First_Group:
		case GDK_KEY_ISO_Last_Group:
		case GDK_KEY_Mode_switch:
		case GDK_KEY_Num_Lock:
		case GDK_KEY_Multi_key:
		case GDK_KEY_Scroll_Lock:
			return FALSE;

		default:
			break;
	}

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_keypress != NULL)
	{
		guint keyval;
		guint keycode;
		GdkModifierType mods;
		char string[16];

		keyval = gdk_key_event_get_keyval (event);
		keycode = gdk_key_event_get_keycode (event);
		mods = gdk_event_get_modifier_state (event)
		     & gtk_accelerator_get_default_mod_mask ();
		gtk_source_vim_state_keyval_to_string (keyval, mods, string);

		return GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_keypress (self, keyval, keycode, mods, string);
	}

	return FALSE;
}

static void
gtk_source_vim_state_real_resume (GtkSourceVimState *self,
                                  GtkSourceVimState *from)
{
	g_assert (GTK_SOURCE_IS_VIM_STATE (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	gtk_source_vim_state_unparent (from);
}

static void
gtk_source_vim_state_dispose (GObject *object)
{
	GtkSourceVimState *self = (GtkSourceVimState *)object;
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	if (priv->child)
	{
		g_object_run_dispose (G_OBJECT (priv->child));
	}

	if (priv->parent != NULL &&
	    gtk_source_vim_state_get_child (priv->parent) == self)
	{
		gtk_source_vim_state_pop (self);
	}

	priv->current_register = NULL;

	g_clear_object (&priv->search_context);
	g_clear_object (&priv->search_settings);

	gtk_source_vim_state_release (&priv->registers);
	gtk_source_vim_state_release (&priv->marks);
	gtk_source_vim_state_release (&priv->jumplist);

	/* First remove the children from our list */
	while (priv->children.length > 0)
	{
		GtkSourceVimState *child = g_queue_peek_head (&priv->children);
		gtk_source_vim_state_unparent (child);
	}

	/* Now make sure we're unlinked from our parent */
	if (priv->parent != NULL)
	{
		gtk_source_vim_state_unparent (self);
	}

	g_assert (priv->parent == NULL);
	g_assert (priv->children.length == 0);
	g_assert (priv->children.head == NULL);
	g_assert (priv->children.tail == NULL);

	g_clear_weak_pointer (&priv->view);

	G_OBJECT_CLASS (gtk_source_vim_state_parent_class)->dispose (object);
}

static void
gtk_source_vim_state_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	GtkSourceVimState *self = GTK_SOURCE_VIM_STATE (object);
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	switch (prop_id)
	{
		case PROP_PARENT:
			g_value_set_object (value, priv->parent);
			break;

		case PROP_VIEW:
			g_value_set_object (value, priv->view);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_state_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	GtkSourceVimState *self = GTK_SOURCE_VIM_STATE (object);
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	switch (prop_id)
	{
		case PROP_PARENT:
			gtk_source_vim_state_set_parent (self, g_value_get_object (value));
			break;

		case PROP_VIEW:
			g_set_weak_pointer (&priv->view, g_value_get_object (value));

			if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->view_set)
			{
				GTK_SOURCE_VIM_STATE_GET_CLASS (self)->view_set (self);
			}

			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_state_class_init (GtkSourceVimStateClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_state_dispose;
	object_class->get_property = gtk_source_vim_state_get_property;
	object_class->set_property = gtk_source_vim_state_set_property;

	klass->handle_event = gtk_source_vim_state_real_handle_event;
	klass->resume = gtk_source_vim_state_real_resume;

	properties [PROP_PARENT] =
		g_param_spec_object ("parent",
		                     "Parent",
		                     "The parent state",
		                     GTK_SOURCE_TYPE_VIM_STATE,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	properties [PROP_VIEW] =
		g_param_spec_object ("view",
		                     "View",
		                     "The source view",
		                     GTK_SOURCE_TYPE_VIEW,
		                     (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_vim_state_init (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	priv->link.data = self;
	priv->count = 1;
}

GtkSourceView *
gtk_source_vim_state_get_view (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	if (priv->view)
		return priv->view;

	if (priv->parent == NULL)
		return NULL;

	return gtk_source_vim_state_get_view (priv->parent);
}

GtkSourceBuffer *
gtk_source_vim_state_get_buffer (GtkSourceVimState *self,
                                 GtkTextIter       *insert,
                                 GtkTextIter       *selection_bound)
{
	GtkSourceView *view;
	GtkTextBuffer *buffer;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	if (!(view = gtk_source_vim_state_get_view (self)))
		return NULL;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

	g_assert (GTK_SOURCE_IS_BUFFER (buffer));

	if (insert != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (buffer, insert, gtk_text_buffer_get_insert (buffer));
	}

	if (selection_bound != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (buffer, selection_bound, gtk_text_buffer_get_selection_bound (buffer));
	}

	return GTK_SOURCE_BUFFER (buffer);
}

void
gtk_source_vim_state_beep (GtkSourceVimState *self)
{
	GtkSourceView *view;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if ((view = gtk_source_vim_state_get_view (self)))
	{
		gtk_widget_error_bell (GTK_WIDGET (view));
	}
}

GtkSourceVimState *
gtk_source_vim_state_get_child (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	return priv->child;
}

GtkSourceVimState *
gtk_source_vim_state_get_current (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	if (priv->child == NULL)
		return self;

	return gtk_source_vim_state_get_current (priv->child);
}

GtkSourceVimState *
gtk_source_vim_state_get_parent (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	return priv->parent;
}

GtkSourceVimState *
gtk_source_vim_state_get_root (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	if (priv->parent == NULL)
		return self;

	return gtk_source_vim_state_get_root (priv->parent);
}

void
gtk_source_vim_state_repeat (GtkSourceVimState *self)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->repeat)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (self)->repeat (self);
	}
}

gboolean
gtk_source_vim_state_handle_event (GtkSourceVimState *self,
                                   GdkEvent          *event)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_event)
	{
		return GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_event (self, event);
	}

	return FALSE;
}

/**
 * gtk_source_vim_state_push:
 * @self: a #GtkSourceVimState
 * @new_state: (transfer full): the new child state for @self
 *
 * Pushes @new_state as the current child of @self.
 *
 * This steals a reference from @new_state to simplify use.
 * Remember to g_object_ref(new_state) if you need to keep
 * a reference.
 */
void
gtk_source_vim_state_push (GtkSourceVimState *self,
                           GtkSourceVimState *new_state)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (new_state));
	g_return_if_fail (gtk_source_vim_state_get_parent (new_state) == NULL);

	if (priv->child != NULL)
	{
		g_warning ("Attempt to push state %s onto %s when it already has a %s",
		           G_OBJECT_TYPE_NAME (new_state),
		           G_OBJECT_TYPE_NAME (self),
		           G_OBJECT_TYPE_NAME (priv->child));
	}

	gtk_source_vim_state_set_parent (new_state, self);

	priv->child = new_state;

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->suspend)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (self)->suspend (self, new_state);
	}

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (new_state)->enter)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (new_state)->enter (new_state);
	}

	g_object_unref (new_state);
}

void
gtk_source_vim_state_pop (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);
	GtkSourceVimStatePrivate *parent_priv;
	GtkSourceVimState *parent;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (priv->child == NULL);
	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (priv->parent));

	parent = g_object_ref (priv->parent);
	parent_priv = gtk_source_vim_state_get_instance_private (parent);

	if (parent_priv->child == self)
	{
		parent_priv->child = NULL;
	}
	else
	{
		g_warning ("Attempt to pop state %s from %s but it is not current",
		           G_OBJECT_TYPE_NAME (self),
		           G_OBJECT_TYPE_NAME (parent));
	}

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->leave)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (self)->leave (self);
	}

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (parent)->resume)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (parent)->resume (parent, self);
	}

	g_object_unref (parent);
}

void
gtk_source_vim_state_set_overwrite (GtkSourceVimState *self,
                                    gboolean           overwrite)
{
	GtkSourceView *view;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);

	if (view != NULL)
	{
		gtk_text_view_set_overwrite (GTK_TEXT_VIEW (view), overwrite);
	}
}

gboolean
gtk_source_vim_state_synthesize (GtkSourceVimState *self,
                                 guint              keyval,
                                 GdkModifierType    mods)
{
	char string[16];

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	gtk_source_vim_state_keyval_to_string (keyval, mods, string);

	return GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_keypress (self, keyval, 0, mods, string);
}

void
gtk_source_vim_state_select (GtkSourceVimState *self,
                             const GtkTextIter *insert,
                             const GtkTextIter *selection)
{
	GtkSourceView *view;
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (insert != NULL);

	if (selection == NULL)
		selection = insert;

	view = gtk_source_vim_state_get_view (self);
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	gtk_text_buffer_select_range (buffer, insert, selection);
}

int
gtk_source_vim_state_get_visible_lines (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GtkTextIter begin, end;
	GdkRectangle rect;
	guint bline, eline;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), 2);

	view = gtk_source_vim_state_get_view (self);

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &begin, rect.x, rect.y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &end, rect.x, rect.y + rect.height);

	bline = gtk_text_iter_get_line (&begin);
	eline = gtk_text_iter_get_line (&end);

	return MAX (2, eline - bline);
}

void
gtk_source_vim_state_scroll_line (GtkSourceVimState *self,
                                  int                count)
{
	GtkSourceView *view;
	GdkRectangle rect;
	GtkTextIter top;
	int y, height;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (count == 0)
		count = 1;

	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &top, 0, rect.y);
	gtk_text_view_get_line_yrange (GTK_TEXT_VIEW (view), &top, &y, &height);

	/* Add a line is slightly visible. Works in both directions */
	if (y < rect.y && (rect.y - y) > (height / 2))
	{
		if (count > 0)
		{
			count++;
		}
	}

	if (count > 0)
		gtk_text_iter_forward_lines (&top, count);
	else
		gtk_text_iter_backward_lines (&top, -count);

	_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (view), &top, 0.0, TRUE, 1.0, 0.0);

	gtk_source_vim_state_place_cursor_onscreen (self);
}

static void
scroll_half_page_down (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GdkRectangle rect;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, rect.x, rect.y + rect.height / 2);
	_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (view), &iter, 0.0, TRUE, 1.0, 0.0);
}

static void
scroll_half_page_up (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GdkRectangle rect;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, rect.x, rect.y + rect.height / 2);
	_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (view), &iter, 0.0, TRUE, 1.0, 1.0);
}

void
gtk_source_vim_state_scroll_half_page (GtkSourceVimState *self,
                                       int                count)
{
	GtkSourceView *view;
	GdkRectangle rect, loc;
	GtkTextIter iter;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (count == 0)
		count = 1;

	gtk_source_vim_state_get_buffer (self, &iter, NULL);
	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &loc);
	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
	                                       GTK_TEXT_WINDOW_TEXT,
	                                       loc.x, loc.y, &loc.x, &loc.y);

	for (int i = 1; i <= ABS (count); i++)
	{
		if (count > 0)
			scroll_half_page_down (self);
		else
			scroll_half_page_up (self);
	}

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
	                                       GTK_TEXT_WINDOW_TEXT,
	                                       loc.x, loc.y, &loc.x, &loc.y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, loc.x, loc.y);
	gtk_source_vim_state_select (self, &iter, &iter);

	gtk_source_vim_state_place_cursor_onscreen (self);
}

static void
scroll_page_down (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GdkRectangle rect;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, rect.x, rect.y+rect.height);
	_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (view), &iter, 0.0, TRUE, 1.0, 0.0);
}

static void
scroll_page_up (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GdkRectangle rect;
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &iter, rect.x, rect.y);
	_gtk_source_view_jump_to_iter (GTK_TEXT_VIEW (view), &iter, 0.0, TRUE, 1.0, 1.0);
}

void
gtk_source_vim_state_scroll_page (GtkSourceVimState *self,
                                  int                count)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (count == 0)
		count = 1;

	for (int i = 1; i <= ABS (count); i++)
	{
		if (count > 0)
			scroll_page_down (self);
		else
			scroll_page_up (self);
	}

	gtk_source_vim_state_place_cursor_onscreen (self);
}

void
gtk_source_vim_state_place_cursor_onscreen (GtkSourceVimState *self)
{
	GtkSourceView *view;
	GtkTextIter iter;
	GdkRectangle rect, loc;
	gboolean move_insert = FALSE;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	view = gtk_source_vim_state_get_view (self);

	gtk_source_vim_state_get_buffer (self, &iter, NULL);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &loc);

	if (loc.y < rect.y)
	{
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
		                                    &iter, rect.x, rect.y);
		move_insert = TRUE;
	}
	else if (loc.y + loc.height > rect.y + rect.height)
	{

		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
		                                    &iter, rect.x, rect.y + rect.height);
		gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), &iter, &loc);
		if (loc.y + loc.height > rect.y + rect.height)
			gtk_text_iter_backward_line (&iter);
		move_insert = TRUE;
	}

	if (move_insert)
	{
		while (!gtk_text_iter_ends_line (&iter) &&
		       g_unichar_isspace (gtk_text_iter_get_char (&iter)))
		{
		       gtk_text_iter_forward_char (&iter);
		}
		gtk_source_vim_state_select (self, &iter, &iter);
	}
}

void
gtk_source_vim_state_z_scroll (GtkSourceVimState *self,
                               double             yalign)
{
	GtkSourceView *view;
	GtkTextIter iter;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	gtk_source_vim_state_get_buffer (self, &iter, NULL);
	view = gtk_source_vim_state_get_view (self);

	gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (view), &iter, 0, TRUE, 1.0, yalign);
}

void
gtk_source_vim_state_append_command (GtkSourceVimState *self,
                                     GString           *string)
{
	GtkSourceVimState *child;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->append_command)
	{
		GTK_SOURCE_VIM_STATE_GET_CLASS (self)->append_command (self, string);
	}

	child = gtk_source_vim_state_get_child (self);

	if (child != NULL)
	{
		gtk_source_vim_state_append_command (child, string);
	}
}

int
gtk_source_vim_state_get_count (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), 0);

	return priv->count;
}

void
gtk_source_vim_state_set_count (GtkSourceVimState *self,
                                int                count)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	priv->count = count ? count : 1;
	priv->count_set = count != 0;
}

void
gtk_source_vim_state_unparent (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);
	GtkSourceVimStatePrivate *parent_priv;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (priv->link.data == self);

	if (priv->parent == NULL)
	{
		return;
	}

	parent_priv = gtk_source_vim_state_get_instance_private (priv->parent);
	priv->parent = NULL;

	if (parent_priv->child == self)
	{
		parent_priv->child = NULL;
	}

	g_queue_unlink (&parent_priv->children, &priv->link);

	g_object_unref (self);
}

void
gtk_source_vim_state_set_parent (GtkSourceVimState *self,
                                 GtkSourceVimState *parent)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);
	GtkSourceVimStatePrivate *parent_priv;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (!parent || GTK_SOURCE_IS_VIM_STATE (parent));

	if (priv->parent == parent)
		return;

	g_object_ref (self);

	if (priv->parent != NULL)
	{
		gtk_source_vim_state_unparent (self);
	}

	g_assert (priv->parent == NULL);
	g_assert (priv->link.data == self);
	g_assert (priv->link.next == NULL);
	g_assert (priv->link.prev == NULL);

	if (parent != NULL)
	{
		priv->parent = parent;
		parent_priv = gtk_source_vim_state_get_instance_private (parent);
		g_queue_push_tail_link (&parent_priv->children, &priv->link);
		g_object_ref (self);
	}

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PARENT]);

	g_object_unref (self);
}

gboolean
gtk_source_vim_state_get_count_set (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	return priv->count_set;
}

void
gtk_source_vim_state_begin_user_action (GtkSourceVimState *self)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	buffer = gtk_source_vim_state_get_buffer (self, NULL, NULL);
	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
}

void
gtk_source_vim_state_end_user_action (GtkSourceVimState *self)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	buffer = gtk_source_vim_state_get_buffer (self, NULL, NULL);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));
}

gboolean
gtk_source_vim_state_get_can_repeat (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	return priv->can_repeat;
}

void
gtk_source_vim_state_set_can_repeat (GtkSourceVimState *self,
                                     gboolean           can_repeat)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	priv->can_repeat = !!can_repeat;
}

GtkSourceVimState *
gtk_source_vim_state_get_registers (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv;
	GtkSourceVimState *root;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);

	if (priv->registers == NULL)
	{
		priv->registers = gtk_source_vim_registers_new ();
		gtk_source_vim_state_set_parent (priv->registers, GTK_SOURCE_VIM_STATE (root));
	}

	return priv->registers;
}

const char *
gtk_source_vim_state_get_current_register (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	if (priv->current_register != NULL)
	{
		return priv->current_register;
	}

	if (priv->parent != NULL)
	{
		return gtk_source_vim_state_get_current_register (priv->parent);
	}

	return NULL;
}

void
gtk_source_vim_state_set_current_register (GtkSourceVimState *self,
                                           const char        *current_register)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (g_strcmp0 (priv->current_register, current_register) != 0)
	{
		priv->current_register = g_intern_string (current_register);
	}
}

const char *
gtk_source_vim_state_get_current_register_value (GtkSourceVimState *self)
{
	GtkSourceVimState *registers;
	const char *current_register;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);

	current_register = gtk_source_vim_state_get_current_register (self);
	registers = gtk_source_vim_state_get_registers (self);

	return gtk_source_vim_registers_get (GTK_SOURCE_VIM_REGISTERS (registers), current_register);
}

void
gtk_source_vim_state_set_current_register_value (GtkSourceVimState *self,
                                                 const char        *value)
{
	GtkSourceVimState *registers;
	const char *current_register;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	current_register = gtk_source_vim_state_get_current_register (self);
	registers = gtk_source_vim_state_get_registers (self);

	if (!gtk_source_vim_register_is_read_only (current_register))
	{
		gtk_source_vim_registers_set (GTK_SOURCE_VIM_REGISTERS (registers),
		                              current_register,
		                              value);
	}
}

guint
gtk_source_vim_state_get_visual_column (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);
	GtkSourceView *view;
	GtkTextIter iter;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	if (priv->column_set)
	{
		return priv->column;
	}

	if (priv->parent != NULL)
	{
		return gtk_source_vim_state_get_visual_column (priv->parent);
	}

	view = gtk_source_vim_state_get_view (self);
	gtk_source_vim_state_get_buffer (self, &iter, NULL);

	return gtk_source_view_get_visual_column (view, &iter);
}

void
gtk_source_vim_state_set_visual_column (GtkSourceVimState *self,
                                        int                visual_column)
{
	GtkSourceVimStatePrivate *priv = gtk_source_vim_state_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if (visual_column < 0)
	{
		priv->column_set = FALSE;
		return;
	}

	priv->column = visual_column;
	priv->column_set = TRUE;
}

static void
extend_lines (GtkTextIter *a,
              GtkTextIter *b)
{
	if (gtk_text_iter_compare (a, b) <= 0)
	{
		gtk_text_iter_set_line_offset (a, 0);
		if (!gtk_text_iter_ends_line (b))
			gtk_text_iter_forward_to_line_end (b);
		if (gtk_text_iter_ends_line (b) && !gtk_text_iter_is_end (b))
			gtk_text_iter_forward_char (b);
	}
	else
	{
		gtk_text_iter_set_line_offset (b, 0);
		if (!gtk_text_iter_ends_line (a))
			gtk_text_iter_forward_to_line_end (a);
		if (gtk_text_iter_ends_line (a) && !gtk_text_iter_is_end (a))
			gtk_text_iter_forward_char (a);
	}
}

void
gtk_source_vim_state_select_linewise (GtkSourceVimState *self,
                                      GtkTextIter       *insert,
                                      GtkTextIter       *selection)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter1, iter2;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	buffer = gtk_source_vim_state_get_buffer (self, &iter1, &iter2);

	if (insert == NULL)
		insert = &iter1;

	if (selection == NULL)
		selection = &iter2;

	extend_lines (insert, selection);

	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), insert, selection);
}

gboolean
gtk_source_vim_state_get_editable (GtkSourceVimState *self)
{
	GtkSourceView *view;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	view = gtk_source_vim_state_get_view (self);

	return gtk_text_view_get_editable (GTK_TEXT_VIEW (view));
}

void
gtk_source_vim_state_get_search (GtkSourceVimState        *self,
                                 GtkSourceSearchSettings **settings,
                                 GtkSourceSearchContext  **context)
{
	GtkSourceVimStatePrivate *priv;
	GtkSourceVimState *root;
	GtkSourceBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);
	buffer = gtk_source_vim_state_get_buffer (self, NULL, NULL);

	if (priv->search_settings == NULL)
	{
		priv->search_settings = gtk_source_search_settings_new ();
		gtk_source_search_settings_set_wrap_around (priv->search_settings, TRUE);
		gtk_source_search_settings_set_regex_enabled (priv->search_settings, TRUE);
		gtk_source_search_settings_set_case_sensitive (priv->search_settings, TRUE);
	}

	if (priv->search_context == NULL)
	{
		priv->search_context = gtk_source_search_context_new (buffer, priv->search_settings);
		gtk_source_search_context_set_highlight (priv->search_context, TRUE);
	}

	if (settings != NULL)
	{
		*settings = priv->search_settings;
	}

	if (context != NULL)
	{
		*context = priv->search_context;
	}
}

gboolean
gtk_source_vim_state_get_reverse_search (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv;
	GtkSourceVimState *root;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);

	return priv->reverse_search;
}

void
gtk_source_vim_state_set_reverse_search (GtkSourceVimState *self,
                                         gboolean           reverse_search)
{
	GtkSourceVimStatePrivate *priv;
	GtkSourceVimState *root;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);

	priv->reverse_search = !!reverse_search;
}

static GtkSourceVimMarks *
gtk_source_vim_state_get_marks (GtkSourceVimState *self)
{
	GtkSourceVimStatePrivate *priv;
	GtkSourceVimState *root;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);

	if (priv->marks == NULL)
	{
		priv->marks = gtk_source_vim_marks_new ();
		gtk_source_vim_state_set_parent (GTK_SOURCE_VIM_STATE (priv->marks), root);
	}

	return GTK_SOURCE_VIM_MARKS (priv->marks);
}

GtkTextMark *
gtk_source_vim_state_get_mark (GtkSourceVimState *self,
                               const char        *name)
{
	GtkSourceVimMarks *marks;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	marks = gtk_source_vim_state_get_marks (self);

	return gtk_source_vim_marks_get_mark (marks, name);
}

void
gtk_source_vim_state_set_mark (GtkSourceVimState *self,
                               const char        *name,
                               const GtkTextIter *iter)
{
	GtkSourceVimMarks *marks;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (name != NULL);

	marks = gtk_source_vim_state_get_marks (self);

	gtk_source_vim_marks_set_mark (marks, name, iter);
}

gboolean
gtk_source_vim_state_get_iter_at_mark (GtkSourceVimState *self,
                                       const char        *name,
                                       GtkTextIter       *iter)
{
	GtkSourceVimMarks *marks;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	marks = gtk_source_vim_state_get_marks (self);

	return gtk_source_vim_marks_get_iter (marks, name, iter);
}

static GtkSourceVimJumplist *
gtk_source_vim_state_get_jumplist (GtkSourceVimState *self)
{
	GtkSourceVimState *root;
	GtkSourceVimStatePrivate *priv;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	root = gtk_source_vim_state_get_root (self);
	priv = gtk_source_vim_state_get_instance_private (root);

	if (priv->jumplist == NULL)
	{
		priv->jumplist = gtk_source_vim_jumplist_new ();
		gtk_source_vim_state_set_parent (priv->jumplist, root);
	}

	return GTK_SOURCE_VIM_JUMPLIST (priv->jumplist);
}

void
gtk_source_vim_state_push_jump (GtkSourceVimState *self,
                                const GtkTextIter *iter)
{
	GtkSourceVimJumplist *jumplist;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));
	g_return_if_fail (iter != NULL);

	jumplist = gtk_source_vim_state_get_jumplist (self);
	gtk_source_vim_jumplist_push (jumplist, iter);
}

gboolean
gtk_source_vim_state_jump_backward (GtkSourceVimState *self,
                                    GtkTextIter       *iter)
{
	GtkSourceVimJumplist *jumplist;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	jumplist = gtk_source_vim_state_get_jumplist (self);

	return gtk_source_vim_jumplist_previous (jumplist, iter);
}

gboolean
gtk_source_vim_state_jump_forward (GtkSourceVimState *self,
                                   GtkTextIter       *iter)
{
	GtkSourceVimJumplist *jumplist;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (self), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	jumplist = gtk_source_vim_state_get_jumplist (self);

	return gtk_source_vim_jumplist_next (jumplist, iter);
}

void
gtk_source_vim_state_scroll_insert_onscreen (GtkSourceVimState *self)
{
	GtkSourceView *view;

	g_return_if_fail (GTK_SOURCE_IS_VIM_STATE (self));

	if ((view = gtk_source_vim_state_get_view (self)))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		GtkTextMark *mark = gtk_text_buffer_get_insert (buffer);

		gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view), mark);
	}
}

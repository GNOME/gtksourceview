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

#include <glib/gi18n.h>

#include "gtksourcecompletion-private.h"
#include "gtksourceindenter.h"
#include "gtksourceview.h"

#include "gtksourceviminsert.h"
#include "gtksourceviminsertliteral.h"
#include "gtksourcevimreplace.h"
#include "gtksourcevimtexthistory.h"

struct _GtkSourceVimInsert
{
	GtkSourceVimState        parent_instance;
	GtkSourceVimTextHistory *history;
	GtkSourceVimMotion      *motion;
	GtkSourceVimMotion      *selection_motion;
	GtkSourceVimTextObject  *text_object;
	char                    *prefix;
	char                    *suffix;
	GtkSourceVimInsertAt     at;
	guint                    indent : 1;
	guint                    finished : 1;
};

G_DEFINE_TYPE (GtkSourceVimInsert, gtk_source_vim_insert, GTK_SOURCE_TYPE_VIM_STATE)

enum {
	PROP_0,
	PROP_INDENT,
	PROP_PREFIX,
	PROP_SUFFIX,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

GtkSourceVimState *
gtk_source_vim_insert_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_INSERT, NULL);
}

static gboolean
clear_to_first_char (GtkSourceVimInsert *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter insert;
	GtkTextIter begin;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &insert, NULL);
	begin = insert;
	gtk_text_iter_set_line_offset (&begin, 0);

	while (gtk_text_iter_compare (&begin, &insert) < 0 &&
	       g_unichar_isspace (gtk_text_iter_get_char (&begin)))
	{
		gtk_text_iter_forward_char (&begin);
	}

	if (gtk_text_iter_equal (&begin, &insert))
	{
		gtk_text_iter_set_line_offset (&begin, 0);
	}

	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &begin, &insert);

	return TRUE;
}

static gboolean
gtk_source_vim_insert_handle_keypress (GtkSourceVimState *state,
                                       guint              keyval,
                                       guint              keycode,
                                       GdkModifierType    mods,
                                       const char        *string)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;
	GtkSourceView *view;

	g_assert (GTK_SOURCE_IS_VIM_STATE (state));
	g_assert (string != NULL);

	/* Leave insert mode if Escape,ctrl+[,ctrl+c was pressed */
	if (gtk_source_vim_state_is_escape (keyval, mods) ||
	    gtk_source_vim_state_is_ctrl_c (keyval, mods))
	{
		gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));
		return TRUE;
	}

	view = gtk_source_vim_state_get_view (state);

	/* Now handle our commands */
	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_u:
				if ((mods & GDK_SHIFT_MASK) == 0)
					return clear_to_first_char (self);
				break;

			case GDK_KEY_v:
				gtk_source_vim_state_push (state, gtk_source_vim_insert_literal_new ());
				return TRUE;

			case GDK_KEY_V:
				/* For the terminal users out there */
				g_signal_emit_by_name (view, "paste-clipboard");
				return TRUE;

			case GDK_KEY_n:
			case GDK_KEY_p:
			{
				GtkSourceCompletion *completion = gtk_source_view_get_completion (view);

				if (_gtk_source_completion_get_visible (completion))
				{
					_gtk_source_completion_move_cursor (completion,
									    GTK_MOVEMENT_DISPLAY_LINES,
									    keyval == GDK_KEY_n ? 1 : -1);
				}
				else
				{
					gtk_source_completion_show (completion);
				}

				return TRUE;
			}

			default:
				break;
		}
	}

	/* XXX: Currently we do not use overwrite mode while in insert even
	 * though that is the only way to get a block cursor. To do that we'd
	 * have to be able to commit text to the textview through the input
	 * method and we don't have a way to do that yet.
	 */

	switch (keyval)
	{
		case GDK_KEY_Insert:
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self),
			                           gtk_source_vim_replace_new ());
			return TRUE;

		default:
			return FALSE;
	}
}

static gboolean
gtk_source_vim_insert_handle_event (GtkSourceVimState *state,
                                    GdkEvent          *event)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;
	GtkSourceView *view;
	char string[16];
	GdkModifierType mods;
	guint keyval;
	guint keycode;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));
	g_assert (event != NULL);

	view = gtk_source_vim_state_get_view (state);

	/* We only handle keypress, otherwise defer to the normal event processing
	 * flow and/or input methods.
	 */
	if (view == NULL || gdk_event_get_event_type (event) != GDK_KEY_PRESS)
	{
		return FALSE;
	}

	/* gtk_text_view_im_context_filter_keypress() will always filter input that
	 * can be converted into an GtkIMContext::commit emission so we must check
	 * to see if any of our handlers will check first.
	 *
	 * This has a sort of annoying impact with the underlying input method that
	 * we could collide, but there doesn't seem to be much we can do about that.
	 *
	 * https://gitlab.gnome.org/GNOME/gtk/-/issues/5349
	 */
	keyval = gdk_key_event_get_keyval (event);
	keycode = gdk_key_event_get_keycode (event);
	mods = gdk_event_get_modifier_state (event)
	     & gtk_accelerator_get_default_mod_mask ();

	gtk_source_vim_state_keyval_to_string (keyval, mods, string);

	if (GTK_SOURCE_VIM_STATE_GET_CLASS (self)->handle_keypress (state, keyval, keycode, mods, string))
	{
		return TRUE;
	}

	return FALSE;
}

static void
gtk_source_vim_insert_prepare (GtkSourceVimInsert *self)
{
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter;
	GtkTextIter selection;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	if (self->text_object)
	{
		selection = iter;
		gtk_source_vim_text_object_select (self->text_object, &iter, &selection);
	}
	else
	{
		if (self->motion)
		{
			gtk_source_vim_motion_apply (self->motion, &iter, self->selection_motion != NULL);

			if (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR ||
			    self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_BOF)
			{
				if (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR ||
				    (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_BOF && !gtk_text_iter_is_start (&iter)) ||
				    (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_SOL && !gtk_text_iter_starts_line (&iter)))
				{
					if (!gtk_text_iter_ends_line (&iter))
						gtk_text_iter_forward_char (&iter);
				}
			}

			if (self->selection_motion == NULL)
			{
				selection = iter;
			}
		}

		if (self->selection_motion)
		{
			gtk_source_vim_motion_apply (self->selection_motion, &selection, TRUE);

			if (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR ||
			    self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_BOF)
			{
				if (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR ||
				    (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_BOF && !gtk_text_iter_is_start (&iter)) ||
				    (self->at == GTK_SOURCE_VIM_INSERT_AFTER_CHAR_UNLESS_SOL && !gtk_text_iter_starts_line (&iter)))
				{
					if (!gtk_text_iter_ends_line (&selection))
						gtk_text_iter_forward_char (&selection);
				}
			}
		}
	}

	gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	if (!gtk_text_iter_equal (&iter, &selection))
	{
		char *removed = gtk_text_iter_get_slice (&iter, &selection);

		if (((self->text_object && gtk_source_vim_text_object_is_linewise (self->text_object)) ||
		     (self->motion && gtk_source_vim_motion_is_linewise (self->motion))))
		{
			char *tmp = removed;
			removed = g_strdup_printf ("%s\n", tmp);
			g_free (tmp);
		}

		gtk_source_vim_state_set_current_register_value (GTK_SOURCE_VIM_STATE (self), removed);
		gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &selection);

		g_free (removed);
	}

	if (self->suffix)
	{
		gsize len = g_utf8_strlen (self->suffix, -1);

		if (len > 0)
		{
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, self->suffix, -1);
			gtk_text_iter_backward_chars (&iter, len);
			gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
			selection = iter;
		}
	}

	if (self->prefix)
	{
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, self->prefix, -1);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
	}

	if (self->indent && gtk_source_view_get_auto_indent (view))
	{
		GtkSourceIndenter *indenter = gtk_source_view_get_indenter (view);

		if (indenter != NULL)
		{
			gtk_source_indenter_indent (indenter, view, &iter);
		}
	}
}

static void
gtk_source_vim_insert_resume (GtkSourceVimState *state,
                              GtkSourceVimState *from)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (state));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	gtk_source_vim_state_set_overwrite (state, FALSE);

	if (GTK_SOURCE_IS_VIM_MOTION (from) && self->motion == NULL)
	{
		gtk_source_vim_state_reparent (from, self, &self->motion);
		gtk_source_vim_text_history_end (self->history);
		gtk_source_vim_insert_prepare (self);
		gtk_source_vim_text_history_begin (self->history);
		return;
	}
	else if (GTK_SOURCE_IS_VIM_REPLACE (from))
	{
		/* If we are leaving replace mode back to insert then
		 * we need also exit insert mode so we end up back on
		 * Normal mode.
		 */
		gtk_source_vim_state_unparent (from);
		gtk_source_vim_state_pop (state);
		return;
	}

	gtk_source_vim_state_unparent (from);
}

static void
gtk_source_vim_insert_enter (GtkSourceVimState *state)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;
	GtkSourceVimState *history;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));

	gtk_source_vim_state_begin_user_action (state);
	gtk_source_vim_state_set_overwrite (state, FALSE);

	history = gtk_source_vim_text_history_new ();
	gtk_source_vim_state_reparent (history, self, &self->history);
	gtk_source_vim_insert_prepare (self);
	gtk_source_vim_text_history_begin (self->history);

	gtk_source_vim_state_scroll_insert_onscreen (state);

	g_object_unref (history);
}

static void
gtk_source_vim_insert_leave (GtkSourceVimState *state)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));

	self->finished = TRUE;

	gtk_source_vim_text_history_end (self->history);

	count = gtk_source_vim_state_get_count (state);

	while (--count > 0)
	{
		gtk_source_vim_insert_prepare (self);
		gtk_source_vim_text_history_replay (self->history);
	}

	gtk_source_vim_state_end_user_action (state);
}

static void
gtk_source_vim_insert_repeat (GtkSourceVimState *state)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)state;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_INSERT (self));

	count = gtk_source_vim_state_get_count (state);

	gtk_source_vim_state_begin_user_action (state);

	for (int i = 0; i < count; i++)
	{
		gtk_source_vim_insert_prepare (self);
		gtk_source_vim_text_history_replay (self->history);
	}

	gtk_source_vim_state_end_user_action (state);
}

static void
gtk_source_vim_insert_append_command (GtkSourceVimState *state,
                                      GString           *string)
{
	/* command should be empty during insert */
	g_string_truncate (string, 0);
}

static void
gtk_source_vim_insert_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GtkSourceVimInsert *self = GTK_SOURCE_VIM_INSERT (object);

	switch (prop_id)
	{
		case PROP_INDENT:
			g_value_set_boolean (value, self->indent);
			break;

		case PROP_PREFIX:
			g_value_set_string (value, self->prefix);
			break;

		case PROP_SUFFIX:
			g_value_set_string (value, self->suffix);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_insert_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
	GtkSourceVimInsert *self = GTK_SOURCE_VIM_INSERT (object);

	switch (prop_id)
	{
		case PROP_INDENT:
			gtk_source_vim_insert_set_indent (self, g_value_get_boolean (value));
			break;

		case PROP_PREFIX:
			gtk_source_vim_insert_set_prefix (self, g_value_get_string (value));
			break;

		case PROP_SUFFIX:
			gtk_source_vim_insert_set_suffix (self, g_value_get_string (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_insert_dispose (GObject *object)
{
	GtkSourceVimInsert *self = (GtkSourceVimInsert *)object;

	G_OBJECT_CLASS (gtk_source_vim_insert_parent_class)->dispose (object);

	g_clear_pointer (&self->prefix, g_free);
	g_clear_pointer (&self->suffix, g_free);
	gtk_source_vim_state_release (&self->history);
	gtk_source_vim_state_release (&self->motion);
	gtk_source_vim_state_release (&self->selection_motion);
	gtk_source_vim_state_release (&self->text_object);
}

static void
gtk_source_vim_insert_class_init (GtkSourceVimInsertClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_insert_dispose;
	object_class->get_property = gtk_source_vim_insert_get_property;
	object_class->set_property = gtk_source_vim_insert_set_property;

	state_class->command_bar_text = _("-- INSERT --");
	state_class->append_command = gtk_source_vim_insert_append_command;
	state_class->handle_event = gtk_source_vim_insert_handle_event;
	state_class->handle_keypress = gtk_source_vim_insert_handle_keypress;
	state_class->resume = gtk_source_vim_insert_resume;
	state_class->enter = gtk_source_vim_insert_enter;
	state_class->leave = gtk_source_vim_insert_leave;
	state_class->repeat = gtk_source_vim_insert_repeat;

	properties [PROP_INDENT] =
		g_param_spec_boolean ("indent",
		                      "Indent",
		                      "Indent after the prefix text",
		                      FALSE,
		                      (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	properties [PROP_PREFIX] =
		g_param_spec_string ("prefix",
		                     "Prefix",
		                     "Text to insert at the insertion cursor before entering insert mode",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	properties [PROP_SUFFIX] =
		g_param_spec_string ("suffix",
		                     "suffix",
		                     "Text to insert after the insertion cursor before entering insert mode",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_vim_insert_init (GtkSourceVimInsert *self)
{
	self->at = GTK_SOURCE_VIM_INSERT_HERE;
	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);
}

void
gtk_source_vim_insert_set_prefix (GtkSourceVimInsert *self,
                                  const char         *prefix)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));

	if (g_set_str (&self->prefix, prefix))
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_PREFIX]);
}

void
gtk_source_vim_insert_set_suffix (GtkSourceVimInsert *self,
                                  const char         *suffix)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));

	if (g_set_str (&self->suffix, suffix))
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SUFFIX]);
}

void
gtk_source_vim_insert_set_indent (GtkSourceVimInsert *self,
                                  gboolean            indent)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));

	indent = !!indent;

	if (self->indent != indent)
	{
		self->indent = indent;
		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INDENT]);
	}
}

void
gtk_source_vim_insert_set_motion (GtkSourceVimInsert *self,
                                  GtkSourceVimMotion *motion)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));
	g_return_if_fail (GTK_SOURCE_IS_VIM_MOTION (motion));

	gtk_source_vim_state_reparent (motion, self, &self->motion);
}

void
gtk_source_vim_insert_set_selection_motion (GtkSourceVimInsert *self,
                                            GtkSourceVimMotion *selection_motion)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));
	g_return_if_fail (GTK_SOURCE_IS_VIM_MOTION (selection_motion));

	gtk_source_vim_state_reparent (selection_motion, self, &self->selection_motion);
}

void
gtk_source_vim_insert_set_at (GtkSourceVimInsert   *self,
                              GtkSourceVimInsertAt  at)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));

	self->at = at;
}

void
gtk_source_vim_insert_set_text_object (GtkSourceVimInsert     *self,
                                       GtkSourceVimTextObject *text_object)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_INSERT (self));

	gtk_source_vim_state_reparent (text_object, self, &self->text_object);
}

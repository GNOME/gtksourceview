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

#include "gtksourceview.h"

#include "gtksourcevim.h"
#include "gtksourcevimcharpending.h"
#include "gtksourcevimcommand.h"
#include "gtksourcevimcommandbar.h"
#include "gtksourceviminsert.h"
#include "gtksourcevimmotion.h"
#include "gtksourcevimnormal.h"
#include "gtksourcevimreplace.h"
#include "gtksourcevimtextobject.h"
#include "gtksourcevimvisual.h"

#define REPLAY(_block) do { _block; } while (--self->count > 0);

typedef gboolean (*KeyHandler) (GtkSourceVimNormal *self,
                                guint               keyval,
                                guint               keycode,
                                GdkModifierType     mods,
                                const char         *string);

typedef enum
{
	CHANGE_NONE  = 0,
	CHANGE_INNER = 1,
	CHANGE_A     = 2,
} ChangeModifier;

struct _GtkSourceVimNormal
{
	GtkSourceVimState  parent_instance;
	GString           *command_text;
	GtkSourceVimState *repeat;
	GtkSourceVimState *last_visual;
	KeyHandler         handler;
	int                count;
	ChangeModifier     change_modifier;
	guint              has_count : 1;
};

static gboolean key_handler_initial (GtkSourceVimNormal *self,
                                     guint               keyval,
                                     guint               keycode,
                                     GdkModifierType     mods,
                                     const char         *string);

G_DEFINE_TYPE (GtkSourceVimNormal, gtk_source_vim_normal, GTK_SOURCE_TYPE_VIM_STATE)

static void
gtk_source_vim_normal_emit_ready (GtkSourceVimNormal *self)
{
	GtkSourceVimState *parent;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	parent = gtk_source_vim_state_get_parent (GTK_SOURCE_VIM_STATE (self));

	if (GTK_SOURCE_IS_VIM (parent))
	{
		gtk_source_vim_emit_ready (GTK_SOURCE_VIM (parent));
	}
}

static gboolean
gtk_source_vim_normal_bail (GtkSourceVimNormal *self)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	gtk_source_vim_state_beep (GTK_SOURCE_VIM_STATE (self));
	gtk_source_vim_state_set_current_register (GTK_SOURCE_VIM_STATE (self), NULL);
	gtk_source_vim_normal_clear (self);

	return TRUE;
}

static GtkSourceVimState *
get_text_object (guint keyval,
                 guint change_modifier)
{
	switch (keyval)
	{
		case GDK_KEY_w:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_word ();
			else
				return gtk_source_vim_text_object_new_inner_word ();

		case GDK_KEY_W:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_WORD ();
			else
				return gtk_source_vim_text_object_new_inner_WORD ();

		case GDK_KEY_p:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_paragraph ();
			else
				return gtk_source_vim_text_object_new_inner_paragraph ();

		case GDK_KEY_s:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_sentence ();
			else
				return gtk_source_vim_text_object_new_inner_sentence ();

		case GDK_KEY_bracketright:
		case GDK_KEY_bracketleft:
			/* TODO: this needs to use separate mechanisms for [ vs ] */
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_block_bracket ();
			else
				return gtk_source_vim_text_object_new_inner_block_bracket ();

		case GDK_KEY_braceleft:
		case GDK_KEY_braceright:
			/* TODO: this needs to use separate mechanisms for { vs } */
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_block_brace ();
			else
				return gtk_source_vim_text_object_new_inner_block_brace ();

		case GDK_KEY_less:
		case GDK_KEY_greater:
			/* TODO: this needs to use separate mechanisms for < vs > */
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_block_brace ();
			else
				return gtk_source_vim_text_object_new_inner_block_brace ();

		case GDK_KEY_apostrophe:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_quote_single ();
			else
				return gtk_source_vim_text_object_new_inner_quote_single ();

		case GDK_KEY_quotedbl:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_quote_double ();
			else
				return gtk_source_vim_text_object_new_inner_quote_double ();

		case GDK_KEY_grave:
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_quote_grave ();
			else
				return gtk_source_vim_text_object_new_inner_quote_grave ();

		case GDK_KEY_parenleft:
		case GDK_KEY_parenright:
		case GDK_KEY_b:
			/* TODO: this needs to use separate mechanisms for ( vs ) */
			if (change_modifier == CHANGE_A)
				return gtk_source_vim_text_object_new_a_block_paren ();
			else
				return gtk_source_vim_text_object_new_inner_block_paren ();

		default:
			return NULL;
	}
}

static gboolean
gtk_source_vim_normal_replace_one (GtkSourceVimNormal *self)
{
	GtkSourceVimState *replace;
	GtkSourceVimState *char_pending;
	GtkSourceVimState *motion;
	GtkSourceVimState *selection_motion;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	count = self->count, self->count = 0;

	char_pending = gtk_source_vim_char_pending_new ();
	replace = gtk_source_vim_command_new ("replace-one");
	motion = gtk_source_vim_motion_new_forward_char ();
	selection_motion = gtk_source_vim_motion_new_none ();
	gtk_source_vim_state_set_count (motion, count);

	gtk_source_vim_command_set_motion (GTK_SOURCE_VIM_COMMAND (replace),
	                                   GTK_SOURCE_VIM_MOTION (motion));
	gtk_source_vim_command_set_selection_motion (GTK_SOURCE_VIM_COMMAND (replace),
	                                             GTK_SOURCE_VIM_MOTION (selection_motion));

	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), replace);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (replace), char_pending);

	g_object_unref (motion);
	g_object_unref (selection_motion);

	return TRUE;
}

G_GNUC_NULL_TERMINATED
static GtkSourceVimState *
gtk_source_vim_normal_begin_change (GtkSourceVimNormal *self,
                                    GtkSourceVimState  *insert_motion,
                                    GtkSourceVimState  *selection_motion,
                                    ...)
{
	GtkSourceVimState *ret;
	const char *first_property_name;
	va_list args;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (!insert_motion || GTK_SOURCE_IS_VIM_MOTION (insert_motion));
	g_assert (!selection_motion || GTK_SOURCE_IS_VIM_MOTION (selection_motion));

	count = self->count, self->count = 0;

	va_start (args, selection_motion);
	first_property_name = va_arg (args, const char *);
	ret = GTK_SOURCE_VIM_STATE (g_object_new_valist (GTK_SOURCE_TYPE_VIM_INSERT, first_property_name, args));
	va_end (args);

	if (insert_motion != NULL)
	{
		gtk_source_vim_state_set_count (insert_motion, count);
		gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (insert_motion), FALSE);
		gtk_source_vim_state_set_parent (insert_motion, ret);
		gtk_source_vim_insert_set_motion (GTK_SOURCE_VIM_INSERT (ret),
		                                  GTK_SOURCE_VIM_MOTION (insert_motion));
		g_object_unref (insert_motion);
	}

	if (selection_motion != NULL)
	{
		gtk_source_vim_state_set_count (selection_motion, count);
		gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (selection_motion), FALSE);
		gtk_source_vim_state_set_parent (selection_motion, ret);
		gtk_source_vim_insert_set_selection_motion (GTK_SOURCE_VIM_INSERT (ret),
		                                            GTK_SOURCE_VIM_MOTION (selection_motion));
		g_object_unref (selection_motion);
	}

	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), ret);

	return ret;
}

static GtkSourceVimState *
gtk_source_vim_normal_begin_insert_text_object (GtkSourceVimNormal *self,
                                                GtkSourceVimState  *text_object)
{
	GtkSourceVimState *ret;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (GTK_SOURCE_IS_VIM_TEXT_OBJECT (text_object));

	count = self->count;

	ret = gtk_source_vim_insert_new ();
	gtk_source_vim_state_set_parent (text_object, ret);
	gtk_source_vim_insert_set_text_object (GTK_SOURCE_VIM_INSERT (ret),
	                                       GTK_SOURCE_VIM_TEXT_OBJECT (text_object));
	gtk_source_vim_state_set_count (ret, count);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), ret);

	return ret;
}

G_GNUC_NULL_TERMINATED
static GtkSourceVimState *
gtk_source_vim_normal_begin_insert (GtkSourceVimNormal   *self,
                                    GtkSourceVimState    *motion,
                                    GtkSourceVimInsertAt  at,
                                    ...)
{
	GtkSourceVimState *ret;
	const char *first_property_name;
	va_list args;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (!motion || GTK_SOURCE_IS_VIM_MOTION (motion));

	count = self->count;

	va_start (args, at);
	first_property_name = va_arg (args, const char *);
	ret = GTK_SOURCE_VIM_STATE (g_object_new_valist (GTK_SOURCE_TYPE_VIM_INSERT, first_property_name, args));
	va_end (args);

	if (motion != NULL)
	{
		gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (motion), FALSE);
		gtk_source_vim_insert_set_at (GTK_SOURCE_VIM_INSERT (ret), at);
		gtk_source_vim_insert_set_motion (GTK_SOURCE_VIM_INSERT (ret),
		                                  GTK_SOURCE_VIM_MOTION (motion));
		g_object_unref (motion);
	}

	gtk_source_vim_state_set_count (ret, count);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), ret);

	return ret;
}

static void
gtk_source_vim_normal_begin_command (GtkSourceVimNormal *self,
                                     GtkSourceVimState  *insert_motion,
                                     GtkSourceVimState  *selection_motion,
                                     const char         *command_str,
                                     guint               linewise_keyval)
{
	GtkSourceVimCommand *command;
	gboolean pop_command = TRUE;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (!insert_motion || GTK_SOURCE_IS_VIM_MOTION (insert_motion));
	g_assert (!selection_motion || GTK_SOURCE_IS_VIM_MOTION (selection_motion));

	count = self->count, self->count = 0;

	if (insert_motion != NULL)
		gtk_source_vim_state_set_count (GTK_SOURCE_VIM_STATE (insert_motion), count);

	if (selection_motion)
		gtk_source_vim_state_set_count (GTK_SOURCE_VIM_STATE (selection_motion), count);

	command = g_object_new (GTK_SOURCE_TYPE_VIM_COMMAND,
	                        "motion", insert_motion,
	                        "selection-motion", selection_motion,
	                        "command", command_str,
	                        NULL);

	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self),
	                           GTK_SOURCE_VIM_STATE (command));

	/* If there is not yet a motion to apply, then that will get
	 * applied to the command as a whole (which will then in turn
	 * repeat motions).
	 */
	if (insert_motion == NULL)
	{
		gtk_source_vim_state_set_count (GTK_SOURCE_VIM_STATE (command), count);

		/* If we got a linewise keyval, then we want to let the motion
		 * know to use the gtk_source_vim_motion_new_down(-1) style
		 * motion. Generally for things like yy, dd, etc.
		 */
		if (linewise_keyval != 0)
		{
			insert_motion = gtk_source_vim_motion_new ();
			gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (insert_motion),
			                                          FALSE);
			gtk_source_vim_motion_set_linewise_keyval (GTK_SOURCE_VIM_MOTION (insert_motion),
			                                           linewise_keyval);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (command),
			                           g_object_ref (insert_motion));
			pop_command = FALSE;
		}
	}

	g_clear_object (&insert_motion);
	g_clear_object (&selection_motion);

	if (pop_command)
	{
		gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (command));
	}
}

static gboolean
gtk_source_vim_normal_begin_command_requiring_motion (GtkSourceVimNormal *self,
                                                      const char         *command_str)
{
	GtkSourceVimState *command;
	GtkSourceVimState *motion;
	GtkSourceVimState *selection_motion;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (command_str != NULL);

	motion = gtk_source_vim_motion_new ();
	selection_motion = gtk_source_vim_motion_new_none ();

	gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (motion), FALSE);

	command = g_object_new (GTK_SOURCE_TYPE_VIM_COMMAND,
	                        "selection-motion", selection_motion,
	                        "command", command_str,
	                        NULL);

	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), command);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (command), motion);

	g_clear_object (&selection_motion);

	return TRUE;
}

static void
gtk_source_vim_normal_begin_visual (GtkSourceVimNormal     *self,
                                    GtkSourceVimVisualMode  mode)
{
	GtkSourceVimState *visual;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	count = self->count, self->count = 0;

	visual = gtk_source_vim_visual_new (mode);
	gtk_source_vim_state_set_count (visual, count);

	gtk_source_vim_normal_clear (self);

	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), visual);
}

static void
go_backward_char (GtkSourceVimNormal *self)
{
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, NULL);
	if (!gtk_text_iter_starts_line (&iter) && gtk_text_iter_backward_char (&iter))
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
}

static void
keep_on_char (GtkSourceVimNormal *self)
{
	GtkTextIter iter;

	gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, NULL);

	if (gtk_text_iter_ends_line (&iter) && !gtk_text_iter_starts_line (&iter))
	{
		go_backward_char (self);
	}
}

static gboolean
key_handler_count (GtkSourceVimNormal *self,
                   guint               keyval,
                   guint               keycode,
                   GdkModifierType     mods,
                   const char         *string)
{
	int n;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	self->has_count = TRUE;

	switch (keyval)
	{
		case GDK_KEY_0: case GDK_KEY_KP_0: n = 0; break;
		case GDK_KEY_1: case GDK_KEY_KP_1: n = 1; break;
		case GDK_KEY_2: case GDK_KEY_KP_2: n = 2; break;
		case GDK_KEY_3: case GDK_KEY_KP_3: n = 3; break;
		case GDK_KEY_4: case GDK_KEY_KP_4: n = 4; break;
		case GDK_KEY_5: case GDK_KEY_KP_5: n = 5; break;
		case GDK_KEY_6: case GDK_KEY_KP_6: n = 6; break;
		case GDK_KEY_7: case GDK_KEY_KP_7: n = 7; break;
		case GDK_KEY_8: case GDK_KEY_KP_8: n = 8; break;
		case GDK_KEY_9: case GDK_KEY_KP_9: n = 9; break;

		default:
			self->handler = key_handler_initial;
			return self->handler (self, keyval, keycode, mods, string);
	}

	self->count = self->count * 10 + n;

	return TRUE;
}

static gboolean
key_handler_command (GtkSourceVimNormal *self,
                     guint               keyval,
                     guint               keycode,
                     GdkModifierType     mods,
                     const char         *string)
{
	GtkSourceVimState *new_state;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_R:
			new_state = gtk_source_vim_replace_new ();
			gtk_source_vim_state_set_count (new_state, self->count);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			return TRUE;

		case GDK_KEY_i:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_none (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_I:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_first_char (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_a:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_none (),
			                                    GTK_SOURCE_VIM_INSERT_AFTER_CHAR,
			                                    NULL);
			return TRUE;

		case GDK_KEY_A:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_line_end (),
			                                    GTK_SOURCE_VIM_INSERT_AFTER_CHAR,
			                                    NULL);
			return TRUE;

		case GDK_KEY_o:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_line_end (),
			                                    GTK_SOURCE_VIM_INSERT_AFTER_CHAR,
			                                    "prefix", "\n",
			                                    "indent", TRUE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_O:
			gtk_source_vim_normal_begin_insert (self,
			                                    gtk_source_vim_motion_new_line_start (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    "suffix", "\n",
			                                    "indent", TRUE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_C:
			if (self->count != 0)
				return gtk_source_vim_normal_bail (self);
			gtk_source_vim_normal_begin_change (self,
			                                    gtk_source_vim_motion_new_line_end (),
			                                    gtk_source_vim_motion_new_none (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_D:
			if (self->count != 0)
				return gtk_source_vim_normal_bail (self);
			gtk_source_vim_normal_begin_command (self,
			                                     gtk_source_vim_motion_new_line_end (),
			                                     gtk_source_vim_motion_new_none (),
			                                     ":delete", 0);
			return TRUE;

		case GDK_KEY_x:
			gtk_source_vim_normal_begin_command (self,
			                                     gtk_source_vim_motion_new_forward_char (),
			                                     gtk_source_vim_motion_new_none (),
			                                     ":delete", 0);
			return TRUE;

		case GDK_KEY_S:
			gtk_source_vim_normal_begin_change (self,
			                                    gtk_source_vim_motion_new_line_end (),
			                                    gtk_source_vim_motion_new_first_char (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_s:
			gtk_source_vim_normal_begin_change (self,
			                                    gtk_source_vim_motion_new_forward_char (),
			                                    gtk_source_vim_motion_new_none (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_J:
			gtk_source_vim_normal_begin_command (self,
			                                     gtk_source_vim_motion_new_next_line_end_with_nl (),
			                                     gtk_source_vim_motion_new_line_start (),
			                                     ":join", 0);
			return TRUE;

		case GDK_KEY_u:
			gtk_source_vim_normal_begin_command (self, NULL, NULL, ":undo", 0);
			return TRUE;

		case GDK_KEY_r:
			if ((mods & GDK_CONTROL_MASK) != 0)
			{
				gtk_source_vim_normal_begin_command (self, NULL, NULL, ":redo", 0);
				return TRUE;
			}
			break;

		case GDK_KEY_period:
			if (self->repeat != NULL)
			{
				GtkSourceBuffer *buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
				int count = MAX (1, self->count);

				gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
				for (int i = 0; i < count && self->repeat != NULL; i++)
					gtk_source_vim_state_repeat (self->repeat);
				gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

				gtk_source_vim_normal_clear (self);
				keep_on_char (self);

				return TRUE;
			}
			break;

		case GDK_KEY_Y:
			gtk_source_vim_normal_begin_command (self,
			                                     gtk_source_vim_motion_new_down (-1),
			                                     gtk_source_vim_motion_new_none (),
			                                     ":yank", 0);
			return TRUE;

		case GDK_KEY_p:
			gtk_source_vim_normal_begin_command (self, NULL, NULL, "paste-after", 0);
			return TRUE;

		case GDK_KEY_P:
			gtk_source_vim_normal_begin_command (self, NULL, NULL, "paste-before", 0);
			return TRUE;

		case GDK_KEY_asciitilde:
			gtk_source_vim_normal_begin_command (self,
			                                     gtk_source_vim_motion_new_forward_char (),
			                                     NULL,
			                                     "toggle-case", 0);
			return TRUE;

		case GDK_KEY_equal:
			gtk_source_vim_normal_begin_command (self,
			                                     NULL,
			                                     gtk_source_vim_motion_new_none (),
			                                     "filter", GDK_KEY_equal);
			return TRUE;

		case GDK_KEY_plus:
		default:
			break;
	}

	return gtk_source_vim_normal_bail (self);
}

static gboolean
key_handler_z (GtkSourceVimNormal *self,
               guint               keyval,
               guint               keycode,
               GdkModifierType     mods,
               const char         *string)
{
	GtkSourceVimState *state = (GtkSourceVimState *)self;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_z:
			gtk_source_vim_state_z_scroll (state, 0.5);
			break;

		case GDK_KEY_b:
			gtk_source_vim_state_z_scroll (state, 1.0);
			break;

		case GDK_KEY_t:
			gtk_source_vim_state_z_scroll (state, 0.0);
			break;

		default:
			return gtk_source_vim_normal_bail (self);
	}

	gtk_source_vim_normal_clear (self);

	return TRUE;
}

static gboolean
key_handler_viewport (GtkSourceVimNormal *self,
                      guint               keyval,
                      guint               keycode,
                      GdkModifierType     mods,
                      const char         *string)
{
	GtkSourceVimState *state = (GtkSourceVimState *)self;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_d:
				gtk_source_vim_state_scroll_half_page (state, MAX (1, self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			case GDK_KEY_u:
				gtk_source_vim_state_scroll_half_page (state, MIN (-1, -self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			case GDK_KEY_e:
				gtk_source_vim_state_scroll_line (state, MAX (1, self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			case GDK_KEY_y:
				gtk_source_vim_state_scroll_line (state, MIN (-1, -self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			case GDK_KEY_f:
				gtk_source_vim_state_scroll_page (state, MAX (1, self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			case GDK_KEY_b:
				gtk_source_vim_state_scroll_page (state, MIN (-1, -self->count));
				gtk_source_vim_normal_clear (self);
				return TRUE;

			default:
				break;
		}
	}

	return gtk_source_vim_normal_bail (self);
}

static gboolean
key_handler_c_with_modifier (GtkSourceVimNormal *self,
                             guint               keyval,
                             guint               keycode,
                             GdkModifierType     mods,
                             const char         *string)
{
	GtkSourceVimState *text_object;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	text_object = get_text_object (keyval, self->change_modifier);

	if (text_object != NULL)
	{
		int count;

		count = self->count, self->count = 0;
		gtk_source_vim_state_set_count (text_object, count);
		gtk_source_vim_normal_begin_insert_text_object (self, text_object);
		gtk_source_vim_normal_clear (self);
		g_object_unref (text_object);

		return TRUE;
	}

	return gtk_source_vim_normal_bail (self);
}

static gboolean
key_handler_c (GtkSourceVimNormal *self,
               guint               keyval,
               guint               keycode,
               GdkModifierType     mods,
               const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_c:
			gtk_source_vim_normal_begin_change (self,
			                                    gtk_source_vim_motion_new_line_end_with_nl (),
			                                    gtk_source_vim_motion_new_line_start (),
			                                    GTK_SOURCE_VIM_INSERT_HERE,
			                                    NULL);
			return TRUE;

		case GDK_KEY_i:
			self->change_modifier = CHANGE_INNER;
			self->handler = key_handler_c_with_modifier;
			return TRUE;

		case GDK_KEY_a:
			self->change_modifier = CHANGE_A;
			self->handler = key_handler_c_with_modifier;
			return TRUE;

		default:
		{
			GtkSourceVimState *motion;
			GtkSourceVimState *selection;
			GtkSourceVimState *insert;
			int count;

			count = self->count, self->count = 0;
			insert = gtk_source_vim_insert_new ();
			motion = gtk_source_vim_motion_new ();
			selection = gtk_source_vim_motion_new_none ();
			gtk_source_vim_motion_set_apply_on_leave (GTK_SOURCE_VIM_MOTION (motion), FALSE);
			gtk_source_vim_insert_set_selection_motion (GTK_SOURCE_VIM_INSERT (insert), GTK_SOURCE_VIM_MOTION (selection));
			gtk_source_vim_state_set_count (motion, count);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), insert);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (insert), motion);
			gtk_source_vim_state_synthesize (motion, keyval, mods);

			gtk_source_vim_normal_clear (self);

			g_object_unref (selection);

			return TRUE;
		}
	}
}

static gboolean
key_handler_d_with_modifier (GtkSourceVimNormal *self,
                             guint               keyval,
                             guint               keycode,
                             GdkModifierType     mods,
                             const char         *string)
{
	GtkSourceVimState *text_object;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	text_object = get_text_object (keyval, self->change_modifier);

	if (text_object != NULL)
	{
		GtkSourceVimState *command = gtk_source_vim_command_new (":delete");
		gtk_source_vim_command_set_text_object (GTK_SOURCE_VIM_COMMAND (command),
		                                        GTK_SOURCE_VIM_TEXT_OBJECT (text_object));
		gtk_source_vim_normal_clear (self);
		gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), command);
		gtk_source_vim_state_pop (command);
		g_object_unref (text_object);
		return TRUE;
	}

	return gtk_source_vim_normal_bail (self);
}

static gboolean
key_handler_d (GtkSourceVimNormal *self,
               guint               keyval,
               guint               keycode,
               GdkModifierType     mods,
               const char         *string)
{
	GtkSourceVimState *current;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_i:
			self->change_modifier = CHANGE_INNER;
			self->handler = key_handler_d_with_modifier;
			return TRUE;

		case GDK_KEY_a:
			self->change_modifier = CHANGE_A;
			self->handler = key_handler_d_with_modifier;
			return TRUE;

		default:
		{
			gtk_source_vim_normal_begin_command (self,
			                                     NULL,
			                                     gtk_source_vim_motion_new_none (),
			                                     ":delete", GDK_KEY_d);
			current = gtk_source_vim_state_get_current (GTK_SOURCE_VIM_STATE (self));
			gtk_source_vim_state_synthesize (current, keyval, mods);
			return TRUE;
		}
	}
}

static gboolean
key_handler_shift (GtkSourceVimNormal *self,
                   guint               keyval,
                   guint               keycode,
                   GdkModifierType     mods,
                   const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_greater:
			gtk_source_vim_normal_begin_command (self, NULL, NULL, "indent", 0);
			return TRUE;

		case GDK_KEY_less:
			gtk_source_vim_normal_begin_command (self, NULL, NULL, "unindent", 0);
			return TRUE;

		default:
			return gtk_source_vim_normal_bail (self);
	}
}

static gboolean
key_handler_search (GtkSourceVimNormal *self,
                    guint               keyval,
                    guint               keycode,
                    GdkModifierType     mods,
                    const char         *string)
{
	GtkSourceVimState *command_bar;
	const char *text;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_slash:
		case GDK_KEY_KP_Divide:
			text = "/";
			break;

		case GDK_KEY_question:
			text = "?";
			break;

		default:
			return gtk_source_vim_normal_bail (self);
	}

	command_bar = gtk_source_vim_command_bar_new ();
	gtk_source_vim_command_bar_set_text (GTK_SOURCE_VIM_COMMAND_BAR (command_bar), text);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), command_bar);

	return TRUE;
}

static gboolean
key_handler_register (GtkSourceVimNormal *self,
                      guint               keyval,
                      guint               keycode,
                      GdkModifierType     mods,
                      const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	if (string == NULL || string[0] == 0)
	{
		/* We require a string to access the register */
		return gtk_source_vim_normal_bail (self);
	}

	gtk_source_vim_state_set_current_register (GTK_SOURCE_VIM_STATE (self), string);

	self->handler = key_handler_initial;

	return TRUE;
}

static gboolean
key_handler_split (GtkSourceVimNormal *self,
                   guint               keyval,
                   guint               keycode,
                   GdkModifierType     mods,
                   const char         *string)
{
	GtkSourceVimState *root;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
	case GDK_KEY_c:
	case GDK_KEY_v:
	case GDK_KEY_s:
	case GDK_KEY_w:
	case GDK_KEY_h:
	case GDK_KEY_l:
	case GDK_KEY_j:
	case GDK_KEY_k:
		if ((root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self))) &&
		    GTK_SOURCE_IS_VIM (root))
		{
			gtk_source_vim_emit_execute_command (GTK_SOURCE_VIM (root), self->command_text->str);
			gtk_source_vim_normal_clear (self);
			return TRUE;
		}

		G_GNUC_FALLTHROUGH;

	default:
		return gtk_source_vim_normal_bail (self);
	}
}

static gboolean
key_handler_increment (GtkSourceVimNormal *self,
                       guint               keyval,
                       guint               keycode,
                       GdkModifierType     mods,
                       const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	return TRUE;
}

static gboolean
key_handler_g (GtkSourceVimNormal *self,
               guint               keyval,
               guint               keycode,
               GdkModifierType     mods,
               const char         *string)
{
	GtkSourceVimState *new_state;
	GtkSourceVimState *root;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	switch (keyval)
	{
		case GDK_KEY_question:
			return gtk_source_vim_normal_begin_command_requiring_motion (self, "rot13");

		case GDK_KEY_q:
			return gtk_source_vim_normal_begin_command_requiring_motion (self, "format");

		case GDK_KEY_g:
		case GDK_KEY_e:
		case GDK_KEY_E:
			new_state = gtk_source_vim_motion_new ();
			gtk_source_vim_state_set_count (new_state, self->count);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			gtk_source_vim_state_synthesize (new_state, GDK_KEY_g, 0);
			gtk_source_vim_state_synthesize (new_state, keyval, mods);
			return TRUE;

		case GDK_KEY_v:
			if (self->last_visual == NULL)
				return gtk_source_vim_normal_bail (self);

			new_state = gtk_source_vim_visual_clone (GTK_SOURCE_VIM_VISUAL (self->last_visual));
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			return TRUE;

		case GDK_KEY_d:
		case GDK_KEY_D:
			if ((root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self))) &&
			    GTK_SOURCE_IS_VIM (root))
			{
				const char *command = keyval == GDK_KEY_d ? "gd" : "gD";
				gtk_source_vim_emit_execute_command (GTK_SOURCE_VIM (root), command);
				gtk_source_vim_normal_clear (self);
				return TRUE;
			}

			G_GNUC_FALLTHROUGH;

		default:
			return gtk_source_vim_normal_bail (self);
	}
}

static gboolean
key_handler_motion (GtkSourceVimNormal *self,
                    guint               keyval,
                    guint               keycode,
                    GdkModifierType     mods,
                    const char         *string)
{
	GtkSourceVimState *new_state;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	if (self->command_text->len > 0)
		g_string_truncate (self->command_text, self->command_text->len - 1);

	new_state = gtk_source_vim_motion_new ();
	gtk_source_vim_state_set_count (new_state, self->count);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
	gtk_source_vim_state_synthesize (new_state, keyval, mods);

	return TRUE;
}

static gboolean
key_handler_mark (GtkSourceVimNormal *self,
                  guint               keyval,
                  guint               keycode,
                  GdkModifierType     mods,
                  const char         *string)
{
	GtkTextIter iter;

	if (!g_ascii_isalpha (string[0]))
	{
		return gtk_source_vim_normal_bail (self);
	}

	gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, NULL);
	gtk_source_vim_state_set_mark (GTK_SOURCE_VIM_STATE (self), string, &iter);
	gtk_source_vim_normal_clear (self);

	return TRUE;
}

static gboolean
key_handler_initial (GtkSourceVimNormal *self,
                     guint               keyval,
                     guint               keycode,
                     GdkModifierType     mods,
                     const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));

	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_a:
			case GDK_KEY_x:
				self->handler = key_handler_increment;
				break;

			case GDK_KEY_d:
			case GDK_KEY_u:
			case GDK_KEY_e:
			case GDK_KEY_y:
			case GDK_KEY_f:
			case GDK_KEY_b:
				self->handler = key_handler_viewport;
				break;

			case GDK_KEY_v:
				gtk_source_vim_normal_begin_visual (self, GTK_SOURCE_VIM_VISUAL_BLOCK);
				return TRUE;

			case GDK_KEY_w:
				self->handler = key_handler_split;
				return TRUE;

			case GDK_KEY_r:
				self->handler = key_handler_command;
				break;

			case GDK_KEY_o:
				gtk_source_vim_normal_begin_command (self, NULL, NULL, "jump-backward", 0);
				return TRUE;

			case GDK_KEY_i:
				gtk_source_vim_normal_begin_command (self, NULL, NULL, "jump-forward", 0);
				return TRUE;

			default:
				break;
		}
	}
	else
	{
		switch (keyval)
		{
			case GDK_KEY_0:
			case GDK_KEY_KP_0:
			case GDK_KEY_apostrophe:
			case GDK_KEY_asciicircum:
			case GDK_KEY_asterisk:
			case GDK_KEY_b:
			case GDK_KEY_bar:
			case GDK_KEY_B:
			case GDK_KEY_BackSpace:
			case GDK_KEY_braceleft:
			case GDK_KEY_braceright:
			case GDK_KEY_bracketleft:
			case GDK_KEY_bracketright:
			case GDK_KEY_dollar:
			case GDK_KEY_Down:
			case GDK_KEY_e:
			case GDK_KEY_E:
			case GDK_KEY_End:
			case GDK_KEY_f:
			case GDK_KEY_F:
			case GDK_KEY_grave:
			case GDK_KEY_G:
			case GDK_KEY_h:
			case GDK_KEY_H:
			case GDK_KEY_ISO_Enter:
			case GDK_KEY_j:
			case GDK_KEY_k:
			case GDK_KEY_KP_Enter:
			case GDK_KEY_KP_Multiply:
			case GDK_KEY_l:
			case GDK_KEY_L:
			case GDK_KEY_Left:
			case GDK_KEY_M:
			case GDK_KEY_n:
			case GDK_KEY_numbersign:
			case GDK_KEY_N:
			case GDK_KEY_parenleft:
			case GDK_KEY_parenright:
			case GDK_KEY_percent:
			case GDK_KEY_Return:
			case GDK_KEY_Right:
			case GDK_KEY_space:
			case GDK_KEY_underscore:
			case GDK_KEY_Up:
			case GDK_KEY_w:
			case GDK_KEY_W:
				self->handler = key_handler_motion;
				break;

			case GDK_KEY_m:
				self->handler = key_handler_mark;
				return TRUE;

			case GDK_KEY_1: case GDK_KEY_KP_1:
			case GDK_KEY_2: case GDK_KEY_KP_2:
			case GDK_KEY_3: case GDK_KEY_KP_3:
			case GDK_KEY_4: case GDK_KEY_KP_4:
			case GDK_KEY_5: case GDK_KEY_KP_5:
			case GDK_KEY_6: case GDK_KEY_KP_6:
			case GDK_KEY_7: case GDK_KEY_KP_7:
			case GDK_KEY_8: case GDK_KEY_KP_8:
			case GDK_KEY_9: case GDK_KEY_KP_9:
				/* ignore if mods set as that is a common keybinding */
				if (self->has_count == FALSE && mods == 0)
					self->handler = key_handler_count;
				break;

			case GDK_KEY_a:
			case GDK_KEY_asciitilde:
			case GDK_KEY_A:
			case GDK_KEY_C:
			case GDK_KEY_D:
			case GDK_KEY_i:
			case GDK_KEY_I:
			case GDK_KEY_J:
			case GDK_KEY_o:
			case GDK_KEY_O:
			case GDK_KEY_p:
			case GDK_KEY_P:
			case GDK_KEY_period:
			case GDK_KEY_R:
			case GDK_KEY_s:
			case GDK_KEY_S:
			case GDK_KEY_u:
			case GDK_KEY_x:
			case GDK_KEY_equal:
			case GDK_KEY_plus:
			case GDK_KEY_Y:
				self->handler = key_handler_command;
				break;

			case GDK_KEY_quotedbl:
				self->handler = key_handler_register;
				return TRUE;

			case GDK_KEY_y:
				gtk_source_vim_normal_begin_command (self,
				                                     NULL,
				                                     gtk_source_vim_motion_new_none (),
				                                     ":yank", GDK_KEY_y);
				return TRUE;

			case GDK_KEY_d:
				self->handler = key_handler_d;
				return TRUE;

			case GDK_KEY_c:
				self->handler = key_handler_c;
				return TRUE;

			case GDK_KEY_g:
				self->handler = key_handler_g;
				return TRUE;

			case GDK_KEY_z:
				self->handler = key_handler_z;
				return TRUE;

			case GDK_KEY_greater:
			case GDK_KEY_less:
				self->handler = key_handler_shift;
				return TRUE;

			case GDK_KEY_r:
				return gtk_source_vim_normal_replace_one (self);

			case GDK_KEY_slash:
			case GDK_KEY_KP_Divide:
			case GDK_KEY_question:
				self->handler = key_handler_search;
				break;

			case GDK_KEY_colon:
				gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self),
				                           gtk_source_vim_command_bar_new ());
				return TRUE;

			case GDK_KEY_v:
				gtk_source_vim_normal_begin_visual (self, GTK_SOURCE_VIM_VISUAL_CHAR);
				return TRUE;

			case GDK_KEY_V:
				gtk_source_vim_normal_begin_visual (self, GTK_SOURCE_VIM_VISUAL_LINE);
				return TRUE;

			default:
				break;
		}
	}

	if (self->handler == key_handler_initial)
	{
		/* If this is possibly a shortcut (alt, control, etc) then we
		 * can let it pass through without being too likely to activate
		 * text insertion. Additionally, if there is no @string value
		 * then there isn't anything likely to be passed on to the
		 * textview to insert but it might be something like F10.
		 */
		if ((mods & (GDK_CONTROL_MASK | GDK_SUPER_MASK | GDK_ALT_MASK)) != 0 || string[0] == 0)
		{
			/* Ignore this from the command_text */
			if (strlen (string) <= self->command_text->len)
			{
				g_string_truncate (self->command_text, self->command_text->len - strlen (string));
			}

			return FALSE;
		}

		return gtk_source_vim_normal_bail (self);
	}

	return self->handler (self, keyval, keycode, mods, string);
}

static gboolean
gtk_source_vim_normal_handle_keypress (GtkSourceVimState *state,
                                       guint              keyval,
                                       guint              keycode,
                                       GdkModifierType    mods,
                                       const char        *string)
{
	GtkSourceVimNormal *self = (GtkSourceVimNormal *)state;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	g_string_append (self->command_text, string);

	if (gtk_source_vim_state_is_escape (keyval, mods))
	{
		gtk_source_vim_normal_clear (self);
		return TRUE;
	}

	return self->handler (self, keyval, keycode, mods, string);
}

static void
gtk_source_vim_normal_resume (GtkSourceVimState *state,
                              GtkSourceVimState *from)
{
	GtkSourceVimNormal *self = (GtkSourceVimNormal *)state;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextMark *insert;
	GtkTextIter origin;
	gboolean unparent = TRUE;

	g_assert (GTK_SOURCE_IS_VIM_NORMAL (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	buffer = gtk_source_vim_state_get_buffer (state, &origin, NULL);
	insert = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (buffer));
	view = gtk_source_vim_state_get_view (state);

	gtk_source_vim_normal_clear (GTK_SOURCE_VIM_NORMAL (state));
	gtk_source_vim_state_set_overwrite (state, TRUE);
	gtk_source_vim_state_set_current_register (state, NULL);

	/* Go back one character if we exited replace/insert state */
	if (GTK_SOURCE_IS_VIM_INSERT (from) || GTK_SOURCE_IS_VIM_REPLACE (from))
	{
		go_backward_char (self);
	}
	else if (GTK_SOURCE_IS_VIM_VISUAL (from))
	{
		/* Store last visual around for reselection in gv */
		gtk_source_vim_state_reparent (from, self, &self->last_visual);
		unparent = FALSE;
	}
	else if (!GTK_SOURCE_IS_VIM_MOTION (from) ||
	         gtk_source_vim_motion_invalidates_visual_column (GTK_SOURCE_VIM_MOTION (from)))
	{
		GtkTextIter iter;
		guint visual_column;

		gtk_source_vim_state_get_buffer (state, &iter, NULL);
		visual_column = gtk_source_view_get_visual_column (view, &iter);
		gtk_source_vim_state_set_visual_column (state, visual_column);
	}

	/* If we're still on the \n, go back a char */
	keep_on_char (self);

	/* Always scroll the insert mark onscreen */
	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view), insert);

	if (gtk_source_vim_state_get_can_repeat (from))
	{
		gtk_source_vim_state_reparent (from, self, &self->repeat);
		unparent = FALSE;
	}

	if (unparent)
	{
		gtk_source_vim_state_unparent (from);
	}
}

static void
gtk_source_vim_normal_enter (GtkSourceVimState *state)
{
	g_assert (GTK_SOURCE_IS_VIM_NORMAL (state));

	gtk_source_vim_state_set_overwrite (state, TRUE);
}

static void
gtk_source_vim_normal_append_command (GtkSourceVimState *state,
                                      GString           *string)
{
	GtkSourceVimNormal *self = (GtkSourceVimNormal *)state;

	g_assert (GTK_SOURCE_IS_VIM_STATE (state));
	g_assert (string != NULL);

	if (self->command_text->len > 0)
	{
		g_string_append_len (string,
		                     self->command_text->str,
		                     self->command_text->len);
	}
}

static void
gtk_source_vim_normal_dispose (GObject *object)
{
	GtkSourceVimNormal *self = (GtkSourceVimNormal *)object;

	gtk_source_vim_state_release (&self->last_visual);
	gtk_source_vim_state_release (&self->repeat);

	if (self->command_text != NULL)
	{
		g_string_free (self->command_text, TRUE);
		self->command_text = NULL;
	}

	G_OBJECT_CLASS (gtk_source_vim_normal_parent_class)->dispose (object);
}

static void
gtk_source_vim_normal_class_init (GtkSourceVimNormalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_normal_dispose;

	state_class->append_command = gtk_source_vim_normal_append_command;
	state_class->handle_keypress = gtk_source_vim_normal_handle_keypress;
	state_class->enter = gtk_source_vim_normal_enter;
	state_class->resume = gtk_source_vim_normal_resume;
}

static void
gtk_source_vim_normal_init (GtkSourceVimNormal *self)
{
	self->handler = key_handler_initial;
	self->command_text = g_string_new (NULL);
}

GtkSourceVimState *
gtk_source_vim_normal_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_NORMAL, NULL);
}

void
gtk_source_vim_normal_clear (GtkSourceVimNormal *self)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_NORMAL (self));

	self->handler = key_handler_initial;
	self->count = 0;
	self->has_count = FALSE;
	self->change_modifier = CHANGE_NONE;

  /* If self->command_text == NULL, then we've disposed and we
   * don't want to notify anything (as they should be disconnected
   * anyway as part of g_object_run_dispose() process.
   */
  if (self->command_text != NULL)
    {
      g_string_truncate (self->command_text, 0);

      /* Let the toplevel know we're back at steady state. This is
       * basically just so observers can watch keys which makes it
       * much easier to debug issues.
       */
      gtk_source_vim_normal_emit_ready (self);
    }
}

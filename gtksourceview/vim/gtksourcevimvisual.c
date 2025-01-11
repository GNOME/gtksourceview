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

#include "gtksourceview.h"

#include "gtksourcevimcharpending.h"
#include "gtksourcevimcommand.h"
#include "gtksourcevimcommandbar.h"
#include "gtksourceviminsert.h"
#include "gtksourcevimmotion.h"
#include "gtksourcevimreplace.h"
#include "gtksourcevimvisual.h"
#include "gtksourcevimregisters.h"

typedef gboolean (*KeyHandler) (GtkSourceVimVisual *self,
                                guint               keyval,
                                guint               keycode,
                                GdkModifierType     mods,
                                const char         *string);

struct _GtkSourceVimVisual
{
	GtkSourceVimState parent_class;

	GtkSourceVimVisualMode mode;

	GString *command_text;

	/* A recording of motions so that we can replay commands
	 * such as delete and get a similar result to VIM. Replaying
	 * our motion's visual selection is not enough as after a
	 * delete it would be empty.
	 */
	GtkSourceVimMotion *motion;

	/* The operation to repeat. This may be a number of things such
	 * as a GtkSourceVimCommand, GtkSourceVimInsert, or GtkSourceVimDelete.
	 */
	GtkSourceVimState *command;

	KeyHandler handler;

	GtkTextMark *started_at;
	GtkTextMark *cursor;

	int count;

	guint ignore_command : 1;
};

typedef struct
{
	GtkTextBuffer *buffer;
	GtkTextMark   *cursor;
	GtkTextMark   *started_at;
	int            cmp;
	guint          line;
	guint          line_offset;
	guint          start_line;
	guint          linewise : 1;
} CursorInfo;

static gboolean gtk_source_vim_visual_bail (GtkSourceVimVisual *self);
static gboolean key_handler_initial        (GtkSourceVimVisual *self,
                                            guint               keyval,
                                            guint               keycode,
                                            GdkModifierType     mods,
                                            const char         *string);

G_DEFINE_TYPE (GtkSourceVimVisual, gtk_source_vim_visual, GTK_SOURCE_TYPE_VIM_STATE)

static void
cursor_info_stash (GtkSourceVimVisual *self,
                   CursorInfo         *info)
{
	GtkTextIter cursor;
	GtkTextIter started_at;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	info->buffer = gtk_text_mark_get_buffer (self->cursor);
	info->cursor = self->cursor;
	info->started_at = self->started_at;

	gtk_text_buffer_get_iter_at_mark (info->buffer, &cursor, self->cursor);
	gtk_text_buffer_get_iter_at_mark (info->buffer, &started_at, self->started_at);

	info->cmp = gtk_text_iter_compare (&cursor, &started_at);
	info->line = gtk_text_iter_get_line (&cursor);
	info->line_offset = gtk_text_iter_get_line_offset (&cursor);
	info->start_line = MIN (gtk_text_iter_get_line (&started_at), info->line);
	info->linewise = self->mode == GTK_SOURCE_VIM_VISUAL_LINE;
}

static void
cursor_info_restore (CursorInfo *info)
{
	if (info->linewise)
	{
		if (info->cmp > 0)
		{
			GtkTextIter iter;

			gtk_text_buffer_get_iter_at_line (info->buffer, &iter, info->start_line);
			gtk_text_buffer_select_range (info->buffer, &iter, &iter);
		}
		else
		{
			GtkTextIter iter;

			gtk_text_buffer_get_iter_at_line_offset (info->buffer, &iter, info->line, info->line_offset);
			gtk_text_buffer_select_range (info->buffer, &iter, &iter);
		}
	}
	else
	{
		GtkTextIter cursor, started_at;

		gtk_text_buffer_get_iter_at_mark (info->buffer, &cursor, info->cursor);
		gtk_text_buffer_get_iter_at_mark (info->buffer, &started_at, info->started_at);
		gtk_text_iter_order (&cursor, &started_at);
		gtk_text_buffer_select_range (info->buffer, &cursor, &cursor);
	}
}

static void
track_visible_column (GtkSourceVimVisual *self)
{
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter;
	guint visual_column;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
	                                  &iter,
	                                  self->cursor);
	visual_column = gtk_source_view_get_visual_column (view, &iter);
	gtk_source_vim_state_set_visual_column (GTK_SOURCE_VIM_STATE (self), visual_column);
}

static void
update_cursor_visible (GtkSourceVimVisual *self)
{
	GtkSourceVimState *child = gtk_source_vim_state_get_child (GTK_SOURCE_VIM_STATE (self));
	gboolean is_line = self->mode == GTK_SOURCE_VIM_VISUAL_LINE;

	gtk_text_mark_set_visible (self->cursor, child == NULL && is_line);
}

static void
gtk_source_vim_visual_clear (GtkSourceVimVisual *self)
{
	self->handler = key_handler_initial;
	self->count = 0;
	g_string_truncate (self->command_text, 0);
}

static gboolean
gtk_source_vim_visual_bail (GtkSourceVimVisual *self)
{
	gtk_source_vim_visual_clear (self);
	return TRUE;
}

static void
gtk_source_vim_visual_track_char (GtkSourceVimVisual *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter cursor;
	GtkTextIter started_at;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &cursor, self->cursor);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &started_at, self->started_at);

	if (gtk_text_iter_equal (&cursor, &started_at))
	{
		if (gtk_text_iter_starts_line (&cursor) && gtk_text_iter_ends_line (&cursor))
		{
			/* Leave the selection empty, since we don't really
			 * have a character to select (other than the newline
			 * which isn't what VIM does.
			 */
		}
		else if (gtk_text_iter_ends_line (&cursor))
		{
			/* Some how ended up on the \n when we shouldn't. Maybe
			 * a stray button press or something. Adjust now.
			 */
			gtk_text_iter_backward_char (&started_at);
		}
		else
		{
			gtk_text_iter_forward_char (&cursor);
		}

		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &cursor, &started_at);
	}
	else if (gtk_text_iter_compare (&started_at, &cursor) < 0)
	{
		/* Include the the character under the cursor */
		if (!gtk_text_iter_ends_line (&cursor))
		{
			gtk_text_iter_forward_char (&cursor);
		}

		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &cursor, &started_at);
	}
	else
	{
		/* We need to swap the started at so that it is one character
		 * above so that the starting character is still selected.
		 */
		if (!gtk_text_iter_ends_line (&started_at))
		{
			gtk_text_iter_forward_char (&started_at);
		}

		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &cursor, &started_at);
	}
}

static void
gtk_source_vim_visual_track_line (GtkSourceVimVisual *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter cursor, started_at;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &cursor, self->cursor);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &started_at, self->started_at);

	gtk_source_vim_state_select_linewise (GTK_SOURCE_VIM_STATE (self), &cursor, &started_at);
}

static void
gtk_source_vim_visual_track_motion (GtkSourceVimVisual *self)
{
	GtkSourceView *view;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	switch (self->mode)
	{
		case GTK_SOURCE_VIM_VISUAL_LINE:
			gtk_source_vim_visual_track_line (self);
			break;

		case GTK_SOURCE_VIM_VISUAL_CHAR:
			gtk_source_vim_visual_track_char (self);
			break;

		case GTK_SOURCE_VIM_VISUAL_BLOCK:
		default:
			break;
	}

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (view), self->cursor);
}

static const char *
gtk_source_vim_visual_get_command_bar_text (GtkSourceVimState *state)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	switch (self->mode)
	{
		case GTK_SOURCE_VIM_VISUAL_CHAR:
			return _("-- VISUAL --");

		case GTK_SOURCE_VIM_VISUAL_LINE:
			return _("-- VISUAL LINE --");

		case GTK_SOURCE_VIM_VISUAL_BLOCK:
			return _("-- VISUAL BLOCK --");

		default:
			g_assert_not_reached ();
			return NULL;
	}
}

static gboolean
key_handler_z (GtkSourceVimVisual *self,
	       guint               keyval,
	       guint               keycode,
	       GdkModifierType     mods,
	       const char         *string)
{
	GtkSourceVimState *state = GTK_SOURCE_VIM_STATE (self);

	switch (keyval)
	{
		case GDK_KEY_z:
			gtk_source_vim_state_z_scroll (state, 0.5);
			return TRUE;

		case GDK_KEY_b:
			gtk_source_vim_state_z_scroll (state, 1.0);
			return TRUE;

		case GDK_KEY_t:
			gtk_source_vim_state_z_scroll (state, 0.0);
			return TRUE;

		default:
			return gtk_source_vim_visual_bail (self);
	}
}

static gboolean
key_handler_register (GtkSourceVimVisual *self,
	              guint               keyval,
	              guint               keycode,
	              GdkModifierType     mods,
	              const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	if (string == NULL || string[0] == 0)
		return gtk_source_vim_visual_bail (self);

	gtk_source_vim_state_set_current_register (GTK_SOURCE_VIM_STATE (self), string);

	self->handler = key_handler_initial;

	return TRUE;
}

static gboolean
gtk_source_vim_visual_begin_command (GtkSourceVimVisual *self,
                                     const char         *command,
                                     gboolean            restore_cursor)
{
	CursorInfo info;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));
	g_assert (command != NULL);

	count = self->count, self->count = 0;

	gtk_source_vim_visual_clear (self);
	gtk_source_vim_state_release (&self->command);

	if (restore_cursor)
		cursor_info_stash (self, &info);

	self->command = gtk_source_vim_command_new (command);
	gtk_source_vim_state_set_count (self->command, count);
	gtk_source_vim_state_set_parent (self->command, GTK_SOURCE_VIM_STATE (self));
	gtk_source_vim_state_repeat (self->command);

	if (gtk_source_vim_state_get_can_repeat (self->command))
		gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	if (restore_cursor)
		cursor_info_restore (&info);

	gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));

	return TRUE;
}

static gboolean
gtk_source_vim_visual_try_motion (GtkSourceVimVisual *self,
                                  guint               keyval,
                                  guint               keycode,
                                  GdkModifierType     mods,
                                  const char         *str)
{
	GtkSourceVimState *motion;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	count = self->count, self->count = 0;

	/* Try to apply a motion to our cursor */
	motion = gtk_source_vim_motion_new ();
	gtk_source_vim_state_set_count (motion, count);
	gtk_source_vim_motion_set_mark (GTK_SOURCE_VIM_MOTION (motion), self->cursor);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), motion);
	gtk_source_vim_state_synthesize (motion, keyval, mods);

	if (self->command_text->len)
		g_string_truncate (self->command_text, 0);

	return TRUE;
}

static gboolean
gtk_source_vim_visual_begin_insert (GtkSourceVimVisual *self)
{
	GtkSourceVimState *insert;
	GtkSourceVimMotion *motion;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	motion = GTK_SOURCE_VIM_MOTION (gtk_source_vim_motion_new_none ());
	insert = gtk_source_vim_insert_new ();

	if (self->mode == GTK_SOURCE_VIM_VISUAL_LINE)
	{
		gtk_source_vim_insert_set_suffix (GTK_SOURCE_VIM_INSERT (insert), "\n");
	}

	gtk_source_vim_insert_set_at (GTK_SOURCE_VIM_INSERT (insert), GTK_SOURCE_VIM_INSERT_HERE);
	gtk_source_vim_insert_set_motion (GTK_SOURCE_VIM_INSERT (insert), motion);
	gtk_source_vim_insert_set_selection_motion (GTK_SOURCE_VIM_INSERT (insert), motion);
	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), insert);

	gtk_source_vim_state_reparent (insert, self, &self->command);

	g_object_unref (motion);

	return TRUE;
}

static gboolean
gtk_source_vim_visual_put (GtkSourceVimVisual *self,
                           gboolean            clipboard)
{
	GtkSourceVimRegisters *registers;
	GtkSourceBuffer *buffer;
	const char *replace_content;
	char *selection_content;
	GtkTextIter start;
	GtkTextIter end;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);

	if (clipboard)
	{
		registers = GTK_SOURCE_VIM_REGISTERS (gtk_source_vim_state_get_registers (GTK_SOURCE_VIM_STATE (self)));
		replace_content = gtk_source_vim_registers_get (registers, "+");
	}
	else
	{
		replace_content = gtk_source_vim_state_get_current_register_value (GTK_SOURCE_VIM_STATE (self));
	}

	gtk_source_vim_visual_get_bounds (self, &start, &end);
	gtk_text_iter_forward_char (&start);
	selection_content = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, FALSE);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete_selection (GTK_TEXT_BUFFER (buffer), TRUE, TRUE);
	gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (buffer), replace_content, -1);
	gtk_source_vim_state_set_current_register_value (GTK_SOURCE_VIM_STATE (self), selection_content);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));

	gtk_source_vim_visual_clear (self);

	g_free (selection_content);

	return TRUE;
}

static gboolean
gtk_source_vim_visual_replace (GtkSourceVimVisual *self)
{
	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	self->command = gtk_source_vim_command_new ("replace-one");

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self),
	                           g_object_ref (self->command));
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self->command),
	                           gtk_source_vim_char_pending_new ());

	gtk_source_vim_visual_clear (self);

	return TRUE;
}

static void
gtk_source_vim_visual_swap_cursor (GtkSourceVimVisual *self)
{
	GtkTextBuffer *buffer;
	GtkTextIter cursor, started_at;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	gtk_source_vim_visual_get_bounds (self, &cursor, &started_at);

	buffer = gtk_text_iter_get_buffer (&cursor);
	gtk_text_buffer_move_mark (buffer, self->cursor, &started_at);
	gtk_text_buffer_move_mark (buffer, self->started_at, &cursor);

	gtk_source_vim_visual_track_motion (self);
}

static gboolean
key_handler_g (GtkSourceVimVisual *self,
	       guint               keyval,
	       guint               keycode,
	       GdkModifierType     mods,
	       const char         *string)
{
	GtkSourceVimState *new_state;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	switch (keyval)
	{
		case GDK_KEY_question:
			return gtk_source_vim_visual_begin_command (self, "rot13", TRUE);

		case GDK_KEY_q:
			return gtk_source_vim_visual_begin_command (self, "format", FALSE);

		default:
			new_state = gtk_source_vim_motion_new ();
			gtk_source_vim_motion_set_mark (GTK_SOURCE_VIM_MOTION (new_state), self->cursor);
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			gtk_source_vim_state_synthesize (new_state, GDK_KEY_g, 0);
			gtk_source_vim_state_synthesize (new_state, keyval, mods);
			return TRUE;
	}
}

static gboolean
key_handler_initial (GtkSourceVimVisual *self,
                     guint               keyval,
                     guint               keycode,
                     GdkModifierType     mods,
                     const char         *string)
{
	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_y:
			case GDK_KEY_e:
			case GDK_KEY_b:
			case GDK_KEY_f:
			case GDK_KEY_u:
			case GDK_KEY_d:
				goto try_visual_motion;

			default:
				break;
		}
	}

	if (self->count == 0)
	{
		switch (keyval)
		{
			case GDK_KEY_0:
			case GDK_KEY_KP_0:
				goto try_visual_motion;

			default:
				break;
		}
	}

	switch (keyval)
	{
		case GDK_KEY_0: case GDK_KEY_KP_0:
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
			if (self->count == 0 && mods != 0)
				return FALSE;

			self->count *= 10;

			if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
				self->count += keyval - GDK_KEY_0;
			else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
				self->count += keyval - GDK_KEY_KP_0;

			return TRUE;

		case GDK_KEY_z:
			self->handler = key_handler_z;
			return TRUE;

		case GDK_KEY_d:
		case GDK_KEY_x:
			return gtk_source_vim_visual_begin_command (self, ":delete", TRUE);

		case GDK_KEY_quotedbl:
			self->handler = key_handler_register;
			return TRUE;

		case GDK_KEY_y:
			return gtk_source_vim_visual_begin_command (self, ":yank", TRUE);

		case GDK_KEY_v:
			self->mode = GTK_SOURCE_VIM_VISUAL_CHAR;
			gtk_source_vim_visual_track_motion (self);
			update_cursor_visible (self);
			return TRUE;

		case GDK_KEY_V:
			self->mode = GTK_SOURCE_VIM_VISUAL_LINE;
			gtk_source_vim_visual_track_motion (self);
			update_cursor_visible (self);
			return TRUE;

		case GDK_KEY_U:
			return gtk_source_vim_visual_begin_command (self, "upcase", TRUE);

		case GDK_KEY_u:
			return gtk_source_vim_visual_begin_command (self, "downcase", TRUE);

		case GDK_KEY_g:
			self->handler = key_handler_g;
			return TRUE;

		case GDK_KEY_c:
		case GDK_KEY_C:
			return gtk_source_vim_visual_begin_insert (self);

		case GDK_KEY_r:
			return gtk_source_vim_visual_replace (self);

		case GDK_KEY_p:
			return gtk_source_vim_visual_put (self, FALSE);

		case GDK_KEY_greater:
			return gtk_source_vim_visual_begin_command (self, "indent", FALSE);

		case GDK_KEY_less:
			return gtk_source_vim_visual_begin_command (self, "unindent", FALSE);

		case GDK_KEY_equal:
			return gtk_source_vim_visual_begin_command (self, "filter", FALSE);

		case GDK_KEY_slash:
		case GDK_KEY_KP_Divide:
		case GDK_KEY_question:
		{
			GtkSourceVimState *new_state = gtk_source_vim_command_bar_new ();
			gtk_source_vim_command_bar_set_text (GTK_SOURCE_VIM_COMMAND_BAR (new_state),
			                                     keyval == GDK_KEY_question ? "?" : "/");
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			return TRUE;
		}

		case GDK_KEY_colon:
		{
			GtkSourceVimState *new_state = gtk_source_vim_command_bar_new ();
			gtk_source_vim_command_bar_set_text (GTK_SOURCE_VIM_COMMAND_BAR (new_state), ":'<,'>");
			gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), new_state);
			return TRUE;
		}

		case GDK_KEY_o:
			gtk_source_vim_visual_swap_cursor (self);
			gtk_source_vim_visual_clear (self);
			return TRUE;

		default:
			break;
	}

try_visual_motion:

	return gtk_source_vim_visual_try_motion (self, keyval, keycode, mods, string);
}

static void
gtk_source_vim_visual_enter (GtkSourceVimState *state)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;
	GtkSourceBuffer *buffer;
	GtkTextIter iter, selection;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	buffer = gtk_source_vim_state_get_buffer (state, &iter, &selection);

	if (self->started_at == NULL)
	{
		self->started_at = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);
		g_object_add_weak_pointer (G_OBJECT (self->started_at),
		                           (gpointer *)&self->started_at);
	}

	if (self->cursor == NULL)
	{
		self->cursor = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, FALSE);
		g_object_add_weak_pointer (G_OBJECT (self->cursor),
		                           (gpointer *)&self->cursor);
	}

	update_cursor_visible (self);

	track_visible_column (self);

	gtk_source_vim_visual_track_motion (self);
}

static void
gtk_source_vim_visual_leave (GtkSourceVimState *state)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));

	buffer = gtk_source_vim_state_get_buffer (state, &iter, &selection);

	if (gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
		                                  &iter, self->cursor);

		if (gtk_text_iter_ends_line (&iter) &&
		    !gtk_text_iter_starts_line (&iter))
		{
			gtk_text_iter_backward_char (&iter);
		}

		gtk_source_vim_state_select (state, &iter, &iter);
	}

	gtk_text_mark_set_visible (self->cursor, FALSE);
}

static void
gtk_source_vim_visual_resume (GtkSourceVimState *state,
                              GtkSourceVimState *from)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	self->handler = key_handler_initial;

	if (self->command_text->len > 0)
		g_string_truncate (self->command_text, self->command_text->len - 1);

	if (GTK_SOURCE_IS_VIM_MOTION (from))
	{
		GtkSourceVimState *chained;

		if (gtk_source_vim_motion_invalidates_visual_column (GTK_SOURCE_VIM_MOTION (from)))
		{
			track_visible_column (self);
		}

		/* Update our selection to match the motion. If we're in
		 * linewise, that needs to be updated to contain the whole line.
		 */
		gtk_source_vim_visual_track_motion (self);

		/* Keep the motion around too so we can potentially replay it
		 * for commands like delete, etc.
		 */
		chained = gtk_source_vim_motion_chain (self->motion, GTK_SOURCE_VIM_MOTION (from));
		gtk_source_vim_state_set_parent (chained, GTK_SOURCE_VIM_STATE (self));
		gtk_source_vim_state_reparent (chained, self, &self->motion);
		g_object_unref (chained);
	}

	update_cursor_visible (self);

	if (GTK_SOURCE_IS_VIM_COMMAND_BAR (from))
	{
		GtkSourceVimState *command = gtk_source_vim_command_bar_take_command (GTK_SOURCE_VIM_COMMAND_BAR (from));

		if (command != NULL && !self->ignore_command)
		{
			gtk_source_vim_state_reparent (command, self, &self->command);
			g_object_unref (command);
		}

		gtk_source_vim_state_unparent (from);

		if (self->ignore_command)
			self->ignore_command = FALSE;
		else
			gtk_source_vim_state_pop (state);
	}
	else if (from == self->command)
	{
		gtk_source_vim_state_pop (state);
	}
	else if (!GTK_SOURCE_IS_VIM_MOTION (from))
	{
		gtk_source_vim_state_unparent (from);
	}
}

static void
gtk_source_vim_visual_suspend (GtkSourceVimState *state,
                               GtkSourceVimState *to)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (to));

	update_cursor_visible (self);
}

static void
gtk_source_vim_visual_repeat (GtkSourceVimState *state)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));

	count = gtk_source_vim_state_get_count (state);
	buffer = gtk_source_vim_state_get_buffer (state, &iter, &selection);

	gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (buffer), self->cursor, &iter);
	gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (buffer), self->started_at, &iter);

	gtk_source_vim_visual_track_motion (self);

	do
	{
		if (self->motion != NULL)
		{
			gtk_source_vim_motion_set_mark (self->motion, self->cursor);
			gtk_source_vim_state_repeat (GTK_SOURCE_VIM_STATE (self->motion));
			gtk_source_vim_visual_track_motion (self);
			gtk_source_vim_motion_set_mark (self->motion, NULL);
		}

		if (self->command != NULL)
		{
			gtk_source_vim_state_repeat (self->command);
		}
	} while (--count > 0);
}

static gboolean
gtk_source_vim_visual_handle_keypress (GtkSourceVimState *state,
                                       guint              keyval,
                                       guint              keycode,
                                       GdkModifierType    mods,
                                       const char        *string)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;

	g_assert (GTK_SOURCE_IS_VIM_VISUAL (state));

	g_string_append (self->command_text, string);

	/* Leave insert mode if Escape/ctrl+[ was pressed */
	if (gtk_source_vim_state_is_escape (keyval, mods))
	{
		gtk_source_vim_visual_clear (self);
		gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));
		return TRUE;
	}

	/* Now handle our commands */
	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			case GDK_KEY_V:
				/* For the terminal users out there */
				gtk_source_vim_visual_put (GTK_SOURCE_VIM_VISUAL (state), TRUE);
				return TRUE;

			default:
				break;
		}
	}

	return self->handler (self, keyval, keycode, mods, string);
}

static void
gtk_source_vim_visual_append_command (GtkSourceVimState *state,
                                      GString           *string)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)state;

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
gtk_source_vim_visual_dispose (GObject *object)
{
	GtkSourceVimVisual *self = (GtkSourceVimVisual *)object;

	if (self->cursor)
	{
		GtkTextMark *mark = self->cursor;
		GtkTextBuffer *buffer = gtk_text_mark_get_buffer (mark);

		g_clear_weak_pointer (&self->cursor);
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), mark);
	}

	if (self->started_at)
	{
		GtkTextMark *mark = self->started_at;
		GtkTextBuffer *buffer = gtk_text_mark_get_buffer (mark);

		g_clear_weak_pointer (&self->started_at);
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), mark);
	}

	if (self->command_text != NULL)
	{
		g_string_free (self->command_text, TRUE);
		self->command_text = NULL;
	}

	gtk_source_vim_state_release (&self->motion);
	gtk_source_vim_state_release (&self->command);

	G_OBJECT_CLASS (gtk_source_vim_visual_parent_class)->dispose (object);
}

static void
gtk_source_vim_visual_class_init (GtkSourceVimVisualClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_visual_dispose;

	state_class->get_command_bar_text = gtk_source_vim_visual_get_command_bar_text;
	state_class->handle_keypress = gtk_source_vim_visual_handle_keypress;
	state_class->enter = gtk_source_vim_visual_enter;
	state_class->leave = gtk_source_vim_visual_leave;
	state_class->resume = gtk_source_vim_visual_resume;
	state_class->suspend = gtk_source_vim_visual_suspend;
	state_class->repeat = gtk_source_vim_visual_repeat;
	state_class->append_command = gtk_source_vim_visual_append_command;
}

static void
gtk_source_vim_visual_init (GtkSourceVimVisual *self)
{
	self->handler = key_handler_initial;
	self->command_text = g_string_new (NULL);
}

GtkSourceVimState *
gtk_source_vim_visual_new (GtkSourceVimVisualMode mode)
{
	GtkSourceVimVisual *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_VISUAL, NULL);
	self->mode = mode;

	return GTK_SOURCE_VIM_STATE (self);
}

gboolean
gtk_source_vim_visual_get_bounds (GtkSourceVimVisual *self,
                                  GtkTextIter        *cursor,
                                  GtkTextIter        *started_at)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_VISUAL (self), FALSE);

	if (cursor != NULL)
	{
		if (self->cursor == NULL)
			return FALSE;

		gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (self->cursor),
		                                  cursor, self->cursor);
	}

	if (started_at != NULL)
	{
		if (self->started_at == NULL)
			return FALSE;

		gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (self->started_at),
		                                  started_at, self->started_at);
	}

	return TRUE;
}

void
gtk_source_vim_visual_warp (GtkSourceVimVisual *self,
                            const GtkTextIter  *iter,
                            const GtkTextIter  *selection)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_VIM_VISUAL (self));
	g_return_if_fail (iter != NULL);

	buffer = gtk_text_mark_get_buffer (self->cursor);

	if (iter != NULL)
		gtk_text_buffer_move_mark (buffer, self->cursor, iter);

	if (selection != NULL)
		gtk_text_buffer_move_mark (buffer, self->started_at, selection);

	gtk_source_vim_visual_track_motion (self);

	update_cursor_visible (self);
}

GtkSourceVimState *
gtk_source_vim_visual_clone (GtkSourceVimVisual *self)
{
	GtkSourceVimState *ret;
	GtkTextIter cursor;
	GtkTextIter started_at;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_VISUAL (self), NULL);

	ret = gtk_source_vim_visual_new (self->mode);

	if (gtk_source_vim_visual_get_bounds (self, &cursor, &started_at))
	{
		GtkSourceBuffer *buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
		GtkTextMark *mark;

		mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &cursor, FALSE);
		g_set_weak_pointer (&GTK_SOURCE_VIM_VISUAL (ret)->cursor, mark);

		mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &started_at, TRUE);
		g_set_weak_pointer (&GTK_SOURCE_VIM_VISUAL (ret)->started_at, mark);
	}

	return ret;
}

void
gtk_source_vim_visual_ignore_command (GtkSourceVimVisual *self)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_VISUAL (self));

	self->ignore_command = TRUE;
}

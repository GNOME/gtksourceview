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

#include <gtksourceview/gtksourceview.h>
#include <gtksourceview/gtksourcesearchcontext.h>
#include <gtksourceview/gtksourcesearchsettings.h>

#include "gtksourcevimcharpending.h"
#include "gtksourcevimmotion.h"

typedef gboolean (*Motion) (GtkTextIter        *iter,
                            GtkSourceVimMotion *state);

typedef enum {
	INCLUSIVE = 0,
	EXCLUSIVE = 1,
} Inclusivity;

typedef enum {
	CHARWISE = 0,
	LINEWISE = 1,
} MotionWise;

struct _GtkSourceVimMotion
{
	GtkSourceVimState parent_instance;

	/* Text as it's typed for append_command() */
	GString *command_text;

	/* The mark to apply the motion to or NULL */
	GtkTextMark *mark;

	/* A function to apply the motion */
	Motion motion;

	/* An array of motions if this is a motion chain (such as those
	 * used by delete to replay Visual state motions.
	 */
	GPtrArray *chained;

	/* character for f or F */
	gunichar f_char;

	/* Where are we applying the :count, useful when you need
	 * to deal with empty lines and forward_to_line_end().
	 */
	int apply_count;

	/* If we need to alter the count of the motion by a value
	 * (typically used for things like yy dd and other things that
	 * are "this line" but can be repeated to extend). Therefore
	 * the value is generally either 0 or -1.
	 */
	int alter_count;

	/* If this is specified, we want to treat it like a `j` but
	 * with the count subtracted by one. Useful for yy, dd, etc.
	 */
	guint linewise_keyval;

	/* Apply the motion when leaving the state. This is useful
	 * so that you can either capture a motion for future use
	 * or simply apply it immediately.
	 */
	guint apply_on_leave : 1;

	/* If the command starts with `g` such as `ge` or `gE`. */
	guint g_command : 1;

	/* If we're in a [( or ]} type motion */
	guint bracket_right : 1;
	guint bracket_left : 1;

	/* If we called gtk_source_vim_motion_bail(). */
	guint failed : 1;

	/* If we just did f/F and need another char */
	guint waiting_for_f_char : 1;

	/* If the motion is exclusive (does not include char) */
	guint inclusivity : 1;

	/* If we're to apply inclusivity (used by chained motions) */
	guint applying_inclusive : 1;

	guint invalidates_visual_column : 1;

	/* Some motions are considered linewise when applying commands,
	 * generally when they land on a new line. Not all are, however, such
	 * as paragraph or sentence movements.
	 */
	MotionWise wise : 1;

	/* Moving to marks */
	guint mark_charwise : 1;
	guint mark_linewise : 1;

	/* If this motion is a "jump" (:help jumplist) */
	guint is_jump : 1;
};

G_DEFINE_TYPE (GtkSourceVimMotion, gtk_source_vim_motion, GTK_SOURCE_TYPE_VIM_STATE)

static inline int
get_adjusted_count (GtkSourceVimMotion *self)
{
	return gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self)) + self->alter_count;
}

static inline gboolean
iter_isspace (const GtkTextIter *iter)
{
	return g_unichar_isspace (gtk_text_iter_get_char (iter));
}

static inline gboolean
get_number (guint  keyval,
            int   *n)
{
	if (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
		*n = keyval - GDK_KEY_0;
	else if (keyval >= GDK_KEY_KP_0 && keyval <= GDK_KEY_KP_9)
		*n = keyval - GDK_KEY_KP_0;
	else
		return FALSE;
	return TRUE;
}

static gboolean
line_is_empty (GtkTextIter *iter)
{
	return gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter);
}

static gboolean
motion_none (GtkTextIter        *iter,
             GtkSourceVimMotion *self)
{
	return TRUE;
}

enum
{
	CLASS_0,
	CLASS_NEWLINE,
	CLASS_SPACE,
	CLASS_SPECIAL,
	CLASS_WORD,
};

static inline int
simple_word_classify (gunichar ch)
{
	switch (ch)
	{
		case ' ':
		case '\t':
		case '\n':
			return CLASS_SPACE;

		case '"': case '\'':
		case '(': case ')':
		case '{': case '}':
		case '[': case ']':
		case '<': case '>':
		case '-': case '+': case '*': case '/':
		case '!': case '@': case '#': case '$': case '%':
		case '^': case '&': case ':': case ';': case '?':
		case '|': case '=': case '\\': case '.': case ',':
			return CLASS_SPECIAL;

		case '_':
		default:
			return CLASS_WORD;
	}
}

static int
classify_word (gunichar           ch,
               const GtkTextIter *iter)
{
	return simple_word_classify (ch);
}

static int
classify_word_newline_stop (gunichar           ch,
                            const GtkTextIter *iter)
{
	if (gtk_text_iter_starts_line (iter) &&
	    gtk_text_iter_ends_line (iter))
		return CLASS_NEWLINE;

	return classify_word (ch, iter);
}

static int
classify_WORD (gunichar           ch,
               const GtkTextIter *iter)
{
	if (g_unichar_isspace (ch))
		return CLASS_SPACE;

	return CLASS_WORD;
}

static int
classify_WORD_newline_stop (gunichar           ch,
                            const GtkTextIter *iter)
{
	if (gtk_text_iter_starts_line (iter) &&
	    gtk_text_iter_ends_line (iter))
		return CLASS_NEWLINE;

	return classify_WORD (ch, iter);
}

static gboolean
forward_classified_start (GtkTextIter  *iter,
                          int         (*classify) (gunichar, const GtkTextIter *))
{
	gint begin_class;
	gint cur_class;
	gunichar ch;

	g_assert (iter);

	ch = gtk_text_iter_get_char (iter);
	begin_class = classify (ch, iter);

	/* Move to the first non-whitespace character if necessary. */
	if (begin_class == CLASS_SPACE)
	{
		for (;;)
		{
			if (!gtk_text_iter_forward_char (iter))
				return FALSE;

			ch = gtk_text_iter_get_char (iter);
			cur_class = classify (ch, iter);
			if (cur_class != CLASS_SPACE)
				return TRUE;
		}
	}

	/* move to first character not at same class level. */
	while (gtk_text_iter_forward_char (iter))
	{
		ch = gtk_text_iter_get_char (iter);
		cur_class = classify (ch, iter);

		if (cur_class == CLASS_SPACE)
		{
			begin_class = CLASS_0;
			continue;
		}

		if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
			return TRUE;
	}

	return FALSE;
}

static gboolean
forward_classified_end (GtkTextIter  *iter,
                        int         (*classify) (gunichar, const GtkTextIter *))
{
	gunichar ch;
	gint begin_class;
	gint cur_class;

	g_assert (iter);

	if (!gtk_text_iter_forward_char (iter))
		return FALSE;

	/* If we are on space, walk to the start of the next word. */
	ch = gtk_text_iter_get_char (iter);
	if (classify (ch, iter) == CLASS_SPACE)
		if (!forward_classified_start (iter, classify))
			return FALSE;

	ch = gtk_text_iter_get_char (iter);
	begin_class = classify (ch, iter);

	if (begin_class == CLASS_NEWLINE)
	{
		gtk_text_iter_backward_char (iter);
		return TRUE;
	}

	for (;;)
	{
		if (!gtk_text_iter_forward_char (iter))
			return FALSE;

		ch = gtk_text_iter_get_char (iter);
		cur_class = classify (ch, iter);

		if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
		{
			gtk_text_iter_backward_char (iter);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
backward_classified_end (GtkTextIter  *iter,
                         int         (*classify) (gunichar, const GtkTextIter *))
{
	gunichar ch;
	gint begin_class;
	gint cur_class;

	g_assert (iter);

	ch = gtk_text_iter_get_char (iter);
	begin_class = classify (ch, iter);

	if (begin_class == CLASS_NEWLINE)
	{
		gtk_text_iter_forward_char (iter);
		return TRUE;
	}

	for (;;)
	{
		if (!gtk_text_iter_backward_char (iter))
			return FALSE;

		ch = gtk_text_iter_get_char (iter);
		cur_class = classify (ch, iter);

		if (cur_class == CLASS_NEWLINE)
		{
			gtk_text_iter_forward_char (iter);
			return TRUE;
		}

		/* reset begin_class if we hit space, we can take anything after that */
		if (cur_class == CLASS_SPACE)
			begin_class = CLASS_SPACE;

		if (cur_class != begin_class && cur_class != CLASS_SPACE)
			return TRUE;
	}

	return FALSE;
}

static gboolean
backward_classified_start (GtkTextIter  *iter,
                           int         (*classify) (gunichar, const GtkTextIter *))
{
	gunichar ch;
	gint begin_class;
	gint cur_class;

	g_assert (iter);

	if (!gtk_text_iter_backward_char (iter))
		return FALSE;

	/* If we are on space, walk to the end of the previous word. */
	ch = gtk_text_iter_get_char (iter);
	if (classify (ch, iter) == CLASS_SPACE)
		if (!backward_classified_end (iter, classify))
			return FALSE;

	ch = gtk_text_iter_get_char (iter);
	begin_class = classify (ch, iter);

	for (;;)
	{
		if (!gtk_text_iter_backward_char (iter))
			return FALSE;

		ch = gtk_text_iter_get_char (iter);
		cur_class = classify (ch, iter);

		if (cur_class != begin_class || cur_class == CLASS_NEWLINE)
		{
			gtk_text_iter_forward_char (iter);
			return TRUE;
		}
	}

	return FALSE;
}

static void
get_iter_at_visual_column (GtkSourceView *view,
                           GtkTextIter   *iter,
                           guint          column)
{
	gunichar tab_char;
	guint visual_col = 0;
	guint tab_width;

	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (iter != NULL);

	tab_char = g_utf8_get_char ("\t");
	tab_width = gtk_source_view_get_tab_width (view);
	gtk_text_iter_set_line_offset (iter, 0);

	while (!gtk_text_iter_ends_line (iter))
	{
		if (gtk_text_iter_get_char (iter) == tab_char)
			visual_col += (tab_width - (visual_col % tab_width));
		else
			++visual_col;

		if (visual_col > column)
			break;

		/* This does not handle invisible text correctly, but
		 * gtk_text_iter_forward_visible_cursor_position is too slow.
		 */
		if (!gtk_text_iter_forward_char (iter))
			break;
	}
}

static gboolean
motion_line_start (GtkTextIter        *iter,
                   GtkSourceVimMotion *state)
{
	if (!gtk_text_iter_starts_line (iter))
	{
		gtk_text_iter_set_line_offset (iter, 0);
		return TRUE;
	}

	return FALSE;
}

static gboolean
motion_line_first_char (GtkTextIter        *iter,
                        GtkSourceVimMotion *state)
{
	if (!gtk_text_iter_starts_line (iter))
	{
		gtk_text_iter_set_line_offset (iter, 0);
	}

	while (!gtk_text_iter_ends_line (iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (iter)))
	{
		if (!gtk_text_iter_forward_char (iter))
		{
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
motion_forward_char_same_line_eol_okay (GtkTextIter        *iter,
                                        GtkSourceVimMotion *state)
{
	if (gtk_text_iter_ends_line (iter))
		return FALSE;
	return gtk_text_iter_forward_char (iter);
}

static gboolean
motion_forward_char (GtkTextIter        *iter,
                     GtkSourceVimMotion *state)
{
	GtkTextIter begin = *iter;

	gtk_text_iter_forward_char (iter);

	if (gtk_text_iter_ends_line (iter) && !gtk_text_iter_starts_line (iter))
	{
		if (gtk_text_iter_is_end (iter))
			gtk_text_iter_backward_char (iter);
		else
			gtk_text_iter_forward_char (iter);
	}

	return !gtk_text_iter_equal (&begin, iter);
}

static gboolean
motion_forward_char_same_line (GtkTextIter        *iter,
                               GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);

	if (self->apply_count != 1)
		return FALSE;

	count = MAX (1, count);

	for (guint i = 0; i < count; i++)
	{
		if (gtk_text_iter_ends_line (iter))
			break;

		if (!gtk_text_iter_forward_char (iter))
			break;
	}

	if (gtk_text_iter_ends_line (iter) && !gtk_text_iter_starts_line (iter))
		gtk_text_iter_backward_char (iter);

	return TRUE;
}

static gboolean
motion_backward_char (GtkTextIter        *iter,
                      GtkSourceVimMotion *state)
{
	GtkTextIter begin = *iter;

	if (gtk_text_iter_backward_char (iter))
	{
		if (gtk_text_iter_ends_line (iter) && !gtk_text_iter_starts_line (iter))
		{
			gtk_text_iter_backward_char (iter);
		}
	}

	return !gtk_text_iter_equal (&begin, iter);
}

static gboolean
motion_backward_char_same_line (GtkTextIter        *iter,
                                GtkSourceVimMotion *state)
{
	if (!gtk_text_iter_starts_line (iter))
	{
		return gtk_text_iter_backward_char (iter);
	}

	return FALSE;
}

static gboolean
motion_prev_line_end (GtkTextIter        *iter,
                      GtkSourceVimMotion *state)
{
	guint line = gtk_text_iter_get_line (iter);

	if (line == 0)
	{
		gtk_text_iter_set_offset (iter, 0);
		return TRUE;
	}

	gtk_text_buffer_get_iter_at_line (gtk_text_iter_get_buffer (iter), iter, line - 1);

	if (!gtk_text_iter_ends_line (iter))
		gtk_text_iter_forward_to_line_end (iter);

	/* Place on last character, not \n */
	if (!gtk_text_iter_starts_line (iter))
		gtk_text_iter_backward_char (iter);

	return TRUE;
}

static gboolean
motion_next_line_first_char (GtkTextIter        *iter,
                             GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	if (!gtk_text_iter_ends_line (iter))
		gtk_text_iter_forward_to_line_end (iter);

	gtk_text_iter_forward_char (iter);

	/* If we are on the same line, then we must be at the end of
	 * the buffer. Just move to one character before EOB.
	 */
	if (gtk_text_iter_get_line (&before) == gtk_text_iter_get_line (iter))
	{
		gtk_text_iter_forward_to_line_end (iter);
		if (!gtk_text_iter_starts_line (iter))
			gtk_text_iter_backward_char (iter);
		return !gtk_text_iter_equal (&before, iter);
	}

	while (!gtk_text_iter_ends_line (iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (iter)))
	{
		if (!gtk_text_iter_forward_char (iter))
			break;
	}

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_next_line_visual_column (GtkTextIter        *iter,
                                GtkSourceVimMotion *self)
{
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	int column = gtk_source_vim_state_get_visual_column (GTK_SOURCE_VIM_STATE (self));
	int count = get_adjusted_count (self);
	int line = gtk_text_iter_get_line (iter);

	self->invalidates_visual_column = FALSE;

	if (self->apply_count != 1 || count == 0)
		return FALSE;

	gtk_text_buffer_get_iter_at_line (buffer, iter, line + count);
	get_iter_at_visual_column (view, iter, column);

	if (!gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter))
	{
		gtk_text_iter_backward_char (iter);
	}

	return TRUE;
}

static gboolean
motion_prev_line_visual_column (GtkTextIter        *iter,
                                GtkSourceVimMotion *self)
{
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	int column = gtk_source_vim_state_get_visual_column (GTK_SOURCE_VIM_STATE (self));
	int count = get_adjusted_count (self);
	int line = gtk_text_iter_get_line (iter);

	self->invalidates_visual_column = FALSE;

	if (self->apply_count != 1 || count == 0)
		return FALSE;

	line = count > line ? 0 : line - count;
	gtk_text_buffer_get_iter_at_line (buffer, iter, line);
	get_iter_at_visual_column (view, iter, column);

	if (!gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter))
	{
		gtk_text_iter_backward_char (iter);
	}

	return TRUE;
}

static gboolean
motion_line_end (GtkTextIter        *iter,
                 GtkSourceVimMotion *state)
{
	GtkTextIter begin = *iter;

	if (!gtk_text_iter_ends_line (iter))
		gtk_text_iter_forward_to_line_end (iter);

	if (!gtk_text_iter_starts_line (iter))
		gtk_text_iter_backward_char (iter);

	return !gtk_text_iter_equal (&begin, iter);
}

static gboolean
motion_last_line_first_char (GtkTextIter        *iter,
                             GtkSourceVimMotion *state)
{
	gtk_text_buffer_get_end_iter (gtk_text_iter_get_buffer (iter), iter);
	gtk_text_iter_set_line_offset (iter, 0);
	while (!gtk_text_iter_is_end (iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (iter)))
		gtk_text_iter_forward_char (iter);
	return TRUE;
}

static gboolean
motion_screen_top (GtkTextIter        *iter,
                   GtkSourceVimMotion *state)
{
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (state));
	GdkRectangle visible;
	GdkRectangle rect;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), iter, visible.x, visible.y);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), iter, &rect);

	if (rect.y < visible.y)
	{
		gtk_text_iter_forward_line (iter);
	}

	return TRUE;
}

static gboolean
motion_screen_bottom (GtkTextIter        *iter,
                      GtkSourceVimMotion *state)
{
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (state));
	GdkRectangle visible;
	GdkRectangle rect;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), iter, visible.x, visible.y + visible.height);
	gtk_text_view_get_iter_location (GTK_TEXT_VIEW (view), iter, &rect);

	if (rect.y + rect.height > visible.y + visible.height)
	{
		gtk_text_iter_backward_line (iter);
	}

	return TRUE;
}

static gboolean
motion_screen_middle (GtkTextIter        *iter,
                      GtkSourceVimMotion *state)
{
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (state));
	GdkRectangle rect;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), iter, rect.x, rect.y + rect.height / 2);

	return TRUE;
}

static gboolean
motion_forward_word_start (GtkTextIter        *iter,
                           GtkSourceVimMotion *state)
{
	return forward_classified_start (iter, classify_word_newline_stop);
}

static gboolean
motion_forward_WORD_start (GtkTextIter        *iter,
                           GtkSourceVimMotion *state)
{
	return forward_classified_start (iter, classify_WORD_newline_stop);
}

static gboolean
motion_forward_word_end (GtkTextIter        *iter,
                         GtkSourceVimMotion *state)
{
	return forward_classified_end (iter, classify_word_newline_stop);
}

static gboolean
motion_forward_WORD_end (GtkTextIter        *iter,
                         GtkSourceVimMotion *state)
{
	return forward_classified_end (iter, classify_WORD_newline_stop);
}

static gboolean
motion_backward_word_start (GtkTextIter        *iter,
                            GtkSourceVimMotion *state)
{
	return backward_classified_start (iter, classify_word_newline_stop);
}

static gboolean
motion_backward_WORD_start (GtkTextIter        *iter,
                            GtkSourceVimMotion *state)
{
	return backward_classified_start (iter, classify_WORD_newline_stop);
}

static gboolean
motion_backward_word_end (GtkTextIter        *iter,
                          GtkSourceVimMotion *state)
{
	return backward_classified_end (iter, classify_word_newline_stop);
}

static gboolean
motion_backward_WORD_end (GtkTextIter        *iter,
                          GtkSourceVimMotion *state)
{
	return backward_classified_end (iter, classify_WORD_newline_stop);
}

static gboolean
motion_buffer_start (GtkTextIter        *iter,
                     GtkSourceVimMotion *state)
{
	if (!gtk_text_iter_is_start (iter))
	{
		gtk_text_iter_set_offset (iter, 0);
		return TRUE;
	}

	return FALSE;
}

static gboolean
motion_buffer_start_first_char (GtkTextIter        *iter,
                                GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	motion_buffer_start (iter, state);

	while (!gtk_text_iter_ends_line (iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (iter)))
	{
		if (!gtk_text_iter_forward_char (iter))
			break;
	}

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_f_char (GtkTextIter        *iter,
               GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	while (!gtk_text_iter_ends_line (iter))
	{
		if (!gtk_text_iter_forward_char (iter))
			break;

		if (gtk_text_iter_get_char (iter) == state->f_char)
			return TRUE;
	}

	*iter = before;

	return FALSE;
}

static gboolean
motion_F_char (GtkTextIter        *iter,
               GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	while (!gtk_text_iter_starts_line (iter))
	{
		if (!gtk_text_iter_backward_char (iter))
			break;

		if (gtk_text_iter_get_char (iter) == state->f_char)
			return TRUE;
	}

	*iter = before;

	return FALSE;
}

static gboolean
motion_forward_paragraph_end (GtkTextIter        *iter,
                              GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	/* Work our way past the current empty lines */
	if (line_is_empty (iter))
	{
		while (line_is_empty (iter))
		{
			if (!gtk_text_iter_forward_line (iter))
				return FALSE;
		}
	}

	/* Now find first line that is empty */
	while (!line_is_empty (iter))
	{
		if (!gtk_text_iter_forward_line (iter))
			return FALSE;
	}

	if (gtk_text_iter_is_end (iter) &&
	    !gtk_text_iter_starts_line (iter))
	{
		gtk_text_iter_backward_char (iter);
	}

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_backward_paragraph_start (GtkTextIter        *iter,
                                 GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;

	/* Work our way past the current empty lines */
	while (line_is_empty (iter))
	{
		if (!gtk_text_iter_backward_line (iter))
			goto finish;
	}

	/* Now find first line that is empty */
	while (!line_is_empty (iter))
	{
		if (!gtk_text_iter_backward_line (iter))
			goto finish;
	}

finish:
	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_forward_sentence_start (GtkTextIter        *iter,
                               GtkSourceVimMotion *state)
{
	GtkTextIter before = *iter;
	guint newline_count = 0;

	/* If we're at the end of a sentence, then walk past any trailing
	 * characters after the punctuation, and then skip space up until
	 * another non-space character.
	 */
	switch (gtk_text_iter_get_char (iter))
	{
		case '.':
		case '!':
		case '?':
		case '\n':
			while (!g_unichar_isspace (gtk_text_iter_get_char (iter)))
			{
				if (!gtk_text_iter_forward_char (iter))
					goto finish;
			}
			while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
			{
				if (!gtk_text_iter_forward_char (iter))
					goto finish;
			}
			return TRUE;

		default:
			break;
	}

	while (gtk_text_iter_forward_char (iter))
	{
		switch (gtk_text_iter_get_char (iter))
		{
			case '\n':
				newline_count++;
				if (newline_count == 1)
					break;
				G_GNUC_FALLTHROUGH;
			case '.':
			case '!':
			case '?':
				while (!g_unichar_isspace (gtk_text_iter_get_char (iter)))
				{
					if (!gtk_text_iter_forward_char (iter))
						goto finish;
				}
				while (g_unichar_isspace (gtk_text_iter_get_char (iter)))
				{
					if (!gtk_text_iter_forward_char (iter))
						goto finish;
				}
				return TRUE;

			default:
				break;
		}
	}

finish:
	if (gtk_text_iter_is_end (iter) && !gtk_text_iter_starts_line (iter))
		gtk_text_iter_backward_char (iter);

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
backward_sentence_end (GtkTextIter *iter)
{
	GtkTextIter before = *iter;

	if (line_is_empty (iter))
	{
		while (gtk_text_iter_backward_char (iter))
		{
			if (!g_unichar_isspace (gtk_text_iter_get_char (iter)))
				break;
		}

		goto finish;
	}

	while (gtk_text_iter_backward_char (iter))
	{
		switch (gtk_text_iter_get_char (iter))
		{
			case '.':
			case '!':
			case '?':
				goto finish;

			case '\n':
				if (gtk_text_iter_starts_line (iter))
				{
					while (gtk_text_iter_backward_char (iter))
					{
						if (!g_unichar_isspace (gtk_text_iter_get_char (iter)))
							break;
					}

					goto finish;
				}
				break;

			default:
				break;
		}
	}

finish:
	if (gtk_text_iter_is_end (iter) && !gtk_text_iter_starts_line (iter))
		gtk_text_iter_backward_char (iter);

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_backward_sentence_start (GtkTextIter        *iter,
                                GtkSourceVimMotion *state)
{
	GtkTextIter *winner = NULL;
	GtkTextIter before = *iter;
	GtkTextIter para;
	GtkTextIter sentence;
	GtkTextIter two_sentence;
	int distance = G_MAXINT;

	para = *iter;
	motion_backward_paragraph_start (&para, state);

	sentence = *iter;
	backward_sentence_end (&sentence);
	motion_forward_sentence_start (&sentence, state);

	two_sentence = *iter;
	backward_sentence_end (&two_sentence);
	backward_sentence_end (&two_sentence);
	motion_forward_sentence_start (&two_sentence, state);

	if (gtk_text_iter_compare (&para, iter) < 0)
	{
		int diff = (int)gtk_text_iter_get_offset (iter) - (int)gtk_text_iter_get_offset (&para);

		if (diff < distance)
		{
			distance = diff;
			winner = &para;
		}
	}

	if (gtk_text_iter_compare (&sentence, iter) < 0)
	{
		int diff = (int)gtk_text_iter_get_offset (iter) - (int)gtk_text_iter_get_offset (&sentence);

		if (diff < distance)
		{
			distance = diff;
			winner = &sentence;
		}
	}

	if (gtk_text_iter_compare (&two_sentence, iter) < 0)
	{
		int diff = (int)gtk_text_iter_get_offset (iter) - (int)gtk_text_iter_get_offset (&two_sentence);

		if (diff < distance)
		{
			distance = diff;
			winner = &two_sentence;
		}
	}

	if (winner != NULL)
		*iter = *winner;
	else
		gtk_text_iter_set_offset (iter, 0);

	return !gtk_text_iter_equal (&before, iter);
}

static gboolean
motion_next_scroll_page (GtkTextIter        *iter,
                         GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_page (GTK_SOURCE_VIM_STATE (self), count);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);

	return TRUE;
}

static gboolean
motion_prev_scroll_page (GtkTextIter        *iter,
                         GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_page (GTK_SOURCE_VIM_STATE (self), -count);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);
	return TRUE;
}

static gboolean
motion_next_scroll_half_page (GtkTextIter        *iter,
                              GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_half_page (GTK_SOURCE_VIM_STATE (self), count);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);

	return TRUE;
}

static gboolean
motion_prev_scroll_half_page (GtkTextIter        *iter,
                              GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_half_page (GTK_SOURCE_VIM_STATE (self), -count);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);
	return TRUE;
}

static gboolean
motion_prev_scroll_line (GtkTextIter        *iter,
                         GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	GtkTextIter loc;
	GdkRectangle rect;

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_line (GTK_SOURCE_VIM_STATE (self), -count);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &loc, rect.x + rect.width, rect.y + rect.height);

	if (gtk_text_iter_compare (&loc, iter) < 0)
	{
		gtk_text_iter_set_line (iter, gtk_text_iter_get_line (&loc));
	}

	return TRUE;
}

static gboolean
motion_next_scroll_line (GtkTextIter        *iter,
                         GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	GtkTextBuffer *buffer = gtk_text_iter_get_buffer (iter);
	GtkTextMark *insert = gtk_text_buffer_get_insert (buffer);
	GtkSourceView *view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
	GtkTextIter loc;
	GdkRectangle rect;

	if (self->apply_count != 1)
		return FALSE;

	gtk_source_vim_state_scroll_line (GTK_SOURCE_VIM_STATE (self), count);
	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &rect);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), iter, insert);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), &loc, rect.x, rect.y);

	if (gtk_text_iter_compare (&loc, iter) > 0)
	{
		gtk_text_iter_set_line (iter, gtk_text_iter_get_line (&loc));

		if (!gtk_text_iter_ends_line (iter))
		{
			gtk_text_iter_forward_to_line_end (iter);
		}

		if (gtk_text_iter_ends_line (iter) &&
		    !gtk_text_iter_starts_line (iter))
			gtk_text_iter_backward_char (iter);
	}

	return TRUE;
}

static gboolean
motion_line_number (GtkTextIter        *iter,
                    GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);

	if (self->apply_count != 1)
		return FALSE;

	if (count > 0)
		count--;

	gtk_text_iter_set_line (iter, count);

	while (!gtk_text_iter_ends_line (iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (iter)) &&
	       gtk_text_iter_forward_char (iter))
	{
		/* Do Nothing */
	}

	return TRUE;
}

static char *
word_under_cursor (const GtkTextIter *iter)
{
	GtkTextIter begin, end;

	end = *iter;
	if (!gtk_source_vim_iter_ends_word (&end))
	{
		if (!gtk_source_vim_iter_forward_word_end (&end))
			return FALSE;
	}

	begin = end;
	if (!gtk_source_vim_iter_starts_word (&begin))
	{
		gtk_source_vim_iter_backward_word_start (&begin);
	}

	gtk_text_iter_forward_char (&end);

	return gtk_text_iter_get_slice (&begin, &end);
}

static char *
WORD_under_cursor (const GtkTextIter *iter)
{
	GtkTextIter begin, end;

	end = *iter;
	if (!gtk_source_vim_iter_ends_WORD (&end))
	{
		if (!gtk_source_vim_iter_forward_WORD_end (&end))
			return FALSE;
	}

	begin = end;
	if (!gtk_source_vim_iter_starts_WORD (&begin))
	{
		gtk_source_vim_iter_backward_WORD_start (&begin);
	}

	gtk_text_iter_forward_char (&end);

	return gtk_text_iter_get_slice (&begin, &end);
}

static gboolean
motion_search (GtkTextIter        *iter,
               GtkSourceVimMotion *self,
               gboolean            WORD,
               gboolean            reverse)
{
	GtkSourceSearchContext *context;
	GtkSourceSearchSettings *settings;
	const char *search_text;
	char *word;
	gboolean has_wrapped_around;
	gboolean ret = FALSE;
	int count;

	g_assert (iter != NULL);
	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	if (self->apply_count != 1)
	{
		return FALSE;
	}

	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), &settings, &context);
	gtk_source_vim_state_set_reverse_search (GTK_SOURCE_VIM_STATE (self), reverse);

	if (!gtk_source_search_settings_get_at_word_boundaries (settings))
	{
		gtk_source_search_settings_set_at_word_boundaries (settings, TRUE);
	}

	word = WORD ? WORD_under_cursor (iter) : word_under_cursor (iter);
	search_text = gtk_source_search_settings_get_search_text (settings);

	if (g_strcmp0 (word, search_text) != 0)
	{
		gtk_source_search_settings_set_search_text (settings, word);
	}

	if (!reverse)
	{
		gtk_text_iter_forward_char (iter);
	}

	g_free (word);

	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	for (guint i = 0; i < count; i++)
	{
		gboolean matched;

		if (reverse)
			 matched = gtk_source_search_context_backward (context, iter, iter, NULL, &has_wrapped_around);
		else
			 matched = gtk_source_search_context_forward (context, iter, iter, NULL, &has_wrapped_around);

		if (!matched)
			break;

		ret = TRUE;
	}

	gtk_source_search_context_set_highlight (context, ret);

	return ret;
}

static gboolean
motion_forward_search_word (GtkTextIter        *iter,
                            GtkSourceVimMotion *self)
{
	return motion_search (iter, self, FALSE, FALSE);
}

static gboolean
motion_backward_search_word (GtkTextIter        *iter,
                             GtkSourceVimMotion *self)
{
	return motion_search (iter, self, FALSE, TRUE);
}

static gboolean
motion_next_search (GtkTextIter        *iter,
                    GtkSourceVimMotion *self)
{
	GtkSourceSearchContext *context;
	gboolean has_wrapped_around;
	gboolean matched;

	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), NULL, &context);

	gtk_text_iter_forward_char (iter);

	matched = gtk_source_search_context_forward (context, iter, iter, NULL, &has_wrapped_around);

	gtk_source_search_context_set_highlight (context, matched);

	return matched;
}

static gboolean
motion_prev_search (GtkTextIter        *iter,
                    GtkSourceVimMotion *self)
{
	GtkSourceSearchContext *context;
	gboolean has_wrapped_around;
	gboolean matched;

	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), NULL, &context);

	matched = gtk_source_search_context_backward (context, iter, iter, NULL, &has_wrapped_around);

	gtk_source_search_context_set_highlight (context, matched);

	return matched;
}

GtkSourceVimState *
gtk_source_vim_motion_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
}

static gboolean
gtk_source_vim_motion_bail (GtkSourceVimMotion *self)
{
	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	g_string_truncate (self->command_text, 0);

	self->failed = TRUE;
	gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));

	return TRUE;
}

static gboolean
gtk_source_vim_motion_complete (GtkSourceVimMotion *self,
                                Motion              motion,
                                Inclusivity         inclusivity,
                                MotionWise          wise)
{
	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	self->motion = motion;
	self->inclusivity = inclusivity;
	self->wise = wise;

	g_string_truncate (self->command_text, 0);

	gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));

	return TRUE;
}

static gboolean
gtk_source_vim_motion_begin_char_pending (GtkSourceVimMotion *self,
                                          Motion              motion,
                                          Inclusivity         inclusivity,
                                          MotionWise          wise)
{
	GtkSourceVimState *char_pending;

	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));
	g_assert (motion != NULL);

	self->motion = motion;
	self->inclusivity = inclusivity;
	self->wise = wise;

	char_pending = gtk_source_vim_char_pending_new ();
	gtk_source_vim_state_push (GTK_SOURCE_VIM_STATE (self), char_pending);

	return TRUE;
}

static gboolean
motion_bracket (GtkTextIter        *iter,
                GtkSourceVimMotion *self)
{
	GtkTextIter orig = *iter;

	if (self->bracket_left)
	{
		gtk_text_iter_backward_char (iter);

		if (self->f_char == '(')
		{
			if (gtk_source_vim_iter_backward_block_paren_start (iter))
				return TRUE;
		}

		if (self->f_char == '{')
		{
			if (gtk_source_vim_iter_backward_block_brace_start (iter))
				return TRUE;
		}
	}
	else
	{
		if (self->f_char == ')')
		{
			if (gtk_source_vim_iter_forward_block_paren_end (iter))
				return TRUE;
		}

		if (self->f_char == '}')
		{
			if (gtk_source_vim_iter_forward_block_brace_end (iter))
				return TRUE;
		}
	}

	*iter = orig;

	return FALSE;
}

static gboolean
motion_matching_char (GtkTextIter        *iter,
                      GtkSourceVimMotion *self)
{
	GtkTextIter orig = *iter;
	gunichar ch = gtk_text_iter_get_char (iter);
	gboolean ret;

	switch (ch)
	{
		case '(':
			ret = gtk_source_vim_iter_forward_block_paren_end (iter);
			break;

		case ')':
			ret = gtk_source_vim_iter_backward_block_paren_start (iter);
			break;

		case '[':
			ret = gtk_source_vim_iter_forward_block_bracket_end (iter);
			break;

		case ']':
			ret = gtk_source_vim_iter_backward_block_bracket_start (iter);
			break;

		case '{':
			ret = gtk_source_vim_iter_forward_block_brace_end (iter);
			break;

		case '}':
			ret = gtk_source_vim_iter_backward_block_brace_start (iter);
			break;

		default:
			/* TODO: check for #if/#ifdef/#elif/#else/#endif */
			ret = FALSE;
			break;
	}

	if (!ret)
		*iter = orig;

	return ret;
}

static gboolean
motion_mark (GtkTextIter        *iter,
             GtkSourceVimMotion *self)
{
	char str[8];

	str[g_unichar_to_utf8 (self->f_char, str)] = 0;

	if (gtk_source_vim_state_get_iter_at_mark (GTK_SOURCE_VIM_STATE (self), str, iter))
	{
		if (self->mark_linewise)
		{
			gtk_text_iter_set_line_offset (iter, 0);
			while (!gtk_text_iter_ends_line (iter) && iter_isspace (iter))
				gtk_text_iter_forward_char (iter);
		}

		return TRUE;
	}

	return FALSE;
}

static gboolean
gtk_source_vim_motion_handle_keypress (GtkSourceVimState *state,
                                       guint              keyval,
                                       guint              keycode,
                                       GdkModifierType    mods,
                                       const char        *string)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)state;
	int count;
	int n;

	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	g_string_append (self->command_text, string);

	count = gtk_source_vim_state_get_count (state);

	if (self->waiting_for_f_char)
	{
		if (string == NULL || string[0] == 0)
			return gtk_source_vim_motion_bail (self);

		self->f_char = g_utf8_get_char (string);
		gtk_source_vim_state_pop (state);
		return TRUE;
	}

	if (self->g_command)
	{
		switch (keyval)
		{
			case GDK_KEY_g:
				self->is_jump = TRUE;
				return gtk_source_vim_motion_complete (self, motion_buffer_start_first_char, INCLUSIVE, LINEWISE);

			case GDK_KEY_e:
				return gtk_source_vim_motion_complete (self, motion_backward_word_end, INCLUSIVE, CHARWISE);

			case GDK_KEY_E:
				return gtk_source_vim_motion_complete (self, motion_backward_WORD_end, INCLUSIVE, CHARWISE);

			default:
				return gtk_source_vim_motion_bail (self);
		}

		g_assert_not_reached ();
	}

	if (self->bracket_left || self->bracket_right)
	{
		switch (keyval)
		{
			case GDK_KEY_parenleft:
				self->f_char = '(';
				self->is_jump = TRUE;
				return gtk_source_vim_motion_complete (self, motion_bracket, INCLUSIVE, CHARWISE);

			case GDK_KEY_parenright:
				self->f_char = ')';
				self->is_jump = TRUE;
				return gtk_source_vim_motion_complete (self, motion_bracket, INCLUSIVE, CHARWISE);

			case GDK_KEY_braceleft:
				self->f_char = '{';
				self->is_jump = TRUE;
				return gtk_source_vim_motion_complete (self, motion_bracket, INCLUSIVE, CHARWISE);

			case GDK_KEY_braceright:
				self->f_char = '}';
				self->is_jump = TRUE;
				return gtk_source_vim_motion_complete (self, motion_bracket, INCLUSIVE, CHARWISE);

			case GDK_KEY_M:
			case GDK_KEY_m:
				/* TODO: support next method */
			default:
				break;
		}
	}

	if (self->mark_linewise || self->mark_charwise)
	{
		GtkTextIter iter;

		/* Make sure we found the mark */
		if (!gtk_source_vim_state_get_iter_at_mark (state, string, &iter))
			return gtk_source_vim_motion_bail (self);

		self->f_char = string[0];

		if (self->mark_linewise)
			return gtk_source_vim_motion_complete (self, motion_mark, INCLUSIVE, LINEWISE);
		else
			return gtk_source_vim_motion_complete (self, motion_mark, EXCLUSIVE, CHARWISE);
	}

	if (gtk_source_vim_state_get_count_set (state) && get_number (keyval, &n))
	{
		count = count * 10 + n;
		gtk_source_vim_state_set_count (state, count);
		return TRUE;
	}

	if ((mods & GDK_CONTROL_MASK) != 0)
	{
		switch (keyval)
		{
			/* Technically, none of these are usable with commands
			 * like d{motion} and therefore may require some extra
			 * tweaking to see how we use them.
			 */
			case GDK_KEY_f:
				return gtk_source_vim_motion_complete (self, motion_next_scroll_page, INCLUSIVE, LINEWISE);

			case GDK_KEY_b:
				return gtk_source_vim_motion_complete (self, motion_prev_scroll_page, INCLUSIVE, LINEWISE);

			case GDK_KEY_e:
				return gtk_source_vim_motion_complete (self, motion_next_scroll_line, INCLUSIVE, LINEWISE);

			case GDK_KEY_y:
				return gtk_source_vim_motion_complete (self, motion_prev_scroll_line, INCLUSIVE, LINEWISE);

			case GDK_KEY_u:
				return gtk_source_vim_motion_complete (self, motion_prev_scroll_half_page, INCLUSIVE, LINEWISE);

			case GDK_KEY_d:
				return gtk_source_vim_motion_complete (self, motion_next_scroll_half_page, INCLUSIVE, LINEWISE);

			default:
				break;
		}
	}

	if (keyval != 0 && keyval == self->linewise_keyval)
	{
		self->motion = motion_next_line_visual_column;
		self->inclusivity = EXCLUSIVE;
		self->wise = LINEWISE;
		self->alter_count = -1;
		g_string_truncate (self->command_text, 0);
		gtk_source_vim_state_pop (GTK_SOURCE_VIM_STATE (self));
		return TRUE;
	}

	switch (keyval)
	{
		case GDK_KEY_0:
		case GDK_KEY_KP_0:
		case GDK_KEY_Home:
		case GDK_KEY_bar:
			return gtk_source_vim_motion_complete (self, motion_line_start, INCLUSIVE, CHARWISE);

		case GDK_KEY_1: case GDK_KEY_KP_1:
		case GDK_KEY_2: case GDK_KEY_KP_2:
		case GDK_KEY_3: case GDK_KEY_KP_3:
		case GDK_KEY_4: case GDK_KEY_KP_4:
		case GDK_KEY_5: case GDK_KEY_KP_5:
		case GDK_KEY_6: case GDK_KEY_KP_6:
		case GDK_KEY_7: case GDK_KEY_KP_7:
		case GDK_KEY_8: case GDK_KEY_KP_8:
		case GDK_KEY_9: case GDK_KEY_KP_9:
			get_number (keyval, &n);
			gtk_source_vim_state_set_count (state, n);
			return TRUE;

		case GDK_KEY_asciicircum:
		case GDK_KEY_underscore:
			return gtk_source_vim_motion_complete (self, motion_line_first_char, INCLUSIVE, CHARWISE);

		case GDK_KEY_space:
			return gtk_source_vim_motion_complete (self, motion_forward_char, EXCLUSIVE, CHARWISE);

		case GDK_KEY_BackSpace:
			return gtk_source_vim_motion_complete (self, motion_backward_char, INCLUSIVE, CHARWISE);

		case GDK_KEY_Left:
		case GDK_KEY_h:
			return gtk_source_vim_motion_complete (self, motion_backward_char_same_line, INCLUSIVE, CHARWISE);

		case GDK_KEY_Right:
		case GDK_KEY_l:
			return gtk_source_vim_motion_complete (self, motion_forward_char_same_line, EXCLUSIVE, CHARWISE);

		case GDK_KEY_ISO_Enter:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_Return:
			return gtk_source_vim_motion_complete (self, motion_next_line_first_char, EXCLUSIVE, LINEWISE);

		case GDK_KEY_End:
		case GDK_KEY_dollar:
			return gtk_source_vim_motion_complete (self, motion_line_end, INCLUSIVE, CHARWISE);

		case GDK_KEY_Down:
		case GDK_KEY_j:
			return gtk_source_vim_motion_complete (self, motion_next_line_visual_column, EXCLUSIVE, LINEWISE);

		case GDK_KEY_Up:
		case GDK_KEY_k:
			return gtk_source_vim_motion_complete (self, motion_prev_line_visual_column, INCLUSIVE, LINEWISE);

		case GDK_KEY_G:
			self->is_jump = TRUE;
			if (gtk_source_vim_state_get_count_set (state))
				return gtk_source_vim_motion_complete (self, motion_line_number, INCLUSIVE, LINEWISE);
			return gtk_source_vim_motion_complete (self, motion_last_line_first_char, INCLUSIVE, LINEWISE);

		case GDK_KEY_g:
			self->g_command = TRUE;
			return TRUE;

		case GDK_KEY_H:
			self->is_jump = TRUE;
			return gtk_source_vim_motion_complete (self, motion_screen_top, INCLUSIVE, LINEWISE);

		case GDK_KEY_M:
			self->is_jump = TRUE;
			return gtk_source_vim_motion_complete (self, motion_screen_middle, INCLUSIVE, LINEWISE);

		case GDK_KEY_L:
			self->is_jump = TRUE;
			return gtk_source_vim_motion_complete (self, motion_screen_bottom, INCLUSIVE, LINEWISE);

		case GDK_KEY_w:
			return gtk_source_vim_motion_complete (self, motion_forward_word_start, EXCLUSIVE, CHARWISE);

		case GDK_KEY_W:
			return gtk_source_vim_motion_complete (self, motion_forward_WORD_start, EXCLUSIVE, CHARWISE);

		case GDK_KEY_b:
			return gtk_source_vim_motion_complete (self, motion_backward_word_start, INCLUSIVE, CHARWISE);

		case GDK_KEY_B:
			return gtk_source_vim_motion_complete (self, motion_backward_WORD_start, INCLUSIVE, CHARWISE);

		case GDK_KEY_e:
			return gtk_source_vim_motion_complete (self, motion_forward_word_end, INCLUSIVE, CHARWISE);

		case GDK_KEY_E:
			return gtk_source_vim_motion_complete (self, motion_forward_WORD_end, INCLUSIVE, CHARWISE);

		case GDK_KEY_f:
			return gtk_source_vim_motion_begin_char_pending (self, motion_f_char, INCLUSIVE, CHARWISE);

		case GDK_KEY_F:
			return gtk_source_vim_motion_begin_char_pending (self, motion_F_char, INCLUSIVE, CHARWISE);

		case GDK_KEY_t:
			return gtk_source_vim_motion_begin_char_pending (self, motion_f_char, EXCLUSIVE, CHARWISE);

		case GDK_KEY_T:
			return gtk_source_vim_motion_begin_char_pending (self, motion_F_char, EXCLUSIVE, CHARWISE);

		case GDK_KEY_parenleft:
			return gtk_source_vim_motion_complete (self, motion_backward_sentence_start, INCLUSIVE, CHARWISE);

		case GDK_KEY_parenright:
			return gtk_source_vim_motion_complete (self, motion_forward_sentence_start, EXCLUSIVE, CHARWISE);

		case GDK_KEY_braceleft:
			return gtk_source_vim_motion_complete (self, motion_backward_paragraph_start, INCLUSIVE, CHARWISE);

		case GDK_KEY_braceright:
			return gtk_source_vim_motion_complete (self, motion_forward_paragraph_end, EXCLUSIVE, CHARWISE);

		case GDK_KEY_asterisk:
		case GDK_KEY_KP_Multiply:
			return gtk_source_vim_motion_complete (self, motion_forward_search_word, EXCLUSIVE, CHARWISE);

		case GDK_KEY_numbersign:
			return gtk_source_vim_motion_complete (self, motion_backward_search_word, INCLUSIVE, CHARWISE);

		case GDK_KEY_n:
			self->is_jump = TRUE;
			if (gtk_source_vim_state_get_reverse_search (GTK_SOURCE_VIM_STATE (self)))
				return gtk_source_vim_motion_complete (self, motion_prev_search, INCLUSIVE, CHARWISE);
			else
				return gtk_source_vim_motion_complete (self, motion_next_search, INCLUSIVE, CHARWISE);

		case GDK_KEY_N:
			self->is_jump = TRUE;
			if (gtk_source_vim_state_get_reverse_search (GTK_SOURCE_VIM_STATE (self)))
				return gtk_source_vim_motion_complete (self, motion_next_search, INCLUSIVE, CHARWISE);
			else
				return gtk_source_vim_motion_complete (self, motion_prev_search, INCLUSIVE, CHARWISE);

		case GDK_KEY_bracketleft:
			self->bracket_left = TRUE;
			return TRUE;

		case GDK_KEY_bracketright:
			self->bracket_right = TRUE;
			return TRUE;

		case GDK_KEY_percent:
			self->is_jump = TRUE;
			return gtk_source_vim_motion_complete (self, motion_matching_char, EXCLUSIVE, CHARWISE);

		case GDK_KEY_grave:
			self->is_jump = TRUE;
			self->mark_charwise = TRUE;
			return TRUE;

		case GDK_KEY_apostrophe:
			self->is_jump = TRUE;
			self->mark_linewise = TRUE;
			return TRUE;

		default:
			return gtk_source_vim_motion_bail (self);
	}

	return FALSE;
}

static void
gtk_source_vim_motion_repeat (GtkSourceVimState *state)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)state;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter;
	int count;

	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	if (self->failed)
	{
		return;
	}

	view = gtk_source_vim_state_get_view (state);
	buffer = gtk_source_vim_state_get_buffer (state, &iter, NULL);
	count = get_adjusted_count (self);

	if (self->mark != NULL)
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
		                                  &iter,
		                                  self->mark);
	}

	do
	{
		if (!gtk_source_vim_motion_apply (self, &iter, FALSE))
			break;
	} while (--count > 0);

	if (self->mark != NULL)
	{
		gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (buffer),
		                           self->mark,
		                           &iter);
	}
	else
	{
		gtk_source_vim_state_select (state, &iter, &iter);
	}

	gtk_text_view_reset_im_context (GTK_TEXT_VIEW (view));
	gtk_text_view_reset_cursor_blink (GTK_TEXT_VIEW (view));
}

static void
gtk_source_vim_motion_leave (GtkSourceVimState *state)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)state;

	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));

	if (self->apply_on_leave)
	{
		/* If this motion is a jump, then add it to the jumplist */
		if (self->is_jump)
		{
			GtkTextIter origin;

			gtk_source_vim_state_get_buffer (state, &origin, NULL);
			gtk_source_vim_state_push_jump (state, &origin);
		}

		gtk_source_vim_motion_repeat (state);
	}
}

static void
gtk_source_vim_motion_resume (GtkSourceVimState *state,
                              GtkSourceVimState *from)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)state;

	g_assert (GTK_SOURCE_IS_VIM_STATE (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	if (GTK_SOURCE_IS_VIM_CHAR_PENDING (from))
	{
		GtkSourceVimCharPending *pending = GTK_SOURCE_VIM_CHAR_PENDING (from);
		gunichar ch = gtk_source_vim_char_pending_get_character (pending);
		const char *str = gtk_source_vim_char_pending_get_string (pending);

		self->f_char = ch;
		g_string_append (self->command_text, str);
		gtk_source_vim_state_unparent (from);
		gtk_source_vim_state_pop (state);
		return;
	}

	gtk_source_vim_state_unparent (from);
}

static void
gtk_source_vim_motion_append_command (GtkSourceVimState *state,
                                      GString           *string)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)state;

	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));
	g_assert (state != NULL);

	if (self->command_text->len > 0)
	{
		g_string_append_len (string,
		                     self->command_text->str,
		                     self->command_text->len);
	}
}

static void
gtk_source_vim_motion_dispose (GObject *object)
{
	GtkSourceVimMotion *self = (GtkSourceVimMotion *)object;

	g_clear_pointer (&self->chained, g_ptr_array_unref);

	g_clear_object (&self->mark);

	if (self->command_text != NULL)
	{
		g_string_free (self->command_text, TRUE);
		self->command_text = NULL;
	}

	G_OBJECT_CLASS (gtk_source_vim_motion_parent_class)->dispose (object);
}

static void
gtk_source_vim_motion_class_init (GtkSourceVimMotionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);

	object_class->dispose = gtk_source_vim_motion_dispose;

	state_class->append_command = gtk_source_vim_motion_append_command;
	state_class->handle_keypress = gtk_source_vim_motion_handle_keypress;
	state_class->leave = gtk_source_vim_motion_leave;
	state_class->repeat = gtk_source_vim_motion_repeat;
	state_class->resume = gtk_source_vim_motion_resume;
}

static void
gtk_source_vim_motion_init (GtkSourceVimMotion *self)
{
	self->apply_on_leave = TRUE;
	self->command_text = g_string_new (NULL);
	self->invalidates_visual_column = TRUE;
	self->wise = CHARWISE;
	self->inclusivity = INCLUSIVE;
}

gboolean
gtk_source_vim_motion_apply (GtkSourceVimMotion *self,
                             GtkTextIter        *iter,
                             gboolean            apply_inclusive)
{
	gboolean ret = FALSE;
	guint begin_offset;
	int count;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MOTION (self), FALSE);

	if (self->motion == NULL || self->failed)
	{
		return FALSE;
	}

	self->applying_inclusive = !!apply_inclusive;

	begin_offset = gtk_text_iter_get_offset (iter);
	count = get_adjusted_count (self);

	do
	{
		self->apply_count++;
		if (!self->motion (iter, self))
			goto do_inclusive;
	} while (--count > 0);

	ret = TRUE;

do_inclusive:
	self->apply_count = 0;

	if (apply_inclusive)
	{
		guint end_offset = gtk_text_iter_get_offset (iter);

		if (FALSE) {}
		else if (self->inclusivity == INCLUSIVE &&
			 end_offset > begin_offset &&
			 !gtk_text_iter_ends_line (iter))
			gtk_text_iter_forward_char (iter);
		else if (self->inclusivity == EXCLUSIVE
			 && end_offset < begin_offset &&
			 !gtk_text_iter_ends_line (iter))
			gtk_text_iter_forward_char (iter);
	}

	self->applying_inclusive = FALSE;

	return ret;
}

gboolean
gtk_source_vim_motion_get_apply_on_leave (GtkSourceVimMotion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MOTION (self), FALSE);

	return self->apply_on_leave;
}

void
gtk_source_vim_motion_set_apply_on_leave (GtkSourceVimMotion *self,
                                          gboolean            apply_on_leave)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_MOTION (self));

	self->apply_on_leave = !!apply_on_leave;
}

void
gtk_source_vim_motion_set_mark (GtkSourceVimMotion *self,
                                GtkTextMark        *mark)
{
	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));
	g_assert (!mark || GTK_IS_TEXT_MARK (mark));

	g_set_object (&self->mark, mark);
}

GtkSourceVimState *
gtk_source_vim_motion_new_first_char (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_line_first_char;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_line_end (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_line_end;
	self->inclusivity = INCLUSIVE;
	self->wise = CHARWISE;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_line_start (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_line_start;
	self->inclusivity = INCLUSIVE;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_previous_line_end (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_prev_line_end;
	self->inclusivity = EXCLUSIVE;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_forward_char (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_forward_char_same_line_eol_okay;
	self->inclusivity = EXCLUSIVE;

	return GTK_SOURCE_VIM_STATE (self);
}

static gboolean
do_motion_line_end_with_nl (GtkTextIter *iter,
                            int          apply_count,
                            int          count)
{
	/* This function has to take into account newlines so that we
	 * can move and delete whole lines. It is extra complicated
	 * because we can't actually move when we have an empty line.
	 * So we know our :count to apply and can do it in one pass
	 * and rely on subsequent calls to be idempotent. When applying
	 * we get the same result and need not worry about the impedance
	 * mismatch with VIM character movements.
	 */

	if (apply_count != 1)
		return FALSE;

	if (count == 1)
	{
		if (gtk_text_iter_ends_line (iter))
			return TRUE;

		return gtk_text_iter_forward_to_line_end (iter);
	}

	gtk_text_iter_set_line (iter, gtk_text_iter_get_line (iter) + count - 1);

	if (!gtk_text_iter_ends_line (iter))
		gtk_text_iter_forward_to_line_end (iter);

	return TRUE;
}

static gboolean
motion_line_end_with_nl (GtkTextIter        *iter,
                         GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	return do_motion_line_end_with_nl (iter, self->apply_count, count);
}

static gboolean
motion_next_line_end_with_nl (GtkTextIter        *iter,
                              GtkSourceVimMotion *self)
{
	int count = get_adjusted_count (self);
	return do_motion_line_end_with_nl (iter, self->apply_count, count + 1);
}

GtkSourceVimState *
gtk_source_vim_motion_new_line_end_with_nl (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_line_end_with_nl;
	self->inclusivity = EXCLUSIVE;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_next_line_end_with_nl (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_next_line_end_with_nl;
	self->inclusivity = EXCLUSIVE;

	return GTK_SOURCE_VIM_STATE (self);
}

GtkSourceVimState *
gtk_source_vim_motion_new_none (void)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_none;
	self->inclusivity = INCLUSIVE;
	self->wise = CHARWISE;

	return GTK_SOURCE_VIM_STATE (self);
}

static gboolean
motion_chained (GtkTextIter        *iter,
                GtkSourceVimMotion *self)
{
	GtkTextIter before = *iter;

	for (guint i = 0; i < self->chained->len; i++)
	{
		GtkSourceVimMotion *motion = g_ptr_array_index (self->chained, i);

		gtk_source_vim_motion_set_mark (motion, self->mark);
		gtk_source_vim_motion_apply (motion, iter, self->applying_inclusive);
		gtk_source_vim_motion_set_mark (motion, NULL);
	}

	return !gtk_text_iter_equal (&before, iter);
}

static void
gtk_source_vim_motion_add (GtkSourceVimMotion *self,
                           GtkSourceVimMotion *other)
{
	g_assert (GTK_SOURCE_IS_VIM_MOTION (self));
	g_assert (self->motion == motion_chained);
	g_assert (GTK_SOURCE_IS_VIM_MOTION (other));
	g_assert (self != other);

	if (self->chained->len > 0)
	{
		GtkSourceVimMotion *last;

		last = g_ptr_array_index (self->chained, self->chained->len - 1);

		if (last->motion == other->motion &&
		    last->inclusivity == other->inclusivity &&
		    last->f_char == other->f_char)
		{
			int count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (last))
			          + gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (other));

			gtk_source_vim_state_set_count (GTK_SOURCE_VIM_STATE (last), count);
			return;
		}
	}

	gtk_source_vim_motion_set_mark (other, NULL);
	g_ptr_array_add (self->chained, g_object_ref (other));
	gtk_source_vim_state_set_parent (GTK_SOURCE_VIM_STATE (other),
	                                 GTK_SOURCE_VIM_STATE (self));
}

static void
clear_state (gpointer data)
{
	GtkSourceVimState *state = data;
	gtk_source_vim_state_unparent (state);
	g_object_unref (state);
}

GtkSourceVimState *
gtk_source_vim_motion_chain (GtkSourceVimMotion *self,
                             GtkSourceVimMotion *other)
{
	GtkSourceVimMotion *chained;

	g_return_val_if_fail (!self || GTK_SOURCE_IS_VIM_MOTION (self), NULL);
	g_return_val_if_fail (!other || GTK_SOURCE_IS_VIM_MOTION (other), NULL);

	if (self == NULL || self->motion != motion_chained)
	{
		chained = GTK_SOURCE_VIM_MOTION (gtk_source_vim_motion_new ());
		chained->motion = motion_chained;
		chained->inclusivity = INCLUSIVE;
		chained->chained = g_ptr_array_new_with_free_func (clear_state);
	}
	else
	{
		chained = g_object_ref (self);
	}

	if (self != chained && self != NULL)
		gtk_source_vim_motion_add (chained, self);

	if (other != NULL)
		gtk_source_vim_motion_add (chained, other);

	return GTK_SOURCE_VIM_STATE (chained);
}

gboolean
gtk_source_vim_motion_invalidates_visual_column (GtkSourceVimMotion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MOTION (self), FALSE);

	return self->invalidates_visual_column;
}

gboolean
gtk_source_vim_motion_is_linewise (GtkSourceVimMotion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MOTION (self), FALSE);

	return self->wise == LINEWISE;
}

gboolean
gtk_source_vim_motion_is_jump (GtkSourceVimMotion *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_MOTION (self), FALSE);

	return self->is_jump;
}

GtkSourceVimState *
gtk_source_vim_motion_new_down (int alter_count)
{
	GtkSourceVimMotion *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_MOTION, NULL);
	self->motion = motion_next_line_visual_column;
	self->inclusivity = EXCLUSIVE;
	self->wise = LINEWISE;
	self->alter_count = alter_count;

	return GTK_SOURCE_VIM_STATE (self);
}

void
gtk_source_vim_motion_set_linewise_keyval (GtkSourceVimMotion *self,
                                           guint               keyval)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_MOTION (self));

	self->linewise_keyval = keyval;
}

gboolean
gtk_source_vim_iter_forward_word_end (GtkTextIter *iter)
{
	forward_classified_end (iter, classify_word_newline_stop);
	return TRUE;
}

gboolean
gtk_source_vim_iter_forward_WORD_end (GtkTextIter *iter)
{
	forward_classified_end (iter, classify_WORD_newline_stop);
	return TRUE;
}

gboolean
gtk_source_vim_iter_backward_word_start (GtkTextIter *iter)
{
	backward_classified_start (iter, classify_word_newline_stop);
	return TRUE;
}

gboolean
gtk_source_vim_iter_backward_WORD_start (GtkTextIter *iter)
{
	backward_classified_start (iter, classify_WORD_newline_stop);
	return TRUE;
}

static inline gboolean
unichar_ends_sentence (gunichar ch)
{
	switch (ch)
	{
		case '.':
		case '!':
		case '?':
			return TRUE;

		default:
			return FALSE;
	}
}

static inline gboolean
unichar_can_trail_sentence (gunichar ch)
{
	switch (ch)
	{
		case '.':
		case '!':
		case '?':
		case '\'':
		case '"':
		case ')':
		case ']':
			return TRUE;

		default:
			return FALSE;
	}
}

gboolean
gtk_source_vim_iter_forward_sentence_end (GtkTextIter *iter)
{
	/* From VIM:
	 *
	 * A sentence is defined as ending at a '.', '!' or '?' followed by either the
	 * end of a line, or by a space or tab.  Any number of closing ')', ']', '"'
	 * and ''' characters may appear after the '.', '!' or '?' before the spaces,
	 * tabs or end of line.  A paragraph and section boundary is also a sentence
	 * boundary.
	 */

	if (gtk_text_iter_is_end (iter))
		return FALSE;

	/* First find a .!? */
	while (gtk_text_iter_forward_char (iter))
	{
		gunichar ch = gtk_text_iter_get_char (iter);

		if (unichar_ends_sentence (ch))
			break;

		/* If we reached a newline, and the next char is also
		 * a newline, then we stop at this newline.
		 */
		if (gtk_text_iter_ends_line (iter))
		{
			GtkTextIter peek = *iter;

			if (gtk_text_iter_forward_char (&peek) || gtk_text_iter_is_end (&peek))
			{
				return TRUE;
			}
		}
	}

	/* Read past any acceptable trailing chars */
	while (gtk_text_iter_forward_char (iter))
	{
		gunichar ch = gtk_text_iter_get_char (iter);

		if (!unichar_can_trail_sentence (ch))
			break;
	}

	/* If we are on a space or end of a buffer, then we found the end */
	if (gtk_text_iter_is_end (iter) || iter_isspace (iter))
	{
		return TRUE;
	}

	/* This is not a suitable sentence candidate. We must try again */
	return gtk_source_vim_iter_forward_sentence_end (iter);
}

gboolean
gtk_source_vim_iter_backward_sentence_start (GtkTextIter *iter)
{
	return motion_backward_sentence_start (iter, NULL);
}

gboolean
gtk_source_vim_iter_forward_paragraph_end (GtkTextIter *iter)
{
	return motion_forward_paragraph_end (iter, NULL);
}

gboolean
gtk_source_vim_iter_backward_paragraph_start (GtkTextIter *iter)
{
	return motion_backward_paragraph_start (iter, NULL);
}

typedef struct
{
	gunichar ch;
	gunichar opposite;
	int count;
} FindPredicate;

static gboolean
find_predicate (gunichar ch,
                gpointer data)
{
	FindPredicate *find = data;

	if (ch == find->opposite)
		find->count++;
	else if (ch == find->ch)
		find->count--;

	return find->count == 0;
}

static gboolean
gtk_source_vim_iter_backward_block_start (GtkTextIter *iter,
                                          gunichar     ch,
                                          gunichar     opposite)
{
	FindPredicate find = { ch, opposite, 1 };

	if (gtk_text_iter_get_char (iter) == ch)
		return TRUE;

	return gtk_text_iter_backward_find_char (iter, find_predicate, &find, NULL);
}

static gboolean
gtk_source_vim_iter_forward_block_end (GtkTextIter *iter,
                                       gunichar     ch,
                                       gunichar     opposite)
{
	FindPredicate find = { ch, opposite, 1 };

	if (gtk_text_iter_get_char (iter) == ch)
		return TRUE;

	return gtk_text_iter_forward_find_char (iter, find_predicate, &find, NULL);
}

gboolean
gtk_source_vim_iter_backward_block_paren_start (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_block_start (iter, '(', ')');
}

gboolean
gtk_source_vim_iter_forward_block_paren_end (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_block_end (iter, ')', '(');
}

gboolean
gtk_source_vim_iter_backward_block_brace_start (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_block_start (iter, '{', '}');
}

gboolean
gtk_source_vim_iter_forward_block_brace_end (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_block_end (iter, '}', '{');
}

gboolean
gtk_source_vim_iter_forward_block_bracket_end (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_block_end (iter, ']', '[');
}

gboolean
gtk_source_vim_iter_backward_block_bracket_start (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_block_start (iter, '[', ']');
}

gboolean
gtk_source_vim_iter_forward_block_lt_gt_end (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_block_end (iter, '>', '<');
}

gboolean
gtk_source_vim_iter_backward_block_lt_gt_start (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_block_start (iter, '<', '>');
}

static gboolean
gtk_source_vim_iter_backward_quote_start (GtkTextIter *iter,
                                          gunichar     ch)
{
	FindPredicate find = { ch, 0 , 1 };
	GtkTextIter limit = *iter;
	gtk_text_iter_set_line_offset (&limit, 0);
	return gtk_text_iter_backward_find_char (iter, find_predicate, &find, NULL);
}

static gboolean
gtk_source_vim_iter_ends_quote (const GtkTextIter *iter,
                                gunichar           ch)
{
	if (ch == gtk_text_iter_get_char (iter) &&
	    !gtk_text_iter_starts_line (iter))
	{
		GtkTextIter alt = *iter;

		if (gtk_source_vim_iter_backward_quote_start (&alt, ch))
		{
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
gtk_source_vim_iter_forward_quote_end (GtkTextIter *iter,
                                       gunichar     ch)
{
	FindPredicate find = { ch, 0, 1 };
	GtkTextIter limit = *iter;

	if (!gtk_text_iter_ends_line (&limit))
		gtk_text_iter_forward_to_line_end (&limit);

	return gtk_text_iter_forward_find_char (iter, find_predicate, &find, NULL);
}

gboolean
gtk_source_vim_iter_forward_quote_double (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_quote_end (iter, '"');
}

gboolean
gtk_source_vim_iter_ends_quote_double (const GtkTextIter *iter)
{
	return gtk_source_vim_iter_ends_quote (iter, '"');
}

gboolean
gtk_source_vim_iter_ends_quote_single (const GtkTextIter *iter)
{
	return gtk_source_vim_iter_ends_quote (iter, '\'');
}

gboolean
gtk_source_vim_iter_ends_quote_grave (const GtkTextIter *iter)
{
	return gtk_source_vim_iter_ends_quote (iter, '\'');
}

gboolean
gtk_source_vim_iter_backward_quote_double (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_quote_start (iter, '"');
}

gboolean
gtk_source_vim_iter_forward_quote_single (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_quote_end (iter, '\'');
}

gboolean
gtk_source_vim_iter_backward_quote_single (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_quote_start (iter, '\'');
}

gboolean
gtk_source_vim_iter_forward_quote_grave (GtkTextIter *iter)
{
	return gtk_source_vim_iter_forward_quote_end (iter, '`');
}

gboolean
gtk_source_vim_iter_backward_quote_grave (GtkTextIter *iter)
{
	return gtk_source_vim_iter_backward_quote_start (iter, '`');
}

gboolean
gtk_source_vim_iter_starts_word (const GtkTextIter *iter)
{
	GtkTextIter prev;

	if (gtk_text_iter_starts_line (iter))
	{
		/* A blank line is a word */
		return gtk_text_iter_ends_line (iter) || !iter_isspace (iter);
	}
	else if (gtk_text_iter_ends_line (iter))
	{
		return FALSE;
	}

	if (iter_isspace (iter))
		return FALSE;

	prev = *iter;
	gtk_text_iter_backward_char (&prev);

	return simple_word_classify (gtk_text_iter_get_char (iter)) !=
	       simple_word_classify (gtk_text_iter_get_char (&prev));
}

gboolean
gtk_source_vim_iter_ends_word (const GtkTextIter *iter)
{
	GtkTextIter next;

	if (gtk_text_iter_ends_line (iter))
	{
		/* A blank line is a word */
		if (gtk_text_iter_starts_line (iter))
			return TRUE;

		return FALSE;
	}

	if (iter_isspace (iter))
		return FALSE;

	next = *iter;
	gtk_text_iter_forward_char (&next);

	return simple_word_classify (gtk_text_iter_get_char (iter)) !=
	       simple_word_classify (gtk_text_iter_get_char (&next));
}

gboolean
gtk_source_vim_iter_starts_WORD (const GtkTextIter *iter)
{
	GtkTextIter prev;

	if (gtk_text_iter_starts_line (iter))
	{
		/* A blank line is a word */
		return gtk_text_iter_ends_line (iter) || !iter_isspace (iter);
	}
	else if (gtk_text_iter_ends_line (iter))
	{
		return FALSE;
	}

	if (iter_isspace (iter))
		return FALSE;

	prev = *iter;
	gtk_text_iter_backward_char (&prev);

	return iter_isspace (&prev);
}

gboolean
gtk_source_vim_iter_ends_WORD (const GtkTextIter *iter)
{
	GtkTextIter next;

	if (gtk_text_iter_ends_line (iter))
	{
		/* A blank line is a word */
		if (gtk_text_iter_starts_line (iter))
			return TRUE;

		return FALSE;
	}

	if (iter_isspace (iter))
		return FALSE;

	next = *iter;
	if (!gtk_text_iter_forward_char (&next))
		return TRUE;

	return iter_isspace (&next);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcecompletionutils.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright (C) 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <string.h>
#include "gtksourcecompletionutils.h"
#include "gtksourcebuffer.h"

/**
 * gtk_source_completion_utils_is_separator:
 * @ch: the character to check.
 *
 * A separator is a character like (, an space etc. An _ is not a separator
 *
 * Returns: %TRUE if @ch is a separator.
 */
static gboolean
gtk_source_completion_utils_is_separator (const gunichar ch)
{
	if (g_unichar_isprint(ch) &&
	    (g_unichar_isalnum(ch) || ch == g_utf8_get_char("_")))
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * gtk_source_completion_utils_get_word_iter:
 * @buffer: a #GtkTextBuffer.
 * @start_word: assign it the start position of the word
 * @end_word: assign it the end position of the word
 */
void
gtk_source_completion_utils_get_word_iter (GtkTextBuffer *buffer,
					   GtkTextIter   *start_word,
					   GtkTextIter   *end_word)
{
	gtk_text_buffer_get_iter_at_mark (buffer,
	                                  end_word,
	                                  gtk_text_buffer_get_insert (buffer));

	*start_word = *end_word;

	while (gtk_text_iter_backward_char (start_word))
	{
		gunichar ch = gtk_text_iter_get_char (start_word);

		if (gtk_source_completion_utils_is_separator (ch))
		{
			gtk_text_iter_forward_char (start_word);
			return;
		}
	}
}

static void
get_iter_pos (GtkSourceView *source_view,
              GtkTextIter   *iter,
              gint          *x,
              gint          *y,
              gint          *height)
{
	GdkWindow *win;
	GtkTextView *text_view;
	GdkRectangle location;
	gint win_x;
	gint win_y;
	gint xx;
	gint yy;

	text_view = GTK_TEXT_VIEW (source_view);

	gtk_text_view_get_iter_location (text_view, iter, &location);

	gtk_text_view_buffer_to_window_coords (text_view,
					       GTK_TEXT_WINDOW_WIDGET,
					       location.x,
					       location.y,
					       &win_x,
					       &win_y);

	win = gtk_text_view_get_window (text_view, GTK_TEXT_WINDOW_WIDGET);
	gdk_window_get_origin (win, &xx, &yy);

	*x = win_x + xx;
	*y = win_y + yy + location.height;
	*height = location.height;
}

/**
 * gtk_source_completion_utils_replace_current_word:
 * @buffer: a #GtkTextBuffer.
 * @text: (allow-none): The text to be inserted instead of the current word.
 *
 * Replaces the current word in the #GtkSourceBuffer with the new word.
 */
void
gtk_source_completion_utils_replace_current_word (GtkTextBuffer *buffer,
						  const gchar   *text)
{
	GtkTextIter word_start;
	GtkTextIter word_end;

	gtk_source_completion_utils_get_word_iter (buffer, &word_start, &word_end);

	gtk_text_buffer_begin_user_action (buffer);

	gtk_text_buffer_delete (buffer, &word_start, &word_end);

	if (text != NULL)
	{
		gtk_text_buffer_insert (buffer, &word_start, text, -1);
	}

	gtk_text_buffer_end_user_action (buffer);
}

static void
compensate_for_gravity (GtkWindow *window,
                        gint      *x,
                        gint      *y,
                        gint      w,
                        gint      h)
{
	GdkGravity gravity;

	gravity = gtk_window_get_gravity (window);

	/* Horizontal */
	switch (gravity)
	{
		case GDK_GRAVITY_NORTH:
		case GDK_GRAVITY_SOUTH:
		case GDK_GRAVITY_CENTER:
			*x = w / 2;
			break;
		case GDK_GRAVITY_NORTH_EAST:
		case GDK_GRAVITY_SOUTH_EAST:
		case GDK_GRAVITY_EAST:
			*x = w;
			break;
		default:
			*x = 0;
			break;
	}

	/* Vertical */
	switch (gravity)
	{
		case GDK_GRAVITY_WEST:
		case GDK_GRAVITY_CENTER:
		case GDK_GRAVITY_EAST:
			*y = w / 2;
			break;
		case GDK_GRAVITY_SOUTH_EAST:
		case GDK_GRAVITY_SOUTH:
		case GDK_GRAVITY_SOUTH_WEST:
			*y = w;
			break;
		default:
			*y = 0;
			break;
	}
}

static void
move_overlap (gint     *x,
              gint     *y,
              gint      w,
              gint      h,
              gint      oy,
              gint      cx,
              gint      cy,
              gint      line_height,
              gboolean  move_up)
{
	/* Test if there is overlap */
	if (*y - cy < oy && *y - cy + h > oy - line_height)
	{
		if (move_up)
		{
			*y = oy - line_height - h + cy;
		}
		else
		{
			*y = oy + cy;
		}
	}
}

/**
 * gtk_source_completion_utils_move_to_iter:
 * @window: (allow-none): the #GtkWindow to move.
 * @view: the #GtkSourceView.
 * @iter: the iter to move @window to.
 *
 */
void
gtk_source_completion_utils_move_to_iter (GtkWindow     *window,
					  GtkSourceView *view,
					  GtkTextIter   *iter)
{
	GdkScreen *screen;
	gint x, y;
	gint w, h;
	gint sw, sh;
	gint cx, cy;
	gint oy;
	gint height;
	gboolean overlapup;

	if (window != NULL)
	{
		screen = gtk_window_get_screen (window);
	}
	else
	{
		screen = gdk_screen_get_default ();
	}

	sw = gdk_screen_get_width (screen);
	sh = gdk_screen_get_height (screen);

	get_iter_pos (view, iter, &x, &y, &height);
	gtk_window_get_size (window, &w, &h);

	oy = y;
	compensate_for_gravity (window, &cx, &cy, w, h);

	/* Push window inside screen */
	if (x - cx + w > sw)
	{
		x = (sw - w) + cx;
	}
	else if (x - cx < 0)
	{
		x = cx;
	}

	if (y - cy + h > sh)
	{
		y = (sh - h) + cy;
		overlapup = TRUE;
	}
	else if (y - cy < 0)
	{
		y = cy;
		overlapup = FALSE;
	}
	else
	{
		overlapup = TRUE;
	}

	/* Make sure that text is still readable */
	move_overlap (&x, &y, w, h, oy, cx, cy, height, overlapup);

	gtk_window_move (window, x, y);
}

/**
 * gtk_source_completion_utils_get_pos_at_cursor:
 * @window: the #GtkWindow to move.
 * @view: the #GtkSoureView.
 *
 */
void
gtk_source_completion_utils_move_to_cursor (GtkWindow     *window,
					    GtkSourceView *view)
{
	GtkTextBuffer *buffer;
	GtkTextIter insert;

	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_buffer_get_iter_at_mark (buffer, &insert, gtk_text_buffer_get_insert (buffer));

	gtk_source_completion_utils_move_to_iter (window,
	                                          view,
	                                          &insert);
}

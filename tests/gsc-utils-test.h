/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gsc-utils.h
 *
 *  Copyright (C) 2007 - Chuchiperriman <chuchiperriman@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef GTK_SNIPPETS_GSV_UTILS_H
#define GTK_SNIPPETS_GSV_UTILS_H

#include <gtk/gtk.h>

/**
* gsc_get_last_word_and_iter:
* @text_view: The #GtkTextView
* @start_word: if != NULL then assign it the start position of the word
* @end_word: if != NULL then assing it the end position of the word
* 
* Returns: the last word written in the #GtkTextView or ""
*
**/
gchar*
gsc_get_last_word_and_iter(GtkTextView *text_view, 
					GtkTextIter *start_word, 
					GtkTextIter *end_word);

/**
 * gsc_get_last_word:
 * @text_view: The #GtkTextView
 *
 * Returns: the last word written in the #GtkTextView or ""
 */
gchar*
gsc_get_last_word(GtkTextView *text_view);

/**
 * gsc_get_last_word_cleaned:
 * @text_view: The #GtkTextView
 *
 * See #gsc_get_last_word and #gsc_clear_word
 *
 * Returns: the last word written in the #GtkTextView or "" (new allocated)
 */
gchar*
gsc_get_last_word_cleaned(GtkTextView *text_view);

/** 
 * gsc_get_cursor_pos:
 * @text_view: The #GtkTextView
 * @x: Assign the x position of the cursor
 * @y: Assign the y position of the cursor
 *
 * Gets the cursor position on the screen.
 */
void
gsc_get_cursor_pos(GtkTextView *text_view, 
				gint *x, 
				gint *y);

/**
 * gsc_gsv_get_text: 
 * @text_view: The #GtkTextView 
 *
 * Returns All the #GtkTextView content .
 */
gchar* 
gsc_gsv_get_text(GtkTextView *text_view);

/**
 * gsc_replace_actual_word:
 * @text_view: The #GtkTextView
 * @text: The text to be inserted instead of the current word
 * 
 * Replaces the current word in the #GtkTextView with the new word
 *
 */
void
gtk_source_completion_replace_actual_word(GtkTextView *text_view, 
				    const gchar* text);

/**
 * gsc_char_is_separator:
 * @ch: The character to check
 *
 * A separator is a character like (, an space etc. An _ is not a separator
 *
 * Returns TRUE if the ch is a separator
 */
gboolean
gsc_char_is_separator(gunichar ch);

/**
 * gsc_clear_word:
 * @word: The word to be cleaned
 * 
 * Clean the word eliminates the special characters at the start of this word.
 * By example "$variable" is cleaned to "variable"
 *
 * Returns New allocated string with the word cleaned. If all characters are 
 * separators, it return NULL;
 *
 */
gchar*
gsc_clear_word(const gchar* word);

/**
 * gsc_compute_line_indentation:
 * @view: The #GtkTextView
 * @cur: Cursor in the line where we compute the indentation
 *
 * Returns: New allocated string with the indentation of this line
 */
gchar *
gsc_compute_line_indentation (GtkTextView *view,
			     GtkTextIter *cur);

/**
 * gsc_get_text_with_indent:
 * @content: The initial text to indent
 * @indent: Indentation string. You can get the indentation of a line with
 * #gsc_compute_line_indentation.
 *
 * Returns: New allocated string with the content indented.
 */
gchar*
gsc_get_text_with_indent(const gchar* content,gchar *indent);

/**
 * gsc_insert_text_with_indent:
 * @view: The #GtkTextView where we will insert the indented text
 * @text: Text to indent and insert into the view.
 *
 * This function indent the text and insert it into the view in the current
 * position.
 */
void
gsc_insert_text_with_indent(GtkTextView *view, const gchar* text);

/**
 * gsc_is_valid_word:
 * @current_word: The current word 
 * @completion_word: The completion word
 *
 * Returns: TRUE if the completion_word starts with current_word and it is not
 * the same word.
 */
gboolean
gsc_is_valid_word(gchar *current_word, gchar *completion_word);

/**
 * gsc_get_window_position_in_cursor:
 * @window: Window to set
 * @view: Parent view where we get the cursor position
 * @x: The returned x position
 * @y: The returned y position
 *
 * Returns: TRUE if the position is over the text and FALSE if 
 * the position is under the text.
 */
gboolean 
gsc_get_window_position_in_cursor(GtkWindow *window,
				  GtkTextView *view,
				  gint *x,
				  gint *y,
				  gboolean *resized);

/**
 * gsc_get_window_position_center_screen:
 * @window: Window to set
 * @x: The returned x position
 * @y: The returned y position
 *
 * Assing x and y values to center the window in the screen
 *
 */
void
gsc_get_window_position_center_screen(GtkWindow *window, gint *x, gint *y);

/**
 * gsc_get_window_position_center_parent:
 * @window: Window to set
 * @parent: Parent widget where we want to center the window
 * @x: The returned x position
 * @y: The returned y position
 *
 * Assing x and y values to center the window in the parent widget
 *
 */
void
gsc_get_window_position_center_parent(GtkWindow *window,
				      GtkWidget *parent,
				      gint *x,
				      gint *y);

#endif 

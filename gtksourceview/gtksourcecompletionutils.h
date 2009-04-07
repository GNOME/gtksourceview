/*
 * gtksourcecompletion-utils.h
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
 
#ifndef GTK_SOURCE_COMPLETION_UTILS_H
#define GTK_SOURCE_COMPLETION_UTILS_H

#include <gtk/gtk.h>

gboolean	gtk_source_completion_utils_is_separator		(gunichar ch);

gchar		*gtk_source_completion_utils_get_word_iter		(GtkTextView *text_view, 
									 GtkTextIter *start_word, 
									 GtkTextIter *end_word);

gchar		*gtk_source_completion_utils_get_word			(GtkTextView *text_view);

void		gtk_source_completion_utils_get_cursor_pos		(GtkTextView *text_view, 
									 gint *x, 
									 gint *y);

void		gtk_source_completion_utils_replace_current_word	(GtkTextView *text_view, 
									 const gchar* text);

gboolean	gtk_source_completion_utils_get_pos_at_cursor		(GtkWindow *window,
									 GtkTextView *view,
									 gint *x,
									 gint *y,
									 gboolean *resized);

#endif 

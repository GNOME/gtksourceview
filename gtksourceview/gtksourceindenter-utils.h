/*
 * gtksourceindenter-utils.h
 * This file is part of gedit
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __GTK_SOURCE_INDENTER_UTILS_H__
#define __GTK_SOURCE_INDENTER_UTILS_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

gint		 gtk_source_indenter_get_amount_indents		(GtkTextView *view,
								 GtkTextIter *cur);

gint		 gtk_source_indenter_get_amount_indents_from_position
								(GtkTextView *view,
								 GtkTextIter *cur);

gboolean	 gtk_source_indenter_move_to_no_space		(GtkTextIter *iter,
								 gint direction);

gboolean	 gtk_source_indenter_move_to_no_comments	(GtkTextIter *iter);

gboolean	 gtk_source_indenter_find_open_char		(GtkTextIter *iter,
								 gchar open,
								 gchar close,
								 gboolean skip_first);

gint		 gtk_source_indenter_add_indent			(GtkTextView *view,
								 gint current_level);

G_END_DECLS

#endif

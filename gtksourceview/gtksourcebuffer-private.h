/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcebuffer-private.h
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#ifndef __GTK_SOURCE_BUFFER_PRIVATE_H__
#define __GTK_SOURCE_BUFFER_PRIVATE_H__

#include <gtk/gtk.h>
#include "gtksourcetypes.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
void			 _gtk_source_buffer_update_highlight		(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *start,
									 const GtkTextIter      *end,
									 gboolean                synchronous);

G_GNUC_INTERNAL
GtkSourceMark		*_gtk_source_buffer_source_mark_next		(GtkSourceBuffer        *buffer,
									 GtkSourceMark          *mark,
									 const gchar            *category);

G_GNUC_INTERNAL
GtkSourceMark		*_gtk_source_buffer_source_mark_prev		(GtkSourceBuffer        *buffer,
									 GtkSourceMark          *mark,
									 const gchar            *category);

G_GNUC_INTERNAL
GtkTextTag		*_gtk_source_buffer_get_bracket_match_tag	(GtkSourceBuffer        *buffer);

G_GNUC_INTERNAL
void			 _gtk_source_buffer_add_search_context		(GtkSourceBuffer        *buffer,
									 GtkSourceSearchContext *search_context);

G_GNUC_INTERNAL
void			 _gtk_source_buffer_set_as_invalid_character	(GtkSourceBuffer        *buffer,
									 const GtkTextIter      *start,
									 const GtkTextIter      *end);

G_GNUC_INTERNAL
gboolean		 _gtk_source_buffer_has_invalid_chars		(GtkSourceBuffer        *buffer);

G_END_DECLS

#endif /* __GTK_SOURCE_BUFFER_PRIVATE_H__ */

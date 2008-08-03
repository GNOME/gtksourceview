/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcebuffer-private.h
 *
 *  Copyright (C) 2008 - Paolo Maggi, Paolo Borelli
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_BUFFER_PRIVATE_H__
#define __GTK_SOURCE_BUFFER_PRIVATE_H__

#include <gtksourceview/gtksourcebuffer.h>

G_BEGIN_DECLS

void			 _gtk_source_buffer_update_highlight	(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *start,
								 const GtkTextIter      *end,
								 gboolean                synchronous);

GtkSourceMark		*_gtk_source_buffer_source_mark_next	(GtkSourceBuffer        *buffer,
								 GtkSourceMark          *mark,
								 const gchar            *category);
GtkSourceMark		*_gtk_source_buffer_source_mark_prev	(GtkSourceBuffer        *buffer,
								 GtkSourceMark          *mark,
								 const gchar            *category);
GList			*_gtk_source_buffer_get_folds_in_region	(GtkSourceBuffer        *buffer,
								 const GtkTextIter      *begin,
								 const GtkTextIter      *end);
GtkSourceFold		*_gtk_source_buffer_get_fold_at_line	(GtkSourceBuffer        *buffer,
								 gint                    line);

G_END_DECLS

#endif /* __GTK_SOURCE_BUFFER_PRIVATE_H__ */

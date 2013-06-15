/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcesearch.h
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

#ifndef __GTK_SOURCE_SEARCH_H__
#define __GTK_SOURCE_SEARCH_H__

#include <gtk/gtk.h>
#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"
#include "gtksourcebuffer.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_SEARCH             (_gtk_source_search_get_type ())
#define GTK_SOURCE_SEARCH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_SEARCH, GtkSourceSearch))
#define GTK_SOURCE_SEARCH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_SEARCH, GtkSourceSearchClass))
#define GTK_SOURCE_IS_SEARCH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_SEARCH))
#define GTK_SOURCE_IS_SEARCH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_SEARCH))
#define GTK_SOURCE_SEARCH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_SOURCE_TYPE_SEARCH, GtkSourceSearchClass))

typedef struct _GtkSourceSearchClass    GtkSourceSearchClass;
typedef struct _GtkSourceSearchPrivate  GtkSourceSearchPrivate;

struct _GtkSourceSearch
{
	GObject parent;

	GtkSourceSearchPrivate *priv;
};

struct _GtkSourceSearchClass
{
	GObjectClass parent_class;
};

G_GNUC_INTERNAL
GType			_gtk_source_search_get_type			(void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkSourceSearch *	_gtk_source_search_new				(GtkSourceBuffer	*buffer);

G_GNUC_INTERNAL
void			_gtk_source_search_set_text			(GtkSourceSearch	*search,
									 const gchar		*text);

G_GNUC_INTERNAL
const gchar *		_gtk_source_search_get_text			(GtkSourceSearch	*search);

G_GNUC_INTERNAL
void			_gtk_source_search_set_case_sensitive		(GtkSourceSearch	*search,
									 gboolean		 case_sensitive);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_get_case_sensitive		(GtkSourceSearch	*search);

G_GNUC_INTERNAL
void			_gtk_source_search_set_at_word_boundaries	(GtkSourceSearch	*search,
									 gboolean		 at_word_boundaries);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_get_at_word_boundaries	(GtkSourceSearch	*search);

G_GNUC_INTERNAL
void			_gtk_source_search_set_wrap_around		(GtkSourceSearch	*search,
									 gboolean		 wrap_around);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_get_wrap_around		(GtkSourceSearch	*search);

G_GNUC_INTERNAL
guint			_gtk_source_search_get_occurrences_count	(GtkSourceSearch	*search);

G_GNUC_INTERNAL
gint			_gtk_source_search_get_occurrence_position	(GtkSourceSearch	*search,
									 const GtkTextIter	*match_start,
									 const GtkTextIter	*match_end);

G_GNUC_INTERNAL
void			_gtk_source_search_update_highlight		(GtkSourceSearch	*search,
									 const GtkTextIter	*start,
									 const GtkTextIter	*end,
									 gboolean		 synchronous);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_forward			(GtkSourceSearch	*search,
									 const GtkTextIter	*iter,
									 GtkTextIter		*match_start,
									 GtkTextIter		*match_end);

G_GNUC_INTERNAL
void			_gtk_source_search_forward_async		(GtkSourceSearch	*search,
									 const GtkTextIter	*iter,
									 GCancellable		*cancellable,
									 GAsyncReadyCallback	 callback,
									 gpointer		 user_data);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_forward_finish		(GtkSourceSearch	*search,
									 GAsyncResult		*result,
									 GtkTextIter		*match_start,
									 GtkTextIter		*match_end,
									 GError		       **error);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_backward			(GtkSourceSearch	*search,
									 const GtkTextIter	*iter,
									 GtkTextIter		*match_start,
									 GtkTextIter		*match_end);

G_GNUC_INTERNAL
void			_gtk_source_search_backward_async		(GtkSourceSearch	*search,
									 const GtkTextIter	*iter,
									 GCancellable		*cancellable,
									 GAsyncReadyCallback	 callback,
									 gpointer		 user_data);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_backward_finish		(GtkSourceSearch	*search,
									 GAsyncResult		*result,
									 GtkTextIter		*match_start,
									 GtkTextIter		*match_end,
									 GError		       **error);

G_GNUC_INTERNAL
gboolean		_gtk_source_search_replace			(GtkSourceSearch	*search,
									 const GtkTextIter	*match_start,
									 const GtkTextIter	*match_end,
									 const gchar		*replace,
									 gint			 replace_length);

G_GNUC_INTERNAL
guint			_gtk_source_search_replace_all			(GtkSourceSearch	*search,
									 const gchar		*replace,
									 gint			 replace_length);

G_END_DECLS

#endif /* __GTK_SOURCE_SEARCH_H__ */

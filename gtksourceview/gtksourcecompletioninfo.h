/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2007 - 2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcetypes.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_INFO             (gtk_source_completion_info_get_type ())
#define GTK_SOURCE_COMPLETION_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_COMPLETION_INFO, GtkSourceCompletionInfo))
#define GTK_SOURCE_COMPLETION_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_COMPLETION_INFO, GtkSourceCompletionInfoClass)
#define GTK_SOURCE_IS_COMPLETION_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_COMPLETION_INFO))
#define GTK_SOURCE_IS_COMPLETION_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_COMPLETION_INFO))
#define GTK_SOURCE_COMPLETION_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_SOURCE_TYPE_COMPLETION_INFO, GtkSourceCompletionInfoClass))

typedef struct _GtkSourceCompletionInfoPrivate GtkSourceCompletionInfoPrivate;

struct _GtkSourceCompletionInfo
{
	GtkWindow parent;

	GtkSourceCompletionInfoPrivate *priv;
};

typedef struct _GtkSourceCompletionInfoClass GtkSourceCompletionInfoClass;

struct _GtkSourceCompletionInfoClass
{
	GtkWindowClass parent_class;

	gpointer padding[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GType		 gtk_source_completion_info_get_type		(void) G_GNUC_CONST;

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionInfo *
		 gtk_source_completion_info_new			(void);

GTK_SOURCE_AVAILABLE_IN_ALL
void		 gtk_source_completion_info_move_to_iter	(GtkSourceCompletionInfo *info,
								 GtkTextView             *view,
								 GtkTextIter             *iter);

G_GNUC_INTERNAL
void		 _gtk_source_completion_info_set_xoffset	(GtkSourceCompletionInfo *info,
								 gint                     xoffset);

G_END_DECLS

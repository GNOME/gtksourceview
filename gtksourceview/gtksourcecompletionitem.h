/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
 * Copyright 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#define GTK_SOURCE_TYPE_COMPLETION_ITEM			(gtk_source_completion_item_get_type ())
#define GTK_SOURCE_COMPLETION_ITEM(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_COMPLETION_ITEM, GtkSourceCompletionItem))
#define GTK_SOURCE_COMPLETION_ITEM_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_COMPLETION_ITEM, GtkSourceCompletionItemClass))
#define GTK_SOURCE_IS_COMPLETION_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_COMPLETION_ITEM))
#define GTK_SOURCE_IS_COMPLETION_ITEM_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_COMPLETION_ITEM))
#define GTK_SOURCE_COMPLETION_ITEM_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_SOURCE_TYPE_COMPLETION_ITEM, GtkSourceCompletionItemClass))

typedef struct _GtkSourceCompletionItemClass	GtkSourceCompletionItemClass;
typedef struct _GtkSourceCompletionItemPrivate	GtkSourceCompletionItemPrivate;

struct _GtkSourceCompletionItem {
	GObject parent;

	GtkSourceCompletionItemPrivate *priv;
};

struct _GtkSourceCompletionItemClass {
	GObjectClass parent_class;

	gpointer padding[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GType 			 gtk_source_completion_item_get_type 		(void) G_GNUC_CONST;

GTK_SOURCE_AVAILABLE_IN_4_0
GtkSourceCompletionItem *gtk_source_completion_item_new			(void);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_label		(GtkSourceCompletionItem *item,
									 const gchar             *label);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_markup		(GtkSourceCompletionItem *item,
									 const gchar             *markup);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_text		(GtkSourceCompletionItem *item,
									 const gchar             *text);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_icon		(GtkSourceCompletionItem *item,
									 GdkPixbuf               *icon);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_icon_name	(GtkSourceCompletionItem *item,
									 const gchar             *icon_name);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_gicon		(GtkSourceCompletionItem *item,
									 GIcon                   *gicon);

GTK_SOURCE_AVAILABLE_IN_3_24
void			 gtk_source_completion_item_set_info		(GtkSourceCompletionItem *item,
									 const gchar             *info);

G_END_DECLS

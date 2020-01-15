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

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_ITEM (gtk_source_completion_item_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceCompletionItem, gtk_source_completion_item, GTK_SOURCE, COMPLETION_ITEM, GObject)

struct _GtkSourceCompletionItemClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_4_0
GtkSourceCompletionItem *gtk_source_completion_item_new           (void);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_label     (GtkSourceCompletionItem *item,
                                                                   const gchar             *label);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_markup    (GtkSourceCompletionItem *item,
                                                                   const gchar             *markup);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_text      (GtkSourceCompletionItem *item,
                                                                   const gchar             *text);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_icon      (GtkSourceCompletionItem *item,
                                                                   GdkTexture              *icon);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_icon_name (GtkSourceCompletionItem *item,
                                                                   const gchar             *icon_name);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_gicon     (GtkSourceCompletionItem *item,
                                                                   GIcon                   *gicon);
GTK_SOURCE_AVAILABLE_IN_3_24
void                     gtk_source_completion_item_set_info      (GtkSourceCompletionItem *item,
                                                                   const gchar             *info);

G_END_DECLS

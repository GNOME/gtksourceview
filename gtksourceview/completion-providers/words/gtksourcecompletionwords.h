/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
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

#include "../../gtksourcecompletionprovider.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_WORDS (gtk_source_completion_words_get_type())

struct _GtkSourceCompletionWordsClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceCompletionWords, gtk_source_completion_words, GTK_SOURCE, COMPLETION_WORDS, GObject)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionWords *gtk_source_completion_words_new        (const gchar              *title);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_words_register   (GtkSourceCompletionWords *words,
                                                                  GtkTextBuffer            *buffer);
GTK_SOURCE_AVAILABLE_IN_ALL
void                      gtk_source_completion_words_unregister (GtkSourceCompletionWords *words,
                                                                  GtkTextBuffer            *buffer);

G_END_DECLS

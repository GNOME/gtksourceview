/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
#include <gtksourceview/gtksourcecompletionprovider.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_SNIPPETS (gtk_source_completion_snippets_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceCompletionSnippets, gtk_source_completion_snippets, GTK_SOURCE, COMPLETION_SNIPPETS, GObject)

struct _GtkSourceCompletionSnippetsClass
{
	GObjectClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionSnippets *gtk_source_completion_snippets_new (void);

G_END_DECLS

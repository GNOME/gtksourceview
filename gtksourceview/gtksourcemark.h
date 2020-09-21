/*
 * This file is part of GtkSourceView
 *
 * Copyright 2007 - Johannes Schmid <jhs@gnome.org>
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

#define GTK_SOURCE_TYPE_MARK (gtk_source_mark_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkSourceMark, gtk_source_mark, GTK_SOURCE, MARK, GtkTextMark)

struct _GtkSourceMarkClass
{
	GtkTextMarkClass parent_class;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceMark *gtk_source_mark_new          (const gchar *name,
                                             const gchar *category);
GTK_SOURCE_AVAILABLE_IN_ALL
const gchar   *gtk_source_mark_get_category (GtkSourceMark *mark);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceMark *gtk_source_mark_next         (GtkSourceMark *mark,
                                             const gchar   *category);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceMark *gtk_source_mark_prev         (GtkSourceMark *mark,
                                             const gchar   *category);

G_END_DECLS

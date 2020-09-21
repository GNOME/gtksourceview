/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014 - Christian Hergert
 * Copyright 2014 - Ignacio Casal Quinteiro
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
 * along with GtkSourceView. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER_BUTTON (gtk_source_style_scheme_chooser_button_get_type())

struct _GtkSourceStyleSchemeChooserButtonClass
{
	GtkButtonClass parent;

	/*< private >*/
	gpointer _reserved[10];
};

GTK_SOURCE_AVAILABLE_IN_3_16
G_DECLARE_DERIVABLE_TYPE (GtkSourceStyleSchemeChooserButton, gtk_source_style_scheme_chooser_button, GTK_SOURCE, STYLE_SCHEME_CHOOSER_BUTTON, GtkButton)

GTK_SOURCE_AVAILABLE_IN_3_16
GtkWidget *gtk_source_style_scheme_chooser_button_new (void);

G_END_DECLS

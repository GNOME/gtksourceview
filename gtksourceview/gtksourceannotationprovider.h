/*
 * gtksourceannotationprovider.h
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#pragma once

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_ANNOTATION_PROVIDER (gtk_source_annotation_provider_get_type())

GTK_SOURCE_AVAILABLE_IN_5_18
G_DECLARE_DERIVABLE_TYPE (GtkSourceAnnotationProvider, gtk_source_annotation_provider, GTK_SOURCE, ANNOTATION_PROVIDER, GObject)

struct _GtkSourceAnnotationProviderClass
{
	GObjectClass parent_class;

	void     (*populate_hover_async)  (GtkSourceAnnotationProvider  *self,
	                                   GtkSourceAnnotation          *annotation,
	                                   GtkSourceHoverDisplay        *display,
	                                   GCancellable                 *cancellable,
	                                   GAsyncReadyCallback           callback,
	                                   gpointer                      user_data);
	gboolean (*populate_hover_finish) (GtkSourceAnnotationProvider  *self,
	                                   GAsyncResult                 *result,
	                                   GError                      **error);
};

GTK_SOURCE_AVAILABLE_IN_5_18
GtkSourceAnnotationProvider *gtk_source_annotation_provider_new                   (void);
GTK_SOURCE_AVAILABLE_IN_5_18
void                         gtk_source_annotation_provider_populate_hover_async  (GtkSourceAnnotationProvider  *self,
                                                                                   GtkSourceAnnotation          *annotation,
                                                                                   GtkSourceHoverDisplay        *display,
                                                                                   GCancellable                 *cancellable,
                                                                                   GAsyncReadyCallback           callback,
                                                                                   gpointer                      user_data);
GTK_SOURCE_AVAILABLE_IN_5_18
gboolean                     gtk_source_annotation_provider_populate_hover_finish (GtkSourceAnnotationProvider  *self,
                                                                                   GAsyncResult                 *result,
                                                                                   GError                      **error);
GTK_SOURCE_AVAILABLE_IN_5_18
void                         gtk_source_annotation_provider_add_annotation        (GtkSourceAnnotationProvider  *self,
                                                                                   GtkSourceAnnotation          *annotation);
GTK_SOURCE_AVAILABLE_IN_5_18
gboolean                     gtk_source_annotation_provider_remove_annotation     (GtkSourceAnnotationProvider  *self,
                                                                                   GtkSourceAnnotation          *annotation);
GTK_SOURCE_AVAILABLE_IN_5_18
void                         gtk_source_annotation_provider_remove_all            (GtkSourceAnnotationProvider  *self);

G_END_DECLS

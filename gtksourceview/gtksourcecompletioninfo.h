/*
 * This file is part of GtkSourceView
 *
 * Copyright 2007 - 2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 * Copyright 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
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

#define GTK_SOURCE_TYPE_COMPLETION_INFO (gtk_source_completion_info_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionInfo, gtk_source_completion_info, GTK_SOURCE, COMPLETION_INFO, GtkWindow)

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionInfo *gtk_source_completion_info_new          (void);
GTK_SOURCE_AVAILABLE_IN_ALL
void                     gtk_source_completion_info_move_to_iter (GtkSourceCompletionInfo *info,
                                                                  GtkTextView             *view,
                                                                  GtkTextIter             *iter);

G_END_DECLS

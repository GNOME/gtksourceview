/*
 * This file is part of GtkSourceView
 *
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

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_GNUC_INTERNAL
GtkSourceCompletion *_gtk_source_completion_new           (GtkSourceView               *source_view);
G_GNUC_INTERNAL
void                 _gtk_source_completion_add_proposals (GtkSourceCompletion         *completion,
                                                           GtkSourceCompletionContext  *context,
                                                           GtkSourceCompletionProvider *provider,
                                                           GList                       *proposals,
                                                           gboolean                     finished);
G_GNUC_INTERNAL
void                 _gtk_source_completion_css_changed   (GtkSourceCompletion         *completion,
                                                           GtkCssStyleChange           *change);

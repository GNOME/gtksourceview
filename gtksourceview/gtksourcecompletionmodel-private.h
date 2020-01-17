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
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_MODEL (gtk_source_completion_model_get_type())

enum
{
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_MARKUP,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_ICON_NAME,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_GICON,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROPOSAL,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_PROVIDER,
	GTK_SOURCE_COMPLETION_MODEL_COLUMN_IS_HEADER,
	GTK_SOURCE_COMPLETION_MODEL_N_COLUMNS
};

GTK_SOURCE_INTERNAL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionModel, gtk_source_completion_model, GTK_SOURCE, COMPLETION_MODEL, GObject)

GTK_SOURCE_INTERNAL
GtkSourceCompletionModel *gtk_source_completion_model_new                   (void);
GTK_SOURCE_INTERNAL
void                      gtk_source_completion_model_add_proposals         (GtkSourceCompletionModel    *model,
                                                                             GtkSourceCompletionProvider *provider,
                                                                             GList                       *proposals);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_is_empty              (GtkSourceCompletionModel    *model,
                                                                             gboolean                     only_visible);
GTK_SOURCE_INTERNAL
void                      gtk_source_completion_model_set_visible_providers (GtkSourceCompletionModel    *model,
                                                                             GList                       *providers);
GTK_SOURCE_INTERNAL
GList                    *gtk_source_completion_model_get_visible_providers (GtkSourceCompletionModel    *model);
GTK_SOURCE_INTERNAL
GList                    *gtk_source_completion_model_get_providers         (GtkSourceCompletionModel    *model);
GTK_SOURCE_INTERNAL
void                      gtk_source_completion_model_set_show_headers      (GtkSourceCompletionModel    *model,
                                                                             gboolean                     show_headers);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_iter_is_header        (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_iter_previous         (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_first_proposal        (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_last_proposal         (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_next_proposal         (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_previous_proposal     (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_has_info              (GtkSourceCompletionModel    *model);
GTK_SOURCE_INTERNAL
gboolean                  gtk_source_completion_model_iter_equal            (GtkSourceCompletionModel    *model,
                                                                             GtkTreeIter                 *iter1,
                                                                             GtkTreeIter                 *iter2);

G_END_DECLS

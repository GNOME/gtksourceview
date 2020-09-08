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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gtk/gtk.h>

#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_LIST_BOX (gtk_source_completion_list_box_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceCompletionListBox, gtk_source_completion_list_box, GTK_SOURCE, COMPLETION_LIST_BOX, GtkWidget)

GtkWidget                     *_gtk_source_completion_list_box_new              (void);
GtkSourceCompletionContext    *_gtk_source_completion_list_box_get_context      (GtkSourceCompletionListBox   *self);
void                           _gtk_source_completion_list_box_set_context      (GtkSourceCompletionListBox   *self,
                                                                                 GtkSourceCompletionContext   *context);
guint                          _gtk_source_completion_list_box_get_n_rows       (GtkSourceCompletionListBox   *self);
void                           _gtk_source_completion_list_box_set_n_rows       (GtkSourceCompletionListBox   *self,
                                                                                 guint                         n_rows);
GtkSourceCompletionProposal   *_gtk_source_completion_list_box_get_proposal     (GtkSourceCompletionListBox   *self);
gboolean                       _gtk_source_completion_list_box_get_selected     (GtkSourceCompletionListBox   *self,
                                                                                 GtkSourceCompletionProvider **provider,
                                                                                 GtkSourceCompletionProposal **proposal);
void                           _gtk_source_completion_list_box_move_cursor      (GtkSourceCompletionListBox   *self,
                                                                                 GtkMovementStep               step,
                                                                                 gint                          direction);
void                           _gtk_source_completion_list_box_set_font_desc    (GtkSourceCompletionListBox   *self,
                                                                                 const PangoFontDescription   *font_desc);
GtkSourceCompletionListBoxRow *_gtk_source_completion_list_box_get_first        (GtkSourceCompletionListBox   *self);
gboolean                       _gtk_source_completion_list_box_key_activates    (GtkSourceCompletionListBox   *self,
                                                                                 const GdkKeyEvent            *key);
int                            _gtk_source_completion_list_box_get_alternate    (GtkSourceCompletionListBox   *self);
guint                          _gtk_source_completion_list_box_get_n_alternates (GtkSourceCompletionListBox   *self);
void                           _gtk_source_completion_list_box_set_show_icons   (GtkSourceCompletionListBox   *self,
                                                                                 gboolean                      show_icons);

G_END_DECLS

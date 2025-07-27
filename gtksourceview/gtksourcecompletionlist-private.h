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

#include "gtksourceassistant-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_LIST (_gtk_source_completion_list_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceCompletionList, _gtk_source_completion_list, GTK_SOURCE, COMPLETION_LIST, GtkSourceAssistant)

GtkSourceCompletionList    *_gtk_source_completion_list_new                          (void);
void                        _gtk_source_completion_list_reposition                   (GtkSourceCompletionList    *self);
GtkSourceCompletionContext *_gtk_source_completion_list_get_context                  (GtkSourceCompletionList    *self);
void                        _gtk_source_completion_list_set_context                  (GtkSourceCompletionList    *self,
                                                                                      GtkSourceCompletionContext *context);
gboolean                    _gtk_source_completion_list_get_show_details             (GtkSourceCompletionList    *self);
void                        _gtk_source_completion_list_set_show_details             (GtkSourceCompletionList    *self,
                                                                                      gboolean                    show_details);
guint                       _gtk_source_completion_list_get_n_rows                   (GtkSourceCompletionList    *self);
void                        _gtk_source_completion_list_set_n_rows                   (GtkSourceCompletionList    *self,
                                                                                      guint                       n_rows);
void                        _gtk_source_completion_list_set_font_desc                (GtkSourceCompletionList    *self,
                                                                                      const PangoFontDescription *font_desc);
void                        _gtk_source_completion_list_set_show_icons               (GtkSourceCompletionList    *self,
                                                                                      gboolean                    show_icons);
void                        _gtk_source_completion_list_set_remember_info_visibility (GtkSourceCompletionList    *self,
                                                                                      gboolean                    remember_info_visibility);
void                        _gtk_source_completion_list_move_cursor                  (GtkSourceCompletionList    *self,
                                                                                      GtkMovementStep             step,
                                                                                      int                         direction);

G_END_DECLS

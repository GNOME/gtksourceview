/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcevimstate.h"

G_BEGIN_DECLS

typedef enum
{
	GTK_SOURCE_VIM_VISUAL_CHAR,
	GTK_SOURCE_VIM_VISUAL_LINE,
	GTK_SOURCE_VIM_VISUAL_BLOCK,
} GtkSourceVimVisualMode;

#define GTK_SOURCE_TYPE_VIM_VISUAL (gtk_source_vim_visual_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceVimVisual, gtk_source_vim_visual, GTK_SOURCE, VIM_VISUAL, GtkSourceVimState)

GtkSourceVimState *gtk_source_vim_visual_new            (GtkSourceVimVisualMode  mode);
GtkSourceVimState *gtk_source_vim_visual_clone          (GtkSourceVimVisual     *self);
gboolean           gtk_source_vim_visual_get_bounds     (GtkSourceVimVisual     *self,
                                                         GtkTextIter            *cursor,
                                                         GtkTextIter            *started_at);
void               gtk_source_vim_visual_warp           (GtkSourceVimVisual     *self,
                                                         const GtkTextIter      *iter,
                                                         const GtkTextIter      *selection);
void               gtk_source_vim_visual_ignore_command (GtkSourceVimVisual     *self);

static inline gboolean
GTK_SOURCE_IN_VIM_VISUAL (gpointer data)
{
	return gtk_source_vim_state_get_ancestor (data, GTK_SOURCE_TYPE_VIM_VISUAL) != NULL;
}

G_END_DECLS

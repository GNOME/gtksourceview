/*
 * This file is part of GtkSourceView
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcegutterrenderertext.h"

G_BEGIN_DECLS

void _gtk_source_gutter_renderer_text_get_draw (GtkSourceGutterRendererText *self,
                                                GdkRGBA                     *foreground_color,
                                                GdkRGBA                     *current_line_color,
                                                gboolean                    *current_line_bold);

G_END_DECLS

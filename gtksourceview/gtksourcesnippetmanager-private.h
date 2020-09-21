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
 */

#pragma once

#include "gtksourcesnippetmanager.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkSourceSnippetManager *_gtk_source_snippet_manager_peek_default (void);
G_GNUC_INTERNAL
const gchar             *_gtk_source_snippet_manager_intern       (GtkSourceSnippetManager *manager,
                                                                   const gchar             *str);

G_END_DECLS

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2015 Christian Hergert <christian@hergert.me>
 * Copyright 2015 Garrett Regier <garrettregier@gmail.com>
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

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_BINDING_GROUP (gtk_source_binding_group_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceBindingGroup, gtk_source_binding_group, GTK_SOURCE, BINDING_GROUP, GObject)

GtkSourceBindingGroup *gtk_source_binding_group_new                (void);
GObject               *gtk_source_binding_group_get_source         (GtkSourceBindingGroup *self);
void                   gtk_source_binding_group_set_source         (GtkSourceBindingGroup *self,
                                                                    gpointer               source);
void                   gtk_source_binding_group_bind               (GtkSourceBindingGroup *self,
                                                                    const gchar           *source_property,
                                                                    gpointer               target,
                                                                    const gchar           *target_property,
                                                                    GBindingFlags          flags);
void                   gtk_source_binding_group_bind_full          (GtkSourceBindingGroup *self,
                                                                    const gchar           *source_property,
                                                                    gpointer               target,
                                                                    const gchar           *target_property,
                                                                    GBindingFlags          flags,
                                                                    GBindingTransformFunc  transform_to,
                                                                    GBindingTransformFunc  transform_from,
                                                                    gpointer               user_data,
                                                                    GDestroyNotify         user_data_destroy);
void                   gtk_source_binding_group_bind_with_closures (GtkSourceBindingGroup *self,
                                                                    const gchar           *source_property,
                                                                    gpointer               target,
                                                                    const gchar           *target_property,
                                                                    GBindingFlags          flags,
                                                                    GClosure              *transform_to,
                                                                    GClosure              *transform_from);

G_END_DECLS

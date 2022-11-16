/* gtksourcelistsnapshot-private.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_LIST_SNAPSHOT (gtk_source_list_snapshot_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceListSnapshot, gtk_source_list_snapshot, GTK_SOURCE, LIST_SNAPSHOT, GObject)

GtkSourceListSnapshot *gtk_source_list_snapshot_new       (void);
GListModel            *gtk_source_list_snapshot_get_model (GtkSourceListSnapshot *self);
void                   gtk_source_list_snapshot_set_model (GtkSourceListSnapshot *self,
                                                           GListModel            *model);
void                   gtk_source_list_snapshot_hold      (GtkSourceListSnapshot *self,
                                                           guint                  position,
                                                           guint                  length);
void                   gtk_source_list_snapshot_release   (GtkSourceListSnapshot *self);

G_END_DECLS

/* gtk_source-signal-group.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_SIGNAL_GROUP (gtk_source_signal_group_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceSignalGroup, gtk_source_signal_group, GTK_SOURCE, SIGNAL_GROUP, GObject)

GtkSourceSignalGroup *gtk_source_signal_group_new             (GType                 target_type);
void                  gtk_source_signal_group_set_target      (GtkSourceSignalGroup *self,
                                                               gpointer              target);
gpointer              gtk_source_signal_group_get_target      (GtkSourceSignalGroup *self);
void                  gtk_source_signal_group_block           (GtkSourceSignalGroup *self);
void                  gtk_source_signal_group_unblock         (GtkSourceSignalGroup *self);
void                  gtk_source_signal_group_connect_object  (GtkSourceSignalGroup *self,
                                                               const gchar          *detailed_signal,
                                                               GCallback             c_handler,
                                                               gpointer              object,
                                                               GConnectFlags         flags);
void                  gtk_source_signal_group_connect_data    (GtkSourceSignalGroup *self,
                                                               const gchar          *detailed_signal,
                                                               GCallback             c_handler,
                                                               gpointer              data,
                                                               GClosureNotify        notify,
                                                               GConnectFlags         flags);
void                  gtk_source_signal_group_connect         (GtkSourceSignalGroup *self,
                                                               const gchar          *detailed_signal,
                                                               GCallback             c_handler,
                                                               gpointer              data);
void                  gtk_source_signal_group_connect_after   (GtkSourceSignalGroup *self,
                                                               const gchar          *detailed_signal,
                                                               GCallback             c_handler,
                                                               gpointer              data);
void                  gtk_source_signal_group_connect_swapped (GtkSourceSignalGroup *self,
                                                               const gchar          *detailed_signal,
                                                               GCallback             c_handler,
                                                               gpointer              data);

G_END_DECLS

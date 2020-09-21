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

#include "gtksourceassistant-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_INFORMATIVE (gtk_source_informative_get_type())

G_DECLARE_DERIVABLE_TYPE (GtkSourceInformative, gtk_source_informative, GTK_SOURCE, INFORMATIVE, GtkSourceAssistant)

struct _GtkSourceInformativeClass
{
	GtkSourceAssistantClass parent_class;
};

const char     *gtk_source_informative_get_message      (GtkSourceInformative *self);
void            gtk_source_informative_set_message      (GtkSourceInformative *self,
                                                         const char           *message);
void            gtk_source_informative_set_message_type (GtkSourceInformative *self,
                                                         GtkMessageType        message_type);
GtkMessageType  gtk_source_informative_get_message_type (GtkSourceInformative *self);
const char     *gtk_source_informative_get_icon_name    (GtkSourceInformative *self);
void            gtk_source_informative_set_icon_name    (GtkSourceInformative *self,
                                                         const char           *icon_name);

G_END_DECLS

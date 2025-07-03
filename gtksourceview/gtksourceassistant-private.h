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

#define GTK_SOURCE_TYPE_ASSISTANT (_gtk_source_assistant_get_type())

G_DECLARE_DERIVABLE_TYPE (GtkSourceAssistant, _gtk_source_assistant, GTK_SOURCE, ASSISTANT, GtkPopover)

struct _GtkSourceAssistantClass
{
	GtkPopoverClass parent_class;

	void (*get_offset)          (GtkSourceAssistant *assistant,
	                             int                *x_offset,
	                             int                *y_offset);
	void (*get_target_location) (GtkSourceAssistant *assistant,
	                             GdkRectangle       *rect);
};

GtkSourceAssistant *_gtk_source_assistant_new                (void);
void                _gtk_source_assistant_attach             (GtkSourceAssistant *assistant,
                                                              GtkSourceAssistant *attached_to);
void                _gtk_source_assistant_detach             (GtkSourceAssistant *assistant);
void                _gtk_source_assistant_get_offset         (GtkSourceAssistant *assistant,
                                                              int                *x,
                                                              int                *y);
GtkTextMark        *_gtk_source_assistant_get_mark           (GtkSourceAssistant *assistant);
void                _gtk_source_assistant_set_mark           (GtkSourceAssistant *assistant,
                                                              GtkTextMark        *mark);
void                _gtk_source_assistant_set_child          (GtkSourceAssistant *assistant,
                                                              GtkWidget          *child);
gboolean            _gtk_source_assistant_update_position    (GtkSourceAssistant *assistant);
void                _gtk_source_assistant_destroy            (GtkSourceAssistant *assistant);
void                _gtk_source_assistant_set_pref_position  (GtkSourceAssistant *self,
                                                              GtkPositionType     position);

G_END_DECLS

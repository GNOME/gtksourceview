/*
 * This file is part of GtkSourceView
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

typedef struct _GtkSourceTagPlan GtkSourceTagPlan;

GtkSourceTagPlan *_gtk_source_tag_plan_new     (GtkTextBuffer    *buffer,
                                                int               start,
                                                int               end);
void              _gtk_source_tag_plan_add_tag (GtkSourceTagPlan *plan,
                                                GtkTextTag       *tag);
void              _gtk_source_tag_plan_add     (GtkSourceTagPlan *plan,
                                                GtkTextTag       *tag,
                                                int               start,
                                                int               end,
                                                gboolean          enabled);
gboolean          _gtk_source_tag_plan_apply   (GtkSourceTagPlan *plan,
                                                gint64            deadline);
void              _gtk_source_tag_plan_free    (GtkSourceTagPlan *plan);

G_END_DECLS

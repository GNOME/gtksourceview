/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 * Copyright 2005, 2006 - Marco Barisione, Emanuele Aina
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

#include <glib.h>

#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

GTK_SOURCE_INTERNAL
GtkSourceRegex *_gtk_source_regex_new             (const gchar         *pattern,
                                                   GRegexCompileFlags   flags,
                                                   GError             **error);
GTK_SOURCE_INTERNAL
GtkSourceRegex *_gtk_source_regex_ref             (GtkSourceRegex      *regex);
GTK_SOURCE_INTERNAL
void            _gtk_source_regex_unref           (GtkSourceRegex      *regex);
GTK_SOURCE_INTERNAL
GtkSourceRegex *_gtk_source_regex_resolve         (GtkSourceRegex      *regex,
                                                   GtkSourceRegex      *start_regex,
                                                   const gchar         *matched_text);
GTK_SOURCE_INTERNAL
gboolean        _gtk_source_regex_is_resolved     (GtkSourceRegex      *regex);
GTK_SOURCE_INTERNAL
gboolean        _gtk_source_regex_match           (GtkSourceRegex      *regex,
                                                   const gchar         *line,
                                                   gint                 byte_length,
                                                   gint                 byte_pos);
GTK_SOURCE_INTERNAL
gchar          *_gtk_source_regex_fetch           (GtkSourceRegex      *regex,
                                                   gint                 num);
GTK_SOURCE_INTERNAL
void            _gtk_source_regex_fetch_pos       (GtkSourceRegex      *regex,
                                                   const gchar         *text,
                                                   gint                 num,
                                                   gint                *start_pos,
                                                   gint                *end_pos);
GTK_SOURCE_INTERNAL
void            _gtk_source_regex_fetch_pos_bytes (GtkSourceRegex      *regex,
                                                   gint                 num,
                                                   gint                *start_pos_p,
                                                   gint                *end_pos_p);
GTK_SOURCE_INTERNAL
void            _gtk_source_regex_fetch_named_pos (GtkSourceRegex      *regex,
                                                   const gchar         *text,
                                                   const gchar         *name,
                                                   gint                *start_pos,
                                                   gint                *end_pos);
GTK_SOURCE_INTERNAL
const gchar    *_gtk_source_regex_get_pattern     (GtkSourceRegex      *regex);

G_END_DECLS

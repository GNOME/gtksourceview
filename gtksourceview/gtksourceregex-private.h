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

GtkSourceRegex *_gtk_source_regex_new                   (const gchar         *pattern,
                                                         GRegexCompileFlags   flags,
                                                         GError             **error);
GtkSourceRegex *_gtk_source_regex_new_matcher           (GtkSourceRegex      *regex);
GtkSourceRegex *_gtk_source_regex_ref                   (GtkSourceRegex      *regex);
void            _gtk_source_regex_unref                 (GtkSourceRegex      *regex);
GtkSourceRegex *_gtk_source_regex_resolve               (GtkSourceRegex      *regex,
                                                         GtkSourceRegex      *start_regex,
                                                         const gchar         *matched_text);
gboolean        _gtk_source_regex_is_resolved           (GtkSourceRegex      *regex);
gboolean        _gtk_source_regex_match                 (GtkSourceRegex      *regex,
                                                         const gchar         *line,
                                                         gint                 byte_length,
                                                         gint                 byte_pos);
gchar          *_gtk_source_regex_fetch                 (GtkSourceRegex      *regex,
                                                         gint                 num);
void            _gtk_source_regex_fetch_pos_bytes       (GtkSourceRegex      *regex,
                                                         gint                 num,
                                                         gint                *start_pos_p,
                                                         gint                *end_pos_p);
void            _gtk_source_regex_fetch_named_pos_bytes (GtkSourceRegex      *regex,
                                                         const char          *name,
                                                         int                 *start_pos,
                                                         int                 *end_pos);
const gchar    *_gtk_source_regex_get_pattern           (GtkSourceRegex      *regex);
/* Required ASCII byte at the beginning of a match, or -1 if unknown. */
int             _gtk_source_regex_get_first_byte        (GtkSourceRegex      *regex);

G_END_DECLS

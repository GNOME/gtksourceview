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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ImplRegex     ImplRegex;
typedef struct _ImplMatchInfo ImplMatchInfo;

typedef gboolean (*ImplRegexEvalCallback) (const ImplMatchInfo *match_info,
                                           GString             *result,
                                           gpointer             user_data);


ImplRegex  *impl_regex_new                   (const char             *pattern,
                                              GRegexCompileFlags      compile_options,
                                              GRegexMatchFlags        match_options,
                                              GError                **error);
gboolean    impl_regex_match                 (const ImplRegex        *regex,
                                              const char             *string,
                                              GRegexMatchFlags        match_options,
                                              ImplMatchInfo         **match_info);
ImplRegex  *impl_regex_ref                   (ImplRegex              *regex);
void        impl_regex_unref                 (ImplRegex              *regex);
void        impl_match_info_free             (ImplMatchInfo          *match_info);
char       *impl_match_info_fetch            (const ImplMatchInfo    *match_info,
                                              int                     match_num);
char       *impl_match_info_fetch_named      (const ImplMatchInfo    *match_info,
                                              const char             *name);
char       *impl_regex_replace_eval          (const ImplRegex        *regex,
                                              const char             *string,
                                              gssize                  string_len,
                                              gsize                   start_position,
                                              GRegexMatchFlags        match_options,
                                              ImplRegexEvalCallback   eval,
                                              gpointer                user_data,
                                              GError                **error);
char       *impl_regex_replace               (const ImplRegex        *regex,
                                              const char             *string,
                                              gssize                  string_len,
                                              int                     start_position,
                                              const char             *replacement,
                                              GRegexMatchFlags        match_options,
                                              GError                **error);
gboolean    impl_regex_match_full            (const ImplRegex        *regex,
                                              const char             *string,
                                              gssize                  string_len,
                                              gsize                   start_position,
                                              GRegexMatchFlags        match_options,
                                              ImplMatchInfo         **match_info,
                                              GError                **error);
gboolean    impl_match_info_fetch_pos        (const ImplMatchInfo    *match_info,
                                              int                     match_num,
                                              int                    *start_pos,
                                              int                    *end_pos);
gboolean    impl_match_info_fetch_named_pos  (const ImplMatchInfo    *match_info,
                                              const char             *name,
                                              int                    *start_pos,
                                              int                    *end_pos);
gboolean    impl_match_info_is_partial_match (const ImplMatchInfo    *match_info);
gboolean    impl_match_info_matches          (const ImplMatchInfo    *match_info);
gboolean    impl_match_info_next             (ImplMatchInfo          *match_info,
                                              GError                **error);
int         impl_match_info_get_match_count  (const ImplMatchInfo    *match_info);
const char *impl_regex_get_pattern           (const ImplRegex        *regex);
int         impl_regex_get_max_lookbehind    (const ImplRegex        *regex);

G_END_DECLS

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

#include "config.h"

#include "implregex-private.h"

struct _ImplRegex
{
	int         ref_count;
	char       *pattern;
	GRegex     *re;
};

struct _ImplMatchInfo
{
	GMatchInfo *match_info;
};

#if 0
static void
set_regex_error (GError **error,
                 int      errnum)
{
	guchar errstr[128];

	pcre2_get_error_message (errnum, errstr, sizeof errstr - 1);
	errstr[sizeof errstr - 1] = 0;

	g_set_error_literal (error,
	                     G_REGEX_ERROR,
	                     G_REGEX_ERROR_COMPILE,
	                     (const gchar *)errstr);
}
#endif

static ImplMatchInfo *
impl_match_info_new (const ImplRegex *regex)
{
	ImplMatchInfo *match_info;

	match_info = g_slice_new0 (ImplMatchInfo);
	match_info->match_info = NULL;

	return match_info;
}

ImplRegex *
impl_regex_new (const char          *pattern,
                GRegexCompileFlags   compile_options,
                GRegexMatchFlags     match_options,
                GError             **error)
{
	GRegex *re;
	ImplRegex *regex;

	g_return_val_if_fail (pattern != NULL, NULL);

	re = g_regex_new (pattern, compile_options, match_options, error);

	if (re == NULL)
	{
		return NULL;
	}

	regex = g_slice_new0 (ImplRegex);
	regex->ref_count = 1;
	regex->pattern = g_strdup (pattern);
	regex->re = re;

	return regex;
}

const char *
impl_regex_get_pattern (const ImplRegex *regex)
{
	g_return_val_if_fail (regex != NULL, NULL);

	return regex->pattern;
}

void
impl_regex_unref (ImplRegex *regex)
{
	g_return_if_fail (regex != NULL);
	g_return_if_fail (regex->ref_count > 0);

	regex->ref_count--;

	if (regex->ref_count == 0)
	{
		g_clear_pointer (&regex->pattern, g_free);
		g_clear_pointer (&regex->re, g_regex_unref);
		g_slice_free (ImplRegex, regex);
	}
}

void
impl_match_info_free (ImplMatchInfo *match_info)
{
	g_clear_pointer (&match_info->match_info, g_match_info_free);
	g_slice_free (ImplMatchInfo, match_info);
}

gboolean
impl_regex_match (const ImplRegex   *regex,
                  const char        *string,
                  GRegexMatchFlags   match_options,
                  ImplMatchInfo    **match_info)
{
	g_return_val_if_fail (regex != NULL, FALSE);
	g_return_val_if_fail (regex->re != NULL, FALSE);

	if (match_info != NULL)
	{
		*match_info = impl_match_info_new (regex);
	}

	return g_regex_match (regex->re,
	                      string,
	                      match_options,
	                      match_info ? &(*match_info)->match_info : NULL);
}

char *
impl_match_info_fetch (const ImplMatchInfo *match_info,
                       int                  match_num)
{
	g_return_val_if_fail (match_info != NULL, NULL);

	return g_match_info_fetch (match_info->match_info, match_num);
}

char *
impl_match_info_fetch_named (const ImplMatchInfo *match_info,
                             const char          *name)
{
	g_return_val_if_fail (match_info != NULL, NULL);

	return g_match_info_fetch_named (match_info->match_info, name);
}

static gboolean
wrapper_eval (const GMatchInfo *match_info,
              GString          *result,
              gpointer          user_data)
{
	struct {
		ImplRegexEvalCallback callback;
		gpointer user_data;
	} *wrapper = user_data;
	ImplMatchInfo wrapped = {
		.match_info = (GMatchInfo *)match_info,
	};

	return wrapper->callback (&wrapped, result, wrapper->user_data);
}

char *
impl_regex_replace_eval (const ImplRegex        *regex,
                         const char             *string,
                         gssize                  string_len,
                         int                     start_position,
                         GRegexMatchFlags        match_options,
                         ImplRegexEvalCallback   eval,
                         gpointer                user_data,
                         GError                **error)
{
	struct {
		ImplRegexEvalCallback callback;
		gpointer user_data;
	} wrapper;

	g_return_val_if_fail (regex != NULL, NULL);
	g_return_val_if_fail (regex->re != NULL, NULL);

	wrapper.callback = eval;
	wrapper.user_data = user_data;

	return g_regex_replace_eval (regex->re,
	                             string,
	                             string_len,
	                             start_position,
	                             match_options,
	                             wrapper_eval,
	                             &wrapper,
	                             error);
}

gboolean
impl_regex_match_full (const ImplRegex   *regex,
                       const char        *string,
                       gssize             string_len,
                       int                start_position,
                       GRegexMatchFlags   match_options,
                       ImplMatchInfo    **match_info,
                       GError           **error)
{
	GMatchInfo *wrapped = NULL;
	gboolean ret;

	g_return_val_if_fail (regex != NULL, FALSE);
	g_return_val_if_fail (regex->re != NULL, FALSE);

	ret = g_regex_match_full (regex->re,
	                          string,
	                          string_len,
	                          start_position,
	                          match_options,
	                          &wrapped,
	                          error);

	if (match_info != NULL)
	{
		*match_info = g_slice_new0 (ImplMatchInfo);
		(*match_info)->match_info = wrapped;
	}
	else
	{
		g_match_info_free (wrapped);
	}

	return ret;
}

gboolean
impl_match_info_fetch_pos (const ImplMatchInfo *match_info,
                           int                  match_num,
                           int                 *start_pos,
                           int                 *end_pos)
{
	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->match_info != NULL, FALSE);

	return g_match_info_fetch_pos (match_info->match_info, match_num, start_pos, end_pos);
}

gboolean
impl_match_info_fetch_named_pos (const ImplMatchInfo *match_info,
                                 const char          *name,
                                 int                 *start_pos,
                                 int                 *end_pos)
{
	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->match_info != NULL, FALSE);

	return g_match_info_fetch_named_pos (match_info->match_info, name, start_pos, end_pos);
}

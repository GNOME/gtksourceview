/*
 * This file is part of GtkSourceView
 *
 * Copyright 1999, 2000 Scott Wimer
 * Copyright 2004, Matthias Clasen <mclasen@redhat.com>
 * Copyright 2005 - 2007, Marco Barisione <marco@barisione.org>
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

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <string.h>

#include "implregex-private.h"
#include "gtksourcetrace.h"

struct _ImplRegex
{
	int                    ref_count;
	char                  *pattern;
	gsize                  compile_flags;
	gsize                  match_flags;
	pcre2_compile_context *context;
	pcre2_code            *code;
	guint                  has_jit : 1;
};

struct _ImplMatchInfo
{
	gsize             compile_flags;
	gsize             match_flags;
	ImplRegex        *regex;
	const char       *string;
	gsize             string_len;
	pcre2_match_data *match_data;
	PCRE2_SIZE       *offsets;
	int               n_groups;
	gsize             start_pos;
};

static gsize
translate_compile_flags (GRegexCompileFlags flags)
{
	gsize ret = 0;

	if (!(flags & G_REGEX_RAW))
		ret |= (PCRE2_UTF | PCRE2_NO_UTF_CHECK);

	if (flags & G_REGEX_ANCHORED)
		ret |= PCRE2_ANCHORED;

	if (flags & G_REGEX_CASELESS)
		ret |= PCRE2_CASELESS;

	if (flags & G_REGEX_EXTENDED)
		ret |= PCRE2_EXTENDED;

	if (flags & G_REGEX_DUPNAMES)
		ret |= PCRE2_DUPNAMES;

	ret |= PCRE2_UCP;

	if (~flags & G_REGEX_BSR_ANYCRLF)
		ret |= PCRE2_BSR_UNICODE;

	return ret;
}

static gsize
translate_match_flags (GRegexMatchFlags flags)
{
	gsize ret = 0;

	if (flags & G_REGEX_MATCH_ANCHORED)
		ret |= PCRE2_ANCHORED;

	return ret;
}

static gboolean
set_regex_error (GError **error,
                 int      rc)
{
	if (rc > 0)
	{
		return FALSE;
	}

	if (error != NULL)
	{
		guchar errstr[128];

		pcre2_get_error_message (rc, errstr, sizeof errstr - 1);
		errstr[sizeof errstr - 1] = 0;

		g_set_error_literal (error,
		                     G_REGEX_ERROR,
		                     G_REGEX_ERROR_MATCH,
		                     (const gchar *)errstr);
	}

	return TRUE;
}

ImplRegex *
impl_regex_new (const char          *pattern,
                GRegexCompileFlags   compile_options,
                GRegexMatchFlags     match_options,
                GError             **error)
{
	pcre2_compile_context *context;
	ImplRegex *regex;
	PCRE2_SIZE erroffset;
	int errnumber = -1;
#ifdef GTK_SOURCE_PROFILER_ENABLED
	char *message = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;
#endif

	g_return_val_if_fail (pattern != NULL, NULL);

	context = pcre2_compile_context_create (NULL);

	regex = g_slice_new0 (ImplRegex);
	regex->ref_count = 1;
	regex->context = context;
	regex->pattern = g_strdup (pattern);
	regex->compile_flags = translate_compile_flags (compile_options);
	regex->match_flags = translate_match_flags (match_options);

	if (compile_options & G_REGEX_NEWLINE_LF)
		pcre2_set_newline (context, PCRE2_NEWLINE_LF);
	else if (compile_options & G_REGEX_NEWLINE_CR)
		pcre2_set_newline (context, PCRE2_NEWLINE_CR);
	else if (compile_options & G_REGEX_NEWLINE_CRLF)
		pcre2_set_newline (context, PCRE2_NEWLINE_CRLF);
	else if (compile_options & G_REGEX_NEWLINE_ANYCRLF)
		pcre2_set_newline (context, PCRE2_NEWLINE_ANYCRLF);
	else
		pcre2_set_newline (context, PCRE2_NEWLINE_ANY);

	regex->code = pcre2_compile ((PCRE2_SPTR)pattern,
	                             PCRE2_ZERO_TERMINATED,
	                             regex->compile_flags,
	                             &errnumber,
	                             &erroffset,
	                             context);

	if (regex->code == NULL)
	{
		char errmsg[128];

		pcre2_get_error_message (errnumber, (guchar *)errmsg, sizeof errmsg-1);
		errmsg[sizeof errmsg-1] = 0;

		g_set_error (error,
		             G_REGEX_ERROR,
		             G_REGEX_ERROR_COMPILE,
		             "%s: offset %d of pattern %s",
		             errmsg,
		             (int)erroffset,
		             pattern);
		impl_regex_unref (regex);
		return NULL;
	}

	/* Now try to JIT the pattern for faster execution time */
	regex->has_jit = pcre2_jit_compile (regex->code, PCRE2_JIT_COMPLETE) == 0;

#ifdef GTK_SOURCE_PROFILER_ENABLED
	if (GTK_SOURCE_PROFILER_ACTIVE)
		message = g_strdup_printf ("compile=%lx match=%lx pattern=%s",
		                           regex->compile_flags,
		                           regex->match_flags,
		                           regex->pattern);
	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, message);
	g_free (message);
#endif

	return regex;
}

const char *
impl_regex_get_pattern (const ImplRegex *regex)
{
	g_return_val_if_fail (regex != NULL, NULL);

	return regex->pattern;
}

ImplRegex *
impl_regex_ref (ImplRegex *regex)
{
	g_return_val_if_fail (regex != NULL, NULL);
	g_return_val_if_fail (regex->ref_count > 0, NULL);

	regex->ref_count++;

	return regex;
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
		g_clear_pointer (&regex->code, pcre2_code_free);
		g_clear_pointer (&regex->context, pcre2_compile_context_free);
		g_slice_free (ImplRegex, regex);
	}
}

static ImplMatchInfo *
impl_match_info_new (ImplRegex        *regex,
                     GRegexMatchFlags  match_options,
                     const char       *string,
                     gssize            string_len)
{
	ImplMatchInfo *match_info;

	g_assert (regex != NULL);
	g_assert (string != NULL);
	g_assert (string_len <= strlen (string));

	if (string_len < 0)
	{
		string_len = strlen (string);
	}

	match_info = g_slice_new0 (ImplMatchInfo);
	match_info->regex = impl_regex_ref (regex);
	match_info->match_flags = regex->match_flags | translate_match_flags (match_options);
	match_info->start_pos = -1;
	match_info->n_groups = -1;
	match_info->string = string;
	match_info->string_len = string_len;
	match_info->match_data = pcre2_match_data_create_from_pattern (regex->code, NULL);

	if (match_info->match_data == NULL)
	{
		g_error ("Failed to allocate match data");
	}

	match_info->offsets = pcre2_get_ovector_pointer (match_info->match_data);

	return match_info;
}

void
impl_match_info_free (ImplMatchInfo *match_info)
{
	if (match_info != NULL)
	{
		g_clear_pointer (&match_info->match_data, pcre2_match_data_free);
		g_clear_pointer (&match_info->regex, impl_regex_unref);
		match_info->string = NULL;
		match_info->string_len = 0;
		match_info->compile_flags = 0;
		match_info->match_flags = 0;
		match_info->n_groups = 0;
		match_info->start_pos = 0;
		match_info->offsets = NULL;
		g_slice_free (ImplMatchInfo, match_info);
	}
}

gboolean
impl_regex_match (const ImplRegex   *regex,
                  const char        *string,
                  GRegexMatchFlags   match_options,
                  ImplMatchInfo    **match_info)
{
	g_return_val_if_fail (regex != NULL, FALSE);
	g_return_val_if_fail (regex->code != NULL, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	return impl_regex_match_full (regex, string, -1, 0, match_options, match_info, NULL);
}

char *
impl_match_info_fetch (const ImplMatchInfo *match_info,
                       int                  match_num)
{
	int begin =  -1;
	int end =  -1;

	g_return_val_if_fail (match_info != NULL, NULL);
	g_return_val_if_fail (match_info->string != NULL, NULL);
	g_return_val_if_fail (match_info->offsets != NULL, NULL);
	g_return_val_if_fail (impl_match_info_matches (match_info), NULL);

	if (impl_match_info_fetch_pos (match_info, match_num, &begin, &end))
	{
		if (begin >= 0 && end >= 0)
		{
			return g_strndup (match_info->string + begin, end - begin);
		}

		return g_strdup ("");
	}

	return NULL;
}

char *
impl_match_info_fetch_named (const ImplMatchInfo *match_info,
                             const char          *name)
{
	int begin = -1;
	int end = -1;

	g_return_val_if_fail (match_info != NULL, NULL);

	if (match_info->start_pos < match_info->string_len)
	{
		if (impl_match_info_fetch_named_pos (match_info, name, &begin, &end))
		{
			if (begin >= 0 && end >= 0)
			{
				return g_strndup (match_info->string + begin, end - begin);
			}
		}
	}

	return NULL;
}

char *
impl_regex_replace_eval (const ImplRegex        *regex,
                         const char             *string,
                         gssize                  string_len,
                         gsize                   start_position,
                         GRegexMatchFlags        match_options,
                         ImplRegexEvalCallback   eval,
                         gpointer                user_data,
                         GError                **error)
{
	ImplMatchInfo *match_info;
	GString *result;
	gsize str_pos = 0;
	gboolean done = FALSE;
	GError *tmp_error = NULL;

	g_return_val_if_fail (regex != NULL, NULL);
	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (eval != NULL, NULL);

	if (string_len < 0)
	{
		string_len = strlen (string);
	}

	result = g_string_sized_new (string_len);

	/* run down the string making matches. */
	impl_regex_match_full (regex,
	                       string,
	                       string_len,
	                       start_position,
	                       match_options,
	                       &match_info,
	                       &tmp_error);

	g_assert (match_info != NULL);

	while (!done && impl_match_info_matches (match_info))
	{
		g_string_append_len (result,
		                     string + str_pos,
		                     match_info->offsets[0] - str_pos);
		done = (*eval) (match_info, result, user_data);
		str_pos = match_info->offsets[1];
		impl_match_info_next (match_info, &tmp_error);
	}

	impl_match_info_free (match_info);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		g_string_free (result, TRUE);
		return NULL;
	}

	g_string_append_len (result, string + str_pos, string_len - str_pos);

	return g_string_free (result, FALSE);
}

gboolean
impl_regex_match_full (const ImplRegex   *regex,
                       const char        *string,
                       gssize             string_len,
                       gsize              start_position,
                       GRegexMatchFlags   match_options,
                       ImplMatchInfo    **match_info,
                       GError           **error)
{
	ImplMatchInfo *local_match_info = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail (regex != NULL, FALSE);
	g_return_val_if_fail (regex->code != NULL, FALSE);
	g_return_val_if_fail (match_options == 0, FALSE);
	g_return_val_if_fail (string != NULL, FALSE);

	if (string_len < 0)
	{
		string_len = strlen (string);
	}

	local_match_info = impl_match_info_new ((ImplRegex *)regex, match_options, string, string_len);
	local_match_info->start_pos = start_position;

	ret = impl_match_info_next (local_match_info, error);

	if (match_info != NULL)
	{
		*match_info = g_steal_pointer (&local_match_info);
	}
	else
	{
		impl_match_info_free (local_match_info);
	}

	return ret;
}

gboolean
impl_match_info_fetch_pos (const ImplMatchInfo *match_info,
                           guint                match_num,
                           int                 *start_pos,
                           int                 *end_pos)
{
	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->match_data != NULL, FALSE);
	g_return_val_if_fail (match_info->offsets != NULL, FALSE);

	if (match_info->n_groups > 0 && match_num < match_info->n_groups)
	{
		if (start_pos)
			*start_pos = match_info->offsets[2*match_num];

		if (end_pos)
			*end_pos = match_info->offsets[2*match_num+1];

		return TRUE;
	}

	return FALSE;
}

gboolean
impl_match_info_fetch_named_pos (const ImplMatchInfo *match_info,
                                 const char          *name,
                                 int                 *start_pos,
                                 int                 *end_pos)
{
	int num;

	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->match_data != NULL, FALSE);
	g_return_val_if_fail (match_info->regex != NULL, FALSE);
	g_return_val_if_fail (start_pos != NULL, FALSE);
	g_return_val_if_fail (end_pos != NULL, FALSE);

	num = pcre2_substring_number_from_name (match_info->regex->code, (PCRE2_SPTR)name);

	if (num >= 0)
	{
		return impl_match_info_fetch_pos (match_info, num, start_pos, end_pos);
	}

	return FALSE;
}

gboolean
impl_match_info_matches (const ImplMatchInfo *match_info)
{
	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->n_groups != 0, FALSE);

	return match_info->n_groups > 0;
}

gboolean
impl_match_info_next (ImplMatchInfo  *match_info,
                      GError        **error)
{
	gssize prev_end;
	gssize prev_begin;
	int rc;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	g_assert (match_info != NULL);
	g_assert (match_info->regex != NULL);
	g_assert (match_info->regex->code != NULL);
	g_assert (match_info->offsets == pcre2_get_ovector_pointer (match_info->match_data));

again:
	match_info->n_groups = -1;

	if (match_info->start_pos >= match_info->string_len)
	{
		return FALSE;
	}

	prev_begin = match_info->offsets[0];
	prev_end = match_info->offsets[1];

	if (match_info->regex->has_jit)
	{
		rc = pcre2_jit_match (match_info->regex->code,
		                      (PCRE2_SPTR)match_info->string,
		                      match_info->string_len,
		                      match_info->start_pos,
		                      match_info->match_flags,
		                      match_info->match_data,
		                      NULL);
	}
	else
	{
		gsize match_flags = match_info->regex->match_flags;

		if (match_info->regex->compile_flags & PCRE2_UTF)
			match_flags |= PCRE2_NO_UTF_CHECK;

		rc = pcre2_match (match_info->regex->code,
		                  (PCRE2_SPTR)match_info->string,
		                  match_info->string_len,
		                  match_info->start_pos,
		                  match_flags,
		                  match_info->match_data,
		                  NULL);
	}

	if (set_regex_error (error, rc))
	{
		match_info->n_groups = -1;
		match_info->start_pos = match_info->string_len + 1;
		return FALSE;
	}

	if (prev_end == match_info->offsets[1])
	{
		const char *next = g_utf8_next_char (match_info->string + prev_end);

		if (match_info->start_pos > match_info->string_len)
		{
			match_info->start_pos = match_info->string_len + 1;
			match_info->n_groups = -1;
			return FALSE;
		}

		match_info->start_pos = next - match_info->string;
	}
	else
	{
		match_info->start_pos = match_info->offsets[1];
	}

	if (match_info->n_groups >= 0 &&
	    prev_begin == match_info->offsets[0] &&
	    prev_end == match_info->offsets[1])
	{
		goto again;
	}

	match_info->n_groups = rc;

	g_assert (match_info->offsets == pcre2_get_ovector_pointer (match_info->match_data));
	g_assert (impl_match_info_matches (match_info));

	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, NULL);

	return impl_match_info_matches (match_info);
}

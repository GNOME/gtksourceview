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

/* Some code in this file is based upon GRegex from GLib */
/* GRegex -- regular expression API wrapper around PCRE.
 *
 * Copyright (C) 1999, 2000 Scott Wimer
 * Copyright (C) 2004, Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005 - 2007, Marco Barisione <marco@barisione.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <glib/gi18n.h>
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
	int               matches;
	uint32_t          n_subpatterns;
	gssize            pos;
};

/* if the string is in UTF-8 use g_utf8_ functions, else use use just +/- 1. */
#define NEXT_CHAR(re, s) ((!((re)->compile_flags & PCRE2_UTF)) ? ((s) + 1) : g_utf8_next_char (s))

#define TAKE(f,gbit,pbit)            \
	G_STMT_START {               \
		if (f & gbit)        \
		{                    \
			ret |= pbit; \
			f &= ~gbit;  \
		}                    \
	} G_STMT_END

static gsize
translate_compile_flags (GRegexCompileFlags flags)
{
	gsize ret = PCRE2_UCP;

	if ((flags & G_REGEX_RAW) == 0)
	{
		ret |= (PCRE2_UTF | PCRE2_NO_UTF_CHECK);
	}
	else
	{
		flags &= ~G_REGEX_RAW;
	}

	if ((flags & G_REGEX_BSR_ANYCRLF) == 0)
	{
		ret |= PCRE2_BSR_UNICODE;
	}
	else
	{
		flags &= ~G_REGEX_BSR_ANYCRLF;
	}

	TAKE (flags, G_REGEX_ANCHORED, PCRE2_ANCHORED);
	TAKE (flags, G_REGEX_CASELESS, PCRE2_CASELESS);
	TAKE (flags, G_REGEX_EXTENDED, PCRE2_EXTENDED);
	TAKE (flags, G_REGEX_DUPNAMES, PCRE2_DUPNAMES);
	TAKE (flags, G_REGEX_MULTILINE, PCRE2_MULTILINE);
	TAKE (flags, G_REGEX_NEWLINE_ANYCRLF, PCRE2_NEWLINE_ANYCRLF);
	TAKE (flags, G_REGEX_NEWLINE_CR, PCRE2_NEWLINE_CR);
	TAKE (flags, G_REGEX_NEWLINE_LF, PCRE2_NEWLINE_LF);

	flags &= ~G_REGEX_OPTIMIZE;

	g_assert (flags == 0);

	return ret;
}

static gsize
translate_match_flags (GRegexMatchFlags flags)
{
	gsize ret = 0;

	TAKE (flags, G_REGEX_MATCH_ANCHORED, PCRE2_ANCHORED);
	TAKE (flags, G_REGEX_MATCH_NOTBOL, PCRE2_NOTBOL);
	TAKE (flags, G_REGEX_MATCH_NOTEOL, PCRE2_NOTEOL);
	TAKE (flags, G_REGEX_MATCH_PARTIAL_SOFT, PCRE2_PARTIAL_SOFT);
	TAKE (flags, G_REGEX_MATCH_PARTIAL_HARD, PCRE2_PARTIAL_HARD);
	TAKE (flags, G_REGEX_MATCH_NOTEMPTY, PCRE2_NOTEMPTY);

	g_assert (flags == 0);

	return ret;
}

static gboolean
set_regex_error (GError **error,
                 int      rc)
{
	if (rc < PCRE2_ERROR_NOMATCH && rc != PCRE2_ERROR_PARTIAL)
	{
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

	return FALSE;
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
	if (compile_options & G_REGEX_OPTIMIZE)
	{
		regex->has_jit = pcre2_jit_compile (regex->code, PCRE2_JIT_COMPLETE) == 0;
	}

#ifdef GTK_SOURCE_PROFILER_ENABLED
	if (GTK_SOURCE_PROFILER_ACTIVE)
		message = g_strdup_printf ("compile=%"G_GSIZE_MODIFIER"x match=%"G_GSIZE_MODIFIER"x pattern=%s",
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
                     gssize            string_len,
                     gssize            position)
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
	match_info->pos = MAX (0, position);
	match_info->matches = PCRE2_ERROR_NOMATCH;
	match_info->string = string;
	match_info->string_len = string_len;
	match_info->match_data = pcre2_match_data_create_from_pattern (regex->code, NULL);

	if (match_info->match_data == NULL)
		g_error ("Failed to allocate match data");

	pcre2_pattern_info (regex->code, PCRE2_INFO_CAPTURECOUNT, &match_info->n_subpatterns);

	match_info->offsets = pcre2_get_ovector_pointer (match_info->match_data);
	match_info->offsets[0] = -1;
	match_info->offsets[1] = -1;

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
		match_info->matches = 0;
		match_info->pos = 0;
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
	char *match = NULL;
	int begin =  -1;
	int end =  -1;

	g_return_val_if_fail (match_info != NULL, NULL);
	g_return_val_if_fail (match_info->string != NULL, NULL);
	g_return_val_if_fail (match_info->offsets != NULL, NULL);
	g_return_val_if_fail (impl_match_info_matches (match_info), NULL);
	g_return_val_if_fail (match_num >= 0, NULL);

	if (!impl_match_info_fetch_pos (match_info, match_num, &begin, &end))
		match = NULL;
	else if (begin == -1)
		match = g_strdup ("");
	else
		match = g_strndup (&match_info->string[begin], end - begin);

	return match;
}

char *
impl_match_info_fetch_named (const ImplMatchInfo *match_info,
                             const char          *name)
{
	int begin = -1;
	int end = -1;

	g_return_val_if_fail (match_info != NULL, NULL);

	if (impl_match_info_fetch_named_pos (match_info, name, &begin, &end))
	{
		if (begin >= 0 && end >= 0)
		{
			return g_strndup (match_info->string + begin, end - begin);
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

		/* We already matched, so ignore future matches */
		if (g_error_matches (tmp_error, G_REGEX_ERROR, G_REGEX_ERROR_MATCH))
		{
			g_clear_error (&tmp_error);
			break;
		}
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
	g_return_val_if_fail (string != NULL, FALSE);

	if (string_len < 0)
	{
		string_len = strlen (string);
	}

	local_match_info = impl_match_info_new ((ImplRegex *)regex, match_options, string, string_len, start_position);

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

enum
{
	REPL_TYPE_STRING,
	REPL_TYPE_CHARACTER,
	REPL_TYPE_SYMBOLIC_REFERENCE,
	REPL_TYPE_NUMERIC_REFERENCE,
	REPL_TYPE_CHANGE_CASE
};

typedef enum
{
	CHANGE_CASE_NONE         = 1 << 0,
	CHANGE_CASE_UPPER        = 1 << 1,
	CHANGE_CASE_LOWER        = 1 << 2,
	CHANGE_CASE_UPPER_SINGLE = 1 << 3,
	CHANGE_CASE_LOWER_SINGLE = 1 << 4,
	CHANGE_CASE_SINGLE_MASK  = CHANGE_CASE_UPPER_SINGLE | CHANGE_CASE_LOWER_SINGLE,
	CHANGE_CASE_LOWER_MASK   = CHANGE_CASE_LOWER | CHANGE_CASE_LOWER_SINGLE,
	CHANGE_CASE_UPPER_MASK   = CHANGE_CASE_UPPER | CHANGE_CASE_UPPER_SINGLE
} ChangeCase;

typedef struct _InterpolationData
{
	char      *text;
	int        type;
	int        num;
	char       c;
	ChangeCase change_case;
} InterpolationData;

static void
free_interpolation_data (InterpolationData *data)
{
	g_free (data->text);
	g_free (data);
}

static const char *
expand_escape (const char         *replacement,
               const char         *p,
               InterpolationData  *data,
               GError            **error)
{
	const char *q, *r;
	int x, d, h, i;
	const char *error_detail;
	int base = 0;
	GError *tmp_error = NULL;

	p++;
	switch (*p)
	{
		case 't':
			p++;
			data->c = '\t';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'n':
			p++;
			data->c = '\n';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'v':
			p++;
			data->c = '\v';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'r':
			p++;
			data->c = '\r';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'f':
			p++;
			data->c = '\f';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'a':
			p++;
			data->c = '\a';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'b':
			p++;
			data->c = '\b';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case '\\':
			p++;
			data->c = '\\';
			data->type = REPL_TYPE_CHARACTER;
			break;
		case 'x':
			p++;
			x = 0;
			if (*p == '{')
			{
				p++;
				do
				{
					h = g_ascii_xdigit_value (*p);
					if (h < 0)
					{
						error_detail = _("hexadecimal digit or “}” expected");
						goto error;
					}
					x = x * 16 + h;
					p++;
				}
				while (*p != '}');
				p++;
			}
			else
			{
				for (i = 0; i < 2; i++)
				{
					h = g_ascii_xdigit_value (*p);
					if (h < 0)
					{
						error_detail = _("hexadecimal digit expected");
						goto error;
					}
					x = x * 16 + h;
					p++;
				}
			}
			data->type = REPL_TYPE_STRING;
			data->text = g_new0 (gchar, 8);
			g_unichar_to_utf8 (x, data->text);
			break;
		case 'l':
			p++;
			data->type = REPL_TYPE_CHANGE_CASE;
			data->change_case = CHANGE_CASE_LOWER_SINGLE;
			break;
		case 'u':
			p++;
			data->type = REPL_TYPE_CHANGE_CASE;
			data->change_case = CHANGE_CASE_UPPER_SINGLE;
			break;
		case 'L':
			p++;
			data->type = REPL_TYPE_CHANGE_CASE;
			data->change_case = CHANGE_CASE_LOWER;
			break;
		case 'U':
			p++;
			data->type = REPL_TYPE_CHANGE_CASE;
			data->change_case = CHANGE_CASE_UPPER;
			break;
		case 'E':
			p++;
			data->type = REPL_TYPE_CHANGE_CASE;
			data->change_case = CHANGE_CASE_NONE;
			break;
		case 'g':
			p++;
			if (*p != '<')
			{
				error_detail = _("missing “<” in symbolic reference");
				goto error;
			}
			q = p + 1;
			do
			{
				p++;
				if (!*p)
				{
					error_detail = _("unfinished symbolic reference");
					goto error;
				}
			}
			while (*p != '>');
			if (p - q == 0)
			{
				error_detail = _("zero-length symbolic reference");
				goto error;
			}
			if (g_ascii_isdigit (*q))
			{
				x = 0;
				do
				{
					h = g_ascii_digit_value (*q);
					if (h < 0)
					{
						error_detail = _("digit expected");
						p = q;
						goto error;
					}
					x = x * 10 + h;
					q++;
				}
				while (q != p);
				data->num = x;
				data->type = REPL_TYPE_NUMERIC_REFERENCE;
			}
			else
			{
				r = q;
				do
				{
					if (!g_ascii_isalnum (*r))
					{
						error_detail = _("illegal symbolic reference");
						p = r;
						goto error;
					}
					r++;
				}
				while (r != p);
				data->text = g_strndup (q, p - q);
				data->type = REPL_TYPE_SYMBOLIC_REFERENCE;
			}
			p++;
			break;
		case '0':
			/* if \0 is followed by a number is an octal number representing a
			 * character, else it is a numeric reference. */
			if (g_ascii_digit_value (*g_utf8_next_char (p)) >= 0)
			{
				base = 8;
				p = g_utf8_next_char (p);
			}
			G_GNUC_FALLTHROUGH;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			x = 0;
			d = 0;
			for (i = 0; i < 3; i++)
			{
				h = g_ascii_digit_value (*p);
				if (h < 0)
					break;
				if (h > 7)
				{
					if (base == 8)
						break;
					else
						base = 10;
				}
				if (i == 2 && base == 10)
					break;
				x = x * 8 + h;
				d = d * 10 + h;
				p++;
			}
			if (base == 8 || i == 3)
			{
				data->type = REPL_TYPE_STRING;
				data->text = g_new0 (gchar, 8);
				g_unichar_to_utf8 (x, data->text);
			}
			else
			{
				data->type = REPL_TYPE_NUMERIC_REFERENCE;
				data->num = d;
			}
			break;
		case 0:
			error_detail = _("stray final “\\”");
			goto error;
			break;
		default:
			error_detail = _("unknown escape sequence");
			goto error;
	}

	return p;

error:
	/* G_GSSIZE_FORMAT doesn't work with gettext, so we use %lu */
	tmp_error = g_error_new (G_REGEX_ERROR,
				 G_REGEX_ERROR_REPLACE,
				 _("Error while parsing replacement "
				   "text “%s” at char %lu: %s"),
				 replacement,
				 (gulong)(p - replacement),
				 error_detail);
	g_propagate_error (error, tmp_error);

	return NULL;
}

static GList *
split_replacement (const gchar  *replacement,
                   GError      **error)
{
	GList *list = NULL;
	InterpolationData *data;
	const gchar *p, *start;

	start = p = replacement;
	while (*p)
	{
		if (*p == '\\')
		{
			data = g_new0 (InterpolationData, 1);
			start = p = expand_escape (replacement, p, data, error);
			if (p == NULL)
			{
				g_list_free_full (list, (GDestroyNotify) free_interpolation_data);
				free_interpolation_data (data);

				return NULL;
			}
			list = g_list_prepend (list, data);
		}
		else
		{
			p++;
			if (*p == '\\' || *p == '\0')
			{
				if (p - start > 0)
				{
					data = g_new0 (InterpolationData, 1);
					data->text = g_strndup (start, p - start);
					data->type = REPL_TYPE_STRING;
					list = g_list_prepend (list, data);
				}
			}
		}
	}

	return g_list_reverse (list);
}

/* Change the case of c based on change_case. */
#define CHANGE_CASE(c, change_case) \
        (((change_case) & CHANGE_CASE_LOWER_MASK) ? \
                g_unichar_tolower (c) : \
                g_unichar_toupper (c))

static void
string_append (GString     *string,
               const gchar *text,
               ChangeCase  *change_case)
{
	gunichar c;

	if (text[0] == '\0')
		return;

	if (*change_case == CHANGE_CASE_NONE)
	{
		g_string_append (string, text);
	}
	else if (*change_case & CHANGE_CASE_SINGLE_MASK)
	{
		c = g_utf8_get_char (text);
		g_string_append_unichar (string, CHANGE_CASE (c, *change_case));
		g_string_append (string, g_utf8_next_char (text));
		*change_case = CHANGE_CASE_NONE;
	}
	else
	{
		while (*text != '\0')
		{
			c = g_utf8_get_char (text);
			g_string_append_unichar (string, CHANGE_CASE (c, *change_case));
			text = g_utf8_next_char (text);
		}
	}
}

static gboolean
interpolate_replacement (const ImplMatchInfo *match_info,
                         GString             *result,
                         gpointer             data)
{
	GList *list;
	InterpolationData *idata;
	gchar *match;
	ChangeCase change_case = CHANGE_CASE_NONE;

	for (list = data; list; list = list->next)
	{
		idata = list->data;
		switch (idata->type)
		{
			case REPL_TYPE_STRING:
				string_append (result, idata->text, &change_case);
				break;
			case REPL_TYPE_CHARACTER:
				g_string_append_c (result, CHANGE_CASE (idata->c, change_case));
				if (change_case & CHANGE_CASE_SINGLE_MASK)
					change_case = CHANGE_CASE_NONE;
				break;
			case REPL_TYPE_NUMERIC_REFERENCE:
				match = impl_match_info_fetch (match_info, idata->num);
				if (match)
				{
					string_append (result, match, &change_case);
					g_free (match);
				}
				break;
			case REPL_TYPE_SYMBOLIC_REFERENCE:
				match = impl_match_info_fetch_named (match_info, idata->text);
				if (match)
				{
					string_append (result, match, &change_case);
					g_free (match);
				}
				break;
			case REPL_TYPE_CHANGE_CASE:
				change_case = idata->change_case;
				break;
			default:
				g_warn_if_reached ();
				break;
		}
	}

	return FALSE;
}

char *
impl_regex_replace (const ImplRegex   *regex,
                    const char        *string,
                    gssize             string_len,
                    int                start_position,
                    const char        *replacement,
                    GRegexMatchFlags   match_options,
                    GError           **error)
{
	char *result;
	GList *list;
	GError *tmp_error = NULL;

	g_return_val_if_fail (regex != NULL, NULL);
	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (start_position >= 0, NULL);
	g_return_val_if_fail (replacement != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	list = split_replacement (replacement, &tmp_error);

	if (tmp_error != NULL)
	{
		g_propagate_error (error, tmp_error);
		return NULL;
	}

	result = impl_regex_replace_eval (regex,
	                                  string, string_len, start_position,
	                                  match_options,
	                                  interpolate_replacement,
	                                  (gpointer)list,
	                                  &tmp_error);

	if (tmp_error != NULL)
		g_propagate_error (error, tmp_error);

	g_list_free_full (list, (GDestroyNotify) free_interpolation_data);

	return result;
}

gboolean
impl_match_info_fetch_pos (const ImplMatchInfo *match_info,
                           int                  match_num,
                           int                 *start_pos,
                           int                 *end_pos)
{
	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (match_info->match_data != NULL, FALSE);
	g_return_val_if_fail (match_info->offsets != NULL, FALSE);
	g_return_val_if_fail (match_num >= 0, FALSE);

	if (match_info->matches < 0)
		return FALSE;

	/* make sure the sub expression number they're requesting is less than
	 * the total number of sub expressions in the regex. When matching all
	 * (g_regex_match_all()), also compare against the number of matches */
	if (match_num >= MAX (match_info->matches, match_info->n_subpatterns + 1))
		return FALSE;

	if (start_pos)
		*start_pos = (match_num < match_info->matches) ? match_info->offsets[2 * match_num] : -1;

	if (end_pos)
		*end_pos = (match_num < match_info->matches) ? match_info->offsets[2 * match_num + 1] : -1;

	return TRUE;
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
	g_return_val_if_fail (match_info->matches != 0, FALSE);

	return match_info->matches >= 0;
}

gboolean
impl_match_info_next (ImplMatchInfo  *match_info,
                      GError        **error)
{
	gssize prev_match_start;
	gssize prev_match_end;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	g_return_val_if_fail (match_info != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (match_info->pos >= 0, FALSE);

	prev_match_start = match_info->offsets[0];
	prev_match_end = match_info->offsets[1];

	if (match_info->pos > match_info->string_len)
	{
		/* we have reached the end of the string */
		match_info->pos = -1;
		match_info->matches = PCRE2_ERROR_NOMATCH;
		return FALSE;
	}

	if (match_info->regex->has_jit)
	{
		match_info->matches = pcre2_jit_match (match_info->regex->code,
		                                       (PCRE2_SPTR)match_info->string,
		                                       match_info->string_len,
		                                       match_info->pos,
		                                       match_info->match_flags,
		                                       match_info->match_data,
		                                       NULL);
	}
	else
	{
		gsize match_flags = match_info->regex->match_flags | match_info->match_flags;

		if (match_info->regex->compile_flags & PCRE2_UTF)
			match_flags |= PCRE2_NO_UTF_CHECK;

		match_info->matches = pcre2_match (match_info->regex->code,
		                                   (PCRE2_SPTR)match_info->string,
		                                   match_info->string_len,
		                                   match_info->pos,
		                                   match_flags,
		                                   match_info->match_data,
		                                   NULL);
	}

	if (set_regex_error (error, match_info->matches))
		return FALSE;

	/* avoid infinite loops if the pattern is an empty string or something
	 * equivalent */
	if (match_info->pos == match_info->offsets[1])
	{
		if (match_info->pos > match_info->string_len)
		{
			/* we have reached the end of the string */
			match_info->pos = -1;
			match_info->matches = PCRE2_ERROR_NOMATCH;
			return FALSE;
		}

		match_info->pos = NEXT_CHAR (match_info->regex, &match_info->string[match_info->pos]) -
		                  match_info->string;


	}
	else
	{
		match_info->pos = match_info->offsets[1];
	}

	g_assert (match_info->matches <= (int)match_info->n_subpatterns + 1);

	/* it's possible to get two identical matches when we are matching
	 * empty strings, for instance if the pattern is "(?=[A-Z0-9])" and
	 * the string is "RegExTest" we have:
	 *  - search at position 0: match from 0 to 0
	 *  - search at position 1: match from 3 to 3
	 *  - search at position 3: match from 3 to 3 (duplicate)
	 *  - search at position 4: match from 5 to 5
	 *  - search at position 5: match from 5 to 5 (duplicate)
	 *  - search at position 6: no match -> stop
	 * so we have to ignore the duplicates.
	 * see bug #515944: http://bugzilla.gnome.org/show_bug.cgi?id=515944 */
	if (match_info->matches >= 0 &&
	    prev_match_start == match_info->offsets[0] &&
	    prev_match_end == match_info->offsets[1])
	{
		/* ignore this match and search the next one */
		return impl_match_info_next (match_info, error);
	}

	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, NULL);

	return match_info->matches >= 0;
}

int
impl_regex_get_max_lookbehind (const ImplRegex *regex)
{
	uint32_t value = 0;

	g_return_val_if_fail (regex != NULL, 0);
	g_return_val_if_fail (regex->code != NULL, 0);

	pcre2_pattern_info (regex->code, PCRE2_INFO_MAXLOOKBEHIND, &value);

	return value;
}

gboolean
impl_match_info_is_partial_match (const ImplMatchInfo *match_info)
{
	g_return_val_if_fail (match_info != NULL, FALSE);

	return match_info->matches == PCRE2_ERROR_PARTIAL;
}

int
impl_match_info_get_match_count (const ImplMatchInfo *match_info)
{
	g_return_val_if_fail (match_info != NULL, 0);

	return MAX (0, match_info->matches);
}

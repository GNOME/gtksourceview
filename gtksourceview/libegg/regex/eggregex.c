/* EggRegex -- regular expression API wrapper around PCRE.
 * Copyright (C) 1999, 2000 Scott Wimer
 * Copyright (C) 2004 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005 Marco Barisione <barisione@gmail.com>
 *
 * This is basically an ease of user wrapper around the functionality of
 * PCRE.
 *
 * With this library, we are, hopefully, drastically reducing the code
 * complexity necessary by making use of a more complex and detailed
 * data structure to store the regex info.  I am hoping to have a regex
 * interface that is almost as easy to use as Perl's.  <fingers crossed>
 *
 * Author: Scott Wimer <scottw@cylant.com>
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * This library is free software, you can distribute it or modify it
 * under any of the following terms:
 *  1) The GNU General Public License (GPL)
 *  2) The GNU Library General Public License (LGPL)
 *  3) The Perl Artistic license (Artistic)
 *  4) The BSD license (BSD)
 *
 * In short, you can use this library in any code you desire, so long as
 * the Copyright notice above remains intact.  If you do make changes to
 * it, I would appreciate that you let me know so I can improve this 
 * library for everybody, but I'm not gonna force you to.
 * 
 * Please note that this library is just a wrapper around Philip Hazel's
 * PCRE library.  Please see the file 'LICENSE' in your PCRE distribution.
 * And, if you live in England, please send him a pint of good beer, his
 * library is great.
 *
 */

#include "config.h"
#include <glib.h>
#include <string.h>
#include "eggregex.h"
#include "pcre/pcre.h"

/* FIXME when this is in glib */
#define _(s) s

/* FIXME move this to the sgml file:
<!-- ##### USER_FUNCTION EggRegexEvalCallback ##### -->
<para>
Specifies the type of the function passed to egg_regex_replace_eval().
It is called for each occurance of the pattern @regex in @string, and it
should append the replacement to @result.
</para>

<para>
Do not call on @regex functions that modify its internal state, such as
egg_regex_match(); if you need it you can create a temporary copy of
@regex using egg_regex_copy().
</para>

@regex: a #EggRegex.
@string: the string used to perform matches against.
@result: a #GString containing the new string.
@user_data: user data passed to egg_regex_replace_eval().
FIXME: We could return the replacement string or NULL to stop the process.
*/

struct _EggRegex
{
  gchar *pattern;       /* the pattern */
  pcre *regex;		/* compiled form of the pattern */
  pcre_extra *extra;	/* data stored when egg_regex_optimize() is used */
  gint matches;		/* number of matching sub patterns */
  gint pos;		/* position in the string where last match left off */
  gint *offsets;	/* array of offsets paired 0,1 ; 2,3 ; 3,4 etc */
  gint n_offsets;	/* number of offsets */
  EggRegexCompileFlags compile_opts;	/* options used at compile time on the pattern */
  EggRegexMatchFlags match_opts;	/* options used at match time on the regex */
  gssize string_len;	/* length of the string last used against */
  gint start_position;	/* starting index in the string */
  GSList *delims;	/* delimiter sub strings from split next */
};

/* converts an offset to an index */
#define OFFSET_TO_INDEX(string, offset) \
	g_utf8_offset_to_pointer ((string), (offset)) - (string)

/* converts a length in characters to a length in bytes, using strlen()
 * if len < 0 */
#define LEN_OFFSET_TO_INDEX(string, len) \
	((len) < 0 ? strlen ((string)) : OFFSET_TO_INDEX (string, len))

GQuark
egg_regex_error_quark (void)
{
  static GQuark error_quark = 0;

  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("g-regex-error-quark");

  return error_quark;
}

static EggRegex *
regex_new (pcre                *re,
	   const gchar         *pattern, 
	   EggRegexCompileFlags compile_options,
	   EggRegexMatchFlags   match_options)
{
  /* function used internally by egg_regex_new() and egg_regex_copy()
   * to create a new EggRegex from a pcre structure */
  EggRegex *regex = g_new0 (EggRegex, 1);
  gint capture_count;
  
  regex->regex = re;
  regex->pattern = g_strdup (pattern);
  regex->extra = NULL;
  regex->pos = 0;
  regex->string_len = -1;	/* not set yet */
  regex->start_position = -1;	/* not set yet */

  /* set the options */
  regex->compile_opts = compile_options | PCRE_UTF8 | PCRE_NO_UTF8_CHECK;
  regex->match_opts = match_options | PCRE_NO_UTF8_CHECK;

  /* find out how many sub patterns exist in this pattern, and
   * setup the offsets array and n_offsets accordingly */
  _pcre_fullinfo (regex->regex, regex->extra, 
		  PCRE_INFO_CAPTURECOUNT, &capture_count);
  regex->n_offsets = (capture_count + 1) * 3;
  regex->offsets = g_new0 (gint, regex->n_offsets);

  return regex;
}

/** 
 * egg_regex_new:
 * @pattern: the regular expression.
 * @compile_options: compile options for the regular expression.
 * @match_options: match options for the regular expression.
 * @error: return location for a #GError.
 * 
 * Compiles the regular expression to an internal form, and does the initial
 * setup of the #EggRegex structure.  
 * 
 * Returns: a #EggRegex structure.
 */
EggRegex *
egg_regex_new (const gchar         *pattern, 
 	     EggRegexCompileFlags   compile_options,
	     EggRegexMatchFlags     match_options,
	     GError             **error)
{
  pcre *re;
  const gchar *errmsg;
  gint erroffset;
  
  g_return_val_if_fail (pattern != NULL, NULL);

  compile_options = compile_options | PCRE_UTF8 | PCRE_NO_UTF8_CHECK;
  match_options = match_options | PCRE_NO_UTF8_CHECK;

  /* compile the pattern */
  re = _pcre_compile (pattern, compile_options, &errmsg, &erroffset, NULL);

  /* if the compilation failed, set the error member and return 
   * immediately */
  if (re == NULL)
    {
      GError *tmp_error = g_error_new (EGG_REGEX_ERROR, 
				       EGG_REGEX_ERROR_COMPILE,
				       _("Error while compiling regular "
					 "expression %s at char %d: %s"),
				       pattern, erroffset, errmsg);
      g_propagate_error (error, tmp_error);

      return NULL;
    }
  else
    return regex_new (re, pattern, compile_options, match_options);
}

/**
 * egg_regex_free:
 * @regex: a #EggRegex structure from egg_regex_new().
 *
 * Frees all the memory associated with the regex structure.
 */
void
egg_regex_free (EggRegex *regex)
{
  if (regex == NULL)
    return;

  g_free (regex->pattern);
  g_slist_free (regex->delims);
  g_free (regex->offsets);
  if (regex->regex != NULL)
    g_free (regex->regex);
  if (regex->extra != NULL)
    g_free (regex->extra);
  g_free (regex);
}

/**
 * egg_regex_copy:
 * @regex: a #EggRegex structure from egg_regex_new().
 *
 * Copies a #EggRegex.
 *
 * Returns: a newly allocated copy of @regex, or %NULL if an error
 *          occurred.
 */
EggRegex *
egg_regex_copy (const EggRegex *regex)
{
  EggRegex *copy;
  gint res;
  gint size;
  pcre *re;

  g_return_val_if_fail (regex != NULL, NULL);

  res = _pcre_fullinfo (regex->regex, NULL, PCRE_INFO_SIZE, &size);
  g_return_val_if_fail (res >= 0, NULL);
  re = g_malloc (size);
  memcpy (re, regex->regex, size);
  copy = regex_new (re, regex->pattern, regex->compile_opts, regex->match_opts);

  if (regex->extra != NULL)
    {
      res = _pcre_fullinfo (regex->regex, regex->extra,
		      	    PCRE_INFO_STUDYSIZE, &size);
      g_return_val_if_fail (res >= 0, copy);
      copy->extra = g_new0 (pcre_extra, 1);
      copy->extra->flags = PCRE_EXTRA_STUDY_DATA;
      copy->extra->study_data = g_malloc (size);
      memcpy (copy->extra->study_data, regex->extra->study_data, size);
    }

  return copy;
}

/**
 * egg_regex_equal:
 * @v: a #EggRegex.
 * @v2: another #EggRegex.
 *
 * Compares two regular expressions for equality, returning TRUE if they are
 * equal. For use with #GHashTable. 
 *
 * Returns: TRUE if the regular expressions have the same patterns and the
 *          same options.
 */
gboolean
egg_regex_equal (const EggRegex *v,
	       const EggRegex *v2)
{
  g_return_val_if_fail (v != NULL, FALSE);
  g_return_val_if_fail (v2 != NULL, FALSE);

  if (v == v2)
  {
    return TRUE;
  }
  else if (v->compile_opts == v2->compile_opts &&
           v->match_opts == v2->match_opts &&
           strcmp (v->pattern, v2->pattern) == 0)
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

/**
 * egg_regex_get_pattern:
 * @regex: a #EggRegex structure.
 *
 * Gets the pattern string associated with @regex, i.e. a copy of the string passed
 * to egg_regex_new().
 *
 * Returns: the pattern of @regex.
 */
const gchar *
egg_regex_get_pattern (const EggRegex *regex)
{
  g_return_val_if_fail (regex != NULL, NULL);

  return regex->pattern;
}

/**
 * egg_regex_clear:
 * @regex: a #EggRegex structure.
 *
 * Clears out the members of @regex that are holding information about the
 * last set of matches for this pattern.  egg_regex_clear() needs to be
 * called between uses of egg_regex_match() or egg_regex_match_next() against
 * new target strings. 
 */
void
egg_regex_clear (EggRegex *regex)
{
  g_return_if_fail (regex != NULL);

  regex->matches = -1;
  regex->string_len = -1;
  regex->start_position = -1;
  regex->pos = 0;

  /* if the pattern was used with egg_regex_split_next(), it may have
   * delimiter offsets stored.  Free up those guys as well. */
  if (regex->delims != NULL)
    g_slist_free (regex->delims);
}

/**
 * egg_regex_optimize:
 * @regex: a #EggRegex structure.
 * @error: return location for a #GError.
 *
 * If the pattern will be used many times, then it may be worth the
 * effort to optimize it to improve the speed of matches.
 */
void
egg_regex_optimize (EggRegex  *regex,
		  GError **error)
{
  const gchar *errmsg;

  g_return_if_fail (regex != NULL);

  if (regex->extra != NULL)
    return;

  regex->extra = _pcre_study (regex->regex, 0, &errmsg);

  if (errmsg)
    {
      GError *tmp_error = g_error_new (EGG_REGEX_ERROR,
				       EGG_REGEX_ERROR_OPTIMIZE, 
				       _("Error while optimizing "
					 "regular expression %s: %s"),
				       regex->pattern,
				       errmsg);
      g_propagate_error (error, tmp_error);
    }
}

/**
 * egg_regex_match:
 * @regex: a #EggRegex structure from egg_regex_new().
 * @string: the string to scan for matches.
 * @match_options:  match options.
 *
 * Scans for a match in string for the pattern in @regex. The @match_options
 * are combined with the match options specified when the @regex structure
 * was created, letting you have more flexibility in reusing #EggRegex
 * structures.
 *
 * Returns: %TRUE is the string matched, %FALSE otherwise.
 */
gboolean
egg_regex_match (EggRegex          *regex, 
	       const gchar     *string, 
	       EggRegexMatchFlags match_options)
{
  return egg_regex_match_extended (regex, string, -1, 0,
				   match_options, NULL);
}

/**
 * egg_regex_match_extended:
 * @regex: a #EggRegex structure from egg_regex_new().
 * @string: the string to scan for matches.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @match_options:  match options.
 * @error: location to store the error occuring, or NULL to ignore errors.
 *
 * Scans for a match in string for the pattern in @regex. The @match_options
 * are combined with the match options specified when the @regex structure
 * was created, letting you have more flexibility in reusing #EggRegex
 * structures.
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: %TRUE is the string matched, %FALSE otherwise.
 */
gboolean
egg_regex_match_extended (EggRegex          *regex,
		        const gchar       *string,
			gssize             string_len,
			gint               start_position,
			EggRegexMatchFlags match_options,
			GError           **error)
{
  g_return_val_if_fail (regex != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (start_position >= 0, FALSE);

  string_len = LEN_OFFSET_TO_INDEX (string, string_len);
  start_position = OFFSET_TO_INDEX (string, start_position);

  regex->string_len = string_len;
  regex->start_position = start_position;

  /* perform the match */
  regex->matches = _pcre_exec (regex->regex, regex->extra, 
			       string, regex->string_len,
			       regex->start_position,
			       regex->match_opts | match_options,
			       regex->offsets, regex->n_offsets);
  if (regex->matches < PCRE_ERROR_NOMATCH)
  {
    g_set_error (error, EGG_REGEX_ERROR, EGG_REGEX_ERROR_MATCH,
		 _("Error while matching regular expression %s"),
		 regex->pattern);
    return FALSE;
  }

  /* if the regex matched, set regex->pos to the character past the 
   * end of the match.
   */
  if (regex->matches > 0)
    regex->pos = regex->offsets[1];

  return regex->matches >= 0;
}

/**
 * egg_regex_match_next:
 * @regex: a #EggRegex structure.
 * @string: the string to scan for matches.
 * @match_options: the match options.
 *
 * Scans for the next match in @string of the pattern in @regex.
 * array.  The match options are combined with the match options set when
 * the @regex was created.
 *
 * You have to call egg_regex_clear() to reuse the same pattern on a new
 * string. This is especially true for use with egg_regex_match_next().
 *
 * Returns: %TRUE is the string matched, %FALSE otherwise.
 */
gboolean
egg_regex_match_next (EggRegex          *regex, 
		    const gchar     *string, 
		    EggRegexMatchFlags match_options)
{
  return egg_regex_match_next_extended (regex, string, -1, 0,
					match_options, NULL);
}

/**
 * egg_regex_match_next_extended:
 * @regex: a #EggRegex structure.
 * @string: the string to scan for matches.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @match_options: the match options.
 * @error: location to store the error occuring, or NULL to ignore errors.
 *
 * Scans for the next match in @string of the pattern in @regex.
 * array.  The match options are combined with the match options set when
 * the @regex was created.
 *
 * You have to call egg_regex_clear() to reuse the same pattern on a new
 * string. This is especially true for use with
 * egg_regex_match_next_extended().
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: %TRUE is the string matched, %FALSE otherwise.
 */
gboolean
egg_regex_match_next_extended (EggRegex          *regex,
			     const gchar       *string,
			     gssize             string_len,
			     gint               start_position,
			     EggRegexMatchFlags match_options,
			     GError           **error)
{
  g_return_val_if_fail (regex != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (start_position >= 0, FALSE);

  /* if this regex hasn't been used on this string before, then we
   * need to calculate the length of the string, and set pos to the
   * start of it.  
   * Knowing if this regex has been used on this string is a bit of
   * a challenge.  For now, we require the user to call egg_regex_clear()
   * in between usages on a new string.  Not perfect, but not such a
   * bad solution either.
   */
  if (regex->string_len == -1)
    {
      string_len = LEN_OFFSET_TO_INDEX (string, string_len);
      regex->string_len = string_len;

      start_position = OFFSET_TO_INDEX (string, start_position);
      regex->start_position = start_position;
    }


  /* perform the match */
  regex->matches = _pcre_exec (regex->regex, regex->extra,
			       string, regex->string_len,
			       regex->start_position + regex->pos,
			       regex->match_opts | match_options,
			       regex->offsets, regex->n_offsets);

  /* avoid infinite loops if regex is an empty string or something
   * equivalent */
  if (regex->pos == regex->offsets[1])
    {
      regex->pos++;
      if (regex->pos > regex->string_len)
        /* we have reached the end of the string */
        return FALSE;
    }
  else
    {
      regex->pos = regex->offsets[1];
    }

  return regex->matches >= 0;
}

/**
 * egg_regex_get_match_count:
 * @regex: a #EggRegex structure.
 *
 * Returns:  Number of matched substrings + 1 in the last call to
 *           egg_regex_match*(), or 1 if the pattern has no
 *           substrings in it. Returns -1 if the pattern did not
 *           match.
 */
gint
egg_regex_get_match_count (const EggRegex *regex)
{
  g_return_val_if_fail (regex != NULL, -1);

  return regex->matches;
}

/**
 * egg_regex_fetch:
 * @regex: #EggRegex structure used in last match.
 * @string: the string on which the last match was made.
 * @match_num: number of the sub expression.
 *
 * Retrieves the text matching the @match_num<!-- -->'th capturing parentheses.
 * 0 is the full text of the match, 1 is the first paren set, 2 the second,
 * and so on.
 *
 * Returns: The matched substring, or %NULL if an error occurred.
 *          You have to free the string yourself.
 */
gchar *
egg_regex_fetch (const EggRegex *regex,
	       const gchar *string,
	       gint         match_num)
{
  gchar *match = NULL;

  g_return_val_if_fail (regex != NULL, NULL);
  g_return_val_if_fail (match_num >= 0, NULL);
  g_return_val_if_fail (regex->start_position >= 0, NULL);

  /* make sure the sub expression number they're requesting is less than
   * the total number of sub expressions that were matched. */
  if (match_num >= regex->matches)
    return NULL;

  _pcre_get_substring (string, regex->offsets, regex->matches, match_num,
                       (const char **)&match);

  return match;
}

/**
 * egg_regex_fetch_pos:
 * @regex: #EggRegex structure used in last match.
 * @string: the string on which the last match was made.
 * @match_num: number of the sub expression.
 * @start_pos: pointer to location where to store the start position.
 * @end_pos: pointer to location where to store the end position.
 *
 * Retrieves the position of the @match_num<!-- -->'th capturing parentheses.
 * 0 is the full text of the match, 1 is the first paren set, 2 the second,
 * and so on.
 *
 * Returns: %TRUE if the position was fetched, %FALSE otherwise. If the
 *          position cannot be fetched, @start_pos and @end_pos are left
 *          unchanged.
 */
gboolean
egg_regex_fetch_pos (const EggRegex    *regex,
		   const gchar *string,
		   gint         match_num,
		   gint        *start_pos,
		   gint        *end_pos)
{
  g_return_val_if_fail (regex != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (match_num >= 0, FALSE);

  /* make sure the sub expression number they're requesting is less than
   * the total number of sub expressions that were matched. */
  if (match_num >= regex->matches)
    return FALSE;

  if (start_pos != NULL)
    {
      *start_pos = regex->offsets[2 * match_num];
      *start_pos = g_utf8_pointer_to_offset (string, &string[*start_pos]);
    }

  if (end_pos != NULL)
    {
      *end_pos = regex->offsets[2 * match_num + 1];
      *end_pos = g_utf8_pointer_to_offset (string, &string[*end_pos]);
    }

  return TRUE;
}

/**
 * egg_regex_fetch_named:
 * @regex: #EggRegex structure used in last match.
 * @string: the string on which the last match was made.
 * @name: name of the subexpression.
 *
 * Retrieves the text matching the capturing parentheses named @name.
 *
 * Returns: The matched substring, or %NULL if an error occurred.
 *          You have to free the string yourself.
 */
gchar *
egg_regex_fetch_named (const EggRegex *regex,
		     const gchar  *string,
		     const gchar  *name)
{
  gchar *match = NULL;

  g_return_val_if_fail (regex != NULL, NULL);
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  _pcre_get_named_substring (regex->regex,
			     string, regex->offsets, regex->matches,
			     name, (const char **)&match);

  return match;
}

/**
 * egg_regex_fetch_named_pos:
 * @regex: #EggRegex structure used in last match.
 * @string: the string on which the last match was made.
 * @name: name of the subexpression.
 * @start_pos: pointer to location where to store the start position.
 * @end_pos: pointer to location where to store the end position.
 *
 * Retrieves the position of the capturing parentheses named @name.
 *
 * Returns: %TRUE if the position was fetched, %FALSE otherwise. If the
 *          position cannot be fetched, @start_pos and @end_pos are left
 *          unchanged.
 */
gboolean
egg_regex_fetch_named_pos (const EggRegex *regex,
			 const gchar  *string,
			 const gchar  *name,
			 gint         *start_pos,
			 gint         *end_pos)
{
  gint num;

  num = egg_regex_expression_number_from_name (regex, name);
  if (num == -1)
    return FALSE;

  return egg_regex_fetch_pos (regex, string, num, start_pos, end_pos);
}

/**
 * egg_regex_fetch_all:
 * @regex: a #EggRegex structure.
 * @string: the string on which the last match was made.
 *
 * Bundles up pointers to each of the matching substrings from a match
 * and stores then in an array of gchar pointers.
 *
 * Returns: a %NULL-terminated array of gchar * pointers. It must be freed
 *          using g_strfreev(). If the memory can't be allocated, returns
 *          %NULL.
 */
gchar **
egg_regex_fetch_all (const EggRegex *regex,
		   const gchar  *string)
{
  gchar **listptr = NULL; /* the list pcre_get_substring_list() will fill */
  gchar **result;

  g_return_val_if_fail (regex != NULL, FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  if (regex->matches < 0)
    return NULL;
  
  _pcre_get_substring_list (string, regex->offsets, 
			    regex->matches, (const char ***)&listptr);

  if (listptr)
    {
      /* PCRE returns a single block of memory which
       * isn't suitable for g_strfreev().
       */
      result = g_strdupv (listptr);
      g_free (listptr);
    }
  else 
    result = NULL;

  return result;
}

/**
 * egg_regex_expression_number_from_name:
 * @regex: #EggRegex structure.
 * @name: name of the subexpression.
 *
 * Retrieves the number of the subexpression named @name.
 *
 * Returns: The number of the subexpression or -1 if @name does not exists.
 */
gint
egg_regex_expression_number_from_name (const EggRegex *regex,
				     const gchar  *name)
{
  gint num;

  g_return_val_if_fail (regex != NULL, -1);
  g_return_val_if_fail (name != NULL, -1);

  num = _pcre_get_stringnumber (regex->regex, name);
  if (num == PCRE_ERROR_NOSUBSTRING)
	  num = -1;

  return num;
}

/**
 * egg_regex_split:
 * @regex:  a #EggRegex structure.
 * @string:  the string to split with the pattern.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @match_options:  match time option flags.
 * @max_pieces:  maximum number of pieces to split the string into,
 *    or 0 for no limit.
 *
 * Breaks the string on the pattern, and returns an array of the pieces.
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: a %NULL-terminated gchar ** array. Free it using g_strfreev().
 **/
gchar **
egg_regex_split (EggRegex           *regex, 
	       const gchar      *string, 
	       gssize            string_len,
	       EggRegexMatchFlags  match_options,
	       gint              max_pieces)
{
  /* FIXME: add a start_position argument */
  gchar **string_list;		/* The array of char **s worked on */
  gint pos;
  gboolean match_ok;
  gint match_count;
  gint pieces;
  gint new_pos;
  gchar *piece;
  GList *list, *last;

  g_return_val_if_fail (regex != NULL, NULL);
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (max_pieces >= 0, NULL);

  new_pos = 0;
  pieces = 0;
  list = NULL;
  while (TRUE)
    {
      match_ok = egg_regex_match_next_extended (regex, string, string_len, 0,
                                              match_options, NULL);
      if (match_ok && ((max_pieces == 0) || (pieces < max_pieces)))
	{
	  piece = g_strndup (string + new_pos, regex->offsets[0] - new_pos);
	  list = g_list_prepend (list, piece);

	  /* if there were substrings, these need to get added to the
	   * list as well */
	  match_count = egg_regex_get_match_count (regex);
	  if (match_count > 1)
	    {
	      int i;
	      for (i = 1; i < match_count; i++)
		list = g_list_prepend (list, egg_regex_fetch (regex, string, i));
	    }

	  new_pos = regex->pos;	/* move new_pos to end of match */
	  pieces++;
	}
      else	 /* if there was no match, copy to end of string, and break */
	{
	  piece = g_strndup (string + new_pos, regex->string_len - new_pos);
	  list = g_list_prepend (list, piece);
	  break;
	}
    }

  string_list = (gchar **) g_malloc (sizeof (gchar *) * (g_list_length (list) + 1));
  pos = 0;
  for (last = g_list_last (list); last; last = last->prev)
    string_list[pos++] = last->data;
  string_list[pos] = 0;

  g_list_free (list);
  return string_list;
}

/**
 * egg_regex_split_next:
 * @pattern:  gchar pointer to the pattern.
 * @string:  the string to split on pattern.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @match_options:  match time options for the regex.
 *
 * egg_regex_split_next() breaks the string on pattern, and returns the
 * pieces, one per call.  If the pattern contains capturing parentheses,
 * then the text for each of the substrings will also be returned.
 * If the pattern does not match anywhere in the string, then the whole
 * string is returned as the first piece.
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns:  a gchar * to the next piece of the string.
 */
gchar *
egg_regex_split_next (EggRegex          *regex,
		    const gchar     *string,
		    gssize           string_len,
		    EggRegexMatchFlags match_options)
{
  /* FIXME: add a start_position argument */
  gint new_pos = regex->pos;
  gchar *piece = NULL;
  gboolean match_ok;
  gint match_count;

  g_return_val_if_fail (regex != NULL, NULL);
  g_return_val_if_fail (string != NULL, NULL);

  /* if there are delimiter substrings stored, return those one at a
   * time.  
   */
  if (regex->delims != NULL)
    {
      piece = regex->delims->data;
      regex->delims = g_slist_remove (regex->delims, piece);
      return piece;
    }

  /* otherwise...
   * use egg_regex_match_next() to find the next occurance of the pattern
   * in the string.  We use new_pos to keep track of where the stuff
   * up to the current match starts.  Copy that piece of the string off
   * and append it to the buffer using strncpy.  We have to NUL term the
   * piece we copied off before returning it.
   */
  match_ok = egg_regex_match_next_extended (regex, string, string_len,
                                          0, match_options,
                                          NULL);
  if (match_ok)
    {
      piece = g_strndup (string + new_pos, regex->offsets[0] - new_pos);

      /* if there were substrings, these need to get added to the
       * list of delims */
      match_count = egg_regex_get_match_count (regex);
      if (match_count > 1)
	{
	  gint i;
	  for (i = 1; i < match_count; i++)
	    regex->delims = g_slist_append (regex->delims,
					    egg_regex_fetch (regex, string, i));
	}
    }
  else		/* if there was no match, copy to end of string */
    piece = g_strndup (string + new_pos, regex->string_len - new_pos);

  return piece;
}

enum
{
  REPL_TYPE_STRING,
  REPL_TYPE_CHARACTER,
  REPL_TYPE_SYMBOLIC_REFERENCE,
  REPL_TYPE_NUMERIC_REFERENCE
}; 

typedef struct 
{
  gchar *text;   
  gint   type;   
  gint   num;
  gchar  c;
} InterpolationData;

static void
free_interpolation_data (InterpolationData *data)
{
  g_free (data->text);
  g_free (data);
}

static const gchar *
expand_escape (const gchar        *replacement,
	       const gchar        *p, 
	       InterpolationData  *data,
	       GError            **error)
{
  const gchar *q, *r;
  gint x, d, h, i;
  gchar *error_detail;
  gint base = 0;
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
		  error_detail = _("hexadecimal digit or '}' expected");
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
    case 'u':
    case 'L':
    case 'U':
    case 'E':
    case 'Q':
    case 'G':
      error_detail = _("escape sequence not allowed");
      goto error;
    case 'g':
      p++;
      if (*p != '<')
	{
	  error_detail = _("missing '<' in symbolic reference");
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
      error_detail = _("stray final '\\'");
      goto error;
      break;
    default:
      data->type = REPL_TYPE_STRING;
      data->text = g_new0 (gchar, 8);
      g_unichar_to_utf8 (g_utf8_get_char (p), data->text);
      p = g_utf8_next_char (p);
    }

  return p;

 error:
  tmp_error = g_error_new (EGG_REGEX_ERROR, 
			   EGG_REGEX_ERROR_REPLACE,
			   _("Error while parsing replacement "
			     "text \"%s\" at char %d: %s"),
			   replacement, 
			   p - replacement,
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
	      g_list_foreach (list, (GFunc)free_interpolation_data, NULL);
	      g_list_free (list);

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

static gboolean
interpolate_replacement (const EggRegex *regex,
			 const gchar  *string,
			 GString      *result,
			 gpointer      data)
{
  GList *list;
  InterpolationData *idata;
  gchar *match;

  for (list = data; list; list = list->next)
    {
      idata = list->data;
      switch (idata->type)
	{
	case REPL_TYPE_STRING:
	  g_string_append (result, idata->text);
	  break;
	case REPL_TYPE_CHARACTER:
	  g_string_append_c (result, idata->c);
	  break;
	case REPL_TYPE_NUMERIC_REFERENCE:
	  match = egg_regex_fetch (regex, string, idata->num);
	  if (match) 
	    {
	      g_string_append (result, match);
	      g_free (match);
	    }
	  break;
	case REPL_TYPE_SYMBOLIC_REFERENCE:
	  match = egg_regex_fetch_named (regex, string, idata->text);
	  if (match) 
	    {
	      g_string_append (result, match);
	      g_free (match);
	    }
	  break;
	}
    }

  return FALSE; 
}

/**
 * egg_regex_replace:
 * @regex:  a #EggRegex structure.
 * @string:  the string to perform matches against.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @replacement:  text to replace each match with.
 * @match_options:  options for the match.
 * @error: location to store the error occuring, or NULL to ignore errors.
 *
 * Replaces all occurances of the pattern in @regex with the
 * replacement text. Backreferences of the form '\number' or '\g<number>'
 * in the replacement text are interpolated by the number-th captured
 * subexpression of the match, '\g<name>' refers to the captured subexpression
 * with the given name. '\0' refers to the complete match, but '\0' followed
 * by a number is the octal representation of a character. To include a
 * literal '\' in the replacement, write '\\'. If you do not need to use
 * backreferences use egg_regex_replace_literal().
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: a newly allocated string containing the replacements.
 */
gchar *
egg_regex_replace (EggRegex            *regex, 
		 const gchar       *string, 
		 gssize             string_len,
		 gint               start_position,
		 const gchar       *replacement,
		 EggRegexMatchFlags   match_options,
		 GError           **error)
{
  gchar *result;
  GList *list;

  g_return_val_if_fail (replacement != NULL, NULL);

  list = split_replacement (replacement, error);
  result = egg_regex_replace_eval (regex, 
				 string, string_len, start_position,
				 interpolate_replacement,
				 (gpointer)list,
				 match_options);
  g_list_foreach (list, (GFunc)free_interpolation_data, NULL);
  g_list_free (list);

  return result;
}

static gboolean
literal_replacement (const EggRegex *regex,
		     const gchar  *string,
		     GString      *result,
		     gpointer      data)
{
  g_string_append (result, data);
  return FALSE;
}

/**
 * egg_regex_replace_literal:
 * @regex:  a #EggRegex structure.
 * @string:  the string to perform matches against.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @replacement:  text to replace each match with.
 * @match_options:  options for the match.
 *
 * Replaces all occurances of the pattern in @regex with the
 * replacement text. @replacement is replaced literally, to
 * include backreferences use egg_regex_replace().
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: a newly allocated string containing the replacements.
 */
gchar *
egg_regex_replace_literal (EggRegex          *regex,
			 const gchar     *string,
			 gssize           string_len,
			 gint             start_position,
			 const gchar     *replacement,
			 EggRegexMatchFlags match_options)
{
  g_return_val_if_fail (replacement != NULL, NULL);

  return egg_regex_replace_eval (regex,
			       string, string_len, start_position,
			       literal_replacement,
			       (gpointer)replacement,
			       match_options);
}

/**
 * egg_regex_replace_eval:
 * @gregex:  a #EggRegex structure.
 * @string:  string to perform matches against.
 * @string_len: the length of @string, or -1 if @string is nul-terminated.
 * @start_position: starting index of the string to match.
 * @eval: a function to call for each match.
 * @match_options:  Options for the match.
 *
 * Replaces occurances of the pattern in regex with the output of @eval
 * for that occurance.
 *
 * Setting @start_position differs from just passing over a shortened string
 * and  setting EGG_REGEX_MATCH_NOTBOL in the case of a pattern that begins
 * with any kind of lookbehind assertion, such as "\b".
 *
 * Returns: a newly allocated string containing the replacements.
 */
gchar *
egg_regex_replace_eval (EggRegex            *regex,
		      const gchar       *string,
		      gssize             string_len,
		      gint               start_position,
		      EggRegexEvalCallback eval,
		      gpointer           user_data,
		      EggRegexMatchFlags   match_options)
{
  gint string_len_bytes;
  GString *result;
  gint str_pos = 0;
  gboolean done = FALSE;

  g_return_val_if_fail (regex != NULL, NULL);
  g_return_val_if_fail (string != NULL, NULL);

  string_len_bytes = LEN_OFFSET_TO_INDEX (string, string_len);

  /* clear out the regex for reuse, just in case */
  egg_regex_clear (regex);

  result = g_string_sized_new (string_len);

  /* run down the string making matches. */
  while (!done &&
	 egg_regex_match_next_extended (regex, string, string_len,
				      start_position, match_options, NULL))
    {
      g_string_append_len (result,
			   string + str_pos,
			   regex->offsets[0] - str_pos);
      done = (*eval) (regex, string, result, user_data);
      str_pos = regex->offsets[1];
    }

  g_string_append_len (result, string + str_pos, string_len_bytes - str_pos);

  return g_string_free (result, FALSE);
}

/**
 * egg_regex_escape_string:
 * @string: the string to escape.
 * @length: the length of @string, or -1 if @string is nul-terminated.
 *
 * Escapes the special characters used for regular expressions in @string,
 * for instance "a.b*c" becomes "a\.b\*c". This function is useful to
 * dynamically generate regular expressions.
 *
 * @string can contain NULL characters that are replaced with "\0", in this
 * case remember to specify the correct length of @string in @length.
 *
 * Returns: a newly allocated escaped string.
 */
gchar *
egg_regex_escape_string (const gchar *string,
		       gint         length)
{
  GString *escaped;
  gchar *tmp;
  gint i;

  g_return_val_if_fail (string != NULL, NULL);

  if (length < 0)
    length = g_utf8_strlen (string, -1);

  escaped = g_string_new ("");
  tmp = (gchar*) string;
  for (i = 0; i < length; i++)
  {
    gunichar wc = g_utf8_get_char (tmp);
    switch (wc)
    {
      case '\0':
        g_string_append (escaped, "\\0");
        break;
      case '\\':
      case '|':
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case '^':
      case '$':
      case '*':
      case '+':
      case '?':
      case '.':
        g_string_append_unichar (escaped, '\\');
      default:
        g_string_append_unichar (escaped, wc);
    }
    tmp = g_utf8_next_char (tmp);
  }

  return g_string_free (escaped, FALSE);
}


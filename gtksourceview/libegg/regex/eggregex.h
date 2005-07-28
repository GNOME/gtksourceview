/* EggRegex -- regular expression API wrapper around PCRE.
 * Copyright (C) 1999 Scott Wimer
 * Copyright (C) 2004 Matthias Clasen
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
 * Author: Scott Wimer <scottw@cgibuilder.com>
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 *
 * This library is free software, you can distribute it or modify it
 * under the following terms:
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
#ifndef __EGGREGEX_H__
#define __EGGREGEX_H__

#include <glib/gtypes.h>
#include <glib/gquark.h>
#include <glib/gerror.h>
#include <glib/gstring.h>

G_BEGIN_DECLS

typedef enum
{
  EGG_REGEX_ERROR_COMPILE,
  EGG_REGEX_ERROR_OPTIMIZE,
  EGG_REGEX_ERROR_REPLACE,
  EGG_REGEX_ERROR_MATCH
} EggRegexError;

#define EGG_REGEX_ERROR egg_regex_error_quark ()

GQuark egg_regex_error_quark (void);

typedef enum
{
  EGG_REGEX_CASELESS          = 1 << 0,
  EGG_REGEX_MULTILINE         = 1 << 1,
  EGG_REGEX_DOTALL            = 1 << 2,
  EGG_REGEX_EXTENDED          = 1 << 3,
  EGG_REGEX_ANCHORED          = 1 << 4,
  EGG_REGEX_DOLLAR_ENDONLY    = 1 << 5,
  EGG_REGEX_UNGREEDY          = 1 << 9,
  EGG_REGEX_NO_AUTO_CAPTURE   = 1 << 12
} EggRegexCompileFlags;

typedef enum
{
  EGG_REGEX_MATCH_ANCHORED    = 1 << 4,
  EGG_REGEX_MATCH_NOTBOL      = 1 << 7,
  EGG_REGEX_MATCH_NOTEOL      = 1 << 8,
  EGG_REGEX_MATCH_NOTEMPTY    = 1 << 10
} EggRegexMatchFlags;

typedef struct _EggRegex  EggRegex;

typedef gboolean (*EggRegexEvalCallback) (const EggRegex*, const gchar*, GString*, gpointer);


EggRegex  *egg_regex_new          (const gchar           *pattern,
				   EggRegexCompileFlags   compile_options,
				   EggRegexMatchFlags     match_options,
				   GError               **error);
void       egg_regex_optimize     (EggRegex              *regex,
				   GError               **error);
void       egg_regex_free         (EggRegex              *regex);
EggRegex  *egg_regex_copy	  (const EggRegex        *regex);
gboolean   egg_regex_equal	  (const EggRegex        *v,
				   const EggRegex        *v2);
const gchar * egg_regex_get_pattern
				  (const EggRegex        *regex);
void       egg_regex_clear        (EggRegex              *regex);
gint       egg_regex_match        (EggRegex              *regex,
				   const gchar           *string,
				   EggRegexMatchFlags     match_options);
gint       egg_regex_match_extended
				  (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   gint                   start_position,
				   EggRegexMatchFlags     match_options,
				   GError               **error);
gboolean   egg_regex_match_next   (EggRegex              *regex,
				   const gchar           *string,
				   EggRegexMatchFlags     match_options);
gint       egg_regex_match_next_extended
				  (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   gint                   start_position,
				   EggRegexMatchFlags     match_options,
				   GError               **error);
gint       egg_regex_get_match_count
				  (const EggRegex        *regex);
gchar     *egg_regex_fetch        (const EggRegex        *regex,
				   const gchar           *string,
				   gint                   match_num);
gboolean   egg_regex_fetch_pos    (const EggRegex        *regex,
				   const gchar           *string,
				   gint                   match_num,
				   gint                  *start_pos,
				   gint                  *end_pos);
gchar     *egg_regex_fetch_named  (const EggRegex        *regex,
				   const gchar           *string,
				   const gchar           *name);
gboolean   egg_regex_fetch_named_pos
				  (const EggRegex        *regex,
				   const gchar           *string,
				   const gchar           *name,
				   gint                  *start_pos,
				   gint                  *end_pos);
gchar    **egg_regex_fetch_all    (const EggRegex        *regex,
				   const gchar           *string);
gint       egg_regex_expression_number_from_name
				  (const EggRegex        *regex, 
				   const gchar           *name);
gchar    **egg_regex_split        (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   EggRegexMatchFlags     match_options,
				   gint                   max_pieces);
gchar     *egg_regex_split_next   (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   EggRegexMatchFlags     match_options);
gchar     *egg_regex_replace      (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   gint                   start_position,
				   const gchar           *replacement,
				   EggRegexMatchFlags     match_options,
				   GError               **error);
gchar     *egg_regex_replace_literal
				  (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   gint                   start_position,
				   const gchar           *replacement,
				   EggRegexMatchFlags     match_options);
gchar     *egg_regex_replace_eval (EggRegex              *regex,
				   const gchar           *string,
				   gssize                 string_len,
				   gint                   start_position,
				   EggRegexEvalCallback   eval,
				   gpointer               user_data,
				   EggRegexMatchFlags     match_options);
gchar     *egg_regex_escape_string
				  (const gchar           *string,
				   gint                   length);


G_END_DECLS


#endif  /*  __EGGREGEX_H__ */

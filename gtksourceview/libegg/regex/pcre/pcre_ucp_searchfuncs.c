/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
           Copyright (c) 1997-2006 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/* This file has been modified to use glib instead of the internal table
 * in ucptable.c -- Marco Barisione */

/* This module contains code for searching the table of Unicode character
properties. */

#include "pcre_internal.h"

#include "ucp.h"               /* Category definitions */
#include "ucpinternal.h"       /* Internal table details */

#include <glib.h>


static guint16 script_for_unichar(gunichar ch);


/* Table to translate from particular type value to the general value. */

static int ucp_gentype[] = {
  ucp_C, ucp_C, ucp_C, ucp_C, ucp_C,  /* Cc, Cf, Cn, Co, Cs */
  ucp_L, ucp_L, ucp_L, ucp_L, ucp_L,  /* Ll, Lu, Lm, Lo, Lt */
  ucp_M, ucp_M, ucp_M,                /* Mc, Me, Mn */
  ucp_N, ucp_N, ucp_N,                /* Nd, Nl, No */
  ucp_P, ucp_P, ucp_P, ucp_P, ucp_P,  /* Pc, Pd, Pe, Pf, Pi */
  ucp_P, ucp_P,                       /* Ps, Po */
  ucp_S, ucp_S, ucp_S, ucp_S,         /* Sc, Sk, Sm, So */
  ucp_Z, ucp_Z, ucp_Z                 /* Zl, Zp, Zs */
};



/*************************************************
*         Search table and return type           *
*************************************************/

/* Three values are returned: the category is ucp_C, ucp_L, etc. The detailed
character type is ucp_Lu, ucp_Nd, etc. The script is ucp_Latin, etc.

Arguments:
  c           the character value
  type_ptr    the detailed character type is returned here
  script_ptr  the script is returned here

Returns:      the character type category
*/

int
_pcre_ucp_findprop(const unsigned int c, int *type_ptr, int *script_ptr)
{
/* Note that the Unicode types have the same values in glib and in
 * PCRE, so ucp_Ll == G_UNICODE_LOWERCASE_LETTER,
 * ucp_Zs == G_UNICODE_SPACE_SEPARATOR, and so on. */
*type_ptr = g_unichar_type(c);
*script_ptr = script_for_unichar(c);
return ucp_gentype[*type_ptr];
}




/*************************************************
*       Search table and return other case       *
*************************************************/

/* If the given character is a letter, and there is another case for the
letter, return the other case. Otherwise, return -1.

Arguments:
  c           the character value

Returns:      the other case or -1 if none
*/

int
_pcre_ucp_othercase(const int c)
{
int other_case = -1;

if (g_unichar_islower(c))
  other_case = g_unichar_toupper(c);
else if (g_unichar_isupper(c))
  other_case = g_unichar_tolower(c);

if (other_case == c)
  other_case = -1;

return other_case;
}


/* End of pcre_ucp_searchfuncs.c */


/* This code, copied from Pango, is used to get the script of a character.
 *
 * For more info see bug #348348 (http://bugzilla.gnome.org/show_bug.cgi?id=348348)
 * and http://mail.gnome.org/archives/gtk-devel-list/2006-July/msg00135.html
 *
 * Copyright (C) 2002 Red Hat Software
 */

/* Tables for the scripts. */
#include <ucptable.c>

#define SCRIPT_TABLE_MIDPOINT (G_N_ELEMENTS (script_table) / 2)

static inline guint16
script_for_unichar_bsearch (gunichar ch)
{
  int lower = 0;
  int upper = G_N_ELEMENTS (script_table) - 1;
  static int saved_mid = SCRIPT_TABLE_MIDPOINT;
  int mid = saved_mid;

  do 
    {
      if (ch < script_table[mid].start)
	upper = mid - 1;
      else if (ch >= script_table[mid].start + script_table[mid].chars)
	lower = mid + 1;
      else
	return script_table[saved_mid = mid].script;

      mid = (lower + upper) / 2;
    }
  while (lower <= upper);

  return ucp_Common;
}

static guint16
script_for_unichar (gunichar ch)
{
  if (ch < EASY_SCRIPTS_RANGE)
    return script_easy_table[ch];
  else 
    return script_for_unichar_bsearch (ch); 
}

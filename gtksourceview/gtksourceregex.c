/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 *  gtksourceregex.c
 *
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <glib.h>

#include "gtksourceview-i18n.h"
#include "gtksourceregex.h"


gboolean
gtk_source_regex_compile (GtkSourceRegex *regex, const gchar *pattern)
{
	g_return_val_if_fail (pattern != NULL, FALSE);
	g_return_val_if_fail (regex != NULL, FALSE);

	memset (&regex->buf, 0, sizeof (regex->buf));
	memset (&regex->reg, 0, sizeof (regex->reg));
	
	regex->len = strlen (pattern);

	regex->buf.translate = NULL;
	regex->buf.fastmap = g_malloc (256);
	regex->buf.allocated = 0;
	regex->buf.buffer = NULL;
	regex->buf.can_be_null = 0;	/* so we wont allow that in patterns! */
	regex->buf.no_sub = 0;
	
	if (re_compile_pattern (pattern, strlen (pattern), &regex->buf) == 0) {
		/* success...now try to compile a fastmap */
		if (re_compile_fastmap (&regex->buf) != 0) {
			g_warning ("Regex failed to create a fastmap.");
			/* error...no fastmap */
			g_free (regex->buf.fastmap);
			regex->buf.fastmap = NULL;
		}

		return TRUE;
	} else {
		g_warning ("Regex failed to compile.");
		return FALSE;
	}
}

void
gtk_source_regex_destroy (GtkSourceRegex *regex)
{
	g_return_if_fail (regex != NULL);

	g_free (regex->buf.fastmap);
	regex->buf.fastmap = NULL;
	regfree (&regex->buf);
	regex->len = 0;
	if (regex->reg.num_regs > 0) {
		
		/* FIXME: maybe we should pre-allocate the registers
		 * at compile time? */
		
		free (regex->reg.start);
		free (regex->reg.end);
		regex->reg.num_regs = 0;
	}
}

/**
 * gtk_source_regex_search:
 * @regex: Regular expression object (GtkSourceRegex)
 * @text: Buffer to search
 * @pos: Offset position (i.e. character offset, not byte offset)
 * @length: Length in bytes; -1 for the full text length
 * @match: (optional) Where to return match information
 * 
 * Return value: the offset where a match occurred, or less than 0 for
 * errors or no match.
 **/
gint 
gtk_source_regex_search (GtkSourceRegex       *regex,
			 const gchar          *text,
			 gint                  pos,
			 gint                  length,
			 GtkSourceBufferMatch *match)
{
	gint res;

	g_return_val_if_fail (regex != NULL, -2);
	g_return_val_if_fail (text != NULL, -2);
	g_return_val_if_fail (pos >= 0, -2);

	if (length < 0)
		length = strlen (text);
	
	/* convert pos from offset to byte index */
	if (pos > 0)
		pos = g_utf8_offset_to_pointer (text, pos) - text;

	res = re_search (&regex->buf, text, length,
			 pos, length - pos, &regex->reg);

	if (res < 0)
		return res;

	if (match != NULL) {
		match->startindex = res;
		match->endindex = regex->reg.end [0];
		
		match->startpos =
			g_utf8_pointer_to_offset (text, text + res);
		match->endpos = match->startpos +
			g_utf8_pointer_to_offset (text + res,
						  text + regex->reg.end [0]);

		return match->startpos;
	} else
		return g_utf8_pointer_to_offset (text, text + res);
}

/* regex_match -- tries to match regex at the 'pos' position in the text. It 
 * returns the number of chars matched, or -1 if no match. 
 * Warning: The number matched can be 0, if the regex matches the empty string.
 */

/*
 * pos: offset
 * 
 * Returns: number of bytes matched
 */
gint
gtk_source_regex_match (GtkSourceRegex *regex,
			const gchar    *text, 
			gint            length, 
			gint            pos)
{
	g_return_val_if_fail (regex != NULL, -1);
	g_return_val_if_fail (pos >= 0, -1);
	
	if (length < 0)
		length = strlen (text);
	
	pos = g_utf8_offset_to_pointer (text, pos) - text;

	return re_match (&regex->buf, text, length, pos,
			 &regex->reg);
}



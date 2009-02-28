/*
 * gtksourceindenter-utils.c
 * This file is part of gedit
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourceindenter-utils.h"
#include "gtksourceview-utils.h"
#include "gtksourceview.h"


gfloat
gtk_source_indenter_get_amount_indents (GtkTextView *view,
					GtkTextIter *cur)
{
	gint indent_width;
	GtkTextIter start;
	gunichar c;
	gint amount = 0;
	gint rest = 0;
	
	start = *cur;
	gtk_text_iter_set_line_offset (&start, 0);
	
	c = gtk_text_iter_get_char (&start);
	if (!g_unichar_isspace (c))
		return 0;
	
	indent_width = _gtk_source_view_get_real_indent_width (GTK_SOURCE_VIEW (view));
	
	while (g_unichar_isspace (c) &&
	       c != '\n' &&
	       c != '\r' &&
	       gtk_text_iter_compare (&start, cur) < 0)
	{
		if (c == '\t')
		{
			if (rest != 0)
				rest = 0;
			amount++;
		}
		else
		{
			rest++;
		}
		
		if (rest == indent_width)
		{
			amount++;
			rest = 0;
		}
		
		if (!gtk_text_iter_forward_char (&start))
			break;
		
		c = gtk_text_iter_get_char (&start);
	}

	return (gfloat)amount + (gfloat)(0.1 * rest);
}

gfloat
gtk_source_indenter_get_amount_indents_from_position (GtkTextView *view,
						      GtkTextIter *cur)
{
	gint indent_width;
	GtkTextIter start;
	gunichar c;
	gint amount = 0;
	gint rest = 0;
	
	indent_width = _gtk_source_view_get_real_indent_width (GTK_SOURCE_VIEW (view));
	
	start = *cur;
	gtk_text_iter_set_line_offset (&start, 0);
	
	c = gtk_text_iter_get_char (&start);
	
	while (gtk_text_iter_compare (&start, cur) < 0)
	{
		if (c == '\t')
		{
			if (rest != 0)
				rest = 0;
			amount++;
		}
		else
		{
			rest++;
		}
		
		if (rest == indent_width)
		{
			amount++;
			rest = 0;
		}
		
		if (!gtk_text_iter_forward_char (&start))
			break;
		
		c = gtk_text_iter_get_char (&start);
	}
	
	return (gfloat)amount + (gfloat)(0.1 * rest);
}

gboolean
gtk_source_indenter_move_to_no_space (GtkTextIter *iter,
				      gint direction)
{
	gunichar c;
	gboolean moved = TRUE;
	
	c = gtk_text_iter_get_char (iter);
	
	while (g_unichar_isspace (c))
	{
		if (!gtk_text_iter_forward_chars (iter, direction))
		{
			moved = FALSE;
			break;
		}
		c = gtk_text_iter_get_char (iter);
	}
	
	return moved;
}

gboolean
gtk_source_indenter_move_to_no_comments (GtkTextIter *iter)
{
	gunichar c;
	
	c = gtk_text_iter_get_char (iter);
	
	if (c == '/' && gtk_text_iter_backward_char (iter))
	{
		c = gtk_text_iter_get_char (iter);
		
		if (c == '*')
		{
			/*
			 * We look backward for '*' '/'
			 */
			for (;;)
			{
				if (!gtk_text_iter_backward_char (iter))
					return FALSE;
				c = gtk_text_iter_get_char (iter);
				
				if (c == '*')
				{
					if (!gtk_text_iter_backward_char (iter))
						return FALSE;
					c = gtk_text_iter_get_char (iter);
					
					if (c == '/')
					{
						/*
						 * We reached to the beggining of the comment,
						 * now we have to look backward for non spaces
						 */
						if (!gtk_text_iter_backward_char (iter))
							return FALSE;
						c = gtk_text_iter_get_char (iter);
						
						while (g_unichar_isspace (c) && gtk_text_iter_backward_char (iter))
						{
							c = gtk_text_iter_get_char (iter);
						}
						
						break;
					}
				}
			}
		}
	}
	
	return TRUE;
}

gboolean
gtk_source_indenter_find_open_char (GtkTextIter *iter,
				    gchar open,
				    gchar close,
				    gboolean skip_first)
{
	GtkTextIter copy;
	gunichar c;
	gboolean moved = FALSE;
	gint counter = 0;
	
	copy = *iter;
	
	/*
	 * FIXME: We have to take care of number of lines to go back
	 */
	c = gtk_text_iter_get_char (&copy);
	do
	{
		/*
		 * This algorithm has to work even if we have if (xxx, xx(),
		 */
		if (c == close || skip_first)
		{
			counter--;
			skip_first = FALSE;
		}
		
		if (c == open && counter != 0)
		{
			counter++;
		}
		
		if (counter == 0)
		{
			*iter = copy;
			moved = TRUE;
			break;
		}
	}
	while (gtk_text_iter_backward_char (&copy) &&
	       (c = gtk_text_iter_get_char (&copy)));
	
	return moved;
}

gfloat
gtk_source_indenter_add_space (GtkTextView *view,
			       gfloat current_level)
{
	gint indent_width;
	gint spaces;
	gint tabs;
	
	indent_width = _gtk_source_view_get_real_indent_width (GTK_SOURCE_VIEW (view));
	tabs = (gint)current_level;
	spaces = round (10 * (gfloat)(current_level - tabs));
	
	/*
	 * We add the new space
	 */
	spaces++;
	
	if (spaces == indent_width)
	{
		spaces = 0;
		tabs++;
	}
	
	return (gfloat)tabs + (gfloat)(0.1 * spaces);
}

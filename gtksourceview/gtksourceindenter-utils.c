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

gint
gtk_source_indenter_get_amount_indents (GtkTextView *view,
					GtkTextIter *cur)
{
	GtkTextIter start;
	gunichar c;
	
	g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), 0);
	g_return_val_if_fail (cur != NULL, 0);
	
	start = *cur;
	gtk_text_iter_set_line_offset (&start, 0);
	
	c = gtk_text_iter_get_char (&start);
	
	while (g_unichar_isspace (c) &&
	       c != '\n' &&
	       c != '\r')
	{
		if (!gtk_text_iter_forward_char (&start))
			break;
		
		c = gtk_text_iter_get_char (&start);
	}

	return gtk_source_indenter_get_amount_indents_from_position (view, &start);
}

gint
gtk_source_indenter_get_amount_indents_from_position (GtkTextView *view,
						      GtkTextIter *cur)
{
	gint indent_width;
	GtkTextIter start;
	gunichar c;
	gint amount = 0;
	gint rest = 0;
	
	g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), 0);
	g_return_val_if_fail (cur != NULL, 0);
	
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
			amount += indent_width;
		}
		else
		{
			rest++;
		}
		
		if (rest == indent_width)
		{
			amount += indent_width;
			rest = 0;
		}
		
		if (!gtk_text_iter_forward_char (&start))
			break;
		
		c = gtk_text_iter_get_char (&start);
	}
	
	return amount + rest;
}

gboolean
gtk_source_indenter_move_to_no_space (GtkTextIter *iter,
				      gint direction)
{
	gunichar c;
	gboolean moved = TRUE;
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
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
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
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
gtk_source_indenter_move_to_no_preprocessor (GtkTextIter *iter)
{
	gunichar c;
	GtkTextIter copy;
	gboolean moved = TRUE;
	
	copy = *iter;
	
	gtk_text_iter_set_line_offset (&copy, 0);
	gtk_source_indenter_move_to_no_space (&copy, 1);
	
	c = gtk_text_iter_get_char (&copy);
	
	if (c == '#')
	{
		/*
		 * Move back until we get a no space char
		 */
		do
		{
			if (!gtk_text_iter_backward_char (&copy))
				moved = FALSE;
			c = gtk_text_iter_get_char (&copy);
		} while (g_unichar_isspace (c));
		
		*iter = copy;
	}
	else
	{
		moved = FALSE;
	}
	
	return moved;
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
	
	g_return_val_if_fail (iter != NULL, FALSE);
	
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

gint
gtk_source_indenter_add_indent (GtkTextView *view,
				gint current_level)
{
	gint indent_width;
	
	g_return_val_if_fail (GTK_IS_TEXT_VIEW (view), 0);
	
	indent_width = _gtk_source_view_get_real_indent_width (GTK_SOURCE_VIEW (view));
	
	return current_level + indent_width;
}

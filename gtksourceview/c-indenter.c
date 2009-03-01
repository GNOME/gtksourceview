/*
 * c-indenter.c
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

#include "c-indenter.h"
#include "gtksourceindenter.h"
#include "gtksourceview.h"
#include "gtksourceindenter-utils.h"

#define C_INDENTER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object),\
				       C_TYPE_INDENTER, CIndenterPrivate))

struct _CIndenterPrivate
{
};

G_DEFINE_TYPE (CIndenter, c_indenter, GTK_TYPE_SOURCE_INDENTER)

static const gchar * regexes[] =
{
	"^\\s*(if|while|else if|for|switch)\\s*\\(.*\\)\\s*$",
	"^\\s*(else|do)\\s*$",
	NULL
};

static gboolean
match_regexes (GtkTextIter *iter)
{
	gint i = 0;
	gboolean match = FALSE;
	gchar *string;
	GtkTextIter start;
	
	start = *iter;
	
	gtk_source_indenter_find_open_char (&start, '(', ')', FALSE);
	
	gtk_text_iter_set_line_offset (&start, 0);
	gtk_text_iter_forward_char (iter);
	
	string = gtk_text_iter_get_text (&start, iter);
	gtk_text_iter_backward_char (iter);
	g_warning (string);
	
	while (regexes[i] != NULL)
	{
		GRegex *regex;
		GMatchInfo *match_info;
		
		regex = g_regex_new (regexes[i], G_REGEX_DOTALL, 0, NULL);
		g_regex_match (regex, string, 0, &match_info);
		
		if (g_match_info_matches (match_info))
			match = TRUE;
		
		g_match_info_free (match_info);
		g_regex_unref (regex);
		
		if (match)
		{
			break;
		}
		i++;
	}
	
	g_free (string);
	
	return match;
}

static gboolean
is_caselabel (const gchar *label)
{
	const gchar *case_label = "case";
	gchar *p;
	gunichar c;
	gint i = 0;
	gboolean is_case = TRUE;
	
	p = (gchar *)label;
	c = g_utf8_get_char (p);
	
	while (case_label[i] != '\0')
	{
		if (case_label[i] != c)
		{
			is_case = FALSE;
			break;
		}
		
		p = g_utf8_next_char (p);
		c = g_utf8_get_char (p);
		i++;
	}
	
	return is_case;
}

static gboolean
find_char_inline (GtkTextIter *iter,
		  gunichar c)
{
	gunichar f;
	gboolean found = FALSE;
	
	f = gtk_text_iter_get_char (iter);
	
	while (f != c && gtk_text_iter_get_line_offset (iter) != 0)
	{
		gtk_text_iter_backward_char (iter);
		f = gtk_text_iter_get_char (iter);
	}
	
	if (f == c)
	{
		found = TRUE;
	}
	
	return found;
}

static gfloat
c_indenter_get_indentation_level (GtkSourceIndenter *indenter,
				  GtkTextView *view,
				  GtkTextIter *cur,
				  gboolean relocating)
{
	/*
	 * The idea of this algorithm is just move the iter to the right position
	 * and manage the iter to get a context to get the right amount of indents
	 */
	GtkTextIter iter;
	gunichar c;
	gfloat amount = 0.;
	
	iter = *cur;
	
	if (!gtk_source_indenter_move_to_no_space (&iter, -1))
		return 0.;

	/*
	 * Check for comments
	 */
	if (!gtk_source_indenter_move_to_no_comments (&iter))
		return 0.;

	c = gtk_text_iter_get_char (&iter);
	g_warning ("char %c", c);
	
	if (c == '*')
	{
		gunichar ch;
		
		/*
		 * We are in a comment
		 */
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter);
		
		/* We are in the case "/ *" so we have to add an space */
		gtk_text_iter_backward_char (&iter);
		ch = gtk_text_iter_get_char (&iter);
		
		if (ch == '/')
		{
			amount = gtk_source_indenter_add_space (view, amount);
		}
	}
	else if (c == ';')
	{
		GtkTextIter copy;
		
		copy = iter;
		
		/*
		 * We have to check that we are not in something like:
		 * hello (eoeo,
		 *        eoeo);
		 */
		if (find_char_inline (&copy, ')') &&
		    gtk_source_indenter_find_open_char (&copy, '(', ')', FALSE))
		{
			amount = gtk_source_indenter_get_amount_indents (view,
									 &copy);
			
			/*
			 * We have to check if we are in just one line block
			 */
			while (!gtk_text_iter_ends_line (&copy) &&
			       gtk_text_iter_backward_char (&copy))
				continue;
		
			gtk_text_iter_backward_char (&copy);
			
			if (match_regexes (&copy))
			{
				gtk_source_indenter_find_open_char (&copy, '(', ')', FALSE);
			
				amount = gtk_source_indenter_get_amount_indents (view,
										 &copy);
			}
		}
		else
		{
			amount = gtk_source_indenter_get_amount_indents (view,
									 &iter);
			
			/*
			 * We have to check if we are in just one line block
			 */
			while (!gtk_text_iter_ends_line (&iter) &&
			       gtk_text_iter_backward_char (&iter))
				continue;
		
			gtk_text_iter_backward_char (&iter);
		
			if (match_regexes (&iter))
			{
				gtk_source_indenter_find_open_char (&iter, '(', ')', FALSE);
			
				amount = gtk_source_indenter_get_amount_indents (view,
										 &iter);
			}
		}
	}
	else if (c == '}')
	{
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter);
		
		/*
		 * We need to look backward for {.
		 * FIXME: We need to set a number of lines to look backward.
		 */
		if (relocating && gtk_source_indenter_find_open_char (&iter, '{', '}', FALSE))
		{
			amount = gtk_source_indenter_get_amount_indents (view,
									 &iter);
		}
	}
	else if (c == '{')
	{
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter);
		
		/*
		 * Check that the previous line match regexes
		 */
		while (gtk_text_iter_backward_char (&iter) &&
		       !gtk_text_iter_ends_line (&iter))
			continue;
		
		gtk_text_iter_backward_char (&iter);
		
		if (relocating)
		{
			if (match_regexes (&iter))
			{
				gtk_source_indenter_find_open_char (&iter, '(', ')', FALSE);
			
				amount = gtk_source_indenter_get_amount_indents (view,
										 &iter);
			}
		}
		else
		{
			amount++;
		}
	}
	else if (c == ',' || c == '&' || c == '|')
	{
		GtkTextIter s;
		
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter);
		
		s = iter;
		if (gtk_source_indenter_find_open_char (&s, '(', ')', TRUE))
		{
			amount = gtk_source_indenter_get_amount_indents_from_position (view, &s);
			amount = gtk_source_indenter_add_space (view, amount);
		}
	}
	else if (c == ')' && relocating)
	{
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter);
		
		if (gtk_source_indenter_find_open_char (&iter, '(', ')', FALSE))
		{
			amount = gtk_source_indenter_get_amount_indents_from_position (view, &iter);
		}
	}
	else if (c == ':')
	{
		if (relocating)
		{
			GtkTextIter start;
			gchar *label;
			
			start = iter;
			gtk_text_iter_set_line_offset (&start, 0);
			gtk_source_indenter_move_to_no_space (&start, 1);
			
			label = gtk_text_iter_get_text (&start, &iter);
			
			if (!is_caselabel (label))
			{
				amount = 0.;
			}
			else
			{
				amount = gtk_source_indenter_get_amount_indents (view, &iter);
			}
			
			g_free (label);
		}
		else
		{
			amount = 1.;
		}
	}
	else if (c == '=')
	{
		amount = gtk_source_indenter_get_amount_indents (view, &iter) + 1;
	}
	else if (match_regexes (&iter))
	{
		gtk_source_indenter_find_open_char (&iter, '(', ')', FALSE);
		
		amount = gtk_source_indenter_get_amount_indents (view,
								 &iter) + 1;
	}
	else
	{
		GtkTextIter copy;
		
		amount = gtk_source_indenter_get_amount_indents (view, &iter);
		
		copy = iter;
		
		/*
		 * We tried all cases, so now look if we are at the end of a
		 * func declaration
		 */
		if (c == ')')
		{
			if (gtk_source_indenter_find_open_char (&copy, '(', ')',
								FALSE))
			{
				amount = gtk_source_indenter_get_amount_indents (view,
										 &copy);
			}
		}
		else
		{
			gunichar ch;
			
			gtk_source_indenter_move_to_no_space (&copy, 1);
			ch = gtk_text_iter_get_char (&copy);
		
			/* # is always indent 0. Example: #ifdef */
			if (relocating && ch == '#')
			{
				amount = 0;
			}
		}
	}
	
	return amount;
}

static void
c_indenter_finalize (GObject *object)
{
	G_OBJECT_CLASS (c_indenter_parent_class)->finalize (object);
}

static void
c_indenter_class_init (CIndenterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceIndenterClass *indenter_class = GTK_SOURCE_INDENTER_CLASS (klass);
	
	object_class->finalize = c_indenter_finalize;
	
	indenter_class->get_indentation_level = c_indenter_get_indentation_level;

	//g_type_class_add_private (object_class, sizeof (CIndenterPrivate));
}

static void
c_indenter_init (CIndenter *self)
{
	//self->priv = C_INDENTER_GET_PRIVATE (self);
}

CIndenter *
c_indenter_new ()
{
	return g_object_new (C_TYPE_INDENTER, NULL);
}

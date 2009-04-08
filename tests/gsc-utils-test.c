/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gsc-utils.c
 *
 *  Copyright (C) 2007 - Chuchiperriman <chuchiperriman@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION:gsc-utils
 * @title: Gsc Utils
 * @short_description: Useful functions
 *
 */
 
#include <string.h> 
#include "gsc-utils-test.h"

gboolean
gsc_char_is_separator(const gunichar ch)
{
	if (g_unichar_isprint(ch) && (g_unichar_isalnum(ch) || ch == g_utf8_get_char("_")))
	{
		return FALSE;
	}
	
	return TRUE;
}

gchar*
gsc_get_last_word_and_iter(GtkTextView *text_view, 
			   GtkTextIter *start_word, 
			   GtkTextIter *end_word)
{
	GtkTextMark* insert_mark;
	GtkTextBuffer* text_buffer;
	GtkTextIter actual,temp;
	GtkTextIter *start_iter;
	gchar* text;
	gunichar ch;
	gboolean found, no_doc_start;
	
	if (start_word != NULL)
	{
		start_iter = start_word;
	}
	else
	{
		start_iter = &temp;
	}
	
	text_buffer = gtk_text_view_get_buffer(text_view);
	insert_mark = gtk_text_buffer_get_insert(text_buffer);
	gtk_text_buffer_get_iter_at_mark(text_buffer,&actual,insert_mark);
	
	*start_iter = actual;
	if (end_word!=NULL)
	{
		*end_word = actual;
	}
	
	found = FALSE;
	while ((no_doc_start = gtk_text_iter_backward_char(start_iter)) == TRUE)
	{
		ch = gtk_text_iter_get_char(start_iter);
		if (gsc_char_is_separator(ch))
		{
			found = TRUE;
			break;
		}
	}
	
	if (!no_doc_start)
	{
		gtk_text_buffer_get_start_iter(text_buffer,start_iter);
		text = gtk_text_iter_get_text (start_iter, &actual);
	}
	else
	{
	
		if (found)
		{
			gtk_text_iter_forward_char(start_iter);
			text = gtk_text_iter_get_text (start_iter, &actual);
		}
		else
		{
			*start_iter = actual;
			/*FIXME dup this var?*/
			text = "";
		}
	}
	
	return text;
}

gchar*
gsc_get_last_word(GtkTextView *text_view)
{
	return gsc_get_last_word_and_iter(text_view, NULL, NULL);
}

gchar*
gsc_get_last_word_cleaned(GtkTextView *view)
{
	gchar *word = gsc_get_last_word_and_iter(view, NULL, NULL);
	gchar *clean_word = gsc_clear_word(word);
	g_free(word);
	return clean_word;
}

void
gsc_get_cursor_pos(GtkTextView *text_view, 
		   gint *x, 
		   gint *y)
{
	GdkWindow *win;
	GtkTextMark* insert_mark;
	GtkTextBuffer* text_buffer;
	GtkTextIter start;
	GdkRectangle location;
	gint win_x, win_y;
	gint xx, yy;

	text_buffer = gtk_text_view_get_buffer(text_view);
	insert_mark = gtk_text_buffer_get_insert(text_buffer);
	gtk_text_buffer_get_iter_at_mark(text_buffer,&start,insert_mark);
	gtk_text_view_get_iter_location(text_view,
					&start,
					&location );
	gtk_text_view_buffer_to_window_coords (text_view,
						GTK_TEXT_WINDOW_WIDGET,
						location.x, 
						location.y,
						&win_x, 
						&win_y);

	win = gtk_text_view_get_window (text_view, 
	                                GTK_TEXT_WINDOW_WIDGET);
	gdk_window_get_origin (win, &xx, &yy);
	
	*x = win_x + xx;
	*y = win_y + yy + location.height;
}

gchar*
gsc_gsv_get_text(GtkTextView *text_view)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	
	buffer = gtk_text_view_get_buffer(text_view);
	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);
	return gtk_text_buffer_get_text(buffer,&start,&end,FALSE);
	
}

void
gtk_source_completion_replace_actual_word(GtkTextView *text_view, 
			const gchar* text)
{
	GtkTextBuffer *buffer;
	GtkTextIter word_start, word_end;
	
	buffer = gtk_text_view_get_buffer(text_view);
	gtk_text_buffer_begin_user_action(buffer);
	
	gsc_get_last_word_and_iter(text_view,&word_start, &word_end);

	GtkTextMark *mark = gtk_text_buffer_create_mark(buffer,
							"temp_replace",
							&word_start,
							TRUE);
	gtk_text_buffer_delete(buffer,&word_start,&word_end);
	gtk_text_buffer_get_iter_at_mark(buffer,&word_start,mark);
	gtk_text_buffer_insert(buffer, &word_start, text,-1);
	gtk_text_buffer_delete_mark(buffer,mark);
	gtk_text_buffer_end_user_action(buffer);
}

gchar*
gsc_clear_word(const gchar* word)
{
	int len = g_utf8_strlen(word,-1);
	int i;
	const gchar *temp = word;
	
	for (i=0;i<len;i++)
	{
		if (gsc_char_is_separator(g_utf8_get_char(temp)))
			temp = g_utf8_next_char(temp);
		else
			return g_strdup(temp);
		
	}
	return NULL;
}

gchar *
gsc_compute_line_indentation (GtkTextView *view,
			      GtkTextIter *cur)
{
	GtkTextIter start;
	GtkTextIter end;

	gunichar ch;
	gint line;

	line = gtk_text_iter_get_line (cur);

	gtk_text_buffer_get_iter_at_line (gtk_text_view_get_buffer (view),
					  &start,
					  line);

	end = start;

	ch = gtk_text_iter_get_char (&end);

	while (g_unichar_isspace (ch) &&
	       (ch != '\n') &&
	       (ch != '\r') &&
	       (gtk_text_iter_compare (&end, cur) < 0))
	{
		if (!gtk_text_iter_forward_char (&end))
			break;

		ch = gtk_text_iter_get_char (&end);
	}

	if (gtk_text_iter_equal (&start, &end))
		return NULL;

	return gtk_text_iter_get_slice (&start, &end);
}

gchar*
gsc_get_text_with_indent(const gchar* content,gchar *indent)
{
	GString *fin = NULL;
	gchar *ret = NULL;
	gint len = strlen(content);
	gint i;
	gint last_line = 0;
	for (i=0;i < len;i++)
	{
		if (content[i] == '\n' || content[i] =='\r')
		{
			if (fin==NULL)
				fin = g_string_new_len(content,i+1);
			else
			{
				fin = g_string_append_len(fin,
							  &content[last_line+1],
							  i - last_line);
			}
			fin = g_string_append(fin,indent);
			last_line = i;
		}
	}
	if (fin==NULL)
		ret = g_strdup(content);
	else
	{
		if (last_line < len -1)
		{
			fin = g_string_append_len(fin,
						  &content[last_line+1],
						  len - last_line);
		}
		ret = g_string_free(fin,FALSE);
	}
	return ret;
}


void
gsc_insert_text_with_indent(GtkTextView *view, const gchar* text)
{
	GtkTextBuffer * buffer = gtk_text_view_get_buffer(view);
	GtkTextMark *insert = gtk_text_buffer_get_insert(buffer);
	GtkTextIter cur;
	gtk_text_buffer_get_iter_at_mark(buffer,&cur,insert);
	gchar *indent = gsc_compute_line_indentation(view,&cur);
	gchar *indent_text = gsc_get_text_with_indent(text, indent);
	g_free(indent);
	gtk_text_buffer_insert_at_cursor(buffer,indent_text,-1);
	g_free(indent_text);
	gtk_text_view_scroll_mark_onscreen(view,insert);
}

gboolean
gsc_is_valid_word(gchar *current_word, gchar *completion_word)
{
	if (completion_word==NULL)
		return FALSE;
	if (current_word==NULL)
		return TRUE;
	
	gint len_cur = g_utf8_strlen (current_word,-1);
	if (g_utf8_collate(current_word,completion_word) == 0)
		return FALSE;

	if (len_cur!=0 && strncmp(current_word,completion_word,len_cur)==0)
		return TRUE;

	return FALSE;
}

void
gsc_get_window_position_center_screen(GtkWindow *window, gint *x, gint *y)
{
	gint w,h;
	gint sw = gdk_screen_width();
	gint sh = gdk_screen_height();
	gtk_window_get_size(window, &w, &h);
	*x = (sw/2) - (w/2) - 20;
	*y = (sh/2) - (h/2);
}

void
gsc_get_window_position_center_parent(GtkWindow *window,
				      GtkWidget *parent,
				      gint *x,
				      gint *y)
{
	GtkWindow *parent_win = GTK_WINDOW(gtk_widget_get_ancestor(parent,
				GTK_TYPE_WINDOW));
	gint w,h,px,py, pw,ph;
	gtk_window_get_position(parent_win,&px,&py);
	gtk_window_get_size(parent_win, &pw, &ph);
	gtk_window_get_size(window, &w, &h);
	
	*x = px + ((pw/2) - (w/2) -20);
	*y = py + ((ph/2) - (h/2));
}

gboolean 
gsc_get_window_position_in_cursor(GtkWindow *window,
				  GtkTextView *view,
				  gint *x,
				  gint *y,
				  gboolean *resized)
{
	gint w, h, xtext, ytext, ytemp;
	gint sw = gdk_screen_width();
	gint sh = gdk_screen_height();
	gboolean resize = FALSE;
	gboolean up = FALSE;
	gsc_get_cursor_pos(view,x,y);
	
	gtk_window_get_size(window, &w, &h);
	
	/* Processing x position and width */
	if (w > (sw - 8))
	{
		/* Resize to view all the window */
		resize = TRUE;
		w = sw -8;
	}
	
	/* Move position to view all the window */
	if ((*x + w) > (sw - 4))
	{
		*x = sw - w -4;
	}

	/* Processing y position and height */
	
	/* 
	If we cannot show it down, we show it up and if we cannot show it up, we
	show the window at the largest position 
	*/
	if ((*y + h) > sh)
	{
		PangoLayout* layout = 
			gtk_widget_create_pango_layout(GTK_WIDGET(view), NULL);
		pango_layout_get_pixel_size(layout,&xtext,&ytext);
		ytemp = *y - ytext;
		/* Cabe arriba? */
		if ((ytemp - h) >= 4)
		{
			*y = ytemp - h;
			up = TRUE;
		}
		else
		{
			/* 
			Si no cabe arriba, lo ponemos donde haya más espacio
			y redimensionamos la ventana
			*/
			if ((sh - *y) > ytemp)
			{
				h = sh - *y - 4;
			}
			else
			{
				*y = 4;
				h = ytemp -4;
				up = TRUE;
			}
			resize = TRUE;
		}
		g_object_unref(layout);
	}
	
	if (resize)
		gtk_window_resize(window, w, h);

	if (resized != NULL)
		*resized = resize;
	
	return up;
}




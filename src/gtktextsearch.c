/*
*    
*
*  Copyright (C) 2002 Mikael Hermansson <tyan@linux.se>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#include <gtk/gtk.h>
#include "gtktextsearch.h"

static GObjectClass *parent_class = NULL;

void gtk_text_search_class_init (GtkTextSearchClass *klass);
void gtk_text_search_init (GtkTextSearch *object);
void gtk_text_search_finalize (GtkTextSearch *object);


void
gtk_text_search_init(GtkTextSearch *text_search)
{
  text_search->search_for = g_strdup("");
  text_search->sflags = 0;
  text_search->mark_current = NULL;
  text_search->mark_stop = NULL;
  text_search->buffer = NULL;
}

void
gtk_text_search_class_init(GtkTextSearchClass *klass)
{
  GObjectClass *object_class;
  object_class = (GObjectClass*) klass;
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gtk_text_search_finalize;
}

void
gtk_text_search_finalize (GtkTextSearch *text_search)
{
  g_free (text_search->search_for);
  g_object_unref (G_OBJECT(text_search->mark_current));
  g_object_unref (G_OBJECT(text_search->mark_stop));
  g_object_unref (G_OBJECT(text_search->buffer));

   (* G_OBJECT_CLASS (parent_class)->finalize) (G_OBJECT(text_search));
}

GtkTextSearch *
gtk_text_search_new (GtkTextBuffer *buffer,
                              const GtkTextIter *start, 
                              const char *searchfor, 
                                  GtkETextSearchFlags sflags, 
                              const GtkTextIter *limit)
{
  GtkTextSearch *text_search;

  text_search = GTK_TEXT_SEARCH(g_object_new (GTK_TYPE_TEXT_SEARCH, NULL));
  gtk_text_search_set(text_search, buffer, start, searchfor, sflags, limit);
  
  return text_search;
}

void
gtk_text_search_set (GtkTextSearch *text_search,
                                GtkTextBuffer *buffer, 
                              const GtkTextIter *startp, 
                              const char *searchfor, 
                                  GtkETextSearchFlags sflags, 
                              const GtkTextIter *limit)
{
  GtkTextIter end, start;

  text_search = GTK_TEXT_SEARCH(g_object_new (GTK_TYPE_TEXT_SEARCH, NULL));

  if (text_search->buffer != buffer)  {
    g_object_ref (G_OBJECT(text_search->buffer));
  }

  if (startp)
    start = *startp;
  else
    gtk_text_buffer_get_start_iter(text_search->buffer, &start);

  if (limit)
    end = *limit;
  else
    gtk_text_buffer_get_end_iter(text_search->buffer, &end);

  if (sflags)  
    text_search->sflags = sflags;  
  
  if(searchfor)  {
    g_free(text_search->search_for);
    text_search->search_for = g_strdup (searchfor);
 }

  if (text_search->mark_current)
    g_object_unref(G_OBJECT(text_search->mark_current));
  if (text_search->mark_stop)
    g_object_unref(G_OBJECT(text_search->mark_stop));

  text_search->mark_current = gtk_text_buffer_create_mark(text_search->buffer, "search_mark_current", &start, FALSE);   
  text_search->mark_stop = gtk_text_buffer_create_mark(text_search->buffer, "search_mark_stop", &end, FALSE);   
}

void
gtk_text_search_set_interval (GtkTextSearch *text_search,
                                GtkTextBuffer *buffer, 
                              const GtkTextIter *start, 
                             const GtkTextIter *end)
{
  gtk_text_search_set (text_search, buffer, start, NULL, 0, end);
}

gboolean
gtk_source_buffer_compare_unichar (gunichar ch,  gpointer data)
{
  GtkTextSearch *si;
  gunichar chsearch; 
  si = GTK_TEXT_SEARCH(data);

  /* there are no more characters match this means we succed scaning  :-) */
  if (!*si->offset)  {  
    si->is_matched = TRUE; 
    return TRUE;
  }


  chsearch = g_utf8_get_char(si->offset);  
  if ( ch == 0xFFFC && si->sflags & GTK_ETEXT_SEARCH_TEXT_ONLY)   /* we don't care about pixbufs */
    return FALSE;

  if (si->sflags & GTK_ETEXT_SEARCH_CASE_INSENSITIVE)  {
    chsearch = g_unichar_tolower(chsearch);
    ch = g_unichar_tolower(ch);
  }

  if (ch == chsearch) 
    si->offset = g_utf8_next_char(si->offset);
  else if (si->offset != si->search_for) /* the scan has already matched first character */
    return TRUE;  /* return TRUE stop scan because we failed on this character */

  return FALSE;
}

gboolean
gtk_text_search_forward (GtkTextSearch *search, GtkTextIter *match_start, GtkTextIter *match_end)
{
  GtkTextIter iter1, iter2;
/*  g_return_val_if_fail(search =! NULL, FALSE);*/

/*  g_return_val_if_fail(GTK_IS_TEXT_SEARCH(search), FALSE);*/

  /* always initialize */   
  search->is_matched = FALSE;
  search->offset = search->search_for;


  gtk_text_buffer_get_iter_at_mark (search->buffer, &iter1, search->mark_current);
  gtk_text_buffer_get_iter_at_mark (search->buffer, &iter2, search->mark_stop);

  gtk_source_buffer_compare_unichar (gtk_text_iter_get_char(&iter1), search);
  gtk_text_iter_forward_find_char (&iter1, gtk_source_buffer_compare_unichar, search , &iter2);
  gtk_text_buffer_move_mark (search->buffer, search->mark_current, &iter1);   
  if ( search->is_matched)
  {
    *match_start = iter1;
    *match_end = iter1;
    gtk_text_iter_backward_chars(match_start, g_utf8_strlen(search->search_for, -1));

    return TRUE;
  }

  return FALSE;  
}

gint
gtk_text_search_forward_foreach (GtkTextSearch *search, GtkTextSearchForeachFunc sfunc, gpointer data)
{
  GtkTextIter iter1, iter2, start_match, end_match;
  gint count = 0;
  
  gtk_text_buffer_get_iter_at_mark (search->buffer, &iter1, search->mark_current);
  gtk_text_buffer_get_iter_at_mark (search->buffer, &iter2, search->mark_stop);
  while (gtk_text_iter_compare (&iter1, &iter2) < 0)  {
    if (gtk_text_search_forward (search, &start_match, &end_match))  {
      count++;
      if (sfunc (&start_match, &end_match, data))
        return count;
    }

    gtk_text_buffer_get_iter_at_mark (search->buffer, &iter1, search->mark_current);
    gtk_text_buffer_get_iter_at_mark (search->buffer, &iter2, search->mark_stop);
  }
  
  return count;
}


GType gtk_text_search_get_type ()
{
  static GType our_type = 0;

  if (our_type == 0)
    {
      static const GTypeInfo our_info =
      {
        sizeof (GtkTextSearchClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_text_search_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkTextSearch),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_text_search_init
      };

      our_type = g_type_register_static (G_TYPE_OBJECT,
                                         "GtkTextSearch",
                                         &our_info,
                                         0);
    }

  return our_type;
  
}


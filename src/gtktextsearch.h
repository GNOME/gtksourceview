/* GtkTextSearch
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


#ifndef _GTK_TEXT_SEARCH__H_
#define _GTK_TEXT_SEARCH__H_

#ifdef __cplusplus
extern "C" {
#endif

#define GTK_TYPE_TEXT_SEARCH                  (gtk_text_search_get_type ())
#define GTK_TEXT_SEARCH(obj)                  ( G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_TEXT_SEARCH, GtkTextSearch))
#define GTK_TEXT_SEARCH_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_SEARCH, GtkTextSearchClass))
#define GTK_IS_TEXT_SEARCH(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT_SEARCH))
#define GTK_IS_TEXT_SEARCH_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_SEARCH))

typedef struct _GtkTextSearch GtkTextSearch;
typedef struct _GtkTextSearchClass GtkTextSearchClass;
typedef gboolean (*GtkTextSearchForeachFunc) (GtkTextIter *match_start, GtkTextIter *match_end, gpointer data);

typedef enum  {
  GTK_ETEXT_SEARCH_VISIBLE_ONLY, /* TODO */
  GTK_ETEXT_SEARCH_TEXT_ONLY,
  GTK_ETEXT_SEARCH_CASE_INSENSITIVE,
  GTK_ETEXT_SEARCH_REGEXP /* TODO: this is not yet implemented */
}GtkETextSearchFlags;

struct _GtkTextSearch
{
  GObject parent;

  gchar *search_for; /* the text to search for */
  gchar *offset;  /* offset in search_for used in forward find char callback */

  GtkTextBuffer *buffer;
  GtkTextMark *mark_current;
  GtkTextMark *mark_stop;  
  GtkETextSearchFlags sflags;

  gint is_matched : 1;  
};

struct _GtkTextSearchClass
{
  GObjectClass parent;
    
};

GType gtk_text_search_get_type ();
GtkTextSearch * gtk_text_search_new (GtkTextBuffer *buffer, const GtkTextIter *start, 
                                              const char *search,
                                                GtkETextSearchFlags sflags, 
                                                const GtkTextIter *limit);
void gtk_text_search_set (GtkTextSearch *search, 
                                    GtkTextBuffer *buffer, 
                                    const GtkTextIter *start, 
                                    const char *searchfor,  
                                   GtkETextSearchFlags sflags, 
                                                const GtkTextIter *limit);
void gtk_text_searh_set_interval (GtkTextSearch *search, GtkTextBuffer *buffer, const GtkTextIter *start, const GtkTextIter *end);

gboolean gtk_text_search_forward (GtkTextSearch *search, GtkTextIter *match_start, GtkTextIter *match_end);
gboolean gtk_text_search_backward (GtkTextSearch *search, GtkTextIter *match_start, GtkTextIter *match_end);
gint gtk_text_search_forward_foreach (GtkTextSearch *search, GtkTextSearchForeachFunc func, gpointer data);
gint gtk_text_search_backward_foreach (GtkTextSearch *search, GtkTextSearchForeachFunc func, gpointer data);

#ifdef __cplusplus
};
#endif

#endif __END  /* END OF GTK_TEXT_SEARCH__H_ */

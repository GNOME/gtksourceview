/*  gtksourceview.h
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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
 *  You should have received a copy of the GNU Library General Public License*  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_VIEW_H__
#define __GTK_SOURCE_VIEW_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>
#include <gtk/gtktextview.h>
#include <gtksourcebuffer.h>

#define GTK_TYPE_SOURCE_VIEW                  (gtk_source_view_get_type ())
#define GTK_SOURCE_VIEW(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_SOURCE_VIEW, GtkSourceView))
#define GTK_SOURCE_VIEW_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_VIEW, GtkSourceViewClass))
#define GTK_IS_SOURCE_VIEW(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_SOURCE_VIEW))
#define GTK_IS_SOURCE_VIEW_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_VIEW))

typedef struct _GtkSourceView GtkSourceView;
typedef struct _GtkSourceViewClass GtkSourceViewClass;

struct _GtkSourceView
{
  GtkTextView parent;
  
  guint tab_stop;
  gint show_line_numbers :1;
  gint line_number_space;
  guint show_line_pixmaps :1;
  GHashTable *pixmap_cache;

  gchar *delete_range;
};

struct _GtkSourceViewClass
{
  GtkTextViewClass parent_class;
  
  void (*undo) ();
  void (*redo) ();
};

GType gtk_source_view_get_type();

GtkWidget *gtk_source_view_new();
GtkWidget *gtk_source_view_new_with_buffer(GtkSourceBuffer *buffer);

void gtk_source_view_set_show_line_numbers(GtkSourceView *view, gboolean show);
gboolean gtk_source_view_get_show_line_numbers(GtkSourceView *view);
void gtk_source_view_set_show_line_pixmaps(GtkSourceView *view, gboolean show);
gboolean gtk_source_view_get_show_line_pixmaps(GtkSourceView *view);

void gtk_source_view_set_tab_stop(GtkSourceView *view, gint tab_stop);
gint gtk_source_view_get_tab_stop(GtkSourceView *view);
/* Get the width in pixels */
gint gtk_source_view_get_tab_stop_width(GtkSourceView *view);

gboolean gtk_source_view_add_pixbuf(GtkSourceView *view, const gchar *key, GdkPixbuf *pixbuf, gboolean overwrite);
GdkPixbuf *gtk_source_view_get_pixbuf(GtkSourceView *view, const gchar *key);
#ifdef __cplusplus
}
#endif
#endif /* end of SOURCE_VIEW_H__ */

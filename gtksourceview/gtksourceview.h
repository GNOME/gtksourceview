/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourceview.h
 *
 *  Copyright (C) 2001 - Mikael Hermansson <tyan@linux.se> and
 *  Chris Phelps <chicane@reninet.com>
 *
 *  Copyright (C) 2003 - Gustavo Giráldez and Paolo Maggi 
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
 *  You should have received a copy of the GNU Library General Public
 *  License* along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 */

#ifndef __GTK_SOURCE_VIEW_H__
#define __GTK_SOURCE_VIEW_H__

#include <gtk/gtk.h>
#include <gtk/gtktextview.h>

#include <gtksourceview/gtksourcebuffer.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_VIEW             (gtk_source_view_get_type ())
#define GTK_SOURCE_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_VIEW, GtkSourceView))
#define GTK_SOURCE_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_VIEW, GtkSourceViewClass))
#define GTK_IS_SOURCE_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_VIEW))
#define GTK_IS_SOURCE_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_VIEW))
#define GTK_SOURCE_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_VIEW, GtkSourceViewClass))


typedef struct _GtkSourceView GtkSourceView;
typedef struct _GtkSourceViewClass GtkSourceViewClass;

typedef struct _GtkSourceViewPrivate GtkSourceViewPrivate;

struct _GtkSourceView 
{
	GtkTextView           parent;

	GtkSourceViewPrivate *priv;
};

struct _GtkSourceViewClass 
{
	GtkTextViewClass parent_class;

	void (*undo) (GtkSourceView *view);
	void (*redo) (GtkSourceView *view);

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) (void);
	void (*_gtk_source_reserved2) (void);
	void (*_gtk_source_reserved3) (void);
};

GType		 gtk_source_view_get_type 		(void) G_GNUC_CONST;

/* Constructors */
GtkWidget 	*gtk_source_view_new 			(void);
GtkWidget 	*gtk_source_view_new_with_buffer	(GtkSourceBuffer *buffer);

/* Properties */
void 		 gtk_source_view_set_show_line_numbers 	(GtkSourceView   *view,
							 gboolean         show);
gboolean 	 gtk_source_view_get_show_line_numbers 	(GtkSourceView   *view);

void 		 gtk_source_view_set_show_line_markers  (GtkSourceView   *view,
							 gboolean         show);
gboolean	 gtk_source_view_get_show_line_markers  (GtkSourceView   *view);

void 		 gtk_source_view_set_tabs_width 	(GtkSourceView   *view, 
							 guint            width);
guint            gtk_source_view_get_tabs_width         (GtkSourceView   *view);

void		 gtk_source_view_set_auto_indent 	(GtkSourceView   *view, 
							 gboolean         enable);
gboolean	 gtk_source_view_get_auto_indent 	(GtkSourceView   *view);

void		 gtk_source_view_set_insert_spaces_instead_of_tabs 
							(GtkSourceView   *view, 
							 gboolean         enable);
gboolean	 gtk_source_view_get_insert_spaces_instead_of_tabs 
							(GtkSourceView   *view);

void		 gtk_source_view_set_show_margin 	(GtkSourceView   *view,
							 gboolean         show);
gboolean 	 gtk_source_view_get_show_margin 	(GtkSourceView   *view);

void		 gtk_source_view_set_highlight_current_line 	
							(GtkSourceView   *view,
							 gboolean         show);
gboolean 	 gtk_source_view_get_highlight_current_line 	
							(GtkSourceView   *view);

void		 gtk_source_view_set_margin 		(GtkSourceView   *view,
							 guint            margin);
guint		 gtk_source_view_get_margin 		(GtkSourceView   *view);

void             gtk_source_view_set_marker_pixbuf      (GtkSourceView   *view,
							 const gchar     *marker_type,
							 GdkPixbuf       *pixbuf);
GdkPixbuf	*gtk_source_view_get_marker_pixbuf      (GtkSourceView   *view,
				       			 const gchar     *marker_type);

void		 gtk_source_view_set_smart_home_end	(GtkSourceView   *view,
							 gboolean         enable);
gboolean	 gtk_source_view_get_smart_home_end	(GtkSourceView   *view);

G_END_DECLS
#endif				/* end of SOURCE_VIEW_H__ */

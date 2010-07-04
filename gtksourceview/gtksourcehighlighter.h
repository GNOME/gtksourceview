/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcehighlighter.h
 *
 *  Copyright (C) 2003 - Gustavo Giráldez
 *  Copyright (C) 2005 - Marco Barisione, Emanuele Aina
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

#ifndef __GTK_SOURCE_HIGHLIGHTER_H__
#define __GTK_SOURCE_HIGHLIGHTER_H__

#include <gtksourceview/gtksourcecontextengine.h>
#include <gtksourceview/gtksourcecontextengine-private.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_HIGHLIGHTER            (_gtk_source_highlighter_get_type ())
#define GTK_SOURCE_HIGHLIGHTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_HIGHLIGHTER, GtkSourceHighlighter))
#define GTK_SOURCE_HIGHLIGHTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_HIGHLIGHTER, GtkSourceHighlighterClass))
#define GTK_IS_SOURCE_HIGHLIGHTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_HIGHLIGHTER))
#define GTK_IS_SOURCE_HIGHLIGHTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_HIGHLIGHTER))
#define GTK_SOURCE_HIGHLIGHTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_HIGHLIGHTER, GtkSourceHighlighterClass))

typedef struct _GtkSourceHighlighter        GtkSourceHighlighter;
typedef struct _GtkSourceHighlighterClass   GtkSourceHighlighterClass;
typedef struct _GtkSourceHighlighterPrivate GtkSourceHighlighterPrivate;

struct _GtkSourceHighlighter
{
	GObject parent_instance;

	/*< private >*/
	GtkSourceHighlighterPrivate *priv;
};

struct _GtkSourceHighlighterClass
{
	GObjectClass parent_class;
};

GType		 	_gtk_source_highlighter_get_type	 (void) G_GNUC_CONST;

GtkSourceHighlighter * 	_gtk_source_highlighter_new 		 (GtkSourceContextEngine *engine);
void 			_gtk_source_highlighter_set_style_scheme (GtkSourceHighlighter   *highlighter,
				       				  GtkSourceStyleScheme   *scheme);
void 			_gtk_source_highlighter_set_styles_map	 (GtkSourceHighlighter   *highlighter,
				       				  GHashTable 	         *styles);
void			_gtk_source_highlighter_attach_buffer 	 (GtkSourceHighlighter   *highlighter,
								  GtkTextBuffer          *buffer,
								  Segment	         *root_segment);
void			_gtk_source_highlighter_ensure_highlight (GtkSourceHighlighter   *highlighter,
								  const GtkTextIter      *start,
								  const GtkTextIter      *end);
void			_gtk_source_highlighter_invalidate_region (GtkSourceHighlighter *highlighter,
			                                          const GtkTextIter *start,
			                                          const GtkTextIter *end);
GtkTextTag *		_gtk_source_highlighter_get_tag_for_style (GtkSourceHighlighter *highlighter,
								   const gchar          *style,
								   GtkTextTag 	        *parent_tag);

G_END_DECLS

#endif /* __GTK_SOURCE_HIGHLIGHTER_H__ */

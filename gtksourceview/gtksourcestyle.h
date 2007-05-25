/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestyle.h
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

#ifndef __GTK_SOURCE_STYLE_H__
#define __GTK_SOURCE_STYLE_H__

#include <gtk/gtktexttag.h>

G_BEGIN_DECLS

typedef struct _GtkSourceStyle GtkSourceStyle;

typedef enum {
	GTK_SOURCE_STYLE_USE_BACKGROUND    = 1 << 0,	/*< nick=use_background >*/
	GTK_SOURCE_STYLE_USE_FOREGROUND    = 1 << 1,	/*< nick=use_foreground >*/
	GTK_SOURCE_STYLE_USE_ITALIC        = 1 << 2,	/*< nick=use_italic >*/
	GTK_SOURCE_STYLE_USE_BOLD          = 1 << 3,	/*< nick=use_bold >*/
	GTK_SOURCE_STYLE_USE_UNDERLINE     = 1 << 4,	/*< nick=use_underline >*/
	GTK_SOURCE_STYLE_USE_STRIKETHROUGH = 1 << 5	/*< nick=use_strikethrough >*/
} GtkSourceStyleMask;

struct _GtkSourceStyle
{
	GtkSourceStyleMask mask;

	GdkColor foreground;
	GdkColor background;

	guint italic : 1;
	guint bold : 1;
	guint underline : 1;
	guint strikethrough : 1;
};

#define GTK_TYPE_SOURCE_STYLE     (gtk_source_style_get_type ())

GType		 gtk_source_style_get_type	(void) G_GNUC_CONST;

GtkSourceStyle	*gtk_source_style_new		(GtkSourceStyleMask    mask);
GtkSourceStyle	*gtk_source_style_copy		(const GtkSourceStyle *style);
void		 gtk_source_style_free		(GtkSourceStyle       *style);

void		 _gtk_source_style_apply	(const GtkSourceStyle *style,
						 GtkTextTag           *tag);

G_END_DECLS

#endif  /* __GTK_SOURCE_STYLE_H__ */

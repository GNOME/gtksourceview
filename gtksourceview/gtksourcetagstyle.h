/*  gtksourcetagstyle.h
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

#ifndef __GTK_SOURCE_TAG_STYLE_H__
#define __GTK_SOURCE_TAG_STYLE_H__

#include <glib.h>
#include <gdk/gdkcolor.h>

G_BEGIN_DECLS

typedef struct _GtkSourceTagStyle GtkSourceTagStyle;

typedef enum {
	GTK_SOURCE_TAG_STYLE_USE_BACKGROUND = 1 << 0,	/*< nick=use_background >*/
	GTK_SOURCE_TAG_STYLE_USE_FOREGROUND = 1 << 1	/*< nick=use_foreground >*/
} GtkSourceTagStyleMask; 

struct _GtkSourceTagStyle {

	/* readonly */
	gboolean is_default;

	guint mask;
	
	GdkColor foreground;
	GdkColor background;
	
	gboolean italic;
	gboolean bold;
	gboolean underline;
	gboolean strikethrough;

	/* Reserved for future expansion */
	guint8 reserved[16];	
};

#define GTK_TYPE_SOURCE_TAG_STYLE     (gtk_source_tag_style_get_type ())

GType              gtk_source_tag_style_get_type (void) G_GNUC_CONST;

GtkSourceTagStyle *gtk_source_tag_style_new      (void);
GtkSourceTagStyle *gtk_source_tag_style_copy     (const GtkSourceTagStyle *style);
void               gtk_source_tag_style_free     (GtkSourceTagStyle       *style);

G_END_DECLS

#endif  /* __GTK_SOURCE_TAG_STYLE_H__ */


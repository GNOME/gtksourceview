/*  gtksourcestylescheme.h
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

#ifndef __GTK_SOURCE_STYLE_SCHEME_H__
#define __GTK_SOURCE_STYLE_SCHEME_H__

#include <glib-object.h> 
#include <gtksourcetagstyle.h>

G_BEGIN_DECLS

/*
 * A theme should define at least the following styles to work well with the included .lang files:
 *
 * TODO
 *
 * The default theme defines all of them.
 *
 */

#define GTK_TYPE_SOURCE_STYLE_SCHEME             (gtk_source_style_scheme_get_type ())
#define GTK_STYLE_SOURCE_SCHEME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleScheme))
#define GTK_STYLE_SOURCE_SCHEME_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleSchemeClass))
#define GTK_IS_SOURCE_STYLE_SCHEME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_STYLE_SCHEME))
#define GTK_IS_SOURCE_STYLE_SCHEME_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_TYPE_SOURCE_STYLE_SCHEME))
#define GTK_SOURCE_STYLE_SCHEME_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleSchemeClass))

typedef struct _GtkSourceStyleScheme       GtkSourceStyleScheme;         /* Dummy typedef */
typedef struct _GtkSourceStyleSchemeClass  GtkSourceStyleSchemeClass;

struct _GtkSourceStyleSchemeClass
{
	GTypeInterface	base_iface;

	/* vtable */
	const gchar		* (* get_name)		(GtkSourceStyleScheme *scheme);
	const GtkSourceTagStyle * (* get_tag_style) 	(GtkSourceStyleScheme *scheme,
						     	 const gchar          *style_name);

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) (void);
	void (*_gtk_source_reserved2) (void);
	void (*_gtk_source_reserved3) (void);
	void (*_gtk_source_reserved4) (void);	
};

GType                        gtk_source_style_scheme_get_type      (void) G_GNUC_CONST;


const GtkSourceTagStyle	    *gtk_source_style_scheme_get_tag_style (GtkSourceStyleScheme *scheme,
								    const gchar          *style_name);
const gchar		    *gtk_source_style_scheme_get_name      (GtkSourceStyleScheme *scheme);


/* Default style scheme */

GtkSourceStyleScheme	    *gtk_source_style_scheme_get_default   (void);

G_END_DECLS

#endif  /* __GTK_SOURCE_STYLE_SCHEME_H__ */


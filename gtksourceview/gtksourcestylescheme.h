/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestylescheme.h
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

#include <gtk/gtkwidget.h>
#include <gtksourceview/gtksourcestyle.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_STYLE_SCHEME             (gtk_source_style_scheme_get_type ())
#define GTK_SOURCE_STYLE_SCHEME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleScheme))
#define GTK_SOURCE_STYLE_SCHEME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleSchemeClass))
#define GTK_IS_SOURCE_STYLE_SCHEME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_STYLE_SCHEME))
#define GTK_IS_SOURCE_STYLE_SCHEME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_STYLE_SCHEME))
#define GTK_SOURCE_STYLE_SCHEME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_STYLE_SCHEME, GtkSourceStyleSchemeClass))

typedef struct _GtkSourceStyleScheme             GtkSourceStyleScheme;
typedef struct _GtkSourceStyleSchemePrivate      GtkSourceStyleSchemePrivate;
typedef struct _GtkSourceStyleSchemeClass        GtkSourceStyleSchemeClass;

struct _GtkSourceStyleScheme
{
	GObject base;
	GtkSourceStyleSchemePrivate *priv;
};

struct _GtkSourceStyleSchemeClass
{
	GObjectClass base_class;

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) (void);
	void (*_gtk_source_reserved2) (void);
};

GType			 gtk_source_style_scheme_get_type	(void) G_GNUC_CONST;

GtkSourceStyleScheme	*_gtk_source_style_scheme_new		(const gchar          *id,
								 const gchar          *name);

const gchar             *gtk_source_style_scheme_get_id         (GtkSourceStyleScheme *scheme);
const gchar             *gtk_source_style_scheme_get_name       (GtkSourceStyleScheme *scheme);

/* Return value must be freed with gtk_source_style_free */
GtkSourceStyle		*gtk_source_style_scheme_get_style	(GtkSourceStyleScheme *scheme,
								 const gchar          *style_name);
GtkSourceStyle		*gtk_source_style_scheme_get_matching_brackets_style
								(GtkSourceStyleScheme *scheme);
gboolean		 gtk_source_style_scheme_get_current_line_color
								(GtkSourceStyleScheme *scheme,
								 GdkColor             *color);
void			 gtk_source_style_scheme_set_style	(GtkSourceStyleScheme *scheme,
								 const gchar          *name,
								 const GtkSourceStyle *style);

void			 _gtk_source_style_scheme_apply		(GtkSourceStyleScheme *scheme,
								 GtkWidget            *widget);
GtkSourceStyleScheme	*_gtk_source_style_scheme_new_from_file (const gchar          *filename);
GtkSourceStyleScheme	*_gtk_source_style_scheme_default_new	(void);
const gchar		*_gtk_source_style_scheme_get_parent_id	(GtkSourceStyleScheme *scheme);
void			 _gtk_source_style_scheme_set_parent	(GtkSourceStyleScheme *scheme,
								 GtkSourceStyleScheme *parent_scheme);

G_END_DECLS

#endif  /* __GTK_SOURCE_STYLE_SCHEME_H__ */

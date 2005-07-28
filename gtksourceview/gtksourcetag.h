/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 *  gtksourcetag.h
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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

#ifndef __GTK_SOURCE_TAG_H__
#define __GTK_SOURCE_TAG_H__

#include <gtk/gtktexttag.h>
#include <gtksourceview/gtksourcetagstyle.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_TAG             (gtk_source_tag_get_type ())
#define GTK_SOURCE_TAG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTag))
#define GTK_SOURCE_TAG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))
#define GTK_IS_SOURCE_TAG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_TAG))
#define GTK_IS_SOURCE_TAG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_TAG))
#define GTK_SOURCE_TAG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))

typedef struct _GtkSourceTag        GtkSourceTag;
typedef struct _GtkSourceTagClass   GtkSourceTagClass;

GType              gtk_source_tag_get_type            (void) G_GNUC_CONST;

GtkTextTag        *gtk_source_tag_new                 (const gchar               *id,
						       const gchar               *name);
gchar             *gtk_source_tag_get_translated_name (GtkSourceTag		 *tag);
void               gtk_source_tag_set_translated_name (GtkSourceTag		 *tag,
						       const gchar               *tr_name);

gchar              *gtk_source_tag_get_id	(GtkSourceTag		 *tag);

GtkSourceTagStyle *gtk_source_tag_get_style           (GtkSourceTag              *tag);
void               gtk_source_tag_set_style           (GtkSourceTag              *tag,
						       const GtkSourceTagStyle   *style);

G_END_DECLS

#endif

/*  gtksourcetagtable.h
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

#ifndef GTK_SOURCE_TAG_TABLE_H
#define GTK_SOURCE_TAG_TABLE_H

#include <gtk/gtktexttagtable.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_TAG_TABLE            (gtk_source_tag_table_get_type ())
#define GTK_SOURCE_TAG_TABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_TAG_TABLE, GtkSourceTagTable))
#define GTK_SOURCE_TAG_TABLE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_TAG_TABLE, GtkSourceTagTableClass))
#define GTK_IS_SOURCE_TAG_TABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_TAG_TABLE))
#define GTK_IS_SOURCE_TAG_TABLE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_TAG_TABLE))
#define GTK_SOURCE_TAG_TABLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_TAG_TABLE, GtkSourceTagTableClass))

typedef struct _GtkSourceTagTable 		GtkSourceTagTable;
typedef struct _GtkSourceTagTablePrivate 	GtkSourceTagTablePrivate;

typedef struct _GtkSourceTagTableClass 		GtkSourceTagTableClass;

struct _GtkSourceTagTable
{
	GtkTextTagTable 	  parent_instance;

	GtkSourceTagTablePrivate *priv;
};

struct _GtkSourceTagTableClass
{
	GtkTextTagTableClass 	  parent_class;

	void (* changed) (GtkSourceTagTable *table);
	
  	/* Padding for future expansion */
  	void (*_gtk_source_reserved1) (void);
  	void (*_gtk_source_reserved2) (void);
};


GType			 gtk_source_tag_table_get_type	 		(void) G_GNUC_CONST;

GtkSourceTagTable 	*gtk_source_tag_table_new	 		(void);

void			 gtk_source_tag_table_add_tags	 		(GtkSourceTagTable *table, 
							  		 const GSList 	   *tags);

void			 gtk_source_tag_table_remove_source_tags 	(GtkSourceTagTable *table);

G_END_DECLS

#endif /* GTK_SOURCE_TAG_TABLE_H */


/*
 *  gtksourcefoldlabel.h
 *
 *  Copyright (C) 2005 - Jeroen Zwartepoorte <jeroen.zwartepoorte@gmail.com>
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

#ifndef __GTK_SOURCE_FOLD_LABEL_H__
#define __GTK_SOURCE_FOLD_LABEL_H__

#include <gtksourceview/gtksourceview.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_FOLD_LABEL		(_gtk_source_fold_label_get_type ())
#define GTK_SOURCE_FOLD_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_FOLD_LABEL, GtkSourceFoldLabel))
#define GTK_SOURCE_FOLD_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_FOLD_LABEL, GtkSourceFoldLabelClass))
#define GTK_IS_SOURCE_FOLD_LABEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_FOLD_LABEL))
#define GTK_IS_SOURCE_FOLD_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_FOLD_LABEL))
#define GTK_SOURCE_FOLD_LABEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_FOLD_LABEL, GtkSourceFoldLabelClass))

typedef struct _GtkSourceFoldLabel		GtkSourceFoldLabel;
typedef struct _GtkSourceFoldLabelClass		GtkSourceFoldLabelClass;
typedef struct _GtkSourceFoldLabelPrivate	GtkSourceFoldLabelPrivate;

struct _GtkSourceFoldLabel
{
	GtkLabel label;

	/*< private > */
	GtkSourceFoldLabelPrivate *priv;
};

struct _GtkSourceFoldLabelClass
{
	GtkLabelClass parent_class;

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) 	(void);
	void (*_gtk_source_reserved2) 	(void);
	void (*_gtk_source_reserved3) 	(void);
};


GType			 _gtk_source_fold_label_get_type	(void) G_GNUC_CONST;

GtkWidget		*_gtk_source_fold_label_new		(GtkSourceView      *view);

void			 _gtk_source_fold_label_get_position	(GtkSourceFoldLabel *label,
								 int                *x,
								 int                *y);
void			 _gtk_source_fold_label_set_position	(GtkSourceFoldLabel *label,
								 int                 x,
								 int                 y);

G_END_DECLS

#endif  /* __GTK_SOURCE_FOLD_LABEL_H__ */

/*  gtksourcefold.h
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

#ifndef __GTK_SOURCE_FOLD_H__
#define __GTK_SOURCE_FOLD_H__

#include <glib.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_FOLD		(gtk_source_fold_get_type ())

typedef struct _GtkSourceFold		GtkSourceFold;

#include <gtksourceview/gtksourcebuffer.h>

GType			 gtk_source_fold_get_type	(void) G_GNUC_CONST;

GtkSourceFold		*gtk_source_fold_copy		(const GtkSourceFold *fold);
void			 gtk_source_fold_free		(GtkSourceFold       *fold);

gboolean		 gtk_source_fold_get_folded	(GtkSourceFold       *fold);
void			 gtk_source_fold_set_folded	(GtkSourceFold       *fold,
							 gboolean             folded);

void			 gtk_source_fold_get_bounds	(GtkSourceFold       *fold,
							 GtkTextIter         *begin,
							 GtkTextIter         *end);

GtkSourceBuffer		*gtk_source_fold_get_buffer	(GtkSourceFold       *fold);

GtkSourceFold		*gtk_source_fold_get_parent	(GtkSourceFold       *fold);

const GList		*gtk_source_fold_get_children	(GtkSourceFold       *fold);

/* INTERNAL private stuff - not even exported from the library on
 * many platforms
 */
GtkSourceFold		*_gtk_source_fold_new		(GtkSourceBuffer     *buffer,
							 const GtkTextIter   *begin,
							 const GtkTextIter   *end);

G_END_DECLS

#endif  /* __GTK_SOURCE_FOLD_H__ */

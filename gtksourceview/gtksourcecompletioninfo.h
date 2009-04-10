/*
 * gtksourcecompletioninfo.h
 * This file is part of gtksourcecompletion
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GTK_SOURCE_COMPLETION_INFO_H
#define _GTK_SOURCE_COMPLETION_INFO_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_COMPLETION_INFO             (gtk_source_completion_info_get_type ())
#define GTK_SOURCE_COMPLETION_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_COMPLETION_INFO, GtkSourceCompletionInfo))
#define GTK_SOURCE_COMPLETION_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_COMPLETION_INFO, GtkSourceCompletionInfoClass)
#define GTK_IS_SOURCE_COMPLETION_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_COMPLETION_INFO))
#define GTK_SOURCE_COMPLETION_IS_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_COMPLETION_INFO))
#define GTK_SOURCE_COMPLETION_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_COMPLETION_INFO, GtkSourceCompletionInfoClass))

typedef struct _GtkSourceCompletionInfoPrivate GtkSourceCompletionInfoPrivate;

typedef struct _GtkSourceCompletionInfo GtkSourceCompletionInfo;

struct _GtkSourceCompletionInfo
{
	GtkWindow parent;
	
	GtkSourceCompletionInfoPrivate *priv;
};

typedef struct _GtkSourceCompletionInfoClass GtkSourceCompletionInfoClass;

struct _GtkSourceCompletionInfoClass
{
	GtkWindowClass parent_class;
};

GType		 gtk_source_completion_info_get_type		(void) G_GNUC_CONST;

GtkSourceCompletionInfo
		*gtk_source_completion_info_new		(void);

void		 gtk_source_completion_info_move_to_cursor	(GtkSourceCompletionInfo* self,
								 GtkTextView *view);

void		 gtk_source_completion_info_set_markup		(GtkSourceCompletionInfo* self,
								 const gchar* markup);

void		 gtk_source_completion_info_set_adjust_height	(GtkSourceCompletionInfo* self,
								 gboolean adjust,
								 gint max_height);

void		 gtk_source_completion_info_set_adjust_width	(GtkSourceCompletionInfo* self,
								 gboolean adjust,
								 gint max_width);

void		 gtk_source_completion_info_set_custom		(GtkSourceCompletionInfo* self,
								 GtkWidget *custom_widget);

GtkWidget	*gtk_source_completion_info_get_custom		(GtkSourceCompletionInfo* self);

G_END_DECLS

#endif

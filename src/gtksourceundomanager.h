/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gtksourceundomanager.h
 * This file is part of GtkSourceView
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi 
 * Copyright (C) 2002, 2003 Paolo Maggi 
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
 * Boston, MA 02111-1307, USA. * *
 */
 
#ifndef __GTK_SOURCE_UNDO_MANAGER_H__
#define __GTK_SOURCE_UNDO_MANAGER_H__

#include <gtk/gtktextbuffer.h>

#define GTK_SOURCE_TYPE_UNDO_MANAGER             	(gtk_source_undo_manager_get_type ())
#define GTK_SOURCE_UNDO_MANAGER(obj)			(GTK_CHECK_CAST ((obj), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManager))
#define GTK_SOURCE_UNDO_MANAGER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManagerClass))
#define GTK_SOURCE_IS_UNDO_MANAGER(obj)			(GTK_CHECK_TYPE ((obj), GTK_SOURCE_TYPE_UNDO_MANAGER))
#define GTK_SOURCE_IS_UNDO_MANAGER_CLASS(klass)  	(GTK_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_UNDO_MANAGER))
#define GTK_SOURCE_UNDO_MANAGER_GET_CLASS(obj)  	(GTK_CHECK_GET_CLASS ((obj), GTK_SOURCE_TYPE_UNDO_MANAGER, GtkSourceUndoManagerClass))


typedef struct _GtkSourceUndoManager        	GtkSourceUndoManager;
typedef struct _GtkSourceUndoManagerClass 	GtkSourceUndoManagerClass;

typedef struct _GtkSourceUndoManagerPrivate 	GtkSourceUndoManagerPrivate;

struct _GtkSourceUndoManager
{
	GObject base;
	
	GtkSourceUndoManagerPrivate *priv;
};

struct _GtkSourceUndoManagerClass
{
	GObjectClass parent_class;

	/* Signals */
	void (*can_undo) (GtkSourceUndoManager *um, gboolean can_undo);
    	void (*can_redo) (GtkSourceUndoManager *um, gboolean can_redo);
};

GType        		gtk_source_undo_manager_get_type	(void) G_GNUC_CONST;

GtkSourceUndoManager* 	gtk_source_undo_manager_new 		(GtkTextBuffer 		*buffer);

gboolean		gtk_source_undo_manager_can_undo	(const GtkSourceUndoManager *um);
gboolean		gtk_source_undo_manager_can_redo 	(const GtkSourceUndoManager *um);

void			gtk_source_undo_manager_undo 		(GtkSourceUndoManager 	*um);
void			gtk_source_undo_manager_redo 		(GtkSourceUndoManager 	*um);

void			gtk_source_undo_manager_begin_not_undoable_action 
								(GtkSourceUndoManager	*um);
void			gtk_source_undo_manager_end_not_undoable_action 
								(GtkSourceUndoManager	*um);

gint			gtk_source_undo_manager_get_max_undo_levels 
								(GtkSourceUndoManager 	*um);
void			gtk_source_undo_manager_set_max_undo_levels 
								(GtkSourceUndoManager 	*um,
				  	     			 gint		 	 undo_levels);

#endif /* __GTK_SOURCE_UNDO_MANAGER_H__ */



/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gtkundomanager.h
 * This file is part of gtksourceview, but copied from gtk
 *
 * Copyright (C) 1998, 1999 Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 Chema Celorio, Paolo Maggi 
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
 
/*
 * Modified by the gtk Team, 1998-2001. See the AUTHORS file for a 
 * list of people on the gtk Team.  
 * See the ChangeLog files for a list of changes. 
 */

#ifndef __GTK_UNDO_MANAGER_H__
#define __GTK_UNDO_MANAGER_H__

#include "gtksourcebuffer.h"

#define GTK_TYPE_UNDO_MANAGER			(gtk_undo_manager_get_type ())
#define GTK_UNDO_MANAGER(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_UNDO_MANAGER, GtkUndoManager))
#define GTK_UNDO_MANAGER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_UNDO_MANAGER, GtkUndoManagerClass))
#define GTK_IS_UNDO_MANAGER(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_UNDO_MANAGER))
#define GTK_IS_UNDO_MANAGER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_UNDO_MANAGER))
#define GTK_UNDO_MANAGER_GET_CLASS(obj)		(GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_UNDO_MANAGER, GtkUndoManagerClass))

typedef struct _GtkUndoManager			GtkUndoManager;
typedef struct _GtkUndoManagerClass		GtkUndoManagerClass;
typedef struct _GtkUndoManagerPrivate		GtkUndoManagerPrivate;

struct _GtkUndoManager {
	GObject base;

	GtkUndoManagerPrivate *priv;
};

struct _GtkUndoManagerClass {
	GObjectClass parent_class;

	/* Signals */
	void (*can_undo) (GtkUndoManager *um, gboolean can_undo);
	void (*can_redo) (GtkUndoManager *um, gboolean can_redo);
};

GType           gtk_undo_manager_get_type (void) G_GNUC_CONST;

GtkUndoManager *gtk_undo_manager_new      (GtkSourceBuffer *buffer);

gboolean        gtk_undo_manager_can_undo (const GtkUndoManager *um);
gboolean        gtk_undo_manager_can_redo (const GtkUndoManager *um);

void            gtk_undo_manager_undo     (GtkUndoManager *um);
void            gtk_undo_manager_redo     (GtkUndoManager *um);

void            gtk_undo_manager_begin_not_undoable_action (GtkUndoManager *um);
void            gtk_undo_manager_end_not_undoable_action   (GtkUndoManager *um);

#endif /* __GTK_UNDO_MANAGER_H__ */

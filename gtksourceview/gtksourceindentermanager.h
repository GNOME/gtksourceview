/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourceindentermanager.h
 *
 *  Copyright (C) 2009 - Ignacio Casal Quinteiro <icq@gnome.org>
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

#ifndef __GTK_SOURCE_INDENTER_MANAGER_H__
#define __GTK_SOURCE_INDENTER_MANAGER_H__

#include <glib-object.h>

#include "gtksourceindenter.h"

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_INDENTER_MANAGER		(gtk_source_indenter_manager_get_type ())
#define GTK_SOURCE_INDENTER_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_INDENTER_MANAGER, GtkSourceIndenterManager))
#define GTK_SOURCE_INDENTER_MANAGER_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_INDENTER_MANAGER, GtkSourceIndenterManager const))
#define GTK_SOURCE_INDENTER_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_INDENTER_MANAGER, GtkSourceIndenterManagerClass))
#define GTK_IS_SOURCE_INDENTER_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_INDENTER_MANAGER))
#define GTK_IS_SOURCE_INDENTER_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_INDENTER_MANAGER))
#define GTK_SOURCE_INDENTER_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_INDENTER_MANAGER, GtkSourceIndenterManagerClass))

typedef struct _GtkSourceIndenterManager	GtkSourceIndenterManager;
typedef struct _GtkSourceIndenterManagerClass	GtkSourceIndenterManagerClass;
typedef struct _GtkSourceIndenterManagerPrivate	GtkSourceIndenterManagerPrivate;

struct _GtkSourceIndenterManager
{
	GObject parent;
	
	GtkSourceIndenterManagerPrivate *priv;
};

struct _GtkSourceIndenterManagerClass
{
	GObjectClass parent_class;
};

GType				 gtk_source_indenter_manager_get_type	(void) G_GNUC_CONST;

GtkSourceIndenterManager	*gtk_source_indenter_manager_get_default(void);

GtkSourceIndenter		*gtk_source_indenter_manager_get_indenter_by_id
									(GtkSourceIndenterManager *manager,
									 const gchar *id);

G_END_DECLS

#endif /* __GTK_SOURCE_INDENTER_MANAGER_H__ */

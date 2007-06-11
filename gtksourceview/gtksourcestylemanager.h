/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestylemanager.h
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

#ifndef __GTK_SOURCE_STYLE_MANAGER_H__
#define __GTK_SOURCE_STYLE_MANAGER_H__

#include <gtksourceview/gtksourcestylescheme.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_STYLE_MANAGER			(gtk_source_style_manager_get_type ())
#define GTK_SOURCE_STYLE_MANAGER(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SOURCE_STYLE_MANAGER, GtkSourceStyleManager))
#define GTK_SOURCE_STYLE_MANAGER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SOURCE_STYLE_MANAGER, GtkSourceStyleManagerClass))
#define GTK_IS_SOURCE_STYLE_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SOURCE_STYLE_MANAGER))
#define GTK_IS_SOURCE_STYLE_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_STYLE_MANAGER))
#define GTK_SOURCE_STYLE_MANAGER_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SOURCE_STYLE_MANAGER, GtkSourceStyleManagerClass))


typedef struct _GtkSourceStyleManager		GtkSourceStyleManager;
typedef struct _GtkSourceStyleManagerClass	GtkSourceStyleManagerClass;
typedef struct _GtkSourceStyleManagerPrivate	GtkSourceStyleManagerPrivate;

struct _GtkSourceStyleManager
{
	GObject parent;

	GtkSourceStyleManagerPrivate *priv;
};

struct _GtkSourceStyleManagerClass
{
	GObjectClass parent_class;

	void (*changed) (GtkSourceStyleManager *mgr);

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) (void);
	void (*_gtk_source_reserved2) (void);
	void (*_gtk_source_reserved3) (void);
	void (*_gtk_source_reserved4) (void);
};


GType			 gtk_source_style_manager_get_type		(void) G_GNUC_CONST;

GtkSourceStyleManager	*gtk_source_style_manager_new			(void);
GtkSourceStyleManager	*gtk_source_style_manager_get_default		(void);

void			 gtk_source_style_manager_set_search_path	(GtkSourceStyleManager	*manager,
						    			 gchar	               **path);
gchar		       **gtk_source_style_manager_get_search_path	(GtkSourceStyleManager	*manager);

/* Do we need to add a GError? I don't think so, we can print a warning message on the terminal */
gboolean		 gtk_source_style_manager_add_scheme		(GtkSourceStyleManager	*manager,
						    			 const gchar		*filename);

/* Newly allocated list of schemes (to free: unref each one and call g_slist_free) */
GSList			*gtk_source_style_manager_list_schemes		(GtkSourceStyleManager	*manager);

GtkSourceStyleScheme	*gtk_source_style_manager_get_scheme		(GtkSourceStyleManager	*manager,
									 const gchar		*scheme_id);

G_END_DECLS

#endif /* __GTK_SOURCE_STYLE_MANAGER_H__ */


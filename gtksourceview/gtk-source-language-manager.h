/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GTK_SOURCE_LANGUAGE_MANAGER_H
#define GTK_SOURCE_LANGUAGE_MANAGER_H

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <glib-object.h>
#include <gtksourceview/gtk-source-types.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_LANGUAGE_MANAGER		(gtk_source_language_manager_get_type ())
#define GTK_SOURCE_LANGUAGE_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_SOURCE_TYPE_LANGUAGE_MANAGER, GtkSourceLanguageManager))
#define GTK_SOURCE_LANGUAGE_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), GTK_SOURCE_TYPE_LANGUAGE_MANAGER, GtkSourceLanguageManagerClass))
#define GTK_SOURCE_IS_LANGUAGE_MANAGER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_SOURCE_TYPE_LANGUAGE_MANAGER))
#define GTK_SOURCE_IS_LANGUAGE_MANAGER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_LANGUAGE_MANAGER))
#define GTK_SOURCE_LANGUAGE_MANAGER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GTK_SOURCE_TYPE_LANGUAGE_MANAGER, GtkSourceLanguageManagerClass))


typedef struct _GtkSourceLanguageManagerClass	GtkSourceLanguageManagerClass;
typedef struct _GtkSourceLanguageManagerPrivate GtkSourceLanguageManagerPrivate;

struct _GtkSourceLanguageManager
{
	GObject parent_instance;

	GtkSourceLanguageManagerPrivate *priv;
};

struct _GtkSourceLanguageManagerClass
{
	GObjectClass parent_class;

	/* Padding for future expansion */
	gpointer padding[10];
};

GTK_SOURCE_AVAILABLE_IN_ALL
GType			  gtk_source_language_manager_get_type			(void) G_GNUC_CONST;

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceLanguageManager *gtk_source_language_manager_new			(void);

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceLanguageManager *gtk_source_language_manager_get_default		(void);

GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const *	  gtk_source_language_manager_get_search_path		(GtkSourceLanguageManager *lm);

GTK_SOURCE_AVAILABLE_IN_ALL
void			  gtk_source_language_manager_set_search_path		(GtkSourceLanguageManager *lm,
										 gchar                   **dirs);

GTK_SOURCE_AVAILABLE_IN_ALL
const gchar * const *	  gtk_source_language_manager_get_language_ids		(GtkSourceLanguageManager *lm);

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceLanguage	 *gtk_source_language_manager_get_language		(GtkSourceLanguageManager *lm,
										 const gchar              *id);

GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceLanguage	 *gtk_source_language_manager_guess_language		(GtkSourceLanguageManager *lm,
										 const gchar		  *filename,
										 const gchar		  *content_type);

G_GNUC_INTERNAL
GtkSourceLanguageManager *_gtk_source_language_manager_peek_default		(void);

G_END_DECLS

#endif /* GTK_SOURCE_LANGUAGE_MANAGER_H */

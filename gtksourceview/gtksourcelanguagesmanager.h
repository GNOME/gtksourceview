/*  gtksourcelanguagesmanager.h
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

#ifndef __GTK_SOURCE_LANGUAGES_MANAGER_H__
#define __GTK_SOURCE_LANGUAGES_MANAGER_H__

#include <glib.h>

#include <gtksourceview/gtksourcelanguage.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_LANGUAGES_MANAGER		(gtk_source_languages_manager_get_type ())
#define GTK_SOURCE_LANGUAGES_MANAGER(obj)		(GTK_CHECK_CAST ((obj), GTK_TYPE_SOURCE_LANGUAGES_MANAGER, GtkSourceLanguagesManager))
#define GTK_SOURCE_LANGUAGES_MANAGER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_LANGUAGES_MANAGER, GtkSourceLanguagesManagerClass))
#define GTK_IS_SOURCE_LANGUAGES_MANAGER(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_SOURCE_LANGUAGES_MANAGER))
#define GTK_IS_SOURCE_LANGUAGES_MANAGER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_LANGUAGES_MANAGER))


typedef struct _GtkSourceLanguagesManager		GtkSourceLanguagesManager;
typedef struct _GtkSourceLanguagesManagerClass		GtkSourceLanguagesManagerClass;

typedef struct _GtkSourceLanguagesManagerPrivate	GtkSourceLanguagesManagerPrivate;

struct _GtkSourceLanguagesManager 
{
	GObject                   parent;

	GtkSourceLanguagesManagerPrivate *priv;
};

struct _GtkSourceLanguagesManagerClass 
{
	GObjectClass              parent_class;

	/* Padding for future expansion */
	void (*_gtk_source_reserved1) (void);
	void (*_gtk_source_reserved2) (void);
};


GType            gtk_source_languages_manager_get_type 		      (void) G_GNUC_CONST;

GtkSourceLanguagesManager 
		*gtk_source_languages_manager_new		      (void);


const GSList	*gtk_source_languages_manager_get_available_languages (GtkSourceLanguagesManager *lm);


GtkSourceLanguage 
		*gtk_source_languages_manager_get_language_from_mime_type 
								      (GtkSourceLanguagesManager *lm,
							  	       const gchar               *mime_type);

/* Property */
const GSList	*gtk_source_languages_manager_get_lang_files_dirs     (GtkSourceLanguagesManager *lm);

G_END_DECLS				

#endif /* __GTK_SOURCE_LANGUAGES_MANAGER_H__ */


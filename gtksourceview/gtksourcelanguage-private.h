/*  gtksourcelanguage-private.h
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

#ifndef __GTK_SOURCE_LANGUAGE_PRIVATE_H__
#define __GTK_SOURCE_LANGUAGE_PRIVATE_H__

#include <glib.h>
#include "gtksourcesimpleengine.h"
#include "gtksourcecontextengine.h"
#include "gtksourcelanguagesmanager.h"

G_BEGIN_DECLS

struct _GtkSourceLanguagePrivate 
{
	gchar			*lang_file_name;
	/* this is allocated by libxml, it should be freed using xmlFree() */
	gchar                   *translation_domain;

	gchar			*id;
	
	/* this is allocated by libxml, it should be freed using xmlFree() */
	gchar			*name;
	/* this is allocated by libxml, it should be freed using xmlFree() */
	gchar			*section;

	gint                     version;
	
	GSList			*mime_types;

	/* this maps from style names to their parent styles */
	GHashTable		*tag_id_to_style_name;
	/* this maps style names to GtkSourceTagStyle structs */
	GHashTable		*tag_id_to_style;

	GtkSourceStyleScheme 	*style_scheme;

	GtkSourceLanguagesManager *languages_manager;
};

GtkSourceLanguage *_gtk_source_language_new_from_file (const gchar			*filename,
						       GtkSourceLanguagesManager	*lm);
gchar *_gtk_source_language_strconvescape (gchar *source);

GtkSourceLanguagesManager *gtk_source_language_get_languages_manager (GtkSourceLanguage *language);

gboolean _gtk_source_language_file_parse_version1 (GtkSourceLanguage      *language,
						   GSList                **tags,
						   GtkSourceSimpleEngine  *engine,
						   gboolean                populate_styles_table);

gboolean _gtk_source_language_file_parse_version2 (GtkSourceLanguage      *language,
						   GSList                **tags,
						   GtkSourceContextEngine *engine,
						   gboolean                populate_styles_table);

G_END_DECLS

#endif  /* __GTK_SOURCE_LANGUAGE_PRIVATE_H__ */


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcelanguage.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <fcntl.h>

#include <libxml/xmlreader.h>
#include <glib/gstdio.h>
#include "gtksourceview-i18n.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcelanguage.h"
#include "gtksourceview-marshal.h"

#define DEFAULT_SECTION _("Others")

G_DEFINE_TYPE (GtkSourceLanguage, gtk_source_language, G_TYPE_OBJECT)

static GtkSourceLanguage *process_language_node 		(xmlTextReaderPtr 		 reader,
								 const gchar 			*filename);


GtkSourceLanguage *
_gtk_source_language_new_from_file (const gchar              *filename,
				    GtkSourceLanguageManager *lm)
{
	GtkSourceLanguage *lang = NULL;
	xmlTextReaderPtr reader = NULL;
	gint ret;
	gint fd;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (lm != NULL, NULL);

	/*
	 * Use fd instead of filename so that it's utf8 safe on w32.
	 */
	fd = g_open (filename, O_RDONLY, 0);
	if (fd != -1)
		reader = xmlReaderForFd (fd, filename, NULL, 0);

	if (reader != NULL)
	{
        	ret = xmlTextReaderRead (reader);

        	while (ret == 1)
		{
			if (xmlTextReaderNodeType (reader) == 1)
			{
				xmlChar *name;

				name = xmlTextReaderName (reader);

				if (xmlStrcmp (name, BAD_CAST "language") == 0)
				{
					lang = process_language_node (reader, filename);
					ret = 0;
				}

				xmlFree (name);
			}

			if (ret == 1)
				ret = xmlTextReaderRead (reader);
		}

		xmlFreeTextReader (reader);
		close (fd);

		if (ret != 0)
		{
	            g_warning("Failed to parse '%s'", filename);
		    return NULL;
		}
        }
	else
	{
		g_warning("Unable to open '%s'", filename);

    	}

	if (lang != NULL)
	{
		lang->priv->language_manager = lm;
		g_object_add_weak_pointer (G_OBJECT (lm),
					   (gpointer*) &lang->priv->language_manager);
	}

	return lang;
}

static void
gtk_source_language_dispose (GObject *object)
{
	GtkSourceLanguage *lang;

	lang = GTK_SOURCE_LANGUAGE (object);

	if (lang->priv->language_manager != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (lang->priv->language_manager),
					      (gpointer*) &lang->priv->language_manager);
		lang->priv->language_manager = NULL;
	}

	G_OBJECT_CLASS (gtk_source_language_parent_class)->finalize (object);
}

static void
gtk_source_language_finalize (GObject *object)
{
	GtkSourceLanguage *lang;

	lang = GTK_SOURCE_LANGUAGE (object);

	if (lang->priv->ctx_data != NULL)
		g_critical ("context data not freed in gtk_source_language_finalize");

	g_free (lang->priv->lang_file_name);
	g_free (lang->priv->translation_domain);
	g_free (lang->priv->name);
	g_free (lang->priv->section);
	g_free (lang->priv->id);
	g_hash_table_destroy (lang->priv->properties);
	g_hash_table_destroy (lang->priv->styles);

	G_OBJECT_CLASS (gtk_source_language_parent_class)->finalize (object);
}

static void
gtk_source_language_class_init (GtkSourceLanguageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose	= gtk_source_language_dispose;
	object_class->finalize	= gtk_source_language_finalize;

	g_type_class_add_private (object_class, sizeof(GtkSourceLanguagePrivate));
}

static void
gtk_source_language_init (GtkSourceLanguage *lang)
{
	lang->priv = G_TYPE_INSTANCE_GET_PRIVATE (lang, GTK_TYPE_SOURCE_LANGUAGE,
						  GtkSourceLanguagePrivate);
	lang->priv->styles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	lang->priv->properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static gboolean
string_to_bool (const gchar *string)
{
	if (!g_ascii_strcasecmp (string, "yes") ||
	    !g_ascii_strcasecmp (string, "true") ||
	    !g_ascii_strcasecmp (string, "1"))
		return TRUE;
	else if (!g_ascii_strcasecmp (string, "no") ||
		 !g_ascii_strcasecmp (string, "false") ||
		 !g_ascii_strcasecmp (string, "0"))
		return FALSE;
	else
		g_return_val_if_reached (FALSE);
}

static void
process_properties (xmlTextReaderPtr   reader,
		    GtkSourceLanguage *language)
{
	gint ret;
	xmlNodePtr child;
	xmlNodePtr node = NULL;

	while (node == NULL && (ret = xmlTextReaderRead (reader)) == 1)
	{
		xmlChar *name;

		if (xmlTextReaderNodeType (reader) != 1)
			continue;

		name = xmlTextReaderName (reader);

		if (xmlStrcmp (name, BAD_CAST "metadata") != 0)
		{
			xmlFree (name);
			continue;
		}

		xmlFree (name);

		node = xmlTextReaderExpand (reader);

		if (node == NULL)
			return;
	}

	if (node == NULL)
		return;

	for (child = node->children; child != NULL; child = child->next)
	{
		xmlChar *name;
		xmlChar *content;

		if (child->type != XML_ELEMENT_NODE ||
		    xmlStrcmp (child->name, BAD_CAST "property") != 0)
			continue;

		name = xmlGetProp (child, BAD_CAST "name");
		content = xmlNodeGetContent (child);

		if (name != NULL && content != NULL)
			g_hash_table_insert (language->priv->properties,
					     g_strdup ((gchar *) name),
					     g_strdup ((gchar *) content));

		xmlFree (name);
		xmlFree (content);
	}
}

static GtkSourceLanguage *
process_language_node (xmlTextReaderPtr reader, const gchar *filename)
{
	xmlChar *version;
	xmlChar *tmp;
	xmlChar *untranslated_name;
	GtkSourceLanguage *lang;

	lang = g_object_new (GTK_TYPE_SOURCE_LANGUAGE, NULL);

	lang->priv->lang_file_name = g_strdup (filename);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "translation-domain");
	lang->priv->translation_domain = g_strdup ((gchar*) tmp);
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "hidden");
	if (tmp != NULL)
		lang->priv->hidden = string_to_bool ((gchar*) tmp);
	else
		lang->priv->hidden = FALSE;
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "mimetypes");
	if (tmp != NULL)
		g_hash_table_insert (lang->priv->properties,
				     g_strdup ("mimetypes"),
				     g_strdup ((char*) tmp));
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "globs");
	if (tmp != NULL)
		g_hash_table_insert (lang->priv->properties,
				     g_strdup ("globs"),
				     g_strdup ((char*) tmp));
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "_name");
	if (tmp == NULL)
	{
		tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "name");

		if (tmp == NULL)
		{
			g_warning ("Impossible to get language name from file '%s'",
				   filename);
			g_object_unref (lang);
			return NULL;
		}

		lang->priv->name = g_strdup ((char*) tmp);
		untranslated_name = tmp;
	}
	else
	{
		lang->priv->name = _gtk_source_language_translate_string (lang, (gchar*) tmp);
		untranslated_name = tmp;
	}

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "id");
	if (tmp != NULL)
	{
		lang->priv->id = g_ascii_strdown ((gchar*) tmp, -1);
	}
	else
	{
		lang->priv->id = g_ascii_strdown ((gchar*) untranslated_name, -1);
	}
	xmlFree (tmp);
	xmlFree (untranslated_name);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "_section");
	if (tmp == NULL)
	{
		tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "section");

		if (tmp == NULL)
			lang->priv->section = g_strdup (DEFAULT_SECTION);
		else
			lang->priv->section = g_strdup ((gchar *) tmp);

		xmlFree (tmp);
	}
	else
	{
		lang->priv->section = _gtk_source_language_translate_string (lang, (gchar*) tmp);
		xmlFree (tmp);
	}

	version = xmlTextReaderGetAttribute (reader, BAD_CAST "version");

	if (version == NULL)
	{
		g_warning ("Impossible to get version number from file '%s'",
			   filename);
		g_object_unref (lang);
		return NULL;
	}

	if (xmlStrcmp (version , BAD_CAST "1.0") == 0)
	{
		lang->priv->version = GTK_SOURCE_LANGUAGE_VERSION_1_0;
	}
	else if (xmlStrcmp (version, BAD_CAST "2.0") == 0)
	{
		lang->priv->version = GTK_SOURCE_LANGUAGE_VERSION_2_0;
	}
	else
	{
		g_warning ("Unsupported language spec version '%s' in file '%s'",
			   (gchar*) version, filename);
		xmlFree (version);
		g_object_unref (lang);
		return NULL;
	}

	xmlFree (version);

	if (lang->priv->version == GTK_SOURCE_LANGUAGE_VERSION_2_0)
		process_properties (reader, lang);

	return lang;
}

gchar *
_gtk_source_language_translate_string (GtkSourceLanguage *language,
				       const gchar       *string)
{
	g_return_val_if_fail (string != NULL, NULL);
	return GD_(language->priv->translation_domain, string);
}

/**
 * gtk_source_language_get_id:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the ID of the language. The ID is not locale-dependent.
 *
 * Returns: the ID of @language.
 * The returned string is owned by @language and should not be freed
 * or modified.
 **/
const gchar *
gtk_source_language_get_id (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->priv->id != NULL, NULL);

	return language->priv->id;
}

/**
 * gtk_source_language_get_name:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the localized name of the language.
 *
 * Returns: the name of @language.
 * The returned string is owned by @language and should not be freed
 * or modified.
 **/
const gchar *
gtk_source_language_get_name (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->priv->name != NULL, NULL);

	return language->priv->name;
}

/**
 * gtk_source_language_get_section:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the localized section of the language.
 * Each language belong to a section (ex. HTML belogs to the
 * Markup section).
 *
 * Returns: the section of @language.
 * The returned string is owned by @language and should not be freed
 * or modified.
 **/
const gchar *
gtk_source_language_get_section	(GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->priv->section != NULL, NULL);

	return language->priv->section;
}

/**
 * gtk_source_language_get_metadata:
 * @language: a #GtkSourceLanguage.
 * @name: metadata property name.
 *
 * Returns: value of property @name stored in the metadata of @language
 * or %NULL if language doesn't contain that metadata property.
 * The returned string is owned by @language and should not be freed
 * or modified.
 **/
const gchar *
gtk_source_language_get_metadata (GtkSourceLanguage *language,
				  const gchar       *name)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (language->priv->properties, name);
}

/**
 * gtk_source_language_get_mime_types:
 *
 * @language: a #GtkSourceLanguage.
 *
 * Returns the mime types associated to this language. This is just
 * an utility wrapper around gtk_source_language_get_metadata() to
 * retrieve the "mimetypes" metadata property and split it into an
 * array.
 *
 * Returns: a newly-allocated %NULL terminated array containing
 * the mime types or %NULL if no mime types are found.
 * The returned array must be freed with g_strfreev().
 **/
gchar **
gtk_source_language_get_mime_types (GtkSourceLanguage *language)
{
	const gchar *mimetypes;

	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);

	mimetypes = gtk_source_language_get_metadata (language, "mimetypes");
	if (mimetypes == NULL)
		return NULL;

	return g_strsplit (mimetypes, ";", 0);
}

/**
 * gtk_source_language_get_globs:
 *
 * @language: a #GtkSourceLanguage.
 *
 * Returns the globs associated to this language. This is just
 * an utility wrapper around gtk_source_language_get_metadata() to
 * retrieve the "globs" metadata property and split it into an array.
 *
 * Returns: a newly-allocated %NULL terminated array containing
 * the globs or %NULL if no globs are found.
 * The returned array must be freed with g_strfreev().
 **/
gchar **
gtk_source_language_get_globs (GtkSourceLanguage *language)
{
	const gchar *globs;

	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);

	globs = gtk_source_language_get_metadata (language, "globs");
	if (globs == NULL)
		return NULL;

	return g_strsplit (globs, ";", 0);
}

/**
 * _gtk_source_language_get_language_manager:
 * @language: a #GtkSourceLanguage.
 *
 * Returns: #GtkSourceLanguageManager for @language.
 **/
GtkSourceLanguageManager *
_gtk_source_language_get_language_manager (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->priv->id != NULL, NULL);

	return language->priv->language_manager;
}

/* Highlighting engine creation ------------------------------------------ */

void
_gtk_source_language_define_language_styles (GtkSourceLanguage *lang)
{
#define ADD_ALIAS(style,mapto)								\
	g_hash_table_insert (lang->priv->styles, g_strdup (style), g_strdup (mapto))

	ADD_ALIAS ("Base-N Integer", "def:base-n-integer");
	ADD_ALIAS ("Character", "def:character");
	ADD_ALIAS ("Comment", "def:comment");
	ADD_ALIAS ("Function", "def:function");
	ADD_ALIAS ("Decimal", "def:decimal");
	ADD_ALIAS ("Floating Point", "def:floating-point");
	ADD_ALIAS ("Keyword", "def:keyword");
	ADD_ALIAS ("Preprocessor", "def:preprocessor");
	ADD_ALIAS ("String", "def:string");
	ADD_ALIAS ("Specials", "def:specials");
	ADD_ALIAS ("Data Type", "def:data-type");

#undef ADD_ALIAS
}

GtkSourceEngine *
_gtk_source_language_create_engine (GtkSourceLanguage *language)
{
	GtkSourceContextEngine *ce = NULL;

	if (language->priv->ctx_data == NULL)
	{
		gboolean success = FALSE;
		GtkSourceContextData *ctx_data;

		if (language->priv->language_manager == NULL)
		{
			g_critical ("_gtk_source_language_create_engine() is called after "
				    "language manager was finalized");
		}
		else
		{
			ctx_data = _gtk_source_context_data_new	(language);

			switch (language->priv->version)
			{
				case GTK_SOURCE_LANGUAGE_VERSION_1_0:
					success = _gtk_source_language_file_parse_version1 (language, ctx_data);
					break;

				case GTK_SOURCE_LANGUAGE_VERSION_2_0:
					success = _gtk_source_language_file_parse_version2 (language, ctx_data);
					break;
			}

			if (!success)
				_gtk_source_context_data_unref (ctx_data);
			else
				language->priv->ctx_data = ctx_data;
		}
	}
	else
	{
		_gtk_source_context_data_ref (language->priv->ctx_data);
	}

	if (language->priv->ctx_data)
	{
		ce = _gtk_source_context_engine_new (language->priv->ctx_data);
		_gtk_source_context_data_unref (language->priv->ctx_data);
	}

	return ce ? GTK_SOURCE_ENGINE (ce) : NULL;
}

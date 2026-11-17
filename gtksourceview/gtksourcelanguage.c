/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003 - Paolo Maggi <paolo.maggi@polito.it>
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

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <fcntl.h>

#include <libxml/xmlreader.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include "gtksourcelanguage-private.h"
#include "gtksourcelanguage.h"
#include "gtksourceutils-private.h"

#ifdef G_OS_WIN32
#include <io.h>
#endif

/**
 * GtkSourceLanguage:
 * 
 * Represents a syntax highlighted language.
 *
 * A `GtkSourceLanguage` represents a programming or markup language, affecting
 * syntax highlighting and [context classes](./class.Buffer.html#context-classes).
 *
 * Use [class@LanguageManager] to obtain a `GtkSourceLanguage` instance, and
 * [method@Buffer.set_language] to apply it to a [class@Buffer].
 *
 * Since 5.24, assigning a language to a buffer prepares it asynchronously.
 * Use [method@Language.prepare_async] to wait until its highlighting data and
 * styles are available. Style queries return %NULL while preparation is pending;
 * when no preparation is pending, they can load the language synchronously.
 */

#define DEFAULT_SECTION _("Others")

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_SECTION,
	PROP_HIDDEN
};

struct _GtkSourceLanguage
{
	GObject                   parent_instance;

	gchar                    *lang_file_name;
	gchar                    *translation_domain;

	gchar                    *id;
	gchar                    *name;
	gchar                    *section;

	/* Maps ids to GtkSourceStyleInfo objects */
	/* Names of styles defined in other lang files are not stored */
	GHashTable               *styles;
	gboolean                  styles_loaded;

	gint                      version;
	gboolean                  hidden;

	GHashTable               *properties;

	GtkSourceLanguageManager *language_manager;

	GtkSourceContextData     *ctx_data;
	GPtrArray                *prepare_tasks;
};

G_DEFINE_TYPE (GtkSourceLanguage, gtk_source_language, G_TYPE_OBJECT)

static GtkSourceLanguage *process_language_node (xmlTextReaderPtr   reader,
                                                 const gchar       *filename);
static gboolean           force_styles          (GtkSourceLanguage *language);

GtkSourceLanguage *
_gtk_source_language_new_from_file (const gchar              *filename,
                                    GtkSourceLanguageManager *lm)
{
	GtkSourceLanguage *lang = NULL;
	xmlTextReaderPtr reader = NULL;
	GBytes *bytes = NULL;
	gint ret;
	gint fd = -1;

	g_return_val_if_fail (filename != NULL, NULL);
	g_return_val_if_fail (lm != NULL, NULL);

	if (g_str_has_prefix (filename, "resource://"))
	{
		const char *path = filename + strlen ("resource://");

		bytes = g_resources_lookup_data (path, 0, NULL);

		if (bytes != NULL)
		{
			reader = xmlReaderForMemory (g_bytes_get_data (bytes, NULL),
			                             g_bytes_get_size (bytes),
			                             NULL, NULL, 0);
		}
	}
	else
	{
		/*
		 * Use fd instead of filename so that it's utf8 safe on w32.
		 */
		fd = g_open (filename, O_RDONLY, 0);
		if (fd != -1)
			reader = xmlReaderForFd (fd, filename, NULL, 0);
	}

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

		if (ret != 0)
		{
	            g_warning ("Failed to parse '%s'", filename);
		}
        }
	else
	{
		g_warning ("Unable to open '%s'", filename);
    	}

	if (lang != NULL)
	{
		lang->language_manager = lm;
		g_object_add_weak_pointer (G_OBJECT (lm),
					   (gpointer) &lang->language_manager);
	}

	if (reader != NULL)
	{
		xmlFreeTextReader (reader);
	}

	if (fd != -1)
	{
		close (fd);
	}

	if (bytes != NULL)
	{
		g_bytes_unref (bytes);
	}

	return lang;
}

static void
gtk_source_language_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
	GtkSourceLanguage *language;

	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE (object));

	language = GTK_SOURCE_LANGUAGE (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, language->id);
			break;

		case PROP_NAME:
			g_value_set_string (value, language->name);
			break;

		case PROP_SECTION:
			g_value_set_string (value, language->section);
			break;

		case PROP_HIDDEN:
			g_value_set_boolean (value, language->hidden);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_language_dispose (GObject *object)
{
	GtkSourceLanguage *lang;

	lang = GTK_SOURCE_LANGUAGE (object);

	if (lang->language_manager != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (lang->language_manager),
					      (gpointer) &lang->language_manager);
		lang->language_manager = NULL;
	}

	G_OBJECT_CLASS (gtk_source_language_parent_class)->dispose (object);
}

static void
gtk_source_language_finalize (GObject *object)
{
	GtkSourceLanguage *lang;

	lang = GTK_SOURCE_LANGUAGE (object);

	if (lang->ctx_data != NULL)
	{
		_gtk_source_context_data_set_language (lang->ctx_data, NULL);
		g_clear_pointer (&lang->ctx_data, _gtk_source_context_data_unref);
	}

	g_assert (lang->prepare_tasks == NULL);

	g_free (lang->lang_file_name);
	g_free (lang->translation_domain);
	g_free (lang->name);
	g_free (lang->section);
	g_free (lang->id);
	g_hash_table_destroy (lang->properties);

	g_hash_table_destroy (lang->styles);

	G_OBJECT_CLASS (gtk_source_language_parent_class)->finalize (object);
}

static void
gtk_source_language_class_init (GtkSourceLanguageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_language_get_property;
	object_class->dispose = gtk_source_language_dispose;
	object_class->finalize = gtk_source_language_finalize;

	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
						 	      "Language id",
							      "Language id",
							      NULL,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
						 	      "Language name",
							      "Language name",
							      NULL,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_SECTION,
					 g_param_spec_string ("section",
						 	      "Language section",
							      "Language section",
							      NULL,
							      G_PARAM_READABLE |
							      G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (object_class,
					 PROP_HIDDEN,
					 g_param_spec_boolean ("hidden",
							       "Hidden",
							       "Whether the language should be hidden from the user",
							       FALSE,
							       G_PARAM_READABLE |
							       G_PARAM_STATIC_STRINGS));
}

static void
gtk_source_language_init (GtkSourceLanguage *lang)
{
	lang = gtk_source_language_get_instance_private (lang);

	lang->styles = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    g_free,
						    (GDestroyNotify)_gtk_source_style_info_free);
	lang->properties = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
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
	xmlNodePtr child;
	xmlNodePtr node = NULL;

	while (node == NULL && xmlTextReaderRead (reader) == 1)
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
			g_hash_table_insert (language->properties,
					     g_strdup ((gchar *) name),
					     g_strdup ((gchar *) content));

		xmlFree (name);
		xmlFree (content);
	}
}

static GtkSourceLanguage *
process_language_node (xmlTextReaderPtr  reader,
                       const gchar      *filename)
{
	xmlChar *version;
	xmlChar *tmp;
	xmlChar *untranslated_name;
	GtkSourceLanguage *lang;

	lang = g_object_new (GTK_SOURCE_TYPE_LANGUAGE, NULL);

	lang->lang_file_name = g_strdup (filename);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "translation-domain");
	lang->translation_domain = g_strdup ((gchar*) tmp);
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "hidden");
	if (tmp != NULL)
		lang->hidden = string_to_bool ((gchar*) tmp);
	else
		lang->hidden = FALSE;
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "mimetypes");
	if (tmp != NULL)
		g_hash_table_insert (lang->properties,
				     g_strdup ("mimetypes"),
				     g_strdup ((char*) tmp));
	xmlFree (tmp);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "globs");
	if (tmp != NULL)
		g_hash_table_insert (lang->properties,
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

		lang->name = g_strdup ((char*) tmp);
		untranslated_name = tmp;
	}
	else
	{
		lang->name = _gtk_source_language_translate_string (lang, (gchar*) tmp);
		untranslated_name = tmp;
	}

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "id");
	if (tmp != NULL)
	{
		lang->id = g_ascii_strdown ((gchar*) tmp, -1);
	}
	else
	{
		lang->id = g_ascii_strdown ((gchar*) untranslated_name, -1);
	}
	xmlFree (tmp);
	xmlFree (untranslated_name);

	tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "_section");
	if (tmp == NULL)
	{
		tmp = xmlTextReaderGetAttribute (reader, BAD_CAST "section");

		if (tmp == NULL)
			lang->section = g_strdup (DEFAULT_SECTION);
		else
			lang->section = g_strdup ((gchar *) tmp);

		xmlFree (tmp);
	}
	else
	{
		lang->section = _gtk_source_language_translate_string (lang, (gchar*) tmp);
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
		lang->version = GTK_SOURCE_LANGUAGE_VERSION_1_0;
	}
	else if (xmlStrcmp (version, BAD_CAST "2.0") == 0)
	{
		lang->version = GTK_SOURCE_LANGUAGE_VERSION_2_0;
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

	if (lang->version == GTK_SOURCE_LANGUAGE_VERSION_2_0)
		process_properties (reader, lang);

	return lang;
}

gchar *
_gtk_source_language_translate_string (GtkSourceLanguage *language,
                                       const gchar       *string)
{
	g_return_val_if_fail (string != NULL, NULL);

	return _gtk_source_utils_dgettext (language->translation_domain, string);
}

/**
 * gtk_source_language_get_id:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the ID of the language. 
 * 
 * The ID is not locale-dependent.The returned string is owned by @language 
 * and should not be freed or modified.
 *
 * Returns: the ID of @language.
 **/
const gchar *
gtk_source_language_get_id (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->id != NULL, NULL);

	return language->id;
}

/**
 * gtk_source_language_get_name:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the localized name of the language.
 *
 * The returned string is owned by @language and should not be freed
 * or modified.
 *
 * Returns: the name of @language.
 **/
const gchar *
gtk_source_language_get_name (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->name != NULL, NULL);

	return language->name;
}

/**
 * gtk_source_language_get_section:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the localized section of the language.
 *
 * Each language belong to a section (ex. HTML belongs to the
 * Markup section).
 * The returned string is owned by @language and should not be freed
 * or modified.
 *
 * Returns: the section of @language.
 **/
const gchar *
gtk_source_language_get_section (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->section != NULL, NULL);

	return language->section;
}

/**
 * gtk_source_language_get_hidden:
 * @language: a #GtkSourceLanguage
 *
 * Returns whether the language should be hidden from the user.
 *
 * Returns: %TRUE if the language should be hidden, %FALSE otherwise.
 */
gboolean
gtk_source_language_get_hidden (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), FALSE);

	return language->hidden;
}

/**
 * gtk_source_language_get_metadata:
 * @language: a #GtkSourceLanguage.
 * @name: metadata property name.
 *
 * Returns: (nullable) (transfer none): value of property @name stored in
 * the metadata of @language or %NULL if language does not contain the
 * specified metadata property.
 * The returned string is owned by @language and should not be freed
 * or modified.
 **/
const gchar *
gtk_source_language_get_metadata (GtkSourceLanguage *language,
                                  const gchar       *name)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (language->properties, name);
}

/**
 * gtk_source_language_get_mime_types:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the mime types associated to this language. 
 * 
 * This is just an utility wrapper around [method@Language.get_metadata] to
 * retrieve the "mimetypes" metadata property and split it into an
 * array.
 *
 * Returns: (nullable) (array zero-terminated=1) (transfer full):
 * a newly-allocated %NULL terminated array containing the mime types
 * or %NULL if no mime types are found.
 * The returned array must be freed with g_strfreev().
 **/
gchar **
gtk_source_language_get_mime_types (GtkSourceLanguage *language)
{
	const gchar *mimetypes;

	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

	mimetypes = gtk_source_language_get_metadata (language, "mimetypes");
	if (mimetypes == NULL)
		return NULL;

	return g_strsplit (mimetypes, ";", 0);
}

/**
 * gtk_source_language_get_globs:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the globs associated to this language. 
 * 
 * This is just an utility wrapper around [method@Language.get_metadata] to
 * retrieve the "globs" metadata property and split it into an array.
 *
 * Returns: (nullable) (array zero-terminated=1) (transfer full):
 * a newly-allocated %NULL terminated array containing the globs or %NULL
 * if no globs are found.
 * The returned array must be freed with g_strfreev().
 **/
gchar **
gtk_source_language_get_globs (GtkSourceLanguage *language)
{
	const gchar *globs;

	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

	globs = gtk_source_language_get_metadata (language, "globs");
	if (globs == NULL)
		return NULL;

	return g_strsplit (globs, ";", 0);
}

/**
 * _gtk_source_language_get_language_manager:
 * @language: a #GtkSourceLanguage.
 *
 * Returns: (transfer none): the #GtkSourceLanguageManager for @language.
 **/
GtkSourceLanguageManager *
_gtk_source_language_get_language_manager (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->id != NULL, NULL);

	return language->language_manager;
}

/* Highlighting engine creation ------------------------------------------ */

static void
copy_style_info (const char         *style_id,
                 GtkSourceStyleInfo *info,
                 GHashTable         *dest)
{
	g_hash_table_insert (dest, g_strdup (style_id),
			     _gtk_source_style_info_copy (info));
}

void
_gtk_source_language_define_language_styles (GtkSourceLanguage *lang)
{
	static const gchar *alias[][2] = {
		{"Base-N Integer", "def:base-n-integer"},
		{"Character", "def:character"},
		{"Comment", "def:comment"},
		{"Function", "def:function"},
		{"Decimal", "def:decimal"},
		{"Floating Point", "def:floating-point"},
		{"Keyword", "def:keyword"},
		{"Preprocessor", "def:preprocessor"},
		{"String", "def:string"},
		{"Specials", "def:specials"},
		{"Data Type", "def:type"},
		{NULL, NULL}};

	gint i = 0;
	GtkSourceLanguageManager *lm;
	GtkSourceLanguage *def_lang;

	while (alias[i][0] != NULL)
	{
		GtkSourceStyleInfo *info;

		info = _gtk_source_style_info_new (alias[i][0], alias[i][1]);

		g_hash_table_insert (lang->styles,
				     g_strdup (alias[i][0]),
				     info);

		++i;
	}

	/* We translate String to def:string, but def:string is mapped-to
	 * def:constant in def.lang, so we got to take style mappings from def.lang */

	lm = _gtk_source_language_get_language_manager (lang);
	def_lang = gtk_source_language_manager_get_language (lm, "def");

	if (def_lang != NULL)
	{
		force_styles (def_lang);
		g_hash_table_foreach (def_lang->styles,
				      (GHFunc) copy_style_info,
				      lang->styles);
	}
}

typedef struct
{
	GtkSourceLanguageDefinition  definition;
	GtkSourceContextData        *ctx_data;
	GHashTable                  *styles;
} LanguagePreparation;

static void
language_preparation_free (LanguagePreparation *preparation)
{
	g_clear_pointer (&preparation->ctx_data, _gtk_source_context_data_unref);
	g_clear_pointer (&preparation->styles, g_hash_table_unref);
	g_clear_pointer (&preparation->definition.filenames, g_hash_table_unref);
	g_free (preparation->definition.id);
	g_free (preparation->definition.filename);
	g_free (preparation->definition.translation_domain);
	g_free (preparation);
}

static LanguagePreparation *
language_preparation_new (GtkSourceLanguage  *language,
			  GError            **error)
{
	LanguagePreparation *preparation;
	const char * const *ids;

	if (language->language_manager == NULL)
	{
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_CLOSED,
				     "The language manager has been finalized");
		return NULL;
	}

	if (language->version != GTK_SOURCE_LANGUAGE_VERSION_2_0)
	{
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "Only language definition version 2 is supported");
		return NULL;
	}

	preparation = g_new0 (LanguagePreparation, 1);
	preparation->definition.id = g_strdup (language->id);
	preparation->definition.filename = g_strdup (language->lang_file_name);
	preparation->definition.translation_domain = g_strdup (language->translation_domain);
	preparation->definition.filenames = g_hash_table_new_full (g_str_hash, g_str_equal,
								   g_free, g_free);
	preparation->ctx_data = _gtk_source_context_data_new (language->id);

	ids = gtk_source_language_manager_get_language_ids (language->language_manager);

	for (guint i = 0; ids != NULL && ids[i] != NULL; i++)
	{
		GtkSourceLanguage *dependency;

		dependency = gtk_source_language_manager_get_language (language->language_manager, ids[i]);
		g_hash_table_insert (preparation->definition.filenames,
				     g_strdup (ids[i]),
				     g_strdup (dependency->lang_file_name));
	}

	return preparation;
}

static void
language_preparation_install (GtkSourceLanguage   *language,
			      LanguagePreparation *preparation)
{
	g_assert (language->ctx_data == NULL);
	g_assert (preparation->ctx_data != NULL);
	g_assert (preparation->styles != NULL);

	language->ctx_data = g_steal_pointer (&preparation->ctx_data);
	_gtk_source_context_data_set_language (language->ctx_data, language);

	if (!language->styles_loaded)
	{
		g_clear_pointer (&language->styles, g_hash_table_unref);
		language->styles = g_steal_pointer (&preparation->styles);
		language->styles_loaded = TRUE;
	}
}

static GtkSourceContextData *
gtk_source_language_parse_file (GtkSourceLanguage *language)
{
	if (language->ctx_data == NULL && language->prepare_tasks == NULL)
	{
		LanguagePreparation *preparation;
		GError *error = NULL;

		preparation = language_preparation_new (language, &error);

		if (preparation != NULL &&
		    _gtk_source_language_file_parse_version2 (&preparation->definition,
							      preparation->ctx_data,
							      &preparation->styles,
							      &error))
			language_preparation_install (language, preparation);

		if (preparation != NULL)
			language_preparation_free (preparation);

		if (error != NULL)
		{
			g_warning ("Failed to load '%s': %s", language->lang_file_name, error->message);
			g_clear_error (&error);
		}
	}

	return language->ctx_data != NULL ? _gtk_source_context_data_ref (language->ctx_data) : NULL;
}

static void
language_prepare_worker (GTask        *task,
			 gpointer      source_object,
			 gpointer      task_data,
			 GCancellable *cancellable)
{
	LanguagePreparation *preparation = task_data;
	GError *error = NULL;

	if (!_gtk_source_language_file_parse_version2 (&preparation->definition,
						       preparation->ctx_data,
						       &preparation->styles,
						       &error))
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, TRUE);
}

static void
language_prepare_cb (GObject      *object,
		     GAsyncResult *result,
		     gpointer      user_data)
{
	GtkSourceLanguage *language = user_data;
	GPtrArray *tasks = g_steal_pointer (&language->prepare_tasks);
	GError *error = NULL;

	if (g_task_propagate_boolean (G_TASK (result), &error))
		language_preparation_install (language, g_task_get_task_data (G_TASK (result)));

	for (guint i = 0; i < tasks->len; i++)
	{
		GTask *task = g_ptr_array_index (tasks, i);

		if (g_task_return_error_if_cancelled (task))
			continue;

		if (error != NULL)
			g_task_return_error (task, g_error_copy (error));
		else
			g_task_return_boolean (task, TRUE);
	}

	g_clear_error (&error);
	g_ptr_array_unref (tasks);
	g_object_unref (language);
}

/**
 * gtk_source_language_prepare_async:
 * @language: a #GtkSourceLanguage
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async) (closure user_data) (nullable): callback invoked when preparation completes
 * @user_data: (nullable): data for @callback
 *
 * Prepares the language for highlighting without blocking the main loop.
 *
 * Parsing and regex compilation run on a worker thread. Concurrent requests
 * share preparation, and the language retains the prepared data for its lifetime.
 * Cancelling a request cancels that caller's result, not the shared preparation.
 *
 * [method@Buffer.set_language] starts preparation automatically. This method can
 * also be used to prepare a language before assigning it to a buffer, or to wait
 * until synchronous highlighting and context-class queries can be used.
 *
 * Call this method on the main thread. The callback runs in the thread-default
 * main context of the caller. Call [method@Language.prepare_finish] to obtain
 * the result.
 *
 * Since: 5.24
 */
void
gtk_source_language_prepare_async (GtkSourceLanguage   *language,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
	GTask *task = NULL;
	GTask *worker = NULL;
	GError *error = NULL;
	LanguagePreparation *preparation;

	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE (language));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (language, cancellable, callback, user_data);
	g_task_set_source_tag (task, gtk_source_language_prepare_async);

	if (g_task_return_error_if_cancelled (task))
		goto out;

	if (language->ctx_data != NULL)
	{
		g_task_return_boolean (task, TRUE);
		goto out;
	}

	if (language->prepare_tasks != NULL)
	{
		g_ptr_array_add (language->prepare_tasks, g_steal_pointer (&task));
		goto out;
	}

	if (!(preparation = language_preparation_new (language, &error)))
	{
		g_task_return_error (task, g_steal_pointer (&error));
		goto out;
	}

	language->prepare_tasks = g_ptr_array_new_with_free_func (g_object_unref);
	g_ptr_array_add (language->prepare_tasks, g_steal_pointer (&task));

	/* Only plain data crosses into the worker. Caller tasks and their source
	 * objects are released by language_prepare_cb() on the main thread.
	 */
	worker = g_task_new (NULL, NULL, language_prepare_cb, g_object_ref (language));
	g_task_set_task_data (worker, preparation, (GDestroyNotify) language_preparation_free);
	g_task_run_in_thread (worker, language_prepare_worker);

out:
	g_clear_object (&worker);
	g_clear_object (&task);
	g_clear_error (&error);
}

/**
 * gtk_source_language_prepare_finish:
 * @language: a #GtkSourceLanguage
 * @result: the #GAsyncResult passed to the callback
 * @error: return location for an error
 *
 * Finishes preparing a language for highlighting.
 *
 * Returns: %TRUE if the language is ready, or %FALSE on failure
 *
 * Since: 5.24
 */
gboolean
gtk_source_language_prepare_finish (GtkSourceLanguage  *language,
				    GAsyncResult       *result,
				    GError            **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, language), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gtk_source_language_prepare_async), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

GtkSourceEngine *
_gtk_source_language_create_engine (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

	if (language->ctx_data == NULL)
		return NULL;

	return GTK_SOURCE_ENGINE (_gtk_source_context_engine_new (language->ctx_data));
}

typedef struct
{
	gchar     *language_id;
	GPtrArray *ids_array;
} AddStyleIdData;

static void
add_style_id (gchar                  *id,
              G_GNUC_UNUSED gpointer  value,
              AddStyleIdData         *data)
{
	if (g_str_has_prefix (id, data->language_id))
		g_ptr_array_add (data->ids_array, g_strdup (id));
}

static gchar **
get_style_ids (GtkSourceLanguage *language)
{
	GPtrArray *ids_array;
	AddStyleIdData data;

	g_return_val_if_fail (language->styles != NULL, NULL);

	ids_array = g_ptr_array_new ();

	data.language_id = g_strdup_printf ("%s:", language->id);
	data.ids_array = ids_array;

	g_hash_table_foreach (language->styles,
			      (GHFunc) add_style_id,
			      &data);

	g_free (data.language_id);

	if (ids_array->len == 0)
	{
		/* No style defined in this language */
		g_ptr_array_free (ids_array, TRUE);

		return NULL;
	}
	else
	{
		/* Terminate the array with NULL */
		g_ptr_array_add (ids_array, NULL);

		return (gchar **)g_ptr_array_free (ids_array, FALSE);
	}
}

static gboolean
force_styles (GtkSourceLanguage *language)
{
	/* To be sure to have the list of styles we need to parse lang file
	 * as if we were to create an engine. In the future we can improve
	 * this by parsing styles only.
	 */
	if (!language->styles_loaded && language->ctx_data == NULL)
	{
		GtkSourceContextData *ctx_data;

		ctx_data = gtk_source_language_parse_file (language);
		if (ctx_data == NULL)
			return FALSE;

		language->styles_loaded = TRUE;
		_gtk_source_context_data_unref (ctx_data);
	}

	return TRUE;
}

/**
 * gtk_source_language_get_style_ids:
 * @language: a #GtkSourceLanguage.
 *
 * Returns the ids of the styles defined by this @language.
 *
 * If preparation is pending, returns %NULL. Wait for
 * [method@Language.prepare_async] to complete before querying styles.
 *
 * Returns: (nullable) (array zero-terminated=1) (transfer full):
 * a newly-allocated %NULL terminated array containing ids of the
 * styles defined by this @language or %NULL if no style is defined.
 * The returned array must be freed with g_strfreev().
*/
gchar **
gtk_source_language_get_style_ids (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->id != NULL, NULL);

	if (!force_styles (language))
		return NULL;

	return get_style_ids (language);
}

static GtkSourceStyleInfo *
get_style_info (GtkSourceLanguage *language,
                const char        *style_id)
{
	GtkSourceStyleInfo *info;

	if (!force_styles (language))
		return NULL;

	g_return_val_if_fail (language->styles != NULL, NULL);

	info = g_hash_table_lookup (language->styles, style_id);

	return info;
}

/**
 * gtk_source_language_get_style_name:
 * @language: a #GtkSourceLanguage.
 * @style_id: a style ID.
 *
 * Returns the name of the style with ID @style_id defined by this @language.
 *
 * If preparation is pending, returns %NULL. Wait for
 * [method@Language.prepare_async] to complete before querying styles.
 *
 * Returns: (nullable) (transfer none): the name of the style with ID @style_id
 * defined by this @language or %NULL if the style has no name or there is no
 * style with ID @style_id defined by this @language.
 * The returned string is owned by the @language and must not be modified.
 */
const gchar *
gtk_source_language_get_style_name (GtkSourceLanguage *language,
                                    const gchar       *style_id)
{
	GtkSourceStyleInfo *info;

	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->id != NULL, NULL);
	g_return_val_if_fail (style_id != NULL, NULL);

	info = get_style_info (language, style_id);

	return info ? info->name : NULL;
}

/**
 * gtk_source_language_get_style_fallback:
 * @language: a #GtkSourceLanguage.
 * @style_id: a style ID.
 *
 * Returns the ID of the style to use if the specified @style_id
 * is not present in the current style scheme.
 *
 * If preparation is pending, returns %NULL. Wait for
 * [method@Language.prepare_async] to complete before querying styles.
 *
 * Returns: (nullable) (transfer none): the ID of the style to use if the
 * specified @style_id is not present in the current style scheme or %NULL
 * if the style has no fallback defined.
 * The returned string is owned by the @language and must not be modified.
 */
const gchar *
gtk_source_language_get_style_fallback (GtkSourceLanguage *language,
                                        const gchar       *style_id)
{
	GtkSourceStyleInfo *info;

	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);
	g_return_val_if_fail (language->id != NULL, NULL);
	g_return_val_if_fail (style_id != NULL, NULL);

	info = get_style_info (language, style_id);

	return info ? info->map_to : NULL;
}

/* Utility functions for GtkSourceStyleInfo */

GtkSourceStyleInfo *
_gtk_source_style_info_new (const gchar *name,
                            const gchar *map_to)
{
	GtkSourceStyleInfo *info = g_new0 (GtkSourceStyleInfo, 1);

	info->name = g_strdup (name);
	info->map_to = g_strdup (map_to);

	return info;
}

GtkSourceStyleInfo *
_gtk_source_style_info_copy (GtkSourceStyleInfo *info)
{
	g_return_val_if_fail (info != NULL, NULL);

	return _gtk_source_style_info_new (info->name, info->map_to);
}

void
_gtk_source_style_info_free (GtkSourceStyleInfo *info)
{
	if (info == NULL)
	{
		return;
	}

	g_free (info->name);
	g_free (info->map_to);
	g_free (info);
}


const gchar *
_gtk_source_language_get_file_name (GtkSourceLanguage *language)
{
	return language->lang_file_name;
}

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003-2007 - Paolo Maggi <paolo.maggi@polito.it>
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

#include <string.h>
#include <gio/gio.h>

#include "gtksourcelanguage.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcelanguagemanager-private.h"
#include "gtksourceutils-private.h"

/**
 * GtkSourceLanguageManager:
 *
 * Provides access to [class@Language]s.
 *
 * `GtkSourceLanguageManager` is an object which processes language description
 * files and creates and stores [class@Language] objects, and provides API to
 * access them.
 *
 * Use [func@LanguageManager.get_default] to retrieve the default
 * instance of `GtkSourceLanguageManager`, and
 * [method@LanguageManager.guess_language] to get a [class@Language] for
 * given file name and content type.
 */

#define RNG_SCHEMA_FILE          "language2.rng"
#define LANGUAGE_DIR             "language-specs"
#define LANG_FILE_SUFFIX         ".lang"
#define FALLBACK_RNG_SCHEMA_FILE PACKAGE_DATADIR "/language-specs/" RNG_SCHEMA_FILE

enum {
	PROP_0,
	PROP_SEARCH_PATH,
	PROP_LANGUAGE_IDS,
	N_PROPS
};

struct _GtkSourceLanguageManager
{
	GObject          parent_instance;

	GHashTable	*language_ids;

	gchar	       **lang_dirs;
	gchar		*rng_file;

	gchar          **ids; /* Cache the IDs of the available languages */
};

static GtkSourceLanguageManager *default_instance;
static GParamSpec *properties[N_PROPS];
static const char *default_rng_file = FALLBACK_RNG_SCHEMA_FILE;

G_DEFINE_TYPE (GtkSourceLanguageManager, gtk_source_language_manager, G_TYPE_OBJECT)

static void
gtk_source_language_manager_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
	GtkSourceLanguageManager *lm;

	lm = GTK_SOURCE_LANGUAGE_MANAGER (object);

	switch (prop_id)
	{
		case PROP_SEARCH_PATH:
			gtk_source_language_manager_set_search_path (lm, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_language_manager_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
	GtkSourceLanguageManager *lm;

	lm = GTK_SOURCE_LANGUAGE_MANAGER (object);

	switch (prop_id)
	{
		case PROP_SEARCH_PATH:
			g_value_set_boxed (value, gtk_source_language_manager_get_search_path (lm));
			break;

		case PROP_LANGUAGE_IDS:
			g_value_set_boxed (value, gtk_source_language_manager_get_language_ids (lm));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_language_manager_finalize (GObject *object)
{
	GtkSourceLanguageManager *lm = GTK_SOURCE_LANGUAGE_MANAGER (object);

	if (lm->language_ids)
		g_hash_table_destroy (lm->language_ids);

	g_strfreev (lm->ids);

	g_strfreev (lm->lang_dirs);
	g_free (lm->rng_file);

	G_OBJECT_CLASS (gtk_source_language_manager_parent_class)->finalize (object);
}

static void
gtk_source_language_manager_class_init (GtkSourceLanguageManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	= gtk_source_language_manager_finalize;

	object_class->set_property = gtk_source_language_manager_set_property;
	object_class->get_property = gtk_source_language_manager_get_property;

	properties[PROP_SEARCH_PATH] =
		g_param_spec_boxed ("search-path",
		                    "Language specification directories",
		                    "List of directories where the "
		                    "language specification files (.lang) "
		                    "are located",
		                    G_TYPE_STRV,
		                    (G_PARAM_READWRITE |
		                     G_PARAM_EXPLICIT_NOTIFY |
		                     G_PARAM_STATIC_STRINGS));

	properties[PROP_LANGUAGE_IDS] =
		g_param_spec_boxed ("language-ids",
		                    "Language ids",
		                    "List of the ids of the available languages",
		                    G_TYPE_STRV,
		                    (G_PARAM_READABLE |
		                     G_PARAM_EXPLICIT_NOTIFY |
		                     G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_language_manager_init (GtkSourceLanguageManager *lm)
{
	lm = gtk_source_language_manager_get_instance_private (lm);
	lm->language_ids = NULL;
	lm->ids = NULL;
	lm->lang_dirs = NULL;
	lm->rng_file = NULL;
}

/**
 * gtk_source_language_manager_new:
 *
 * Creates a new language manager.
 *
 * If you do not need more than one language manager or a private language manager
 * instance then use [func@LanguageManager.get_default] instead.
 *
 * Returns: a new #GtkSourceLanguageManager.
 */
GtkSourceLanguageManager *
gtk_source_language_manager_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_LANGUAGE_MANAGER, NULL);
}

/**
 * gtk_source_language_manager_get_default:
 *
 * Returns the default #GtkSourceLanguageManager instance.
 *
 * Returns: (transfer none): a #GtkSourceLanguageManager.
 * Return value is owned by GtkSourceView library and must not be unref'ed.
 */
GtkSourceLanguageManager *
gtk_source_language_manager_get_default (void)
{
	if (default_instance == NULL)
	{
		default_instance = gtk_source_language_manager_new ();
		g_object_add_weak_pointer (G_OBJECT (default_instance),
					   (gpointer) &default_instance);
	}

	return default_instance;
}

GtkSourceLanguageManager *
_gtk_source_language_manager_peek_default (void)
{
	return default_instance;
}

static void
notify_search_path (GtkSourceLanguageManager *mgr)
{
	g_object_notify_by_pspec (G_OBJECT (mgr), properties[PROP_SEARCH_PATH]);
	g_object_notify_by_pspec (G_OBJECT (mgr), properties[PROP_LANGUAGE_IDS]);
}

/**
 * gtk_source_language_manager_set_search_path:
 * @lm: a #GtkSourceLanguageManager.
 * @dirs: (nullable) (array zero-terminated=1): a %NULL-terminated array of
 *   strings or %NULL.
 *
 * Sets the list of directories where the @lm looks for
 * language files.
 *
 * If @dirs is %NULL, the search path is reset to default.
 *
 * At the moment this function can be called only before the
 * language files are loaded for the first time. In practice
 * to set a custom search path for a `GtkSourceLanguageManager`,
 * you have to call this function right after creating it.
 *
 * Since GtkSourceView 5.4 this function will allow you to provide
 * paths in the form of "resource:///" URIs to embedded `GResource`s.
 * They must contain the path of a directory within the `GResource`.
 */
void
gtk_source_language_manager_set_search_path (GtkSourceLanguageManager *lm,
                                             const gchar * const      *dirs)
{
	gchar **tmp;

	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm));

	/* Search path cannot be changed in the list of available languages
	 * as been already computed */
	g_return_if_fail (lm->ids == NULL);

	tmp = lm->lang_dirs;

	if (dirs == NULL)
		lm->lang_dirs = _gtk_source_utils_get_default_dirs (LANGUAGE_DIR);
	else
		lm->lang_dirs = g_strdupv ((gchar **)dirs);

	g_strfreev (tmp);

	notify_search_path (lm);
}

/**
 * gtk_source_language_manager_append_search_path:
 * @lm: a #GtkSourceLanguageManager.
 * @path: a directory or a filename.
 *
 * Appends @path to the list of directories where the @manager looks for
 * language files.
 *
 * See [method@LanguageManager.set_search_path] for details.
 *
 * Since: 5.4
 */
void
gtk_source_language_manager_append_search_path (GtkSourceLanguageManager *lm,
                                                const gchar              *path)
{
	guint len = 0;

	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm));
	g_return_if_fail (path != NULL);

	if (lm->lang_dirs == NULL)
		lm->lang_dirs = _gtk_source_utils_get_default_dirs (LANGUAGE_DIR);

	g_return_if_fail (lm->lang_dirs != NULL);

	len = g_strv_length (lm->lang_dirs);

	lm->lang_dirs = g_renew (gchar *,
	                         lm->lang_dirs,
	                         len + 2); /* old path + new entry + NULL */

	lm->lang_dirs[len] = g_strdup (path);
	lm->lang_dirs[len + 1] = NULL;

	notify_search_path (lm);
}

/**
 * gtk_source_language_manager_prepend_search_path:
 * @lm: a #GtkSourceLanguageManager.
 * @path: a directory or a filename.
 *
 * Prepends @path to the list of directories where the @manager looks
 * for language files.
 *
 * See [method@LanguageManager.set_search_path] for details.
 *
 * Since: 5.4
 */
void
gtk_source_language_manager_prepend_search_path (GtkSourceLanguageManager *lm,
                                                 const gchar              *path)
{
	guint len = 0;
	gchar **new_lang_dirs;

	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm));
	g_return_if_fail (path != NULL);

	if (lm->lang_dirs == NULL)
		lm->lang_dirs = _gtk_source_utils_get_default_dirs (LANGUAGE_DIR);

	g_return_if_fail (lm->lang_dirs != NULL);

	len = g_strv_length (lm->lang_dirs);

	new_lang_dirs = g_new (gchar *, len + 2);
	new_lang_dirs[0] = g_strdup (path);
	memcpy (new_lang_dirs + 1, lm->lang_dirs, (len + 1) * sizeof (gchar*));

	g_free (lm->lang_dirs);
	lm->lang_dirs = new_lang_dirs;

	notify_search_path (lm);
}

/**
 * gtk_source_language_manager_get_search_path:
 * @lm: a #GtkSourceLanguageManager.
 *
 * Gets the list directories where @lm looks for language files.
 *
 * Returns: (array zero-terminated=1) (transfer none): %NULL-terminated array
 * containing a list of language files directories.
 * The array is owned by @lm and must not be modified.
 */
const gchar * const *
gtk_source_language_manager_get_search_path (GtkSourceLanguageManager *lm)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm), NULL);

	if (lm->lang_dirs == NULL)
		lm->lang_dirs = _gtk_source_utils_get_default_dirs (LANGUAGE_DIR);

	return (const gchar * const *)lm->lang_dirs;
}

void
_gtk_source_language_manager_set_rng_file (const char *rng_file)
{
	if (rng_file == NULL)
	{
		default_rng_file = FALLBACK_RNG_SCHEMA_FILE;
	}
	else
	{
		default_rng_file = g_intern_string (rng_file);
	}
}

/**
 * _gtk_source_language_manager_get_rng_file:
 * @lm: a #GtkSourceLanguageManager.
 *
 * Returns location of the RNG schema file for lang files version 2.
 *
 * Returns: path to RNG file. It belongs to %lm and must not be freed or modified.
 */
const char *
_gtk_source_language_manager_get_rng_file (GtkSourceLanguageManager *lm)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm), NULL);

	if (lm->rng_file == NULL)
	{
		const gchar * const *dirs;

		for (dirs = gtk_source_language_manager_get_search_path (lm);
		     dirs != NULL && *dirs != NULL;
		     ++dirs)
		{
			gchar *file;

			file = g_build_filename (*dirs, RNG_SCHEMA_FILE, NULL);
			if (g_file_test (file, G_FILE_TEST_EXISTS))
			{
				lm->rng_file = file;
				break;
			}

			g_free (file);
		}

		if (lm->rng_file == NULL)
		{
			if (g_file_test (default_rng_file, G_FILE_TEST_EXISTS))
			{
				lm->rng_file = g_strdup (default_rng_file);
			}
		}
	}

	return lm->rng_file;
}

static gint
language_compare (const gchar **id1,
                  const gchar **id2,
                  GHashTable   *language_ids)
{
	GtkSourceLanguage *lang1, *lang2;
	const gchar *name1, *name2;

	lang1 = g_hash_table_lookup (language_ids, *id1);
	lang2 = g_hash_table_lookup (language_ids, *id2);

	name1 = gtk_source_language_get_name (lang1);
	name2 = gtk_source_language_get_name (lang2);

	return g_utf8_collate (name1, name2);
}

static void
ensure_languages (GtkSourceLanguageManager *lm)
{
	GSList *filenames, *l;
	GPtrArray *ids_array = NULL;

	if (lm->language_ids != NULL)
		return;

	lm->language_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
							g_free, g_object_unref);

	filenames = _gtk_source_utils_get_file_list ((gchar **)gtk_source_language_manager_get_search_path (lm),
						     LANG_FILE_SUFFIX,
						     TRUE);

	for (l = filenames; l != NULL; l = l->next)
	{
		GtkSourceLanguage *lang;
		const gchar *id;
		gchar *filename;

		filename = l->data;

		lang = _gtk_source_language_new_from_file (filename, lm);

		if (lang == NULL)
		{
			g_warning ("Error reading language specification file '%s'", filename);
			continue;
		}

		id = gtk_source_language_get_id (lang);

		if (g_hash_table_lookup (lm->language_ids, id) == NULL)
		{
			g_hash_table_insert (lm->language_ids,
					     g_strdup (id),
					     lang);

			if (ids_array == NULL)
				ids_array = g_ptr_array_new ();

			g_ptr_array_add (ids_array, g_strdup (id));
		}
		else
		{
			g_object_unref (lang);
		}
	}

	if (ids_array != NULL)
	{
		/* Sort the array alphabetically so that it
		 * is ready to use in a list of a GUI */
		g_ptr_array_sort_with_data (ids_array,
		                            (GCompareDataFunc)language_compare,
		                            lm->language_ids);

		/* Ensure the array is NULL terminated */
		g_ptr_array_add (ids_array, NULL);

		lm->ids = (gchar **)g_ptr_array_free (ids_array, FALSE);
	}

	g_slist_free_full (filenames, g_free);
}

/**
 * gtk_source_language_manager_get_language_ids:
 * @lm: a #GtkSourceLanguageManager.
 *
 * Returns the ids of the available languages.
 *
 * Returns: (nullable) (array zero-terminated=1) (transfer none):
 * a %NULL-terminated array of strings containing the ids of the available
 * languages or %NULL if no language is available.
 * The array is sorted alphabetically according to the language name.
 * The array is owned by @lm and must not be modified.
 */
const gchar * const *
gtk_source_language_manager_get_language_ids (GtkSourceLanguageManager *lm)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm), NULL);

	ensure_languages (lm);

	return (const gchar * const *)lm->ids;
}

/**
 * gtk_source_language_manager_get_language:
 * @lm: a #GtkSourceLanguageManager.
 * @id: a language id.
 *
 * Gets the [class@Language] identified by the given @id in the language
 * manager.
 *
 * Returns: (nullable) (transfer none): a #GtkSourceLanguage, or %NULL
 * if there is no language identified by the given @id. Return value is
 * owned by @lm and should not be freed.
 */
GtkSourceLanguage *
gtk_source_language_manager_get_language (GtkSourceLanguageManager *lm,
                                          const gchar              *id)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	ensure_languages (lm);

	return g_hash_table_lookup (lm->language_ids, id);
}

static GSList *
pick_langs_for_filename (GtkSourceLanguageManager *lm,
                         const gchar              *filename)
{
	char *filename_utf8;
	const gchar* const * p;
	GSList *langs = NULL;

	/* Use g_filename_display_name() instead of g_filename_to_utf8() because
	 * g_filename_display_name() doesn't fail and replaces non-convertible
	 * characters to unicode substitution symbol. */
	filename_utf8 = g_filename_display_name (filename);

	for (p = gtk_source_language_manager_get_language_ids (lm);
	     p != NULL && *p != NULL;
	     p++)
	{
		GtkSourceLanguage *lang;
		gchar **globs, **gptr;

		lang = gtk_source_language_manager_get_language (lm, *p);
		globs = gtk_source_language_get_globs (lang);

		for (gptr = globs; gptr != NULL && *gptr != NULL; gptr++)
		{
			/* FIXME g_pattern_match is wrong: there are no '[...]'
			 * character ranges and '*' and '?' can not be escaped
			 * to include them literally in a pattern.  */
			if (**gptr != '\0' && g_pattern_match_simple (*gptr, filename_utf8))
			{
				langs = g_slist_prepend (langs, lang);
				break;
			}
		}

		g_strfreev (globs);
	}

	g_free (filename_utf8);

	return g_slist_reverse (langs);
}

static GtkSourceLanguage *
pick_lang_for_mime_type_pass (GtkSourceLanguageManager *lm,
                              const char               *mime_type,
                              gboolean                  exact_match)
{
	const gchar* const * id_ptr;

	for (id_ptr = gtk_source_language_manager_get_language_ids (lm);
	     id_ptr != NULL && *id_ptr != NULL;
	     id_ptr++)
	{
		GtkSourceLanguage *lang;
		gchar **mime_types, **mptr;

		lang = gtk_source_language_manager_get_language (lm, *id_ptr);
		mime_types = gtk_source_language_get_mime_types (lang);

		for (mptr = mime_types; mptr != NULL && *mptr != NULL; mptr++)
		{
			gboolean matches;

			if (exact_match)
				matches = strcmp (mime_type, *mptr) == 0;
			else
				matches = g_content_type_is_a (mime_type, *mptr);

			if (matches)
			{
				g_strfreev (mime_types);
				return lang;
			}
		}

		g_strfreev (mime_types);
	}

	return NULL;
}

static GtkSourceLanguage *
pick_lang_for_mime_type_real (GtkSourceLanguageManager *lm,
                              const char               *mime_type)
{
	GtkSourceLanguage *lang;
	lang = pick_lang_for_mime_type_pass (lm, mime_type, TRUE);
	if (!lang)
		lang = pick_lang_for_mime_type_pass (lm, mime_type, FALSE);
	return lang;
}

#ifdef G_OS_WIN32
static void
grok_win32_content_type (const gchar  *content_type,
                         gchar       **alt_filename,
                         gchar       **mime_type)
{
	*alt_filename = NULL;
	*mime_type = NULL;

	/* If it contains slash, then it's probably a mime type.
	 * Otherwise treat is an extension. */
	if (strchr (content_type, '/') != NULL)
		*mime_type = g_strdup (content_type);
	else
		*alt_filename = g_strjoin ("filename", content_type, NULL);
}
#endif

static GtkSourceLanguage *
pick_lang_for_mime_type (GtkSourceLanguageManager *lm,
                         const gchar              *content_type)
{
	GtkSourceLanguage *lang = NULL;

#ifndef G_OS_WIN32
	/* On Unix "content type" is mime type */
	lang = pick_lang_for_mime_type_real (lm, content_type);
#else
	/* On Windows "content type" is an extension, but user may pass a mime type too */
	gchar *mime_type;
	gchar *alt_filename;

	grok_win32_content_type (content_type, &alt_filename, &mime_type);

	if (alt_filename != NULL)
	{
		GSList *langs;

		langs = pick_langs_for_filename (lm, alt_filename);

		if (langs != NULL)
			lang = GTK_SOURCE_LANGUAGE (langs->data);
	}

	if (lang == NULL && mime_type != NULL)
		lang = pick_lang_for_mime_type_real (lm, mime_type);

	g_free (mime_type);
	g_free (alt_filename);
#endif
	return lang;
}

/**
 * gtk_source_language_manager_guess_language:
 * @lm: a #GtkSourceLanguageManager.
 * @filename: (nullable) (type filename): a filename in Glib filename encoding, or %NULL.
 * @content_type: (nullable): a content type (as in GIO API), or %NULL.
 *
 * Picks a [class@Language] for given file name and content type,
 * according to the information in lang files.
 *
 * Either @filename or @content_type may be %NULL. This function can be used as follows:
 *
 * ```c
 * GtkSourceLanguage *lang;
 * GtkSourceLanguageManager *manager;
 * lm = gtk_source_language_manager_get_default ();
 * lang = gtk_source_language_manager_guess_language (manager, filename, NULL);
 * gtk_source_buffer_set_language (buffer, lang);
 * ```
 * ```python
 * manager = GtkSource.LanguageManager.get_default()
 * language = manager.guess_language(filename=filename, content_type=None)
 * buffer.set_language(language=language)
 * ```
 *
 * or
 *
 * ```c
 * GtkSourceLanguage *lang = NULL;
 * GtkSourceLanguageManager *manager;
 * gboolean result_uncertain;
 * gchar *content_type;
 *
 * content_type = g_content_type_guess (filename, NULL, 0, &result_uncertain);
 * if (result_uncertain)
 *   {
 *     g_free (content_type);
 *     content_type = NULL;
 *   }
 *
 * manager = gtk_source_language_manager_get_default ();
 * lang = gtk_source_language_manager_guess_language (manager, filename, content_type);
 * gtk_source_buffer_set_language (buffer, lang);
 *
 * g_free (content_type);
 * ```
 * ```python
 * content_type, uncertain = Gio.content_type_guess(filename=filename, data=None)
 * if uncertain:
 *     content_type = None
 *
 * manager = GtkSource.LanguageManager.get_default()
 * language = manager.guess_language(filename=filename, content_type=content_type)
 * buffer.set_language(language=language)
 * ```
 *
 * etc. Use [method@Language.get_mime_types] and [method@Language.get_globs]
 * if you need full control over file -> language mapping.
 *
 * Returns: (nullable) (transfer none): a #GtkSourceLanguage, or %NULL if there
 * is no suitable language for given @filename and/or @content_type. Return
 * value is owned by @lm and should not be freed.
 */
GtkSourceLanguage *
gtk_source_language_manager_guess_language (GtkSourceLanguageManager *lm,
                                            const gchar              *filename,
                                            const gchar              *content_type)
{
	GtkSourceLanguage *lang = NULL;
	GSList *langs = NULL;

	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE_MANAGER (lm), NULL);
	g_return_val_if_fail ((filename != NULL && *filename != '\0') ||
	                      (content_type != NULL && *content_type != '\0'), NULL);

	ensure_languages (lm);

	/* Glob take precedence over mime match. Mime match is used in the
	   following cases:
	  - to pick among the list of glob matches
	  - to refine a glob match (e.g. glob is xml and mime is an xml dialect)
	  - no glob matches
	*/

	if (filename != NULL && *filename != '\0')
		langs = pick_langs_for_filename (lm, filename);

	if (langs != NULL)
	{
		/* Use mime to pick among glob matches */
		if (content_type != NULL)
		{
			GSList *l;

			for (l = langs; l != NULL; l = g_slist_next (l))
			{
				gchar **mime_types, **gptr;

				lang = GTK_SOURCE_LANGUAGE (l->data);
				mime_types = gtk_source_language_get_mime_types (lang);

				for (gptr = mime_types; gptr != NULL && *gptr != NULL; gptr++)
				{
					gchar *content;

					content = g_content_type_from_mime_type (*gptr);

					if (content != NULL && g_content_type_is_a (content_type, content))
					{
						if (!g_content_type_equals (content_type, content))
						{
							GtkSourceLanguage *mimelang;

							mimelang = pick_lang_for_mime_type (lm, content_type);

							if (mimelang != NULL)
								lang = mimelang;
						}

						g_strfreev (mime_types);
						g_slist_free (langs);
						g_free (content);

						return lang;
					}
					g_free (content);
				}

				g_strfreev (mime_types);
			}
		}
		lang = GTK_SOURCE_LANGUAGE (langs->data);

		g_slist_free (langs);
	}
	/* No glob match */
	else if (langs == NULL && content_type != NULL)
	{
		lang = pick_lang_for_mime_type (lm, content_type);
	}

	return lang;
}

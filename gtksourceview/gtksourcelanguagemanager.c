/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcelanguagemanager.c
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

#include <string.h>
#include "gtksourceview-i18n.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcelanguage.h"
#include "gtksourceview-utils.h"

#define RNG_SCHEMA_FILE		"language2.rng"
#define LANGUAGE_DIR		"language-specs"
#define LANG_FILE_SUFFIX	".lang"

enum {
	PROP_0,
	PROP_SEARCH_PATH
};

struct _GtkSourceLanguageManagerPrivate
{
	GHashTable	*language_ids;
	GSList 		*available_languages;
	char	       **lang_dirs;
	char		*rng_file;
};

G_DEFINE_TYPE (GtkSourceLanguageManager, gtk_source_language_manager, G_TYPE_OBJECT)


static void
gtk_source_language_manager_set_property (GObject 	*object,
					  guint 	 prop_id,
					  const GValue *value,
					  GParamSpec	*pspec)
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
gtk_source_language_manager_get_property (GObject 	*object,
					  guint 	 prop_id,
					  GValue 	*value,
					  GParamSpec	*pspec)
{
	GtkSourceLanguageManager *lm;

	lm = GTK_SOURCE_LANGUAGE_MANAGER (object);

	switch (prop_id)
	{
		case PROP_SEARCH_PATH:
			g_value_set_boxed (value, gtk_source_language_manager_get_search_path (lm));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_language_manager_finalize (GObject *object)
{
	GtkSourceLanguageManager *lm;

	lm = GTK_SOURCE_LANGUAGE_MANAGER (object);

	if (lm->priv->language_ids)
		g_hash_table_destroy (lm->priv->language_ids);

	g_slist_foreach (lm->priv->available_languages, (GFunc) g_object_unref, NULL);
	g_slist_free (lm->priv->available_languages);
	g_strfreev (lm->priv->lang_dirs);
	g_free (lm->priv->rng_file);

	G_OBJECT_CLASS (gtk_source_language_manager_parent_class)->finalize (object);
}

static void
gtk_source_language_manager_class_init (GtkSourceLanguageManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	= gtk_source_language_manager_finalize;

	object_class->set_property = gtk_source_language_manager_set_property;
	object_class->get_property = gtk_source_language_manager_get_property;

	g_object_class_install_property (object_class,
					 PROP_SEARCH_PATH,
					 g_param_spec_boxed ("search-path",
						 	     _("Language specification directories"),
							     _("List of directories where the "
							       "language specification files (.lang) "
							       "are located"),
							     G_TYPE_STRV,
							     G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(GtkSourceLanguageManagerPrivate));
}

static void
gtk_source_language_manager_init (GtkSourceLanguageManager *lm)
{
	lm->priv = G_TYPE_INSTANCE_GET_PRIVATE (lm, GTK_TYPE_SOURCE_LANGUAGE_MANAGER,
						GtkSourceLanguageManagerPrivate);
	lm->priv->language_ids = NULL;
	lm->priv->available_languages = NULL;
	lm->priv->lang_dirs = NULL;
	lm->priv->rng_file = NULL;
}

/**
 * gtk_source_language_manager_new:
 *
 * Creates a new language manager. If you do not need more than one language
 * manager or a private language manager instance then use
 * gtk_source_language_manager_get_default() instead.
 *
 * Returns: a #GtkSourceLanguageManager.
 */
GtkSourceLanguageManager *
gtk_source_language_manager_new (void)
{
	return g_object_new (GTK_TYPE_SOURCE_LANGUAGE_MANAGER, NULL);
}

/**
 * gtk_source_language_manager_get_default:
 *
 * Returns the default #GtkSourceLanguageManager instance.
 *
 * Returns: a #GtkSourceLanguageManager. Return value is owned
 * by GtkSourceView library and must not be unref'ed.
 */
GtkSourceLanguageManager *
gtk_source_language_manager_get_default (void)
{
	static GtkSourceLanguageManager *instance;

	if (instance == NULL)
	{
		instance = gtk_source_language_manager_new ();
		g_object_add_weak_pointer (G_OBJECT (instance),
					   (gpointer) &instance);
	}

	return instance;
}

/**
 * gtk_source_language_manager_set_search_path:
 * @lm: a #GtkSourceLanguageManager.
 * @dirs: a %NULL-terminated array of strings or %NULL.
 *
 * Sets the list of directories where the @lm looks for
 * language files.
 * If @dirs is %NULL, the search path is reset to default.
 *
 * <note>
 *   <para>
 *     At the moment this function can be called only before the
 *     language files are loaded for the first time. In practice
 *     to set a custom search path for a #GtkSourceLanguageManager,
 *     you have to call this function right after creating it.
 *   </para>
 * </note>
 */
void
gtk_source_language_manager_set_search_path (GtkSourceLanguageManager *lm,
					     gchar                   **dirs)
{
	gchar **tmp;

	g_return_if_fail (GTK_IS_SOURCE_LANGUAGE_MANAGER (lm));

	/* Search path cannot be changed in the list of available languages
	 * as been already changed */
	g_return_if_fail (lm->priv->available_languages == NULL);

	tmp = lm->priv->lang_dirs;
	lm->priv->lang_dirs = g_strdupv (dirs);
	g_strfreev (tmp);

	g_object_notify (G_OBJECT (lm), "search-path");
}

/**
 * gtk_source_language_manager_get_search_path:
 * @lm: a #GtkSourceLanguageManager.
 *
 * Gets the list directories where @lm looks for language files.
 *
 * Returns: %NULL-terminated array containg a list of language files directories.
 * It is owned by @lm and must not be modified or freed.
 */
gchar **
gtk_source_language_manager_get_search_path (GtkSourceLanguageManager *lm)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE_MANAGER (lm), NULL);

	if (lm->priv->lang_dirs == NULL)
		lm->priv->lang_dirs = _gtk_source_view_get_default_dirs (LANGUAGE_DIR, TRUE);

	return lm->priv->lang_dirs;
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
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE_MANAGER (lm), NULL);

	if (lm->priv->rng_file == NULL)
	{
		gchar **dirs;

		for (dirs = gtk_source_language_manager_get_search_path (lm);
		     dirs != NULL && *dirs != NULL;
		     ++dirs)
		{
			gchar *file;

			file = g_build_filename (*dirs, RNG_SCHEMA_FILE, NULL);
			if (g_file_test (file, G_FILE_TEST_EXISTS))
			{
				lm->priv->rng_file = file;
				break;
			}

			g_free (file);
		}
	}

	return lm->priv->rng_file;
}

static void
prepend_lang (G_GNUC_UNUSED gchar      *id,
	      GtkSourceLanguage        *lang,
	      GtkSourceLanguageManager *lm)
{
	lm->priv->available_languages =
		g_slist_prepend (lm->priv->available_languages, g_object_ref (lang));
}

static void
ensure_languages (GtkSourceLanguageManager *lm)
{
	GSList *filenames, *l;

	if (lm->priv->language_ids != NULL)
		return;

	lm->priv->language_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
							g_free, g_object_unref);

	filenames = _gtk_source_view_get_file_list (gtk_source_language_manager_get_search_path (lm),
						    LANG_FILE_SUFFIX,
						    TRUE);

	for (l = filenames; l != NULL; l = l->next)
	{
		GtkSourceLanguage *lang;
		gchar *filename;

		filename = l->data;

		lang = _gtk_source_language_new_from_file (filename, lm);

		if (lang == NULL)
		{
			g_warning ("Error reading language specification file '%s'", filename);
			continue;
		}

		if (g_hash_table_lookup (lm->priv->language_ids, lang->priv->id) == NULL)
		{
			g_hash_table_insert (lm->priv->language_ids,
					     g_strdup (lang->priv->id),
					     lang);
		}
		else
		{
			g_object_unref (lang);
		}
	}

	g_hash_table_foreach (lm->priv->language_ids, (GHFunc) prepend_lang, lm);

	g_slist_foreach (filenames, (GFunc) g_free, NULL);
	g_slist_free (filenames);
}

/**
 * gtk_source_language_manager_get_available_languages:
 * @lm: a #GtkSourceLanguageManager.
 *
 * Gets a list of available languages for the given language manager.
 *
 * Returns: a list of #GtkSourceLanguage objects. It must be freed with
 * g_slist_free(), but its elements must not be unref'ed.
 */
GSList *
gtk_source_language_manager_list_languages (GtkSourceLanguageManager *lm)
{
	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE_MANAGER (lm), NULL);

	ensure_languages (lm);

	return g_slist_copy (lm->priv->available_languages);
}

/**
 * gtk_source_language_manager_get_language_by_id:
 * @lm: a #GtkSourceLanguageManager.
 * @id: a language id.
 *
 * Gets the #GtkSourceLanguage identified by the given @id in the language
 * manager.
 *
 * Returns: a #GtkSourceLanguage, or %NULL if there is no language
 * identified by the given @id. Return value is owned by @lm and should not
 * be freed.
 */
GtkSourceLanguage *
gtk_source_language_manager_get_language_by_id (GtkSourceLanguageManager *lm,
						const gchar              *id)
{
	g_return_val_if_fail (id != NULL, NULL);
	ensure_languages (lm);
	return g_hash_table_lookup (lm->priv->language_ids, id);
}


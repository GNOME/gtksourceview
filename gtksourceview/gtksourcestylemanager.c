/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestylemanager.c
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

#include "gtksourcestylemanager.h"
#include "gtksourceview-marshal.h"
#include "gtksourceview-i18n.h"
#include "gtksourceview-utils.h"
#include <string.h>

#define SCHEME_FILE_SUFFIX	".xml"
#define STYLES_DIR		"styles"


struct _GtkSourceStyleManagerPrivate
{
	GSList		*schemes;
	gchar	       **_dirs;
	GSList          *added_files;
	gboolean	 need_reload;
};

enum {
	LIST_CHANGED,
	N_SIGNALS
};

enum {
	PROP_0,
	PROP_SEARCH_PATH
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE (GtkSourceStyleManager, gtk_source_style_manager, G_TYPE_OBJECT)

static void
gtk_source_style_manager_set_property (GObject 	    *object,
				       guint 	     prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
	GtkSourceStyleManager *sm;

	sm = GTK_SOURCE_STYLE_MANAGER (object);

	switch (prop_id)
	{
		case PROP_SEARCH_PATH:
			gtk_source_style_manager_set_search_path (sm, g_value_get_boxed (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   pspec);
			break;
	}
}

static void
gtk_source_style_manager_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
	GtkSourceStyleManager *sm;

	sm = GTK_SOURCE_STYLE_MANAGER (object);

	switch (prop_id)
	{
		case PROP_SEARCH_PATH:
			g_value_set_boxed (value, gtk_source_style_manager_get_search_path (sm));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object,
							   prop_id,
							   pspec);
			break;
	}
}

static void
gtk_source_style_manager_finalize (GObject *object)
{
	GtkSourceStyleManager *mgr;
	GSList *schemes;

	mgr = GTK_SOURCE_STYLE_MANAGER (object);

	schemes = mgr->priv->schemes;
	mgr->priv->schemes = NULL;
	g_slist_foreach (schemes, (GFunc) g_object_unref, NULL);
	g_slist_free (schemes);
	g_strfreev (mgr->priv->_dirs);
	g_slist_foreach (mgr->priv->added_files, (GFunc) g_free, NULL);
	g_slist_free (mgr->priv->added_files);

	G_OBJECT_CLASS (gtk_source_style_manager_parent_class)->finalize (object);
}

static void
gtk_source_style_manager_class_init (GtkSourceStyleManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize	= gtk_source_style_manager_finalize;
	object_class->set_property = gtk_source_style_manager_set_property;
	object_class->get_property = gtk_source_style_manager_get_property;

	g_object_class_install_property (object_class,
					 PROP_SEARCH_PATH,
					 g_param_spec_boxed ("search-path",
						 	     _("Style scheme directories"),
							     _("List of directories where the "
							       "style scheme files "
							       "are located"),
							     G_TYPE_STRV,
							     G_PARAM_READWRITE));

	signals[LIST_CHANGED] = g_signal_new ("list-changed",
					      G_OBJECT_CLASS_TYPE (object_class),
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (GtkSourceStyleManagerClass, list_changed),
					      NULL, NULL,
					      _gtksourceview_marshal_VOID__VOID,
					      G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof(GtkSourceStyleManagerPrivate));
}

static void
gtk_source_style_manager_init (GtkSourceStyleManager *mgr)
{
	mgr->priv = G_TYPE_INSTANCE_GET_PRIVATE (mgr, GTK_TYPE_SOURCE_STYLE_MANAGER,
						GtkSourceStyleManagerPrivate);
	mgr->priv->schemes = NULL;
	mgr->priv->_dirs = NULL;
	mgr->priv->need_reload = TRUE;
}

/**
 * gtk_source_style_manager_new:
 *
 * Creates a new style manager. If you do not need more than one style
 * manager then use gtk_source_style_manager_get_default() instead.
 *
 * Returns: a #GtkSourceStyleManager.
 **/
GtkSourceStyleManager *
gtk_source_style_manager_new (void)
{
	return g_object_new (GTK_TYPE_SOURCE_STYLE_MANAGER, NULL);
}

/**
 * gtk_source_style_manager_get_default:
 *
 * Returns the default #GtkSourceStyleManager instance.
 *
 * Returns: a #GtkSourceStyleManager. Return value is owned
 * by GtkSourceView library and must not be unref'ed.
 **/
GtkSourceStyleManager *
gtk_source_style_manager_get_default (void)
{
	static GtkSourceStyleManager *instance;

	if (instance == NULL)
	{
		instance = gtk_source_style_manager_new ();
		g_object_add_weak_pointer (G_OBJECT (instance),
					   (gpointer*) &instance);
	}

	return instance;
}

static gboolean
build_reference_chain (GtkSourceStyleScheme *scheme,
		       GHashTable           *hash,
		       GSList              **ret)
{
	GSList *chain;
	gboolean retval = TRUE;

	chain = g_slist_prepend (NULL, scheme);

	while (TRUE)
	{
		GtkSourceStyleScheme *parent_scheme;
		const gchar *parent_id;

		parent_id = _gtk_source_style_scheme_get_parent_id (scheme);

		if (parent_id == NULL)
			break;

		parent_scheme = g_hash_table_lookup (hash, parent_id);

		if (parent_scheme == NULL)
		{
			g_warning ("unknown parent scheme %s in scheme %s",
				   parent_id, gtk_source_style_scheme_get_id (scheme));
			retval = FALSE;
			break;
		}
		else if (g_slist_find (chain, parent_scheme) != NULL)
		{
			g_warning ("reference cycle in scheme %s", parent_id);
			retval = FALSE;
			break;
		}
		else
		{
			_gtk_source_style_scheme_set_parent (scheme, parent_scheme);
		}

		chain = g_slist_prepend (chain, parent_scheme);
		scheme = parent_scheme;
	}

	*ret = chain;
	return retval;
}

static GSList *
check_parents (GSList     *schemes,
	       GHashTable *hash)
{
	GSList *to_check;

	to_check = g_slist_copy (schemes);

	while (to_check != NULL)
	{
		GSList *chain;
		gboolean valid;

		valid = build_reference_chain (to_check->data, hash, &chain);

		while (chain != NULL)
		{
			GtkSourceStyleScheme *scheme = chain->data;
			to_check = g_slist_remove (to_check, scheme);

			if (!valid)
			{
				schemes = g_slist_remove (schemes, scheme);
				g_hash_table_remove (hash, gtk_source_style_scheme_get_id (scheme));
				g_object_unref (scheme);
			}

			chain = g_slist_delete_link (chain, chain);
		}
	}

	return schemes;
}

static void
gtk_source_style_manager_changed (GtkSourceStyleManager *mgr)
{
	if (!mgr->priv->need_reload)
	{
		mgr->priv->need_reload = TRUE;
		g_signal_emit (mgr, signals[LIST_CHANGED], 0);
	}
}

static GSList *
add_scheme_from_file_real (GSList                *schemes,
			   GHashTable            *schemes_hash,
			   const gchar           *filename,
			   GtkSourceStyleScheme **new_scheme)
{
	GtkSourceStyleScheme *scheme;

	scheme = _gtk_source_style_scheme_new_from_file (filename);

	if (scheme != NULL)
	{
		const gchar *id = gtk_source_style_scheme_get_id (scheme);
		GtkSourceStyleScheme *old;

		old = g_hash_table_lookup (schemes_hash, id);
		if (old != NULL)
			schemes = g_slist_remove (schemes, old);

		schemes = g_slist_prepend (schemes, scheme);
		g_hash_table_insert (schemes_hash, g_strdup (id), g_object_ref (scheme));
	}

	if (new_scheme != NULL)
		*new_scheme = scheme;

	return schemes;
}

/* Load all the scheme files found in the search path and
 * all the files that have been manually added so far.
 * If new_file is not NULL, it tries to add it to the list
 * of schemes and if it succeeds the new scheme is returned */
static GtkSourceStyleScheme *
gtk_source_style_manager_reload (GtkSourceStyleManager *mgr,
				 const gchar           *new_file)
{
	GtkSourceStyleScheme *new_scheme = NULL;
	GHashTable *schemes_hash;
	GSList *schemes = NULL;
	GSList *files;
	GSList *l;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr), NULL);

	schemes = mgr->priv->schemes;
	mgr->priv->schemes = NULL;
	g_slist_foreach (schemes, (GFunc) g_object_unref, NULL);
	g_slist_free (schemes);

	schemes_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	/* files in the search path */
	files = _gtk_source_view_get_file_list (gtk_source_style_manager_get_search_path (mgr),
						SCHEME_FILE_SUFFIX);

	for (l = files; l != NULL; l = l->next)
	{
		schemes = add_scheme_from_file_real (schemes,
						     schemes_hash,
						     l->data,
						     NULL);
	}

	g_slist_foreach (files, (GFunc) g_free, NULL);
	g_slist_free (files);

	/* files added in the past */
	for (l = mgr->priv->added_files; l != NULL; l = l->next)
	{
		schemes = add_scheme_from_file_real (schemes,
						     schemes_hash,
						     l->data,
						     NULL);
	}

	/* file we are adding (if any) */
	if (new_file)
	{
		schemes = add_scheme_from_file_real (schemes,
						     schemes_hash,
						     new_file,
						     &new_scheme);

		if (new_scheme != NULL)
		{
			/* can go away in check_parents() */
			g_object_add_weak_pointer (G_OBJECT (new_scheme),
						   (gpointer *)&new_scheme);
		}
	}

	schemes = check_parents (schemes, schemes_hash);
	g_hash_table_destroy (schemes_hash);

	if (new_scheme != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (new_scheme),
					      (gpointer *)&new_scheme);
	}

	mgr->priv->schemes = schemes;
	mgr->priv->need_reload = FALSE;

	return new_scheme;
}

/**
 * gtk_source_style_manager_add_scheme_from_file()
 * @manager: the style manager
 * @filename: file path of the scheme to add
 *
 * Add a style scheme loaded from @filename.
 *
 * Return: the id of the newly added scheme or NULL if an error occurs.
 */
const gchar *
gtk_source_style_manager_add_scheme_from_file (GtkSourceStyleManager *manager,
					       const gchar           *filename)
{
	GtkSourceStyleScheme *scheme;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	scheme = gtk_source_style_manager_reload (manager, filename);
	if (scheme == NULL)
		return NULL;

	/* append because it has to take priority in case of same id */
	manager->priv->added_files = g_slist_append (manager->priv->added_files,
						     g_strdup (filename));

	return gtk_source_style_scheme_get_id (scheme);
}

/**
 * gtk_source_style_manager_set_search_path:
 * @manager: a #GtkSourceStyleManager.
 * @dirs: a %NULL-terminated array of strings or %NULL.
 *
 * Sets the list of directories where the given language manager
 * looks for style files.
 * @dirs == %NULL resets directories list to the default.
 */
void
gtk_source_style_manager_set_search_path (GtkSourceStyleManager	*manager,
					  gchar		       **dirs)
{
	char **tmp;

	g_return_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager));

	tmp = manager->priv->_dirs;
	manager->priv->_dirs = g_strdupv (dirs);
	g_strfreev (tmp);

	g_object_notify (G_OBJECT (manager), "search-path");
	gtk_source_style_manager_changed (manager);
}

/**
 * gtk_source_style_manager_get_search_path:
 * @manager: a #GtkSourceStyleManager.
 *
 * Gets the list of directories where @manager looks for style files.
 *
 * Returns: %NULL-terminated array containg a list of style files directories.
 * It is owned by @manager and must not be modified or freed.
 */
gchar **
gtk_source_style_manager_get_search_path (GtkSourceStyleManager	*manager)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager), NULL);

	if (manager->priv->_dirs == NULL)
		manager->priv->_dirs = _gtk_source_view_get_default_dirs (STYLES_DIR, FALSE);

	return manager->priv->_dirs;
}

static void
reload_if_needed (GtkSourceStyleManager *mgr)
{
	if (mgr->priv->need_reload)
		gtk_source_style_manager_reload (mgr, NULL);
}

/**
 * gtk_source_style_manager_list_schemes:
 * @manager: a #GtkSourceStyleManager
 *
 * Returns the list of style schemes.
 *
 * Returns: a list of #GtkSourceStyleScheme objects. It must be freed with
 * g_slist_free(), but its elements must not be unref'ed
 **/
GSList *
gtk_source_style_manager_list_schemes (GtkSourceStyleManager *manager)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager), NULL);

	reload_if_needed (manager);

	return g_slist_copy (manager->priv->schemes);
}

/**
 * gtk_source_style_manager_get_scheme:
 * @manager: a #GtkSourceStyleManager
 * @scheme_id: style scheme id to find
 *
 * Looks up style scheme by id.
 *
 * Returns: a #GtkSourceStyleScheme object. Returned value is owned by
 * @manager and must not be unref'ed.
 **/
GtkSourceStyleScheme *
gtk_source_style_manager_get_scheme (GtkSourceStyleManager *manager,
				     const gchar           *scheme_id)
{
	GSList *l;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (manager), NULL);
	g_return_val_if_fail (scheme_id != NULL, NULL);

	reload_if_needed (manager);

	for (l = manager->priv->schemes; l != NULL; l = l->next)
		if (!strcmp (scheme_id, gtk_source_style_scheme_get_id (l->data)))
			return l->data;

	return NULL;
}

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
	gboolean	 need_reload;
};

enum {
	CHANGED,
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

	signals[CHANGED] = g_signal_new ("changed",
					 G_OBJECT_CLASS_TYPE (object_class),
					 G_SIGNAL_RUN_LAST,
					 G_STRUCT_OFFSET (GtkSourceStyleManagerClass, changed),
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
		g_signal_emit (mgr, signals[CHANGED], 0);
	}
}

static void
gtk_source_style_manager_reload (GtkSourceStyleManager *mgr)
{
	GHashTable *schemes_hash;
	GSList *schemes = NULL;
	GSList *files;
	GSList *l;

	g_return_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr));

	schemes = mgr->priv->schemes;
	mgr->priv->schemes = NULL;
	g_slist_foreach (schemes, (GFunc) g_object_unref, NULL);
	g_slist_free (schemes);

	schemes_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	files = _gtk_source_view_get_file_list (gtk_source_style_manager_get_search_path (mgr),
						SCHEME_FILE_SUFFIX);

	for (l = files; l != NULL; l = l->next)
	{
		GtkSourceStyleScheme *scheme;
		gchar *filename;

		filename = l->data;

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
	}

	schemes = check_parents (schemes, schemes_hash);
	g_hash_table_destroy (schemes_hash);

	g_slist_foreach (files, (GFunc) g_free, NULL);
	g_slist_free (files);

	mgr->priv->schemes = schemes;
	mgr->priv->need_reload = FALSE;
}

void
gtk_source_style_manager_set_search_path (GtkSourceStyleManager	*mgr,
					  gchar		       **dirs)
{
	char **tmp;

	g_return_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr));

	tmp = mgr->priv->_dirs;
	mgr->priv->_dirs = g_strdupv (dirs);
	g_strfreev (tmp);

	g_object_notify (G_OBJECT (mgr), "search-path");
	gtk_source_style_manager_changed (mgr);
}

gchar **
gtk_source_style_manager_get_search_path (GtkSourceStyleManager	*mgr)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr), NULL);

	if (mgr->priv->_dirs == NULL)
		mgr->priv->_dirs = _gtk_source_view_get_default_dirs (STYLES_DIR, FALSE);

	return mgr->priv->_dirs;
}

static void
reload_if_needed (GtkSourceStyleManager *mgr)
{
	if (mgr->priv->need_reload)
		gtk_source_style_manager_reload (mgr);
}

GSList *
gtk_source_style_manager_list_schemes (GtkSourceStyleManager *mgr)
{
	GSList *list;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr), NULL);

	reload_if_needed (mgr);

	list = g_slist_copy (mgr->priv->schemes);
	g_slist_foreach (list, (GFunc) g_object_ref, NULL);

	return list;
}

GtkSourceStyleScheme *
gtk_source_style_manager_get_scheme (GtkSourceStyleManager *mgr,
				     const gchar           *scheme_id)
{
	GSList *l;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_MANAGER (mgr), NULL);
	g_return_val_if_fail (scheme_id != NULL, NULL);

	reload_if_needed (mgr);

	for (l = mgr->priv->schemes; l != NULL; l = l->next)
		if (!strcmp (scheme_id, gtk_source_style_scheme_get_id (l->data)))
			return l->data;

	return NULL;
}

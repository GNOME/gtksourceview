/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcesnippet-private.h"
#include "gtksourcesnippetbundle-private.h"
#include "gtksourcesnippetmanager-private.h"
#include "gtksourceutils-private.h"

/**
 * SECTION:snippetmanager
 * @title: GtkSourceSnippetManager
 * @short_description: Provides access to #GtkSourceSnippet
 * @see_also: #GtkSourceSnippet
 *
 * #GtkSourceSnippetManager is an object which processes snippet description
 * files and creates #GtkSourceSnippet objects.
 *
 * Use gtk_source_snippet_manager_get_default() to retrieve the default
 * instance of #GtkSourceSnippetManager.
 *
 * Use gtk_source_snippet_manager_get_snippets() to retrieve snippets for
 * a given snippets.
 *
 * Since: 5.0
 */

#define SNIPPET_DIR         "snippets"
#define SNIPPET_FILE_SUFFIX ".snippets"

struct _GtkSourceSnippetManager
{
	GObject parent_instance;

	/* To reduce the number of duplicated strings, we use a GStringChunk
	 * so that all of the GtkSourceSnippetInfo structs can point to const
	 * data. The bundles use _gtk_source_snippet_manager_intern() to get
	 * an "interned" string inside this string chunk.
	 */
	GStringChunk *strings;

	/* The snippet search path to look up files containing snippets like
	 * "license.snippets".
	 */
	gchar **search_path;

	/* The GtkSourceSnippetBundle handles both parsing a single snippet
	 * file on disk as well as collecting all the parsed files together.
	 * The strings contained in it are "const", and reference @strings
	 * to reduce duplicated memory as well as fragmentation.
	 *
	 * When searching for matching snippets, by language, name etc, we
	 * query the @bundle.
	 */
	GtkSourceSnippetBundle *bundle;
};

enum {
	PROP_0,
	PROP_SEARCH_PATH,
	N_PROPS
};

static GtkSourceSnippetManager *default_instance;
static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GtkSourceSnippetManager, gtk_source_snippet_manager, G_TYPE_OBJECT)

static void
gtk_source_snippet_manager_dispose (GObject *object)
{
	GtkSourceSnippetManager *self = GTK_SOURCE_SNIPPET_MANAGER (object);

	if (self->bundle != NULL)
	{
		g_object_run_dispose (G_OBJECT (self->bundle));
	}

	G_OBJECT_CLASS (gtk_source_snippet_manager_parent_class)->dispose (object);
}

static void
gtk_source_snippet_manager_finalize (GObject *object)
{
	GtkSourceSnippetManager *self = GTK_SOURCE_SNIPPET_MANAGER (object);

	g_clear_object (&self->bundle);
	g_clear_pointer (&self->search_path, g_strfreev);
	g_clear_pointer (&self->strings, g_string_chunk_free);

	G_OBJECT_CLASS (gtk_source_snippet_manager_parent_class)->finalize (object);
}

static void
gtk_source_snippet_manager_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceSnippetManager *self = GTK_SOURCE_SNIPPET_MANAGER (object);

	switch (prop_id)
	{
	case PROP_SEARCH_PATH:
		gtk_source_snippet_manager_set_search_path (self, g_value_get_boxed (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_snippet_manager_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceSnippetManager *self = GTK_SOURCE_SNIPPET_MANAGER (object);

	switch (prop_id)
	{
	case PROP_SEARCH_PATH:
		g_value_set_boxed (value, gtk_source_snippet_manager_get_search_path (self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_snippet_manager_class_init (GtkSourceSnippetManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_snippet_manager_dispose;
	object_class->finalize = gtk_source_snippet_manager_finalize;
	object_class->set_property = gtk_source_snippet_manager_set_property;
	object_class->get_property = gtk_source_snippet_manager_get_property;

	/**
	 * GtkSourceSnippetManager:search-path:
	 *
	 * The "search-path" property contains a list of directories to search
	 * for files containing snippets (*.snippets).
	 *
	 * Since: 5.0
	 */
	properties[PROP_SEARCH_PATH] =
		g_param_spec_boxed ("search-path",
		                    "Snippet directories",
		                    "List of directories with snippet definitions (*.snippets)",
		                    G_TYPE_STRV,
		                    (G_PARAM_READWRITE |
		                     G_PARAM_EXPLICIT_NOTIFY |
		                     G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_snippet_manager_init (GtkSourceSnippetManager *self)
{
}

/**
 * gtk_source_snippet_manager_get_default:
 *
 * Returns the default #GtkSourceSnippetManager instance.
 *
 * Returns: (transfer none) (not nullable): a #GtkSourceSnippetManager which
 *   is owned by GtkSourceView library and must not be unref'd.
 *
 * Since: 5.0
 */
GtkSourceSnippetManager *
gtk_source_snippet_manager_get_default (void)
{
	if (default_instance == NULL)
	{
		GtkSourceSnippetManager *self;

		self = g_object_new (GTK_SOURCE_TYPE_SNIPPET_MANAGER, NULL);
		g_set_weak_pointer (&default_instance, self);
	}

	return default_instance;
}

GtkSourceSnippetManager *
_gtk_source_snippet_manager_peek_default (void)
{
	return default_instance;
}

const gchar *
_gtk_source_snippet_manager_intern (GtkSourceSnippetManager *self,
                                    const gchar             *str)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self), NULL);

	if (str == NULL)
	{
		return NULL;
	}

	if (self->strings == NULL)
	{
		self->strings = g_string_chunk_new (4096*2);
	}

	return g_string_chunk_insert_const (self->strings, str);
}

/**
 * gtk_source_snippet_manager_set_search_path:
 * @self: a #GtkSourceSnippetManager
 * @dirs: (nullable) (array zero-terminated=1): a %NULL-terminated array of
 *   strings or %NULL.
 *
 * Sets the list of directories in which the #GtkSourceSnippetManagerlooks for
 * snippet files.  If @dirs is %NULL, the search path is reset to default.
 *
 * <note>
 *   <para>
 *     At the moment this function can be called only before the
 *     snippet files are loaded for the first time. In practice
 *     to set a custom search path for a #GtkSourceSnippetManager,
 *     you have to call this function right after creating it.
 *   </para>
 * </note>
 *
 * Since: 5.0
 */
void
gtk_source_snippet_manager_set_search_path (GtkSourceSnippetManager *self,
                                            const gchar * const     *dirs)
{
	gchar **tmp;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self));

	tmp = self->search_path;

	if (dirs == NULL)
		self->search_path = _gtk_source_utils_get_default_dirs (SNIPPET_DIR);
	else
		self->search_path = g_strdupv ((gchar **)dirs);

	g_strfreev (tmp);

	g_object_notify_by_pspec (G_OBJECT (self),
	                          properties [PROP_SEARCH_PATH]);
}

/**
 * gtk_source_snippet_manager_get_search_path:
 * @self: a #GtkSourceSnippetManager.
 *
 * Gets the list directories where @self looks for snippet files.
 *
 * Returns: (array zero-terminated=1) (transfer none): %NULL-terminated array
 *   containing a list of snippet files directories.
 *   The array is owned by @lm and must not be modified.
 *
 * Since: 5.0
 */
const gchar * const *
gtk_source_snippet_manager_get_search_path (GtkSourceSnippetManager *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self), NULL);

	if (self->search_path == NULL)
		self->search_path = _gtk_source_utils_get_default_dirs (SNIPPET_DIR);

	return (const gchar * const *)self->search_path;
}

static void
ensure_snippets (GtkSourceSnippetManager *self)
{
	GtkSourceSnippetBundle *bundle;
	GSList *filenames;

	g_assert (GTK_SOURCE_IS_SNIPPET_MANAGER (self));

	filenames = _gtk_source_utils_get_file_list (
		(gchar **)gtk_source_snippet_manager_get_search_path (self),
		SNIPPET_FILE_SUFFIX,
		TRUE);

	bundle = _gtk_source_snippet_bundle_new ();

	for (const GSList *f = filenames; f; f = f->next)
	{
		const gchar *filename = f->data;
		GtkSourceSnippetBundle *parsed;

		parsed = _gtk_source_snippet_bundle_new_from_file (filename, self);

		if (parsed != NULL)
			_gtk_source_snippet_bundle_merge (bundle, parsed);
		else
			g_warning ("Error reading snippet file '%s'", filename);

		g_clear_object (&parsed);
	}

	g_clear_object (&self->bundle);
	self->bundle = g_steal_pointer (&bundle);

	g_slist_free_full (filenames, g_free);

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self->bundle));
}

/**
 * gtk_source_snippet_manager_list_groups:
 * @self: a #GtkSourceSnippetManager
 *
 * List all the known groups within the snippet manager.
 *
 * The result should be freed with g_free(), and the individual strings are
 * owned by @self and should never be freed by the caller.
 *
 * Returns: (transfer container) (array zero-terminated=1) (element-type utf8):
 *   An array of strings which should be freed with g_free().
 *
 * Since: 5.0
 */
const gchar **
gtk_source_snippet_manager_list_groups (GtkSourceSnippetManager *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self), NULL);

	ensure_snippets (self);

	return _gtk_source_snippet_bundle_list_groups (self->bundle);
}

/**
 * gtk_source_snippet_manager_list_matching:
 * @self: a #GtkSourceSnippetManager
 * @group: (nullable): a group name or %NULL
 * @language_id: (nullable): a #GtkSourceLanguage:id or %NULL
 * @trigger_prefix: (nullable): a prefix for a trigger to activate
 *
 * Queries the known snippets for those matching @group, @language_id, and/or
 * @trigger_prefix. If any of these are %NULL, they will be ignored when
 * filtering the available snippets.
 *
 * The #GListModel only contains information about the available snippets until
 * g_list_model_get_item() is called for a specific snippet. This helps reduce
 * the number of #GObject's that are created at runtime to those needed by
 * the calling application.
 *
 * Returns: (transfer full): a #GListModel of #GtkSourceSnippet.
 *
 * Since: 5.0
 */
GListModel *
gtk_source_snippet_manager_list_matching (GtkSourceSnippetManager *self,
                                          const gchar             *group,
                                          const gchar             *language_id,
                                          const gchar             *trigger_prefix)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self), NULL);

	ensure_snippets (self);

	return _gtk_source_snippet_bundle_list_matching (self->bundle, group, language_id, trigger_prefix);
}

/**
 * gtk_source_snippet_manager_get_snippet:
 * @self: a #GtkSourceSnippetManager
 * @group: (nullable): a group name or %NULL
 * @language_id: (nullable): a #GtkSourceLanguage:id or %NULL
 * @trigger: the trigger for the snippet
 *
 * Queries the known snippets for the first matching @group, @language_id,
 * and/or @trigger. If @group or @language_id are %NULL, they will be ignored.
 *
 * Returns: (transfer full) (nullable): a #GtkSourceSnippet or %NULL if no
 *   matching snippet was found.
 *
 * Since: 5.0
 */
GtkSourceSnippet *
gtk_source_snippet_manager_get_snippet (GtkSourceSnippetManager *self,
                                        const gchar             *group,
                                        const gchar             *language_id,
                                        const gchar             *trigger)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (self), NULL);

	ensure_snippets (self);

	return _gtk_source_snippet_bundle_get_snippet (self->bundle, group, language_id, trigger);
}

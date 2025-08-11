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

#include "gtksourcesnippet.h"
#include "gtksourcesnippetchunk-private.h"
#include "gtksourcesnippetbundle-private.h"
#include "gtksourcesnippetmanager-private.h"

typedef struct
{
	guint identifier;
	guint focus_position;
	const char *text;
} GtkSourceSnippetTooltip;

struct _GtkSourceSnippetBundle
{
	GObject     parent_instance;
	GArray     *infos;
	GArray     *tooltips;
};

typedef struct
{
	GtkSourceSnippetManager *manager;
	GtkSourceSnippetBundle  *self;
	gchar                   *group;
	gchar                   *name;
	gchar                   *description;
	gchar                   *trigger;
	gchar                  **languages;
	GString                 *text;
	guint                    last_identifier;
} ParseState;

static void list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceSnippetBundle, _gtk_source_snippet_bundle, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static const char *
find_tooltip (GtkSourceSnippetBundle *self,
              guint                   identifier,
              guint                   focus_position)
{
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (self));

	for (guint i = 0; i < self->tooltips->len; i++)
	{
		const GtkSourceSnippetTooltip *tooltip = &g_array_index (self->tooltips, GtkSourceSnippetTooltip, i);

		if (tooltip->identifier == identifier &&
		    tooltip->focus_position == focus_position)
		{
			return tooltip->text;
		}
	}

	return NULL;
}

static gint
compare_infos (const GtkSourceSnippetInfo *info_a,
	       const GtkSourceSnippetInfo *info_b)
{
	gint ret = g_strcmp0 (info_a->language, info_b->language);

	if (ret == 0)
	{
		ret = g_strcmp0 (info_a->trigger, info_b->trigger);
	}

	return ret;
}

static void
gtk_source_snippet_bundle_dispose (GObject *object)
{
	GtkSourceSnippetBundle *self = (GtkSourceSnippetBundle *)object;

	if (self->infos->len > 0)
	{
		g_array_remove_range (self->infos, 0, self->infos->len);
	}

	G_OBJECT_CLASS (_gtk_source_snippet_bundle_parent_class)->dispose (object);
}

static void
gtk_source_snippet_bundle_finalize (GObject *object)
{
	GtkSourceSnippetBundle *self = (GtkSourceSnippetBundle *)object;

	g_clear_pointer (&self->infos, g_array_unref);
	g_clear_pointer (&self->tooltips, g_array_unref);

	G_OBJECT_CLASS (_gtk_source_snippet_bundle_parent_class)->finalize (object);
}

static void
_gtk_source_snippet_bundle_class_init (GtkSourceSnippetBundleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_snippet_bundle_dispose;
	object_class->finalize = gtk_source_snippet_bundle_finalize;
}

static void
_gtk_source_snippet_bundle_init (GtkSourceSnippetBundle *self)
{
	self->infos = g_array_new (FALSE, FALSE, sizeof (GtkSourceSnippetInfo));
	self->tooltips = g_array_new (FALSE, FALSE, sizeof (GtkSourceSnippetTooltip));
}

static void
gtk_source_snippet_bundle_add (GtkSourceSnippetBundle     *self,
                               const GtkSourceSnippetInfo *info)
{
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (self));
	g_assert (info != NULL);

	/* If there is no name, and no trigger, then there is no way to
	 * instantiate the snippet. Just ignore it.
	 */
	if (info->name != NULL || info->trigger != NULL)
	{
		g_array_append_vals (self->infos, info, 1);
	}
}

static void
text_and_cdata (GMarkupParseContext  *context,
                const gchar          *text,
                gsize                 text_len,
                gpointer              user_data,
                GError              **error)
{
	ParseState *state = user_data;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));

	g_string_append_len (state->text, text, text_len);
}

static const GMarkupParser text_parser = {
	.text = text_and_cdata,
};

static void
elements_start_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        gpointer              user_data,
                        GError              **error)
{
	ParseState *state = user_data;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	if (g_strcmp0 (element_name, "text") == 0)
	{
		const char *languages = NULL;

		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
		                                  G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "languages", &languages,
		                                  G_MARKUP_COLLECT_INVALID))
			return;

		if (languages != NULL && languages[0] != 0)
		{
			char **strv = g_strsplit (languages, ";", 0);

			g_strfreev (state->languages);
			state->languages = g_steal_pointer (&strv);
		}

		g_markup_parse_context_push (context, &text_parser, state);
	}
	else if (g_strcmp0 (element_name, "tooltip") == 0)
	{
		GtkSourceSnippetTooltip tooltip;
		const char *position = NULL;
		const char *text = NULL;

		if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
		                                  G_MARKUP_COLLECT_STRING, "position", &position,
		                                  G_MARKUP_COLLECT_STRING, "text", &text,
		                                  G_MARKUP_COLLECT_INVALID))
			return;

		tooltip.identifier = state->last_identifier;
		tooltip.focus_position = g_ascii_strtoll (position, NULL, 10);
		tooltip.text = _gtk_source_snippet_manager_intern (state->manager, text);

		g_array_append_val (state->self->tooltips, tooltip);
	}
	else
	{
		g_set_error (error,
		             G_MARKUP_ERROR,
		             G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		             "Element %s not supported",
		             element_name);
	}
}

static void
elements_end_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      gpointer              user_data,
                      GError              **error)
{
	ParseState *state = user_data;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_MANAGER (state->manager));
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	if (g_strcmp0 (element_name, "text") == 0)
	{
		if (state->languages != NULL && state->languages[0] != NULL)
		{
			GtkSourceSnippetInfo info = {0};

			info.identifier = state->last_identifier;
			info.group = _gtk_source_snippet_manager_intern (state->manager, state->group);
			info.name = _gtk_source_snippet_manager_intern (state->manager, state->name);
			info.description = _gtk_source_snippet_manager_intern (state->manager, state->description);
			info.trigger = _gtk_source_snippet_manager_intern (state->manager, state->trigger);
			info.text = _gtk_source_snippet_manager_intern (state->manager, state->text->str);

			for (guint i = 0; state->languages[i]; i++)
			{
				char *stripped = g_strstrip (g_strdup (state->languages[i]));

				if (stripped != NULL && stripped[0] != 0)
				{
					info.language = _gtk_source_snippet_manager_intern (state->manager, stripped);
					gtk_source_snippet_bundle_add (state->self, &info);
				}

				g_free (stripped);
			}

		}

		g_clear_pointer (&state->languages, g_strfreev);
		g_string_truncate (state->text, 0);

		g_markup_parse_context_pop (context);
	}
}

static const GMarkupParser elements_parser = {
	.start_element = elements_start_element,
	.end_element = elements_end_element,
};

static void
snippet_start_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       gpointer              user_data,
                       GError              **error)
{
	ParseState *state = user_data;
	const gchar *_name = NULL;
	const gchar *_description = NULL;
	const gchar *trigger = NULL;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	if (g_strcmp0 (element_name, "snippet") != 0)
	{
		g_set_error (error,
		             G_MARKUP_ERROR,
		             G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		             "Element %s not supported",
		             element_name);
		return;
	}

	state->last_identifier++;

	if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
	                                  G_MARKUP_COLLECT_STRING, "trigger", &trigger,
	                                  G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "_name", &_name,
	                                  G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "_description", &_description,
	                                  G_MARKUP_COLLECT_INVALID))
		return;

	if (_name != NULL)
	{
		const gchar *name = g_dgettext (GETTEXT_PACKAGE, _name);

		if (g_strcmp0 (state->name, name) != 0)
		{
			g_free (state->name);
			state->name = g_strdup (name);
		}
	}

	if (_description != NULL)
	{
		const gchar *description = g_dgettext (GETTEXT_PACKAGE, _description);

		if (g_strcmp0 (state->description, description) != 0)
		{
			g_free (state->description);
			state->description = g_strdup (description);
		}
	}

	if (g_strcmp0 (state->trigger, trigger) != 0)
	{
		g_free (state->trigger);
		state->trigger = g_strdup (trigger);
	}

	g_markup_parse_context_push (context, &elements_parser, state);
}

static void
snippet_end_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     gpointer              user_data,
                     GError              **error)
{
	ParseState *state = user_data;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	g_clear_pointer (&state->trigger, g_free);
	g_clear_pointer (&state->name, g_free);

	g_markup_parse_context_pop (context);
}

static const GMarkupParser snippet_parser = {
	.start_element = snippet_start_element,
	.end_element = snippet_end_element,
};

static void
snippets_start_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        gpointer              user_data,
                        GError              **error)
{
	ParseState *state = user_data;
	const gchar *_group = NULL;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	if (g_strcmp0 (element_name, "snippets") != 0)
	{
		g_set_error (error,
		             G_MARKUP_ERROR,
		             G_MARKUP_ERROR_UNKNOWN_ELEMENT,
		             "Element %s not supported",
		             element_name);
		return;
	}

	if (!g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
	                                  G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "_group", &_group,
	                                  G_MARKUP_COLLECT_INVALID))
		return;

	if (_group != NULL)
	{
		g_free (state->group);
		state->group = g_strdup (g_dgettext (GETTEXT_PACKAGE, _group));
	}

	g_markup_parse_context_push (context, &snippet_parser, state);
}

static void
snippets_end_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      gpointer              user_data,
                      GError              **error)
{
	ParseState *state = user_data;

	g_assert (state != NULL);
	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (state->self));
	g_assert (element_name != NULL);

	g_clear_pointer (&state->group, g_free);

	g_markup_parse_context_pop (context);
}

static const GMarkupParser snippets_parser = {
	.start_element = snippets_start_element,
	.end_element = snippets_end_element,
};

static gboolean
gtk_source_snippet_bundle_parse (GtkSourceSnippetBundle  *self,
                                 GtkSourceSnippetManager *manager,
                                 const gchar             *path)
{
	gchar *contents = NULL;
	gsize length = 0;
	gboolean ret = FALSE;
	GFile *file;

	g_assert (GTK_SOURCE_IS_SNIPPET_BUNDLE (self));
	g_assert (path != NULL);

	if (g_str_has_prefix (path, "resource://"))
		file = g_file_new_for_uri (path);
	else
		file = g_file_new_for_path (path);

	if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL))
	{
		GMarkupParseContext *context;
		ParseState state = {0};

		state.self = self;
		state.manager = manager;
		state.text = g_string_new (NULL);
		state.last_identifier = 0;

		context = g_markup_parse_context_new (&snippets_parser,
		                                      (G_MARKUP_TREAT_CDATA_AS_TEXT |
		                                       G_MARKUP_PREFIX_ERROR_POSITION),
		                                       &state, NULL);

		ret = g_markup_parse_context_parse (context, contents, length, NULL);

		g_clear_pointer (&state.description, g_free);
		g_clear_pointer (&state.languages, g_strfreev);
		g_clear_pointer (&state.name, g_free);
		g_clear_pointer (&state.trigger, g_free);
		g_clear_pointer (&state.group, g_free);
		g_string_free (state.text, TRUE);

		g_markup_parse_context_free (context);
		g_free (contents);

		g_array_sort (self->infos, (GCompareFunc) compare_infos);

#if 0
		for (guint i = 0; i < self->infos->len; i++)
		{
			GtkSourceSnippetInfo *info = &g_array_index (self->infos, GtkSourceSnippetInfo, i);
			g_print ("group=%s name=%s language=%s trigger=%s\n",
				 info->group, info->name, info->language, info->trigger);
		}
#endif
	}

	g_object_unref (file);

	return ret;
}


GtkSourceSnippetBundle *
_gtk_source_snippet_bundle_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SNIPPET_BUNDLE, NULL);
}

GtkSourceSnippetBundle *
_gtk_source_snippet_bundle_new_from_file (const gchar             *path,
                                          GtkSourceSnippetManager *manager)
{
	GtkSourceSnippetBundle *self;

	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_MANAGER (manager), NULL);

	self = _gtk_source_snippet_bundle_new ();

	if (!gtk_source_snippet_bundle_parse (self, manager, path))
	{
		g_clear_object (&self);
	}

	return g_steal_pointer (&self);
}

void
_gtk_source_snippet_bundle_merge (GtkSourceSnippetBundle *self,
                                  GtkSourceSnippetBundle *other)
{
	guint max_id = 0;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self));
	g_return_if_fail (!other || GTK_SOURCE_IS_SNIPPET_BUNDLE (other));

	if (other == NULL || other->infos->len == 0)
	{
		return;
	}

	for (guint i = 0; i < self->infos->len; i++)
	{
		const GtkSourceSnippetInfo *info = &g_array_index (self->infos, GtkSourceSnippetInfo, i);
		max_id = MAX (max_id, info->identifier);
	}

	for (guint i = 0; i < other->infos->len; i++)
	{
		GtkSourceSnippetInfo info = g_array_index (other->infos, GtkSourceSnippetInfo, i);
		info.identifier += max_id;
		g_array_append_val (self->infos, info);
	}

	g_array_sort (self->infos, (GCompareFunc) compare_infos);

	for (guint i = 0; i < other->tooltips->len; i++)
	{
		GtkSourceSnippetTooltip tooltip = g_array_index (other->tooltips, GtkSourceSnippetTooltip, i);
		tooltip.identifier += max_id;
		g_array_append_val (self->tooltips, tooltip);
	}
}

const gchar **
_gtk_source_snippet_bundle_list_groups (GtkSourceSnippetBundle *self)
{
	const char **ret;
	GHashTable *ht;
	guint len;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self), NULL);

	ht = g_hash_table_new (NULL, NULL);

	for (guint i = 0; i < self->infos->len; i++)
	{
		const GtkSourceSnippetInfo *info = &g_array_index (self->infos, GtkSourceSnippetInfo, i);

		/* We can use pointer comparison because all of these strings
		 * are interned using the same #GStringChunk with
		 * g_string_chunk_insert_const().
		 */
		if (!g_hash_table_contains (ht, info->group))
		{
			g_hash_table_add (ht, (gchar *)info->group);
		}
	}

	ret = (const gchar **)g_hash_table_get_keys_as_array (ht, &len);

	g_hash_table_unref (ht);

	return ret;
}

static GtkSourceSnippet *
create_snippet_from_info (GtkSourceSnippetBundle     *self,
                          const GtkSourceSnippetInfo *info)
{
	GtkSourceSnippet *snippet;
	GPtrArray *chunks = NULL;

	g_assert (info != NULL);

	if (info->text != NULL)
	{
		chunks = _gtk_source_snippet_bundle_parse_text (info->text, NULL);

		if (chunks == NULL)
		{
			GtkSourceSnippetChunk *chunk;

			/* If we failed to parse, then show the text unprocessed
			 * to the user so they at least get something in the
			 * editor to help them debug the issue.
			 */
			chunks = g_ptr_array_new_with_free_func (g_object_unref);
			chunk = gtk_source_snippet_chunk_new ();
			gtk_source_snippet_chunk_set_text (chunk, info->text);
			gtk_source_snippet_chunk_set_text_set (chunk, TRUE);
			g_ptr_array_add (chunks, g_object_ref_sink (chunk));
		}
	}

	snippet = gtk_source_snippet_new (info->trigger, info->language);
	gtk_source_snippet_set_description (snippet, info->description);
	gtk_source_snippet_set_name (snippet, info->name);

	if (chunks != NULL)
	{
		for (guint i = 0; i < chunks->len; i++)
		{
			GtkSourceSnippetChunk *chunk = g_ptr_array_index (chunks, i);

			if (chunk->focus_position >= 0)
			{
				gtk_source_snippet_chunk_set_tooltip_text (chunk,
				                                           find_tooltip (self,
				                                                         info->identifier,
				                                                         chunk->focus_position));
			}

			gtk_source_snippet_add_chunk (snippet, chunk);
		}
	}

	g_clear_pointer (&chunks, g_ptr_array_unref);

	return g_steal_pointer (&snippet);
}

GtkSourceSnippet *
_gtk_source_snippet_bundle_create_snippet (GtkSourceSnippetBundle     *self,
                                           const GtkSourceSnippetInfo *info)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	return create_snippet_from_info (self, info);
}

static gboolean
info_matches (const GtkSourceSnippetInfo *info,
              const gchar                *group,
              const gchar                *language_id,
              const gchar                *trigger,
              gboolean                    trigger_prefix_only)
{
	g_assert (info != NULL);

	if (group != NULL && g_strcmp0 (group, info->group) != 0)
		return FALSE;

	if (language_id != NULL)
	{
		/* If we got "" for language, skip it */
		if (info->language != NULL && info->language[0] == 0)
			return FALSE;

		if (g_strcmp0 (language_id, info->language) != 0)
			return FALSE;
	}

	if (trigger != NULL)
	{
		if (info->trigger == NULL)
			return FALSE;

		if (trigger_prefix_only)
		{
			if (!g_str_has_prefix (info->trigger, trigger))
				return FALSE;
		}
		else
		{
			if (!g_str_equal (trigger, info->trigger))
				return FALSE;
		}
	}

	return TRUE;
}

GtkSourceSnippet *
_gtk_source_snippet_bundle_get_snippet (GtkSourceSnippetBundle *self,
                                        const gchar            *group,
                                        const gchar            *language_id,
                                        const gchar            *trigger)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self), NULL);

	/* TODO: This could use bsearch(), but the complication here is that
	 *       we want to ignore fields when the key field is NULL and the
	 *       sort order for infos doesn't match what we are querying, so
	 *       we would need an alternate index.
	 */

	for (guint i = 0; i < self->infos->len; i++)
	{
		const GtkSourceSnippetInfo *info = &g_array_index (self->infos, GtkSourceSnippetInfo, i);

		if (info_matches (info, group, language_id, trigger, FALSE))
		{
			return create_snippet_from_info (self, info);
		}
	}

	return NULL;
}

GListModel *
_gtk_source_snippet_bundle_list_matching (GtkSourceSnippetBundle *self,
                                          const gchar            *group,
                                          const gchar            *language_id,
                                          const gchar            *trigger_prefix)
{
	GtkSourceSnippetBundle *ret;
	const char *last = NULL;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_BUNDLE (self), NULL);

	ret = _gtk_source_snippet_bundle_new ();

	for (guint i = 0; i < self->infos->len; i++)
	{
		const GtkSourceSnippetInfo *info = &g_array_index (self->infos, GtkSourceSnippetInfo, i);

		if (info_matches (info, group, language_id, trigger_prefix, TRUE))
		{
			if (info->trigger != NULL && last != info->trigger)
			{
				g_array_append_vals (ret->infos, info, 1);
				last = info->trigger;
			}
		}
	}

	g_array_set_size (ret->tooltips, self->tooltips->len);

	if (self->tooltips->len > 0)
	{
		memcpy (ret->tooltips->data,
			self->tooltips->data,
			sizeof (GtkSourceSnippetTooltip) * self->tooltips->len);
	}

	return G_LIST_MODEL (g_steal_pointer (&ret));
}

static GType
gtk_source_snippet_bundle_get_item_type (GListModel *model)
{
	return GTK_SOURCE_TYPE_SNIPPET;
}

static guint
gtk_source_snippet_bundle_get_n_items (GListModel *model)
{
	return GTK_SOURCE_SNIPPET_BUNDLE (model)->infos->len;
}

GtkSourceSnippetInfo *
_gtk_source_snippet_bundle_get_info (GtkSourceSnippetBundle *self,
                                     guint                   position)
{
	if (position >= self->infos->len)
	{
		return NULL;
	}

	return &g_array_index (self->infos, GtkSourceSnippetInfo, position);
}

static gpointer
gtk_source_snippet_bundle_get_item (GListModel *model,
                                    guint       position)
{
	GtkSourceSnippetBundle *self = GTK_SOURCE_SNIPPET_BUNDLE (model);

	if (position >= self->infos->len)
	{
		return NULL;
	}

	return create_snippet_from_info (self, &g_array_index (self->infos, GtkSourceSnippetInfo, position));
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_item_type = gtk_source_snippet_bundle_get_item_type;
	iface->get_n_items = gtk_source_snippet_bundle_get_n_items;
	iface->get_item = gtk_source_snippet_bundle_get_item;
}

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#include "gtksourcesearchsettings.h"

/**
 * GtkSourceSearchSettings:
 * 
 * Search settings.
 *
 * A `GtkSourceSearchSettings` object represents the settings of a search. The
 * search settings can be associated with one or several
 * [class@SearchContext]s.
 */

enum
{
	PROP_0,
	PROP_SEARCH_TEXT,
	PROP_CASE_SENSITIVE,
	PROP_AT_WORD_BOUNDARIES,
	PROP_WRAP_AROUND,
	PROP_REGEX_ENABLED,
	PROP_VISIBLE_ONLY,
	N_PROPS
};

typedef struct
{
	gchar *search_text;
	guint case_sensitive : 1;
	guint at_word_boundaries : 1;
	guint wrap_around : 1;
	guint regex_enabled : 1;
	guint visible_only : 1;
} GtkSourceSearchSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceSearchSettings, gtk_source_search_settings, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gtk_source_search_settings_finalize (GObject *object)
{
	GtkSourceSearchSettings *settings = GTK_SOURCE_SEARCH_SETTINGS (object);
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_free (priv->search_text);

	G_OBJECT_CLASS (gtk_source_search_settings_parent_class)->finalize (object);
}

static void
gtk_source_search_settings_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceSearchSettings *settings = GTK_SOURCE_SEARCH_SETTINGS (object);
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	switch (prop_id)
	{
		case PROP_SEARCH_TEXT:
			g_value_set_string (value, priv->search_text);
			break;

		case PROP_CASE_SENSITIVE:
			g_value_set_boolean (value, priv->case_sensitive);
			break;

		case PROP_AT_WORD_BOUNDARIES:
			g_value_set_boolean (value, priv->at_word_boundaries);
			break;

		case PROP_WRAP_AROUND:
			g_value_set_boolean (value, priv->wrap_around);
			break;

		case PROP_REGEX_ENABLED:
			g_value_set_boolean (value, priv->regex_enabled);
			break;

		case PROP_VISIBLE_ONLY:
			g_value_set_boolean (value, priv->visible_only);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_search_settings_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceSearchSettings *settings = GTK_SOURCE_SEARCH_SETTINGS (object);
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	switch (prop_id)
	{
		case PROP_SEARCH_TEXT:
			gtk_source_search_settings_set_search_text (settings, g_value_get_string (value));
			break;

		case PROP_CASE_SENSITIVE:
			priv->case_sensitive = g_value_get_boolean (value);
			break;

		case PROP_AT_WORD_BOUNDARIES:
			priv->at_word_boundaries = g_value_get_boolean (value);
			break;

		case PROP_WRAP_AROUND:
			priv->wrap_around = g_value_get_boolean (value);
			break;

		case PROP_REGEX_ENABLED:
			priv->regex_enabled = g_value_get_boolean (value);
			break;

		case PROP_VISIBLE_ONLY:
			priv->visible_only = g_value_get_boolean (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_search_settings_class_init (GtkSourceSearchSettingsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_search_settings_finalize;
	object_class->get_property = gtk_source_search_settings_get_property;
	object_class->set_property = gtk_source_search_settings_set_property;

	/**
	 * GtkSourceSearchSettings:search-text:
	 *
	 * A search string, or %NULL if the search is disabled. 
	 * 
	 * If the regular expression search is enabled, [property@SearchSettings:search-text] is
	 * the pattern.
	 */
	properties[PROP_SEARCH_TEXT] =
		g_param_spec_string ("search-text",
				     "Search text",
				     "The text to search",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSearchSettings:case-sensitive:
	 *
	 * Whether the search is case sensitive.
	 */
	properties[PROP_CASE_SENSITIVE] =
		g_param_spec_boolean ("case-sensitive",
				      "Case sensitive",
				      "Case sensitive",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSearchSettings:at-word-boundaries:
	 *
	 * If %TRUE, a search match must start and end a word. The match can
	 * span multiple words.
	 */
	properties[PROP_AT_WORD_BOUNDARIES] =
		g_param_spec_boolean ("at-word-boundaries",
				      "At word boundaries",
				      "Search at word boundaries",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSearchSettings:wrap-around:
	 *
	 * For a forward search, continue at the beginning of the buffer if no
	 * search occurrence is found. For a backward search, continue at the
	 * end of the buffer.
	 */
	properties[PROP_WRAP_AROUND] =
		g_param_spec_boolean ("wrap-around",
				      "Wrap around",
				      "Wrap around",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSearchSettings:regex-enabled:
	 *
	 * Search by regular expressions with
	 * [property@SearchSettings:search-text] as the pattern.
	 */
	properties[PROP_REGEX_ENABLED] =
		g_param_spec_boolean ("regex-enabled",
				      "Regex enabled",
				      "Whether to search by regular expression",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSearchSettings:visible-only:
	 *
	 * Exclude invisible text from the search.
	 * A search match may have invisible text interspersed.
	 *
	 * Since: 5.12
	 */
	properties[PROP_VISIBLE_ONLY] =
		g_param_spec_boolean ("visible-only",
				      "Visible only",
				      "Whether to exclude invisible text from the search",
				      TRUE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_search_settings_init (GtkSourceSearchSettings *self)
{
}

/**
 * gtk_source_search_settings_new:
 *
 * Creates a new search settings object.
 *
 * Returns: a new search settings object.
 */
GtkSourceSearchSettings *
gtk_source_search_settings_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SEARCH_SETTINGS, NULL);
}

/**
 * gtk_source_search_settings_set_search_text:
 * @settings: a #GtkSourceSearchSettings.
 * @search_text: (nullable): the nul-terminated text to search, or %NULL to disable the search.
 *
 * Sets the text to search. 
 * 
 * If @search_text is %NULL or is empty, the search will be disabled. A copy of @search_text 
 * will be made, so you can safely free @search_text after a call to this function.
 *
 * You may be interested to call [func@utils_unescape_search_text] before
 * this function.
 */
void
gtk_source_search_settings_set_search_text (GtkSourceSearchSettings *settings,
                                            const gchar             *search_text)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));
	g_return_if_fail (search_text == NULL || g_utf8_validate (search_text, -1, NULL));

	if ((priv->search_text == NULL &&
	     (search_text == NULL || search_text[0] == '\0')) ||
	    g_strcmp0 (priv->search_text, search_text) == 0)
	{
		return;
	}

	g_free (priv->search_text);

	if (search_text == NULL || search_text[0] == '\0')
	{
		priv->search_text = NULL;
	}
	else
	{
		priv->search_text = g_strdup (search_text);
	}

	g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_SEARCH_TEXT]);
}

/**
 * gtk_source_search_settings_get_search_text:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Gets the text to search. 
 * 
 * The return value must not be freed.
 *
 * You may be interested to call [func@utils_escape_search_text] after
 * this function.
 *
 * Returns: (nullable): the text to search, or %NULL if the search is disabled.
 */
const gchar *
gtk_source_search_settings_get_search_text (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), NULL);

	return priv->search_text;
}

/**
 * gtk_source_search_settings_set_case_sensitive:
 * @settings: a #GtkSourceSearchSettings.
 * @case_sensitive: the setting.
 *
 * Enables or disables the case sensitivity for the search.
 */
void
gtk_source_search_settings_set_case_sensitive (GtkSourceSearchSettings *settings,
                                               gboolean                 case_sensitive)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

	case_sensitive = case_sensitive != FALSE;

	if (priv->case_sensitive != case_sensitive)
	{
		priv->case_sensitive = case_sensitive;
		g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_CASE_SENSITIVE]);
	}
}

/**
 * gtk_source_search_settings_get_case_sensitive:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Returns: whether the search is case sensitive.
 */
gboolean
gtk_source_search_settings_get_case_sensitive (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), FALSE);

	return priv->case_sensitive;
}

/**
 * gtk_source_search_settings_set_at_word_boundaries:
 * @settings: a #GtkSourceSearchSettings.
 * @at_word_boundaries: the setting.
 *
 * Change whether the search is done at word boundaries. 
 * 
 * If @at_word_boundaries is %TRUE, a search match must start and end a word.
 * The match can span multiple words. See also [method@Gtk.TextIter.starts_word] and
 * [method@Gtk.TextIter.ends_word].
 */
void
gtk_source_search_settings_set_at_word_boundaries (GtkSourceSearchSettings *settings,
                                                   gboolean                 at_word_boundaries)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

	at_word_boundaries = at_word_boundaries != FALSE;

	if (priv->at_word_boundaries != at_word_boundaries)
	{
		priv->at_word_boundaries = at_word_boundaries;
		g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_AT_WORD_BOUNDARIES]);
	}
}

/**
 * gtk_source_search_settings_get_at_word_boundaries:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Returns: whether to search at word boundaries.
 */
gboolean
gtk_source_search_settings_get_at_word_boundaries (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), FALSE);

	return priv->at_word_boundaries;
}

/**
 * gtk_source_search_settings_set_wrap_around:
 * @settings: a #GtkSourceSearchSettings.
 * @wrap_around: the setting.
 *
 * Enables or disables the wrap around search. 
 * 
 * If @wrap_around is %TRUE, the forward search continues at the beginning of the buffer
 * if no search occurrences are found. Similarly, the backward search continues to search at
 * the end of the buffer.
 */
void
gtk_source_search_settings_set_wrap_around (GtkSourceSearchSettings *settings,
                                            gboolean                 wrap_around)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

	wrap_around = wrap_around != FALSE;

	if (priv->wrap_around != wrap_around)
	{
		priv->wrap_around = wrap_around;
		g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_WRAP_AROUND]);
	}
}

/**
 * gtk_source_search_settings_get_wrap_around:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Returns: whether to wrap around the search.
 */
gboolean
gtk_source_search_settings_get_wrap_around (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), FALSE);

	return priv->wrap_around;
}

/**
 * gtk_source_search_settings_set_regex_enabled:
 * @settings: a #GtkSourceSearchSettings.
 * @regex_enabled: the setting.
 *
 * Enables or disables whether to search by regular expressions.
 *
 * If enabled, the [property@SearchSettings:search-text] property contains the
 * pattern of the regular expression.
 *
 * [class@SearchContext] uses #GRegex when regex search is enabled. See the
 * [Regular expression syntax](https://developer.gnome.org/glib/stable/glib-regex-syntax.html)
 * page in the GLib reference manual.
 */
void
gtk_source_search_settings_set_regex_enabled (GtkSourceSearchSettings *settings,
                                              gboolean                 regex_enabled)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

	regex_enabled = regex_enabled != FALSE;

	if (priv->regex_enabled != regex_enabled)
	{
		priv->regex_enabled = regex_enabled;
		g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_REGEX_ENABLED]);
	}
}

/**
 * gtk_source_search_settings_get_regex_enabled:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Returns: whether to search by regular expressions.
 */
gboolean
gtk_source_search_settings_get_regex_enabled (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), FALSE);

	return priv->regex_enabled;
}

/**
 * gtk_source_search_settings_set_visible_only:
 * @settings: a #GtkSourceSearchSettings.
 * @visible_only: the setting.
 *
 * Enables or disables whether to exclude invisible text from the search.
 *
 * If enabled, only visible text will be searched.
 * A search match may have invisible text interspersed.
 *
 * Since: 5.12
 */
void
gtk_source_search_settings_set_visible_only (GtkSourceSearchSettings *settings,
                                             gboolean                 visible_only)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings));

	visible_only = visible_only != FALSE;

	if (priv->visible_only != visible_only)
	{
		priv->visible_only = visible_only;
		g_object_notify_by_pspec (G_OBJECT (settings), properties[PROP_VISIBLE_ONLY]);
	}
}

/**
 * gtk_source_search_settings_get_visible_only:
 * @settings: a #GtkSourceSearchSettings.
 *
 * Returns: whether to exclude invisible text from the search.
 *
 * Since: 5.12
 */
gboolean
gtk_source_search_settings_get_visible_only (GtkSourceSearchSettings *settings)
{
	GtkSourceSearchSettingsPrivate *priv = gtk_source_search_settings_get_instance_private (settings);

	g_return_val_if_fail (GTK_SOURCE_IS_SEARCH_SETTINGS (settings), FALSE);

	return priv->visible_only;
}

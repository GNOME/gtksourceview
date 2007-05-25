/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestylescheme.c
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

#include "gtksourceview-i18n.h"
#include "gtksourcestylescheme.h"
#include "gtksourceview.h"
#include "gtksourcelanguage-private.h"
#include <string.h>

#define STYLE_HAS_FOREGROUND(s) ((s) && ((s)->mask & GTK_SOURCE_STYLE_USE_FOREGROUND))
#define STYLE_HAS_BACKGROUND(s) ((s) && ((s)->mask & GTK_SOURCE_STYLE_USE_BACKGROUND))

#define STYLE_TEXT		"text"
#define STYLE_SELECTED		"text-selected"
#define STYLE_BRACKET_MATCH	"bracket-match"
#define STYLE_BRACKET_MISMATCH	"bracket-mismatch"
#define STYLE_CURSOR		"cursor"
#define STYLE_CURRENT_LINE	"current-line"

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME
};

struct _GtkSourceStyleSchemePrivate
{
	gchar *id;
	gchar *name;
	GtkSourceStyleScheme *parent;
	gchar *parent_id;
	GHashTable *styles;
};

G_DEFINE_TYPE (GtkSourceStyleScheme, gtk_source_style_scheme, G_TYPE_OBJECT)

static void
gtk_source_style_scheme_finalize (GObject *object)
{
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	g_hash_table_destroy (scheme->priv->styles);
	g_free (scheme->priv->id);
	g_free (scheme->priv->name);
	g_free (scheme->priv->parent_id);

	if (scheme->priv->parent != NULL)
		g_object_unref (scheme->priv->parent);

	G_OBJECT_CLASS (gtk_source_style_scheme_parent_class)->finalize (object);
}

static void
gtk_source_style_scheme_set_property (GObject 	   *object,
				      guint 	    prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
	char *tmp;
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	switch (prop_id)
	{
	    case PROP_ID:
		tmp = scheme->priv->id;
		scheme->priv->id = g_strdup (g_value_get_string (value));
		g_free (tmp);
		break;

	    case PROP_NAME:
		tmp = scheme->priv->name;
		scheme->priv->name = g_strdup (g_value_get_string (value));
		g_free (tmp);
		g_object_notify (object, "name");
		break;

	    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_style_scheme_get_property (GObject 	 *object,
				      guint 	  prop_id,
				      GValue 	 *value,
				      GParamSpec *pspec)
{
	GtkSourceStyleScheme *scheme = GTK_SOURCE_STYLE_SCHEME (object);

	switch (prop_id)
	{
	    case PROP_ID:
		    g_value_set_string (value, scheme->priv->id);
		    break;

	    case PROP_NAME:
		    g_value_set_string (value, scheme->priv->name);
		    break;

	    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_style_scheme_class_init (GtkSourceStyleSchemeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_style_scheme_finalize;
	object_class->set_property = gtk_source_style_scheme_set_property;
	object_class->get_property = gtk_source_style_scheme_get_property;

	/**
	 * GtkSourceStyleScheme:id:
	 *
	 * Style scheme id, a unique string used to identify the style scheme
	 * in #GtkSourceStyleManager.
	 *
	 * Since: 2.0
	 */
	g_object_class_install_property (object_class,
					 PROP_ID,
					 g_param_spec_string ("id",
						 	      _("Style scheme id"),
							      _("Style scheme id"),
							      NULL,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceStyleScheme:name:
	 *
	 * Style scheme name, a translatable string to present to user.
	 *
	 * Since: 2.0
	 */
	g_object_class_install_property (object_class,
					 PROP_NAME,
					 g_param_spec_string ("name",
						 	      _("Style scheme name"),
							      _("Style scheme name"),
							      NULL,
							      G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GtkSourceStyleSchemePrivate));
}

static void
gtk_source_style_scheme_init (GtkSourceStyleScheme *scheme)
{
	scheme->priv = G_TYPE_INSTANCE_GET_PRIVATE (scheme, GTK_TYPE_SOURCE_STYLE_SCHEME,
						    GtkSourceStyleSchemePrivate);
	scheme->priv->styles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
						      (GDestroyNotify) gtk_source_style_free);
}

/**
 * gtk_source_style_scheme_get_id:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: @scheme id.
 *
 * Since: 2.0
 */
const gchar *
gtk_source_style_scheme_get_id (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (scheme->priv->id != NULL, "");
	return scheme->priv->id;
}

/**
 * gtk_source_style_scheme_get_name:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: @scheme name.
 *
 * Since: 2.0
 */
const gchar *
gtk_source_style_scheme_get_name (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (scheme->priv->name != NULL, "");
	return scheme->priv->name;
}

/**
 * _gtk_source_style_scheme_new:
 * @id: scheme id.
 * @name: scheme name.
 *
 * Returns: new empty #GtkSourceStyleScheme.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_new (const gchar *id,
			      const gchar *name)
{
	GtkSourceStyleScheme *scheme;

	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	scheme = g_object_new (GTK_TYPE_SOURCE_STYLE_SCHEME,
			       "id", id, "name", name, NULL);

	return scheme;
}

/**
 * gtk_source_style_scheme_get_style:
 * @scheme: a #GtkSourceStyleScheme.
 * @style_name: style name to find.
 *
 * Returns: style which corresponds to @style_name in the @scheme,
 * or %NULL when no style with this name found. Free it with
 * gtk_source_style_free().
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_scheme_get_style (GtkSourceStyleScheme *scheme,
				   const gchar          *style_name)
{
	GtkSourceStyle *style = NULL;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	g_return_val_if_fail (style_name != NULL, NULL);

	style = g_hash_table_lookup (scheme->priv->styles, style_name);

	if (style != NULL)
		return gtk_source_style_copy (style);

	if (scheme->priv->parent != NULL)
		return gtk_source_style_scheme_get_style (scheme->priv->parent,
							  style_name);
	else
		return NULL;
}

/**
 * gtk_source_style_scheme_set_style:
 * @scheme: a #GtkSourceStyleScheme.
 * @name: style name.
 * @style: style to set or %NULL.
 *
 * Since: 2.0
 */
void
gtk_source_style_scheme_set_style (GtkSourceStyleScheme *scheme,
				   const gchar          *name,
				   const GtkSourceStyle *style)
{
	g_return_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (name != NULL);

	if (style != NULL)
		g_hash_table_insert (scheme->priv->styles, g_strdup (name),
				     gtk_source_style_copy (style));
	else
		g_hash_table_remove (scheme->priv->styles, name);
}

/**
 * gtk_source_style_scheme_get_matching_brackets_style:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: style which corresponds to "bracket-match" name, to use
 * in an editor. Free it with gtk_source_style_free().
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_scheme_get_matching_brackets_style (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	return gtk_source_style_scheme_get_style (scheme, STYLE_BRACKET_MATCH);
}

/**
 * gtk_source_style_scheme_get_current_line_color:
 * @scheme: a #GtkSourceStyleScheme.
 * @color: a #GdkColor structure to fill.
 *
 * Returns: %TRUE if @scheme has style for current line set, or %FALSE
 * otherwise.
 *
 * Since: 2.0
 */
gboolean
gtk_source_style_scheme_get_current_line_color (GtkSourceStyleScheme *scheme,
						GdkColor             *color)
{
	GtkSourceStyle *style;
	gboolean ret = FALSE;

	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), FALSE);
	g_return_val_if_fail (color != NULL, FALSE);

	style = gtk_source_style_scheme_get_style (scheme, STYLE_CURRENT_LINE);

	if (STYLE_HAS_FOREGROUND (style))
	{
		*color = style->foreground;
		ret = TRUE;
	}

	gtk_source_style_free (style);
	return ret;
}

static void
set_text_style (GtkWidget      *widget,
		GtkSourceStyle *style,
		GtkStateType    state)
{
	GdkColor *color;

	if (style != NULL && STYLE_HAS_BACKGROUND (style))
		color = &style->background;
	else
		color = NULL;

	gtk_widget_modify_base (widget, state, color);

	if (style != NULL && STYLE_HAS_FOREGROUND (style))
		color = &style->foreground;
	else
		color = NULL;

	gtk_widget_modify_text (widget, state, color);
}

static void
set_cursor_color (GtkWidget      *widget,
		  GtkSourceStyle *style)
{
	GdkColor *color;

	if (style != NULL && STYLE_HAS_FOREGROUND (style))
		color = &style->foreground;
	else
		color = &widget->style->text[GTK_STATE_NORMAL];

	g_print ("implement me\n");
}

/**
 * _gtk_source_style_scheme_apply:
 * @scheme: a #GtkSourceStyleScheme or NULL.
 * @widget: a #GtkWidget to apply styles to.
 *
 * Sets text colors from @scheme in the @widget.
 *
 * Since: 2.0
 */
void
_gtk_source_style_scheme_apply (GtkSourceStyleScheme *scheme,
				GtkWidget            *widget)
{
	g_return_if_fail (!scheme || GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (scheme != NULL)
	{
		GtkSourceStyle *style;

		gtk_widget_ensure_style (widget);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_TEXT);
		set_text_style (widget, style, GTK_STATE_NORMAL);
		set_text_style (widget, style, GTK_STATE_ACTIVE);
		set_text_style (widget, style, GTK_STATE_PRELIGHT);
		set_text_style (widget, style, GTK_STATE_INSENSITIVE);
		gtk_source_style_free (style);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_SELECTED);
		set_text_style (widget, style, GTK_STATE_SELECTED);
		gtk_source_style_free (style);

		style = gtk_source_style_scheme_get_style (scheme, STYLE_CURSOR);
		set_cursor_color (widget, style);
		gtk_source_style_free (style);
	}
	else
	{
		set_text_style (widget, NULL, GTK_STATE_NORMAL);
		set_text_style (widget, NULL, GTK_STATE_ACTIVE);
		set_text_style (widget, NULL, GTK_STATE_PRELIGHT);
		set_text_style (widget, NULL, GTK_STATE_INSENSITIVE);
		set_text_style (widget, NULL, GTK_STATE_SELECTED);
		set_cursor_color (widget, NULL);
	}
}

/* --- PARSER ---------------------------------------------------------------- */

typedef struct {
	GtkSourceStyleScheme *scheme;
	gboolean done;
} ParserData;

#define ERROR_QUARK (g_quark_from_static_string ("gtk-source-style-scheme-parser-error"))

static gboolean
str_to_bool (const gchar *string)
{
	return !g_ascii_strcasecmp (string, "true") ||
		!g_ascii_strcasecmp (string, "yes") ||
		!g_ascii_strcasecmp (string, "1");
}

static gboolean
parse_style (GtkSourceStyleScheme *scheme,
	     const gchar         **names,
	     const gchar         **values,
	     gchar               **style_name_p,
	     GtkSourceStyle      **style_p,
	     GError              **error)
{
	GtkSourceStyle *use_style = NULL;
	GtkSourceStyle *result = NULL;
	const gchar *fg = NULL, *bg = NULL;
	const gchar *style_name = NULL;
	GtkSourceStyleMask mask = 0;
	gboolean bold = FALSE;
	gboolean italic = FALSE;
	gboolean underline = FALSE;
	gboolean strikethrough = FALSE;

	for ( ; names && *names; names++, values++)
	{
		const gchar *name = *names;
		const gchar *val = *values;

		if (!strcmp (name, "name"))
			style_name = val;
		else if (!strcmp (name, "foreground"))
			fg = val;
		else if (!strcmp (name, "background"))
			bg = val;
		else if (!strcmp (name, "italic"))
		{
			mask |= GTK_SOURCE_STYLE_USE_ITALIC;
			italic = str_to_bool (val);
		}
		else if (!strcmp (name, "bold"))
		{
			mask |= GTK_SOURCE_STYLE_USE_BOLD;
			bold = str_to_bool (val);
		}
		else if (!strcmp (name, "underline"))
		{
			mask |= GTK_SOURCE_STYLE_USE_UNDERLINE;
			underline = str_to_bool (val);
		}
		else if (!strcmp (name, "strikethrough"))
		{
			mask |= GTK_SOURCE_STYLE_USE_STRIKETHROUGH;
			strikethrough = str_to_bool (val);
		}
		else if (!strcmp (name, "use-style"))
		{
			if (use_style != NULL)
			{
				g_set_error (error, ERROR_QUARK, 0,
					     "in style '%s': duplicated use-style attribute",
					     style_name ? style_name : "(null)");
				gtk_source_style_free (use_style);
				return FALSE;
			}

			use_style = gtk_source_style_scheme_get_style (scheme, val);

			if (!use_style)
			{
				g_set_error (error, ERROR_QUARK, 0,
					     "in style '%s': unknown style '%s'",
					     style_name ? style_name : "(null)", val);
				return FALSE;
			}
		}
		else
		{
			g_set_error (error, ERROR_QUARK, 0,
				     "in style '%s': unexpected attribute '%s'",
				     style_name ? style_name : "(null)", name);
			gtk_source_style_free (use_style);
			return FALSE;
		}
	}

	if (style_name == NULL)
	{
		g_set_error (error, ERROR_QUARK, 0, "name attribute missing");
		gtk_source_style_free (use_style);
		return FALSE;
	}

	if (use_style)
	{
		if (fg != NULL || bg != NULL || mask != 0)
		{
			g_set_error (error, ERROR_QUARK, 0,
				     "in style '%s': style attributes used along with use-style",
				     style_name);
			gtk_source_style_free (use_style);
			return FALSE;
		}

		result = use_style;
		use_style = NULL;
	}
	else
	{
		result = gtk_source_style_new (mask);
		result->bold = bold;
		result->italic = italic;
		result->underline = underline;
		result->strikethrough = strikethrough;

		if (fg != NULL)
		{
			if (gdk_color_parse (fg, &result->foreground))
				result->mask |= GTK_SOURCE_STYLE_USE_FOREGROUND;
			else
				g_warning ("in style '%s': invalid color '%s'",
					   style_name, fg);
		}

		if (bg != NULL)
		{
			if (gdk_color_parse (bg, &result->background))
				result->mask |= GTK_SOURCE_STYLE_USE_BACKGROUND;
			else
				g_warning ("in style '%s': invalid color '%s'",
					   style_name, bg);
		}
	}

	*style_p = result;
	*style_name_p = g_strdup (style_name);
	return TRUE;
}

static void
start_element (G_GNUC_UNUSED GMarkupParseContext *context,
	       const gchar         *element_name,
	       const gchar        **attribute_names,
	       const gchar        **attribute_values,
	       gpointer             user_data,
	       GError             **error)
{
	ParserData *data = user_data;
	GtkSourceStyle *style;
	gchar *style_name;

	if (data->done || (data->scheme == NULL && strcmp (element_name, "style-scheme") != 0))
	{
		g_set_error (error, ERROR_QUARK, 0,
			     "unexpected element '%s'",
			     element_name);
		return;
	}

	if (data->scheme == NULL)
	{
		data->scheme = g_object_new (GTK_TYPE_SOURCE_STYLE_SCHEME, NULL);

		while (attribute_names != NULL && *attribute_names != NULL)
		{
			gboolean error_set = FALSE;

			if (!strcmp (*attribute_names, "id"))
			{
				data->scheme->priv->id = g_strdup (*attribute_values);
			}
			else if (!strcmp (*attribute_names, "_name"))
			{
				if (data->scheme->priv->name != NULL)
				{
					g_set_error (error, ERROR_QUARK, 0,
						     "duplicated name attribute");
					error_set = TRUE;
				}
				else
				{
					data->scheme->priv->name = g_strdup (_(*attribute_values));
				}
			}
			else if (!strcmp (*attribute_names, "name"))
			{
				if (data->scheme->priv->name != NULL)
				{
					g_set_error (error, ERROR_QUARK, 0,
						     "duplicated name attribute");
					error_set = TRUE;
				}
				else
				{
					data->scheme->priv->name = g_strdup (*attribute_values);
				}
			}
			else if (!strcmp (*attribute_names, "parent-scheme"))
			{
				data->scheme->priv->parent_id = g_strdup (*attribute_values);
			}
			else
			{
				g_set_error (error, ERROR_QUARK, 0,
					     "unexpected attribute '%s' in element 'style-scheme'",
					     *attribute_names);
				error_set = TRUE;
			}

			if (error_set)
				return;

			attribute_names++;
			attribute_values++;
		}

		return;
	}

	if (strcmp (element_name, "style") != 0)
	{
		g_set_error (error, ERROR_QUARK, 0,
			     "unexpected element '%s'",
			     element_name);
		return;
	}

	if (!parse_style (data->scheme, attribute_names, attribute_values,
			  &style_name, &style, error))
	{
		return;
	}

	g_hash_table_insert (data->scheme->priv->styles, style_name, style);
}

static void
end_element (G_GNUC_UNUSED GMarkupParseContext *context,
	     const gchar         *element_name,
	     gpointer             user_data,
	     G_GNUC_UNUSED GError **error)
{
	ParserData *data = user_data;
	if (!strcmp (element_name, "style-scheme"))
		data->done = TRUE;
}

/**
 * _gtk_source_style_scheme_new_from_file:
 * @filename: file to parse.
 *
 * Returns: new #GtkSourceStyleScheme created from file, or
 * %NULL on error.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_new_from_file (const gchar *filename)
{
	GMarkupParseContext *ctx = NULL;
	GError *error = NULL;
	char *text = NULL;
	ParserData data;
	GMarkupParser parser = {start_element, end_element, NULL, NULL, NULL};

	g_return_val_if_fail (filename != NULL, NULL);

	if (!g_file_get_contents (filename, &text, NULL, &error))
	{
		g_warning ("could not load style scheme file '%s': %s",
			   filename, error->message);
		g_error_free (error);
		return NULL;
	}

	data.scheme = NULL;
	data.done = FALSE;

	ctx = g_markup_parse_context_new (&parser, 0, &data, NULL);

	if (!g_markup_parse_context_parse (ctx, text, -1, &error) ||
	    !g_markup_parse_context_end_parse (ctx, &error))
	{
		if (data.scheme != NULL)
			g_object_unref (data.scheme);
		data.scheme = NULL;
		g_warning ("could not load style scheme file '%s': %s",
			   filename, error->message);
		g_error_free (error);
	}

	g_markup_parse_context_free (ctx);
	g_free (text);
	return data.scheme;
}

/**
 * _gtk_source_style_scheme_get_parent_id:
 * @scheme: a #GtkSourceStyleScheme.
 *
 * Returns: parent style scheme id or %NULL.
 *
 * Since: 2.0
 */
const gchar *
_gtk_source_style_scheme_get_parent_id (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme), NULL);
	return scheme->priv->parent_id;
}

/**
 * _gtk_source_style_scheme_set_parent:
 * @scheme: a #GtkSourceStyleScheme.
 * @parent_scheme: parent #GtkSourceStyleScheme for @scheme.
 *
 * Sets @parent_scheme as parent scheme for @scheme, @scheme will
 * look for styles in @parent_scheme if it doesn't have style set
 * for given name.
 *
 * Since: 2.0
 */
void
_gtk_source_style_scheme_set_parent (GtkSourceStyleScheme *scheme,
				     GtkSourceStyleScheme *parent_scheme)
{
	g_return_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme));
	g_return_if_fail (!parent_scheme || GTK_IS_SOURCE_STYLE_SCHEME (parent_scheme));

	if (scheme->priv->parent != NULL)
		g_object_unref (scheme->priv->parent);
	if (parent_scheme)
		g_object_ref (parent_scheme);
	scheme->priv->parent = parent_scheme;
}

/**
 * _gtk_source_style_scheme_default_new:
 *
 * Returns: new default style scheme. Not clear what it means though.
 *
 * Since: 2.0
 */
GtkSourceStyleScheme *
_gtk_source_style_scheme_default_new (void)
{
	return _gtk_source_style_scheme_new ("gvim", "GVim");
}

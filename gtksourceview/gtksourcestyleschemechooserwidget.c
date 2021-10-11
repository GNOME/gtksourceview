/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014 - Christian Hergert
 * Copyright 2014 - Ignacio Casal Quinteiro
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
 * along with GtkSourceView. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#if ENABLE_FONT_CONFIG
# include <fontconfig/fontconfig.h>
# include <pango/pangocairo.h>
# include <pango/pangofc-fontmap.h>
#endif

#include "gtksourcestyleschemechooserwidget.h"
#include "gtksourcestyleschemechooser.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagemanager.h"
#include "gtksourcebuffer.h"
#include "gtksourceview.h"

/**
 * SECTION:styleschemechooserwidget
 * @Short_description: A widget for choosing style schemes
 * @Title: GtkSourceStyleSchemeChooserWidget
 * @See_also: #GtkSourceStyleSchemeChooserButton
 *
 * The #GtkSourceStyleSchemeChooserWidget widget lets the user select a
 * style scheme. By default, the chooser presents a predefined list
 * of style schemes.
 *
 * To change the initially selected style scheme,
 * use gtk_source_style_scheme_chooser_set_style_scheme().
 * To get the selected style scheme
 * use gtk_source_style_scheme_chooser_get_style_scheme().
 *
 * Since: 3.16
 */

typedef struct
{
	GtkListBox *list_box;
	GtkSourceStyleScheme *scheme;
} GtkSourceStyleSchemeChooserWidgetPrivate;

static void style_scheme_chooser_interface_init (GtkSourceStyleSchemeChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceStyleSchemeChooserWidget,
                         gtk_source_style_scheme_chooser_widget,
                         GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkSourceStyleSchemeChooserWidget)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER,
                                                style_scheme_chooser_interface_init))

enum
{
	PROP_0,
	PROP_STYLE_SCHEME
};

#if ENABLE_FONT_CONFIG
static FcConfig *map_font_config;
static GtkCssProvider *css_provider;

static void
load_override_font (GtkWidget *widget)
{
	PangoFontDescription *font_desc;
	PangoFontMap *font_map;

	if (g_once_init_enter (&map_font_config))
	{
		const gchar *font_path = PACKAGE_DATADIR"/fonts/BuilderBlocks.ttf";
		FcConfig *config = FcInitLoadConfigAndFonts ();

		if (!g_file_test (font_path, G_FILE_TEST_IS_REGULAR))
			g_debug ("\"%s\" is missing or inaccessible", font_path);

		FcConfigAppFontAddFile (config, (const FcChar8 *)font_path);

		css_provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_data (css_provider, "textview, textview text { font-family: BuilderBlocks; font-size: 10pt; }", -1);

		g_once_init_leave (&map_font_config, config);
	}

	font_map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
	pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (font_map), map_font_config);
	gtk_widget_set_font_map (widget, font_map);
	font_desc = pango_font_description_from_string ("BuilderBlocks 8");

	g_assert (map_font_config != NULL);
	g_assert (font_map != NULL);
	g_assert (font_desc != NULL);

	gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
					GTK_STYLE_PROVIDER (css_provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION-1);

	pango_font_description_free (font_desc);
	g_object_unref (font_map);
}
#endif

static void
gtk_source_style_scheme_chooser_widget_dispose (GObject *object)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (object);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);

	g_clear_pointer ((GtkWidget **)&priv->list_box, gtk_widget_unparent);
	g_clear_object (&priv->scheme);

	G_OBJECT_CLASS (gtk_source_style_scheme_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_source_style_scheme_chooser_widget_get_property (GObject    *object,
                                                     guint       prop_id,
                                                     GValue     *value,
                                                     GParamSpec *pspec)
{
	switch (prop_id)
	{
		case PROP_STYLE_SCHEME:
			g_value_set_object (value,
			                    gtk_source_style_scheme_chooser_get_style_scheme (GTK_SOURCE_STYLE_SCHEME_CHOOSER (object)));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_style_scheme_chooser_widget_set_property (GObject      *object,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec)
{
	switch (prop_id)
	{
		case PROP_STYLE_SCHEME:
			gtk_source_style_scheme_chooser_set_style_scheme (GTK_SOURCE_STYLE_SCHEME_CHOOSER (object),
			                                                  g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_style_scheme_chooser_widget_class_init (GtkSourceStyleSchemeChooserWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gtk_source_style_scheme_chooser_widget_dispose;
	object_class->get_property = gtk_source_style_scheme_chooser_widget_get_property;
	object_class->set_property = gtk_source_style_scheme_chooser_widget_set_property;

	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gtksourceview/ui/gtksourcestyleschemechooserwidget.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GtkSourceStyleSchemeChooserWidget, list_box);

	g_object_class_override_property (object_class, PROP_STYLE_SCHEME, "style-scheme");
}

static GtkWidget *
make_row (GtkSourceStyleScheme *scheme,
          GtkSourceLanguage    *language)
{
	PangoAttrList *attrs;
	GtkWidget *row;
	GtkSourceBuffer *buffer;
	GtkWidget *view;
	GtkWidget *overlay;
	GtkWidget *label;
	gchar *text;
	const char *name;
	GdkRGBA fg;
	gboolean has_fg;

	row = gtk_list_box_row_new ();
	name = gtk_source_style_scheme_get_name (scheme);
	has_fg = _gtk_source_style_scheme_get_text_color (scheme, &fg);

	g_object_set_data (G_OBJECT (row), "scheme", scheme);

	buffer = gtk_source_buffer_new_with_language (language);
	gtk_source_buffer_set_highlight_matching_brackets (buffer, FALSE);
	gtk_source_buffer_set_style_scheme (buffer, scheme);

	text = g_strdup_printf ("/* %s */\n#include <gtksourceview/gtksource.h>",
	                        gtk_source_style_scheme_get_name (scheme));
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
	g_free (text);

	overlay = gtk_overlay_new ();
	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), overlay);

	view = g_object_new (GTK_SOURCE_TYPE_VIEW,
	                     "buffer", buffer,
	                     "focusable", FALSE,
	                     "can-focus", FALSE,
	                     "cursor-visible", FALSE,
	                     "editable", FALSE,
	                     "right-margin-position", 30,
	                     "show-right-margin", TRUE,
	                     "top-margin", 6,
	                     "bottom-margin", 6,
	                     "left-margin", 6,
	                     "right-margin", 6,
	                     "height-request", 38,
	                     NULL);
	load_override_font (view);
	gtk_overlay_set_child (GTK_OVERLAY (overlay), view);

	if (!has_fg)
		gtk_style_context_get_color (gtk_widget_get_style_context (view), &fg);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_foreground_new (fg.red * 65535, fg.green * 65535, fg.blue * 65535));
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (0.9));

	label = g_object_new (GTK_TYPE_LABEL,
	                      "attributes", attrs,
			      "focusable", FALSE,
			      "can-focus", FALSE,
			      "hexpand", TRUE,
			      "vexpand", TRUE,
			      "yalign", 0.0f,
			      "xalign", 1.0f,
			      "label", name,
			      "selectable", FALSE,
			      "margin-top", 3,
			      "margin-end", 6,
			      NULL);
	gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);

	gtk_widget_show (row);
	pango_attr_list_unref (attrs);

	return row;
}

static void
on_row_selected (GtkListBox                        *list_box,
                 GtkListBoxRow                     *row,
                 GtkSourceStyleSchemeChooserWidget *widget)
{
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);

	if (row != NULL)
	{
		GtkSourceStyleScheme *scheme;

		scheme = g_object_get_data (G_OBJECT (row), "scheme");

		if (g_set_object (&priv->scheme, scheme))
		{
			g_object_notify (G_OBJECT (widget), "style-scheme");
		}
	}
}

static void
gtk_source_style_scheme_chooser_widget_populate (GtkSourceStyleSchemeChooserWidget *widget)
{
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);
	GtkSourceLanguageManager *lm;
	GtkSourceLanguage *lang;
	GtkSourceStyleSchemeManager *manager;
	const gchar * const *scheme_ids;
	GtkWidget *child;
	guint i;
	gboolean row_selected = FALSE;

	g_signal_handlers_block_by_func (priv->list_box, on_row_selected, widget);

	while ((child = gtk_widget_get_first_child (GTK_WIDGET (priv->list_box))))
	{
		gtk_list_box_remove (priv->list_box, child);
	}

	manager = gtk_source_style_scheme_manager_get_default ();
	scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

	lm = gtk_source_language_manager_get_default ();
	lang = gtk_source_language_manager_get_language (lm, "c");

	for (i = 0; scheme_ids [i]; i++)
	{
		GtkWidget *row;
		GtkSourceStyleScheme *scheme;

		scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids [i]);
		row = make_row (scheme, lang);
		gtk_list_box_insert (priv->list_box, GTK_WIDGET (row), -1);

		if (scheme == priv->scheme)
		{
			gtk_list_box_select_row (priv->list_box, GTK_LIST_BOX_ROW (row));

			row_selected = TRUE;
		}
	}

	g_signal_handlers_unblock_by_func (priv->list_box, on_row_selected, widget);

	/* The current scheme may have been removed so select the default one */
	if (!row_selected)
	{
		gtk_source_style_scheme_chooser_set_style_scheme (GTK_SOURCE_STYLE_SCHEME_CHOOSER (widget),
		                                                  _gtk_source_style_scheme_get_default ());
	}
}

static void
on_scheme_ids_changed (GtkSourceStyleSchemeManager       *manager,
                       GParamSpec                        *pspec,
                       GtkSourceStyleSchemeChooserWidget *widget)
{
	gtk_source_style_scheme_chooser_widget_populate (widget);
}

static void
gtk_source_style_scheme_chooser_widget_init (GtkSourceStyleSchemeChooserWidget *widget)
{
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);
	GtkSourceStyleSchemeManager *manager;

	gtk_widget_init_template (GTK_WIDGET (widget));

	manager = gtk_source_style_scheme_manager_get_default ();
	g_signal_connect (manager,
	                  "notify::scheme-ids",
	                  G_CALLBACK (on_scheme_ids_changed),
	                  widget);

	gtk_source_style_scheme_chooser_widget_populate (widget);

	gtk_source_style_scheme_chooser_set_style_scheme (GTK_SOURCE_STYLE_SCHEME_CHOOSER (widget),
	                                                  _gtk_source_style_scheme_get_default ());

	g_signal_connect (priv->list_box,
	                  "row-selected",
	                  G_CALLBACK (on_row_selected),
	                  widget);
}

static GtkSourceStyleScheme *
gtk_source_style_scheme_chooser_widget_get_style_scheme (GtkSourceStyleSchemeChooser *chooser)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (chooser);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);

	return priv->scheme;
}

static void
gtk_source_style_scheme_chooser_widget_set_style_scheme (GtkSourceStyleSchemeChooser *chooser,
                                                         GtkSourceStyleScheme        *scheme)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (chooser);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);

	if (g_set_object (&priv->scheme, scheme))
	{
		GtkWidget *child;

		for (child = gtk_widget_get_first_child (GTK_WIDGET (priv->list_box));
		     child != NULL;
		     child = gtk_widget_get_next_sibling (child))
		{
			GtkListBoxRow *row = GTK_LIST_BOX_ROW (child);
			GtkSourceStyleScheme *cur;

			cur = g_object_get_data (G_OBJECT (row), "scheme");

			if (cur == scheme)
			{
				g_signal_handlers_block_by_func (priv->list_box, on_row_selected, widget);
				gtk_list_box_select_row (priv->list_box, row);
				g_signal_handlers_unblock_by_func (priv->list_box, on_row_selected, widget);
				break;
			}
		}

		g_object_notify (G_OBJECT (chooser), "style-scheme");
	}
}

static void
style_scheme_chooser_interface_init (GtkSourceStyleSchemeChooserInterface *iface)
{
	iface->get_style_scheme = gtk_source_style_scheme_chooser_widget_get_style_scheme;
	iface->set_style_scheme = gtk_source_style_scheme_chooser_widget_set_style_scheme;
}

/**
 * gtk_source_style_scheme_chooser_widget_new:
 *
 * Creates a new #GtkSourceStyleSchemeChooserWidget.
 *
 * Returns: a new  #GtkSourceStyleSchemeChooserWidget.
 *
 * Since: 3.16
 */
GtkWidget *
gtk_source_style_scheme_chooser_widget_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER_WIDGET, NULL);
}

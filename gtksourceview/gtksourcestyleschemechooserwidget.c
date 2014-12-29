/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcestyleschemechooserwidget.h
 * This file is part of gtksourceview
 *
 * Copyright (C) 2014 - Christian Hergert
 *               2014 - Ignacio Casal Quinteiro
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with gtksourceview. If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "gtksourcestyleschemechooserwidget.h"
#include "gtksourcestyleschemechooser.h"
#include "gtksourcestylescheme.h"
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
 * style scheme. By default, the chooser presents a prefined list
 * of style schemes.
 *
 * To change the initially selected style scheme,
 * use gtk_source_style_scheme_chooser_set_style_scheme().
 * To get the selected style scheme
 * use gtk_source_style_scheme_chooser_get_style_scheme().
 *
 * The #GtkSourceStyleSchemeChooserWidget is used in the
 * #GtkSourceStyleSchemeChooserButton
 * to provide a dialog for selecting style schemes.
 *
 * Since: 3.16
 */

typedef struct
{
	GtkListBox *list_box;
} GtkSourceStyleSchemeChooserWidgetPrivate;

static void gtk_source_style_scheme_chooser_widget_style_scheme_chooser_interface_init (GtkSourceStyleSchemeChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceStyleSchemeChooserWidget,
                         gtk_source_style_scheme_chooser_widget,
                         GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkSourceStyleSchemeChooserWidget)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER,
                                                gtk_source_style_scheme_chooser_widget_style_scheme_chooser_interface_init))

#define GET_PRIV(o) gtk_source_style_scheme_chooser_widget_get_instance_private (o)

enum
{
	PROP_0,
	PROP_STYLE_SCHEME
};

static GtkWidget *
make_row (GtkSourceStyleScheme *scheme,
          GtkSourceLanguage    *language)
{
	GtkWidget *row;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	gchar *text;

	row = gtk_list_box_row_new ();
	gtk_widget_show (row);

	g_object_set_data (G_OBJECT (row), "scheme_id", scheme);

	buffer = gtk_source_buffer_new_with_language (language);
	gtk_source_buffer_set_highlight_matching_brackets (buffer, FALSE);
	gtk_source_buffer_set_style_scheme (buffer, scheme);

	text = g_strdup_printf ("/* %s */\n#include <gtksourceview/gtksource.h>",
	                        gtk_source_style_scheme_get_name (scheme));
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);

	view = g_object_new (GTK_SOURCE_TYPE_VIEW,
	                     "buffer", buffer,
	                     "can-focus", FALSE,
	                     "cursor-visible", FALSE,
	                     "editable", FALSE,
	                     "visible", TRUE,
	                     "show-line-numbers", TRUE,
	                     "right-margin-position", 30,
	                     "show-right-margin", TRUE,
	                     NULL);
	gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (view));

	return row;
}

static void
gtk_source_style_scheme_chooser_widget_populate (GtkSourceStyleSchemeChooserWidget *widget)
{
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = GET_PRIV (widget);
	GtkSourceLanguageManager *lm;
	GtkSourceLanguage *lang;
	GtkSourceStyleSchemeManager *manager;
	const gchar * const *scheme_ids;
	guint i;

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
		gtk_container_add (GTK_CONTAINER (priv->list_box), GTK_WIDGET (row));
	}
}

static void
gtk_source_style_scheme_chooser_widget_constructed (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_style_scheme_chooser_widget_parent_class)->constructed (object);

	gtk_source_style_scheme_chooser_widget_populate (GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (object));
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

	object_class->get_property = gtk_source_style_scheme_chooser_widget_get_property;
	object_class->set_property = gtk_source_style_scheme_chooser_widget_set_property;
	object_class->constructed = gtk_source_style_scheme_chooser_widget_constructed;

	g_object_class_override_property (object_class, PROP_STYLE_SCHEME, "style-scheme");

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gtksourceview/ui/gtksourcestyleschemechooserwidget.ui");
	gtk_widget_class_bind_template_child_private (widget_class, GtkSourceStyleSchemeChooserWidget, list_box);
}

static void
gtk_source_style_scheme_chooser_widget_init (GtkSourceStyleSchemeChooserWidget *widget)
{
	gtk_widget_init_template (GTK_WIDGET (widget));
}

static GtkSourceStyleScheme *
gtk_source_style_scheme_chooser_widget_get_style_scheme (GtkSourceStyleSchemeChooser *chooser)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (chooser);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = GET_PRIV (widget);
	GtkListBoxRow *row;

	row = gtk_list_box_get_selected_row (priv->list_box);

	return row != NULL ? g_object_get_data (G_OBJECT (row), "scheme_id") : NULL;
}

static void
gtk_source_style_scheme_chooser_widget_set_style_scheme (GtkSourceStyleSchemeChooser *chooser,
                                                         GtkSourceStyleScheme        *scheme)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (chooser);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = GET_PRIV (widget);
	GList *children;
	GList *l;

	children = gtk_container_get_children (GTK_CONTAINER (priv->list_box));

	for (l = children; l != NULL; l = g_list_next (l))
	{
		GtkListBoxRow *row = l->data;
		GtkSourceStyleScheme *cur;

		cur = g_object_get_data (G_OBJECT (row), "scheme_id");

		if (cur == scheme)
		{
			gtk_list_box_select_row (priv->list_box, row);
			break;
		}
	}

	g_list_free (children);
}

static void
gtk_source_style_scheme_chooser_widget_style_scheme_chooser_interface_init (GtkSourceStyleSchemeChooserInterface *iface)
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

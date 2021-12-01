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

#include "gtksourcestyleschemechooserwidget.h"
#include "gtksourcestyleschemechooser.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcestyleschemepreview.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagemanager.h"
#include "gtksourcebuffer.h"
#include "gtksourceview.h"

/**
 * GtkSourceStyleSchemeChooserWidget:
 * 
 * A widget for choosing style schemes.
 *
 * The `GtkSourceStyleSchemeChooserWidget` widget lets the user select a
 * style scheme. By default, the chooser presents a predefined list
 * of style schemes.
 *
 * To change the initially selected style scheme,
 * use [method@StyleSchemeChooser.set_style_scheme].
 * To get the selected style scheme
 * use [method@StyleSchemeChooser.get_style_scheme].
 */

typedef struct
{
	GtkGrid              *grid;
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

static void
chooser_style_scheme_action_cb (GtkWidget  *widget,
			       	const char *action_name,
			       	GVariant   *param)
{
	GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
	GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, g_variant_get_string (param, NULL));

	if (scheme == NULL)
		return;

	g_object_set (widget, "style-scheme", scheme, NULL);
}

static void
gtk_source_style_scheme_chooser_widget_dispose (GObject *object)
{
	GtkSourceStyleSchemeChooserWidget *widget = GTK_SOURCE_STYLE_SCHEME_CHOOSER_WIDGET (object);
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);

	if (priv->grid != NULL)
	{
		gtk_widget_unparent (GTK_WIDGET (priv->grid));
		priv->grid = NULL;
	}

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
	gtk_widget_class_bind_template_child_private (widget_class, GtkSourceStyleSchemeChooserWidget, grid);

	g_object_class_override_property (object_class, PROP_STYLE_SCHEME, "style-scheme");

	gtk_widget_class_install_action (widget_class, "chooser.style-scheme", "s", chooser_style_scheme_action_cb);
}

static void
gtk_source_style_scheme_chooser_widget_populate (GtkSourceStyleSchemeChooserWidget *widget)
{
	GtkSourceStyleSchemeChooserWidgetPrivate *priv = gtk_source_style_scheme_chooser_widget_get_instance_private (widget);
	GtkSourceStyleSchemeManager *manager;
	const char * const *scheme_ids;
	GtkWidget *child;

	g_assert (GTK_SOURCE_IS_STYLE_SCHEME_CHOOSER (widget));

	while ((child = gtk_widget_get_first_child (GTK_WIDGET (priv->grid))))
	{
	       gtk_grid_remove (priv->grid, child);
	}

	manager = gtk_source_style_scheme_manager_get_default ();
	scheme_ids = gtk_source_style_scheme_manager_get_scheme_ids (manager);

	for (guint i = 0; scheme_ids[i]; i++)
	{
		GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, scheme_ids[i]);
		GtkWidget *preview = gtk_source_style_scheme_preview_new (scheme);

		gtk_actionable_set_action_name (GTK_ACTIONABLE (preview), "chooser.style-scheme");
		gtk_actionable_set_action_target (GTK_ACTIONABLE (preview), "s", scheme_ids[i]);
		gtk_widget_set_hexpand (preview, TRUE);
		gtk_grid_attach (priv->grid, preview, i % 2, i / 2, 1, 1);
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
	gtk_widget_init_template (GTK_WIDGET (widget));

	g_signal_connect_object (gtk_source_style_scheme_manager_get_default (),
	                         "notify::scheme-ids",
	                         G_CALLBACK (on_scheme_ids_changed),
	                         widget,
				 0);

	gtk_source_style_scheme_chooser_widget_populate (widget);
	gtk_source_style_scheme_chooser_set_style_scheme (GTK_SOURCE_STYLE_SCHEME_CHOOSER (widget),
	                                                  _gtk_source_style_scheme_get_default ());
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

	g_assert (GTK_SOURCE_IS_STYLE_SCHEME_CHOOSER_WIDGET (widget));

	if (!g_set_object (&priv->scheme, scheme))
		return;

	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (priv->grid));
	     child;
	     child = gtk_widget_get_next_sibling (child))
	{
		GtkSourceStyleSchemePreview *preview = GTK_SOURCE_STYLE_SCHEME_PREVIEW (child);
		GtkSourceStyleScheme *child_scheme = gtk_source_style_scheme_preview_get_scheme (preview);

		gtk_source_style_scheme_preview_set_selected (preview, scheme == child_scheme);
	}

	g_object_notify (G_OBJECT (chooser), "style-scheme");
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
 */
GtkWidget *
gtk_source_style_scheme_chooser_widget_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_STYLE_SCHEME_CHOOSER_WIDGET, NULL);
}

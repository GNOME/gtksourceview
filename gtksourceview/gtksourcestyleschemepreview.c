/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtksourcebuffer.h"
#include "gtksourcelanguage.h"
#include "gtksourcelanguagemanager.h"
#include "gtksourcestyle.h"
#include "gtksourcestylescheme.h"
#include "gtksourcestyleschemepreview.h"
#include "gtksourceview.h"
#include "gtksourceutils-private.h"

/**
 * GtkSourceStyleSchemePreview:
 *
 * A preview widget for [class@StyleScheme].
 *
 * This widget provides a convenient [class@Gtk.Widget] to preview a [class@StyleScheme].
 *
 * The [property@StyleSchemePreview:selected] property can be used to manage
 * the selection state of a single preview widget.
 *
 * Since: 5.4
 */

struct _GtkSourceStyleSchemePreview
{
	GtkWidget             parent_instance;
	GtkSourceStyleScheme *scheme;
	GtkImage             *image;
	char                 *action_name;
	GVariant             *action_target;
	guint                 selected : 1;
};

static void actionable_iface_init (GtkActionableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceStyleSchemePreview, gtk_source_style_scheme_preview, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, actionable_iface_init))

enum {
	PROP_0,
	PROP_SCHEME,
	PROP_SELECTED,
	N_PROPS,

	PROP_ACTION_NAME,
	PROP_ACTION_TARGET,
};

enum {
	ACTIVATE,
	N_SIGNALS
};

static GParamSpec *properties [N_PROPS];
static guint signals [N_SIGNALS];

static GtkCssProvider *css_provider;

static void
load_override_font (GtkSourceView *view,
                    PangoFontMap  *font_map)
{
	static gsize initialized;

	if (g_once_init_enter (&initialized))
	{
		css_provider = gtk_css_provider_new ();
		gtk_css_provider_load_from_string (css_provider,
		                                   "textview, textview text { font-family: BuilderBlocks; font-size: 4px; line-height: 8px; }\n"
		                                   "textview border.left gutter { padding-left: 2px; }\n");

		g_once_init_leave (&initialized, TRUE);
	}

	_gtk_source_widget_add_css_provider (GTK_WIDGET (view), css_provider,
	                                     GTK_STYLE_PROVIDER_PRIORITY_APPLICATION-1);

	if (font_map != NULL)
	{
		gtk_widget_set_font_map (GTK_WIDGET (view), font_map);
	}
}

static void
gtk_source_style_scheme_preview_real_activate (GtkSourceStyleSchemePreview *self)
{
	g_assert (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (self));

	if (self->action_name)
	{
		gtk_widget_activate_action_variant (GTK_WIDGET (self),
		                                    self->action_name,
		                                    self->action_target);
	}
}

static void
on_click_pressed_cb (GtkSourceStyleSchemePreview *self,
                     int                          n_presses,
                     double                       x,
                     double                       y,
                     GtkGestureClick             *click)
{
	g_assert (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (self));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	g_signal_emit (self, signals [ACTIVATE], 0);
}

static void
add_text (GtkSourceBuffer      *buffer,
	  GtkSourceStyleScheme *scheme)
{
	static const struct {
		const char *text;
		const char *style;
	} runs[] = {
		{ "XXXXXXXXXXX", "def:type" },
		{ "   ", NULL },
		{ "XXXXXXXXXXXXXXXXXXXX", "def:function" },
		{ "   ", NULL },
		{ "XXXXXXXXXXXXXXX", "def:comment" },
		{ "\n", NULL },
		{ "    ", NULL },
		{ "XXXXXXXXXXX", "def:preprocessor" },
		{ "    ", NULL },
		{ "XXXXX", "def:comment" },
		{ "    ", NULL },
		{ "XXXXXXXX", "def:string" },
		{ "    ", NULL },
		{ "XXXXXXXXXXXX", "def:decimal" },
		{ "\n", NULL },
		{ "    ", NULL },
		{ "XXXXXXXXXXX", "def:keyword" },
		{ "    ", NULL },
		{ "XXXXXXXXXXXXX", "def:boolean" },
		{ "    ", NULL },
		{ "XXXXXXX", "def:comment" },
		{ "\n", NULL },
		{ "    ", NULL },
		{ "XXXXXXXXX", "def:constant" },
		{ "    ", NULL },
		{ "XXX", "def:special-char" },
		{ "    ", NULL },
		{ "XXXXXXX", NULL },
		{ "    ", NULL },
		{ "XXXXXXXXXXX", "def:string" },
		{ "\n", NULL },
		{ "          ", NULL },
		{ "XXXXXXXXXXXXXXXXXXX", NULL },
		{ "\n", NULL },
		{ "XXXXXXXXXXXXXXX", NULL },
		{ "    ", NULL },
		{ "XXXXXX", "def:statement" },
		{ "    ", NULL },
		{ "XXXXXXXX", "def:identifier" },
	};
	GtkSourceLanguage *def;
	GHashTable *tags;
	GtkTextIter iter;

	def = gtk_source_language_manager_get_language (gtk_source_language_manager_get_default (), "def");
	tags = g_hash_table_new (NULL, NULL);

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &iter);

	for (guint i = 0; i < G_N_ELEMENTS (runs); i++)
	{
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, runs[i].text, -1);

		if (runs[i].style)
		{
			const char *fallback = runs[i].style;
			GtkSourceStyle *style = NULL;

			while (style == NULL && fallback != NULL)
			{
				style = gtk_source_style_scheme_get_style (scheme, fallback);

				if (style == NULL)
				{
					fallback = gtk_source_language_get_style_fallback (def, fallback);
				}
			}

			if (style != NULL)
			{
				GtkTextTag *tag;
				GtkTextIter begin;

				begin = iter;
				gtk_text_iter_backward_chars (&begin, g_utf8_strlen (runs[i].text, -1));

				if (!(tag = g_hash_table_lookup (tags, runs[i].style)))
				{
					tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer), NULL, NULL);
					gtk_source_style_apply (style, tag);
					g_hash_table_insert (tags, (gpointer)runs[i].style, tag);
				}

				gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer), tag, &begin, &iter);
			}
		}
	}

	g_hash_table_unref (tags);
}

static void
gtk_source_style_scheme_preview_constructed (GObject *object)
{
	GtkSourceStyleSchemePreview *self = (GtkSourceStyleSchemePreview *)object;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	PangoFontMap *font_map;
	const char *name;
	GtkLabel *label;

	G_OBJECT_CLASS (gtk_source_style_scheme_preview_parent_class)->constructed (object);

	if (self->scheme == NULL)
	{
		static gboolean warned;
		if (!warned)
			g_warning ("Attempt to create GtkSourceStyleSchemePreview without a scheme!");
		warned = TRUE;
		return;
	}

	name = gtk_source_style_scheme_get_name (self->scheme);
	gtk_widget_set_tooltip_text (GTK_WIDGET (self), name);

	view = g_object_new (GTK_SOURCE_TYPE_VIEW,
	                     "focusable", FALSE,
	                     "can-focus", FALSE,
	                     "cursor-visible", FALSE,
	                     "editable", FALSE,
	                     "right-margin-position", 48,
	                     "show-right-margin", TRUE,
	                     "top-margin", 6,
	                     "bottom-margin", 6,
	                     "left-margin", 9,
	                     "width-request", 120,
	                     "right-margin", 9,
	                     NULL);
	label = g_object_new (GTK_TYPE_LABEL, NULL);
	self->image = g_object_new (GTK_TYPE_IMAGE,
	                            "icon-name", "object-select-symbolic",
	                            "pixel-size", 14,
	                            "halign", GTK_ALIGN_END,
	                            "valign", GTK_ALIGN_END,
	                            "visible", FALSE,
	                            NULL);

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	gtk_source_buffer_set_style_scheme (buffer, self->scheme);
	add_text (buffer, self->scheme);

	font_map = _gtk_source_utils_get_builder_blocks ();

	if (font_map != NULL)
	{
		load_override_font (view, font_map);
	}

	gtk_widget_set_parent (GTK_WIDGET (view), GTK_WIDGET (self));
	gtk_widget_set_parent (GTK_WIDGET (label), GTK_WIDGET (self));
	gtk_widget_set_parent (GTK_WIDGET (self->image), GTK_WIDGET (self));

	gtk_widget_set_overflow (GTK_WIDGET (self), GTK_OVERFLOW_HIDDEN);
}

static void
gtk_source_style_scheme_preview_dispose (GObject *object)
{
	GtkSourceStyleSchemePreview *self = (GtkSourceStyleSchemePreview *)object;
	GtkWidget *child;

	while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
	{
		gtk_widget_unparent (child);
	}

	g_clear_pointer (&self->action_name, g_free);
	g_clear_pointer (&self->action_target, g_variant_unref);

	G_OBJECT_CLASS (gtk_source_style_scheme_preview_parent_class)->dispose (object);
}

static void
gtk_source_style_scheme_preview_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
	GtkSourceStyleSchemePreview *self = GTK_SOURCE_STYLE_SCHEME_PREVIEW (object);

	switch (prop_id)
	{
	case PROP_SCHEME:
		g_value_set_object (value, self->scheme);
		break;

	case PROP_SELECTED:
		g_value_set_boolean (value, gtk_source_style_scheme_preview_get_selected (self));
		break;

	case PROP_ACTION_NAME:
		g_value_set_string (value, self->action_name);
		break;

	case PROP_ACTION_TARGET:
		g_value_set_variant (value, self->action_target);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_style_scheme_preview_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
	GtkSourceStyleSchemePreview *self = GTK_SOURCE_STYLE_SCHEME_PREVIEW (object);

	switch (prop_id)
	{
	case PROP_SCHEME:
		self->scheme = g_value_dup_object (value);
		break;

	case PROP_SELECTED:
		gtk_source_style_scheme_preview_set_selected (self, g_value_get_boolean (value));
		break;

	case PROP_ACTION_NAME:
		g_free (self->action_name);
		self->action_name = g_value_dup_string (value);
		break;

	case PROP_ACTION_TARGET:
		g_clear_pointer (&self->action_target, g_variant_unref);
		self->action_target = g_value_dup_variant (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_style_scheme_preview_class_init (GtkSourceStyleSchemePreviewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->constructed = gtk_source_style_scheme_preview_constructed;
	object_class->dispose = gtk_source_style_scheme_preview_dispose;
	object_class->get_property = gtk_source_style_scheme_preview_get_property;
	object_class->set_property = gtk_source_style_scheme_preview_set_property;

	g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
	g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");

	properties [PROP_SCHEME] =
		g_param_spec_object ("scheme",
		                     "Scheme",
		                     "The style scheme to preview",
		                     GTK_SOURCE_TYPE_STYLE_SCHEME,
		                     (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	properties [PROP_SELECTED] =
		g_param_spec_boolean ("selected",
		                      "Selected",
		                      "If the preview should have the selected state",
		                      FALSE,
		                      (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	signals [ACTIVATE] = g_signal_new_class_handler ("activate",
	                                                 G_TYPE_FROM_CLASS (klass),
	                                                 G_SIGNAL_RUN_LAST,
	                                                 G_CALLBACK (gtk_source_style_scheme_preview_real_activate),
	                                                 NULL, NULL, NULL,
	                                                 G_TYPE_NONE, 0);

	gtk_widget_class_set_activate_signal (widget_class, signals [ACTIVATE]);
	gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
	gtk_widget_class_set_css_name (widget_class, "GtkSourceStyleSchemePreview");
}

static void
gtk_source_style_scheme_preview_init (GtkSourceStyleSchemePreview *self)
{
	GtkGesture *gesture;

	gesture = gtk_gesture_click_new ();
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
	g_signal_connect_object (gesture,
	                         "pressed",
	                         G_CALLBACK (on_click_pressed_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

/**
 * gtk_source_style_scheme_preview_new:
 * @scheme: a #GtkSourceStyleScheme
 *
 * Creates a new #GtkSourceStyleSchemePreview to preview the style scheme
 * provided in @scheme.
 *
 * Returns: a #GtkWidget
 *
 * Since: 5.4
 */
GtkWidget *
gtk_source_style_scheme_preview_new (GtkSourceStyleScheme *scheme)
{
	g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme), NULL);

	return g_object_new (GTK_SOURCE_TYPE_STYLE_SCHEME_PREVIEW,
	                     "scheme", scheme,
	                     NULL);
}

/**
 * gtk_source_style_scheme_preview_get_scheme:
 * @self: a #GtkSourceStyleSchemePreview
 *
 * Gets the #GtkSourceStyleScheme previewed by the widget.
 *
 * Returns: (transfer none): a #GtkSourceStyleScheme
 *
 * Since: 5.4
 */
GtkSourceStyleScheme *
gtk_source_style_scheme_preview_get_scheme (GtkSourceStyleSchemePreview *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (self), NULL);

	return self->scheme;
}

gboolean
gtk_source_style_scheme_preview_get_selected (GtkSourceStyleSchemePreview *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (self), FALSE);

	return self->selected;
}

void
gtk_source_style_scheme_preview_set_selected (GtkSourceStyleSchemePreview *self,
                                              gboolean                     selected)
{
	g_return_if_fail (GTK_SOURCE_IS_STYLE_SCHEME_PREVIEW (self));

	selected = !!selected;

	if (selected != self->selected)
	{
		self->selected = selected;

		if (selected)
		{
			gtk_widget_add_css_class (GTK_WIDGET (self), "selected");
			gtk_widget_set_visible (GTK_WIDGET (self->image), TRUE);
		}
		else
		{
			gtk_widget_remove_css_class (GTK_WIDGET (self), "selected");
			gtk_widget_set_visible (GTK_WIDGET (self->image), FALSE);
		}

		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_SELECTED]);
	}
}

static const char *
get_action_name (GtkActionable *actionable)
{
  return GTK_SOURCE_STYLE_SCHEME_PREVIEW (actionable)->action_name;
}

static void
set_action_name (GtkActionable *actionable,
                 const char    *action_name)
{
  g_object_set (actionable, "action-name", action_name, NULL);
}

static GVariant *
get_action_target (GtkActionable *actionable)
{
  return GTK_SOURCE_STYLE_SCHEME_PREVIEW (actionable)->action_target;
}

static void
set_action_target (GtkActionable *actionable,
                   GVariant      *action_target)
{
  g_object_set (actionable, "action-target", action_target, NULL);
}

static void
actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = get_action_name;
  iface->set_action_name = set_action_name;
  iface->get_action_target_value = get_action_target;
  iface->set_action_target_value = set_action_target;
}

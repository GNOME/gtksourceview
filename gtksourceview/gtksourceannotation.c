/*
 * gtksourceannotation.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
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

#include "gtksource.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourceannotation-private.h"
#include "gtksource-enumtypes.h"

/**
 * GtkSourceAnnotation:
 *
 * Represents an annotation added to [class@View], it has a [property@Annotation:line] property,
 * [property@Annotation:description], icon and a style.
 *
 * It will be displayed always at the end of a line.
 *
 * If the style is GTK_SOURCE_ANNOTATION_STYLE_NONE it will use the same color as [class@SpaceDrawer].
 *
 * Since: 5.18
 */

struct _GtkSourceAnnotation
{
	GObject                  parent_instance;
	char                    *description;
	GIcon                   *icon;
	int                      line;
	GtkSourceAnnotationStyle style;
	GdkRectangle             bounds;
	PangoLayout             *layout;
	char                    *font_string;
	int                      description_width;
	int                      description_height;
};

enum
{
	PROP_0,
	PROP_STYLE,
	PROP_DESCRIPTION,
	PROP_ICON,
	PROP_LINE,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GtkSourceAnnotation, gtk_source_annotation, G_TYPE_OBJECT)

static void
gtk_source_annotation_finalize (GObject *object)
{
	GtkSourceAnnotation *self = GTK_SOURCE_ANNOTATION (object);

	g_clear_pointer (&self->description, g_free);
	g_clear_object (&self->icon);
	g_clear_object (&self->layout);
	g_clear_pointer (&self->font_string, g_free);

	G_OBJECT_CLASS (gtk_source_annotation_parent_class)->finalize (object);
}

static void
gtk_source_annotation_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
	GtkSourceAnnotation *self;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (object));

	self = GTK_SOURCE_ANNOTATION (object);

	switch (prop_id)
	{
		case PROP_STYLE:
			g_value_set_enum (value, gtk_source_annotation_get_style (self));
			break;

		case PROP_DESCRIPTION:
			g_value_set_string (value, gtk_source_annotation_get_description (self));
			break;

		case PROP_ICON:
			g_value_set_object (value, gtk_source_annotation_get_icon (self));
			break;

		case PROP_LINE:
			g_value_set_uint (value, gtk_source_annotation_get_line (self));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_annotation_class_init (GtkSourceAnnotationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_annotation_finalize;
	object_class->get_property = gtk_source_annotation_get_property;

	/**
	 * GtkSourceAnnotation:description:
	 *
	 * The text description displayed at [property@Annotation:line]
	 *
	 * Since: 5.18
	 */
	properties[PROP_DESCRIPTION] =
		g_param_spec_string ("description", NULL, NULL,
		                     NULL,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceAnnotation:icon:
	 *
	 * The icon displayed at [property@Annotation:line]
	 *
	 * It will be displayed before the text description
	 *
	 * Since: 5.18
	 */
	properties[PROP_ICON] =
		g_param_spec_object ("icon", NULL, NULL,
				     G_TYPE_ICON,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceAnnotation:line:
	 *
	 * The line where to display the annotation
	 *
	 * Since: 5.18
	 */
	properties[PROP_LINE] =
		g_param_spec_uint ("line", NULL, NULL,
		                   0,
		                   G_MAXUINT,
		                   1,
		                   (G_PARAM_READABLE |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceAnnotation:style:
	 *
	 * The style of the annotation
	 *
	 * Since: 5.18
	 */
	properties[PROP_STYLE] =
		g_param_spec_enum ("style", NULL, NULL,
		                   GTK_SOURCE_TYPE_ANNOTATION_STYLE,
		                   GTK_SOURCE_ANNOTATION_STYLE_NONE,
		                   (G_PARAM_READABLE |
		                    G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_annotation_init (GtkSourceAnnotation *self)
{
	self->font_string = g_strdup ("");
}

/**
 * gtk_source_annotation_new:
 * @description: (nullable): the text to display or %NULL.
 * @icon: (nullable): the icon name to display or %NULL.
 * @line: the line where to display the annotation.
 * @style: #GtkSourceAnnotationStyle
 *
 * Returns: a new [class@Annotation]
 */
GtkSourceAnnotation *
gtk_source_annotation_new (const char              *description,
                           GIcon                   *icon,
                           int                      line,
                           GtkSourceAnnotationStyle style)
{
	GtkSourceAnnotation *self = g_object_new (GTK_SOURCE_TYPE_ANNOTATION, NULL);

	self->description = g_strdup (description);
	self->icon = icon;
	self->line = line;
	self->style = style;

	return self;
}

/**
 * gtk_source_annotation_get_description:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: the description text displayed
 */
const char *
gtk_source_annotation_get_description (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), NULL);

	return self->description ? self->description : "";
}

/**
 * gtk_source_annotation_get_icon:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: (transfer none) (nullable): a #GIcon or %NULL
 */
GIcon *
gtk_source_annotation_get_icon (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), NULL);

	return self->icon;
}

/**
 * gtk_source_annotation_get_line:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: the line number.
 */
int
gtk_source_annotation_get_line (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), 0);

	return self->line;
}

/**
 * gtk_source_annotation_get_style:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: the style of the annotation
 */
GtkSourceAnnotationStyle
gtk_source_annotation_get_style (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), FALSE);

	return self->style;
}

gboolean
_gtk_source_annotation_get_rect (GtkSourceAnnotation *self,
                                 GdkRectangle        *rect)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), FALSE);
	g_return_val_if_fail (rect != NULL, FALSE);

	*rect = self->bounds;

	return TRUE;
}

gboolean
_gtk_source_annotation_contains_point (GtkSourceAnnotation *self,
                                       int                  x,
                                       int                  y)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), FALSE);

	return gdk_rectangle_contains_point (&self->bounds, x, y);
}

static void
_gtk_source_annotation_ensure_updated_layout (GtkSourceAnnotation *self,
                                              GtkWidget           *widget)
{
	PangoFontDescription *font_desc;
	char *font_string;

	font_desc = pango_font_description_copy (pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
	font_string = pango_font_description_to_string (font_desc);

	if (g_set_str (&self->font_string, font_string))
	{
		g_clear_object (&self->layout);

		self->layout = gtk_widget_create_pango_layout (widget, self->description);
		pango_layout_set_font_description (self->layout, font_desc);
		pango_layout_get_pixel_size (self->layout, &self->description_width, &self->description_height);
	}

	g_clear_pointer (&font_string, g_free);
	g_clear_pointer (&font_desc, pango_font_description_free);
}

void
_gtk_source_annotation_draw (GtkSourceAnnotation *self,
                             GtkSnapshot         *snapshot,
                             GtkSourceView       *view,
                             GdkRectangle         rect,
                             const GdkRGBA       *color)
{
	GdkRGBA choosen_color;
	GdkRectangle draw_rect;
	graphene_matrix_t color_matrix;
	graphene_vec4_t color_vector;
	int window_x, window_y;
	int icon_size;
	int spacing;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (self));
	g_return_if_fail (snapshot != NULL);

	if (!self->description && !self->icon)
	{
		return;
	}

	if (self->style == GTK_SOURCE_ANNOTATION_STYLE_NONE)
	{
		choosen_color = *color;
	}
	else
	{
		GtkTextBuffer *buffer;
		GtkSourceStyleScheme *style_scheme;

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		style_scheme = gtk_source_buffer_get_style_scheme (GTK_SOURCE_BUFFER (buffer));

		switch (self->style)
		{
		case GTK_SOURCE_ANNOTATION_STYLE_WARNING:
			_gtk_source_style_scheme_get_warning_color (style_scheme, &choosen_color);
			break;

		case GTK_SOURCE_ANNOTATION_STYLE_ERROR:
			_gtk_source_style_scheme_get_error_color (style_scheme, &choosen_color);
			break;

		case GTK_SOURCE_ANNOTATION_STYLE_ACCENT:
			_gtk_source_style_scheme_get_accent_color (style_scheme, &choosen_color);
			break;

		case GTK_SOURCE_ANNOTATION_STYLE_NONE:
		default:
			g_assert_not_reached ();
		}
	}

	if (gdk_rgba_is_clear (&choosen_color))
		choosen_color = *color;

	spacing = rect.height * 0.4;
	icon_size = rect.height * 0.8;

	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
	                                       GTK_TEXT_WINDOW_TEXT,
	                                       rect.x,
	                                       rect.y,
	                                       &window_x,
	                                       &window_y);

	self->bounds = (GdkRectangle) {window_x, window_y, rect.width, rect.height};

	draw_rect = rect;

	if (self->icon != NULL)
	{
		GtkIconTheme *icon_theme;
		GtkIconPaintable *icon_paintable;

		icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
		icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme,
		                                                 self->icon,
		                                                 icon_size,
		                                                 1,
		                                                 GTK_TEXT_DIR_NONE,
		                                                 GTK_ICON_LOOKUP_PRELOAD);
		if (icon_paintable != NULL)
		{
			int icon_y_offset;

			gtk_snapshot_save (snapshot);

			icon_y_offset = (draw_rect.height - icon_size) / 2;
			gtk_snapshot_translate (snapshot,
			                        &GRAPHENE_POINT_INIT (draw_rect.x,
			                                              draw_rect.y + icon_y_offset));

			graphene_matrix_init_from_float (&color_matrix,
			                                 (float[16]) {0, 0, 0, 0,
			                                              0, 0, 0, 0,
			                                              0, 0, 0, 0,
			                                              0, 0, 0, choosen_color.alpha});

			graphene_vec4_init (&color_vector,
			                    choosen_color.red,
			                    choosen_color.green,
			                    choosen_color.blue,
			                    0);

			gtk_snapshot_push_color_matrix (snapshot,
			                                &color_matrix,
			                                &color_vector);

			gdk_paintable_snapshot (GDK_PAINTABLE (icon_paintable),
			                        snapshot,
			                        icon_size,
			                        icon_size);

			gtk_snapshot_pop (snapshot);
			gtk_snapshot_restore (snapshot);
			g_object_unref (icon_paintable);

			self->bounds.width += icon_size + spacing;

			draw_rect.x += icon_size + spacing;
		}
	}

	if (self->description && strlen (self->description) > 0)
	{
		_gtk_source_annotation_ensure_updated_layout (self, GTK_WIDGET (view));

		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot,
		                        &GRAPHENE_POINT_INIT (draw_rect.x,
		                                              draw_rect.y));
		gtk_snapshot_append_layout (snapshot,
		                            self->layout,
		                            &choosen_color);
		gtk_snapshot_restore (snapshot);

		self->bounds.width += self->description_width;
	}
}

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

#include "gtksourceannotation.h"

/**
 * GtkSourceAnnotation:
 *
 * Represents an annotation added to [class@View], it has a [property@Annotation:line] property,
 * [property@Annotation:text], icon-name and optionally a [property@Annotation:color] if you make
 * it with [method@Annotation.new_with_color].
 *
 * It will be displayed always at the end of a line.
 *
 * If the color is not set it will use the same color as [class@SpaceDrawer].
 *
 */

struct _GtkSourceAnnotation
{
	GObject         parent_instance;

	GdkRGBA         color;
	char           *text;
	char           *icon_name;
	int             line;

	gboolean        color_set;

	GdkRectangle    bounds;
};

enum
{
	PROP_0,
	PROP_COLOR,
	PROP_TEXT,
	PROP_ICON_NAME,
	PROP_LINE,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GtkSourceAnnotation, gtk_source_annotation, G_TYPE_OBJECT)

static void
gtk_source_annotation_finalize (GObject *object)
{
	GtkSourceAnnotation *self = GTK_SOURCE_ANNOTATION (object);

	g_free (self->text);
	g_free (self->icon_name);

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
		case PROP_COLOR:
			g_value_set_boxed(value, &self->color);
			break;

		case PROP_TEXT:
			g_value_set_string (value, gtk_source_annotation_get_text (self));
			break;

		case PROP_ICON_NAME:
			g_value_set_string (value, gtk_source_annotation_get_icon_name (self));
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
	 * GtkSourceAnnotation:text:
	 *
	 * The text displayed at [property@Annotation:line]
	 */
	properties [PROP_TEXT] =
		g_param_spec_string ("text",
		                     "Text",
		                     "",
		                     NULL,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceAnnotation:icon-name:
	 *
	 * The name of the icon displayed at [property@Annotation:icon-name]
	 *
	 * It will be displayed before the text
	 */
	properties [PROP_ICON_NAME] =
		g_param_spec_string ("icon-name",
		                     "Icon Name",
		                     "",
		                     NULL,
		                     (G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceAnnotation:icon-name:
	 *
	 * The line where to display the annotation
	 */
	properties [PROP_LINE] =
		g_param_spec_uint ("line",
		                   "Line",
		                   "",
		                   1,
		                   G_MAXUINT,
		                   1,
		                   (G_PARAM_READABLE |
		                    G_PARAM_STATIC_STRINGS));

	properties[PROP_COLOR] =
	g_param_spec_boxed("color",
	                   "Color",
	                   "",
	                   GDK_TYPE_RGBA,
	                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(object_class, N_PROPS, properties);
}

static void
gtk_source_annotation_init (GtkSourceAnnotation *self)
{
}

/**
 * gtk_source_annotation_new:
 * @text: (nullable): the text to display or %NULL.
 * @icon_name: (nullable): the icon name to display or %NULL.
 * @line: the line where to display the annotation.
 *
 * Used to create a new annotation, use [method@Annotation.new_with_color]
 * if you want to use a color.
 *
 * Returns: a new [class@Annotation]
 */
GtkSourceAnnotation *
gtk_source_annotation_new (char *text,
                           char *icon_name,
                           int   line)
{
	GtkSourceAnnotation *self = g_object_new (GTK_SOURCE_TYPE_ANNOTATION, NULL);

	self->text = g_strdup (text);
	self->icon_name = g_strdup (icon_name);

	self->line = line;

	self->color_set = FALSE;

	return self;
}

/**
 * gtk_source_annotation_new_with_color:
 * @text: (nullable): the text to display or %NULL.
 * @icon_name: (nullable): the icon name to display or %NULL.
 * @line: the line where to display the annotation.
 * @color: the color of the annotation.
 *
 * Used to create a new annotation with a color.
 *
 * Returns: a new [class@Annotation]
 */
GtkSourceAnnotation *
gtk_source_annotation_new_with_color (char   *text,
                                      char   *icon_name,
                                      int     line,
                                      GdkRGBA color)
{
	GtkSourceAnnotation *self = g_object_new (GTK_SOURCE_TYPE_ANNOTATION, NULL);

	self->text = g_strdup (text);
	self->icon_name = g_strdup (icon_name);

	self->line = line;
	self->color = color;

	self->color_set = TRUE;

	return self;
}

/**
 * gtk_source_annotation_get_text:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: the annotation text.
 */
const char *
gtk_source_annotation_get_text (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), NULL);

	return self->text ? self->text : "";
}

/**
 * gtk_source_annotation_get_icon_name:
 * @self: a #GtkSourceAnnotation
 *
 * Returns: the icon name.
 */
const char *
gtk_source_annotation_get_icon_name (GtkSourceAnnotation *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), NULL);

	return self->icon_name;
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
 * gtk_source_annotation_get_color:
 * @self: a #GtkSourceAnnotation
 * @color: a #GdkRGBA.
 *
 * Sets color as the color of this annotation.
 *
 * Returns: %TRUE if this annotation has a color, %FALSE otherwise.
 */
gboolean
gtk_source_annotation_get_color (GtkSourceAnnotation *self,
				 GdkRGBA             *color)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (self), FALSE);

	if (self->color_set)
	{
		color = &self->color;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
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

	return (x >= self->bounds.x && x < self->bounds.x + self->bounds.width &&
	        y >= self->bounds.y && y < self->bounds.y + self->bounds.height);
}

static PangoLayout *
_gtk_source_annotation_get_layout (GtkSourceAnnotation *self,
                                   GtkWidget           *widget)
{
	PangoFontDescription *font_desc;
	PangoLayout *layout;

	layout = gtk_widget_create_pango_layout (widget, self->text);

	font_desc = pango_font_description_copy (
		pango_context_get_font_description (gtk_widget_get_pango_context (widget)));
	pango_layout_set_font_description (layout, font_desc);
	pango_font_description_free (font_desc);

	return g_object_ref (layout);
}

void
_gtk_source_annotation_render (GtkSourceAnnotation *self,
                               GtkSnapshot         *snapshot,
                               GtkSourceView       *view,
                               int                  x,
                               int                  y,
                               int                  line_height,
                               const GdkRGBA       *color)
{
	GtkIconTheme *icon_theme;
	GtkIconPaintable *icon_paintable;
	PangoLayout *layout;
	int draw_x = x;
	int text_width, text_height;
	int window_x, window_y;
	int icon_size;
	int spacing;
	GdkRectangle bounds;
	graphene_matrix_t color_matrix;
	graphene_vec4_t color_vector;
	GdkRGBA choosen_color;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (self));
	g_return_if_fail (snapshot != NULL);

	if (self->color_set)
	{
		choosen_color = self->color;
	}
	else
	{
		choosen_color = *color;
	}

	bounds.x = x;
	bounds.y = y;
	bounds.width = 0;
	bounds.height = line_height;

	icon_size = line_height * 0.8;

	spacing = line_height * 0.4;

	if (self->icon_name)
	{
		icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
		icon_paintable = gtk_icon_theme_lookup_icon (icon_theme,
			                                    self->icon_name,
			                                    NULL,
			                                    icon_size,
			                                    1,
			                                    GTK_TEXT_DIR_NONE,
			                                    GTK_ICON_LOOKUP_PRELOAD);
		if (icon_paintable != NULL)
		{
			int icon_y_offset;
			gtk_snapshot_save (snapshot);

			icon_y_offset = (line_height - icon_size) / 2;
			gtk_snapshot_translate (snapshot,
				               &GRAPHENE_POINT_INIT (draw_x, y + icon_y_offset));

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

			bounds.width += icon_size;
			draw_x += icon_size + spacing;
			g_object_unref (icon_paintable);
		}
	}

	if (self->text && strlen (self->text) > 0)
	{
		layout = _gtk_source_annotation_get_layout (self, GTK_WIDGET (view));
		pango_layout_get_pixel_size (layout, &text_width, &text_height);

		if (line_height > 0 && bounds.width > 0)
			bounds.width += spacing;

		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (draw_x, y));
		gtk_snapshot_append_layout (snapshot, layout, &choosen_color);
		gtk_snapshot_restore (snapshot);

		bounds.width += text_width;
		bounds.height = MAX (bounds.height, text_height);

		g_object_unref (layout);
	}

	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (view),
	                                       GTK_TEXT_WINDOW_TEXT,
	                                       bounds.x, bounds.y,
	                                       &window_x, &window_y);
	bounds.x = window_x;
	bounds.y = window_y;

	self->bounds = bounds;
}

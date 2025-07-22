/*
 * This file is part of GtkSourceView
 *
 * Copyright 2008, 2011, 2016 - Paolo Borelli <pborelli@gnome.org>
 * Copyright 2008, 2010 - Ignacio Casal Quinteiro <icq@gnome.org>
 * Copyright 2010 - Garret Regier
 * Copyright 2013 - Arpad Borsos <arpad.borsos@googlemail.com>
 * Copyright 2015, 2016 - Sébastien Wilmet <swilmet@gnome.org>
 * Copyright 2016 - Christian Hergert <christian@hergert.me>
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

#include "gtksourcespacedrawer.h"
#include "gtksourcespacedrawer-private.h"
#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"
#include "gtksourceiter-private.h"
#include "gtksourcestylescheme.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcetag.h"
#include "gtksourcetrace.h"
#include "gtksourceview.h"

/**
 * GtkSourceSpaceDrawer:
 *
 * Represent white space characters with symbols.
 *
 * #GtkSourceSpaceDrawer provides a way to visualize white spaces, by drawing
 * symbols.
 *
 * Call [method@View.get_space_drawer] to get the `GtkSourceSpaceDrawer`
 * instance of a certain [class@View].
 *
 * By default, no white spaces are drawn because the
 * [property@SpaceDrawer:enable-matrix] is %FALSE.
 *
 * To draw white spaces, [method@SpaceDrawer.set_types_for_locations] can
 * be called to set the [property@SpaceDrawer:matrix] property (by default all
 * space types are enabled at all locations). Then call
 * [method@SpaceDrawer.set_enable_matrix].
 *
 * For a finer-grained method, there is also the [class@Tag]'s
 * [property@Tag:draw-spaces] property.
 *
 * # Example
 *
 * To draw non-breaking spaces everywhere and draw all types of trailing spaces
 * except newlines:
 * ```c
 * gtk_source_space_drawer_set_types_for_locations (space_drawer,
 *                                                  GTK_SOURCE_SPACE_LOCATION_ALL,
 *                                                  GTK_SOURCE_SPACE_TYPE_NBSP);
 *
 * gtk_source_space_drawer_set_types_for_locations (space_drawer,
 *                                                  GTK_SOURCE_SPACE_LOCATION_TRAILING,
 *                                                  GTK_SOURCE_SPACE_TYPE_ALL &
 *                                                  ~GTK_SOURCE_SPACE_TYPE_NEWLINE);
 *
 * gtk_source_space_drawer_set_enable_matrix (space_drawer, TRUE);
 * ```
 * ```python
 * space_drawer.set_types_for_locations(
 *     locations=GtkSource.SpaceLocationFlags.ALL,
 *     types=GtkSource.SpaceTypeFlags.NBSP,
 * )
 *
 * all_types_except_newline = GtkSource.SpaceTypeFlags(
 *     int(GtkSource.SpaceTypeFlags.ALL) & ~int(GtkSource.SpaceTypeFlags.NEWLINE)
 * )
 * space_drawer.set_types_for_locations(
 *     locations=GtkSource.SpaceLocationFlags.TRAILING,
 *     types=all_types_except_newline,
 * )
 *
 * space_drawer.set_enable_matrix(True)
 * ```
 *
 * # Use-case: draw unwanted white spaces
 *
 * A possible use-case is to draw only unwanted white spaces. Examples:
 *
 * - Draw all trailing spaces.
 * - If the indentation and alignment must be done with spaces, draw tabs.
 *
 * And non-breaking spaces can always be drawn, everywhere, to distinguish them
 * from normal spaces.
 */

/* A drawer specially designed for the International Space Station. It comes by
 * default with a DVD of Matrix, in case the astronauts are bored.
 */

typedef enum
{
	DRAW_TAB,
	DRAW_NARROW_NBSP,
	DRAW_NBSP,
	DRAW_SPACE,
	DRAW_NEWLINE,
	N_DRAW
} Draw;

typedef struct
{
	GskRenderNode *node;
	gint           width;
	gint           height;
} CachedNode;

struct _GtkSourceSpaceDrawer
{
	GObject                  parent_instance;

	GtkSourceSpaceTypeFlags *matrix;

	CachedNode               cached[N_DRAW];

	GdkRGBA                  color;

	guint                    color_set : 1;
	guint                    enable_matrix : 1;
};

enum
{
	PROP_0,
	PROP_ENABLE_MATRIX,
	PROP_MATRIX,
	N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE (GtkSourceSpaceDrawer, gtk_source_space_drawer, G_TYPE_OBJECT)

static void
gtk_source_space_drawer_purge_cache (GtkSourceSpaceDrawer *drawer)
{
	guint i;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));

	for (i = 0; i < G_N_ELEMENTS (drawer->cached); i++)
	{
		g_clear_pointer (&drawer->cached[i].node, gsk_render_node_unref);
	}
}

static gint
get_number_of_locations (void)
{
	gint num;
	gint flags;

	num = 0;
	flags = GTK_SOURCE_SPACE_LOCATION_ALL;

	while (flags != 0)
	{
		flags >>= 1;
		num++;
	}

	return num;
}

static GVariant *
get_default_matrix (void)
{
	GVariantBuilder builder;
	gint num_locations;
	gint i;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("au"));

	num_locations = get_number_of_locations ();

	for (i = 0; i < num_locations; i++)
	{
		GVariant *space_types;

		space_types = g_variant_new_uint32 (GTK_SOURCE_SPACE_TYPE_ALL);

		g_variant_builder_add_value (&builder, space_types);
	}

	return g_variant_builder_end (&builder);
}

static gboolean
is_zero_matrix (GtkSourceSpaceDrawer *drawer)
{
	gint num_locations;
	gint i;

	num_locations = get_number_of_locations ();

	for (i = 0; i < num_locations; i++)
	{
		if (drawer->matrix[i] != 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

static void
set_zero_matrix (GtkSourceSpaceDrawer *drawer)
{
	gint num_locations;
	gint i;
	gboolean changed = FALSE;

	num_locations = get_number_of_locations ();

	for (i = 0; i < num_locations; i++)
	{
		if (drawer->matrix[i] != 0)
		{
			drawer->matrix[i] = 0;
			changed = TRUE;
		}
	}

	if (changed)
	{
		g_object_notify_by_pspec (G_OBJECT (drawer), properties[PROP_MATRIX]);
	}
}

/* AND */
static GtkSourceSpaceTypeFlags
get_types_at_all_locations (GtkSourceSpaceDrawer        *drawer,
                            GtkSourceSpaceLocationFlags  locations)
{
	GtkSourceSpaceTypeFlags ret = GTK_SOURCE_SPACE_TYPE_ALL;
	gint index;
	gint num_locations;
	gboolean found;

	index = 0;
	num_locations = get_number_of_locations ();
	found = FALSE;

	while (locations != 0 && index < num_locations)
	{
		if ((locations & 1) == 1)
		{
			ret &= drawer->matrix[index];
			found = TRUE;
		}

		locations >>= 1;
		index++;
	}

	return found ? ret : GTK_SOURCE_SPACE_TYPE_NONE;
}

/* OR */
static GtkSourceSpaceTypeFlags
get_types_at_any_locations (GtkSourceSpaceDrawer        *drawer,
                            GtkSourceSpaceLocationFlags  locations)
{
	GtkSourceSpaceTypeFlags ret = GTK_SOURCE_SPACE_TYPE_NONE;
	gint index;
	gint num_locations;

	index = 0;
	num_locations = get_number_of_locations ();

	while (locations != 0 && index < num_locations)
	{
		if ((locations & 1) == 1)
		{
			ret |= drawer->matrix[index];
		}

		locations >>= 1;
		index++;
	}

	return ret;
}

static void
gtk_source_space_drawer_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	GtkSourceSpaceDrawer *drawer = GTK_SOURCE_SPACE_DRAWER (object);

	switch (prop_id)
	{
		case PROP_ENABLE_MATRIX:
			g_value_set_boolean (value, gtk_source_space_drawer_get_enable_matrix (drawer));
			break;

		case PROP_MATRIX:
			g_value_set_variant (value, gtk_source_space_drawer_get_matrix (drawer));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_space_drawer_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	GtkSourceSpaceDrawer *drawer = GTK_SOURCE_SPACE_DRAWER (object);

	switch (prop_id)
	{
		case PROP_ENABLE_MATRIX:
			gtk_source_space_drawer_set_enable_matrix (drawer, g_value_get_boolean (value));
			break;

		case PROP_MATRIX:
			gtk_source_space_drawer_set_matrix (drawer, g_value_get_variant (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_space_drawer_finalize (GObject *object)
{
	GtkSourceSpaceDrawer *drawer = GTK_SOURCE_SPACE_DRAWER (object);

	gtk_source_space_drawer_purge_cache (drawer);
	g_free (drawer->matrix);

	G_OBJECT_CLASS (gtk_source_space_drawer_parent_class)->finalize (object);
}

static void
gtk_source_space_drawer_class_init (GtkSourceSpaceDrawerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_space_drawer_get_property;
	object_class->set_property = gtk_source_space_drawer_set_property;
	object_class->finalize = gtk_source_space_drawer_finalize;

	/**
	 * GtkSourceSpaceDrawer:enable-matrix:
	 *
	 * Whether the [property@SpaceDrawer:matrix] property is enabled.
	 */
	properties[PROP_ENABLE_MATRIX] =
		g_param_spec_boolean ("enable-matrix",
				      "Enable Matrix",
				      "",
				      FALSE,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT |
				      G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceSpaceDrawer:matrix:
	 *
	 * The property is a [struct@GLib.Variant] property to specify where and
	 * what kind of white spaces to draw.
	 *
	 * The [struct@GLib.Variant] is of type `"au"`, an array of unsigned integers. Each
	 * integer is a combination of [flags@SpaceTypeFlags]. There is one
	 * integer for each [flags@SpaceLocationFlags], in the same order as
	 * they are defined in the enum (%GTK_SOURCE_SPACE_LOCATION_NONE and
	 * %GTK_SOURCE_SPACE_LOCATION_ALL are not taken into account).
	 *
	 * If the array is shorter than the number of locations, then the value
	 * for the missing locations will be %GTK_SOURCE_SPACE_TYPE_NONE.
	 *
	 * By default, %GTK_SOURCE_SPACE_TYPE_ALL is set for all locations.4
	 */
	properties[PROP_MATRIX] =
		g_param_spec_variant ("matrix",
				      "Matrix",
				      "",
				      G_VARIANT_TYPE ("au"),
				      get_default_matrix (),
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT |
				      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
gtk_source_space_drawer_init (GtkSourceSpaceDrawer *drawer)
{
	drawer->matrix = g_new0 (GtkSourceSpaceTypeFlags, get_number_of_locations ());
}

/**
 * gtk_source_space_drawer_new:
 *
 * Creates a new #GtkSourceSpaceDrawer object.
 *
 * Useful for storing space drawing settings independently of a [class@View].
 *
 * Returns: a new #GtkSourceSpaceDrawer.
 */
GtkSourceSpaceDrawer *
gtk_source_space_drawer_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SPACE_DRAWER, NULL);
}

/**
 * gtk_source_space_drawer_get_types_for_locations:
 * @drawer: a #GtkSourceSpaceDrawer.
 * @locations: one or several #GtkSourceSpaceLocationFlags.
 *
 * If only one location is specified, this function returns what kind of
 * white spaces are drawn at that location.
 *
 * The value is retrieved from the [property@SpaceDrawer:matrix] property.
 *
 * If several locations are specified, this function returns the logical AND for
 * those locations. Which means that if a certain kind of white space is present
 * in the return value, then that kind of white space is drawn at all the
 * specified @locations.
 *
 * Returns: a combination of #GtkSourceSpaceTypeFlags.
 */
GtkSourceSpaceTypeFlags
gtk_source_space_drawer_get_types_for_locations (GtkSourceSpaceDrawer        *drawer,
                                                 GtkSourceSpaceLocationFlags  locations)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer), GTK_SOURCE_SPACE_TYPE_NONE);

	return get_types_at_all_locations (drawer, locations);
}

/**
 * gtk_source_space_drawer_set_types_for_locations:
 * @drawer: a #GtkSourceSpaceDrawer.
 * @locations: one or several #GtkSourceSpaceLocationFlags.
 * @types: a combination of #GtkSourceSpaceTypeFlags.
 *
 * Modifies the [property@SpaceDrawer:matrix] property at the specified
 * @locations.
 */
void
gtk_source_space_drawer_set_types_for_locations (GtkSourceSpaceDrawer        *drawer,
                                                 GtkSourceSpaceLocationFlags  locations,
                                                 GtkSourceSpaceTypeFlags      types)
{
	gint index;
	gint num_locations;
	gboolean changed = FALSE;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));

	index = 0;
	num_locations = get_number_of_locations ();

	while (locations != 0 && index < num_locations)
	{
		if ((locations & 1) == 1 &&
		    drawer->matrix[index] != types)
		{
			drawer->matrix[index] = types;
			changed = TRUE;
		}

		locations >>= 1;
		index++;
	}

	if (changed)
	{
		g_object_notify_by_pspec (G_OBJECT (drawer), properties[PROP_MATRIX]);
	}
}

/**
 * gtk_source_space_drawer_get_matrix:
 * @drawer: a #GtkSourceSpaceDrawer.
 *
 * Gets the value of the [property@SpaceDrawer:matrix] property, as a [struct@GLib.Variant].
 *
 * An empty array can be returned in case the matrix is a zero matrix.
 *
 * The [method@SpaceDrawer.get_types_for_locations] function may be more
 * convenient to use.
 *
 * Returns: the #GtkSourceSpaceDrawer:matrix value as a new floating #GVariant
 *   instance.
 */
GVariant *
gtk_source_space_drawer_get_matrix (GtkSourceSpaceDrawer *drawer)
{
	GVariantBuilder builder;
	gint num_locations;
	gint i;

	g_return_val_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer), NULL);

	if (is_zero_matrix (drawer))
	{
		return g_variant_new ("au", NULL);
	}

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("au"));

	num_locations = get_number_of_locations ();

	for (i = 0; i < num_locations; i++)
	{
		GVariant *space_types;

		space_types = g_variant_new_uint32 (drawer->matrix[i]);

		g_variant_builder_add_value (&builder, space_types);
	}

	return g_variant_builder_end (&builder);
}

/**
 * gtk_source_space_drawer_set_matrix:
 * @drawer: a #GtkSourceSpaceDrawer.
 * @matrix: (transfer floating) (nullable): the new matrix value, or %NULL.
 *
 * Sets a new value to the [property@SpaceDrawer:matrix] property, as a [struct@GLib.Variant].
 *
 * If @matrix is %NULL, then an empty array is set.
 *
 * If @matrix is floating, it is consumed.
 *
 * The [method@SpaceDrawer.set_types_for_locations] function may be more
 * convenient to use.
 */
void
gtk_source_space_drawer_set_matrix (GtkSourceSpaceDrawer *drawer,
                                    GVariant             *matrix)
{
	gint num_locations;
	gint index;
	GVariantIter iter;
	gboolean changed = FALSE;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));

	if (matrix == NULL)
	{
		set_zero_matrix (drawer);
		return;
	}

	g_return_if_fail (g_variant_is_of_type (matrix, G_VARIANT_TYPE ("au")));

	g_variant_iter_init (&iter, matrix);

	num_locations = get_number_of_locations ();
	index = 0;
	while (index < num_locations)
	{
		GVariant *child;
		guint32 space_types;

		child = g_variant_iter_next_value (&iter);
		if (child == NULL)
		{
			break;
		}

		space_types = g_variant_get_uint32 (child);

		if (drawer->matrix[index] != space_types)
		{
			drawer->matrix[index] = space_types;
			changed = TRUE;
		}

		g_variant_unref (child);
		index++;
	}

	while (index < num_locations)
	{
		if (drawer->matrix[index] != 0)
		{
			drawer->matrix[index] = 0;
			changed = TRUE;
		}

		index++;
	}

	if (changed)
	{
		g_object_notify_by_pspec (G_OBJECT (drawer), properties[PROP_MATRIX]);
	}

	if (g_variant_is_floating (matrix))
	{
		g_variant_ref_sink (matrix);
		g_variant_unref (matrix);
	}
}

/**
 * gtk_source_space_drawer_get_enable_matrix:
 * @drawer: a #GtkSourceSpaceDrawer.
 *
 * Returns: whether the #GtkSourceSpaceDrawer:matrix property is enabled.
 */
gboolean
gtk_source_space_drawer_get_enable_matrix (GtkSourceSpaceDrawer *drawer)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer), FALSE);

	return drawer->enable_matrix;
}

/**
 * gtk_source_space_drawer_set_enable_matrix:
 * @drawer: a #GtkSourceSpaceDrawer.
 * @enable_matrix: the new value.
 *
 * Sets whether the [property@SpaceDrawer:matrix] property is enabled.
 */
void
gtk_source_space_drawer_set_enable_matrix (GtkSourceSpaceDrawer *drawer,
                                           gboolean              enable_matrix)
{
	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));

	enable_matrix = enable_matrix != FALSE;

	if (drawer->enable_matrix != enable_matrix)
	{
		drawer->enable_matrix = enable_matrix;
		g_object_notify_by_pspec (G_OBJECT (drawer), properties[PROP_ENABLE_MATRIX]);
	}
}

static gboolean
matrix_get_mapping (GValue   *value,
                    GVariant *variant,
                    gpointer  user_data)
{
	g_value_set_variant (value, variant);
	return TRUE;
}

static GVariant *
matrix_set_mapping (const GValue       *value,
                    const GVariantType *expected_type,
                    gpointer            user_data)
{
	return g_value_dup_variant (value);
}

/**
 * gtk_source_space_drawer_bind_matrix_setting:
 * @drawer: a #GtkSourceSpaceDrawer object.
 * @settings: a #GSettings object.
 * @key: the @settings key to bind.
 * @flags: flags for the binding.
 *
 * Binds the [property@SpaceDrawer:matrix] property to a [class@Gio.Settings] key.
 *
 * The [class@Gio.Settings] key must be of the same type as the
 * [property@SpaceDrawer:matrix] property, that is, `"au"`.
 *
 * The [method@Gio.Settings.bind] function cannot be used, because the default GIO
 * mapping functions don't support [struct@GLib.Variant] properties (maybe it will be
 * supported by a future GIO version, in which case this function can be
 * deprecated).
 */
void
gtk_source_space_drawer_bind_matrix_setting (GtkSourceSpaceDrawer *drawer,
                                             GSettings            *settings,
                                             const gchar          *key,
                                             GSettingsBindFlags    flags)
{
	GVariant *value;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));
	g_return_if_fail (G_IS_SETTINGS (settings));
	g_return_if_fail (key != NULL);
	g_return_if_fail ((flags & G_SETTINGS_BIND_INVERT_BOOLEAN) == 0);

	value = g_settings_get_value (settings, key);
	if (!g_variant_is_of_type (value, G_VARIANT_TYPE ("au")))
	{
		g_warning ("%s(): the GSettings key must be of type \"au\".", G_STRFUNC);
		g_variant_unref (value);
		return;
	}
	g_variant_unref (value);

	g_settings_bind_with_mapping (settings, key,
				      drawer, "matrix",
				      flags,
				      matrix_get_mapping,
				      matrix_set_mapping,
				      NULL, NULL);
}

void
_gtk_source_space_drawer_update_color (GtkSourceSpaceDrawer *drawer,
                                       GtkSourceView        *view)
{
	GtkSourceBuffer *buffer;
	GtkSourceStyleScheme *style_scheme;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	gtk_source_space_drawer_purge_cache (drawer);

	drawer->color_set = FALSE;

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	style_scheme = gtk_source_buffer_get_style_scheme (buffer);

	if (style_scheme != NULL)
	{
		GtkSourceStyle *style;

		style = _gtk_source_style_scheme_get_draw_spaces_style (style_scheme);

		if (style != NULL)
		{
			gchar *color_str = NULL;
			gboolean color_set;
			GdkRGBA color;

			g_object_get (style,
				      "foreground", &color_str,
				      "foreground-set", &color_set,
				      NULL);

			if (color_set &&
			    color_str != NULL &&
			    gdk_rgba_parse (&color, color_str))
			{
				drawer->color = color;
				drawer->color_set = TRUE;
			}

			g_free (color_str);
		}
	}

	if (!drawer->color_set)
	{
		gtk_widget_get_color (GTK_WIDGET (view), &drawer->color);
		drawer->color.alpha *= .5;
		drawer->color_set = TRUE;
	}
}

static inline gboolean
is_tab (gunichar ch)
{
	return ch == '\t';
}

static inline gboolean
is_nbsp (gunichar ch)
{
	return g_unichar_break_type (ch) == G_UNICODE_BREAK_NON_BREAKING_GLUE;
}

static inline gboolean
is_narrowed_nbsp (gunichar ch)
{
	return ch == 0x202F;
}

static inline gboolean
is_space (gunichar ch)
{
	return g_unichar_type (ch) == G_UNICODE_SPACE_SEPARATOR;
}

static gboolean
is_newline (const GtkTextIter *iter)
{
	if (gtk_text_iter_is_end (iter))
	{
		GtkSourceBuffer *buffer;

		buffer = GTK_SOURCE_BUFFER (gtk_text_iter_get_buffer (iter));

		return gtk_source_buffer_get_implicit_trailing_newline (buffer);
	}

	return gtk_text_iter_ends_line (iter);
}

static inline gboolean
is_whitespace (gunichar ch)
{
	return (g_unichar_isspace (ch) || is_nbsp (ch) || is_space (ch));
}

static void
path_builder_stroke_and_free (GtkSnapshot    *snapshot,
                              GskPathBuilder *builder)
{
	GskPath *path;
	GskStroke *stroke;

	path = gsk_path_builder_free_to_path (builder);
	stroke = gsk_stroke_new (0.8);
	gtk_snapshot_push_stroke (snapshot, path, stroke);

	gsk_stroke_free (stroke);
	gsk_path_unref (path);
}

static void
path_builder_fill_and_free (GtkSnapshot    *snapshot,
                            GskPathBuilder *builder)
{
	GskPath *path;

	path = gsk_path_builder_free_to_path (builder);
	gtk_snapshot_push_fill (snapshot, path, GSK_FILL_RULE_WINDING);

	gsk_path_unref (path);
}

static void
draw_space_at_pos (GtkSnapshot *snapshot,
                   float        w,
                   float        h)
{
	GskPathBuilder *builder;
	const gint x = 0;
	const gint y = h * 2 / 3;

	builder = gsk_path_builder_new ();

	gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (x + w / 2, y), 0.8);

	path_builder_stroke_and_free (snapshot, builder);
}

static void
draw_tab_at_pos (GtkSnapshot *snapshot,
                 float        w,
                 float        h)
{
	GskPathBuilder *builder;
	const gint x = 0;
	const gint y = h * 2 / 3;

	builder = gsk_path_builder_new ();

	gsk_path_builder_move_to (builder, x + h * 1 / 6, y);
	gsk_path_builder_rel_line_to (builder, w - h * 2 / 6, 0);
	gsk_path_builder_rel_line_to (builder, -h * 1 / 4, -h * 1 / 4);
	gsk_path_builder_rel_move_to (builder, +h * 1 / 4, +h * 1 / 4);
	gsk_path_builder_rel_line_to (builder, -h * 1 / 4, +h * 1 / 4);

	path_builder_stroke_and_free (snapshot, builder);
}

static void
draw_newline_at_pos (GtkSnapshot *snapshot,
                     float        w,
                     float        h)
{
	GskPathBuilder *builder;
	const gint x = 0;
	const gint y = h / 3;

	w = w * 2;

	builder = gsk_path_builder_new ();

	if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_LTR)
	{
		gsk_path_builder_move_to (builder, x + w * 7 / 8, y);
		gsk_path_builder_rel_line_to (builder, 0, h * 1 / 3);
		gsk_path_builder_rel_line_to (builder, -w * 6 / 8, 0);
		gsk_path_builder_rel_line_to (builder, +h * 1 / 4, -h * 1 / 4);
		gsk_path_builder_rel_move_to (builder, -h * 1 / 4, +h * 1 / 4);
		gsk_path_builder_rel_line_to (builder, +h * 1 / 4, +h * 1 / 4);
	}
	else
	{
		gsk_path_builder_move_to (builder, x + w * 1 / 8, y);
		gsk_path_builder_rel_line_to (builder, 0, h * 1 / 3);
		gsk_path_builder_rel_line_to (builder, w * 6 / 8, 0);
		gsk_path_builder_rel_line_to (builder, -h * 1 / 4, -h * 1 / 4);
		gsk_path_builder_rel_move_to (builder, +h * 1 / 4, +h * 1 / 4);
		gsk_path_builder_rel_line_to (builder, -h * 1 / 4, -h * 1 / 4);
	}

	path_builder_stroke_and_free (snapshot, builder);
}

static void
draw_narrow_nbsp_at_pos (GtkSnapshot *snapshot,
                         float        w,
                         float        h)
{
	GskPathBuilder *builder;
	const gint x = 0;
	const gint y = h / 2;

	builder = gsk_path_builder_new ();

	gsk_path_builder_move_to (builder, x + w * 1 / 6, y);
	gsk_path_builder_rel_line_to (builder, w * 4 / 6, 0);
	gsk_path_builder_rel_line_to (builder, -w * 2 / 6, +h * 1 / 4);
	gsk_path_builder_rel_line_to (builder, -w * 2 / 6, -h * 1 / 4);

	path_builder_fill_and_free (snapshot, builder);
}

static void
draw_nbsp_at_pos (GtkSnapshot *snapshot,
                  float        w,
                  float        h)
{
	GskPathBuilder *builder;
	const gint x = 0;
	const gint y = h / 2;

	builder = gsk_path_builder_new ();

	gsk_path_builder_move_to (builder, x + w * 1 / 6, y);
	gsk_path_builder_rel_line_to (builder, w * 4 / 6, 0);
	gsk_path_builder_rel_line_to (builder, -w * 2 / 6, +h * 1 / 4);
	gsk_path_builder_rel_line_to (builder, -w * 2 / 6, -h * 1 / 4);

	path_builder_stroke_and_free (snapshot, builder);
}

static void
draw_whitespace_at_iter (GtkSourceSpaceDrawer *drawer,
                         GtkTextView          *text_view,
                         const GtkTextIter    *iter,
                         const GdkRGBA        *color,
                         GtkSnapshot          *snapshot)
{
	void (*draw) (GtkSnapshot *snapshot, float w, float h) = NULL;
	CachedNode *cache = NULL;
	GdkRectangle rect;
	gunichar ch;
	gint ratio = 1;

	gtk_text_view_get_iter_location (text_view, iter, &rect);

	/* If the space is at a line-wrap position, or if the character is a
	 * newline, we get 0 width so we fallback to the height.
	 */
	if (rect.width == 0)
	{
		rect.width = rect.height;
	}

	ch = gtk_text_iter_get_char (iter);

	if (is_tab (ch))
	{
		draw = draw_tab_at_pos;
		cache = &drawer->cached[DRAW_TAB];
	}
	else if (is_nbsp (ch))
	{
		if (is_narrowed_nbsp (ch))
		{
			draw = draw_narrow_nbsp_at_pos;
			cache = &drawer->cached[DRAW_NARROW_NBSP];
		}
		else
		{
			draw = draw_nbsp_at_pos;
			cache = &drawer->cached[DRAW_NBSP];
		}
	}
	else if (is_space (ch))
	{
		draw = draw_space_at_pos;
		cache = &drawer->cached[DRAW_SPACE];
	}
	else if (is_newline (iter))
	{
		draw = draw_newline_at_pos;
		cache = &drawer->cached[DRAW_NEWLINE];
		ratio = 2;
	}

	g_assert (draw == NULL || cache != NULL);

	if (draw != NULL)
	{
		if (cache->width != rect.width || cache->height != rect.height)
		{
			g_clear_pointer (&cache->node, gsk_render_node_unref);
		}

		if G_UNLIKELY (cache->node == NULL)
		{
			GtkSnapshot *to_cache;

			to_cache = gtk_snapshot_new ();
			gtk_snapshot_translate (to_cache, &GRAPHENE_POINT_INIT (-0.5, -0.5));
			draw (to_cache, rect.width, rect.height);
			gtk_snapshot_append_color (to_cache,
			                           color,
			                           &GRAPHENE_RECT_INIT (0, 0, rect.width * ratio, rect.height));
			gtk_snapshot_pop (to_cache);

			cache->node = gtk_snapshot_free_to_node (g_steal_pointer (&to_cache));
			cache->width = rect.width;
			cache->height = rect.height;
		}

		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (rect.x, rect.y));
		gtk_snapshot_append_node (snapshot, cache->node);
		gtk_snapshot_restore (snapshot);
	}
}

static void
space_needs_drawing_according_to_tag (const GtkTextIter *iter,
                                      gboolean          *has_tag,
                                      gboolean          *needs_drawing)
{
	GSList *tags;
	GSList *l;

	*has_tag = FALSE;
	*needs_drawing = FALSE;

	tags = gtk_text_iter_get_tags (iter);
	tags = g_slist_reverse (tags);

	for (l = tags; l != NULL; l = l->next)
	{
		GtkTextTag *tag = l->data;

		if (GTK_SOURCE_IS_TAG (tag))
		{
			gboolean draw_spaces_set;
			gboolean draw_spaces;

			g_object_get (tag,
				      "draw-spaces-set", &draw_spaces_set,
				      "draw-spaces", &draw_spaces,
				      NULL);

			if (draw_spaces_set)
			{
				*has_tag = TRUE;
				*needs_drawing = draw_spaces;
				break;
			}
		}
	}

	g_slist_free (tags);
}

static GtkSourceSpaceLocationFlags
get_iter_locations (const GtkTextIter *iter,
                    const GtkTextIter *leading_end,
                    const GtkTextIter *trailing_start)
{
	GtkSourceSpaceLocationFlags iter_locations = GTK_SOURCE_SPACE_LOCATION_NONE;

	if (gtk_text_iter_compare (iter, leading_end) < 0)
	{
		iter_locations |= GTK_SOURCE_SPACE_LOCATION_LEADING;
	}

	if (gtk_text_iter_compare (trailing_start, iter) <= 0)
	{
		iter_locations |= GTK_SOURCE_SPACE_LOCATION_TRAILING;
	}

	/* Neither leading nor trailing, must be in text. */
	if (iter_locations == GTK_SOURCE_SPACE_LOCATION_NONE)
	{
		iter_locations = GTK_SOURCE_SPACE_LOCATION_INSIDE_TEXT;
	}

	return iter_locations;
}

static GtkSourceSpaceTypeFlags
get_iter_space_type (const GtkTextIter *iter)
{
	gunichar ch;

	ch = gtk_text_iter_get_char (iter);

	if (is_tab (ch))
	{
		return GTK_SOURCE_SPACE_TYPE_TAB;
	}
	else if (is_nbsp (ch))
	{
		return GTK_SOURCE_SPACE_TYPE_NBSP;
	}
	else if (is_space (ch))
	{
		return GTK_SOURCE_SPACE_TYPE_SPACE;
	}
	else if (is_newline (iter))
	{
		return GTK_SOURCE_SPACE_TYPE_NEWLINE;
	}

	return GTK_SOURCE_SPACE_TYPE_NONE;
}

static gboolean
space_needs_drawing_according_to_matrix (GtkSourceSpaceDrawer *drawer,
                                         const GtkTextIter    *iter,
                                         const GtkTextIter    *leading_end,
                                         const GtkTextIter    *trailing_start)
{
	GtkSourceSpaceLocationFlags iter_locations;
	GtkSourceSpaceTypeFlags iter_space_type;
	GtkSourceSpaceTypeFlags allowed_space_types;

	iter_locations = get_iter_locations (iter, leading_end, trailing_start);
	iter_space_type = get_iter_space_type (iter);
	allowed_space_types = get_types_at_any_locations (drawer, iter_locations);

	return (iter_space_type & allowed_space_types) != 0;
}

static gboolean
space_needs_drawing (GtkSourceSpaceDrawer *drawer,
                     const GtkTextIter    *iter,
                     const GtkTextIter    *leading_end,
                     const GtkTextIter    *trailing_start)
{
	gboolean has_tag;
	gboolean needs_drawing;

	/* Check the GtkSourceTag:draw-spaces property (higher priority) */
	space_needs_drawing_according_to_tag (iter, &has_tag, &needs_drawing);
	if (has_tag)
	{
		return needs_drawing;
	}

	/* Check the matrix */
	return (drawer->enable_matrix &&
		space_needs_drawing_according_to_matrix (drawer, iter, leading_end, trailing_start));
}

static void
get_line_end (GtkTextView       *text_view,
              const GtkTextIter *start_iter,
              GtkTextIter       *line_end,
              gint               max_x,
              gint               max_y,
              gboolean           is_wrapping)
{
	gint min;
	gint max;
	GdkRectangle rect;

	*line_end = *start_iter;
	if (!gtk_text_iter_ends_line (line_end))
	{
		gtk_text_iter_forward_to_line_end (line_end);
	}

	/* Check if line_end is inside the bounding box anyway. */
	gtk_text_view_get_iter_location (text_view, line_end, &rect);
	if (( is_wrapping && rect.y < max_y) ||
	    (!is_wrapping && rect.x < max_x))
	{
		return;
	}

	min = gtk_text_iter_get_line_offset (start_iter);
	max = gtk_text_iter_get_line_offset (line_end);

	while (max >= min)
	{
		gint i;

		i = (min + max) >> 1;
		gtk_text_iter_set_line_offset (line_end, i);
		gtk_text_view_get_iter_location (text_view, line_end, &rect);

		if (( is_wrapping && rect.y < max_y) ||
		    (!is_wrapping && rect.x < max_x))
		{
			min = i + 1;
		}
		else if (( is_wrapping && rect.y > max_y) ||
			 (!is_wrapping && rect.x > max_x))
		{
			max = i - 1;
		}
		else
		{
			break;
		}
	}
}

void
_gtk_source_space_drawer_draw (GtkSourceSpaceDrawer *drawer,
                               GtkSourceView        *view,
                               GtkSnapshot          *snapshot)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	GdkRectangle visible;
	gint min_x;
	gint min_y;
	gint max_x;
	gint max_y;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextIter iter;
	GtkTextIter leading_end;
	GtkTextIter trailing_start;
	GtkTextIter line_end;
	gboolean is_wrapping;

	g_return_if_fail (GTK_SOURCE_IS_SPACE_DRAWER (drawer));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	if (!drawer->color_set)
	{
		g_warning ("GtkSourceSpaceDrawer: color not set.");
		return;
	}

	text_view = GTK_TEXT_VIEW (view);
	buffer = gtk_text_view_get_buffer (text_view);

	if ((!drawer->enable_matrix || is_zero_matrix (drawer)) &&
	    !_gtk_source_buffer_has_spaces_tag (GTK_SOURCE_BUFFER (buffer)))
	{
		return;
	}

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible);

	is_wrapping = gtk_text_view_get_wrap_mode (text_view) != GTK_WRAP_NONE;

	min_x = visible.x;
	min_y = visible.y;
	max_x = min_x + visible.width;
	max_y = min_y + visible.height;

	gtk_text_view_get_iter_at_location (text_view, &start, min_x, min_y);
	gtk_text_view_get_iter_at_location (text_view, &end, max_x, max_y);

	iter = start;
	_gtk_source_iter_get_leading_spaces_end_boundary (&iter, &leading_end);
	_gtk_source_iter_get_trailing_spaces_start_boundary (&iter, &trailing_start);
	get_line_end (text_view, &iter, &line_end, max_x, max_y, is_wrapping);

	while (TRUE)
	{
		gunichar ch = gtk_text_iter_get_char (&iter);
		gint ly;

		/* Allow end iter, to draw implicit trailing newline. */
		if ((is_whitespace (ch) || gtk_text_iter_is_end (&iter)) &&
		    space_needs_drawing (drawer, &iter, &leading_end, &trailing_start))
		{
			draw_whitespace_at_iter (drawer,
			                         text_view,
			                         &iter,
			                         &drawer->color,
			                         snapshot);
		}

		if (gtk_text_iter_is_end (&iter) ||
		    gtk_text_iter_compare (&iter, &end) >= 0)
		{
			break;
		}

		gtk_text_iter_forward_char (&iter);

		if (gtk_text_iter_compare (&iter, &line_end) > 0)
		{
			GtkTextIter next_iter = iter;

			/* Move to the first iter in the exposed area of the
			 * next line.
			 */
			if (!gtk_text_iter_starts_line (&next_iter))
			{
				/* We're trying to move forward on the last
				 * line of the buffer, so we can stop now.
				 */
				if (!gtk_text_iter_forward_line (&next_iter))
				{
					break;
				}
			}

			gtk_text_view_get_line_yrange (text_view, &next_iter, &ly, NULL);
			gtk_text_view_get_iter_at_location (text_view, &next_iter, min_x, ly);

			/* Move back one char otherwise tabs may not be redrawn. */
			if (!gtk_text_iter_starts_line (&next_iter))
			{
				gtk_text_iter_backward_char (&next_iter);
			}

			/* Ensure that we have actually advanced, since the
			 * above backward_char() is dangerous and can lead to
			 * infinite loops.
			 */
			if (gtk_text_iter_compare (&next_iter, &iter) > 0)
			{
				iter = next_iter;
			}

			_gtk_source_iter_get_leading_spaces_end_boundary (&iter, &leading_end);
			_gtk_source_iter_get_trailing_spaces_start_boundary (&iter, &trailing_start);
			get_line_end (text_view, &iter, &line_end, max_x, max_y, is_wrapping);
		}
	};

	GTK_SOURCE_PROFILER_END_MARK ("GtkSourceSpaceDrawer::draw", NULL);
}

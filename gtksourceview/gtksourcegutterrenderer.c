/*
 * This file is part of GtkSourceView
 *
 * Copyright 2010 - Jesse van den Kieboom
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

#include "gtksourcebuffer.h"
#include "gtksourcegutter.h"
#include "gtksourcegutter-private.h"
#include "gtksourcegutterrenderer.h"
#include "gtksourcegutterrenderer-private.h"
#include "gtksourcegutterlines.h"
#include "gtksourcestylescheme.h"
#include "gtksourceview.h"
#include "gtksource-enumtypes.h"
#include "gtksource-marshal.h"

/**
 * GtkSourceGutterRenderer:
 *
 * Gutter cell renderer.
 *
 * A `GtkSourceGutterRenderer` represents a column in a [class@Gutter]. The
 * column contains one cell for each visible line of the [class@Gtk.TextBuffer]. Due to
 * text wrapping, a cell can thus span multiple lines of the [class@Gtk.TextView]. In
 * this case, [enum@GutterRendererAlignmentMode] controls the alignment of
 * the cell.
 *
 * The gutter renderer is a [class@Gtk.Widget] and is measured using the normal widget
 * measurement facilities. The width of the gutter will be determined by the
 * measurements of the gutter renderers.
 *
 * The width of a gutter renderer generally takes into account the entire text
 * buffer. For instance, to display the line numbers, if the buffer contains 100
 * lines, the gutter renderer will always set its width such as three digits can
 * be printed, even if only the first 20 lines are shown. Another strategy is to
 * take into account only the visible lines.  In this case, only two digits are
 * necessary to display the line numbers of the first 20 lines. To take another
 * example, the gutter renderer for [class@Mark]s doesn't need to take
 * into account the text buffer to announce its width. It only depends on the
 * icons size displayed in the gutter column.
 *
 * When the available size to render a cell is greater than the required size to
 * render the cell contents, the cell contents can be aligned horizontally and
 * vertically with [method@GutterRenderer.set_alignment_mode].
 *
 * The cells rendering occurs using [vfunc@Gtk.Widget.snapshot]. Implementations
 * should use `gtk_source_gutter_renderer_get_lines()` to retrieve information
 * about the lines to be rendered. To help with aligning content which takes
 * into account the padding and alignment of a cell, implementations may call
 * [method@GutterRenderer.align_cell] for a given line number with the
 * width and height measurement of the content they width to render.
 */

typedef struct
{
	GtkSourceGutter *gutter;
	GtkSourceView *view;
	GtkSourceBuffer *buffer;
	GtkSourceGutterLines *lines;

	gfloat xalign;
	gfloat yalign;

	gint xpad;
	gint ypad;

	GtkSourceGutterRendererAlignmentMode alignment_mode;

	guint visible : 1;
} GtkSourceGutterRendererPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkSourceGutterRenderer, gtk_source_gutter_renderer, GTK_TYPE_WIDGET)

enum
{
	PROP_0,
	PROP_ALIGNMENT_MODE,
	PROP_LINES,
	PROP_VIEW,
	PROP_XALIGN,
	PROP_XPAD,
	PROP_YALIGN,
	PROP_YPAD,
	N_PROPS
};

enum
{
	ACTIVATE,
	QUERY_ACTIVATABLE,
	QUERY_DATA,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint       signals[N_SIGNALS];

static void
emit_buffer_changed (GtkSourceView           *view,
                     GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	GtkSourceBuffer *buffer;
	GtkSourceBuffer *old_buffer;

	old_buffer = priv->buffer;
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	if (buffer == old_buffer)
	{
		return;
	}

	g_set_weak_pointer (&priv->buffer, buffer);
	GTK_SOURCE_GUTTER_RENDERER_GET_CLASS (renderer)->change_buffer (renderer, old_buffer);
}

static void
on_buffer_changed (GtkSourceView           *view,
                   GParamSpec              *spec,
                   GtkSourceGutterRenderer *renderer)
{
	emit_buffer_changed (view, renderer);
}

static void
gtk_source_gutter_renderer_change_buffer (GtkSourceGutterRenderer *renderer,
                                          GtkSourceBuffer         *buffer)
{
}

static void
gtk_source_gutter_renderer_change_view (GtkSourceGutterRenderer *renderer,
                                        GtkSourceView           *old_view)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	if (old_view != NULL)
	{
		g_signal_handlers_disconnect_by_func (old_view,
		                                      G_CALLBACK (on_buffer_changed),
		                                      renderer);
	}

	if (priv->view != NULL)
	{
		emit_buffer_changed (priv->view, renderer);

		g_signal_connect (priv->view,
		                  "notify::buffer",
		                  G_CALLBACK (on_buffer_changed),
		                  renderer);
	}
}

static void
gtk_source_gutter_renderer_snapshot (GtkWidget   *widget,
                                     GtkSnapshot *snapshot)
{
	GtkSourceGutterRenderer *renderer = GTK_SOURCE_GUTTER_RENDERER (widget);
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	GtkSourceGutterRendererClass *klass = GTK_SOURCE_GUTTER_RENDERER_GET_CLASS (widget);
	GtkSourceGutterRendererAlignmentMode mode = priv->alignment_mode;
	GtkSourceGutterLines *lines = priv->lines;
	guint first;
	guint last;
	guint line;
	double y;
	double h;

	if (lines == NULL || klass->snapshot_line == NULL)
	{
		return;
	}

	first = gtk_source_gutter_lines_get_first (lines);
	last = gtk_source_gutter_lines_get_last (lines);

	if (klass->query_data)
	{
		for (line = first; line <= last; line++)
		{
			gtk_source_gutter_lines_get_line_extent (lines, line, mode, &y, &h);
			klass->query_data (renderer, lines, line);
			klass->snapshot_line (renderer, snapshot, lines, line);
		}
	}
	else
	{
		for (line = first; line <= last; line++)
		{
			gtk_source_gutter_lines_get_line_extent (lines, line, mode, &y, &h);
			klass->snapshot_line (renderer, snapshot, lines, line);
		}
	}
}

static void
gtk_source_gutter_renderer_dispose (GObject *object)
{
	GtkSourceGutterRenderer *renderer = GTK_SOURCE_GUTTER_RENDERER (object);
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_clear_weak_pointer (&priv->buffer);

	if (priv->view != NULL)
	{
		_gtk_source_gutter_renderer_set_view (renderer, NULL);
	}

	G_OBJECT_CLASS (gtk_source_gutter_renderer_parent_class)->dispose (object);
}

static void
gtk_source_gutter_renderer_root (GtkWidget *widget)
{
	GtkSourceGutterRenderer *renderer = GTK_SOURCE_GUTTER_RENDERER (widget);
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	GtkWidget *gutter;

	GTK_WIDGET_CLASS (gtk_source_gutter_renderer_parent_class)->root (widget);

	gutter = gtk_widget_get_ancestor (widget, GTK_SOURCE_TYPE_GUTTER);

	if (GTK_SOURCE_IS_GUTTER (gutter))
	{
		priv->gutter = GTK_SOURCE_GUTTER (gutter);
	}
}

static void
gtk_source_gutter_renderer_unroot (GtkWidget *widget)
{
	GtkSourceGutterRenderer *renderer = GTK_SOURCE_GUTTER_RENDERER (widget);
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	priv->gutter = NULL;

	GTK_WIDGET_CLASS (gtk_source_gutter_renderer_parent_class)->unroot (widget);
}

static void
gtk_source_gutter_renderer_real_begin (GtkSourceGutterRenderer *renderer,
                                       GtkSourceGutterLines    *lines)
{
}

static void
gtk_source_gutter_renderer_real_end (GtkSourceGutterRenderer *renderer)
{
}

static void
gtk_source_gutter_renderer_query_data (GtkSourceGutterRenderer *renderer,
                                       GtkSourceGutterLines    *lines,
                                       guint                    line)
{
	g_signal_emit (renderer, signals[QUERY_DATA], 0, lines, line);
}

static void
gtk_source_gutter_renderer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceGutterRenderer *self = GTK_SOURCE_GUTTER_RENDERER (object);

	switch (prop_id)
	{
		case PROP_XPAD:
			gtk_source_gutter_renderer_set_xpad (self, g_value_get_int (value));
			break;

		case PROP_YPAD:
			gtk_source_gutter_renderer_set_ypad (self, g_value_get_int (value));
			break;

		case PROP_XALIGN:
			gtk_source_gutter_renderer_set_xalign (self, g_value_get_float (value));
			break;

		case PROP_YALIGN:
			gtk_source_gutter_renderer_set_yalign (self, g_value_get_float (value));
			break;

		case PROP_ALIGNMENT_MODE:
			gtk_source_gutter_renderer_set_alignment_mode (self, g_value_get_enum (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceGutterRenderer *self = GTK_SOURCE_GUTTER_RENDERER (object);
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (self);

	switch (prop_id)
	{
		case PROP_LINES:
			g_value_set_object (value, priv->lines);
			break;

		case PROP_XALIGN:
			g_value_set_float (value, priv->xalign);
			break;

		case PROP_XPAD:
			g_value_set_int (value, priv->xpad);
			break;

		case PROP_YALIGN:
			g_value_set_float (value, priv->yalign);
			break;

		case PROP_YPAD:
			g_value_set_int (value, priv->ypad);
			break;

		case PROP_VIEW:
			g_value_set_object (value, priv->view);
			break;

		case PROP_ALIGNMENT_MODE:
			g_value_set_enum (value, priv->alignment_mode);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_class_init (GtkSourceGutterRendererClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gtk_source_gutter_renderer_dispose;
	object_class->get_property = gtk_source_gutter_renderer_get_property;
	object_class->set_property = gtk_source_gutter_renderer_set_property;

	widget_class->root = gtk_source_gutter_renderer_root;
	widget_class->unroot = gtk_source_gutter_renderer_unroot;
	widget_class->snapshot = gtk_source_gutter_renderer_snapshot;

	klass->begin = gtk_source_gutter_renderer_real_begin;
	klass->end = gtk_source_gutter_renderer_real_end;
	klass->change_buffer = gtk_source_gutter_renderer_change_buffer;
	klass->change_view = gtk_source_gutter_renderer_change_view;
	klass->query_data = gtk_source_gutter_renderer_query_data;

	/**
	 * GtkSourceGutterRenderer:lines:
	 *
	 * Contains information about the lines to be rendered.
	 *
	 * It should be used by #GtkSourceGutterRenderer implementations from [vfunc@Gtk.Widget.snapshot].
	 */
	properties[PROP_LINES] =
		g_param_spec_object ("lines",
		                     "Lines",
		                     "Information about the lines to render",
		                     GTK_SOURCE_TYPE_GUTTER_LINES,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceGutterRenderer:xpad:
	 *
	 * The left and right padding of the renderer.
	 */
	properties[PROP_XPAD] =
		g_param_spec_int ("xpad",
		                  "X Padding",
		                  "The x-padding",
		                  0,
		                  G_MAXINT,
		                  0,
		                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceGutterRenderer:ypad:
	 *
	 * The top and bottom padding of the renderer.
	 */
	properties[PROP_YPAD] =
		g_param_spec_int ("ypad",
		                  "Y Padding",
		                  "The y-padding",
		                  0,
		                  G_MAXINT,
		                  0,
		                  G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceGutterRenderer:xalign:
	 *
	 * The horizontal alignment of the renderer.
	 *
	 * Set to 0 for a left alignment. 1 for a right alignment. And 0.5 for centering the cells.
	 * A value lower than 0 doesn't modify the alignment.
	 */
	properties[PROP_XALIGN] =
		g_param_spec_float ("xalign",
		                    "X Alignment",
		                    "The x-alignment",
		                    0.0,
		                    1.0,
		                    0.0,
		                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceGutterRenderer:yalign:
	 *
	 * The vertical alignment of the renderer.
	 *
	 * Set to 0 for a top alignment. 1 for a bottom alignment. And 0.5 for centering the cells.
	 * A value lower than 0 doesn't modify the alignment.
	 */
	properties[PROP_YALIGN] =
		g_param_spec_float ("yalign",
		                    "Y Alignment",
		                    "The y-alignment",
		                    0.0,
		                    1.0,
		                    0.0,
		                    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);


	/**
	 * GtkSourceGutterRenderer:view:
	 *
	 * The view on which the renderer is placed.
	 **/
	properties[PROP_VIEW] =
		g_param_spec_object ("view",
		                     "The View",
		                     "The view",
		                     GTK_TYPE_TEXT_VIEW,
		                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceGutterRenderer:alignment-mode:
	 *
	 * The alignment mode of the renderer.
	 *
	 * This can be used to indicate that in the case a cell spans multiple lines (due to text wrapping)
	 * the alignment should work on either the full cell, the first line or the last line.
	 **/
	properties[PROP_ALIGNMENT_MODE] =
		g_param_spec_enum ("alignment-mode",
		                   "Alignment Mode",
		                   "The alignment mode",
		                   GTK_SOURCE_TYPE_GUTTER_RENDERER_ALIGNMENT_MODE,
		                   GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL,
		                   G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceGutterRenderer::activate:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @iter: a #GtkTextIter
	 * @area: a #GdkRectangle
	 * @button: the button that was pressed
	 * @state: a #GdkModifierType of state
	 * @n_presses: the number of button presses
	 *
	 * The signal is emitted when the renderer is activated.
	 */
	signals[ACTIVATE] =
		g_signal_new ("activate",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, activate),
		              NULL, NULL,
		              _gtk_source_marshal_VOID__BOXED_BOXED_UINT_FLAGS_INT,
		              G_TYPE_NONE,
		              5,
		              GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		              GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE,
		              G_TYPE_UINT,
		              GDK_TYPE_MODIFIER_TYPE,
		              G_TYPE_INT);
	g_signal_set_va_marshaller (signals[ACTIVATE],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__BOXED_BOXED_UINT_FLAGS_INTv);

	/**
	 * GtkSourceGutterRenderer::query-activatable:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @iter: a #GtkTextIter
	 * @area: a #GdkRectangle
	 * @event: the #GdkEvent that is causing the activatable query
	 *
	 * The signal is emitted when the renderer can possibly be activated.
	 */
	signals[QUERY_ACTIVATABLE] =
		g_signal_new ("query-activatable",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, query_activatable),
		              g_signal_accumulator_true_handled,
		              NULL,
		              _gtk_source_marshal_BOOLEAN__BOXED_BOXED,
		              G_TYPE_BOOLEAN,
		              2,
		              GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
		              GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);
	g_signal_set_va_marshaller (signals[QUERY_ACTIVATABLE],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_BOOLEAN__BOXED_BOXEDv);

	signals[QUERY_DATA] =
		g_signal_new ("query-data",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              _gtk_source_marshal_VOID__OBJECT_UINT,
		              G_TYPE_NONE,
		              2,
		              G_TYPE_OBJECT,
		              G_TYPE_UINT);
	g_signal_set_va_marshaller (signals[QUERY_DATA],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__OBJECT_UINTv);

  gtk_widget_class_set_css_name (widget_class, "gutterrenderer");
}

static void
gtk_source_gutter_renderer_init (GtkSourceGutterRenderer *self)
{
}

/**
 * gtk_source_gutter_renderer_query_activatable:
 * @renderer: a #GtkSourceGutterRenderer
 * @iter: a #GtkTextIter at the start of the line to be activated
 * @area: a #GdkRectangle of the cell area to be activated
 *
 * Get whether the renderer is activatable at the location provided. This is
 * called from [class@Gutter] to determine whether a renderer is activatable
 * using the mouse pointer.
 *
 * Returns: %TRUE if the renderer can be activated, %FALSE otherwise
 *
 **/
gboolean
gtk_source_gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                              const GtkTextIter       *iter,
                                              const GdkRectangle      *area)
{
	gboolean ret;

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (area != NULL, FALSE);

	ret = FALSE;

	g_signal_emit (renderer,
	               signals[QUERY_ACTIVATABLE],
	               0,
	               iter,
	               area,
	               &ret);

	return ret;
}

/**
 * gtk_source_gutter_renderer_activate:
 * @renderer: a #GtkSourceGutterRenderer
 * @iter: a #GtkTextIter at the start of the line where the renderer is activated
 * @area: a #GdkRectangle of the cell area where the renderer is activated
 * @button: the button that was pressed
 * @state: a #GdkModifierType
 * @n_presses: the number of button presses
 *
 * Emits the [signal@GutterRenderer::activate] signal of the renderer. This is
 * called from [class@Gutter] and should never have to be called manually.
 */
void
gtk_source_gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                                     const GtkTextIter       *iter,
                                     const GdkRectangle      *area,
                                     guint                    button,
                                     GdkModifierType          state,
                                     gint                     n_presses)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (area != NULL);

	g_signal_emit (renderer, signals[ACTIVATE], 0, iter, area, button, state, n_presses);
}

/**
 * gtk_source_gutter_renderer_set_alignment_mode:
 * @renderer: a #GtkSourceGutterRenderer
 * @mode: a #GtkSourceGutterRendererAlignmentMode
 *
 * Set the alignment mode. The alignment mode describes the manner in which the
 * renderer is aligned (see [property@GutterRenderer:xalign] and
 * [property@GutterRenderer:yalign]).
 **/
void
gtk_source_gutter_renderer_set_alignment_mode (GtkSourceGutterRenderer              *renderer,
                                               GtkSourceGutterRendererAlignmentMode  mode)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_GUTTER_RENDERER (renderer));
	g_return_if_fail (mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL ||
	                  mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST ||
	                  mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST);

	if (priv->alignment_mode != mode)
	{
		priv->alignment_mode = mode;
		g_object_notify_by_pspec (G_OBJECT (renderer),
		                          properties[PROP_ALIGNMENT_MODE]);
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

/**
 * gtk_source_gutter_renderer_get_alignment_mode:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get the alignment mode.
 *
 * The alignment mode describes the manner in which the
 * renderer is aligned (see [property@GutterRenderer:xalign] and
 * [property@GutterRenderer:yalign]).
 *
 * Returns: a #GtkSourceGutterRendererAlignmentMode
 **/
GtkSourceGutterRendererAlignmentMode
gtk_source_gutter_renderer_get_alignment_mode (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), 0);

	return priv->alignment_mode;
}

/**
 * gtk_source_gutter_renderer_get_view:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get the view associated to the gutter renderer
 *
 * Returns: (transfer none): a #GtkSourceView
 **/
GtkSourceView *
gtk_source_gutter_renderer_get_view (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), NULL);

	return GTK_SOURCE_VIEW (priv->view);
}

void
_gtk_source_gutter_renderer_set_view (GtkSourceGutterRenderer *renderer,
                                      GtkSourceView           *view)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	GtkSourceView *old_view;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (view == NULL || GTK_SOURCE_IS_VIEW (view));

	old_view = priv->view;

	if (g_set_weak_pointer (&priv->view, view))
	{
		GTK_SOURCE_GUTTER_RENDERER_GET_CLASS (renderer)->change_view (renderer, old_view);
	}

	g_object_notify_by_pspec (G_OBJECT (renderer), properties[PROP_VIEW]);
}

static void
get_line_rect (GtkSourceGutterRenderer *renderer,
               guint                    line,
               graphene_rect_t         *rect)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	GtkSourceGutterLines *lines = NULL;

	if (priv->gutter != NULL)
	{
		lines = _gtk_source_gutter_get_lines (priv->gutter);
	}

	if (lines != NULL)
	{
		double y;
		double height;

		gtk_source_gutter_lines_get_line_extent (lines,
		                                         line,
		                                         priv->alignment_mode,
		                                         &y,
		                                         &height);

		rect->origin.x = priv->xpad;
		rect->origin.y = y + priv->ypad;
		rect->size.width = gtk_widget_get_width (GTK_WIDGET (renderer));
		rect->size.height = height;

		rect->size.width -= 2 * priv->xpad;
		rect->size.height -= 2 * priv->ypad;
	}
	else
	{
		rect->origin.x = 0;
		rect->origin.y = 0;
		rect->size.width = 0;
		rect->size.height = 0;
	}
}

/**
 * gtk_source_gutter_renderer_align_cell:
 * @renderer: the #GtkSourceGutterRenderer
 * @line: the line number for content
 * @width: the width of the content to draw
 * @height: the height of the content to draw
 * @x: (out): the X position to render the content
 * @y: (out): the Y position to render the content
 *
 * Locates where to render content that is @width x @height based on
 * the renderers alignment and padding.
 *
 * The location will be placed into @x and @y and is relative to the
 * renderer's coordinates.
 *
 * It is encouraged that renderers use this function when snappshotting
 * to ensure consistent placement of their contents.
 */
void
gtk_source_gutter_renderer_align_cell (GtkSourceGutterRenderer *renderer,
                                       guint                    line,
                                       gfloat                   width,
                                       gfloat                   height,
                                       gfloat                  *x,
                                       gfloat                  *y)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);
	graphene_rect_t rect;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));

	get_line_rect (renderer, line, &rect);

	*x = rect.origin.x + MAX (0, (rect.size.width - width)) * priv->xalign;
	*y = rect.origin.y + MAX (0, (rect.size.height - height)) * priv->yalign;
}

/**
 * gtk_source_gutter_renderer_get_xpad:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Gets the `xpad` property.
 *
 * This may be used to adjust the cell rectangle that the renderer will use to draw.
 */
gint
gtk_source_gutter_renderer_get_xpad (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), 0);

	return priv->xpad;
}

/**
 * gtk_source_gutter_renderer_set_xpad:
 * @renderer: a #GtkSourceGutterRenderer
 * @xpad: the Y padding for the drawing cell
 *
 * Adjusts the `xpad` property.
 *
 * This may be used to adjust the cell rectangle that the renderer will use to draw.
 */
void
gtk_source_gutter_renderer_set_xpad (GtkSourceGutterRenderer *renderer,
                                     gint                     xpad)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (xpad >= 0);

	if (priv->xpad != xpad)
	{
		priv->xpad = xpad;
		g_object_notify_by_pspec (G_OBJECT (renderer),
		                          properties[PROP_XPAD]);
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

/**
 * gtk_source_gutter_renderer_get_ypad:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Gets the `ypad` property.
 *
 * This may be used to adjust the cell rectangle that the renderer will use to draw.
 */
gint
gtk_source_gutter_renderer_get_ypad (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), 0);

	return priv->ypad;
}

/**
 * gtk_source_gutter_renderer_set_ypad:
 * @renderer: a #GtkSourceGutterRenderer
 * @ypad: the Y padding for the drawing cell
 *
 * Adjusts the `ypad` property.
 *
 * This may be used to adjust the cell rectangle that the renderer will use to draw.
 */
void
gtk_source_gutter_renderer_set_ypad (GtkSourceGutterRenderer *renderer,
                                     gint                     ypad)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (ypad >= 0);

	if (priv->ypad != ypad)
	{
		priv->ypad = ypad;
		g_object_notify_by_pspec (G_OBJECT (renderer),
		                          properties[PROP_YPAD]);
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

/**
 * gtk_source_gutter_renderer_get_xalign:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Gets the `xalign` property.
 *
 * This may be used to adjust where within the cell rectangle the renderer will draw.
 */
gfloat
gtk_source_gutter_renderer_get_xalign (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), 0);

	return priv->xalign;
}

/**
 * gtk_source_gutter_renderer_set_xalign:
 * @renderer: a #GtkSourceGutterRenderer
 * @xalign: the Y padding for the drawing cell
 *
 * Adjusts the `xalign` property.
 *
 * This may be used to adjust where within the cell rectangle the renderer will draw.
 */
void
gtk_source_gutter_renderer_set_xalign (GtkSourceGutterRenderer *renderer,
                                       gfloat                   xalign)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (xalign >= 0);

	if (priv->xalign != xalign)
	{
		priv->xalign = xalign;
		g_object_notify_by_pspec (G_OBJECT (renderer),
		                          properties[PROP_XALIGN]);
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

/**
 * gtk_source_gutter_renderer_get_yalign:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Gets the `yalign` property.
 *
 * This may be used to adjust where within the cell rectangle the renderer will draw.
 */
gfloat
gtk_source_gutter_renderer_get_yalign (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), 0);

	return priv->yalign;
}

/**
 * gtk_source_gutter_renderer_set_yalign:
 * @renderer: a #GtkSourceGutterRenderer
 * @yalign: the Y padding for the drawing cell
 *
 * Adjusts the `yalign` property.
 *
 * This may be used to adjust where within the cell rectangle the renderer will draw.
 */
void
gtk_source_gutter_renderer_set_yalign (GtkSourceGutterRenderer *renderer,
                                       gfloat                   yalign)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (yalign >= 0);

	if (priv->yalign != yalign)
	{
		priv->yalign = yalign;
		g_object_notify_by_pspec (G_OBJECT (renderer),
		                          properties[PROP_YALIGN]);
		gtk_widget_queue_draw (GTK_WIDGET (renderer));
	}
}

/**
 * gtk_source_gutter_renderer_get_buffer:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Gets the [class@Buffer] for which the gutter renderer is drawing.
 *
 * Returns: (transfer none) (nullable): a #GtkTextBuffer or %NULL
 */
GtkSourceBuffer *
gtk_source_gutter_renderer_get_buffer (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), NULL);

	return priv->buffer;
}

void
_gtk_source_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                   GtkSourceGutterLines    *lines)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines));

	g_set_object (&priv->lines, lines);
	GTK_SOURCE_GUTTER_RENDERER_GET_CLASS (renderer)->begin (renderer, lines);
}

void
_gtk_source_gutter_renderer_end (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererPrivate *priv = gtk_source_gutter_renderer_get_instance_private (renderer);

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));

	GTK_SOURCE_GUTTER_RENDERER_GET_CLASS (renderer)->end (renderer);
	g_clear_object (&priv->lines);
}

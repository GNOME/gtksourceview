/*
 * This file is part of GtkSourceView
 *
 * Copyright 2009 - Jesse van den Kieboom
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

#include "gtksourcegutter.h"
#include "gtksourcegutter-private.h"
#include "gtksourcegutterlines.h"
#include "gtksourcegutterlines-private.h"
#include "gtksourceview.h"
#include "gtksourcegutterrenderer.h"
#include "gtksourcegutterrenderer-private.h"

/**
 * SECTION:gutter
 * @Short_description: Gutter object for GtkSourceView
 * @Title: GtkSourceGutter
 * @See_also: #GtkSourceView, #GtkSourceMark
 *
 * The #GtkSourceGutter object represents the left or right gutter of the text
 * view. It is used by #GtkSourceView to draw the line numbers and
 * #GtkSourceMark<!-- -->s that might be present on a line. By packing
 * additional #GtkSourceGutterRenderer objects in the gutter, you can extend the
 * gutter with your own custom drawings.
 *
 * To get a #GtkSourceGutter, use the gtk_source_view_get_gutter() function.
 *
 * The gutter works very much the same way as cells rendered in a #GtkTreeView.
 * The concept is similar, with the exception that the gutter does not have an
 * underlying #GtkTreeModel. The builtin line number renderer is at position
 * #GTK_SOURCE_VIEW_GUTTER_POSITION_LINES (-30) and the marks renderer is at
 * #GTK_SOURCE_VIEW_GUTTER_POSITION_MARKS (-20). The gutter sorts the renderers
 * in ascending order, from left to right. So the marks are displayed on the
 * right of the line numbers.
 */

enum
{
	PROP_0,
	PROP_VIEW,
	PROP_WINDOW_TYPE,
};

typedef struct
{
	GtkSourceGutterRenderer *renderer;

	gint prelit;
	gint position;
} Renderer;

struct _GtkSourceGutter
{
  GtkContainer          parent_instance;

	GtkSourceView        *view;
	GList                *renderers;
	GtkSourceGutterLines *lines;

	GtkTextWindowType     window_type;
	GtkOrientation        orientation;

	gulong                adj_changed_handler;
	gulong                realize_handler;
	gulong                style_updated_handler;

	guint                 is_drawing : 1;
};

G_DEFINE_TYPE (GtkSourceGutter, gtk_source_gutter, GTK_TYPE_CONTAINER)

static void gtk_source_gutter_add           (GtkContainer             *container,
                                             GtkWidget                *widget);
static void gtk_source_gutter_remove        (GtkContainer             *container,
                                             GtkWidget                *widget);
static void on_view_style_updated           (GtkSourceView            *view,
                                             GtkSourceGutter          *gutter);
static void on_gutter_pressed_cb            (GtkSourceGutter          *gutter,
                                             gint                      n_presses,
                                             gdouble                   x,
                                             gdouble                   y,
                                             GtkGestureClick          *click);
static void do_redraw                       (GtkSourceGutter          *gutter);
static void gtk_source_gutter_snapshot      (GtkWidget                *widget,
                                             GtkSnapshot              *snapshot);
static void gtk_source_gutter_size_allocate (GtkWidget                *widget,
                                             gint                      width,
                                             gint                      height,
                                             gint                      baseline);

static Renderer *
renderer_new (GtkSourceGutter         *gutter,
              GtkSourceGutterRenderer *renderer,
              gint                     position)
{
	Renderer *ret;

	ret = g_slice_new0 (Renderer);
	ret->renderer = g_object_ref_sink (renderer);
	ret->position = position;
	ret->prelit = -1;

	_gtk_source_gutter_renderer_set_view (renderer, gutter->view);

	return ret;
}

static void
renderer_free (Renderer *renderer)
{
	_gtk_source_gutter_renderer_set_view (renderer->renderer, NULL);

	g_object_unref (renderer->renderer);
	g_slice_free (Renderer, renderer);
}

static void
get_alignment_modes (GtkSourceGutter *gutter,
                     gboolean        *needs_wrap_first,
                     gboolean        *needs_wrap_last)
{
	const GList *list;

	g_assert (GTK_SOURCE_GUTTER (gutter));
	g_assert (needs_wrap_first != NULL);
	g_assert (needs_wrap_last != NULL);

	*needs_wrap_first = FALSE;
	*needs_wrap_last = FALSE;

	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;
		GtkSourceGutterRendererAlignmentMode mode;

		mode = gtk_source_gutter_renderer_get_alignment_mode (renderer->renderer);

		switch (mode)
		{
			case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST:
				*needs_wrap_first = TRUE;
				break;

			case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST:
				*needs_wrap_last = TRUE;
				break;

			case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL:
			default:
				break;
		}
	}
}

static void
gtk_source_gutter_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			g_value_set_object (value, gutter->view);
			break;

		case PROP_WINDOW_TYPE:
			g_value_set_enum (value, gutter->window_type);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
on_adjustment_value_changed (GtkAdjustment   *adj,
                             GtkSourceGutter *gutter)
{
	const GList *list;

	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;

		gtk_widget_queue_draw (GTK_WIDGET (renderer->renderer));
	}
}

static GtkAdjustment *
get_adjustment (GtkSourceGutter *gutter,
		GtkSourceView   *view)
{
	if (gutter->window_type == GTK_TEXT_WINDOW_LEFT ||
	    gutter->window_type == GTK_TEXT_WINDOW_RIGHT)
	{
		return gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view));
	}
	else
	{
		return gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view));
	}
}

static void
connect_view (GtkSourceGutter *gutter,
              GtkSourceView   *view)
{
	g_assert (GTK_SOURCE_IS_GUTTER (gutter));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	gutter->adj_changed_handler =
		g_signal_connect (get_adjustment (gutter, view),
		                  "value-changed",
		                  G_CALLBACK (on_adjustment_value_changed),
		                  gutter);

	gutter->style_updated_handler =
		g_signal_connect (view,
		                  "style-updated",
		                  G_CALLBACK (on_view_style_updated),
		                  gutter);
}

static void
disconnect_view (GtkSourceGutter *gutter,
                 GtkSourceView   *view)
{
	g_assert (GTK_SOURCE_IS_GUTTER (gutter));
	g_assert (GTK_SOURCE_IS_VIEW (view));

	g_clear_signal_handler (&gutter->adj_changed_handler,
	                        get_adjustment (gutter, view));
	g_clear_signal_handler (&gutter->realize_handler, view);
	g_clear_signal_handler (&gutter->style_updated_handler, view);
}

static void
set_view (GtkSourceGutter *gutter,
          GtkSourceView   *view)
{
	g_return_if_fail (GTK_SOURCE_IS_GUTTER (gutter));
	g_return_if_fail (!view || GTK_SOURCE_IS_VIEW (view));

	if (view == gutter->view)
	{
		return;
	}

	if (gutter->view != NULL)
	{
		disconnect_view (gutter, gutter->view);
	}

	gutter->view = view;

	if (view != NULL)
	{
		connect_view (gutter, view);
	}
}

static void
do_redraw (GtkSourceGutter *gutter)
{
	if (!gutter->is_drawing)
	{
		gtk_widget_queue_draw (GTK_WIDGET (gutter));
	}
}

static void
gtk_source_gutter_map (GtkWidget *widget)
{
	gtk_widget_set_cursor_from_name (widget, "arrow");
	GTK_WIDGET_CLASS (gtk_source_gutter_parent_class)->map (widget);
}

static void
gtk_source_gutter_measure (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           int             for_size,
                           int            *minimum,
                           int            *natural,
                           int            *minimum_baseline,
                           int            *natural_baseline)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (widget);
	const GList *item;

	/* Calculate size */
	for (item = gutter->renderers; item; item = item->next)
	{
		Renderer *renderer = item->data;
		int r_minimum;
		int r_natural;
		int r_minimum_baseline;
		int r_natural_baseline;

		if (!gtk_widget_get_visible (GTK_WIDGET (renderer->renderer)))
		{
			continue;
		}

		gtk_widget_measure (GTK_WIDGET (renderer->renderer),
				    orientation,
				    for_size,
				    &r_minimum,
				    &r_natural,
				    &r_minimum_baseline,
				    &r_natural_baseline);

		*minimum += r_minimum;
		*natural += r_natural;
	}

	*minimum_baseline = -1;
	*natural_baseline = -1;
}

static void
apply_style (GtkSourceGutter *gutter,
	     GtkStyleContext *style_context)
{
	const gchar *class;

	switch (gutter->window_type)
	{
		case GTK_TEXT_WINDOW_TOP:
			class = GTK_STYLE_CLASS_TOP;
			break;

		case GTK_TEXT_WINDOW_RIGHT:
			class = GTK_STYLE_CLASS_RIGHT;
			break;

		case GTK_TEXT_WINDOW_BOTTOM:
			class = GTK_STYLE_CLASS_BOTTOM;
			break;

		case GTK_TEXT_WINDOW_LEFT:
			class = GTK_STYLE_CLASS_LEFT;
			break;

		case GTK_TEXT_WINDOW_WIDGET:
		case GTK_TEXT_WINDOW_TEXT:
		default:
			g_return_if_reached ();
	}

	gtk_style_context_add_class (style_context, class);
}

static void
gtk_source_gutter_root (GtkWidget *widget)
{
	GtkWidget *view;

	GTK_WIDGET_CLASS (gtk_source_gutter_parent_class)->root (widget);

	view = gtk_widget_get_ancestor (widget, GTK_SOURCE_TYPE_VIEW);
	set_view (GTK_SOURCE_GUTTER (widget), GTK_SOURCE_VIEW (view));
}

static void
gtk_source_gutter_unroot (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (gtk_source_gutter_parent_class)->unroot (widget);
	set_view (GTK_SOURCE_GUTTER (widget), NULL);
}

static void
gtk_source_gutter_forall (GtkContainer *container,
                          GtkCallback   callback,
                          gpointer      callback_data)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (container);
	const GList *list = gutter->renderers;

	while (list != NULL)
	{
		Renderer *renderer = list->data;

		list = list->next;

		callback (GTK_WIDGET (renderer->renderer), callback_data);
	}
}

static void
gtk_source_gutter_set_property (GObject       *object,
                                guint          prop_id,
                                const GValue  *value,
                                GParamSpec    *pspec)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (object);

	switch (prop_id)
	{
		case PROP_WINDOW_TYPE:
			gutter->window_type = g_value_get_enum (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_constructed (GObject *object)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (object);
	GtkStyleContext *context;

	if (gutter->window_type == GTK_TEXT_WINDOW_LEFT ||
	    gutter->window_type == GTK_TEXT_WINDOW_RIGHT)
	{
		gutter->orientation = GTK_ORIENTATION_HORIZONTAL;
		gtk_widget_set_vexpand (GTK_WIDGET (gutter), TRUE);
	}
	else
	{
		gutter->orientation = GTK_ORIENTATION_VERTICAL;
		gtk_widget_set_hexpand (GTK_WIDGET (gutter), TRUE);
	}

	G_OBJECT_CLASS (gtk_source_gutter_parent_class)->constructed (object);

	context = gtk_widget_get_style_context (GTK_WIDGET (gutter));
	apply_style (gutter, context);
}

static void
gtk_source_gutter_class_init (GtkSourceGutterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

	object_class->constructed = gtk_source_gutter_constructed;
	object_class->get_property = gtk_source_gutter_get_property;
	object_class->set_property = gtk_source_gutter_set_property;

	widget_class->map = gtk_source_gutter_map;
	widget_class->measure = gtk_source_gutter_measure;
	widget_class->root = gtk_source_gutter_root;
	widget_class->size_allocate = gtk_source_gutter_size_allocate;
	widget_class->snapshot = gtk_source_gutter_snapshot;
	widget_class->unroot = gtk_source_gutter_unroot;

	container_class->forall = gtk_source_gutter_forall;
	container_class->add = gtk_source_gutter_add;
	container_class->remove = gtk_source_gutter_remove;

	/**
	 * GtkSourceGutter:view:
	 *
	 * The #GtkSourceView of the gutter.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      "View",
	                                                      "",
	                                                      GTK_SOURCE_TYPE_VIEW,
	                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceGutter:window-type:
	 *
	 * The text window type on which the window is placed.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_WINDOW_TYPE,
	                                 g_param_spec_enum ("window_type",
	                                                    "Window Type",
	                                                    "The gutters' text window type",
	                                                    GTK_TYPE_TEXT_WINDOW_TYPE,
	                                                    GTK_TEXT_WINDOW_LEFT,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	gtk_widget_class_set_css_name (widget_class, "gutter");
}

static void
gtk_source_gutter_init (GtkSourceGutter *gutter)
{
	GtkGesture *click;
	GtkEventController *motion;

  gutter->window_type = GTK_TEXT_WINDOW_LEFT;

	/* Setup fallback click handling */
	click = gtk_gesture_click_new ();
	g_signal_connect_swapped (click,
	                          "pressed",
	                          G_CALLBACK (on_gutter_pressed_cb),
	                         gutter);
	gtk_widget_add_controller (GTK_WIDGET (gutter), GTK_EVENT_CONTROLLER (click));

	/* Track motion enter/leave for prelit status */
	motion = gtk_event_controller_motion_new ();
	g_signal_connect_swapped (motion,
	                          "enter",
	                          G_CALLBACK (gtk_widget_queue_draw),
	                          gutter);
	g_signal_connect_swapped (motion,
	                          "leave",
	                          G_CALLBACK (gtk_widget_queue_draw),
	                          gutter);
	g_signal_connect_swapped (motion,
	                          "motion",
	                          G_CALLBACK (gtk_widget_queue_draw),
	                          gutter);
	gtk_widget_add_controller (GTK_WIDGET (gutter), motion);
}

static gint
sort_by_position (Renderer *r1,
                  Renderer *r2,
                  gpointer  data)
{
	if (r1->position < r2->position)
	{
		return -1;
	}
	else if (r1->position > r2->position)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void
append_renderer (GtkSourceGutter *gutter,
                 Renderer        *renderer)
{
	gutter->renderers = g_list_insert_sorted_with_data (gutter->renderers,
	                                                  renderer,
	                                                  (GCompareDataFunc)sort_by_position,
	                                                  NULL);
}

GtkSourceGutter *
_gtk_source_gutter_new (GtkTextWindowType type)
{
	return g_object_new (GTK_SOURCE_TYPE_GUTTER,
	                     "window_type", type,
	                     NULL);
}

/**
 * gtk_source_gutter_get_view:
 * @gutter: a #GtkSourceGutter.
 *
 * Returns: (transfer none): the associated #GtkSourceView.
 *
 * Since: 3.24
 */
GtkSourceView *
gtk_source_gutter_get_view (GtkSourceGutter *gutter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER (gutter), NULL);

	return gutter->view;
}

/**
 * gtk_source_gutter_insert:
 * @gutter: a #GtkSourceGutter.
 * @renderer: a gutter renderer (must inherit from #GtkSourceGutterRenderer).
 * @position: the renderer position.
 *
 * Insert @renderer into the gutter. If @renderer is yet unowned then gutter
 * claims its ownership. Otherwise just increases renderer's reference count.
 * @renderer cannot be already inserted to another gutter.
 *
 * Returns: %TRUE if operation succeeded. Otherwise %FALSE.
 *
 * Since: 3.0
 *
 **/
gboolean
gtk_source_gutter_insert (GtkSourceGutter         *gutter,
                          GtkSourceGutterRenderer *renderer,
                          gint                     position)
{
	Renderer* internal_renderer;

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER (gutter), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer), FALSE);
	g_return_val_if_fail (gtk_source_gutter_renderer_get_view (renderer) == NULL, FALSE);

	internal_renderer = renderer_new (gutter, renderer, position);
	append_renderer (gutter, internal_renderer);
	gtk_widget_set_parent (GTK_WIDGET (renderer), GTK_WIDGET (gutter));
	gtk_widget_queue_resize (GTK_WIDGET (gutter));

	return TRUE;
}

static gboolean
renderer_find (GtkSourceGutter          *gutter,
               GtkSourceGutterRenderer  *renderer,
               Renderer                **ret,
               GList                   **retlist)
{
	GList *list;

	for (list = gutter->renderers; list; list = list->next)
	{
		*ret = list->data;

		if ((*ret)->renderer == renderer)
		{
			if (retlist)
			{
				*retlist = list;
			}

			return TRUE;
		}
	}

	return FALSE;
}

static void
gtk_source_gutter_add (GtkContainer *container,
		       GtkWidget    *widget)
{
	if (!GTK_SOURCE_IS_GUTTER_RENDERER (widget))
	{
		g_warning ("Cannot add %s to %s as it is not a GtkSourceGutterRenderer",
		           G_OBJECT_TYPE_NAME (widget),
		           G_OBJECT_TYPE_NAME (container));
	}
	else
	{
		gtk_source_gutter_insert (GTK_SOURCE_GUTTER (container),
		                          GTK_SOURCE_GUTTER_RENDERER (widget),
		                          0);
	}
}

static void
gtk_source_gutter_remove (GtkContainer *container,
                          GtkWidget    *widget)
{
	GtkSourceGutterRenderer *renderer;
	GtkSourceGutter *gutter;
	Renderer *ret;
	GList *retlist;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER (container));
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (widget));

	gutter = GTK_SOURCE_GUTTER (container);
	renderer = GTK_SOURCE_GUTTER_RENDERER (widget);

	if (renderer_find (gutter, renderer, &ret, &retlist))
	{
		gutter->renderers =
			g_list_delete_link (gutter->renderers, retlist);
		gtk_widget_unparent (GTK_WIDGET (renderer));
		renderer_free (ret);
		gtk_widget_queue_resize (GTK_WIDGET (gutter));
	}
	else
	{
		g_warning ("Failed to locate %s within %s",
		           G_OBJECT_TYPE_NAME (widget),
		           G_OBJECT_TYPE_NAME (gutter));
	}
}

/**
 * gtk_source_gutter_reorder:
 * @gutter: a #GtkSourceGutterRenderer.
 * @renderer: a #GtkCellRenderer.
 * @position: the new renderer position.
 *
 * Reorders @renderer in @gutter to new @position.
 *
 * Since: 2.8
 */
void
gtk_source_gutter_reorder (GtkSourceGutter         *gutter,
                           GtkSourceGutterRenderer *renderer,
                           gint                     position)
{
	Renderer *ret;
	GList *retlist;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER (gutter));
	g_return_if_fail (GTK_SOURCE_IS_GUTTER_RENDERER (renderer));

	if (renderer_find (gutter, renderer, &ret, &retlist))
	{
		gutter->renderers =
			g_list_delete_link (gutter->renderers, retlist);
		ret->position = position;
		append_renderer (gutter, ret);
		gtk_widget_queue_allocate (GTK_WIDGET (gutter));
	}
}

static void
gtk_source_gutter_size_allocate (GtkWidget *widget,
                                 gint       width,
                                 gint       height,
                                 gint       baseline)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (widget);
	const GList *list;
	gint x = 0;

	GTK_WIDGET_CLASS (gtk_source_gutter_parent_class)->size_allocate (widget,
	                                                                  width,
	                                                                  height,
	                                                                  baseline);

	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;
		GtkRequisition child_req;
		GtkAllocation alloc;

		gtk_widget_get_preferred_size (GTK_WIDGET (renderer->renderer),
		                               &child_req, NULL);

		alloc.x = x;
		alloc.y = 0;
		alloc.width = child_req.width;
		alloc.height = height;

		gtk_widget_size_allocate (GTK_WIDGET (renderer->renderer),
		                          &alloc,
		                          -1);

		x += alloc.width;
	}

	gtk_widget_queue_draw (widget);
}

static void
gtk_source_gutter_snapshot (GtkWidget   *widget,
                            GtkSnapshot *snapshot)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (widget);
	GtkTextView *text_view = GTK_TEXT_VIEW (gutter->view);
	GtkStyleContext *view_context;
	const GList *list;
	GdkRectangle visible_rect;
	GtkTextIter begin;
	GtkTextIter end;
	gboolean needs_wrap_first = FALSE;
	gboolean needs_wrap_last = FALSE;

	g_clear_object (&gutter->lines);

	if (text_view == NULL || gtk_widget_get_width (widget) == 0)
	{
		return;
	}

	/* We need the style from the view itself for highlight lines */
	view_context = gtk_widget_get_style_context (GTK_WIDGET (gutter->view));

	gtk_text_view_get_visible_rect (text_view, &visible_rect);
	gtk_text_view_get_iter_at_location (text_view, &begin,
	                                    visible_rect.x, visible_rect.y);
	gtk_text_view_get_iter_at_location (text_view, &end,
	                                    visible_rect.x,
	                                    visible_rect.y + visible_rect.height);

	/* The first step is to get line information about all the visible
	 * lines. We do this up front so that we can do it once to reduce many
	 * times the renderers need to walk through the buffer contents as that
	 * can be expensive.
	 */
	get_alignment_modes (gutter, &needs_wrap_first, &needs_wrap_last);
	gutter->lines = _gtk_source_gutter_lines_new (text_view,
	                                              &begin,
	                                              &end,
	                                              needs_wrap_first,
	                                              needs_wrap_last);

	/* Draw the current-line highlight if necessary */
	if (gtk_source_view_get_highlight_current_line (gutter->view))
	{
		guint cursor_line;

		cursor_line = _gtk_source_gutter_lines_get_cursor_line (gutter->lines);

		if (cursor_line >= gtk_source_gutter_lines_get_first (gutter->lines) &&
		    cursor_line <= gtk_source_gutter_lines_get_last (gutter->lines))
		{
			gint y;
			gint height;

			gtk_source_gutter_lines_get_line_yrange (gutter->lines,
			                                         cursor_line,
			                                         GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL,
			                                         &y,
			                                         &height);

			gtk_style_context_save (view_context);
			gtk_style_context_add_class (view_context, "current-line-number");

			gtk_snapshot_render_background (snapshot,
							view_context,
							0,
							y,
							gtk_widget_get_width (widget),
							height);

			gtk_style_context_restore (view_context);
		}
	}

	gutter->is_drawing = TRUE;

	/* Now let the renderers populate information about the lines that are
	 * to be rendered. They may need to go through line by line and add
	 * classes (GQuark) to the lines to be used when snapshoting. Since
	 * we've already calculated line information, this is relatively fast.
	 *
	 * We also only emit the ::query-data signal in the case that the
	 * renderer has not override then (*query_data) vfunc which saves quite
	 * a bit of signal overhead.
	 */
	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;

		_gtk_source_gutter_renderer_begin (renderer->renderer,
		                                   gutter->lines);
	}

	gtk_snapshot_push_clip (snapshot,
	                        &GRAPHENE_RECT_INIT (0,
	                                             0,
	                                             gtk_widget_get_width (widget),
	                                             gtk_widget_get_height (widget)));

	/* Now let the renderers draw the content for each line. Because
	 * iterating a Linked-List is slower than iterating a series of line
	 * numbers, we make the renderer list the outter loop, and the
	 * snapshotting of lines (within the renderer) the inner loop as part
	 * of snapshot.
	 */
	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;

		gtk_widget_snapshot_child (widget,
		                           GTK_WIDGET (renderer->renderer),
		                           snapshot);
	}

	gtk_snapshot_pop (snapshot);

	/* Allow to call queue_redraw() in end. */
	gutter->is_drawing = FALSE;

	/* Now notify the renderers of completion */
	for (list = gutter->renderers; list; list = list->next)
	{
		Renderer *renderer = list->data;

		_gtk_source_gutter_renderer_end (renderer->renderer);
	}
}

static Renderer *
renderer_at_x (GtkSourceGutter *gutter,
               gint             x,
               gint            *width)
{
	const GList *item;

	for (item = gutter->renderers; item; item = g_list_next (item))
	{
		Renderer *renderer = item->data;
		GtkAllocation alloc;

		gtk_widget_get_allocation (GTK_WIDGET (renderer->renderer),
		                           &alloc);

		if (x >= alloc.x && x <= alloc.x + alloc.width)
		{
			return renderer;
		}
	}

	return NULL;
}

static void
get_renderer_rect (GtkSourceGutter *gutter,
                   Renderer        *renderer,
                   GtkTextIter     *iter,
                   gint             line,
                   GdkRectangle    *rectangle)
{
	gint y;
	gint ypad;

	gtk_widget_get_allocation (GTK_WIDGET (renderer->renderer), rectangle);

	gtk_text_view_get_line_yrange (GTK_TEXT_VIEW (gutter->view),
	                               iter,
	                               &y,
	                               &rectangle->height);

	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (gutter->view),
	                                       gutter->window_type,
	                                       0,
	                                       y,
	                                       NULL,
	                                       &rectangle->y);

	ypad = gtk_source_gutter_renderer_get_ypad (renderer->renderer);

	rectangle->y += ypad;
	rectangle->height -= 2 * ypad;
}

static gboolean
renderer_query_activatable (GtkSourceGutter *gutter,
                            Renderer        *renderer,
                            gdouble          x,
                            gdouble          y,
                            GtkTextIter     *line_iter,
                            GdkRectangle    *rect)
{
	gint y_buf;
	gint yline;
	GtkTextIter iter;
	GdkRectangle r = {0};

	if (renderer == NULL)
	{
		return FALSE;
	}

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (gutter->view),
	                                       GTK_TEXT_WINDOW_WIDGET,
	                                       (gint)x,
	                                       (gint)y,
	                                       NULL,
	                                       &y_buf);

	gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (gutter->view),
	                             &iter,
	                             y_buf,
	                             &yline);

	if (yline > y_buf)
	{
		return FALSE;
	}

	get_renderer_rect (gutter, renderer, &iter, yline, &r);

	if (line_iter)
	{
		*line_iter = iter;
	}

	if (rect)
	{
		*rect = r;
	}

	if (y < r.y || y > r.y + r.height)
	{
		return FALSE;
	}

	return gtk_source_gutter_renderer_query_activatable (renderer->renderer, &iter, &r);
}

static void
on_gutter_pressed_cb (GtkSourceGutter *gutter,
                      gint             n_presses,
                      gdouble          x,
                      gdouble          y,
                      GtkGestureClick *click)
{
	const GdkEvent *last_event;
	Renderer *renderer;
	GtkTextIter line_iter;
	GdkRectangle rect;
	GdkModifierType state;
	guint button;

	g_assert (GTK_SOURCE_IS_GUTTER (gutter));
	g_assert (GTK_IS_GESTURE_CLICK (click));

	last_event = gtk_gesture_get_last_event (GTK_GESTURE (click), NULL);

	if (last_event == NULL ||
	    !gdk_event_get_state (last_event, &state) ||
	    !gdk_event_get_button (last_event, &button))
	{
		return;
	}

	/* Check cell renderer */
	renderer = renderer_at_x (gutter, x, NULL);

	if (renderer_query_activatable (gutter,
	                                renderer,
	                                x,
	                                y,
	                                &line_iter,
	                                &rect))
	{
		gtk_source_gutter_renderer_activate (renderer->renderer,
		                                     &line_iter,
		                                     &rect,
		                                     button,
		                                     state,
		                                     n_presses);

		do_redraw (gutter);

		gtk_gesture_set_state (GTK_GESTURE (click),
		                       GTK_EVENT_SEQUENCE_CLAIMED);
	}
}

static void
on_view_style_updated (GtkSourceView   *view,
                       GtkSourceGutter *gutter)
{
	do_redraw (gutter);
}

GtkSourceGutterLines *
_gtk_source_gutter_get_lines (GtkSourceGutter *gutter)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER (gutter), NULL);

	return gutter->lines;
}

void
_gtk_source_gutter_queue_draw (GtkSourceGutter *gutter)
{
	for (const GList *iter = gutter->renderers; iter; iter = iter->next)
	{
		Renderer *renderer = iter->data;

		gtk_widget_queue_allocate (GTK_WIDGET (renderer->renderer));
	}
}

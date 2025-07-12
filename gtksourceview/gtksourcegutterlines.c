/*
 * This file is part of GtkSourceView
 *
 * Copyright 2019 - Christian Hergert <chergert@redhat.com>
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

#include "gtksourcegutterlines.h"
#include "gtksourcegutterlines-private.h"

#include "quarkset-inline.h"

/**
 * GtkSourceGutterLines:
 *
 * Collected information about visible lines.
 *
 * The `GtkSourceGutterLines` object is used to collect information about
 * visible lines.
 *
 * Use this from your [signal@GutterRenderer::query-data] to collect the
 * necessary information on visible lines. Doing so reduces the number of
 * passes through the text btree allowing GtkSourceView to reach more
 * frames-per-second while performing kinetic scrolling.
 */

struct _GtkSourceGutterLines
{
	GObject           parent_instance;
	GtkTextView      *view;
	GArray           *lines;
	double            visible_offset;
	guint             first;
	guint             last;
	guint             cursor_line;
};

typedef struct
{
	QuarkSet classes;
	gint     y;
	gint     height;
	gint     first_height;
	gint     last_height;
} LineInfo;

G_DEFINE_TYPE (GtkSourceGutterLines, gtk_source_gutter_lines, G_TYPE_OBJECT)

static GQuark q_cursor_line;
static GQuark q_prelit;
static GQuark q_selected;

static void
gtk_source_gutter_lines_finalize (GObject *object)
{
	GtkSourceGutterLines *lines = (GtkSourceGutterLines *)object;

	g_clear_pointer (&lines->lines, g_array_unref);
	g_clear_object (&lines->view);

	G_OBJECT_CLASS (gtk_source_gutter_lines_parent_class)->finalize (object);
}

static void
gtk_source_gutter_lines_class_init (GtkSourceGutterLinesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_gutter_lines_finalize;

	q_cursor_line = g_quark_from_static_string ("cursor-line");
	q_prelit = g_quark_from_static_string ("prelit");
	q_selected = g_quark_from_static_string ("selected");
}

static void
gtk_source_gutter_lines_init (GtkSourceGutterLines *self)
{
	self->cursor_line = -1;
}

static void
clear_line_info (gpointer data)
{
	LineInfo *info = data;

	info->y = 0;
	info->height = 0;
	quark_set_clear (&info->classes);
}

GtkSourceGutterLines *
_gtk_source_gutter_lines_new (GtkTextView       *text_view,
                              const GtkTextIter *begin,
                              const GtkTextIter *end,
                              gboolean           needs_wrap_first,
                              gboolean           needs_wrap_last)
{
	GtkSourceGutterLines *lines;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	GtkTextIter sel_begin, sel_end;
	gboolean single_line;
	guint cursor_line;
	guint i;
	int first_selected = -1;
	int last_selected = -1;

	g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);
	g_return_val_if_fail (begin != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);
	g_return_val_if_fail (begin != end, NULL);

	buffer = gtk_text_view_get_buffer (text_view);

	g_return_val_if_fail (gtk_text_iter_get_buffer (begin) == buffer, NULL);
	g_return_val_if_fail (gtk_text_iter_get_buffer (end) == buffer, NULL);

	if (gtk_text_buffer_get_selection_bounds (buffer, &sel_begin, &sel_end))
	{
		gtk_text_iter_order (&sel_begin, &sel_end);
		first_selected = gtk_text_iter_get_line (&sel_begin);
		last_selected = gtk_text_iter_get_line (&sel_end);
	}

	if (gtk_text_iter_compare (begin, end) > 0)
	{
		const GtkTextIter *tmp = begin;
		begin = end;
		end = tmp;
	}

	g_return_val_if_fail (begin != end, NULL);

	lines = g_object_new (GTK_SOURCE_TYPE_GUTTER_LINES, NULL);
	lines->view = g_object_ref (text_view);
	lines->first = gtk_text_iter_get_line (begin);
	lines->last = gtk_text_iter_get_line (end);
	lines->lines = g_array_sized_new (FALSE,
	                                  FALSE,
	                                  sizeof (LineInfo),
	                                  lines->last - lines->first + 1);
	g_array_set_clear_func (lines->lines, clear_line_info);

	gtk_text_view_get_visible_offset (text_view, NULL, &lines->visible_offset);

	/* No need to calculate special wrapping if wrap mode is none */
	if (gtk_text_view_get_wrap_mode (text_view) == GTK_WRAP_NONE)
	{
		needs_wrap_first = FALSE;
		needs_wrap_last = FALSE;
	}

	single_line = !needs_wrap_first && !needs_wrap_last;

	/* Get the line number containing the cursor to compare while
	 * building the lines to add the "cursor-line" quark.
	 */
	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
	cursor_line = gtk_text_iter_get_line (&iter);

	lines->cursor_line = cursor_line;

	iter = *begin;

	if (!gtk_text_iter_starts_line (&iter))
	{
		gtk_text_iter_set_line_offset (&iter, 0);
	}

	for (i = lines->first; i <= lines->last; i++)
	{
		LineInfo info = {0};

		if G_LIKELY (single_line)
		{
			/* Need to use yrange so that line-height can be taken
			 * into account.
			 */
			gtk_text_view_get_line_yrange (text_view, &iter, &info.y, &info.height);

			info.first_height = info.height;
			info.last_height = info.height;
		}
		else
		{
			gtk_text_view_get_line_yrange (text_view,
			                               &iter,
			                               &info.y,
			                               &info.height);

			if (gtk_text_iter_starts_line (&iter) &&
			    gtk_text_iter_ends_line (&iter))
			{
				info.first_height = info.height;
				info.last_height = info.height;
			}
			else
			{
				GdkRectangle rect;

				if (needs_wrap_first)
				{
					gtk_text_view_get_iter_location (text_view, &iter, &rect);

					/* Try to somewhat handle line-height correctly */
					info.first_height = ((rect.y - info.y) * 2) + rect.height;
				}
				else
				{
					info.first_height = info.height;
				}

				if (needs_wrap_last)
				{
					gtk_text_iter_forward_to_line_end (&iter);

					/* Prefer the character right before \n to get
					 * more accurate rectangle sizing.
					 */
					if (!gtk_text_iter_starts_line (&iter))
					{
						gtk_text_iter_backward_char (&iter);
						gtk_text_view_get_iter_location (text_view, &iter, &rect);
						gtk_text_iter_forward_char (&iter);
					}
					else
					{
						gtk_text_view_get_iter_location (text_view, &iter, &rect);
					}

					/* Try to somewhat handle line-height correctly */
					info.last_height = ((info.y + info.height) - (rect.y + rect.height)) * 2 + rect.height;
				}
				else
				{
					info.last_height = info.first_height;
				}
			}
		}

		if G_UNLIKELY (i == cursor_line)
		{
			quark_set_add (&info.classes, q_cursor_line);
		}

		if G_UNLIKELY (i >= first_selected && i <= last_selected)
		{
			quark_set_add (&info.classes, q_selected);
		}

		g_array_append_val (lines->lines, info);

		if G_UNLIKELY (!gtk_text_iter_forward_line (&iter) &&
			       !gtk_text_iter_is_end (&iter))
		{
			break;
		}
	}

	g_return_val_if_fail (lines->lines->len > 0, NULL);
	g_return_val_if_fail ((lines->last - lines->first) >= (lines->lines->len - 1), NULL);

	return g_steal_pointer (&lines);
}

/**
 * gtk_source_gutter_lines_add_qclass:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @qname: a class name as a #GQuark
 *
 * Adds the class denoted by @qname to @line.
 *
 * You may check if a line has @qname by calling
 * [method@GutterLines.has_qclass].
 *
 * You can remove @qname by calling
 * [method@GutterLines.remove_qclass].
 */
void
gtk_source_gutter_lines_add_qclass (GtkSourceGutterLines *lines,
                                    guint                 line,
                                    GQuark                qname)
{
	LineInfo *info;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines));
	g_return_if_fail (qname != 0);
	g_return_if_fail (line >= lines->first);
	g_return_if_fail (line <= lines->last);
	g_return_if_fail (line - lines->first < lines->lines->len);

	info = &g_array_index (lines->lines, LineInfo, line - lines->first);
	quark_set_add (&info->classes, qname);
}

/**
 * gtk_source_gutter_lines_add_class:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @name: a class name
 *
 * Adds the class @name to @line.
 *
 * @name will be converted to a [alias@GLib.Quark] as part of this process. A
 * faster version of this function is available via
 * [method@GutterLines.add_qclass] for situations where the [alias@GLib.Quark] is
 * known ahead of time.
 */
void
gtk_source_gutter_lines_add_class (GtkSourceGutterLines *lines,
                                   guint                 line,
                                   const gchar          *name)
{
	g_return_if_fail (name != NULL);

	gtk_source_gutter_lines_add_qclass (lines,
	                             line,
	                             g_quark_from_string (name));
}

/**
 * gtk_source_gutter_lines_remove_class:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @name: a class name
 *
 * Removes the class matching @name from @line.
 *
 * A faster version of this function is available via
 * [method@GutterLines.remove_qclass] for situations where the
 * #GQuark is known ahead of time.
 */
void
gtk_source_gutter_lines_remove_class (GtkSourceGutterLines *lines,
                                      guint                 line,
                                      const gchar          *name)
{
	GQuark qname;

	g_return_if_fail (name != NULL);

	qname = g_quark_try_string (name);

	if (qname != 0)
	{
		gtk_source_gutter_lines_remove_qclass (lines, line, qname);
	}
}

/**
 * gtk_source_gutter_lines_remove_qclass:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @qname: a #GQuark to remove from @line
 *
 * Reverses a call to [method@GutterLines.add_qclass] by removing
 * the [alias@GLib.Quark] matching @qname.
 */
void
gtk_source_gutter_lines_remove_qclass (GtkSourceGutterLines *lines,
                                       guint                 line,
                                       GQuark                qname)
{
	LineInfo *info;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines));
	g_return_if_fail (qname != 0);
	g_return_if_fail (line >= lines->first);
	g_return_if_fail (line <= lines->last);
	g_return_if_fail (line - lines->first < lines->lines->len);

	info = &g_array_index (lines->lines, LineInfo, line - lines->first);
	quark_set_remove (&info->classes, qname);
}

/**
 * gtk_source_gutter_lines_has_class:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @name: a class name that may be converted, to a #GQuark
 *
 * Checks to see if [method@GutterLines.add_class] was called with
 * the @name for @line.
 *
 * A faster version of this function is provided via
 * [method@GutterLines.has_qclass] for situations where the quark
 * is known ahead of time.
 *
 * Returns: %TRUE if @line contains @name
 */
gboolean
gtk_source_gutter_lines_has_class (GtkSourceGutterLines *lines,
                                   guint                 line,
                                   const gchar          *name)
{
	GQuark qname;

	g_return_val_if_fail (name != NULL, FALSE);

	qname = g_quark_try_string (name);

	if (qname == 0)
	{
		return FALSE;
	}

	return gtk_source_gutter_lines_has_qclass (lines, line, qname);
}

/**
 * gtk_source_gutter_lines_has_qclass:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @qname: a #GQuark containing the class name
 *
 * Checks to see if [method@GutterLines.add_qclass] was called with
 * the quark denoted by @qname for @line.
 *
 * Returns: %TRUE if @line contains @qname
 */
gboolean
gtk_source_gutter_lines_has_qclass (GtkSourceGutterLines *lines,
                                    guint                 line,
                                    GQuark                qname)
{
	LineInfo *info;

	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), FALSE);
	g_return_val_if_fail (qname != 0, FALSE);
	g_return_val_if_fail (line >= lines->first, FALSE);
	g_return_val_if_fail (line <= lines->last, FALSE);
	g_return_val_if_fail (line - lines->first < lines->lines->len, FALSE);

	info = &g_array_index (lines->lines, LineInfo, line - lines->first);

	return quark_set_contains (&info->classes, qname);
}

/**
 * gtk_source_gutter_lines_is_cursor:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 *
 * Checks to see if @line contains the insertion cursor.
 *
 * Returns: %TRUE if the insertion cursor is on @line
 */
gboolean
gtk_source_gutter_lines_is_cursor (GtkSourceGutterLines *lines,
                                   guint                 line)
{
	return line == lines->cursor_line ||
	       gtk_source_gutter_lines_has_qclass (lines, line, q_cursor_line);
}

/**
 * gtk_source_gutter_lines_is_prelit:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 *
 * Checks to see if @line is marked as prelit. Generally, this means
 * the mouse pointer is over the line within the gutter.
 *
 * Returns: %TRUE if the line is prelit
 */
gboolean
gtk_source_gutter_lines_is_prelit (GtkSourceGutterLines *lines,
                                   guint                 line)
{
	return gtk_source_gutter_lines_has_qclass (lines, line, q_prelit);
}

/**
 * gtk_source_gutter_lines_is_selected:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 *
 * Checks to see if the view had a selection and if that selection overlaps
 * @line in some way.
 *
 * Returns: %TRUE if the line contains a selection
 */
gboolean
gtk_source_gutter_lines_is_selected (GtkSourceGutterLines *lines,
                                     guint                 line)
{
	return gtk_source_gutter_lines_has_qclass (lines, line, q_selected);
}

/**
 * gtk_source_gutter_lines_get_first:
 * @lines: a #GtkSourceGutterLines
 *
 * Gets the line number (starting from 0) for the first line that is
 * user visible.
 *
 * Returns: a line number starting from 0
 */
guint
gtk_source_gutter_lines_get_first (GtkSourceGutterLines *lines)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), 0);

	return lines->first;
}

/**
 * gtk_source_gutter_lines_get_last:
 * @lines: a #GtkSourceGutterLines
 *
 * Gets the line number (starting from 0) for the last line that is
 * user visible.
 *
 * Returns: a line number starting from 0
 */
guint
gtk_source_gutter_lines_get_last (GtkSourceGutterLines *lines)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), 0);

	return lines->last;
}

/**
 * gtk_source_gutter_lines_get_iter_at_line:
 * @lines: a #GtkSourceGutterLines
 * @iter: (out): a location for a #GtkTextIter
 * @line: the line number
 *
 * Gets a #GtkTextIter for the current buffer at @line
 */
void
gtk_source_gutter_lines_get_iter_at_line (GtkSourceGutterLines *lines,
                                          GtkTextIter          *iter,
                                          guint                 line)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines));
	g_return_if_fail (iter != NULL);

	buffer = gtk_text_view_get_buffer (lines->view);
	gtk_text_buffer_get_iter_at_line (buffer, iter, line);
}

/**
 * gtk_source_gutter_lines_get_view:
 * @lines: a #GtkSourceGutterLines
 *
 * Gets the [class@Gtk.TextView] that the `GtkSourceGutterLines` represents.
 *
 * Returns: (transfer none) (not nullable): a #GtkTextView
 */
GtkTextView *
gtk_source_gutter_lines_get_view (GtkSourceGutterLines *lines)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), NULL);

	return lines->view;
}

/**
 * gtk_source_gutter_lines_get_buffer:
 * @lines: a #GtkSourceGutterLines
 *
 * Gets the [class@Gtk.TextBuffer] that the `GtkSourceGutterLines` represents.
 *
 * Returns: (transfer none) (not nullable): a #GtkTextBuffer
 */
GtkTextBuffer *
gtk_source_gutter_lines_get_buffer (GtkSourceGutterLines *lines)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), NULL);

	return gtk_text_view_get_buffer (lines->view);
}

/**
 * gtk_source_gutter_lines_get_line_extent:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @mode: a #GtkSourceGutterRendererAlignmentMode
 * @y: (out): a location for the Y position in widget coordinates
 * @height: (out): the line height based on @mode
 *
 * Gets the Y range for a line based on @mode.
 *
 * The value for @y is relative to the renderers widget coordinates.
 *
 * Since: 5.18
 */
void
gtk_source_gutter_lines_get_line_extent (GtkSourceGutterLines                 *lines,
                                         guint                                 line,
                                         GtkSourceGutterRendererAlignmentMode  mode,
                                         double                               *y,
                                         double                               *height)
{
	LineInfo *info;

	g_return_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines));
	g_return_if_fail (line >= lines->first);
	g_return_if_fail (line <= lines->last);

	info = &g_array_index (lines->lines, LineInfo, line - lines->first);

	if (mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL)
	{
		*y = info->y;
		*height = info->height;
	}
	else if (mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST)
	{
		*y = info->y;
		*height = info->first_height;
	}
	else if (mode == GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST)
	{
		*y = info->y + info->height - info->last_height;
		*height = info->last_height;
	}
	else
	{
		g_return_if_reached ();
	}

	*y -= lines->visible_offset;
}

/**
 * gtk_source_gutter_lines_get_line_yrange:
 * @lines: a #GtkSourceGutterLines
 * @line: a line number starting from zero
 * @mode: a #GtkSourceGutterRendererAlignmentMode
 * @y: (out): a location for the Y position in widget coordinates
 * @height: (out): the line height based on @mode
 *
 * Gets the Y range for a line based on @mode.
 *
 * The value for @y is relative to the renderers widget coordinates.
 */
void
gtk_source_gutter_lines_get_line_yrange (GtkSourceGutterLines                 *lines,
                                         guint                                 line,
                                         GtkSourceGutterRendererAlignmentMode  mode,
                                         gint                                 *y,
                                         gint                                 *height)
{
	double d_y, d_height;

	gtk_source_gutter_lines_get_line_extent (lines, line, mode, &d_y, &d_height);

	if (y != NULL)
	{
		*y = floor (d_y);
	}

	if (height != NULL)
	{
		*height = ceil (d_height);
	}
}

guint
_gtk_source_gutter_lines_get_cursor_line (GtkSourceGutterLines *lines)
{
	g_return_val_if_fail (GTK_SOURCE_IS_GUTTER_LINES (lines), 0);

	return lines->cursor_line;
}

/**
 * gtk_source_gutter_lines_has_any_class:
 * @lines: a #GtkSourceGutterLines
 * @line: a line contained within @lines
 *
 * Checks to see if the line has any GQuark classes set. This can be
 * used to help renderer implementations avoid work if nothing has
 * been set on the class.
 *
 * Returns: %TRUE if any quark was set for the line
 *
 * Since: 5.6
 */
gboolean
gtk_source_gutter_lines_has_any_class (GtkSourceGutterLines *lines,
                                       guint                 line)
{
	if (lines == NULL || line < lines->first || line > lines->last || line - lines->first >= lines->lines->len)
    return FALSE;

  return g_array_index (lines->lines, LineInfo, line - lines->first).classes.len > 0;
}

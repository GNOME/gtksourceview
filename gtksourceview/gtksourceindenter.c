/*
 * This file is part of GtkSourceView
 *
 * Copyright 2015-2021 Christian Hergert <chergert@redhat.com>
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

#include "gtksourceindenter-private.h"
#include "gtksourceview.h"

/**
 * GtkSourceIndenter:
 *
 * Auto-indentation interface.
 *
 * By default, [class@View] can auto-indent as you type when
 * [property@View:auto-indent] is enabled. The indentation simply copies the
 * previous lines indentation.
 *
 * This can be changed by implementing `GtkSourceIndenter` and setting the
 * [property@View:indenter] property.
 *
 * Implementors of this interface should implement both
 * [vfunc@GtkSource.Indenter.is_trigger] and [vfunc@GtkSource.Indenter.indent].
 *
 * [vfunc@GtkSource.Indenter.is_trigger] is called upon key-press to
 * determine of the key press should trigger an indentation.  The default
 * implementation of the interface checks to see if the key was
 * [const@Gdk.KEY_Return] or [const@Gdk.KEY_KP_Enter] without %GDK_SHIFT_MASK set.
 *
 * [vfunc@GtkSource.Indenter.indent] is called after text has been
 * inserted into [class@Buffer] when
 * [vfunc@GtkSource.Indenter.is_trigger] returned %TRUE. The [struct@Gtk.TextIter]
 * is placed directly after the inserted character or characters.
 *
 * It may be beneficial to move the insertion mark using
 * [method@Gtk.TextBuffer.select_range] depending on how the indenter changes
 * the indentation.
 *
 * All changes are encapsulated within a single user action so that the
 * user may undo them using standard undo/redo accelerators.
 */

/**
 * GtkSourceIndenterInterface:
 * @is_trigger: the virtual function pointer for gtk_source_indenter_is_trigger()
 * @indent: the virtual function pointer for gtk_source_indenter_indent()
 *
 * The virtual function table for #GtkSourceIndenter.
 */

static inline gboolean
char_is_space (gunichar ch)
{
	return ch != '\n' &&
	       ch != '\r' &&
	       g_unichar_isspace (ch);
}

static gchar *
copy_prefix_for_line (GtkTextBuffer *buffer,
                      guint          line)
{
	GtkTextIter begin;
	GtkTextIter end;

	g_assert (GTK_IS_TEXT_BUFFER (buffer));

	gtk_text_buffer_get_iter_at_line_offset (buffer, &begin, line, 0);

	end = begin;
	while (!gtk_text_iter_ends_line (&end) &&
	       char_is_space (gtk_text_iter_get_char (&end)) &&
	       gtk_text_iter_forward_char (&end))
	{
		/* Do Nothing */
	}

	return gtk_text_iter_get_slice (&begin, &end);
}

static void
indent_by_copying_previous_line (GtkSourceIndenter *self,
                                 GtkSourceView     *view,
                                 GtkTextIter       *location)
{
	GtkTextBuffer *buffer;
	GtkTextIter begin;
	GtkTextIter end;
	guint line;

	g_assert (GTK_SOURCE_IS_INDENTER (self));
	g_assert (GTK_SOURCE_IS_VIEW (view));
	g_assert (location != NULL);

	buffer = gtk_text_iter_get_buffer (location);
	line = gtk_text_iter_get_line (location);

	begin = *location;
	if (!gtk_text_iter_starts_line (&begin))
	{
		gtk_text_iter_set_line_offset (&begin, 0);
	}

	end = *location;
	while (!gtk_text_iter_ends_line (&end) &&
	       char_is_space (gtk_text_iter_get_char (&end)) &&
	       gtk_text_iter_forward_char (&end))
	{
		/* Do Nothing */
	}

	if (!gtk_text_iter_equal (&begin, &end))
	{
		gtk_text_buffer_delete (buffer, &begin, &end);
	}

	if (line > 0)
	{
		gchar *text = copy_prefix_for_line (buffer, line - 1);
		gtk_text_buffer_insert (buffer, &begin, text, -1);
		g_free (text);
	}

	*location = begin;
}

static gboolean
trigger_on_newline (GtkSourceIndenter *self,
                    GtkSourceView     *view,
                    const GtkTextIter *location,
                    GdkModifierType    state,
                    guint              keyval)
{
	if ((state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_SUPER_MASK)) != 0)
		return FALSE;

	if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter)
        {
                GtkTextBuffer *buffer = gtk_text_iter_get_buffer (location);

		/* Ignore if we're at the beginning of the line. If we have
		 * content after the cursor then it's implied they just want
		 * to move the line downwards. If there is not content after
		 * the line then there is nothing to copy anyway.
		 *
		 * See https://gitlab.gnome.org/GNOME/gtksourceview/-/issues/366
		 */
		if (gtk_text_iter_starts_line (location))
		{
			return FALSE;
		}

                return !gtk_text_buffer_get_has_selection (buffer);
        }

        return FALSE;
}

G_DEFINE_INTERFACE (GtkSourceIndenter, gtk_source_indenter, G_TYPE_OBJECT)

static void
gtk_source_indenter_default_init (GtkSourceIndenterInterface *iface)
{
	iface->is_trigger = trigger_on_newline;
	iface->indent = indent_by_copying_previous_line;
}

/**
 * gtk_source_indenter_is_trigger:
 * @self: a #GtkSourceIndenter
 * @view: a #GtkSourceView
 * @location: the location where @ch is to be inserted
 * @state: modifier state for the insertion
 * @keyval: the keyval pressed such as [const@Gdk.KEY_Return]
 *
 * This function is used to determine if a key pressed should cause the
 * indenter to automatically indent.
 *
 * The default implementation of this virtual method will check to see
 * if @keyval is [const@Gdk.KEY_Return] or [const@Gdk.KEY_KP_Enter] and @state does
 * not have %GDK_SHIFT_MASK set. This is to allow the user to avoid
 * indentation when Shift+Return is pressed. Other indenters may want
 * to copy this behavior to provide a consistent experience to users.
 *
 * Returns: %TRUE if indentation should be automatically triggered;
 *   otherwise %FALSE and no indentation will be performed.
 */
gboolean
gtk_source_indenter_is_trigger (GtkSourceIndenter *self,
                                GtkSourceView     *view,
                                const GtkTextIter *location,
                                GdkModifierType    state,
                                guint              keyval)
{
	g_return_val_if_fail (GTK_SOURCE_IS_INDENTER (self), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_VIEW (view), FALSE);
	g_return_val_if_fail (location != NULL, FALSE);

	return GTK_SOURCE_INDENTER_GET_IFACE (self)->is_trigger (self, view, location, state, keyval);
}

/**
 * gtk_source_indenter_indent:
 * @self: a #GtkSourceIndenter
 * @view: a #GtkSourceView
 * @iter: (inout): the location of the indentation request
 *
 * This function should be implemented to alter the indentation of text
 * within the view.
 *
 * @view is provided so that the indenter may retrieve settings such as indentation and tab widths.
 *
 * @iter is the location where the indentation was requested. This typically
 * is after having just inserted a newline (\n) character but can be other
 * situations such as a manually requested indentation or reformatting.
 *
 * See [vfunc@GtkSource.Indenter.is_trigger] for how to trigger indentation on
 * various characters inserted into the buffer.
 *
 * The implementor of this function is expected to keep @iter valid across
 * calls to the function and should contain the location of the insert mark
 * after calling this function.
 *
 * The default implementation for this virtual function will copy the
 * indentation of the previous line.
 */
void
gtk_source_indenter_indent (GtkSourceIndenter *self,
                            GtkSourceView     *view,
                            GtkTextIter       *iter)
{
	g_return_if_fail (GTK_SOURCE_IS_INDENTER (self));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (iter != NULL);

	GTK_SOURCE_INDENTER_GET_IFACE (self)->indent (self, view, iter);
}

struct _GtkSourceIndenterInternal
{
	GObject parent_instance;
};

#define GTK_SOURCE_TYPE_INDENTER_INTERNAL (gtk_source_indenter_internal_get_type())
G_DECLARE_FINAL_TYPE (GtkSourceIndenterInternal, gtk_source_indenter_internal, GTK_SOURCE, INDENTER_INTERNAL, GObject)
G_DEFINE_TYPE_WITH_CODE (GtkSourceIndenterInternal, gtk_source_indenter_internal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_INDENTER, NULL))

static void
gtk_source_indenter_internal_class_init (GtkSourceIndenterInternalClass *klass)
{
}

static void
gtk_source_indenter_internal_init (GtkSourceIndenterInternal *self)
{
}

GtkSourceIndenter *
_gtk_source_indenter_internal_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_INDENTER_INTERNAL, NULL);
}

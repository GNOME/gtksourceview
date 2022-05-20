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

#pragma once

#include <gtk/gtk.h>

#include <gtksourceview/gtksourcetypes.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_VIM_STATE (gtk_source_vim_state_get_type())

G_DECLARE_DERIVABLE_TYPE (GtkSourceVimState, gtk_source_vim_state, GTK_SOURCE, VIM_STATE, GObject)

struct _GtkSourceVimStateClass
{
	GObjectClass parent_class;

	const char *command_bar_text;

	const char *(*get_command_bar_text) (GtkSourceVimState *self);
	void        (*view_set)             (GtkSourceVimState *self);
	void        (*enter)                (GtkSourceVimState *self);
	void        (*suspend)              (GtkSourceVimState *self,
	                                     GtkSourceVimState *to);
	void        (*resume)               (GtkSourceVimState *self,
	                                     GtkSourceVimState *from);
	void        (*leave)                (GtkSourceVimState *self);
	gboolean    (*handle_event)         (GtkSourceVimState *self,
	                                     GdkEvent          *event);
	gboolean    (*handle_keypress)      (GtkSourceVimState *self,
	                                     guint              keyval,
	                                     guint              keycode,
	                                     GdkModifierType    mods,
	                                     const char        *string);
	void        (*repeat)               (GtkSourceVimState *self);
	void        (*append_command)       (GtkSourceVimState *self,
	                                     GString           *string);
};

gboolean           gtk_source_vim_state_get_editable               (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_parent                 (GtkSourceVimState        *self,
                                                                    GtkSourceVimState        *parent);
void               gtk_source_vim_state_unparent                   (GtkSourceVimState        *self);
void               gtk_source_vim_state_push                       (GtkSourceVimState        *self,
                                                                    GtkSourceVimState        *new_state);
void               gtk_source_vim_state_pop                        (GtkSourceVimState        *self);
void               gtk_source_vim_state_append_command             (GtkSourceVimState        *self,
                                                                    GString                  *string);
void               gtk_source_vim_state_beep                       (GtkSourceVimState        *self);
GtkSourceVimState *gtk_source_vim_state_get_child                  (GtkSourceVimState        *self);
GtkSourceVimState *gtk_source_vim_state_get_current                (GtkSourceVimState        *self);
GtkSourceView     *gtk_source_vim_state_get_view                   (GtkSourceVimState        *self);
GtkSourceBuffer   *gtk_source_vim_state_get_buffer                 (GtkSourceVimState        *self,
                                                                    GtkTextIter              *insert,
                                                                    GtkTextIter              *selection_bound);
GtkSourceVimState *gtk_source_vim_state_get_root                   (GtkSourceVimState        *self);
GtkSourceVimState *gtk_source_vim_state_get_parent                 (GtkSourceVimState        *self);
GtkSourceVimState *gtk_source_vim_state_get_registers              (GtkSourceVimState        *self);
int                gtk_source_vim_state_get_count                  (GtkSourceVimState        *self);
gboolean           gtk_source_vim_state_get_count_set              (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_count                  (GtkSourceVimState        *self,
                                                                    int                       count);
gboolean           gtk_source_vim_state_get_can_repeat             (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_can_repeat             (GtkSourceVimState        *self,
                                                                    gboolean                  can_repeat);
void               gtk_source_vim_state_begin_user_action          (GtkSourceVimState        *self);
void               gtk_source_vim_state_end_user_action            (GtkSourceVimState        *self);
gboolean           gtk_source_vim_state_handle_event               (GtkSourceVimState        *self,
                                                                    GdkEvent                 *event);
void               gtk_source_vim_state_set_overwrite              (GtkSourceVimState        *self,
                                                                    gboolean                  overwrite);
gboolean           gtk_source_vim_state_synthesize                 (GtkSourceVimState        *self,
                                                                    guint                     keyval,
                                                                    GdkModifierType           mods);
void               gtk_source_vim_state_repeat                     (GtkSourceVimState        *self);
int                gtk_source_vim_state_get_visible_lines          (GtkSourceVimState        *self);
void               gtk_source_vim_state_scroll_page                (GtkSourceVimState        *self,
                                                                    int                       count);
void               gtk_source_vim_state_scroll_half_page           (GtkSourceVimState        *self,
                                                                    int                       count);
void               gtk_source_vim_state_scroll_line                (GtkSourceVimState        *self,
                                                                    int                       count);
void               gtk_source_vim_state_z_scroll                   (GtkSourceVimState        *self,
                                                                    double                    yalign);
void               gtk_source_vim_state_scroll_insert_onscreen     (GtkSourceVimState        *self);
void               gtk_source_vim_state_select                     (GtkSourceVimState        *self,
                                                                    const GtkTextIter        *insert,
                                                                    const GtkTextIter        *selection);
const char        *gtk_source_vim_state_get_current_register       (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_current_register       (GtkSourceVimState        *self,
                                                                    const char               *current_register);
const char        *gtk_source_vim_state_get_current_register_value (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_current_register_value (GtkSourceVimState        *self,
                                                                    const char               *value);
void               gtk_source_vim_state_place_cursor_onscreen      (GtkSourceVimState        *self);
guint              gtk_source_vim_state_get_visual_column          (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_visual_column          (GtkSourceVimState        *self,
                                                                    int                       visual_column);
void               gtk_source_vim_state_select_linewise            (GtkSourceVimState        *self,
                                                                    GtkTextIter              *insert,
                                                                    GtkTextIter              *selection);
void               gtk_source_vim_state_get_search                 (GtkSourceVimState        *self,
                                                                    GtkSourceSearchSettings **settings,
                                                                    GtkSourceSearchContext  **context);
gboolean           gtk_source_vim_state_get_reverse_search         (GtkSourceVimState        *self);
void               gtk_source_vim_state_set_reverse_search         (GtkSourceVimState        *self,
                                                                    gboolean                  reverse_search);
GtkTextMark       *gtk_source_vim_state_get_mark                   (GtkSourceVimState        *self,
                                                                    const char               *name);
void               gtk_source_vim_state_set_mark                   (GtkSourceVimState        *self,
                                                                    const char               *name,
                                                                    const GtkTextIter        *iter);
gboolean           gtk_source_vim_state_get_iter_at_mark           (GtkSourceVimState        *self,
                                                                    const char               *name,
                                                                    GtkTextIter              *iter);
void               gtk_source_vim_state_keyval_to_string           (guint                     keyval,
                                                                    GdkModifierType           mods,
                                                                    char                      string[16]);
void               gtk_source_vim_state_keyval_unescaped           (guint                     keyval,
                                                                    GdkModifierType           mods,
                                                                    char                      string[16]);
void               gtk_source_vim_state_push_jump                  (GtkSourceVimState        *self,
                                                                    const GtkTextIter        *iter);
gboolean           gtk_source_vim_state_jump_backward              (GtkSourceVimState        *self,
                                                                    GtkTextIter              *iter);
gboolean           gtk_source_vim_state_jump_forward               (GtkSourceVimState        *self,
                                                                    GtkTextIter              *iter);

static inline void
gtk_source_vim_state_release (GtkSourceVimState **dest)
{
	if (*dest != NULL)
	{
		gtk_source_vim_state_unparent (*dest);
		g_clear_object (dest);
	}
}

static inline void
gtk_source_vim_state_reparent (GtkSourceVimState  *state,
                               GtkSourceVimState  *parent,
                               GtkSourceVimState **dest)
{
	if (*dest == state)
		return;

	g_object_ref (parent);
	g_object_ref (state);

	gtk_source_vim_state_release (dest);
	gtk_source_vim_state_set_parent (state, parent);

	*dest = state;
	g_object_unref (parent);
}

#define gtk_source_vim_state_reparent(a,b,c)                   \
	(gtk_source_vim_state_reparent)((GtkSourceVimState*)a, \
	                                (GtkSourceVimState*)b, \
	                                (GtkSourceVimState**)c)

#define gtk_source_vim_state_release(a) \
	(gtk_source_vim_state_release)((GtkSourceVimState**)a)

static inline gboolean
gtk_source_vim_state_is_escape (guint           keyval,
                                GdkModifierType mods)
{
	return keyval == GDK_KEY_Escape ||
	       (keyval == GDK_KEY_bracketleft && (mods & GDK_CONTROL_MASK) != 0);
}

static inline gboolean
gtk_source_vim_state_is_ctrl_c (guint           keyval,
                                GdkModifierType mods)
{
	return keyval == GDK_KEY_c && (mods & GDK_CONTROL_MASK) != 0;
}

static inline GtkSourceVimState *
gtk_source_vim_state_get_ancestor (GtkSourceVimState *state,
                                   GType              type)
{
  if (state == NULL)
    return NULL;
  if (G_TYPE_CHECK_INSTANCE_TYPE (state, type))
    return state;
  return gtk_source_vim_state_get_ancestor (gtk_source_vim_state_get_parent (state), type);
}

G_END_DECLS

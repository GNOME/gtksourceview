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

#include <string.h>

#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourcelanguagemanager.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcesearchcontext.h>
#include <gtksourceview/gtksourcesearchsettings.h>
#include <gtksourceview/gtksourcestyleschememanager.h>
#include <gtksourceview/gtksourcestylescheme.h>
#include <gtksourceview/gtksourceview.h>

#include "gtksourcevim.h"
#include "gtksourcevimcharpending.h"
#include "gtksourcevimcommand.h"
#include "gtksourcevimjumplist.h"
#include "gtksourcevimregisters.h"
#include "gtksourcevimvisual.h"

typedef void (*Command) (GtkSourceVimCommand *self);

struct _GtkSourceVimCommand
{
	GtkSourceVimState       parent_instance;

	GtkSourceVimMotion     *motion;
	GtkSourceVimMotion     *selection_motion;
	GtkSourceVimTextObject *text_object;
	GtkTextMark            *mark_begin;
	GtkTextMark            *mark_end;
	char                   *command;
	char                   *options;
	char                    char_pending[16];

	guint                   ignore_mark : 1;
};

G_DEFINE_TYPE (GtkSourceVimCommand, gtk_source_vim_command, GTK_SOURCE_TYPE_VIM_STATE)

enum {
	PROP_0,
	PROP_COMMAND,
	PROP_MOTION,
	PROP_SELECTION_MOTION,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];
static GHashTable *commands;
static GPtrArray *commands_sorted;

static const struct {
	const char *ft;
	const char *id;
} ft_mappings[] = {
	{ "cs", "c-sharp" },
	{ "docbk", "docbook" },
	{ "javascript", "js" },
	{ "lhaskell", "haskell-literate" },
	{ "spec", "rpmspec" },
	{ "tex", "latex" },
	{ "xhtml", "html" },
};

static inline gboolean
parse_number (const char *str,
              int        *number)
{
	gint64 out_num;

	if (str == NULL)
		return FALSE;
	else if (!g_ascii_string_to_signed (str, 10, 0, G_MAXINT, &out_num, NULL))
		return FALSE;
	*number = out_num;
	return TRUE;
}

static void
gtk_source_vim_command_filter (GtkSourceVimCommand *self)
{
	GtkSourceVimState *root;
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self));

	if (GTK_SOURCE_IS_VIM (root))
	{
		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
		gtk_source_vim_emit_filter (GTK_SOURCE_VIM (root), &iter, &selection);
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

		gtk_text_iter_order (&iter, &selection);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
	}

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_format (GtkSourceVimCommand *self)
{
	GtkSourceVimState *root;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	root = gtk_source_vim_state_get_root (GTK_SOURCE_VIM_STATE (self));

	if (GTK_SOURCE_IS_VIM (root))
	{
		GtkSourceBuffer *buffer;
		GtkTextIter iter;
		GtkTextIter selection;

		buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

		/* Extend our selection linewise */
		gtk_text_iter_order (&iter, &selection);
		gtk_text_iter_set_line_offset (&iter, 0);
		if (!gtk_text_iter_ends_line (&selection))
		{
			gtk_text_iter_forward_to_line_end (&selection);
		}

		/* Request formatting from the application or internally */
		gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
		gtk_source_vim_emit_format (GTK_SOURCE_VIM (root), &iter, &selection);
		gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

		/* Leave cursor on first non-space character */
		gtk_text_iter_order (&iter, &selection);
		gtk_text_iter_set_line_offset (&iter, 0);
		while (!gtk_text_iter_ends_line (&iter) &&
		       g_unichar_isspace (gtk_text_iter_get_char (&iter)))
		{
			gtk_text_iter_forward_char (&iter);
		}
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
	}

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_shift (GtkSourceVimCommand *self,
                              int                  direction)
{
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter, selection;
	int count;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	if (count == 0)
	{
		return;
	}

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	gtk_text_iter_order (&iter, &selection);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

	for (int i = 0; i < count; i++)
	{

		if (direction > 0)
			gtk_source_view_indent_lines (view, &iter, &selection);
		else
			gtk_source_view_unindent_lines (view, &iter, &selection);
	}

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_text_iter_set_line_offset (&iter, 0);
	while (!gtk_text_iter_ends_line (&iter) &&
	       g_unichar_isspace (gtk_text_iter_get_char (&iter)))
		gtk_text_iter_forward_char (&iter);

	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_indent (GtkSourceVimCommand *self)
{
	gtk_source_vim_command_shift (self, 1);
}

static void
gtk_source_vim_command_unindent (GtkSourceVimCommand *self)
{
	gtk_source_vim_command_shift (self, -1);
}

static void
gtk_source_vim_command_delete (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter, selection;
	char *text;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_text_iter_get_slice (&iter, &selection);

	if (gtk_text_iter_is_end (&selection) || gtk_text_iter_is_end (&iter))
	{
		char *tmp = text;
		text = g_strdup_printf ("%s\n", tmp);
		g_free (tmp);
	}

	gtk_source_vim_state_set_current_register_value (GTK_SOURCE_VIM_STATE (self), text);

	if (self->motion != NULL)
	{
		if (gtk_source_vim_motion_is_linewise (self->motion))
		{
			gtk_text_iter_order (&iter, &selection);

			/* If we are at the end of the buffer, then we need
			 * to emulate linewise by swallowing the leading
			 * newline.
			 */
			if (gtk_text_iter_is_end (&selection) &&
			    gtk_text_iter_starts_line (&iter))
			{
				gtk_text_iter_backward_char (&iter);
			}
		}
	}

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &selection);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	g_free (text);
}

static void
gtk_source_vim_command_join (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	GtkTextIter end;
	guint offset;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_text_iter_order (&iter, &selection);
	offset = gtk_text_iter_get_offset (&iter);

	end = iter;
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);
	offset = gtk_text_iter_get_offset (&end);

	gtk_source_buffer_join_lines (buffer, &iter, &selection);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_sort (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	GtkTextIter end;
	guint offset;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_text_iter_order (&iter, &selection);
	offset = gtk_text_iter_get_offset (&iter);

	end = iter;
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);
	offset = gtk_text_iter_get_offset (&end);

	gtk_source_buffer_sort_lines (buffer, &iter, &selection, GTK_SOURCE_SORT_FLAGS_CASE_SENSITIVE, 0);
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
	gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_yank (GtkSourceVimCommand *self)
{
	GtkTextIter iter;
	GtkTextIter selection;
	char *text;

	gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_text_iter_get_slice (&iter, &selection);

	if (gtk_text_iter_is_end (&iter) || gtk_text_iter_is_end (&selection))
	{
		char *tmp = text;
		text = g_strdup_printf ("%s\n", tmp);
		g_free (tmp);
	}

	gtk_source_vim_state_set_current_register_value (GTK_SOURCE_VIM_STATE (self), text);

	g_free (text);
}

static void
gtk_source_vim_command_paste_after (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	const char *text;
	int count;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_source_vim_state_get_current_register_value (GTK_SOURCE_VIM_STATE (self));
	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	if (text == NULL)
	{
		return;
	}

	gtk_text_iter_order (&selection, &iter);

	gtk_source_vim_state_begin_user_action (GTK_SOURCE_VIM_STATE (self));

	/* If there is a \n, this is a linewise paste */
	if (g_str_has_suffix (text, "\n"))
	{
		int offset = -1;

		do
		{
			if (!gtk_text_iter_ends_line (&iter))
				gtk_text_iter_forward_to_line_end (&iter);

			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, "\n", -1);

			/* Save to place cursor later */
			if (offset == -1)
				offset = gtk_text_iter_get_offset (&iter);

			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, text, strlen (text) - 1);
		} while (--count > 0);

		/* try to place cursor in same position as vim */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}
	else
	{
		if (!gtk_text_iter_ends_line (&iter))
		{
			gtk_text_iter_forward_char (&iter);
		}

		do
		{
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, text, -1);
		} while (--count > 0);
	}

	gtk_source_vim_state_end_user_action (GTK_SOURCE_VIM_STATE (self));
}

static void
gtk_source_vim_command_paste_before (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	const char *text;
	int count;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_source_vim_state_get_current_register_value (GTK_SOURCE_VIM_STATE (self));
	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	if (text == NULL)
	{
		return;
	}

	gtk_text_iter_order (&selection, &iter);

	gtk_source_vim_state_begin_user_action (GTK_SOURCE_VIM_STATE (self));

	/* If there is a \n, this is a linewise paste */
	if (g_str_has_suffix (text, "\n"))
	{
		int offset;

		gtk_text_iter_set_line_offset (&iter, 0);
		offset = gtk_text_iter_get_offset (&iter);

		do
		{
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, text, -1);
		} while (--count > 0);

		/* try to place cursor in same position as vim */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, offset);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}
	else
	{
		do
		{
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, text, -1);
		} while (--count > 0);
	}

	gtk_source_vim_state_end_user_action (GTK_SOURCE_VIM_STATE (self));
}

static void
gtk_source_vim_command_toggle_case (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	gtk_source_vim_state_begin_user_action (GTK_SOURCE_VIM_STATE (self));
	gtk_source_buffer_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, &iter, &selection);
	gtk_source_vim_state_end_user_action (GTK_SOURCE_VIM_STATE (self));

	if (gtk_text_iter_ends_line (&iter) &&
	    !gtk_text_iter_starts_line (&iter))
	{
		gtk_text_iter_backward_char (&iter);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
	}

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_change_case (GtkSourceVimCommand     *self,
                                    GtkSourceChangeCaseType  case_type)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);

	gtk_text_iter_order (&iter, &selection);

	gtk_source_vim_state_begin_user_action (GTK_SOURCE_VIM_STATE (self));
	gtk_source_buffer_change_case (buffer, case_type, &iter, &selection);
	gtk_source_vim_state_end_user_action (GTK_SOURCE_VIM_STATE (self));

	gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	self->ignore_mark = TRUE;
}

static void
gtk_source_vim_command_upcase (GtkSourceVimCommand *self)
{
	gtk_source_vim_command_change_case (self, GTK_SOURCE_CHANGE_CASE_UPPER);
}

static void
gtk_source_vim_command_downcase (GtkSourceVimCommand *self)
{
	gtk_source_vim_command_change_case (self, GTK_SOURCE_CHANGE_CASE_LOWER);
}

static char *
rot13 (const char *str)
{
	GString *ret = g_string_new (NULL);

	for (const char *c = str; *c; c = g_utf8_next_char (c))
	{
		gunichar ch = g_utf8_get_char (c);

		if ((ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122))
		{
			if (g_ascii_tolower (ch) < 'n')
				g_string_append_c (ret, ch + 13);
			else
				g_string_append_c (ret, ch - 13);
		}
		else
		{
			g_string_append_unichar (ret, ch);
		}
	}

	return g_string_free (ret, FALSE);
}

static void
gtk_source_vim_command_rot13 (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter selection;
	char *text;
	char *new_text;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_text_iter_get_slice (&iter, &selection);
	new_text = rot13 (text);

	gtk_source_vim_state_begin_user_action (GTK_SOURCE_VIM_STATE (self));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &selection);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, new_text, -1);
	gtk_source_vim_state_end_user_action (GTK_SOURCE_VIM_STATE (self));

	gtk_source_vim_state_set_can_repeat (GTK_SOURCE_VIM_STATE (self), TRUE);

	g_free (text);
	g_free (new_text);
}

static char *
replace_chars_with (const char *text,
                    const char *replacement)
{
	GString *str;
	gsize len;

	g_assert (text != NULL);
	g_assert (replacement != NULL);

	str = g_string_new (NULL);
	len = strlen (replacement);

	for (const char *c = text; *c; c = g_utf8_next_char (c))
	{
		if (*c == '\n')
			g_string_append_c (str, '\n');
		else
			g_string_append_len (str, replacement, len);
	}

	return g_string_free (str, FALSE);
}

static void
gtk_source_vim_command_replace_one (GtkSourceVimCommand *self)
{
	GtkTextIter iter, selection;
	GtkSourceBuffer *buffer;
	char *text;
	char *new_text;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	if (self->char_pending[0] == 0)
	{
		return;
	}

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	text = gtk_text_iter_get_slice (&iter, &selection);
	new_text = replace_chars_with (text, self->char_pending);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));
	gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &iter, &selection);
	gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), &iter, new_text, -1);
	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	if (self->motion != NULL &&
	    !gtk_source_vim_motion_is_linewise (self->motion))
	{
		gtk_text_iter_backward_char (&iter);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}

	g_free (text);
	g_free (new_text);
}

static void
gtk_source_vim_command_undo (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	int count;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	do
	{
		if (!gtk_text_buffer_get_can_undo (GTK_TEXT_BUFFER (buffer)))
			break;

		gtk_text_buffer_undo (GTK_TEXT_BUFFER (buffer));
	} while (--count > 0);
}

static void
gtk_source_vim_command_redo (GtkSourceVimCommand *self)
{
	GtkSourceBuffer *buffer;
	int count;

	if (!gtk_source_vim_state_get_editable (GTK_SOURCE_VIM_STATE (self)))
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));

	do
	{
		if (!gtk_text_buffer_get_can_redo (GTK_TEXT_BUFFER (buffer)))
			break;

		gtk_text_buffer_redo (GTK_TEXT_BUFFER (buffer));
	} while (--count > 0);
}

static void
gtk_source_vim_command_colorscheme (GtkSourceVimCommand *self)
{
	GtkSourceStyleSchemeManager *manager;
	GtkSourceStyleScheme *scheme;
	GtkSourceBuffer *buffer;
	char *stripped;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (self->options == NULL)
		return;

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), NULL, NULL);
	manager = gtk_source_style_scheme_manager_get_default ();
	stripped = g_strstrip (g_strdup (self->options));
	scheme = gtk_source_style_scheme_manager_get_scheme (manager, stripped);

	if (scheme != NULL)
	{
		gtk_source_buffer_set_style_scheme (buffer, scheme);
	}

	g_free (stripped);
}

static void
gtk_source_vim_command_nohl (GtkSourceVimCommand *self)
{
	GtkSourceSearchContext *context;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), NULL, &context);
	gtk_source_search_context_set_highlight (context, FALSE);
}

static void
gtk_source_vim_command_search (GtkSourceVimCommand *self)
{
	GtkSourceSearchContext *context;
	GtkSourceSearchSettings *settings;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter, selection;
	GtkTextIter match;
	GRegex *regex;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	gtk_source_vim_state_set_reverse_search (GTK_SOURCE_VIM_STATE (self), FALSE);
	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), &settings, &context);

	if ((regex = g_regex_new (self->options, 0, 0, NULL)))
	{
		gtk_source_search_settings_set_search_text (settings, self->options);
		gtk_source_search_settings_set_regex_enabled (settings, TRUE);
		g_regex_unref (regex);
	}
	else
	{
		gtk_source_search_settings_set_regex_enabled (settings, FALSE);
		gtk_source_search_settings_set_search_text (settings, self->options);
	}

	gtk_source_search_settings_set_case_sensitive (settings, TRUE);
	gtk_source_search_settings_set_at_word_boundaries (settings, FALSE);
	gtk_source_search_context_set_highlight (context, TRUE);

	if (gtk_source_search_context_forward (context, &iter, &match, NULL, NULL))
	{
		gtk_source_vim_state_push_jump (GTK_SOURCE_VIM_STATE (self), &iter);

		if (GTK_SOURCE_IN_VIM_VISUAL (self))
		{
			GtkSourceVimState *visual;

			visual = gtk_source_vim_state_get_ancestor (GTK_SOURCE_VIM_STATE (self),
			                                            GTK_SOURCE_TYPE_VIM_VISUAL);
			gtk_source_vim_visual_warp (GTK_SOURCE_VIM_VISUAL (visual), &match, NULL);
			gtk_source_vim_visual_ignore_command (GTK_SOURCE_VIM_VISUAL (visual));
		}
		else
		{
			gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &match, &match);
		}

		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (view), &match, 0.25, TRUE, 1.0, 0.0);

		self->ignore_mark = TRUE;
	}
	else
	{
		gtk_source_search_context_set_highlight (context, FALSE);
	}
}

static void
gtk_source_vim_command_search_reverse (GtkSourceVimCommand *self)
{
	GtkSourceSearchContext *context;
	GtkSourceSearchSettings *settings;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	GtkTextIter iter, selection;
	GtkTextIter match;
	GRegex *regex;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, &selection);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	gtk_source_vim_state_set_reverse_search (GTK_SOURCE_VIM_STATE (self), TRUE);
	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), &settings, &context);

	if ((regex = g_regex_new (self->options, 0, 0, NULL)))
	{
		gtk_source_search_settings_set_search_text (settings, self->options);
		gtk_source_search_settings_set_regex_enabled (settings, TRUE);
		g_regex_unref (regex);
	}
	else
	{
		gtk_source_search_settings_set_regex_enabled (settings, FALSE);
		gtk_source_search_settings_set_search_text (settings, self->options);
	}

	gtk_source_search_settings_set_case_sensitive (settings, TRUE);
	gtk_source_search_settings_set_at_word_boundaries (settings, FALSE);
	gtk_source_search_context_set_highlight (context, TRUE);

	gtk_text_iter_backward_char (&iter);

	if (gtk_source_search_context_backward (context, &iter, &match, NULL, NULL))
	{
		gtk_source_vim_state_push_jump (GTK_SOURCE_VIM_STATE (self), &iter);

		if (GTK_SOURCE_IN_VIM_VISUAL (self))
		{
			GtkSourceVimState *visual;

			visual = gtk_source_vim_state_get_ancestor (GTK_SOURCE_VIM_STATE (self),
			                                            GTK_SOURCE_TYPE_VIM_VISUAL);
			gtk_source_vim_visual_warp (GTK_SOURCE_VIM_VISUAL (visual), &match, NULL);
			gtk_source_vim_visual_ignore_command (GTK_SOURCE_VIM_VISUAL (visual));
		}
		else
		{
			gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &match, &match);
		}

		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (view), &match, 0.25, TRUE, 1.0, 0.0);

		self->ignore_mark = TRUE;
	}
	else
	{
		gtk_source_search_context_set_highlight (context, FALSE);
	}
}

static void
gtk_source_vim_command_line_number (GtkSourceVimCommand *self)
{
	int line;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (parse_number (self->options, &line))
	{
		GtkSourceBuffer *buffer;
		GtkSourceView *view;
		GtkTextIter iter;

		if (line > 0)
			line--;

		view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));
		buffer = gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &iter, NULL);

		gtk_source_vim_state_push_jump (GTK_SOURCE_VIM_STATE (self), &iter);

		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, line);
		while (!gtk_text_iter_ends_line (&iter) &&
		       g_unichar_isspace (gtk_text_iter_get_char (&iter)))
			gtk_text_iter_forward_char (&iter);

		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
		gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (view), &iter, 0.25, TRUE, 1.0, 0.0);

		self->ignore_mark = TRUE;
	}
}

gboolean
gtk_source_vim_command_parse_search_and_replace (const char  *str,
                                                 char       **search,
                                                 char       **replace,
                                                 char       **options)
{
	const char *c;
	GString *build = NULL;
	gunichar sep;
	gboolean escaped;

	g_assert (search != NULL);
	g_assert (replace != NULL);
	g_assert (options != NULL);

	*search = NULL;
	*replace = NULL;
	*options = NULL;

	if (str == NULL || *str == 0)
		return FALSE;

	sep = g_utf8_get_char (str);
	str = g_utf8_next_char (str);

	/* Check for something like "s/" */
	if (*str == 0)
		return TRUE;

	build = g_string_new (NULL);
	escaped = FALSE;
	for (c = str; *c; c = g_utf8_next_char (c))
	{
		gunichar ch = g_utf8_get_char (c);

		if (escaped)
		{
			escaped = FALSE;

			if (ch == sep)
			{
				/* don't escape separator in output string */
				g_string_truncate (build, build->len - 1);
			}
		}
		else if (ch == '\\')
		{
			escaped = TRUE;
		}
		else if (ch == sep)
		{
			*search = g_string_free (g_steal_pointer (&build), FALSE);
			str = g_utf8_next_char (c);
			break;
		}

		g_string_append_unichar (build, ch);
	}

	if (escaped)
		return FALSE;

	/* Handle s/foobar (imply //) */
	if (build != NULL)
	{
		*search = g_string_free (g_steal_pointer (&build), FALSE);
		return TRUE;
	}

	if (*str == 0)
		return TRUE;

	build = g_string_new (NULL);
	escaped = FALSE;
	for (c = str; *c; c = g_utf8_next_char (c))
	{
		gunichar ch = g_utf8_get_char (c);

		if (escaped)
		{
			escaped = FALSE;

			if (ch == sep)
			{
				/* don't escape separator in output string */
				g_string_truncate (build, build->len - 1);
			}
		}
		else if (ch == '\\')
		{
			escaped = TRUE;
		}
		else if (ch == sep)
		{
			*replace = g_string_free (g_steal_pointer (&build), FALSE);
			str = g_utf8_next_char (c);
			break;
		}

		g_string_append_unichar (build, ch);
	}

	if (escaped)
		return FALSE;

	/* Handle s/foo/bar (imply trailing /) */
	if (build != NULL)
	{
		*replace = g_string_free (g_steal_pointer (&build), FALSE);
		return TRUE;
	}

	if (*str != 0)
		*options = g_strdup (str);

	return TRUE;
}

static void
gtk_source_vim_command_search_replace (GtkSourceVimCommand *self)
{
	GtkSourceSearchSettings *settings = NULL;
	GtkSourceSearchContext *context = NULL;
	GtkSourceBuffer *buffer;
	GtkTextIter iter;
	GtkTextIter match_start;
	GtkTextIter match_end;
	const char *replace_str;
	char *search = NULL;
	char *replace = NULL;
	char *options = NULL;
	gboolean wrapped = FALSE;
	gboolean flag_g = FALSE;
	gboolean flag_i = FALSE;
	gboolean found_match = FALSE;
	guint line = 0;
	int last_line;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (!gtk_source_vim_command_parse_search_and_replace (self->options, &search, &replace, &options))
		goto cleanup;

	if (search == NULL || search[0] == 0)
		goto cleanup;

	replace_str = replace ? replace : "";

	for (const char *c = options ? options : ""; *c; c = g_utf8_next_char (c))
	{
		flag_g |= *c == 'g';
		flag_i |= *c == 'i';
	}

	gtk_source_vim_state_get_search (GTK_SOURCE_VIM_STATE (self), &settings, &context);
	gtk_source_vim_state_set_reverse_search (GTK_SOURCE_VIM_STATE (self), FALSE);

	gtk_source_search_settings_set_at_word_boundaries (settings, FALSE);
	gtk_source_search_settings_set_regex_enabled (settings, TRUE);
	gtk_source_search_settings_set_search_text (settings, search);
	gtk_source_search_context_set_highlight (context, FALSE);
	gtk_source_search_settings_set_case_sensitive (settings, !flag_i);

	buffer = gtk_source_search_context_get_buffer (context);

	if (self->mark_begin)
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, self->mark_begin);
	else
		gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &iter, NULL);

	line = gtk_text_iter_get_line (&iter);
	last_line = -1;

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

	while (gtk_source_search_context_forward (context, &iter, &match_start, &match_end, &wrapped) && !wrapped)
	{
		guint cur_line = gtk_text_iter_get_line (&match_start);

		if (!found_match)
		{
			GtkTextIter cursor;

			gtk_source_vim_state_get_buffer (GTK_SOURCE_VIM_STATE (self), &cursor, NULL);
			gtk_source_vim_state_push_jump (GTK_SOURCE_VIM_STATE (self), &cursor);

			found_match = TRUE;
		}

		if (self->mark_end)
		{
			GtkTextIter end;
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &end, self->mark_end);
			if (gtk_text_iter_compare (&match_start, &end) >= 0)
				break;
		}
		else if (gtk_text_iter_get_line (&match_start) != line)
		{
			/* If we have no bounds, it's only the current line */
			break;
		}

		if (cur_line == last_line && !flag_g)
		{
			goto next_result;
		}

		last_line = cur_line;
		if (!gtk_source_search_context_replace (context, &match_start, &match_end, replace_str, -1, NULL))
		{
			break;
		}

	next_result:
		iter = match_end;
		gtk_text_iter_forward_char (&iter);
	}

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	if (last_line >= 0)
	{
		gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer), &iter, last_line);
		while (!gtk_text_iter_ends_line (&iter) &&
		       g_unichar_isspace (gtk_text_iter_get_char (&iter)))
			gtk_text_iter_forward_char (&iter);
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}

cleanup:

	g_free (search);
	g_free (replace);
	g_free (options);
}

static void
gtk_source_vim_command_set (GtkSourceVimCommand *self)
{
	GtkSourceVimState *state = (GtkSourceVimState *)self;
	GtkSourceSearchContext *context;
	GtkSourceSearchSettings *search;
	GtkSourceBuffer *buffer;
	GtkSourceView *view;
	char **parts = NULL;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (self->options == NULL || g_strstrip (self->options)[0] == 0)
	{
		/* TODO: display current settings */
		return;
	}

	view = gtk_source_vim_state_get_view (state);
	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	parts = g_strsplit (self->options, " ", 0);

	for (guint i = 0; parts[i]; i++)
	{
		const char *part = parts[i];

		if (g_str_equal (part, "hls"))
		{
			gtk_source_vim_state_get_search (state, &search, &context);
			gtk_source_search_context_set_highlight (context, TRUE);
		}
		else if (g_str_equal (part, "incsearch"))
		{
			/* TODO */
		}
		else if (g_str_has_prefix (part, "ft=") ||
			 g_str_has_prefix (part, "filetype="))
		{
			const char *ft = strchr (part, '=') + 1;
			GtkSourceLanguageManager *manager;
			GtkSourceLanguage *language;

			for (guint j = 0; j < G_N_ELEMENTS (ft_mappings); j++)
			{
				if (g_str_equal (ft_mappings[j].ft, ft))
				{
					ft = ft_mappings[j].id;
					break;
				}
			}

			manager = gtk_source_language_manager_get_default ();
			language = gtk_source_language_manager_get_language (manager, ft);

			gtk_source_buffer_set_language (buffer, language);

			if (language != NULL)
			{
				gtk_source_buffer_set_highlight_syntax (buffer, TRUE);
			}
		}
		else if (g_str_has_prefix (part, "ts=") ||
			 g_str_has_prefix (part, "tabstop="))
		{
			const char *ts = strchr (part, '=') + 1;
			int n;

			if (parse_number (ts, &n))
			{
				if (n >= -1 && n != 0 && n <= 32)
					gtk_source_view_set_tab_width (view, n);
			}
		}
		else if (g_str_has_prefix (part, "sw=") ||
			 g_str_has_prefix (part, "shiftwidth="))
		{
			const char *sw = strchr (part, '=') + 1;
			int n;

			if (parse_number (sw, &n))
			{
				if (n >= -1 && n != 0 && n <= 32)
					gtk_source_view_set_indent_width (view, n);
			}
		}
		else if (g_str_has_prefix (part, "tw=") ||
			 g_str_has_prefix (part, "textwidth="))
		{
			const char *sw = strchr (part, '=') + 1;
			int n;

			if (parse_number (sw, &n))
			{
				if (n >= 1 && n <= 1000)
					gtk_source_view_set_right_margin_position (view, n);
			}
		}
		else if (g_str_equal (part, "syntax=off"))
		{
			gtk_source_buffer_set_highlight_syntax (buffer, FALSE);
		}
		else if (g_str_equal (part, "et") ||
			 g_str_equal (part, "expandtab"))
		{
			gtk_source_view_set_insert_spaces_instead_of_tabs (view, TRUE);
		}
		else if (g_str_equal (part, "noet") ||
			 g_str_equal (part, "noexpandtab"))
		{
			gtk_source_view_set_insert_spaces_instead_of_tabs (view, FALSE);
		}
		else if (g_str_equal (part, "nu"))
		{
			gtk_source_view_set_show_line_numbers (view, TRUE);
		}
		else if (g_str_equal (part, "nonu"))
		{
			gtk_source_view_set_show_line_numbers (view, FALSE);
		}
		else if (g_str_equal (part, "wrap"))
		{
			gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
		}
		else if (g_str_equal (part, "nowrap"))
		{
			gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_NONE);
		}
		else if (g_str_equal (part, "ai") || g_str_equal (part, "autoindent"))
		{
			gtk_source_view_set_auto_indent (view, TRUE);
		}
		else if (g_str_equal (part, "noai") || g_str_equal (part, "noautoindent"))
		{
			gtk_source_view_set_auto_indent (view, FALSE);
		}
	}

	g_strfreev (parts);
}

static void
gtk_source_vim_command_append_command (GtkSourceVimState *state,
                                       GString           *string)
{
	/* command should be empty during command */
	g_string_truncate (string, 0);
}

static void
gtk_source_vim_command_repeat (GtkSourceVimState *state)
{
	GtkSourceVimCommand *self = (GtkSourceVimCommand *)state;
	GtkSourceBuffer *buffer;
	Command command;
	GtkTextIter iter;
	GtkTextIter selection;
	GtkTextMark *mark;
	gboolean linewise = FALSE;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (self->command == NULL ||
	    !(command = g_hash_table_lookup (commands, self->command)))
	{
		return;
	}

	buffer = gtk_source_vim_state_get_buffer (state, &iter, &selection);
	mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &iter, TRUE);

	if (self->text_object)
	{
		selection = iter;
		gtk_source_vim_text_object_select (self->text_object, &iter, &selection);
	}
	else
	{
		if (self->motion)
		{
			gtk_source_vim_motion_apply (self->motion, &iter, TRUE);
			linewise |= gtk_source_vim_motion_is_linewise (self->motion);
		}

		if (self->selection_motion)
		{
			gtk_source_vim_motion_apply (self->selection_motion, &selection, TRUE);
			linewise |= gtk_source_vim_motion_is_linewise (self->selection_motion);
		}
	}

	if (linewise)
	{
		gtk_source_vim_state_select_linewise (state, &iter, &selection);
	}
	else
	{
		gtk_source_vim_state_select (state, &iter, &selection);
	}

	command (self);

	if (!self->ignore_mark)
	{
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, mark);
		gtk_text_buffer_select_range (GTK_TEXT_BUFFER (buffer), &iter, &iter);
	}

	gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), mark);
}

static void
gtk_source_vim_command_jump_backward (GtkSourceVimCommand *self)
{
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (gtk_source_vim_state_jump_backward (GTK_SOURCE_VIM_STATE (self), &iter))
	{
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}
}

static void
gtk_source_vim_command_jump_forward (GtkSourceVimCommand *self)
{
	GtkTextIter iter;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));

	if (gtk_source_vim_state_jump_forward (GTK_SOURCE_VIM_STATE (self), &iter))
	{
		gtk_source_vim_state_select (GTK_SOURCE_VIM_STATE (self), &iter, &iter);
		self->ignore_mark = TRUE;
	}
}

static void
gtk_source_vim_command_enter (GtkSourceVimState *state)
{
	g_assert (GTK_SOURCE_IS_VIM_COMMAND (state));

}

static void
gtk_source_vim_command_leave (GtkSourceVimState *state)
{
	g_assert (GTK_SOURCE_IS_VIM_COMMAND (state));

	gtk_source_vim_command_repeat (state);
}

static void
gtk_source_vim_command_resume (GtkSourceVimState *state,
                               GtkSourceVimState *from)
{
	GtkSourceVimCommand *self = (GtkSourceVimCommand *)state;

	g_assert (GTK_SOURCE_IS_VIM_COMMAND (self));
	g_assert (GTK_SOURCE_IS_VIM_STATE (from));

	/* Complete if waiting for a motion. If we had a count, instead
	 * move it to the motion.
	 */
	if (GTK_SOURCE_IS_VIM_MOTION (from) && self->motion == NULL)
	{
		int count = gtk_source_vim_state_get_count (state);

		if (count > 1)
		{
			gtk_source_vim_state_set_count (from, count);
			gtk_source_vim_state_set_count (state, 0);
		}

		gtk_source_vim_state_reparent (from, state, &self->motion);
		gtk_source_vim_state_pop (state);

		return;
	}

	/* If we're waiting for a character, that could complete too */
	if (GTK_SOURCE_IS_VIM_CHAR_PENDING (from))
	{
		gunichar ch = gtk_source_vim_char_pending_get_character (GTK_SOURCE_VIM_CHAR_PENDING (from));
		const char *string = gtk_source_vim_char_pending_get_string (GTK_SOURCE_VIM_CHAR_PENDING (from));

		if (ch && string && string[0])
			g_strlcpy (self->char_pending, string, sizeof self->char_pending);

		gtk_source_vim_state_unparent (from);
		gtk_source_vim_state_pop (state);
		return;
	}

	gtk_source_vim_state_unparent (from);
}

static int
sort_longest_first (gconstpointer a,
                    gconstpointer b)
{
	const char * const *astr = a;
	const char * const *bstr = b;
	int lena = strlen (*astr);
	int lenb = strlen (*bstr);

	if (lena > lenb)
		return -1;
	else if (lena < lenb)
		return 1;
	return 0;
}

static void
gtk_source_vim_command_dispose (GObject *object)
{
	GtkSourceVimCommand *self = (GtkSourceVimCommand *)object;
	GtkTextBuffer *buffer;

	if (self->mark_begin)
	{
		if ((buffer = gtk_text_mark_get_buffer (self->mark_begin)))
			gtk_text_buffer_delete_mark (buffer, self->mark_begin);
		g_clear_weak_pointer (&self->mark_begin);
	}

	if (self->mark_end)
	{
		if ((buffer = gtk_text_mark_get_buffer (self->mark_end)))
			gtk_text_buffer_delete_mark (buffer, self->mark_end);
		g_clear_weak_pointer (&self->mark_end);
	}

	gtk_source_vim_state_release (&self->motion);
	gtk_source_vim_state_release (&self->selection_motion);
	gtk_source_vim_state_release (&self->text_object);

	g_clear_pointer (&self->command, g_free);
	g_clear_pointer (&self->options, g_free);

	G_OBJECT_CLASS (gtk_source_vim_command_parent_class)->dispose (object);
}

static void
gtk_source_vim_command_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GtkSourceVimCommand *self = GTK_SOURCE_VIM_COMMAND (object);

	switch (prop_id)
	{
		case PROP_COMMAND:
			g_value_set_string (value, self->command);
			break;

		case PROP_MOTION:
			g_value_set_object (value, self->motion);
			break;

		case PROP_SELECTION_MOTION:
			g_value_set_object (value, self->selection_motion);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_command_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GtkSourceVimCommand *self = GTK_SOURCE_VIM_COMMAND (object);

	switch (prop_id)
	{
		case PROP_COMMAND:
			self->command = g_value_dup_string (value);
			break;

		case PROP_MOTION:
			gtk_source_vim_command_set_motion (self, g_value_get_object (value));
			break;

		case PROP_SELECTION_MOTION:
			gtk_source_vim_command_set_selection_motion (self, g_value_get_object (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_command_class_init (GtkSourceVimCommandClass *klass)
{
	GtkSourceVimStateClass *state_class = GTK_SOURCE_VIM_STATE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_command_dispose;
	object_class->get_property = gtk_source_vim_command_get_property;
	object_class->set_property = gtk_source_vim_command_set_property;

	state_class->append_command = gtk_source_vim_command_append_command;
	state_class->enter = gtk_source_vim_command_enter;
	state_class->leave = gtk_source_vim_command_leave;
	state_class->repeat = gtk_source_vim_command_repeat;
	state_class->resume = gtk_source_vim_command_resume;

	properties [PROP_COMMAND] =
		g_param_spec_string ("command",
		                     "Command",
		                     "The command to run",
		                     NULL,
		                     (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

	properties [PROP_MOTION] =
		g_param_spec_object ("motion",
		                     "Motion",
		                     "The motion for the insertion cursor",
		                     GTK_SOURCE_TYPE_VIM_MOTION,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	properties [PROP_SELECTION_MOTION] =
		g_param_spec_object ("selection-motion",
		                     "Seleciton Motion",
		                     "The motion for the selection bound",
		                     GTK_SOURCE_TYPE_VIM_MOTION,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	commands = g_hash_table_new (g_str_hash, g_str_equal);
	commands_sorted = g_ptr_array_new ();

#define ADD_COMMAND(name, func) \
	G_STMT_START { \
		g_hash_table_insert(commands, (char*)name, (gpointer)func); \
		g_ptr_array_add (commands_sorted, (char *)name); \
	} G_STMT_END
	ADD_COMMAND (":colorscheme",   gtk_source_vim_command_colorscheme);
	ADD_COMMAND (":delete",        gtk_source_vim_command_delete);
	ADD_COMMAND (":j",             gtk_source_vim_command_join);
	ADD_COMMAND (":join",          gtk_source_vim_command_join);
	ADD_COMMAND (":nohl",          gtk_source_vim_command_nohl);
	ADD_COMMAND (":redo",          gtk_source_vim_command_redo);
	ADD_COMMAND (":set",           gtk_source_vim_command_set);
	ADD_COMMAND (":sort",          gtk_source_vim_command_sort);
	ADD_COMMAND (":u",             gtk_source_vim_command_undo);
	ADD_COMMAND (":undo",          gtk_source_vim_command_undo);
	ADD_COMMAND (":y",             gtk_source_vim_command_yank);
	ADD_COMMAND (":yank",          gtk_source_vim_command_yank);
	ADD_COMMAND ("paste-after",    gtk_source_vim_command_paste_after);
	ADD_COMMAND ("paste-before",   gtk_source_vim_command_paste_before);
	ADD_COMMAND ("toggle-case",    gtk_source_vim_command_toggle_case);
	ADD_COMMAND ("upcase",         gtk_source_vim_command_upcase);
	ADD_COMMAND ("downcase",       gtk_source_vim_command_downcase);
	ADD_COMMAND ("rot13",          gtk_source_vim_command_rot13);
	ADD_COMMAND ("replace-one",    gtk_source_vim_command_replace_one);
	ADD_COMMAND ("indent",         gtk_source_vim_command_indent);
	ADD_COMMAND ("unindent",       gtk_source_vim_command_unindent);
	ADD_COMMAND ("line-number",    gtk_source_vim_command_line_number);
	ADD_COMMAND ("filter",         gtk_source_vim_command_filter);
	ADD_COMMAND ("format",         gtk_source_vim_command_format);
	ADD_COMMAND ("search",         gtk_source_vim_command_search);
	ADD_COMMAND ("search-replace", gtk_source_vim_command_search_replace);
	ADD_COMMAND ("search-reverse", gtk_source_vim_command_search_reverse);
	ADD_COMMAND ("jump-backward",  gtk_source_vim_command_jump_backward);
	ADD_COMMAND ("jump-forward",   gtk_source_vim_command_jump_forward);
#undef ADD_COMMAND

	g_ptr_array_sort (commands_sorted, sort_longest_first);
}

static void
gtk_source_vim_command_init (GtkSourceVimCommand *self)
{
}

void
gtk_source_vim_command_set_motion (GtkSourceVimCommand *self,
                                   GtkSourceVimMotion  *motion)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_COMMAND (self));
	g_return_if_fail (!motion || GTK_SOURCE_IS_VIM_MOTION (motion));

	gtk_source_vim_state_reparent (motion, self, &self->motion);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MOTION]);
}

void
gtk_source_vim_command_set_selection_motion (GtkSourceVimCommand *self,
                                             GtkSourceVimMotion  *selection_motion)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_COMMAND (self));
	g_return_if_fail (!selection_motion || GTK_SOURCE_IS_VIM_MOTION (selection_motion));

	gtk_source_vim_state_reparent (selection_motion, self, &self->selection_motion);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTION_MOTION]);
}

const char *
gtk_source_vim_command_get_command (GtkSourceVimCommand *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_COMMAND (self), NULL);

	return self->command;
}

GtkSourceVimState *
gtk_source_vim_command_new (const char *command)
{
	g_return_val_if_fail (command != NULL, NULL);

	return g_object_new (GTK_SOURCE_TYPE_VIM_COMMAND,
	                     "command", command,
	                     NULL);
}

void
gtk_source_vim_command_set_text_object (GtkSourceVimCommand    *self,
                                        GtkSourceVimTextObject *text_object)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_COMMAND (self));

	gtk_source_vim_state_reparent (text_object, self, &self->text_object);
}

static gboolean
parse_position (GtkSourceVimState  *current,
                const char        **str,
                GtkTextIter        *iter)
{
	GtkTextBuffer *buffer;
	const char *c;

	g_assert (GTK_SOURCE_IS_VIM_STATE (current));
	g_assert (str != NULL);
	g_assert (*str != NULL);
	g_assert (iter != NULL);

	buffer = GTK_TEXT_BUFFER (gtk_source_vim_state_get_buffer (current, NULL, NULL));
	c = *str;

	if (*c == '\'')
	{
		GtkTextMark *mark;
		char name[2];

		c++;
		name[0] = *c;
		name[1] = 0;

		if (!(mark = gtk_source_vim_state_get_mark (current, name)))
			return FALSE;

		gtk_text_buffer_get_iter_at_mark (buffer, iter, mark);

		/* As of Vim 7.3, substitutions applied to a range defined by
		 * marks or a visual selection (which uses a special type of
		 * marks '< and '>) are not bounded by the column position of
		 * the marks by default.
		 */
		if (*c == '<' && !gtk_text_iter_starts_line (iter))
		{
			gtk_text_iter_set_line_offset (iter, 0);
		}
		else if (*c == '>' && !gtk_text_iter_ends_line (iter))
		{
			if (gtk_text_iter_starts_line (iter))
				gtk_text_iter_backward_char (iter);
		}

		*str = ++c;
		return TRUE;
	}
	else if (*c == '.')
	{
		gtk_text_buffer_get_iter_at_mark (buffer, iter, gtk_text_buffer_get_insert (buffer));
		gtk_text_iter_set_line_offset (iter, 0);
		*str = ++c;
		return TRUE;
	}
	else if (*c == '$')
	{
		gtk_text_buffer_get_end_iter (buffer, iter);
		*str = ++c;
		return TRUE;
	}
	else if (*c == '+' && g_ascii_isdigit (c[1]))
	{
		int number = 0;

		for (++c; *c; c = g_utf8_next_char (c))
		{
			if (!g_ascii_isdigit (*c))
				break;
			number = number * 10 + *c - '0';
		}

		gtk_text_buffer_get_iter_at_mark (buffer, iter, gtk_text_buffer_get_insert (buffer));
		gtk_text_iter_forward_lines (iter, number);
		if (!gtk_text_iter_ends_line (iter))
			gtk_text_iter_forward_to_line_end (iter);

		*str = c;

		return TRUE;
	}
	else if (g_ascii_isdigit (*c))
	{
		int number = 0;

		for (; *c; c = g_utf8_next_char (c))
		{
			if (!g_ascii_isdigit (*c))
				break;
			number = number * 10 + *c - '0';
		}

		if (number > 0)
			number--;

		gtk_text_buffer_get_iter_at_line (buffer, iter, number);
		*str = c;
		return TRUE;
	}

	return FALSE;
}

static gboolean
parse_range (GtkSourceVimState  *current,
             const char        **cmdline_inout,
             GtkTextIter        *begin,
             GtkTextIter        *end)
{
	GtkSourceBuffer *buffer = gtk_source_vim_state_get_buffer (current, NULL, NULL);
	const char *cmdline = *cmdline_inout;

	if (cmdline[0] == '%')
	{
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), begin, end);
		*cmdline_inout = ++cmdline;
		return TRUE;
	}

	if (!parse_position (current, &cmdline, begin))
		return FALSE;

	if (*cmdline != ',')
		return FALSE;

	cmdline++;
	if (!parse_position (current, &cmdline, end))
		return FALSE;

	*cmdline_inout = cmdline;

	return TRUE;
}

GtkSourceVimState *
gtk_source_vim_command_new_parsed (GtkSourceVimState *current,
                                   const char        *command_line)
{
	GtkSourceVimCommand *ret = NULL;
	GtkSourceVimCommandClass *klass;
	GtkTextMark *mark_begin = NULL;
	GtkTextMark *mark_end = NULL;
	GtkTextIter begin;
	GtkTextIter end;
	char *key = NULL;
	int number;

	g_return_val_if_fail (command_line != NULL, NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_STATE (current), NULL);

	klass = g_type_class_ref (GTK_SOURCE_TYPE_VIM_COMMAND);

	if (*command_line == ':')
	{
		command_line++;
	}

	if (parse_range (current, &command_line, &begin, &end))
	{
		GtkSourceBuffer *buffer;

		buffer = gtk_source_vim_state_get_buffer (current, NULL, NULL);
		mark_begin = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &begin, TRUE);
		mark_end = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, &end, FALSE);
	}

	/* Try to locate command after parsing ranges */
	key = g_strdup_printf (":%s", command_line);
	if (g_hash_table_contains (commands, key))
	{
		ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new (key));
		goto finish;
	}

	if (*command_line == '/')
	{
		ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new ("search"));
		g_set_str (&ret->options, command_line+1);

		goto finish;
	}
	else if (*command_line == '?')
	{
		ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new ("search-reverse"));
		g_set_str (&ret->options, command_line+1);

		goto finish;
	}

	if (strchr (command_line, ' '))
	{
		char **split = g_strsplit (command_line, " ", 2);
		char *name = g_strdup_printf (":%s", split[0]);

		if (g_hash_table_contains (commands, name))
		{
			ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new (name));
			g_set_str (&ret->options, split[1]);
		}

		g_strfreev (split);
		g_free (name);

		if (ret != NULL)
			goto finish;
	}

	if (parse_number (command_line, &number))
	{
		ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new ("line-number"));
		g_set_str (&ret->options, command_line);

		goto finish;
	}

	if (*command_line == 's')
	{
		ret = GTK_SOURCE_VIM_COMMAND (gtk_source_vim_command_new ("search-replace"));
		g_set_str (&ret->options, command_line+1);

		goto finish;
	}

finish:

	if (ret != NULL)
	{
		g_set_weak_pointer (&ret->mark_begin, mark_begin);
		g_set_weak_pointer (&ret->mark_end, mark_end);
	}
	else if (mark_begin || mark_end)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (mark_begin), mark_begin);
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (mark_end), mark_end);
	}

	g_type_class_unref (klass);
	g_free (key);

	return GTK_SOURCE_VIM_STATE (ret);
}

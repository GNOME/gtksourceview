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

#include "gtksourcevimregisters.h"

#define DEFAULT_REGISTER "\""
#define MAX_BYTES (4096L*16L) /* 64kb */

struct _GtkSourceVimRegisters
{
	GtkSourceVimState parent_instance;
};

G_DEFINE_TYPE (GtkSourceVimRegisters, gtk_source_vim_registers, GTK_SOURCE_TYPE_VIM_STATE)

static GHashTable *g_values;
static char       *g_clipboard;
static char       *g_primary_clipboard;
static char       *g_numbered[10];
static int         g_numbered_pos;

static void
gtk_source_vim_registers_class_init (GtkSourceVimRegistersClass *klass)
{
	g_values = g_hash_table_new_full (g_str_hash,
	                                  g_str_equal,
	                                  NULL,
	                                  (GDestroyNotify)g_ref_string_release);
}

static void
gtk_source_vim_registers_init (GtkSourceVimRegisters *self)
{
}

static void
write_clipboard (GtkSourceVimRegisters *self,
                 GdkClipboard          *clipboard,
                 char                  *refstr)
{
	g_assert (GTK_SOURCE_IS_VIM_REGISTERS (self));
	g_assert (GDK_IS_CLIPBOARD (clipboard));
	g_assert (refstr != NULL);

	gdk_clipboard_set_text (clipboard, refstr);
}

static gboolean
cancel_cb (gpointer data)
{
	g_cancellable_cancel (data);
	return G_SOURCE_REMOVE;
}

typedef struct
{
	char *text;
	GMainLoop *main_loop;
	GCancellable *cancellable;
} ReadClipboard;

static void
read_clipboard_cb (GObject      *object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
	ReadClipboard *clip = user_data;

	g_assert (GDK_IS_CLIPBOARD (object));
	g_assert (G_IS_ASYNC_RESULT (result));
	g_assert (clip != NULL);
	g_assert (clip->main_loop != NULL);
	g_assert (G_IS_CANCELLABLE (clip->cancellable));

	clip->text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (object), result, NULL);

	g_main_loop_quit (clip->main_loop);
}

static void
read_clipboard (GtkSourceVimRegisters  *self,
                GdkClipboard           *clipboard,
                char                  **text)
{
	ReadClipboard clip;
	GSource *source;

	g_assert (GTK_SOURCE_IS_VIM_REGISTERS (self));
	g_assert (GDK_IS_CLIPBOARD (clipboard));

	clip.text = NULL;
	clip.main_loop = g_main_loop_new (NULL, FALSE);
	clip.cancellable = g_cancellable_new ();

	source = g_timeout_source_new (500);
	g_source_set_name (source, "[gtksourceview cancel clipboard]");
	g_source_set_callback (source, cancel_cb, clip.cancellable, NULL);
	g_source_attach (source, NULL);

	gdk_clipboard_read_text_async (clipboard,
	                               clip.cancellable,
	                               read_clipboard_cb,
	                               &clip);

	g_main_loop_run (clip.main_loop);

	g_main_loop_unref (clip.main_loop);
	g_object_unref (clip.cancellable);

	g_source_destroy (source);

	if (clip.text != NULL)
	{
		g_clear_pointer (text, g_ref_string_release);
		*text = g_ref_string_new (clip.text);
		g_free (clip.text);
	}
}

GtkSourceVimState *
gtk_source_vim_registers_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_REGISTERS, NULL);
}

const char *
gtk_source_vim_registers_get (GtkSourceVimRegisters *self,
                              const char            *name)
{
	GtkSourceView *view;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_REGISTERS (self), NULL);

	if (name == NULL)
	{
		name = DEFAULT_REGISTER;
	}

	if (g_ascii_isdigit (*name))
	{
		return gtk_source_vim_registers_get_numbered (self, *name - '0');
	}

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	if (g_str_equal (name, "+"))
	{
		GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
		read_clipboard (self, clipboard, &g_clipboard);
		return g_clipboard;
	}
	else if (g_str_equal (name, "*"))
	{
		GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (view));
		read_clipboard (self, clipboard, &g_primary_clipboard);
		return g_primary_clipboard;
	}
	else
	{
		return g_hash_table_lookup (g_values, name);
	}
}

static inline char **
get_numbered_pos (GtkSourceVimRegisters *self,
                  guint                  n)
{
	return &g_numbered[(g_numbered_pos + n) % 10];
}

const char *
gtk_source_vim_registers_get_numbered (GtkSourceVimRegisters *self,
                                       guint                  n)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_REGISTERS (self), NULL);
	g_return_val_if_fail (n <= 9, NULL);

	return *get_numbered_pos (self, n);
}

static void
gtk_source_vim_registers_push (GtkSourceVimRegisters *self,
                               char                  *str)
{
	char **pos;

	g_return_if_fail (GTK_SOURCE_IS_VIM_REGISTERS (self));

	if (g_numbered_pos == 0)
	{
		g_numbered_pos = G_N_ELEMENTS (g_numbered) - 1;
	}
	else
	{
		g_numbered_pos--;
	}

	pos = get_numbered_pos (self, 0);

	if (*pos != NULL)
	{
		g_ref_string_release (*pos);
	}

	*pos = str ? g_ref_string_acquire (str) : NULL;
}

void
gtk_source_vim_registers_set (GtkSourceVimRegisters *self,
                              const char            *name,
                              const char            *value)
{
	GtkSourceView *view;
	char *str;

	g_return_if_fail (GTK_SOURCE_IS_VIM_REGISTERS (self));

	if (name == NULL)
	{
		name = DEFAULT_REGISTER;
	}

	/* TODO: Allow :set viminfo to tweak register lines, bytes, etc */
	if (value != NULL && strlen (value) > MAX_BYTES)
	{
		value = NULL;
	}

	if (value == NULL)
	{
		g_hash_table_remove (g_values, name);
		return;
	}

	str = g_ref_string_new (value);
	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self));

	if (g_str_equal (name, "+"))
	{
		GdkClipboard *clipboard = gtk_widget_get_clipboard (GTK_WIDGET (view));
		write_clipboard (self, clipboard, str);
	}
	else if (g_str_equal (name, "*"))
	{
		GdkClipboard *clipboard = gtk_widget_get_primary_clipboard (GTK_WIDGET (view));
		write_clipboard (self, clipboard, str);
	}
	else
	{
		g_hash_table_insert (g_values,
		                     (char *)g_intern_string (name),
		                     str);
	}

	/* Push into the 0 numbered register and each 1..8 to
	 * the next numbered register position.
	 */
	if (g_strcmp0 (name, DEFAULT_REGISTER) == 0)
	{
		gtk_source_vim_registers_push (self, str);
	}
}

void
gtk_source_vim_registers_clear (GtkSourceVimRegisters *self,
                                const char            *name)
{
	gtk_source_vim_registers_set (self, name, NULL);
}

gboolean
gtk_source_vim_register_is_read_only (const char *name)
{
	switch (name ? name[0] : 0)
	{
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
	case '%': case '.': case '#': case ':':
		return TRUE;

	default:
		return FALSE;
	}
}

void
gtk_source_vim_registers_reset (GtkSourceVimRegisters *self)
{
	g_return_if_fail (GTK_SOURCE_IS_VIM_REGISTERS (self));

	g_hash_table_remove_all (g_values);

	/* Clear global state, but this is just for tests anyway */
	g_clear_pointer (&g_clipboard, g_ref_string_release);
	g_clear_pointer (&g_primary_clipboard, g_ref_string_release);

	for (guint i = 0; i < G_N_ELEMENTS (g_numbered); i++)
	{
		g_clear_pointer (&g_numbered[i], g_ref_string_release);
	}

	g_numbered_pos = 0;
}

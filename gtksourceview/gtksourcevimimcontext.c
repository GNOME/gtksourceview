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

#include "gtksourceview.h"
#include "gtksourcevimimcontext-private.h"
#include "gtksource-enumtypes.h"

#include "vim/gtksourcevim.h"
#include "vim/gtksourcevimcommand.h"

/**
 * GtkSourceVimIMContext:
 *
 * Vim emulation.
 *
 * The `GtkSourceVimIMContext` is a [class@Gtk.IMContext] implementation that can
 * be used to provide Vim-like editing controls within a [class@View].
 *
 * The `GtkSourceViMIMContext` will process incoming [class@Gdk.KeyEvent] as the
 * user types. It should be used in conjunction with a [class@Gtk.EventControllerKey].
 *
 * Various features supported by `GtkSourceVimIMContext` include:
 *
 *  - Normal, Insert, Replace, Visual, and Visual Line modes
 *  - Support for an integrated command bar and current command preview
 *  - Search and replace
 *  - Motions and Text Objects
 *  - History replay
 *  - Jumplists within the current file
 *  - Registers including the system and primary clipboards
 *  - Creation and motion to marks
 *  - Some commonly used Vim commands
 *
 * It is recommended that applications display the contents of
 * [property@VimIMContext:command-bar-text] and
 * [property@VimIMContext:command-text] to the user as they represent the
 * command-bar and current command preview found in Vim.
 *
 * `GtkSourceVimIMContext` attempts to work with additional [class@Gtk.IMContext]
 * implementations such as IBus by querying the [class@Gtk.TextView] before processing
 * the command in states which support it (notably Insert and Replace modes).
 *
 * ```c
 * GtkEventController *key;
 * GtkIMContext *im_context;
 * GtkWidget *view;
 *
 * view = gtk_source_view_new ();
 * im_context = gtk_source_vim_im_context_new ();
 * key = gtk_event_controller_key_new ();
 *
 * gtk_event_controller_key_set_im_context (GTK_EVENT_CONTROLLER_KEY (key), im_context);
 * gtk_event_controller_set_propagation_phase (key, GTK_PHASE_CAPTURE);
 * gtk_widget_add_controller (view, key);
 * gtk_im_context_set_client_widget (im_context, view);
 *
 * g_object_bind_property (im_context, "command-bar-text", command_bar_label, "label", 0);
 * g_object_bind_property (im_context, "command-text", command_label, "label", 0);
 * ```
 * ```python
 * key = Gtk.EventControllerKey.new()
 * im_context = GtkSource.VimIMContext.new()
 * buffer = GtkSource.Buffer()
 * view = GtkSource.View.new_with_buffer(buffer)
 *
 * key.set_im_context(im_context)
 * key.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
 * view.add_controller(key)
 * im_context.set_client_widget(view)
 *
 * im_context.bind_property(
 *     source_property="command-text",
 *     target=command_label,
 *     target_property="label",
 *     flags=GObject.BindingFlags.DEFAULT,
 * )
 *
 * im_context.bind_property(
 *     source_property="command-bar-text",
 *     target=command_bar_label,
 *     target_property="label",
 *     flags=GObject.BindingFlags.DEFAULT,
 * )
 * ```
 *
 * Since: 5.4
 */

struct _GtkSourceVimIMContext
{
	GtkIMContext  parent_instance;
	GtkSourceVim *vim;
	GArray       *observers;
	guint         reset_observer : 1;
};

typedef struct
{
	GtkSourceVimIMContextObserver observer;
	gpointer data;
	GDestroyNotify notify;
} Observer;

G_DEFINE_TYPE (GtkSourceVimIMContext, gtk_source_vim_im_context, GTK_TYPE_IM_CONTEXT)

enum {
	PROP_0,
	PROP_COMMAND_BAR_TEXT,
	PROP_COMMAND_TEXT,
	N_PROPS
};

enum {
	EXECUTE_COMMAND,
	FORMAT_TEXT,
	EDIT,
	WRITE,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static void
clear_observer (Observer *o)
{
	if (o->notify)
	{
		o->notify (o->data);
	}
}

GtkIMContext *
gtk_source_vim_im_context_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_VIM_IM_CONTEXT, NULL);
}

static gboolean
gtk_source_vim_im_context_real_execute_command (GtkSourceVimIMContext *self,
                                                const char            *command)
{
	GtkSourceView *view;
	char **parts;
	gboolean ret = FALSE;

	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (command != NULL);

	view = gtk_source_vim_state_get_view (GTK_SOURCE_VIM_STATE (self->vim));
	parts = g_strsplit (command, " ", 2);

	if (parts[1] != NULL)
	{
		g_strstrip (parts[1]);
	}

	if (g_str_equal (command, ":w") ||
	    g_str_equal (command, ":write"))
	{
		g_signal_emit (self, signals[WRITE], 0, view, NULL);
		ret = TRUE;
	}
	else if (g_str_equal (command, ":e") ||
	         g_str_equal (command, ":edit"))
	{
		g_signal_emit (self, signals[EDIT], 0, view, NULL);
		ret = TRUE;
	}
	else if (g_str_has_prefix (command, ":w ") ||
	         g_str_has_prefix (command, ":write "))
	{
		g_signal_emit (self, signals[WRITE], 0, view, parts[1]);
		ret = TRUE;
	}
	else if (g_str_has_prefix (command, ":e ") ||
	         g_str_has_prefix (command, ":edit "))
	{
		g_signal_emit (self, signals[EDIT], 0, view, parts[1]);
		ret = TRUE;
	}

	g_strfreev (parts);

	return ret;
}

static void
on_vim_notify_cb (GtkSourceVimIMContext *self,
                  GParamSpec            *pspec,
                  GtkSourceVim          *vim)
{
	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (GTK_SOURCE_IS_VIM (vim));

	if (g_str_equal (pspec->name, "command-bar-text"))
		pspec = properties[PROP_COMMAND_BAR_TEXT];
	else if (g_str_equal (pspec->name, "command-text"))
		pspec = properties[PROP_COMMAND_TEXT];
	else
		pspec = NULL;

	if (pspec)
		g_object_notify_by_pspec (G_OBJECT (self), pspec);
}

static gboolean
on_vim_execute_command_cb (GtkSourceVimIMContext *self,
                           const char            *command,
                           GtkSourceVim          *vim)
{
	gboolean ret = FALSE;

	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (GTK_SOURCE_IS_VIM (vim));

	g_signal_emit (self, signals[EXECUTE_COMMAND], 0, command, &ret);
	return ret;
}

static void
on_vim_ready_cb (GtkSourceVimIMContext *self,
                 GtkSourceVim          *vim)
{
	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (GTK_SOURCE_IS_VIM (vim));

	self->reset_observer = TRUE;
}

static void
on_vim_format_cb (GtkSourceVimIMContext *self,
                  GtkTextIter           *begin,
                  GtkTextIter           *end,
                  GtkSourceVim          *vim)
{
	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (begin != NULL);
	g_assert (end != NULL);
	g_assert (GTK_SOURCE_IS_VIM (vim));

	g_signal_emit (self, signals [FORMAT_TEXT], 0, begin, end);
}

static void
gtk_source_vim_im_context_set_client_widget (GtkIMContext *context,
                                             GtkWidget    *widget)
{
	GtkSourceVimIMContext *self = (GtkSourceVimIMContext *)context;

	g_return_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_return_if_fail (!widget || GTK_SOURCE_IS_VIEW (widget));

	if (self->vim != NULL)
    {
      g_object_run_dispose (G_OBJECT (self->vim));
      g_clear_object (&self->vim);
    }

  if (widget != NULL)
    {
      self->vim = gtk_source_vim_new (GTK_SOURCE_VIEW (widget));

      g_signal_connect_object (self->vim,
                               "notify",
                               G_CALLBACK (on_vim_notify_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->vim,
                               "execute-command",
                               G_CALLBACK (on_vim_execute_command_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->vim,
                               "format",
                               G_CALLBACK (on_vim_format_cb),
                               self,
                               G_CONNECT_SWAPPED);

      g_signal_connect_object (self->vim,
                               "ready",
                               G_CALLBACK (on_vim_ready_cb),
                               self,
                               G_CONNECT_SWAPPED);
    }

	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_TEXT]);
	g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMMAND_BAR_TEXT]);
}

static void
gtk_source_vim_im_context_reset (GtkIMContext *context)
{
	GtkSourceVimIMContext *self = (GtkSourceVimIMContext *)context;

	g_return_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));

	gtk_source_vim_reset (self->vim);
}

static void
gtk_source_vim_im_context_focus_in (GtkIMContext *context)
{
	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (context));

}

static void
gtk_source_vim_im_context_focus_out (GtkIMContext *context)
{
	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (context));
}

static gboolean
gtk_source_vim_im_context_filter_keypress (GtkIMContext *context,
                                           GdkEvent     *event)
{
	GtkSourceVimIMContext *self = (GtkSourceVimIMContext *)context;

	g_assert (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_assert (gdk_event_get_event_type (event) == GDK_KEY_PRESS ||
	          gdk_event_get_event_type (event) == GDK_KEY_RELEASE);

	if (self->vim == NULL)
	{
		return FALSE;
	}

	if (gdk_event_get_event_type (event) == GDK_KEY_PRESS)
	{
		GdkModifierType mods;
		guint keyval;
		char str[16];

		mods = gdk_event_get_modifier_state (event);
		keyval = gdk_key_event_get_keyval (event);
		gtk_source_vim_state_keyval_to_string (keyval, mods, str);

		for (guint i = 0; i < self->observers->len; i++)
		{
			const Observer *o = &g_array_index (self->observers, Observer, i);

			o->observer (self, str, self->reset_observer, o->data);
		}

		self->reset_observer = FALSE;
	}

	return gtk_source_vim_state_handle_event (GTK_SOURCE_VIM_STATE (self->vim), event);
}

static void
gtk_source_vim_im_context_dispose (GObject *object)
{
	GtkSourceVimIMContext *self = (GtkSourceVimIMContext *)object;

	g_clear_object (&self->vim);
	g_clear_pointer (&self->observers, g_array_unref);

	G_OBJECT_CLASS (gtk_source_vim_im_context_parent_class)->dispose (object);
}

static void
gtk_source_vim_im_context_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
	GtkSourceVimIMContext *self = GTK_SOURCE_VIM_IM_CONTEXT (object);

	switch (prop_id)
	{
	case PROP_COMMAND_TEXT:
		g_value_set_string (value, gtk_source_vim_im_context_get_command_text (self));
		break;

	case PROP_COMMAND_BAR_TEXT:
		g_value_set_string (value, gtk_source_vim_im_context_get_command_bar_text (self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_vim_im_context_class_init (GtkSourceVimIMContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (klass);

	object_class->dispose = gtk_source_vim_im_context_dispose;
	object_class->get_property = gtk_source_vim_im_context_get_property;

	im_context_class->set_client_widget = gtk_source_vim_im_context_set_client_widget;
	im_context_class->reset = gtk_source_vim_im_context_reset;
	im_context_class->focus_in = gtk_source_vim_im_context_focus_in;
	im_context_class->focus_out = gtk_source_vim_im_context_focus_out;
	im_context_class->filter_keypress = gtk_source_vim_im_context_filter_keypress;

	properties [PROP_COMMAND_TEXT] =
		g_param_spec_string ("command-text",
		                     "Command Text",
		                     "The text for the current command",
		                     NULL,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	properties [PROP_COMMAND_BAR_TEXT] =
		g_param_spec_string ("command-bar-text",
		                     "Command Bar Text",
		                     "The text for the command bar",
		                     NULL,
		                     (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceVimIMContext::execute-command:
	 * @self: a #GtkSourceVimIMContext
	 * @command: the command to execute
	 *
	 * The signal is emitted when a command should be
	 * executed. This might be something like `:wq` or `:e <path>`.
	 *
	 * If the application chooses to implement this, it should return
	 * %TRUE from this signal to indicate the command has been handled.
	 *
	 * Returns: %TRUE if handled; otherwise %FALSE.
	 *
	 * Since: 5.4
	 */
	signals[EXECUTE_COMMAND] =
		g_signal_new_class_handler ("execute-command",
		                            G_TYPE_FROM_CLASS (klass),
		                            G_SIGNAL_RUN_LAST,
		                            G_CALLBACK (gtk_source_vim_im_context_real_execute_command),
		                            g_signal_accumulator_true_handled, NULL,
		                            NULL,
		                            G_TYPE_BOOLEAN,
		                            1,
		                            G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * GtkSourceVimIMContext::format-text:
	 * @self: a #GtkSourceVimIMContext
	 * @begin: the start location
	 * @end: the end location
	 *
	 * Requests that the application format the text between
	 * @begin and @end.
	 *
	 * Since: 5.4
	 */
	signals[FORMAT_TEXT] =
		g_signal_new ("format-text",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2,
		              GTK_TYPE_TEXT_ITER,
		              GTK_TYPE_TEXT_ITER);

	/**
	 * GtkSourceVimIMContext::write:
	 * @self: a #GtkSourceVimIMContext
	 * @view: the #GtkSourceView
	 * @path: (nullable): the path if provided, otherwise %NULL
	 *
	 * Requests the application save the file.
	 *
	 * If a filename was provided, it will be available to the signal handler as @path.
	 * This may be executed in relation to the user running the `:write` or `:w` commands.
	 *
	 * Since: 5.4
	 */
	signals[WRITE] =
		g_signal_new ("write",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2,
		              GTK_SOURCE_TYPE_VIEW,
		              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);

	/**
	 * GtkSourceVimIMContext::edit:
	 * @self: a #GtkSourceVimIMContext
	 * @view: the #GtkSourceView
	 * @path: (nullable): the path if provided, otherwise %NULL
	 *
	 * Requests the application open the file found at @path.
	 *
	 * If @path is %NULL, then the current file should be reloaded from storage.
	 *
	 * This may be executed in relation to the user running the
	 * `:edit` or `:e` commands.
	 *
	 * Since: 5.4
	 */
	signals[EDIT] =
		g_signal_new ("edit",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE,
		              2,
		              GTK_SOURCE_TYPE_VIEW,
		              G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gtk_source_vim_im_context_init (GtkSourceVimIMContext *self)
{
	self->observers = g_array_new (FALSE, FALSE, sizeof (Observer));
	g_array_set_clear_func (self->observers, (GDestroyNotify)clear_observer);
}

void
_gtk_source_vim_im_context_add_observer (GtkSourceVimIMContext         *self,
                                         GtkSourceVimIMContextObserver  observer,
                                         gpointer                       data,
                                         GDestroyNotify                 notify)
{
	Observer o;

	g_return_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_return_if_fail (observer != NULL);

	o.observer = observer;
	o.data = data;
	o.notify = notify;

	g_array_append_val (self->observers, o);
}

/**
 * gtk_source_vim_im_context_get_command_text:
 * @self: a #GtkSourceVimIMContext
 *
 * Gets the current command text as it is entered by the user.
 *
 * Returns: (not nullable): A string containing the command text
 *
 * Since: 5.4
 */
const char *
gtk_source_vim_im_context_get_command_text (GtkSourceVimIMContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self), NULL);

	if (self->vim == NULL)
		return NULL;

	return gtk_source_vim_get_command_text (self->vim);
}

/**
 * gtk_source_vim_im_context_get_command_bar_text:
 * @self: a #GtkSourceVimIMContext
 *
 * Gets the current command-bar text as it is entered by the user.
 *
 * Returns: (not nullable): A string containing the command-bar text
 *
 * Since: 5.4
 */
const char *
gtk_source_vim_im_context_get_command_bar_text (GtkSourceVimIMContext *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self), NULL);

	if (self->vim == NULL)
		return NULL;

	return gtk_source_vim_get_command_bar_text (self->vim);
}

/**
 * gtk_source_vim_im_context_execute_command:
 * @self: a #GtkSourceVimIMContext
 * @command: the command text
 *
 * Executes @command as if it was typed into the command bar by the
 * user except that this does not emit the
 * [signal@VimIMContext::execute-command] signal.
 *
 * Since: 5.4
 */
void
gtk_source_vim_im_context_execute_command (GtkSourceVimIMContext *self,
                                           const char            *command)
{
	GtkSourceVimState *normal;
	GtkSourceVimState *parsed;

	g_return_if_fail (GTK_SOURCE_IS_VIM_IM_CONTEXT (self));
	g_return_if_fail (command != NULL);

	if (self->vim == NULL)
		return;

	normal = gtk_source_vim_state_get_child (GTK_SOURCE_VIM_STATE (self->vim));
	if (!(parsed = gtk_source_vim_command_new_parsed (normal, command)))
		return;

	gtk_source_vim_state_set_parent (parsed, normal);
	gtk_source_vim_state_repeat (parsed);
	gtk_source_vim_state_unparent (parsed);
	g_object_unref (parsed);
}

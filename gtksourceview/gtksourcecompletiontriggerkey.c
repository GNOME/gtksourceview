/*
 * gtksourcecompletiontriggerkey.c
 * This file is part of gtksourceview
 *
 * Copyright (C) 2007 -2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gsc-trigger-key
 * @title: GtkSourceCompletionTriggerKey
 * @short_description: Custom keys trigger
 *
 * This object trigger a completion event when the user press the configured
 * keys.
 * 
 */

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <gtksourceview/gtksourcecompletiontriggerkey.h>
#include "gtksourcecompletionutils.h"
#include "gtksourceview-i18n.h"

#define GTK_SOURCE_COMPLETION_TRIGGER_KEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
							GTK_TYPE_SOURCE_COMPLETION_TRIGGER_KEY, \
							GtkSourceCompletionTriggerKeyPrivate))

#define GTK_SOURCE_COMPLETION_TRIGGER_KEY_NAME "Key Trigger"

enum
{
	PROP_0,
	PROP_ENABLE_FILTER
};

struct _GtkSourceCompletionTriggerKeyPrivate
{
	GtkSourceCompletion *completion;
	
	gchar *trigger_name;

	guint key;
	GdkModifierType mod;
	gint line;
	gint line_offset;
	
	gboolean filter;
};

static void	 gtk_source_completion_trigger_key_iface_init	(GtkSourceCompletionTriggerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionTriggerKey,
			 gtk_source_completion_trigger_key,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_TRIGGER,
				 		gtk_source_completion_trigger_key_iface_init))

static gboolean
filter_func (GtkSourceCompletionProposal *proposal,
	     gpointer			  user_data)
{
	const gchar *label;
	const gchar *text;
	
	label = gtk_source_completion_proposal_get_label (proposal);
	text = (const gchar *)user_data;
	
	return g_str_has_prefix (label, text);
}

static gboolean
view_key_press_event_cb (GtkWidget                     *view,
			 GdkEventKey                   *event,
			 GtkSourceCompletionTriggerKey *self)
{
	guint s;
	guint key;
	
	s = event->state & gtk_accelerator_get_default_mod_mask ();
	key = gdk_keyval_to_lower (self->priv->key);
	
	if (s == self->priv->mod && gdk_keyval_to_lower (event->keyval) == key)
	{
		GtkTextBuffer *buffer;
		GtkTextMark *insert;
		GtkTextIter location;
		
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		insert = gtk_text_buffer_get_insert (buffer);
		gtk_text_buffer_get_iter_at_mark (buffer, &location, insert);
		
		self->priv->line = gtk_text_iter_get_line (&location);
		self->priv->line_offset = gtk_text_iter_get_line_offset (&location);
		
		gtk_source_completion_trigger_activate (GTK_SOURCE_COMPLETION_TRIGGER (self));
		
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
delete_range_cb (GtkWidget		       *buffer,
		 GtkTextIter		       *start,
		 GtkTextIter		       *end,
		 GtkSourceCompletionTriggerKey *self)
{
	if (GTK_WIDGET_VISIBLE (self->priv->completion) && self->priv->filter &&
	    gtk_source_completion_get_active_trigger (self->priv->completion) == GTK_SOURCE_COMPLETION_TRIGGER (self))
	{
		if (gtk_text_iter_get_line (start) != self->priv->line ||
		    gtk_text_iter_get_line_offset (start) < self->priv->line_offset)
		{
			gtk_widget_hide (GTK_WIDGET (self->priv->completion));
		}
		else
		{
			/* Filter the current proposals */
			gchar *temp;
			
			temp = gtk_source_completion_utils_get_word (GTK_SOURCE_BUFFER (buffer));
			gtk_source_completion_filter_proposals (self->priv->completion,
								filter_func,
								temp);
			g_free (temp);
		}
	}
	
	return FALSE;
}

static void
insert_text_cb (GtkTextBuffer		      *buffer,
		GtkTextIter		      *location,
		gchar			      *text,
		gint			       len,
		GtkSourceCompletionTriggerKey *self)
{
	/* Raise the event if completion is not visible */
	if (GTK_WIDGET_VISIBLE (self->priv->completion) && self->priv->filter &&
	    gtk_source_completion_get_active_trigger (self->priv->completion) == GTK_SOURCE_COMPLETION_TRIGGER (self))
	{
		if (gtk_source_completion_utils_is_separator (g_utf8_get_char (text)) ||
		    gtk_text_iter_get_line (location) != self->priv->line ||
		    gtk_text_iter_get_line_offset (location) < self->priv->line_offset)
		{
			gtk_widget_hide (GTK_WIDGET (self->priv->completion));
		}
		else
		{
			/* Filter the current proposals */
			gchar *temp;
			
			temp = gtk_source_completion_utils_get_word (GTK_SOURCE_BUFFER (buffer));
			gtk_source_completion_filter_proposals (self->priv->completion,
								filter_func,
								temp);
			g_free (temp);
		}
	}
}

static const gchar *
gtk_source_completion_trigger_key_real_get_name (GtkSourceCompletionTrigger *base)
{
	GtkSourceCompletionTriggerKey *self;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (base);
	
	return self->priv->trigger_name;
}

static void 
gtk_source_completion_trigger_key_init (GtkSourceCompletionTriggerKey *self)
{
	self->priv = GTK_SOURCE_COMPLETION_TRIGGER_KEY_GET_PRIVATE (self);
	
	self->priv->line = 0;
	self->priv->line_offset = 0;
	self->priv->filter = TRUE;
	
	/* Default accelerator <Control>Space */
	gtk_source_completion_trigger_key_set_accelerator (self,
							   GDK_space,
							   GDK_CONTROL_MASK);
}

static void 
gtk_source_completion_trigger_key_finalize (GObject *object)
{
	GtkSourceCompletionTriggerKey *self;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (object);
	
	g_free (self->priv->trigger_name);

	G_OBJECT_CLASS (gtk_source_completion_trigger_key_parent_class)->finalize (object);
}

static void
gtk_source_completion_trigger_key_dispose (GObject *object)
{
	GtkSourceCompletionTriggerKey *self;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (object);
	
	if (self->priv->completion != NULL)
	{
		g_object_unref (self->priv->completion);
		self->priv->completion = NULL;
	}
	
	G_OBJECT_CLASS (gtk_source_completion_trigger_key_parent_class)->dispose (object);
}

static void
gtk_source_completion_trigger_key_get_property (GObject    *object,
						guint       prop_id,
						GValue     *value,
						GParamSpec *pspec)
{
	GtkSourceCompletionTriggerKey *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (object));

	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (object);

	switch (prop_id)
	{
		case PROP_ENABLE_FILTER:
			g_value_set_boolean (value,
					     gtk_source_completion_trigger_key_get_enable_filter (self));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_completion_trigger_key_set_property (GObject      *object,
						guint         prop_id,
						const GValue *value,
						GParamSpec   *pspec)
{
	GtkSourceCompletionTriggerKey *self;

	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (object));

	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (object);

	switch (prop_id)
	{
		case PROP_ENABLE_FILTER:
			gtk_source_completion_trigger_key_set_enable_filter (self,
									     g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void 
gtk_source_completion_trigger_key_class_init (GtkSourceCompletionTriggerKeyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gtk_source_completion_trigger_key_finalize;
	object_class->dispose = gtk_source_completion_trigger_key_dispose;
	object_class->set_property = gtk_source_completion_trigger_key_set_property;
	object_class->get_property = gtk_source_completion_trigger_key_get_property;

	/**
	 * GtkSourceCompletionTriggerKey:enable-filter:
	 *
	 * Whether the proposals must be filtered
	 */
	g_object_class_install_property (object_class,
					 PROP_ENABLE_FILTER,
					 g_param_spec_boolean ("enable-filter",
							       _("Enable filter"),
							       _("Whether the proposals must be filtered"),
							       TRUE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (GtkSourceCompletionTriggerKeyPrivate));
}

static void 
gtk_source_completion_trigger_key_iface_init (GtkSourceCompletionTriggerIface *iface)
{
	iface->get_name = gtk_source_completion_trigger_key_real_get_name;
}

/**
 * gtk_source_completion_trigger_key_new:
 * @completion: The #GtkSourceCompletion
 *
 * This is a generic trigger. This trigger will be triggered when you
 * press <Control>space. See gtk_source_completion_trigger_key_set_accelerator()
 * to change the default accelerator.
 *
 * Returns: a new #GtkSourceCompletionTriggerKey
 *
 */
GtkSourceCompletionTriggerKey * 
gtk_source_completion_trigger_key_new (GtkSourceCompletion *completion,
				       const gchar         *trigger_name)
{
	GtkSourceCompletionTriggerKey *self;
	GtkTextView *view;
	GtkTextBuffer *buffer;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);
	g_return_val_if_fail (trigger_name != NULL, NULL);
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (g_object_new (GTK_TYPE_SOURCE_COMPLETION_TRIGGER_KEY,
								NULL));
	
	self->priv->completion = g_object_ref (completion);
	self->priv->trigger_name = g_strdup (trigger_name);
	
	view = gtk_source_completion_get_view (self->priv->completion);
	buffer = gtk_text_view_get_buffer (view);
	
	g_signal_connect (view,
			  "key-press-event",
			  G_CALLBACK (view_key_press_event_cb),
			  self);
	
	g_signal_connect_after (buffer,
				"delete-range",
				G_CALLBACK (delete_range_cb),
				self);
	
	g_signal_connect_after (buffer,
				"insert-text",
				G_CALLBACK (insert_text_cb),
				self);
	
	return self;
}

/**
 * gtk_source_completion_trigger_key_set_accelerator:
 * @self: The #GtkSourceCompletionTriggerKey 
 * @key: the gdk key value for which the trigger will be activated
 * @modifier: the gdk modifier key which activates the trigger
 *
 * Assign the keys that we will use to activate the user request event.
 */
void
gtk_source_completion_trigger_key_set_accelerator (GtkSourceCompletionTriggerKey *self,
						   guint                          key,
						   GdkModifierType                modifier)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (self));

	self->priv->key = key;
	self->priv->mod = modifier;
}

/**
 * gtk_source_completion_trigger_key_set_enable_filter:
 * @self: The #GtkSourceCompletionTriggerKey
 * @filter: %TRUE if you want to filter the proposals
 *
 * Enables or disables the key filtering.
 */
void
gtk_source_completion_trigger_key_set_enable_filter (GtkSourceCompletionTriggerKey *self,
						     gboolean filter)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (self));
	
	self->priv->filter = filter;
}

/**
 * gtk_source_completion_trigger_key_get_enable_filter:
 * @self: The #GtkSourceCompletionTriggerKey
 *
 * Whether the proposal filter is enabled.
 *
 * Returns: %TRUE if the filter is enabled.
 */
gboolean
gtk_source_completion_trigger_key_get_enable_filter (GtkSourceCompletionTriggerKey *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (self), FALSE);
	
	return self->priv->filter;
}

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

#include <glib/gprintf.h>
#include <string.h>
#include <ctype.h>
#include <gtksourceview/gtksourcecompletiontriggerkey.h>

#define GTK_SOURCE_COMPLETION_TRIGGER_KEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
							GTK_TYPE_SOURCE_COMPLETION_TRIGGER_KEY, \
							GtkSourceCompletionTriggerKeyPrivate))

struct _GtkSourceCompletionTriggerKeyPrivate
{
	GtkSourceCompletion *completion;

	gchar *trigger_name;

	guint key;
	GdkModifierType mod;
};

static void	 gtk_source_completion_trigger_key_iface_init	(GtkSourceCompletionTriggerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionTriggerKey,
			 gtk_source_completion_trigger_key,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_TRIGGER,
				 		gtk_source_completion_trigger_key_iface_init))

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
		gtk_source_completion_trigger_activate (GTK_SOURCE_COMPLETION_TRIGGER (self));
		return TRUE;
	}
	
	return FALSE;
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
	
	self->priv->trigger_name = NULL;
}

static void 
gtk_source_completion_trigger_key_finalize (GObject *object)
{
	GtkSourceCompletionTriggerKey *self;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (object);
	
	g_free (self->priv->trigger_name);
	g_object_unref (self->priv->completion);

	G_OBJECT_CLASS (gtk_source_completion_trigger_key_parent_class)->finalize (object);
}

static void 
gtk_source_completion_trigger_key_class_init (GtkSourceCompletionTriggerKeyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (GtkSourceCompletionTriggerKeyPrivate));

	object_class->finalize = gtk_source_completion_trigger_key_finalize;
}

static void 
gtk_source_completion_trigger_key_iface_init (GtkSourceCompletionTriggerIface *iface)
{
	iface->get_name = gtk_source_completion_trigger_key_real_get_name;
}

/**
 * gtk_source_completion_trigger_key_new:
 * @completion: The #GtkSourceCompletion
 * @trigger_name: The trigger name wich will be user the we trigger the event.
 * @accelerator: The string representation of the keys that we will
 * use to activate the event. You can get this 
 * string with #gtk_accelerator_name
 *
 * This is a generic trigger. You tell the name and the key and the trigger
 * will be triggered when the user press this key (or keys).
 *
 * Returns: a new #GtkSourceCompletionTriggerKey
 *
 */
GtkSourceCompletionTriggerKey * 
gtk_source_completion_trigger_key_new (GtkSourceCompletion *completion,
				       const gchar         *trigger_name,
				       const gchar         *accelerator)
{
	GtkSourceCompletionTriggerKey *self;
	GtkTextView *view;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);
	g_return_val_if_fail (trigger_name != NULL, NULL);
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_KEY (g_object_new (GTK_TYPE_SOURCE_COMPLETION_TRIGGER_KEY,
								NULL));
	
	self->priv->completion = g_object_ref (completion);
	self->priv->trigger_name = g_strdup (trigger_name);

	gtk_source_completion_trigger_key_set_accelerator (self, accelerator);
	
	view = gtk_source_completion_get_view (self->priv->completion);
	
	g_signal_connect (view,
			  "key-press-event",
			  G_CALLBACK (view_key_press_event_cb),
			  self);
	
	return self;
}

/**
 * gtk_source_completion_trigger_key_set_accelerator:
 * @self: The #GtkSourceCompletionTriggerKey 
 * @accelerator: The string representation of the keys that we will
 * use to activate the user request event. You can get this 
 * string with #gtk_accelerator_name
 *
 * Assign the keys that we will use to activate the user request event.
 */
void
gtk_source_completion_trigger_key_set_accelerator (GtkSourceCompletionTriggerKey *self,
						   const gchar                   *accelerator)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_KEY (self));

	gtk_accelerator_parse (accelerator, &self->priv->key, &self->priv->mod);
}



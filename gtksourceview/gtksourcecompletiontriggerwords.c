/*
 * gtksourcecompletiontriggerwords.c
 * This file is part of gtksourcecompletion
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
 * SECTION:gtksourcecompletiontriggerwords
 * @title: GtkSourceCompletionTriggerWords
 * @short_description: Autocompletion words trigger
 *
 * This object trigger a completion event when the user writes a words with 
 * a configured lenght (default 3 characters). You can configure a delay and
 * the completion event will be triggered n millisecons after the user 
 * insertion.
 * 
 */

#include <string.h>
#include "gtksourcecompletionutils.h"
#include <gtksourceview/gtksourcecompletiontriggerwords.h>
#include "gtksourceview-i18n.h"

#define DEFAULT_MIN_LEN (3)
#define DEFAULT_DELAY (200)

#define GTK_SOURCE_COMPLETION_TRIGGER_WORDS_NAME "GtkSourceCompletionTriggerWords"

#define GTK_SOURCE_COMPLETION_TRIGGER_WORDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
								GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS, \
								GtkSourceCompletionTriggerWordsPrivate))

struct _GtkSourceCompletionTriggerWordsPrivate
{
	GtkSourceCompletion* completion;
	GtkTextView *view;
	
	guint source_id;
	guint delay;
	guint min_len;
	
	gint line;
	gint line_offset;
};

enum
{
	PROP_0,
	PROP_MIN_LEN,
	PROP_DELAY
};

static void		gtk_source_completion_trigger_words_iface_init	(GtkSourceCompletionTriggerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionTriggerWords,
			 gtk_source_completion_trigger_words,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_TRIGGER,
				 		gtk_source_completion_trigger_words_iface_init))

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
raise_event (gpointer event)
{
	GtkSourceCompletionTriggerWords *self;
	GtkTextBuffer *buffer;
	GtkTextMark *insert_mark;
	GtkTextIter iter;
	gchar* word;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_WORDS (event);
	buffer = gtk_text_view_get_buffer (self->priv->view);
	insert_mark = gtk_text_buffer_get_insert (buffer);

	/* Check if the user has changed the cursor position.If yes, we don't complete */
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, insert_mark);
	
	if ((gtk_text_iter_get_line (&iter) != self->priv->line) ||
	    (gtk_text_iter_get_line_offset (&iter) != self->priv->line_offset))
	{
		return FALSE;
	}
	
	word = gtk_source_completion_utils_get_word (GTK_SOURCE_BUFFER (buffer));
	
	if (strlen (word) >= self->priv->min_len)
	{
		gtk_source_completion_trigger_activate (GTK_SOURCE_COMPLETION_TRIGGER (self));
	}
	g_free (word);
	
	return FALSE;
}

static gboolean
delete_range_cb (GtkWidget			 *buffer,
		 GtkTextIter			 *start,
		 GtkTextIter			 *end,
		 GtkSourceCompletionTriggerWords *self)
{
	if (GTK_WIDGET_VISIBLE (self->priv->completion) &&
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
insert_text_cb (GtkTextBuffer			*buffer,
		GtkTextIter			*location,
		gchar				*text,
		gint				 len,
		GtkSourceCompletionTriggerWords *self)
{
	/*Raise the event if completion is not visible*/
	if (!GTK_WIDGET_VISIBLE (self->priv->completion))
	{
		/* Prevent "paste" */
		if (len <= 2)
		{
			if (self->priv->source_id != 0)
			{
				/* Stop the event because the user is writing very fast */
				g_source_remove (self->priv->source_id);
				self->priv->source_id = 0;
			}

			self->priv->line = gtk_text_iter_get_line (location);
			self->priv->line_offset = gtk_text_iter_get_line_offset (location);
			/* raise event in 0,5 seconds */
			self->priv->source_id = g_timeout_add (self->priv->delay,
							       raise_event,
							       self);
		}
	}
	else
	{
		/* If completion is visible, filter the content if the trigger si words */
		if (gtk_source_completion_get_active_trigger (self->priv->completion) == GTK_SOURCE_COMPLETION_TRIGGER (self))
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
}

static const gchar * 
gtk_source_completion_trigger_words_real_get_name (GtkSourceCompletionTrigger *self)
{
	return GTK_SOURCE_COMPLETION_TRIGGER_WORDS_NAME;
}

static void 
gtk_source_completion_trigger_words_get_property (GObject *object,
						  guint property_id,
						  GValue *value,
						  GParamSpec *pspec)
{
	GtkSourceCompletionTriggerWords *self;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_WORDS (object));

	self = GTK_SOURCE_COMPLETION_TRIGGER_WORDS (object);

	switch (property_id)
	{
		case PROP_DELAY:
			g_value_set_int (value,
					 self->priv->delay);
			break;
		case PROP_MIN_LEN:
			g_value_set_int (value,
					 self->priv->min_len);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}


static void 
gtk_source_completion_trigger_words_set_property (GObject *object,
						  guint property_id,
						  const GValue *value,
						  GParamSpec *pspec)
{
	GtkSourceCompletionTriggerWords *self;
	
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_WORDS (object));

	self = GTK_SOURCE_COMPLETION_TRIGGER_WORDS (object);

	switch (property_id)
	{
		case PROP_DELAY:
			self->priv->delay = g_value_get_int (value);
			break;
		case PROP_MIN_LEN:
			self->priv->min_len = g_value_get_int (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;
	}
}

static void 
gtk_source_completion_trigger_words_init (GtkSourceCompletionTriggerWords *self)
{
	self->priv = GTK_SOURCE_COMPLETION_TRIGGER_WORDS_GET_PRIVATE (self);
	
	self->priv->source_id = 0;
	self->priv->delay = DEFAULT_DELAY;
	self->priv->min_len = DEFAULT_MIN_LEN;
}

static void 
gtk_source_completion_trigger_words_finalize (GObject *object)
{
	GtkSourceCompletionTriggerWords *self;
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_WORDS (object);

	if (self->priv->source_id != 0)
	{
		g_source_remove (self->priv->source_id);
		self->priv->source_id = 0;
	}

	G_OBJECT_CLASS (gtk_source_completion_trigger_words_parent_class)->finalize (object);
}

static void 
gtk_source_completion_trigger_words_class_init (GtkSourceCompletionTriggerWordsClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GtkSourceCompletionTriggerWordsPrivate));

	object_class->get_property = gtk_source_completion_trigger_words_get_property;
	object_class->set_property = gtk_source_completion_trigger_words_set_property;
	object_class->finalize     = gtk_source_completion_trigger_words_finalize;
	
	/**
	 * GscCompletion:delay:
	 *
	 * Sets the delay between the key pressed and the trigger event
	 */
	g_object_class_install_property (object_class,
					 PROP_DELAY,
					 g_param_spec_int ("delay",
							   _("Delay"),
							   _("Delay between the key pressed and the trigger event"),
							   0,
							   10000,
							   DEFAULT_DELAY,
							   G_PARAM_READWRITE));
	/**
	 * GscCompletion:min-len:
	 *
	 * Sets the minimum word length to be autocompleted
	 */
	g_object_class_install_property (object_class,
					 PROP_MIN_LEN,
					 g_param_spec_int ("min-len",
							   _("Minimum length"),
							   _("Minimum word length to be autocompleted"),
							   0,
							   100,
							   DEFAULT_MIN_LEN,
							   G_PARAM_READWRITE));
}

static void 
gtk_source_completion_trigger_words_iface_init (GtkSourceCompletionTriggerIface * iface)
{
	iface->get_name = gtk_source_completion_trigger_words_real_get_name;
}

/**
 * gtk_source_completion_trigger_words_new:
 * @completion: The #GscCompletion where the triggered will be used
 *
 * Returns: A new #GtkSourceCompletionTriggerWords
 */
GtkSourceCompletionTriggerWords*
gtk_source_completion_trigger_words_new (GtkSourceCompletion *completion)
{
	GtkSourceCompletionTriggerWords *self;
	GtkTextBuffer *buffer;
	
	g_return_val_if_fail (GTK_IS_SOURCE_COMPLETION (completion), NULL);
	
	self = GTK_SOURCE_COMPLETION_TRIGGER_WORDS (g_object_new (GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS,
								  NULL));
	
	self->priv->completion = completion;
	self->priv->view = gtk_source_completion_get_view (completion);
	
	buffer = gtk_text_view_get_buffer (self->priv->view);
	
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
 * gtk_source_completion_trigger_words_set_delay:
 * @self: The #GtkSourceCompletionTriggerWords
 * @delay: milliseconds to delay the autocompletion event.
 *
 * The delay time is the time between the last user key pressed
 * and the instant when the trigger call to the completion. If 
 * delay is 2000 then the user press a key and 2 seconds later
 * this trigger call to the completion if the user don't press
 * another key.
 *
 */
void
gtk_source_completion_trigger_words_set_delay (GtkSourceCompletionTriggerWords* self,
					       guint delay)
{
	g_return_if_fail (GTK_IS_SOURCE_COMPLETION_TRIGGER_WORDS (self));

	self->priv->delay = delay;
}



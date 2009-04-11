/*
 * gtksourcecompletiontriggerwords.h
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

#ifndef __GTK_SOURCE_COMPLETION_TRIGGER_WORDS_H__
#define __GTK_SOURCE_COMPLETION_TRIGGER_WORDS_H__

#include <glib.h>
#include <glib-object.h>
#include <gtksourceview/gtksourcecompletiontrigger.h>
#include <gtksourceview/gtksourcecompletion.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS (gtk_source_completion_trigger_words_get_type ())
#define GTK_SOURCE_COMPLETION_TRIGGER_WORDS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS, GtkSourceCompletionTriggerWords))
#define GTK_SOURCE_COMPLETION_TRIGGER_WORDS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS, GtkSourceCompletionTriggerWordsClass))
#define GTK_IS_SOURCE_COMPLETION_TRIGGER_WORDS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS))
#define GTK_IS_SOURCE_COMPLETION_TRIGGER_WORDS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS))
#define GTK_SOURCE_COMPLETION_TRIGGER_WORDS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_COMPLETION_TRIGGER_WORDS, GtkSourceCompletionTriggerWordsClass))

typedef struct _GtkSourceCompletionTriggerWordsPrivate GtkSourceCompletionTriggerWordsPrivate;

typedef struct _GtkSourceCompletionTriggerWords GtkSourceCompletionTriggerWords;

struct _GtkSourceCompletionTriggerWords
{
	GObject parent;
	
	GtkSourceCompletionTriggerWordsPrivate *priv;
};

typedef struct _GtkSourceCompletionTriggerWordsClass GtkSourceCompletionTriggerWordsClass;

struct _GtkSourceCompletionTriggerWordsClass
{
	GObjectClass parent;
};

GType		 		 gtk_source_completion_trigger_words_get_type	(void);

GtkSourceCompletionTriggerWords	*gtk_source_completion_trigger_words_new	(GtkSourceCompletion *completion);

void				 gtk_source_completion_trigger_words_set_delay	(GtkSourceCompletionTriggerWords* trigger,
										 guint delay);

G_END_DECLS

#endif

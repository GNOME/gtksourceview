/*
 * gtksourcecompletion.h
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
 
#ifndef GTK_SOURCE_COMPLETION_H
#define GTK_SOURCE_COMPLETION_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcecompletionprovider.h>
#include <gtksourceview/gtksourcecompletioninfo.h>

G_BEGIN_DECLS

/*
 * FIXME: And this here?
 */
#define USER_REQUEST_TRIGGER_NAME "user-request"

/*
 * Type checking and casting macros
 */
#define GTK_TYPE_SOURCE_COMPLETION              (gtk_source_completion_get_type())
#define GTK_SOURCE_COMPLETION(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SOURCE_COMPLETION, GtkSourceCompletion))
#define GTK_SOURCE_COMPLETION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SOURCE_COMPLETION, GtkSourceCompletionClass))
#define GTK_IS_SOURCE_COMPLETION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SOURCE_COMPLETION))
#define GTK_IS_SOURCE_COMPLETION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_COMPLETION))
#define GTK_SOURCE_COMPLETION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SOURCE_COMPLETION, GtkSourceCompletionClass))

#define DEFAULT_PAGE "Default"

typedef gboolean (* GtkSourceCompletionFilterFunc) (GtkSourceCompletionProposal *proposal,
						    gpointer     user_data);

typedef struct _GtkSourceCompletionPriv GtkSourceCompletionPriv;
typedef struct _GtkSourceCompletion GtkSourceCompletion;
typedef struct _GtkSourceCompletionClass GtkSourceCompletionClass;

struct _GtkSourceCompletion
{
	GtkWindow parent;

	GtkSourceCompletionPriv *priv;
};

struct _GtkSourceCompletionClass
{
	GtkWindowClass parent_class;

	gboolean (* proposal_selected)(GtkSourceCompletion *completion,
				       GtkSourceCompletionProposal *proposal);
	gboolean (* display_info)     (GtkSourceCompletion *completion,
				       GtkSourceCompletionProposal *proposal);
};

GType		 gtk_source_completion_get_type			(void) G_GNUC_CONST;

GtkSourceCompletion
		*_gtk_source_completion_new			(GtkTextView *view);

GtkTextView	*gtk_source_completion_get_view			(GtkSourceCompletion *self);

GtkSourceCompletionTrigger
		*gtk_source_completion_get_trigger		(GtkSourceCompletion *self,
								 const gchar *trigger_name);

GtkSourceCompletionProvider
		*gtk_source_completion_get_provider		(GtkSourceCompletion *self,
								 const gchar *prov_name);

gboolean	 gtk_source_completion_register_provider	(GtkSourceCompletion *self,
								 GtkSourceCompletionProvider *provider,
								 GtkSourceCompletionTrigger *trigger);

gboolean	 gtk_source_completion_unregister_provider	(GtkSourceCompletion *self,
								 GtkSourceCompletionProvider *provider,
								 GtkSourceCompletionTrigger *trigger);

gboolean	 gtk_source_completion_register_trigger		(GtkSourceCompletion *self,
								 GtkSourceCompletionTrigger *trigger);

gboolean	 gtk_source_completion_unregister_trigger	(GtkSourceCompletion *self,
								 GtkSourceCompletionTrigger *trigger);

GtkSourceCompletionTrigger
		*gtk_source_completion_get_active_trigger	(GtkSourceCompletion *self);

/*gboolean	 gtk_source_completion_trigger_event		(GtkSourceCompletion *self,
								 GtkSourceCompletionTrigger *trigger);*/

void		 gtk_source_completion_finish_completion	(GtkSourceCompletion *self);

void		 gtk_source_completion_filter_proposals		(GtkSourceCompletion *self,
								 GtkSourceCompletionFilterFunc func,
								 gpointer user_data);

void		 gtk_source_completion_set_active 		(GtkSourceCompletion *self,
								 gboolean active);

gboolean	 gtk_source_completion_get_active 		(GtkSourceCompletion *self);

GtkWidget	*gtk_source_completion_get_bottom_bar		(GtkSourceCompletion *self);

GtkSourceCompletionInfo
		*gtk_source_completion_get_info_widget		(GtkSourceCompletion *self);

gint		 gtk_source_completion_get_page_pos		(GtkSourceCompletion *self,
								 const gchar *page_name);

gint		 gtk_source_completion_get_n_pages		(GtkSourceCompletion *self);

void		 gtk_source_completion_set_page_pos		(GtkSourceCompletion *self,
								 const gchar *page_name,
								 gint position);

G_END_DECLS

#endif 

/*
 * gtksourcecompletionproposal.h
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
 
#ifndef _GTK_SOURCE_COMPLETION_PROPOSAL_H
#define _GTK_SOURCE_COMPLETION_PROPOSAL_H

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_COMPLETION_PROPOSAL             (gtk_source_completion_proposal_get_type ())
#define GTK_SOURCE_COMPLETION_PROPOSAL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_COMPLETION_PROPOSAL, GtkSourceCompletionProposal))
#define GTK_SOURCE_COMPLETION_PROPOSAL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_COMPLETION_PROPOSAL, GtkSourceCompletionProposalClass))
#define GTK_IS_SOURCE_COMPLETION_PROPOSAL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_COMPLETION_PROPOSAL))
#define GTK_IS_SOURCE_COMPLETION_PROPOSAL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_COMPLETION_PROPOSAL))
#define GTK_SOURCE_COMPLETION_PROPOSAL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_COMPLETION_PROPOSAL, GtkSourceCompletionProposalClass))

typedef struct _GtkSourceCompletionProposalPrivate GtkSourceCompletionProposalPrivate;
typedef struct _GtkSourceCompletionProposalClass GtkSourceCompletionProposalClass;
typedef struct _GtkSourceCompletionProposal GtkSourceCompletionProposal;

struct _GtkSourceCompletionProposal
{
	GObject parent_instance;
	
	GtkSourceCompletionProposalPrivate *priv;
};

struct _GtkSourceCompletionProposalClass
{
	GObjectClass parent_class;
	
	gboolean     (*apply)    (GtkSourceCompletionProposal *proposal,
				  GtkTextView *view);
	const gchar* (*get_info) (GtkSourceCompletionProposal *proposal);
};

GType			 gtk_source_completion_proposal_get_type	(void) G_GNUC_CONST;

GtkSourceCompletionProposal
			*gtk_source_completion_proposal_new		(const gchar *label,
									 const gchar *info,
									 GdkPixbuf *icon);

const gchar		*gtk_source_completion_proposal_get_label	(GtkSourceCompletionProposal *proposal);

const GdkPixbuf		*gtk_source_completion_proposal_get_icon	(GtkSourceCompletionProposal *proposal);


void			 gtk_source_completion_proposal_set_page_name	(GtkSourceCompletionProposal *proposal,
									 const gchar *page_name);

const gchar		*gtk_source_completion_proposal_get_page_name	(GtkSourceCompletionProposal *proposal);

const gchar		*gtk_source_completion_proposal_get_info	(GtkSourceCompletionProposal *proposal);

void			 gtk_source_completion_proposal_apply		(GtkSourceCompletionProposal *proposal,
									 GtkTextView *view);

G_END_DECLS

#endif

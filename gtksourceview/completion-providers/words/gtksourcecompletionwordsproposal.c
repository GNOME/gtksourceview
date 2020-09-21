/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtksourcecompletionwordsproposal-private.h"

struct _GtkSourceCompletionWordsProposal
{
	GObject parent_instance;
	gchar *word;
	gint use_count;
};

enum
{
	UNUSED,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

static void gtk_source_completion_proposal_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (GtkSourceCompletionWordsProposal,
			 gtk_source_completion_words_proposal,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_COMPLETION_PROPOSAL,
			 			gtk_source_completion_proposal_iface_init))

static gchar *
gtk_source_completion_words_proposal_get_text (GtkSourceCompletionProposal *proposal)
{
	return g_strdup (GTK_SOURCE_COMPLETION_WORDS_PROPOSAL(proposal)->word);
}

static void
gtk_source_completion_proposal_iface_init (gpointer g_iface,
                                           gpointer iface_data)
{
	GtkSourceCompletionProposalInterface *iface = (GtkSourceCompletionProposalInterface *)g_iface;

	/* Interface data getter implementations */
	iface->get_label = gtk_source_completion_words_proposal_get_text;
	iface->get_text = gtk_source_completion_words_proposal_get_text;
}

static void
gtk_source_completion_words_proposal_finalize (GObject *object)
{
	GtkSourceCompletionWordsProposal *proposal;

	proposal = GTK_SOURCE_COMPLETION_WORDS_PROPOSAL (object);
	g_free (proposal->word);

	G_OBJECT_CLASS (gtk_source_completion_words_proposal_parent_class)->finalize (object);
}

static void
gtk_source_completion_words_proposal_class_init (GtkSourceCompletionWordsProposalClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_completion_words_proposal_finalize;

	signals[UNUSED] =
		g_signal_new ("unused",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL, NULL,
		              G_TYPE_NONE, 0);
}

static void
gtk_source_completion_words_proposal_init (GtkSourceCompletionWordsProposal *self)
{
	self->use_count = 1;
}

GtkSourceCompletionWordsProposal *
gtk_source_completion_words_proposal_new (const gchar *word)
{
	GtkSourceCompletionWordsProposal *proposal =
		g_object_new (GTK_SOURCE_TYPE_COMPLETION_WORDS_PROPOSAL, NULL);

	proposal->word = g_strdup (word);
	return proposal;
}

void
gtk_source_completion_words_proposal_use (GtkSourceCompletionWordsProposal *proposal)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_PROPOSAL (proposal));

	g_atomic_int_inc (&proposal->use_count);
}

void
gtk_source_completion_words_proposal_unuse (GtkSourceCompletionWordsProposal *proposal)
{
	g_return_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_PROPOSAL (proposal));

	if (g_atomic_int_dec_and_test (&proposal->use_count))
	{
		g_signal_emit (proposal, signals[UNUSED], 0);
	}
}

const gchar *
gtk_source_completion_words_proposal_get_word (GtkSourceCompletionWordsProposal *proposal)
{
	g_return_val_if_fail (GTK_SOURCE_IS_COMPLETION_WORDS_PROPOSAL (proposal), NULL);
	return proposal->word;
}


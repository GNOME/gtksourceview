/* 
 *  gsc-provider-words.c - Type here a short description of your plugin
 *
 *  Copyright (C) 2008 - perriman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gsc-provider-words.h"
#include <gtksourceview/gtksourcecompletion.h>
#include <gtksourceview/gtksourceview.h>

#include <string.h>

#define POPULATE_BATCH 500
#define PROCESS_BATCH 20
#define MAX_ITEMS 5000

#define GSC_PROVIDER_WORDS_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GSC_TYPE_PROVIDER_WORDS, GscProviderWordsPrivate))

#define REGION_FROM_LIST(list) ((ScanRegion *)list->data)
#define ITEM_FROM_ITER(iter) ((GscProposalWords *)g_sequence_get (iter))

enum
{
	PROP_0,
	PROP_VIEW
};

typedef struct
{
	guint start;
	guint end;
} ScanRegion;

enum
{
	INSERT_TEXT,
	INSERT_TEXT_AFTER,
	DELETE_RANGE,
	DELETE_RANGE_AFTER,
	NUM_SIGNALS
};

struct _GscProviderWordsPrivate
{
	GtkSourceView *view;
	GSequence *proposals;
	GList *lines;
	
	GList *scan_regions;
	guint idle_scan_id;
	
	gchar *word;
	gint word_len;
	guint idle_id;
	GtkSourceCompletionContext *context;
	GSequenceIter *populate_iter;

	GdkPixbuf *icon;
	GtkTextMark *completion_mark;
	guint cancel_id;
	
	guint signals[NUM_SIGNALS];
	
	guint current_insert_line;
	gboolean finalizing;
};

typedef struct
{
	GObject parent;

	gchar *word;
	gint use_count;
} GscProposalWords;

typedef struct
{
	GObjectClass parent_class;
} GscProposalWordsClass;

static void gsc_proposal_words_iface_init (gpointer g_iface, gpointer iface_data);
static void gsc_provider_words_iface_init (GtkSourceCompletionProviderIface *iface);

GType gsc_proposal_words_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GscProviderWords,
			 gsc_provider_words,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROVIDER,
				 		gsc_provider_words_iface_init))

G_DEFINE_TYPE_WITH_CODE (GscProposalWords, 
			 gsc_proposal_words, 
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SOURCE_COMPLETION_PROPOSAL,
			 			gsc_proposal_words_iface_init))

static const gchar *
gsc_proposal_words_get_text_impl (GtkSourceCompletionProposal *proposal)
{
	return ((GscProposalWords *)proposal)->word;
}

static void
gsc_proposal_words_iface_init (gpointer g_iface, 
                           gpointer iface_data)
{
	GtkSourceCompletionProposalIface *iface = (GtkSourceCompletionProposalIface *)g_iface;
	
	/* Interface data getter implementations */
	iface->get_label = gsc_proposal_words_get_text_impl;
	iface->get_text = gsc_proposal_words_get_text_impl;
}

static void
gsc_proposal_words_finalize (GObject *object)
{
	GscProposalWords *item = (GscProposalWords *)object;
	
	g_free (item->word);
}

static void
gsc_proposal_words_class_init (GscProposalWordsClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gsc_proposal_words_finalize;
}

static void
gsc_proposal_words_init (GscProposalWords *proposal)
{

}

static const gchar * 
gsc_provider_words_get_name (GtkSourceCompletionProvider *self)
{
	return "Document Words";
}

static GdkPixbuf * 
gsc_provider_words_get_icon (GtkSourceCompletionProvider *self)
{
	return GSC_PROVIDER_WORDS (self)->priv->icon;
}

static gboolean
gsc_provider_words_match (GtkSourceCompletionProvider *provider,
                         GtkSourceCompletionContext  *context)
{
	return TRUE;
}

static ScanRegion *
scan_region_new (gint start,
                 gint end)
{
	ScanRegion *region = (ScanRegion *)g_slice_new (ScanRegion);
	
	region->start = start;
	region->end = end;
	
	return region;
}

static void
scan_region_free (ScanRegion *region)
{
	g_slice_free (ScanRegion, region);
}

static gboolean
is_word_char (gunichar ch)
{
	return g_unichar_isprint (ch) && 
	       (g_unichar_isalnum (ch) || ch == g_utf8_get_char ("_"));
}

static gboolean
iter_at_word_start (GtkTextIter *iter)
{
	GtkTextIter prev = *iter;
	
	if (!gtk_text_iter_starts_word (iter) ||
	    g_unichar_isdigit (gtk_text_iter_get_char (iter)))
	{
		return FALSE;
	}
	
	if (!gtk_text_iter_is_start (&prev) && 
	    !gtk_text_iter_starts_line (&prev))
	{
		gtk_text_iter_backward_char (&prev);
		
		return !is_word_char (gtk_text_iter_get_char (&prev));
	}
	else
	{
		return TRUE;
	}
}

static gboolean
iter_at_word_end (GtkTextIter *iter)
{
	if (!gtk_text_iter_ends_word (iter) || 
	    g_unichar_isdigit (gtk_text_iter_get_char (iter)))
	{
		return FALSE;
	}
	
	if (!gtk_text_iter_is_end (iter) && 
	    !gtk_text_iter_ends_line (iter))
	{
		return !is_word_char (gtk_text_iter_get_char (iter));
	}
	else
	{
		return TRUE;
	}
}

static gint
compare_two_items (GscProposalWords *a,
                   GscProposalWords *b,
                   gpointer        data)
{
	return strcmp (a->word, b->word);
}

static gint
compare_items (GscProposalWords *a,
               GscProposalWords *b,
               gchar const      *word)
{
	gchar const *m1 = a == NULL ? b->word : a->word;
	
	return strcmp (m1, word);
}

static gboolean
iter_match_prefix (GSequenceIter *iter,
                   gchar const   *word,
                   gint           word_len)
{
	GscProposalWords *item = (GscProposalWords *)g_sequence_get (iter);
	
	return strncmp (item->word, 
	                word, 
	                word_len != -1 ? word_len : strlen (word)) == 0;
}

static GSequenceIter *
find_first_proposal (GscProviderWords *provider,
                     gchar const      *word,
                     gint              word_len)
{
	GSequenceIter *iter;
	GSequenceIter *prev;

	iter = g_sequence_search (provider->priv->proposals,
	                          NULL,
	                          (GCompareDataFunc)compare_items,
	                          (gpointer)word);

	if (iter == NULL)
	{
		return NULL;
	}
	
	/* Test if this position might be after the last match */
	if (!g_sequence_iter_is_begin (iter) && 
	    (g_sequence_iter_is_end (iter) || 
	     !iter_match_prefix (iter, word, word_len)))
	{
		iter = g_sequence_iter_prev (iter);
	
		/* Maybe there is actually nothing in the sequence */
		if (g_sequence_iter_is_end (iter) || 
		    !iter_match_prefix (iter, word, word_len))
		{
			return NULL;
		}
	}
	
	if (g_sequence_iter_is_end (iter))
	{
		return NULL;
	}
	
	/* Go back while it matches */
	while (iter &&
	       (prev = g_sequence_iter_prev (iter)) && 
	       iter_match_prefix (prev, word, word_len))
	{
		iter = prev;
		
		if (g_sequence_iter_is_begin (iter))
		{
			break;
		}
	}
	
	return iter;
}


static GSequenceIter *
find_next_proposal (GSequenceIter *iter,
                    gchar const   *word,
                    gint           word_len)
{
	GscProposalWords *item;
	iter = g_sequence_iter_next (iter);

	if (!iter || g_sequence_iter_is_end (iter))
	{
		return NULL;
	}
	
	item = (GscProposalWords *)g_sequence_get (iter);
	
	return iter_match_prefix (iter, word, word_len) ? iter : NULL;
}

static GSequenceIter *
find_exact_proposal (GscProviderWords *provider,
                     GscProposalWords *proposal)
{
	GSequenceIter *iter;
	gint len = strlen (proposal->word);
	GscProposalWords *other;
	
	iter = find_first_proposal (provider, proposal->word, len);
	
	if (!iter)
	{
		return NULL;
	}
	
	do
	{
		other = (GscProposalWords *)g_sequence_get (iter);
		
		if (proposal == other)
		{
			return iter;
		}
		
		iter = g_sequence_iter_next (iter);
	} while (!g_sequence_iter_is_end (iter) &&
	         strcmp (other->word, proposal->word) == 0);

	return NULL;
}

static GscProposalWords *
add_word (GscProviderWords  *words,
          gint               line,
          gchar const       *word)
{
	GscProposalWords *proposal;
	GSequenceIter *iter;
	
	/* Check if word already exists */
	iter = find_first_proposal (words, word, -1);
	
	if (iter && strcmp ((proposal = ITEM_FROM_ITER (iter))->word, word) == 0)
	{
		/* Already exists, increase the use count */
		++proposal->use_count;

		return proposal;
	}
	
	proposal = g_object_new (gsc_proposal_words_get_type (), NULL);
	proposal->word = g_strdup (word);
	proposal->use_count = 1;
	
	/* Insert proposal into binary tree of words */
	g_sequence_insert_sorted (words->priv->proposals,
				  proposal,
				  (GCompareDataFunc)compare_two_items,
				  NULL);

	return proposal;
}

static GList *
scan_line (GscProviderWords  *provider,
           gint               line)
{
	GtkTextBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GList *ret = NULL;
	
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (provider->priv->view));
	gtk_text_buffer_get_iter_at_line (buffer, &start, line);
	
	while (gtk_text_iter_get_line (&start) == line)
	{
		gchar *word;
		
		/* Forward start to next word start */
		while (!iter_at_word_start (&start) && 
		       !gtk_text_iter_ends_line (&start))
		{
			if (!gtk_text_iter_forward_char (&start))
			{
				break;
			}
		}
		
		if (gtk_text_iter_ends_line (&start))
		{
			break;
		}
		
		end = start;
		
		if (!gtk_text_iter_forward_char (&end))
		{
			break;
		}
		
		/* Forward end to next word end */
		while (!iter_at_word_end (&end))
		{
			if (!gtk_text_iter_forward_char (&end))
			{
				break;
			}
		}
		
		if (gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&start) > 2)
		{
			word = gtk_text_iter_get_text (&start, &end);
			ret = g_list_prepend (ret, 
			                      add_word (provider, line, word));
		}
		
		start = end;
		
		if (!gtk_text_iter_forward_char (&start))
		{
			break;
		}
	}
	
	return g_list_reverse (ret);
}

static void
remove_proposal (GscProposalWords *proposal,
                 GscProviderWords *provider)
{
	GSequenceIter *iter;
	
	if (!g_atomic_int_dec_and_test (&proposal->use_count) || 
	    provider->priv->finalizing)
	{
		return;
	}
	
	iter = find_exact_proposal (provider, proposal);
	
	if (iter != NULL)
	{
		g_sequence_remove (iter);
	}
}

static void
remove_line (GList            *line,
             GscProviderWords *provider)
{
	g_list_foreach (line, (GFunc)remove_proposal, provider);
	g_list_free (line);
}

static gboolean
idle_scan_regions (GscProviderWords *provider)
{
	guint num = 0;
	GList *ptr = provider->priv->lines;
	guint prev = 0;
	gboolean finished;
	
	/* Scan a few lines */
	while (provider->priv->scan_regions)
	{
		ScanRegion *region = REGION_FROM_LIST (provider->priv->scan_regions);

		gint span = (region->end - region->start) + 1;
		gint doscan = MIN(PROCESS_BATCH - num, span);
		gint i;
		
		ptr = g_list_nth (ptr, region->start - prev);
		
		for (i = 0; i < doscan; ++i)
		{
			/* First remove this line */
			remove_line ((GList *)ptr->data, provider);
			
			/* Then scan it which adds words */
			ptr->data = scan_line (provider, region->start + i);

			ptr = g_list_next (ptr);
		}
		
		prev = region->start + doscan;
		
		if (doscan == span)
		{
			/* Simply remove the region */
			scan_region_free (region);

			provider->priv->scan_regions = 
				g_list_delete_link (provider->priv->scan_regions,
				                    provider->priv->scan_regions);
		}
		else
		{
			/* Update the region and break */
			region->start = region->start + doscan;
			break;
		}
		
		num += doscan;
	}

	finished = provider->priv->scan_regions == NULL;
	
	if (finished)
	{
		provider->priv->idle_scan_id = 0;
	}
	
	return !finished;
}

static void
population_finished (GscProviderWords *words)
{
	if (words->priv->idle_id != 0)
	{
		g_source_remove (words->priv->idle_id);
		words->priv->idle_id = 0;
	}
	
	g_free (words->priv->word);
	words->priv->word = NULL;
	
	if (words->priv->context != NULL)
	{
		if (words->priv->cancel_id)
		{
			g_signal_handler_disconnect (words->priv->context, 
			                             words->priv->cancel_id);
			words->priv->cancel_id = 0;
		}
	
		g_object_unref (words->priv->context);
		words->priv->context = NULL;
	}
	
	if (words->priv->scan_regions && words->priv->idle_scan_id == 0)
	{
		words->priv->idle_scan_id = 
			g_idle_add ((GSourceFunc)idle_scan_regions,
			            words);
	}
}

static gchar *
get_word_at_iter (GscProviderWords *words,
                  GtkTextIter      *iter)
{
	GtkTextIter start = *iter;
	gint line = gtk_text_iter_get_line (iter);
	gboolean went_back = TRUE;
	
	if (!gtk_text_iter_backward_char (&start))
	{
		return NULL;
	}

	while (went_back &&
	       line == gtk_text_iter_get_line (&start) && 
	       is_word_char (gtk_text_iter_get_char (&start)))
	{
		went_back = gtk_text_iter_backward_char (&start);
	}
	
	if (went_back)
	{
		gtk_text_iter_forward_char (&start);
	}
	
	if (gtk_text_iter_equal (iter, &start))
	{
		return NULL;
	}
	
	if (words->priv->completion_mark != NULL)
	{
		gtk_text_buffer_move_mark (gtk_text_iter_get_buffer (iter),
		                           words->priv->completion_mark,
		                           &start);
	}
	else
	{
		words->priv->completion_mark = gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (iter),
		                                                            NULL,
		                                                            &start,
		                                                            TRUE);
	}

	return gtk_text_iter_get_text (&start, iter);
}

static gboolean
add_in_idle (GscProviderWords *words)
{
	guint idx = 0;
	GList *ret = NULL;
	gboolean finished;
	
	/* Don't complete empty string (when word == NULL) */
	if (words->priv->word == NULL)
	{
		gtk_source_completion_context_add_proposals (words->priv->context,
	                                                     GTK_SOURCE_COMPLETION_PROVIDER (words),
	                                                     NULL,
	                                                     TRUE);
		population_finished (words);
		return FALSE;
	}
	
	if (words->priv->populate_iter == NULL)
	{
		words->priv->populate_iter = find_first_proposal (words,
		                                                  words->priv->word,
		                                                  words->priv->word_len);
	}

	while (idx < PROCESS_BATCH && words->priv->populate_iter)
	{
		GscProposalWords *proposal = ITEM_FROM_ITER (words->priv->populate_iter);
		
		/* Only add non-exact matches */
		if (strcmp (proposal->word, words->priv->word) != 0)
		{
			ret = g_list_prepend (ret, proposal);
		}

		words->priv->populate_iter = find_next_proposal (words->priv->populate_iter,
		                                                 words->priv->word,
		                                                 words->priv->word_len);
		++idx;
	}
	
	ret = g_list_reverse (ret);
	finished = words->priv->populate_iter == NULL;
	
	gtk_source_completion_context_add_proposals (words->priv->context,
	                                             GTK_SOURCE_COMPLETION_PROVIDER (words),
	                                             ret,
	                                             finished);
	
	if (finished)
	{
		population_finished (words);
	}

	return !finished;
}

static void
gsc_provider_words_populate (GtkSourceCompletionProvider *provider,
                             GtkSourceCompletionContext  *context)
{
	GscProviderWords *words = GSC_PROVIDER_WORDS (provider);
	GtkTextIter iter;
	gchar *word;

	gtk_source_completion_context_get_iter (context, &iter);

	g_free (words->priv->word);
	word = get_word_at_iter (words, &iter);
	
	if (word == NULL)
	{
		gtk_source_completion_context_add_proposals (context,
		                                             provider,
		                                             NULL,
		                                             TRUE);
		return;
	}

	words->priv->cancel_id = 
		g_signal_connect_swapped (context, 
			                  "cancelled", 
			                   G_CALLBACK (population_finished), 
			                   provider);

	words->priv->context = g_object_ref (context);

	words->priv->word = word;
	words->priv->word_len = strlen (word);
	
	/* Do first right now */
	if (add_in_idle (words))
	{
		if (words->priv->idle_scan_id != 0)
		{
			g_source_remove (words->priv->idle_scan_id);
			words->priv->idle_scan_id = 0;
		}

		words->priv->idle_id = g_idle_add ((GSourceFunc)add_in_idle,
		                                   words);
	}	
}

static void
on_insert_text_cb (GtkTextBuffer    *buffer,
                   GtkTextIter      *location,
                   gchar const      *text,
                   gint              len,
                   GscProviderWords *words)
{
	words->priv->current_insert_line = gtk_text_iter_get_line (location);
}

static void
remove_scan_regions (GscProviderWords *words,
                     gint              start,
                     gint              end)
{
	GList *item;
	gint span = end - start + 1;
	
	for (item = words->priv->scan_regions; item; item = g_list_next (item))
	{
		ScanRegion *region = REGION_FROM_LIST (item);
		
		if (region->start >= start && region->end <= end)
		{
			GList *remove = item;
			scan_region_free (region);

			item = g_list_previous (item);
			
			/* Remove whole thing */
			words->priv->scan_regions = 
				g_list_delete_link (words->priv->scan_regions,
				                    remove);
		}
		else if (region->start >= start)
		{
			/* Start in region */
			region->end = start;
		}
		else if (region->end <= end)
		{
			/* End in region */
			region->start = start;
			region->end -= span;
		}
		else if (region->start > start)
		{
			region->start -= span;
			region->end -= span;
		}
	}
}

static void
remove_range (GscProviderWords *words,
              gint              start,
              gint              end)
{
	if (start > end)
	{
		return;
	}
	
	remove_scan_regions (words, start, end);

	GList *line = g_list_nth (words->priv->lines, start);
	
	while (start <= end && line)
	{
		GList *cur = line;
		
		/* Remove proposals */
		remove_line ((GList *)line->data, words);
		line = g_list_next (cur);

		words->priv->lines = g_list_delete_link (words->priv->lines,
		                                         cur);
		++start;
	}
}

static gint
get_line_count (GscProviderWords *words)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (words->priv->view));
	
	return gtk_text_buffer_get_line_count (buffer);
}

static void
add_scan_region (GscProviderWords *provider,
                 gint              start,
                 gint              end)
{
	GList *item;
	GList *merge_start = NULL;
	gint line_count = get_line_count (provider);
	
	if (end >= line_count)
	{
		end = line_count - 1;
	}
	
	if (start > end)
	{
		return;
	}
	
	for (item = provider->priv->scan_regions; item; item = g_list_next (item))
	{
		ScanRegion *region = (ScanRegion *)item->data;
		
		if (merge_start != NULL)
		{
			if (end < region->start)
			{
				break;
			}
		}
		else if (start >= region->start)
		{
			merge_start = item;
		}
	}
	
	if (merge_start == NULL)
	{
		/* Simply prepend */
		provider->priv->scan_regions = 
			g_list_prepend (provider->priv->scan_regions,
			                scan_region_new (start, end));
	}
	else
	{
		GList *insert_at;

		if (end < REGION_FROM_LIST (merge_start)->start)
		{
			/* In this case, merge from next(merge_start) to item */
			merge_start = g_list_next (merge_start);
		}
		
		insert_at = g_list_previous (merge_start);
		
		start = MIN(start, REGION_FROM_LIST (merge_start)->start);
		
		if (item)
		{
			end = MIN(end, REGION_FROM_LIST (item)->end);
		}
		
		while (merge_start != item)
		{
			GList *tmp = merge_start;
			
			merge_start = g_list_next (merge_start);
			scan_region_free (REGION_FROM_LIST (tmp));

			provider->priv->scan_regions = 
				g_list_delete_link (provider->priv->scan_regions,
				                    tmp);
		}
		
		/* Insert new region */
		insert_at = g_list_next (insert_at);
		
		if (insert_at != NULL)
		{
			provider->priv->scan_regions = 
				g_list_insert_before (provider->priv->scan_regions,
				                      insert_at,
				                      scan_region_new (start, end));
		}
		else
		{
			provider->priv->scan_regions = 
					g_list_append (provider->priv->scan_regions,
						       scan_region_new (start, end));
		}
	}
	
	if (provider->priv->idle_scan_id == 0)
	{
		provider->priv->idle_scan_id = 
			g_idle_add ((GSourceFunc)idle_scan_regions,
			            provider);
	}
}

static void
handle_text_inserted (GscProviderWords *words,
                      gint              start,
                      gint              end)
{
	gint pos = start;
	GList *ptr = NULL;
	
	if (start > end)
	{
		ptr = g_list_nth (words->priv->lines,
		                  start + 1);
	}
	
	while (pos < end)
	{
		/* Insert new empty lines */
		if (ptr == NULL)
		{
			words->priv->lines = 
				g_list_append (words->priv->lines,
				               NULL);
		}
		else
		{
			words->priv->lines = 
				g_list_insert_before (words->priv->lines,
					              ptr,
					              NULL);
		}

		++pos;
	}
	
	/* Invalidate new region */
	add_scan_region (words, 
	                 start,
	                 end);
}

static void
on_insert_text_after_cb (GtkTextBuffer    *buffer,
                         GtkTextIter      *location,
                         gchar const      *text,
                         gint              len,
                         GscProviderWords *words)
{
	handle_text_inserted (words,
	                      words->priv->current_insert_line,
	                      gtk_text_iter_get_line (location));
}

static void
on_delete_range_cb (GtkTextBuffer    *buffer,
                    GtkTextIter      *start,
                    GtkTextIter      *end,
                    GscProviderWords *words)
{
	gint start_line = gtk_text_iter_get_line (start);
	gint end_line = gtk_text_iter_get_line (end);
	
	/* Simply remove everything from lines start + 1 to end */
	remove_range (words, start_line + 1, end_line);
}

static void
on_delete_range_after_cb (GtkTextBuffer    *buffer,
                          GtkTextIter      *start,
                          GtkTextIter      *end,
                          GscProviderWords *words)
{
	gint start_line = gtk_text_iter_get_line (start);
	
	/* Add start line to scan regions */
	add_scan_region (words, start_line, start_line);
}

static void 
gsc_provider_words_finalize (GObject *object)
{
	GscProviderWords *provider = GSC_PROVIDER_WORDS (object);

	if (provider->priv->completion_mark)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (provider->priv->completion_mark),
		                             provider->priv->completion_mark);
	}

	provider->priv->finalizing = TRUE;
	g_sequence_free (provider->priv->proposals);

	g_list_foreach (provider->priv->lines, (GFunc)remove_line, provider);
	g_list_free (provider->priv->lines);

	G_OBJECT_CLASS (gsc_provider_words_parent_class)->finalize (object);
}

static void
gsc_provider_words_dispose (GObject *object)
{
	GscProviderWords *provider = GSC_PROVIDER_WORDS (object);
	
	if (provider->priv->view)
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (provider->priv->view));
		gint i;
		
		for (i = 0; i < NUM_SIGNALS; ++i)
		{
			g_signal_handler_disconnect (buffer, provider->priv->signals[i]);
		}
		
		g_object_unref (provider->priv->view);
		provider->priv->view = NULL;
	}
	
	if (provider->priv->idle_scan_id)
	{
		g_source_remove (provider->priv->idle_scan_id);
		provider->priv->idle_scan_id = 0;
	}
	
	g_list_foreach (provider->priv->scan_regions, (GFunc)scan_region_free, NULL);
	g_list_free (provider->priv->scan_regions);
	
	provider->priv->scan_regions = NULL;
	
	population_finished (provider);
}

static void
gsc_provider_words_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	GscProviderWords *self = GSC_PROVIDER_WORDS (object);
	
	switch (prop_id)
	{
		case PROP_VIEW:
			self->priv->view = g_value_dup_object (value);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gsc_provider_words_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GscProviderWords *self = GSC_PROVIDER_WORDS (object);
	
	switch (prop_id)
	{
		case PROP_VIEW:
			g_value_set_object (value, self->priv->view);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
initialize_icon (GscProviderWords *provider)
{
	gint size;
	GtkIconTheme *theme = gtk_icon_theme_get_for_screen (
		gtk_widget_get_screen (GTK_WIDGET (provider->priv->view)));
	
	
	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &size, NULL);
	
	provider->priv->icon = 
		gtk_icon_theme_load_icon (theme, 
		                          GTK_STOCK_FILE,
		                          size,
		                          GTK_ICON_LOOKUP_USE_BUILTIN,
		                          NULL);
}

static void
gsc_provider_words_constructed (GObject *object)
{
	GscProviderWords *words = GSC_PROVIDER_WORDS (object);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (words->priv->view));
	
	words->priv->signals[INSERT_TEXT] =
		g_signal_connect (buffer,
			          "insert-text",
			          G_CALLBACK (on_insert_text_cb),
			          words);

	words->priv->signals[INSERT_TEXT_AFTER] =
		g_signal_connect_after (buffer,
			                "insert-text",
			                G_CALLBACK (on_insert_text_after_cb),
			                words);

	words->priv->signals[DELETE_RANGE] =
		g_signal_connect (buffer,
			          "delete-range",
			          G_CALLBACK (on_delete_range_cb),
			          words);

	words->priv->signals[DELETE_RANGE_AFTER] =
		g_signal_connect_after (buffer,
			                "delete-range",
			                G_CALLBACK (on_delete_range_after_cb),
			                words);

	initialize_icon (words);
	
	handle_text_inserted (words,
	                      0,
	                      gtk_text_buffer_get_line_count (buffer));
}

static void 
gsc_provider_words_class_init (GscProviderWordsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gsc_provider_words_finalize;
	object_class->dispose = gsc_provider_words_dispose;

	object_class->set_property = gsc_provider_words_set_property;
	object_class->get_property = gsc_provider_words_get_property;
	
	object_class->constructed = gsc_provider_words_constructed;
	
	g_object_class_install_property (object_class,
	                                 PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      "View",
	                                                      "The view",
	                                                      GTK_TYPE_SOURCE_VIEW,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	
	g_type_class_add_private (object_class, sizeof(GscProviderWordsPrivate));
}

static gboolean
gsc_provider_words_get_start_iter (GtkSourceCompletionProvider *provider,
                                   GtkSourceCompletionProposal *proposal,
                                   GtkTextIter                 *iter)
{
	GscProviderWords *words = GSC_PROVIDER_WORDS (provider);
	
	if (words->priv->completion_mark == NULL ||
	    gtk_text_mark_get_deleted (words->priv->completion_mark))
	{
		return FALSE;
	}
	
	gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (words->priv->completion_mark),
	                                  iter,
	                                  words->priv->completion_mark);
	return TRUE;
}

static void
gsc_provider_words_iface_init (GtkSourceCompletionProviderIface *iface)
{
	iface->get_name = gsc_provider_words_get_name;
	iface->get_icon = gsc_provider_words_get_icon;

	iface->populate = gsc_provider_words_populate;
	iface->match = gsc_provider_words_match;
	iface->get_start_iter = gsc_provider_words_get_start_iter;
}

static void 
gsc_provider_words_init (GscProviderWords *self)
{	
	self->priv = GSC_PROVIDER_WORDS_GET_PRIVATE (self);
	
	self->priv->proposals = g_sequence_new ((GDestroyNotify)g_object_unref);
}

GscProviderWords *
gsc_provider_words_new (GtkSourceView *view)
{
	return g_object_new (GSC_TYPE_PROVIDER_WORDS, "view", view, NULL);
}

/* vi:ex:ts=8 */

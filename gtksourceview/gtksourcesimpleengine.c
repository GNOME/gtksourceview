/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcesimpleengine.c
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "gtksourceview-i18n.h"
#include "gtksourcesimpleengine.h"
#include "gtksourcebuffer.h"
#include "gtksourceregex.h"
#include "gtktextregion.h"
#include "gtksourcetag.h"
#include "gtksourcetag-private.h"

#define ENABLE_DEBUG
#define ENABLE_PROFILE
/*
#undef ENABLE_DEBUG
#undef ENABLE_PROFILE
*/

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#ifdef ENABLE_PROFILE
#define PROFILE(x) (x)
#else
#define PROFILE(x)
#endif

/* define this to always highlight in an idle handler, and not
 * possibly in the expose method of the view */
#undef LAZIEST_MODE

/* in milliseconds */
#define WORKER_TIME_SLICE                   30
#define INITIAL_WORKER_BATCH                40960
#define MINIMUM_WORKER_BATCH                1024

typedef struct _SyntaxDelimiter      SyntaxDelimiter;
typedef struct _PatternMatch         PatternMatch;

struct _SyntaxDelimiter 
{
	gint                offset;
	gint                depth;
	GtkSyntaxTag       *tag;
};

struct _PatternMatch
{
	GtkPatternTag        *tag;
	GtkSourceBufferMatch  match;
};

struct _GtkSourceSimpleEnginePrivate 
{
	GtkSourceBuffer       *buffer;

	/* signal handlers */
	gulong                 tag_table_changed_handler;
	gulong                 insert_text_handler;
	gulong                 delete_range_handler;
	gulong                 delete_range_handler_after;
	gint                   offsets_deleted;
	gulong                 escape_char_notify_handler;

	/* highlighting "input" */
	GList                 *syntax_items;
	GList                 *pattern_items;
	GtkSourceRegex        *reg_syntax_all;
	gunichar               escape_char;

	/* Region covering the unhighlighted text */
	GtkTextRegion         *refresh_region;

	/* Syntax regions data */
	GArray                *syntax_regions;
	GArray                *old_syntax_regions;
	gint                   worker_last_offset;
	gint                   worker_batch_size;
	guint                  worker_handler;

	/* views highlight requests */
	GtkTextRegion         *highlight_requests;
};



static GtkSourceEngineClass *parent_class = NULL;

static void   gtk_source_simple_engine_class_init          (GtkSourceSimpleEngineClass *klass);
static void   gtk_source_simple_engine_init 	           (GtkSourceSimpleEngine      *engine);
static void   gtk_source_simple_engine_finalize            (GObject                    *object);

static void   gtk_source_simple_engine_attach_buffer       (GtkSourceEngine            *engine,
							    GtkSourceBuffer            *buffer);

static GList *gtk_source_simple_engine_get_syntax_entries  (GtkSourceSimpleEngine      *se);
static GList *gtk_source_simple_engine_get_pattern_entries (GtkSourceSimpleEngine      *se);

static void   build_syntax_regions_table                   (GtkSourceSimpleEngine      *se,
							    const GtkTextIter          *needed_end);
static void   update_syntax_regions                        (GtkSourceSimpleEngine      *se,
							    gint                        start,
							    gint                        delta);
static void   invalidate_syntax_regions                    (GtkSourceSimpleEngine      *se,
							    GtkTextIter                *from,
							    gint                        delta);

static void   highlight_region                             (GtkSourceSimpleEngine      *se,
							    GtkTextIter                *start,
							    GtkTextIter                *end);
static void   refresh_range                                (GtkSourceSimpleEngine      *se,
							    GtkTextIter                *start,
							    GtkTextIter                *end);
static void   ensure_highlighted                           (GtkSourceSimpleEngine      *se,
							    const GtkTextIter          *start,
							    const GtkTextIter          *end);
static void   gtk_source_simple_engine_highlight_region    (GtkSourceEngine            *engine,
							    const GtkTextIter          *start,
							    const GtkTextIter          *end,
							    gboolean                    synchronous);

GType
gtk_source_simple_engine_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceSimpleEngineClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gtk_source_simple_engine_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceSimpleEngine),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_simple_engine_init
		};

		our_type = g_type_register_static (GTK_TYPE_SOURCE_ENGINE,
						   "GtkSourceSimpleEngine",
						   &our_info, 
						   0);
	}
	
	return our_type;
}


/* Class and object lifecycle ------------------------------------------- */

static void
gtk_source_simple_engine_class_init (GtkSourceSimpleEngineClass *klass)
{
	GObjectClass         *object_class;
	GtkSourceEngineClass *engine_class;

	object_class 	= G_OBJECT_CLASS (klass);
	parent_class 	= g_type_class_peek_parent (klass);
	engine_class	= GTK_SOURCE_ENGINE_CLASS (klass);
		
	object_class->finalize	       = gtk_source_simple_engine_finalize;

	engine_class->attach_buffer    = gtk_source_simple_engine_attach_buffer;
	engine_class->highlight_region = gtk_source_simple_engine_highlight_region;
}

static void
gtk_source_simple_engine_init (GtkSourceSimpleEngine *engine)
{
	GtkSourceSimpleEnginePrivate *priv;

	priv = g_new0 (GtkSourceSimpleEnginePrivate, 1);

	engine->priv = priv;

	priv->buffer = NULL;
}

static void
gtk_source_simple_engine_finalize (GObject *object)
{
	GtkSourceSimpleEngine *engine;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (object));

	engine = GTK_SOURCE_SIMPLE_ENGINE (object);
	g_return_if_fail (engine->priv != NULL);

	/* disconnect buffer (if there is one), which destroys almost eveything */
	gtk_source_simple_engine_attach_buffer (GTK_SOURCE_ENGINE (engine), NULL);
	
	g_free (engine->priv);
	engine->priv = NULL;
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Buffer attachment and change tracking functions ------------------------------ */

static void 
insert_text_cb (GtkTextBuffer *buffer,
		GtkTextIter   *iter,
		const gchar   *text,
		gint           len,
		gpointer       data)
{
	gint start_offset, text_length;
	GtkSourceSimpleEngine *se = data;
	GtkTextIter start_iter;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer == GTK_SOURCE_BUFFER (buffer));

	/* we get the iter at the end of the inserted text */
	text_length = g_utf8_strlen (text, len);
	start_iter = *iter;
	gtk_text_iter_backward_chars (iter, text_length);
	start_offset = gtk_text_iter_get_offset (&start_iter);
	
	update_syntax_regions (se, start_offset, text_length);
}

static void
delete_range_cb (GtkTextBuffer *buffer,
		 GtkTextIter   *start,
		 GtkTextIter   *end,
		 gpointer       data)
{
	GtkSourceSimpleEngine *se = data;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer == GTK_SOURCE_BUFFER (buffer));

	gtk_text_iter_order (start, end);
	/* keep deleted offsets in the engine struct so the "after"
	 * handler can use it to invalidate regions */
	se->priv->offsets_deleted = gtk_text_iter_get_offset (start) - 
		gtk_text_iter_get_offset (end);
}

static void
delete_range_after_cb (GtkTextBuffer *buffer,
		       GtkTextIter   *start,
		       GtkTextIter   *end,
		       gpointer       data)
{
	GtkSourceSimpleEngine *se = data;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer == GTK_SOURCE_BUFFER (buffer));

	if (se->priv->offsets_deleted > 0)
	{
		/* we use the stored deleted offsets in the engine to
		 * invalidate regions */
		update_syntax_regions (se, gtk_text_iter_get_offset (start),
				       se->priv->offsets_deleted);
		se->priv->offsets_deleted = 0;
	}
}

static void
get_tags_func (GtkTextTag *tag, gpointer data)
{
        GSList **list = NULL;

	g_return_if_fail (data != NULL);

	list = (GSList **) data;

	if (GTK_IS_SOURCE_TAG (tag))
	{
		*list = g_slist_prepend (*list, tag);
	}
}

static GSList *
get_source_tags_from_buffer (const GtkSourceBuffer *buffer)
{
	GSList *list = NULL;
	GtkTextTagTable *table;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	gtk_text_tag_table_foreach (table, get_tags_func, &list);
	list = g_slist_reverse (list);	
	
	return list;
}

static void
sync_syntax_regex (GtkSourceSimpleEngine *se)
{
	GString *str;
	GList *cur;
	GtkSyntaxTag *tag;

	str = g_string_new ("");
	cur = se->priv->syntax_items;

	if (se->priv->reg_syntax_all)
	{
		gtk_source_regex_destroy (se->priv->reg_syntax_all);
		se->priv->reg_syntax_all = NULL;
	}
	if (cur == NULL)
		return;

	while (cur != NULL) {
		g_return_if_fail (GTK_IS_SYNTAX_TAG (cur->data));

		tag = GTK_SYNTAX_TAG (cur->data);
		g_string_append (str, tag->start);
		
		cur = g_list_next (cur);
		
		if (cur != NULL)
			g_string_append (str, "|");
	}

	se->priv->reg_syntax_all = gtk_source_regex_compile (str->str);

	g_string_free (str, TRUE);
}

static void
sync_with_tag_table (GtkSourceSimpleEngine *se)
{
	GtkTextTagTable *tag_table;
	GtkSourceBuffer *buffer;
	GSList *entries;
	GSList *list;

	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer != NULL);
	buffer = se->priv->buffer;
	
	if (se->priv->syntax_items)
	{
		g_list_free (se->priv->syntax_items);
		se->priv->syntax_items = NULL;
	}

	if (se->priv->pattern_items)
	{
		g_list_free (se->priv->pattern_items);
		se->priv->pattern_items = NULL;
	}

	tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (tag_table != NULL);

	list = entries = get_source_tags_from_buffer (buffer);
	
	while (entries != NULL) 
	{	
		if (GTK_IS_SYNTAX_TAG (entries->data)) 
		{
			se->priv->syntax_items =
				g_list_prepend (se->priv->syntax_items, entries->data);
			
		} 
		else if (GTK_IS_PATTERN_TAG (entries->data)) 
		{
			se->priv->pattern_items =
				g_list_prepend (se->priv->pattern_items, entries->data);
			
		}

		entries = g_slist_next (entries);
	}

	g_slist_free (list);

	se->priv->syntax_items = g_list_reverse (se->priv->syntax_items);
	se->priv->pattern_items = g_list_reverse (se->priv->pattern_items);
	
	sync_syntax_regex (se);

	invalidate_syntax_regions (se, NULL, 0);
}

static void 
tag_table_changed_cb (GtkSourceTagTable     *table,
		      GtkSourceSimpleEngine *se)
{
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (GTK_TEXT_TAG_TABLE (table) ==
			  gtk_text_buffer_get_tag_table (
				  GTK_TEXT_BUFFER (se->priv->buffer)));
	
	/* FIXME: we can probably do this in idle to avoid unnecessary
	 * multiple sync operations */
	sync_with_tag_table (se);
}

static void 
notify_cb (GtkSourceBuffer *buffer,
	   GParamSpec      *pspec,
	   gpointer         user_data)
{
	GtkSourceSimpleEngine *se = user_data;
	gunichar new_escape_char;
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (se->priv->buffer == buffer);

	new_escape_char = gtk_source_buffer_get_escape_char (buffer);
	if (new_escape_char != se->priv->escape_char)
	{
		se->priv->escape_char = new_escape_char;
		/* regenerate syntax tables */
		invalidate_syntax_regions (se, NULL, 0);
	}
}

static void
gtk_source_simple_engine_attach_buffer (GtkSourceEngine *engine,
					GtkSourceBuffer *buffer)
{
	GtkSourceSimpleEngine *se;
	GtkTextTagTable *table;
		
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (engine));
	g_return_if_fail (buffer == NULL || GTK_IS_SOURCE_BUFFER (buffer));
	se = GTK_SOURCE_SIMPLE_ENGINE (engine);
	
	/* detach previous buffer if there is one */
	if (se->priv->buffer)
	{
		if (se->priv->worker_handler)
		{
			g_source_remove (se->priv->worker_handler);
			se->priv->worker_handler = 0;
		}

		gtk_text_region_destroy (se->priv->refresh_region, FALSE);
		gtk_text_region_destroy (se->priv->highlight_requests, FALSE);
		se->priv->refresh_region = NULL;
		se->priv->highlight_requests = NULL;

		g_array_free (se->priv->syntax_regions, TRUE);
		se->priv->syntax_regions = NULL;

		if (se->priv->old_syntax_regions)
		{
			g_array_free (se->priv->old_syntax_regions, TRUE);
			se->priv->old_syntax_regions = NULL;
		}

		g_list_free (se->priv->syntax_items);
		g_list_free (se->priv->pattern_items);
		se->priv->syntax_items = NULL;
		se->priv->pattern_items = NULL;

		if (se->priv->reg_syntax_all)
		{
			gtk_source_regex_destroy (se->priv->reg_syntax_all);
			se->priv->reg_syntax_all = NULL;
		}

		/* disconnect tag table signals */
		if (se->priv->tag_table_changed_handler)
		{
			table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (se->priv->buffer));
			g_signal_handler_disconnect (table, se->priv->tag_table_changed_handler);
			se->priv->tag_table_changed_handler = 0;
		}
		
		g_signal_handler_disconnect (se->priv->buffer, se->priv->insert_text_handler);
		g_signal_handler_disconnect (se->priv->buffer, se->priv->delete_range_handler);
		g_signal_handler_disconnect (se->priv->buffer, se->priv->delete_range_handler_after);
		g_signal_handler_disconnect (se->priv->buffer, se->priv->escape_char_notify_handler);
		
		se->priv->buffer = NULL;
	}

	if (buffer)
	{
		se->priv->buffer = buffer;
		
		/* highlight data */
		se->priv->refresh_region = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
		se->priv->syntax_regions = g_array_new (FALSE, FALSE,
							sizeof (SyntaxDelimiter));
		se->priv->highlight_requests = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
		
		/* initially the buffer is empty so it's entirely analyzed */
		se->priv->worker_last_offset = -1;
		se->priv->worker_batch_size = INITIAL_WORKER_BATCH;

		se->priv->insert_text_handler = g_signal_connect_after (
			buffer, "insert_text", G_CALLBACK (insert_text_cb), se);
		se->priv->delete_range_handler = g_signal_connect (
			buffer, "delete_range", G_CALLBACK (delete_range_cb), se);
		se->priv->delete_range_handler_after = g_signal_connect_after (
			buffer, "delete_range", G_CALLBACK (delete_range_after_cb), se);
		se->priv->escape_char_notify_handler = g_signal_connect (
			buffer, "notify::escape_char", G_CALLBACK (notify_cb), se);

		table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
		if (GTK_IS_SOURCE_TAG_TABLE (table))
		{
			se->priv->tag_table_changed_handler =
				g_signal_connect (table, "changed",
						  G_CALLBACK (tag_table_changed_cb), engine);
		}
		else
		{
			g_warning ("Please use GtkSourceTagTable with GtkSourceBuffer.");
		}

		/* this function starts the syntax table building process */
		sync_with_tag_table (se);
	}
}

static GList *
gtk_source_simple_engine_get_syntax_entries (GtkSourceSimpleEngine *se)
{
	g_return_val_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se), NULL);

	return se->priv->syntax_items;
}

static GList *
gtk_source_simple_engine_get_pattern_entries (GtkSourceSimpleEngine *se)
{
	g_return_val_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se), NULL);

	return se->priv->pattern_items;
}


/* Idle worker code ------------------------------------------------------ */

static gboolean
idle_worker (GtkSourceSimpleEngine *se)
{
	GtkTextIter start_iter, end_iter, last_end_iter;
	GtkSourceBuffer *source_buffer;
	gint i;

	g_assert (se->priv->buffer);
	source_buffer = se->priv->buffer;

	if (se->priv->worker_last_offset >= 0) {
		/* the syntax regions table is incomplete */
		build_syntax_regions_table (se, NULL);
	}

	/* Now we highlight subregions requested by the views */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer), &last_end_iter, 0);
	for (i = 0; i < gtk_text_region_subregions (se->priv->highlight_requests); i++) {
		gtk_text_region_nth_subregion (se->priv->highlight_requests,
					       i, &start_iter, &end_iter);

		if (se->priv->worker_last_offset < 0 ||
		    se->priv->worker_last_offset >= gtk_text_iter_get_offset (&end_iter)) {
			ensure_highlighted (se, 
					    &start_iter, 
					    &end_iter);
			last_end_iter = end_iter;
		} else {
			/* since the subregions are ordered, we are
			 * guaranteed here that all subsequent
			 * subregions will be beyond the already
			 * analyzed text */
			break;
		}
	}
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer), &start_iter, 0);

	if (!gtk_text_iter_equal (&start_iter, &last_end_iter)) {
		/* remove already highlighted subregions from requests */
		gtk_text_region_substract (se->priv->highlight_requests,
					   &start_iter, &last_end_iter);
		gtk_text_region_clear_zero_length_subregions (
			se->priv->highlight_requests);
	}
	
	if (se->priv->worker_last_offset < 0) {
		/* idle handler will be removed */
		se->priv->worker_handler = 0;
		return FALSE;
	}
	
	return TRUE;
}

static void
install_idle_worker (GtkSourceSimpleEngine *se)
{
	if (se->priv->worker_handler == 0) {
		/* use the text view validation priority to get
		 * highlighted text even before complete validation of
		 * the buffer */
		se->priv->worker_handler =
			g_idle_add_full (GTK_TEXT_VIEW_PRIORITY_VALIDATE,
					 (GSourceFunc) idle_worker,
					 se, 
					 NULL);
	}
}


/* Syntax analysis code ---------------------------------------------------- */

static gboolean
is_escaped (GtkSourceSimpleEngine *se, const gchar *text, gint index)
{
	gchar *tmp = (gchar *) text + index;
	gboolean retval = FALSE;

	if (se->priv->escape_char == 0)
		return FALSE;
	
	tmp = g_utf8_find_prev_char (text, tmp);
	while (tmp && g_utf8_get_char (tmp) == se->priv->escape_char) 
	{
		retval = !retval;
		tmp = g_utf8_find_prev_char (text, tmp);
	}
	return retval;
}

static const GtkSyntaxTag * 
get_syntax_start (GtkSourceSimpleEngine *se,
		  const gchar           *text,
		  gint                   length,
		  GtkSourceBufferMatch  *match)
{
	GList *list;
	GtkSyntaxTag *tag;
	gint pos;
	
	if (length == 0)
		return NULL;
	
	list = gtk_source_simple_engine_get_syntax_entries (se);

	if (list == NULL)
		return NULL;

	pos = 0;
	do {
		/* check for any of the syntax highlights */
		pos = gtk_source_regex_search (
			se->priv->reg_syntax_all,
			text,
			pos,
			length,
			match);
		if (pos < 0 || !is_escaped (se, text, match->startindex))
			break;
		pos = match->startpos + 1;
	} while (pos >= 0);

	if (pos < 0)
		return NULL;

	while (list != NULL) {
		tag = list->data;
		
		if (gtk_source_regex_match (tag->reg_start, text,
					    pos, match->endindex))
			return tag;

		list = g_list_next (list);
	}

	return NULL;
}

static gboolean 
get_syntax_end (GtkSourceSimpleEngine *se,
		const gchar           *text,
		gint                   length,
		GtkSyntaxTag          *tag,
		GtkSourceBufferMatch  *match)
{
	GtkSourceBufferMatch tmp;
	gint pos;

	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (length >= 0, FALSE);
	g_return_val_if_fail (tag != NULL, FALSE);

	if (!match)
		match = &tmp;
	
	pos = 0;
	do {
		pos = gtk_source_regex_search (tag->reg_end, text, pos,
					       length, match);
		if (pos < 0 || !is_escaped (se, text, match->startindex))
			break;
		pos = match->startpos + 1;
	} while (pos >= 0);

	return (pos >= 0);
}


/* Syntax regions code ---------------------------------------------------- */

static gint
bsearch_offset (GArray *array, gint offset)
{
	gint i, j, k;
	gint off_tmp;
	
	if (!array || array->len == 0)
		return 0;
	
	i = 0;
	/* border conditions */
	if (g_array_index (array, SyntaxDelimiter, i).offset > offset)
		return 0;
	j = array->len - 1;
	if (g_array_index (array, SyntaxDelimiter, j).offset <= offset)
		return array->len;
	
	while (j - i > 1) {
		k = (i + j) / 2;
		off_tmp = g_array_index (array, SyntaxDelimiter, k).offset;
		if (off_tmp == offset)
			return k + 1;
		else if (off_tmp > offset)
			j = k;
		else
			i = k;
	}
	return j;
}

static void
adjust_table_offsets (GArray *table, gint start, gint delta)
{
	if (!table)
		return;

	while (start < table->len) {
		g_array_index (table, SyntaxDelimiter, start).offset += delta;
		start++;
	}
}
	
static void 
invalidate_syntax_regions (GtkSourceSimpleEngine *se,
			   GtkTextIter           *from,
			   gint                   delta)
{
	GArray *table, *old_table;
	gint region, saved_region;
	gint offset;
	SyntaxDelimiter *delim;
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer);
	
	table = se->priv->syntax_regions;
	g_assert (table != NULL);
	
	if (from) {
		offset = gtk_text_iter_get_offset (from);
	} else {
		offset = 0;
	}

	DEBUG (g_message ("invalidating from %d", offset));
	
	if (!gtk_source_simple_engine_get_syntax_entries (se))
	{
		/* Shortcut case: we don't have syntax entries, so we
		 * won't build the table.  OTOH, we do need to refresh
		 * the highilighting in case there are pattern
		 * entries. */
		GtkTextIter start, end;
		
		g_array_set_size (table, 0);
		se->priv->worker_last_offset = -1;

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (se->priv->buffer), &start, &end);
		if (from)
			start = *from;
		refresh_range (se, &start, &end);

		return;
	}
	
	/* check if the offset has been analyzed already */
	if ((se->priv->worker_last_offset >= 0) &&
	    (offset > se->priv->worker_last_offset))
		/* not yet */
		return;

	region = bsearch_offset (table, offset);
	if (region > 0) {
		delim = &g_array_index (table,
					SyntaxDelimiter,
					region - 1);
		if (delim->tag &&
		    delim->offset == offset) {
			/* take previous region if we are just at the
			   start of a syntax region (i.e. we're
			   invalidating because somebody deleted a
			   opening syntax pattern) */
			region--;
		}
	}
	
	/* if delta is negative, some text was deleted and surely some
	 * syntax delimiters have gone, so we don't need those in the
	 * saved table */
	if (delta < 0) {
		saved_region = bsearch_offset (table, offset - delta);
	} else {
		saved_region = region;
	}

	/* free saved old table */
	if (se->priv->old_syntax_regions) {
		g_array_free (se->priv->old_syntax_regions, TRUE);
		se->priv->old_syntax_regions = NULL;
	}

	/* we don't want to save information if delta is zero,
	 * i.e. the invalidation is not because the user edited the
	 * buffer */
	if (table->len - saved_region > 0 && delta != 0) {
		gint old_table_size;

		DEBUG (g_message ("saving table information"));
				
		/* save table to try to recover still valid information */
		old_table_size = table->len - saved_region;
		old_table = g_array_new (FALSE, FALSE, sizeof (SyntaxDelimiter));
		g_array_set_size (old_table, old_table_size);
		se->priv->old_syntax_regions = old_table;

		/* now copy from r through the end of the table */
		memcpy (&g_array_index (old_table, SyntaxDelimiter, 0),
			&g_array_index (table, SyntaxDelimiter, saved_region),
			sizeof (SyntaxDelimiter) * old_table_size);

		/* adjust saved table offsets */
		adjust_table_offsets (old_table, 0, delta);
	}
	
	/* chop table */
	g_array_set_size (table, region);

	/* update worker_last_offset from the new conditions in the table */
	if (region > 0) {
		se->priv->worker_last_offset =
			g_array_index (table, SyntaxDelimiter, region - 1).offset;
	} else {
		se->priv->worker_last_offset = 0;
	}
	
	install_idle_worker (se);
}

static gboolean
delimiter_is_equal (SyntaxDelimiter *d1, SyntaxDelimiter *d2)
{
	return (d1->offset == d2->offset &&
		d1->depth == d2->depth &&
		d1->tag == d2->tag);
}

/**
 * next_syntax_region:
 * @source_buffer: the GtkSourceBuffer to work on
 * @state: the current SyntaxDelimiter
 * @head: text to analyze
 * @head_length: length in bytes of @head
 * @head_offset: offset in the buffer where @head starts
 * @match: GtkSourceBufferMatch object to get the results
 * 
 * This function can be seen as a single iteration in the analyzing
 * process.  It takes the current @state, searches for the next syntax
 * pattern in head (starting from byte index 0) and if found, updates
 * @state to reflect the new state.  @match is also filled with the
 * matching bounds.
 * 
 * Return value: TRUE if a syntax pattern was found in @head.
 **/
static gboolean 
next_syntax_region (GtkSourceSimpleEngine *se,
		    SyntaxDelimiter       *state,
		    const gchar           *head,
		    gint                   head_length,
		    gint                   head_offset,
		    GtkSourceBufferMatch  *match)
{
	GtkSyntaxTag *tag;
	gboolean found;
	
	if (!state->tag) {
		/* we come from a non-syntax colored region, so seek
		 * for an opening pattern */
		tag = (GtkSyntaxTag *) get_syntax_start (se, head, head_length, match);

		if (!tag)
			return FALSE;
		
		state->tag = tag;
		state->offset = match->startpos + head_offset;
		state->depth = 1;

	} else {
		/* seek the closing pattern for the current syntax
		 * region */
		found = get_syntax_end (se, head, head_length, state->tag, match);
		
		if (!found)
			return FALSE;
		
		state->offset = match->endpos + head_offset;
		state->tag = NULL;
		state->depth = 0;
		
	}
	return TRUE;
}

static void 
build_syntax_regions_table (GtkSourceSimpleEngine *se,
			    const GtkTextIter     *needed_end)
{
	GArray *table;
	GtkTextIter start, end;
	GArray *old_table;
	gint old_region;
	gboolean use_old_data;
	gchar *slice, *head;
	gint offset, head_length;
	GtkSourceBufferMatch match;
	SyntaxDelimiter delim;
	GTimer *timer;

	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer);

	/* we shouldn't have been called if the buffer has no syntax entries */
	g_assert (gtk_source_simple_engine_get_syntax_entries (se) != NULL);
	
	/* check if we still have text to analyze */
	if (se->priv->worker_last_offset < 0)
		return;
	
	/* compute starting iter of the batch */
	offset = se->priv->worker_last_offset;
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
					    &start, offset);
	
	DEBUG (g_message ("restarting syntax regions from %d", offset));
	
	/* compute ending iter of the batch */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
					    &end, offset + se->priv->worker_batch_size);
	
	/* extend the range to include needed_end if necessary */
	if (needed_end && gtk_text_iter_compare (&end, needed_end) < 0)
		end = *needed_end;
	
	/* always stop processing at end of lines: this minimizes the
	 * chance of not getting a syntax pattern because it was split
	 * in between batches */
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);

	table = se->priv->syntax_regions;
	g_assert (table != NULL);
	
	/* get old table information */
	use_old_data = FALSE;
	old_table = se->priv->old_syntax_regions;
	old_region = old_table ? bsearch_offset (old_table, offset) : 0;
	
	/* setup analyzer */
	if (table->len == 0) {
		delim.offset = offset;
		delim.tag = NULL;
		delim.depth = 0;

	} else {
		delim = g_array_index (table, SyntaxDelimiter, table->len - 1);
		g_assert (delim.offset <= offset);
	}

	/* get slice of text to work on */
	slice = gtk_text_iter_get_slice (&start, &end);
	head = slice;
	head_length = strlen (head);

	timer = g_timer_new ();

	/* MAIN LOOP: build the table */
	while (head_length > 0) {
		if (!next_syntax_region (se,
					 &delim,
					 head,
					 head_length,
					 offset,
					 &match)) {
			/* no further data */
			break;
		}

		/* check if we can use the saved table */
		if (old_table && old_region < old_table->len) {
			/* don't fall behind the current match */
			while (old_region < old_table->len &&
			       g_array_index (old_table,
					      SyntaxDelimiter,
					      old_region).offset < delim.offset) {
				old_region++;
			}
			if (old_region < old_table->len &&
			    delimiter_is_equal (&delim,
						&g_array_index (old_table,
								SyntaxDelimiter,
								old_region))) {
				/* we have an exact match; we can use
				 * the saved data */
				use_old_data = TRUE;
				break;
			}
		}

		/* add the delimiter to the table */
		g_array_append_val (table, delim);
			
		/* move pointers */
		head += match.endindex;
		head_length -= match.endindex;
		offset += match.endpos;
	}
    
	g_free (slice);
	g_timer_stop (timer);

	if (use_old_data) {
		/* now we copy the saved information from old_table to
		 * the end of table */
		gint region = table->len;
		gint count = old_table->len - old_region;
		
		DEBUG (g_message ("copying %d delimiters from saved table information", count));

		g_array_set_size (table, table->len + count);
		memcpy (&g_array_index (table, SyntaxDelimiter, region),
			&g_array_index (old_table, SyntaxDelimiter, old_region),
			sizeof (SyntaxDelimiter) * count);
		
		/* set worker_last_offset from the last copied
		 * element, so we can continue to analyze the text in
		 * case the saved table was incomplete */
		region = table->len;
		offset = g_array_index (table, SyntaxDelimiter, region - 1).offset;
		se->priv->worker_last_offset = offset;
		gtk_text_iter_set_offset (&end, offset);
		
	} else {
		/* update worker information */
		se->priv->worker_last_offset =
			gtk_text_iter_is_end (&end) ? -1 : gtk_text_iter_get_offset (&end);
		
		head_length = gtk_text_iter_get_offset (&end) -	gtk_text_iter_get_offset (&start);
		
		if (head_length > 0) {
			/* update profile information only if we didn't use the saved data */
			se->priv->worker_batch_size =
				MAX (head_length * WORKER_TIME_SLICE
				     / (g_timer_elapsed (timer, NULL) * 1000),
				     MINIMUM_WORKER_BATCH);
		}
	}
		
	/* make sure the analyzed region gets highlighted */
	refresh_range (se, &start, &end);
	
	/* forget saved table if we have already "consumed" at least
	 * two of its delimiters, since that probably means it
	 * contains invalid, useless data */
	if (old_table && (use_old_data ||
			  se->priv->worker_last_offset < 0 ||
			  old_region > 1)) {
		g_array_free (old_table, TRUE);
		se->priv->old_syntax_regions = NULL;
	}
	
	PROFILE (g_message ("ended worker batch, %g ms elapsed",
			    g_timer_elapsed (timer, NULL) * 1000));
	DEBUG (g_message ("table has %u entries", table->len));

	g_timer_destroy (timer);
}

static void 
update_syntax_regions (GtkSourceSimpleEngine *se,
		       gint                   start_offset,
		       gint                   delta)
{
	GArray *table;
	gint first_region, region;
	gint table_index, expected_end_index;
	gchar *slice, *head;
	gint head_length, head_offset;
	GtkTextIter start_iter, end_iter;
	GtkSourceBufferMatch match;
	SyntaxDelimiter delim;
	gboolean mismatch;
	
	table = se->priv->syntax_regions;
	g_assert (table != NULL);

	if (!gtk_source_simple_engine_get_syntax_entries (se))
	{
		/* Shortcut case: we don't have syntax entries, so we
		 * just refresh_range() the edited area */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
						    &start_iter, start_offset);
		end_iter = start_iter;
		if (delta > 0)
			gtk_text_iter_forward_chars (&end_iter, delta);
	
		gtk_text_iter_set_line_offset (&start_iter, 0);
		gtk_text_iter_forward_to_line_end (&end_iter);

		refresh_range (se, &start_iter, &end_iter);

		return;
	}
	
	/* check if the offset is at an unanalyzed region */
	if (se->priv->worker_last_offset >= 0 &&
	    start_offset >= se->priv->worker_last_offset) {
		/* update saved table offsets which potentially
		 * contain the offset */
		region = bsearch_offset (se->priv->old_syntax_regions, start_offset);
		adjust_table_offsets (se->priv->old_syntax_regions, region, delta);
		return;
	}
	
	/* we shall start analyzing from the beginning of the line */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
					    &start_iter, start_offset);
	gtk_text_iter_set_line_offset (&start_iter, 0);
	head_offset = gtk_text_iter_get_offset (&start_iter);
	first_region = bsearch_offset (table, head_offset);

	/* initialize analyzing context */
	delim.tag = NULL;
	delim.offset = 0;
	delim.depth = 0;
	/* first expected match */
	table_index = first_region;
	
	/* calculate starting context: delim, head_offset, start_iter and table_index */
	if (first_region > 0) {
		head_offset = g_array_index (table, SyntaxDelimiter, first_region - 1).offset;
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
						    &start_iter,
						    head_offset);
		if (g_array_index (table, SyntaxDelimiter, first_region - 1).tag) {
			/* we are inside a syntax colored region, so
			 * we expect to see the opening delimiter
			 * first */
			table_index = first_region - 1;
		}

		if (table_index > 0) {
			/* set the initial analyzing context to the
			 * delimiter right before the next expected
			 * delimiter */
			delim = g_array_index (table, SyntaxDelimiter, table_index - 1);
		}
		
	} else {
		/* no previous delimiter, so start analyzing at the
		 * start of the buffer */
		head_offset = 0;
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (se->priv->buffer),
						&start_iter);
	}

	/* lookup the edited region */
	region = bsearch_offset (table, start_offset);
	
	/* calculate ending context: expected_end_index and end_iter */
	if (region < table->len) {
		gint end_offset;
		
		/* *corrected* end_offset */
		end_offset = g_array_index (table, SyntaxDelimiter, region).offset + delta;

		/* FIRST INVALIDATION CASE:
		 * the ending delimiter was deleted */
		if (end_offset < start_offset) {
			/* ending delimiter was deleted, so invalidate
			   from the starting delimiter onwards */
			DEBUG (g_message ("deleted ending delimiter"));
			invalidate_syntax_regions (se, &start_iter, delta);
			
			return;
		}

		/* set ending iter */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
						    &end_iter,
						    end_offset);

		/* calculate expected_end_index */
		if (g_array_index (table, SyntaxDelimiter, region).tag)
			expected_end_index = region;
		else
			expected_end_index = MIN (region + 1, table->len);
		
	} else {
		/* set the ending iter to the end of the buffer */
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (se->priv->buffer),
					      &end_iter);
		expected_end_index = table->len;
	}

	/* get us the chunk of text to analyze */
	head = slice = gtk_text_iter_get_slice (&start_iter, &end_iter);
	head_length = strlen (head);

	/* We will start analyzing the slice of text and see if it
	 * matches the information from the table.  When we hit a
	 * mismatch, it means we need to invalidate. */
	mismatch = FALSE;
	while (next_syntax_region (se,
				   &delim,
				   head,
				   head_length,
				   head_offset,
				   &match)) {
		/* correct offset, since the table has the old offsets */
		if (delim.offset > start_offset + delta)
			delim.offset -= delta;

		if (table_index + 1 > table->len ||
		    !delimiter_is_equal (&delim,
					 &g_array_index (table,
							 SyntaxDelimiter,
							 table_index))) {
			/* SECOND INVALIDATION CASE: a mismatch
			   against the saved information or a new
			   delimiter not present in the table */
			mismatch = TRUE;
			break;
		}
		
		/* move pointers */
		head += match.endindex;
		head_length -= match.endindex;
		head_offset += match.endpos;
		table_index++;
	}

	g_free (slice);

	if (mismatch || table_index < expected_end_index) {
		/* we invalidate if there was a mismatch or we didn't
		 * advance the table_index enough (which means some
		 * delimiter was deleted) */
		DEBUG (g_message ("changed delimiter at %d", delim.offset));
		
		invalidate_syntax_regions (se, &start_iter, delta);
		
		return;
	}

	/* no syntax regions changed */
	
	/* update trailing offsets with delta ... */
	adjust_table_offsets (table, region, delta);

	/* ... worker data ... */
	if (se->priv->worker_last_offset >= start_offset + delta)
		se->priv->worker_last_offset += delta;
	
	/* ... and saved table offsets */
	adjust_table_offsets (se->priv->old_syntax_regions, 0, delta);

	/* the syntax regions have not changed, so set the refreshing bounds */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
					    &start_iter, start_offset);
	end_iter = start_iter;
	if (delta > 0)
		gtk_text_iter_forward_chars (&end_iter, delta);
	
	/* adjust bounds to line bounds to correctly highlight
	   non-syntax patterns */
	gtk_text_iter_set_line_offset (&start_iter, 0);
	gtk_text_iter_forward_to_line_end (&end_iter);
	
	refresh_range (se, &start_iter, &end_iter);
}


/* Actual highlighting code --------------------------------------------------- */

/**
 * search_patterns:
 * @matches: the starting list of matches to work from (can be NULL)
 * @text: the text which will be searched for
 * @length: the length (in bytes) of @text
 * @offset: the offset the beginning of @text is at
 * @index: an index to add the match indexes (usually: @text - base_text)
 * @patterns: additional patterns (can be NULL)
 * 
 * This function will fill and return a list of PatternMatch
 * structures ordered by match position in @text.  The initial list to
 * work on is @matches and it will be modified in-place.  Additional
 * new pattern tags might be specified in @patterns.
 *
 * From the patterns already in @matches only those whose starting
 * position is before @offset will be processed, and will be removed
 * if they don't match again.  New patterns will only be added if they
 * match.  The returned list is ordered.
 * 
 * Return value: the new list of matches
 **/
static GList * 
search_patterns (GList       *matches,
		 const gchar *text,
		 gint         length,
		 gint         offset,
		 gint         index,
		 GList       *patterns)
{
	GtkSourceBufferMatch match;
	PatternMatch *pmatch;
	GList *new_pattern;
	
	new_pattern = patterns;
	while (new_pattern || matches) {
		GtkPatternTag *tag;
		gint i;
		
		if (new_pattern) {
			/* process new patterns first */
			tag = new_pattern->data;
			new_pattern = new_pattern->next;
			pmatch = NULL;
		} else {
			/* process patterns already in @matches */
			pmatch = matches->data;
			tag = pmatch->tag;
			if (pmatch->match.startpos >= offset) {
				/* pattern is ahead of offset, so our
				 * work is done */
				break;
			}
			/* temporarily remove the PatternMatch from
			 * the list */
			matches = g_list_delete_link (matches, matches);
		}
		
		/* do the regex search on @text */
		i = gtk_source_regex_search (tag->reg_pattern,
					     text,
					     0,
					     length,
					     &match);
		
		if (i >= 0 && match.endpos != i) {
			GList *p;
			
			/* create the match structure */
			if (!pmatch) {
				pmatch = g_new0 (PatternMatch, 1);
				pmatch->tag = tag;
			}
			/* adjust offsets (indexes remain relative to
			 * the current pointer in the text) */
			pmatch->match.startpos = match.startpos + offset;
			pmatch->match.endpos = match.endpos + offset;
			pmatch->match.startindex = match.startindex + index;
			pmatch->match.endindex = match.endindex + index;
			
			/* insert the match in order (prioritize longest match) */
			for (p = matches; p; p = p->next) {
				PatternMatch *tmp = p->data;
				if (tmp->match.startpos > pmatch->match.startpos ||
				    (tmp->match.startpos == pmatch->match.startpos &&
				     tmp->match.endpos < pmatch->match.endpos)) {
					break;
				}
			}
			matches = g_list_insert_before (matches, p, pmatch);

		} else if (pmatch) {
			/* either no match was found or the match has
			 * zero length (which probably indicates a
			 * buggy syntax pattern), so free the
			 * PatternMatch structure if we were analyzing
			 * a pattern from @matches */
			if (i >= 0 && i == match.endpos) {
				gchar *name;
				g_object_get (G_OBJECT (tag), "name", &name, NULL);
				g_warning ("The regex for pattern tag `%s' matched "
					   "a zero length string.  That's probably "
					   "due to a buggy regular expression.", name);
				g_free (name);
			}
			g_free (pmatch);
		}
	}

	return matches;
}

static void 
check_pattern (GtkSourceSimpleEngine *se,
	       GtkTextIter           *start,
	       const gchar           *text,
	       gint                   length)
{
	GList *matches;
	gint offset, index;
	GtkTextIter start_iter, end_iter;
	const gchar *ptr;

#ifdef ENABLE_PROFILE
	static GTimer *timer = NULL;
	static gdouble seconds = 0.0;
	static gint acc_length = 0;
#endif
	
	if (length == 0 || !gtk_source_simple_engine_get_pattern_entries (se))
		return;
	
	PROFILE ({
		if (timer == NULL)
			timer = g_timer_new ();
		acc_length += length;
		g_timer_start (timer);
	});
	
	/* setup environment */
	index = 0;
	offset = gtk_text_iter_get_offset (start);
	start_iter = end_iter = *start;
	ptr = text;
	
	/* get the initial list of matches */
	matches = search_patterns (NULL,
				   ptr, length,
				   offset, index,
				   gtk_source_simple_engine_get_pattern_entries (se));
	
	while (matches && length > 0) {
		/* pick the first (nearest) match... */
		PatternMatch *pmatch = matches->data;
		
		gtk_text_iter_set_offset (&start_iter,
					  pmatch->match.startpos);
		gtk_text_iter_set_offset (&end_iter,
					  pmatch->match.endpos);

		/* ... and apply it */
		gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (se->priv->buffer),
					   GTK_TEXT_TAG (pmatch->tag),
					   &start_iter,
					   &end_iter);

		/* now skip it completely */
		offset = pmatch->match.endpos;
		index = pmatch->match.endindex;
		length -= (text + index) - ptr;
		ptr = text + index;

		/* and update matches from the new position */
		matches = search_patterns (matches,
					   ptr, length,
					   offset, index,
					   NULL);
	}

	if (matches) {
		/* matches should have been consumed completely */
		g_assert_not_reached ();
	}

	PROFILE ({
		g_timer_stop (timer);
		seconds += g_timer_elapsed (timer, NULL);
		g_message ("%g bytes/sec", acc_length / seconds);
	});
}

static void 
highlight_region (GtkSourceSimpleEngine *se,
		  GtkTextIter           *start,
		  GtkTextIter           *end)
{
	GtkTextIter b_iter, e_iter;
	gint b_off, e_off, end_offset;
	GtkSyntaxTag *current_tag;
	SyntaxDelimiter *delim;
	GArray *table;
	gint region;
	gchar *slice, *slice_ptr;
	GTimer *timer;

	timer = NULL;
	PROFILE ({
 		timer = g_timer_new ();
 		g_message ("highlighting from %d to %d",
 			   gtk_text_iter_get_offset (start),
 			   gtk_text_iter_get_offset (end));
 	});

	table = se->priv->syntax_regions;
	g_return_if_fail (table != NULL);
	
	/* remove_all_tags is not efficient: for different positions
	   in the buffer it takes different times to complete, taking
	   longer if the slice is at the beginning */
	gtk_source_buffer_remove_all_source_tags (se->priv->buffer, start, end);

	slice_ptr = slice = gtk_text_iter_get_slice (start, end);
	end_offset = gtk_text_iter_get_offset (end);
	
	/* get starting syntax region */
	b_off = gtk_text_iter_get_offset (start);
	region = bsearch_offset (table, b_off);
	delim = region > 0 && region <= table->len ?
		&g_array_index (table, SyntaxDelimiter, region - 1) :
		NULL;

	e_iter = *start;
	e_off = b_off;

	do {
		/* select region to work on */
		b_iter = e_iter;
		b_off = e_off;
		current_tag = delim ? delim->tag : NULL;
		region++;
		delim = region <= table->len ?
			&g_array_index (table, SyntaxDelimiter, region - 1) :
			NULL;

		if (delim)
			e_off = MIN (delim->offset, end_offset);
		else
			e_off = end_offset;
		gtk_text_iter_forward_chars (&e_iter, (e_off - b_off));

		/* do the highlighting for the selected region */
		if (current_tag) {
			/* apply syntax tag from b_iter to e_iter */
			gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (se->priv->buffer),
						   GTK_TEXT_TAG (current_tag),
						   &b_iter,
						   &e_iter);

			slice_ptr = g_utf8_offset_to_pointer (slice_ptr,
							      e_off - b_off);
		
		} else {
			gchar *tmp;
			
			/* highlight from b_iter through e_iter using
			   non-syntax patterns */
			tmp = g_utf8_offset_to_pointer (slice_ptr,
							e_off - b_off);
			check_pattern (se, &b_iter, slice_ptr, tmp - slice_ptr);
			slice_ptr = tmp;
		}

	} while (gtk_text_iter_compare (&b_iter, end) < 0);

	g_free (slice);

 	PROFILE ({
		g_message ("highlighting took %g ms",
			   g_timer_elapsed (timer, NULL) * 1000);
		g_timer_destroy (timer);
	});
}

static void 
refresh_range (GtkSourceSimpleEngine *se,
	       GtkTextIter           *start,
	       GtkTextIter           *end)
{
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));

	/* Add the region to the refresh region */
	gtk_text_region_add (se->priv->refresh_region, start, end);

	/* Notify views of the updated highlight region */
	/* FIXME: should we emit this here? or have an engine signal
	 * to notify the buffer, which in turn would notify the
	 * views? */
	g_signal_emit_by_name (se->priv->buffer, "highlight_updated", start, end);
}

static void 
ensure_highlighted (GtkSourceSimpleEngine *se,
		    const GtkTextIter     *start,
		    const GtkTextIter     *end)
{
	GtkTextRegion *region;

	/* Assumes the entire region to highlight has already been syntax analyzed */
	
	/* get the subregions not yet highlighted */
	region = gtk_text_region_intersect (se->priv->refresh_region, start, end);
	if (region) {
		GtkTextIter iter1, iter2;
		gint i;
		
		/* highlight all subregions from the intersection.
                   hopefully this will only be one subregion */
		for (i = 0; i < gtk_text_region_subregions (region); i++) {
			gtk_text_region_nth_subregion (region, i,
						       &iter1, &iter2);
			highlight_region (se, &iter1, &iter2);
		}
		gtk_text_region_destroy (region, TRUE);
		/* remove the just highlighted region */
		gtk_text_region_substract (se->priv->refresh_region, start, end);
		gtk_text_region_clear_zero_length_subregions (se->priv->refresh_region);
	}
}

static void 
highlight_queue (GtkSourceSimpleEngine *se,
		 const GtkTextIter     *start,
		 const GtkTextIter     *end)
{
	gtk_text_region_add (se->priv->highlight_requests, start, end);

	DEBUG (g_message ("queueing highlight [%d, %d]",
			  gtk_text_iter_get_offset (start),
			  gtk_text_iter_get_offset (end)));
}

static void
gtk_source_simple_engine_highlight_region (GtkSourceEngine   *engine,
					   const GtkTextIter *start,
					   const GtkTextIter *end,
					   gboolean           synchronous)
{
	GtkSourceSimpleEngine *se;
	
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (engine));
	g_return_if_fail (start != NULL);
       	g_return_if_fail (end != NULL);

	se = GTK_SOURCE_SIMPLE_ENGINE (engine);

	if (se->priv->worker_last_offset < 0 ||
	    se->priv->worker_last_offset >= gtk_text_iter_get_offset (end))
	{
		ensure_highlighted (se, start, end);
	}
	else
	{
		if (synchronous)
		{
			build_syntax_regions_table (se, end);
			ensure_highlighted (se, start, end);
		}
		else
		{
			highlight_queue (se, start, end);
			install_idle_worker (se);
		}
	}
}

/* Public API ------------------------------------------------------------ */

/**
 * gtk_source_simple_engine_new:
 * 
 * Return value: a new simple highlighting engine
 **/
GtkSourceEngine *
gtk_source_simple_engine_new ()
{
	GtkSourceEngine *engine;

	engine = GTK_SOURCE_ENGINE (g_object_new (GTK_TYPE_SOURCE_SIMPLE_ENGINE, 
						  NULL));
	
	return engine;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcesimpleengine.c
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *  Copyright (C) 2005, 2006 - Marco Barisione, Emanuele Aina
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
#include "gtksourcetag.h"
#include "gtksourcebuffer.h"
#include "gtksourcesimpleengine.h"
#include "gtktextregion.h"
#include "libegg/regex/eggregex.h"

/*
#define ENABLE_DEBUG
#define ENABLE_PROFILE
*/
#undef ENABLE_DEBUG
#undef ENABLE_PROFILE

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

/* struct for holding patterns */

typedef struct _Pattern       Pattern;
typedef struct _SimplePattern SimplePattern;
typedef struct _SyntaxPattern SyntaxPattern;

struct _Pattern
{
	gchar          *id;
	gchar          *style;
	GtkSourceTag   *tag;
};

struct _SimplePattern
{
	Pattern         p;
	EggRegex       *reg_pattern;
};

struct _SyntaxPattern
{
	Pattern         p;
	
	gchar          *start_pattern;
	gchar          *end_pattern;
	EggRegex       *reg_start;
	EggRegex       *reg_end;
};

/* define this to always highlight in an idle handler, and not
 * possibly in the expose method of the view */
#undef LAZIEST_MODE

/* in milliseconds */
#define WORKER_TIME_SLICE                   30
#define INITIAL_WORKER_BATCH                40960
#define MINIMUM_WORKER_BATCH                1024

typedef struct _RegexMatch           RegexMatch;
typedef struct _SyntaxDelimiter      SyntaxDelimiter;
typedef struct _PatternMatch         PatternMatch;

struct _RegexMatch
{
	gint start;
	gint end;
};

struct _SyntaxDelimiter 
{
	gint                 offset;
	gint                 depth;
	const SyntaxPattern *pattern;
};

struct _PatternMatch
{
	const SimplePattern  *pattern;
	RegexMatch            match;
};

struct _GtkSourceSimpleEnginePrivate 
{
	GtkSourceBuffer       *buffer;

	/* whether or not to actually highlight the buffer */
	gboolean               highlight;
	
	/* highlighting "input" */
	GList                 *syntax_items;
	GList                 *pattern_items;
	EggRegex              *reg_syntax_all;
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

static void   simple_pattern_destroy                       (SimplePattern              *pat);
static void   syntax_pattern_destroy                       (SyntaxPattern              *pat);

static void   gtk_source_simple_engine_attach_buffer       (GtkSourceEngine            *engine,
							    GtkSourceBuffer            *buffer);

static GList *gtk_source_simple_engine_get_syntax_entries  (GtkSourceSimpleEngine      *se);
static GList *gtk_source_simple_engine_get_pattern_entries (GtkSourceSimpleEngine      *se);

static void   install_idle_worker                          (GtkSourceSimpleEngine      *se);

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
static void   invalidate_highlight                         (GtkSourceSimpleEngine      *se,
							    gboolean                    highlight);
static void   ensure_highlighted                           (GtkSourceSimpleEngine      *se,
							    const GtkTextIter          *start,
							    const GtkTextIter          *end);
static void   highlight_queue                              (GtkSourceSimpleEngine      *se,
							    const GtkTextIter          *start,
							    const GtkTextIter          *end);


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
	
	/* destroy patterns */
	g_list_foreach (engine->priv->pattern_items, (GFunc) simple_pattern_destroy, NULL);
	g_list_foreach (engine->priv->syntax_items, (GFunc) syntax_pattern_destroy, NULL);
	egg_regex_free (engine->priv->reg_syntax_all);
	
	g_free (engine->priv);
	engine->priv = NULL;
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Regex handling ------------------------------------------------------------------- */

static EggRegex *
regex_new (const gchar *pattern)
{
	return egg_regex_new (pattern, EGG_REGEX_MULTILINE, 0, NULL);
}

static gint
regex_match (EggRegex		*regex,
	     const gchar	*string,
	     gssize		 string_len,
	     gint		 start_position,
	     EggRegexMatchFlags  match_options,
	     RegexMatch		*match)
{
	RegexMatch tmp_match = {-1, -1};
	if (egg_regex_match_extended (regex, string, string_len,
				start_position, match_options, NULL))
	{
		egg_regex_fetch_pos (regex, string, 0,
				&tmp_match.start, &tmp_match.end);
		if (match != NULL)
			*match = tmp_match;
	}
	return tmp_match.start;
}


/* Pattern structures handling ------------------------------------------------------ */

static SimplePattern *
simple_pattern_new (const gchar *id,
		    const gchar *style,
		    const gchar *pattern)
{
	SimplePattern *pat;
	EggRegex *regex;
	
	/* try to compile pattern */
	regex = regex_new (pattern);
	if (regex)
	{
		pat = g_new0 (SimplePattern, 1);

		pat->p.id = g_strdup (id);
		pat->p.style = g_strdup (style);
		pat->reg_pattern = regex;
	}
	else
		pat = NULL;

	return pat;
}

static void
simple_pattern_destroy (SimplePattern *pat)
{
	if (pat)
	{
		g_free (pat->p.id);
		g_free (pat->p.style);
		if (pat->p.tag)
			g_object_unref (pat->p.tag);
		egg_regex_free (pat->reg_pattern);

		g_free (pat);
	}
}

static SyntaxPattern *
syntax_pattern_new (const gchar *id,
		    const gchar *style,
		    const gchar *start_pattern,
		    const gchar *end_pattern)
{
	SyntaxPattern *sp;
	EggRegex *rs, *re;
	
	/* try to compile pattern */
	rs = regex_new (start_pattern);
	re = regex_new (end_pattern);
	if (rs != NULL && re != NULL)
	{
		sp = g_new0 (SyntaxPattern, 1);

		sp->p.id = g_strdup (id);
		sp->p.style = g_strdup (style);
		sp->start_pattern = g_strdup (start_pattern);
		sp->end_pattern = g_strdup (end_pattern);
		sp->reg_start = rs;
		sp->reg_end = re;
	}
	else
	{
		egg_regex_free (rs);
		egg_regex_free (re);
		sp = NULL;
	}
	
	return sp;
}

static void
syntax_pattern_destroy (SyntaxPattern *sp)
{
	if (sp)
	{
		g_free (sp->p.id);
		g_free (sp->p.style);
		if (sp->p.tag)
			g_object_unref (sp->p.tag);
		g_free (sp->start_pattern);
		g_free (sp->end_pattern);
		egg_regex_free (sp->reg_start);
		egg_regex_free (sp->reg_end);

		g_free (sp);
	}
}

static GList *
find_pattern (GList *patterns, const gchar *id)
{
	GList *r;
	for (r = patterns; r; r = r->next)
	{
		Pattern *pat = r->data;
		if (!strcmp (pat->id, id))
			break;
	}
	return r;
}

/* Buffer attachment and change tracking functions ------------------------------ */

static void 
text_inserted_cb (GtkSourceBuffer   *buffer,
		  const GtkTextIter *start,
		  const GtkTextIter *end,
		  gpointer           data)
{
	gint start_offset, text_length;
	GtkSourceSimpleEngine *se = data;

	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer == buffer);

	/* we get the iter at the end of the inserted text */
	start_offset = gtk_text_iter_get_offset (start);
	text_length = gtk_text_iter_get_offset (end) - start_offset;
	
	update_syntax_regions (se, start_offset, text_length);
}

static void 
text_deleted_cb (GtkSourceBuffer   *buffer,
		 const GtkTextIter *iter,
		 const gchar       *text,
		 gpointer           data)
{
	GtkSourceSimpleEngine *se = data;
	
	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer == buffer);

	/* we use the stored deleted offsets in the engine to
	 * invalidate regions */
	update_syntax_regions (se, gtk_text_iter_get_offset (iter),
			       g_utf8_strlen (text, -1));
}

static void 
update_highlight_cb (GtkSourceBuffer   *buffer,
		     const GtkTextIter *start,
		     const GtkTextIter *end,
		     gboolean           synchronous,
		     gpointer           data)
{
	GtkSourceSimpleEngine *se = data;
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (start != NULL);
       	g_return_if_fail (end != NULL);

	if (!se->priv->highlight)
		return;
	
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

static void
forget_pattern_tag (GtkSourceSimpleEngine *se, Pattern *pattern)
{
	GtkTextIter start, end;

	g_return_if_fail (se->priv->buffer);

	if (!pattern->tag) return;
	
	/* remove tag from buffer */
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (se->priv->buffer), &start, &end);
	gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (se->priv->buffer),
				    GTK_TEXT_TAG (pattern->tag),
				    &start, &end);

	g_object_unref (pattern->tag);
	pattern->tag = NULL;
}

static gboolean
retrieve_pattern_tag (GtkSourceSimpleEngine *se, Pattern *pattern)
{
	GtkTextTagTable *table;
	GtkTextTag *tag;
	GtkSourceTag *stag;
	gboolean rval = FALSE;

	g_return_val_if_fail (se->priv->buffer, FALSE);

	/* tries to get a text tag to apply to the given pattern */
	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (se->priv->buffer));
	g_return_val_if_fail (table, FALSE);

	/* lookup specific id first */
	tag = gtk_text_tag_table_lookup (table, pattern->id);
	tag = GTK_IS_SOURCE_TAG (tag) ? tag : NULL;
	if (!tag)
	{
		/* lookup style next */
		tag = gtk_text_tag_table_lookup (table, pattern->style);
		tag = GTK_IS_SOURCE_TAG (tag) ? tag : NULL;
	}

	/* cast the tag */
	stag = tag ? GTK_SOURCE_TAG (tag) : NULL;

	/* check for changes */
	if (stag != pattern->tag)
	{
		if (pattern->tag)
			forget_pattern_tag (se, pattern);
		pattern->tag = stag;
		if (stag)
			g_object_ref (stag);
		rval = TRUE;
	}
	
	return rval;
}

static void
sync_with_tag_table (GtkSourceSimpleEngine *se)
{
	gboolean invalidate = FALSE;
	GList *list, *next_list;
	
	/* check for changes in used tags in the buffer's tag table */
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer);

	list = se->priv->pattern_items;
	next_list = se->priv->syntax_items;
	while (list)
	{
		invalidate = retrieve_pattern_tag (se, list->data) || invalidate;
		
		list = list->next;
		if (!list)
		{
			list = next_list;
			next_list = NULL;
		}
	}
	
	if (invalidate)
		invalidate_highlight (se, FALSE);
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
buffer_notify_cb (GObject    *object,
		  GParamSpec *pspec,
		  gpointer    user_data)
{
	GtkSourceBuffer *buffer;
	GtkSourceSimpleEngine *se;
	gboolean highlight;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (user_data));

	buffer = GTK_SOURCE_BUFFER (object);
	se = GTK_SOURCE_SIMPLE_ENGINE (user_data);

	highlight = gtk_source_buffer_get_highlight (buffer);
	if (highlight != se->priv->highlight)
	{
		se->priv->highlight = highlight;
		invalidate_highlight (se, highlight);
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
		GList *list, *next_list;
		
		invalidate_highlight (se, FALSE);

		/* forget all cached tags */
		list = se->priv->pattern_items;
		next_list = se->priv->syntax_items;
		while (list)
		{
			forget_pattern_tag (se, list->data);
			
			list = list->next;
			if (!list)
			{
				list = next_list;
				next_list = NULL;
			}
		}

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

		/* disconnect signals */
		table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (se->priv->buffer));
		g_signal_handlers_disconnect_matched (table,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, se);
		g_signal_handlers_disconnect_matched (se->priv->buffer,
						      G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, se);
	}

	se->priv->buffer = buffer;

	if (buffer)
	{
		se->priv->highlight = gtk_source_buffer_get_highlight (buffer);
		
		/* retrieve references to all text tags */
		sync_with_tag_table (se);
	
		/* highlight data */
		se->priv->refresh_region = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
		se->priv->syntax_regions = g_array_new (FALSE, FALSE,
							sizeof (SyntaxDelimiter));
		se->priv->highlight_requests = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
		
		/* initially the buffer is empty so it's entirely analyzed */
		se->priv->worker_last_offset = -1;
		se->priv->worker_batch_size = INITIAL_WORKER_BATCH;

		g_signal_connect (buffer, "text_inserted",
				  G_CALLBACK (text_inserted_cb), se);
		g_signal_connect (buffer, "text_deleted",
				  G_CALLBACK (text_deleted_cb), se);
		g_signal_connect (buffer, "update_highlight",
				  G_CALLBACK (update_highlight_cb), se);
		g_signal_connect (buffer, "notify::highlight",
				  G_CALLBACK (buffer_notify_cb), se);
		
		table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
		if (GTK_IS_SOURCE_TAG_TABLE (table))
		{
			g_signal_connect (table, "changed", G_CALLBACK (tag_table_changed_cb), se);
		}
		else
		{
			g_warning ("Please use GtkSourceTagTable with GtkSourceBuffer.");
		}

		/* this function starts the syntax table building process */
		invalidate_syntax_regions (se, NULL, 0);
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
	GtkTextRegionIterator reg_iter;
	GtkTextIter start_iter, end_iter, last_end_iter;
	GtkSourceBuffer *source_buffer;

	g_assert (se->priv->buffer);
	source_buffer = se->priv->buffer;

	if (se->priv->worker_last_offset >= 0) {
		/* the syntax regions table is incomplete */
		build_syntax_regions_table (se, NULL);
	}

	if (se->priv->highlight)
	{
		/* Now we highlight subregions requested by the views */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
				&last_end_iter, 0);

		gtk_text_region_get_iterator (se->priv->highlight_requests,
				&reg_iter, 0);

		while (!gtk_text_region_iterator_is_end (&reg_iter))
		{
			gtk_text_region_iterator_get_subregion (&reg_iter,
				&start_iter,
				&end_iter);

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

			gtk_text_region_iterator_next (&reg_iter);
		}

		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer), &start_iter, 0);

		if (!gtk_text_iter_equal (&start_iter, &last_end_iter)) {
			/* remove already highlighted subregions from requests */
			gtk_text_region_subtract (se->priv->highlight_requests,
						  &start_iter, &last_end_iter);
		}
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

static void
sync_reg_syntax_all (GtkSourceSimpleEngine *se)
{
	GString *str;
	GList *cur;
	SyntaxPattern *sp;
	
	str = g_string_new ("");
	cur = se->priv->syntax_items;

	if (se->priv->reg_syntax_all)
	{
		egg_regex_free (se->priv->reg_syntax_all);
		se->priv->reg_syntax_all = NULL;
	}
	if (cur == NULL)
		return;

	while (cur != NULL) {
		sp = cur->data;

		g_string_append (str, sp->start_pattern);
		
		cur = g_list_next (cur);
		
		if (cur != NULL)
			g_string_append (str, "|");
	}

	se->priv->reg_syntax_all = regex_new (str->str);

	g_string_free (str, TRUE);
}

static gboolean
is_escaped (GtkSourceSimpleEngine *se, const gchar *text, gint offset)
{
	gchar *tmp = g_utf8_offset_to_pointer (text, offset);
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

static const SyntaxPattern * 
get_syntax_start (GtkSourceSimpleEngine *se,
		  const gchar           *text,
		  gint                   length,
		  guint                  match_options,
		  RegexMatch            *match)
{
	GList *list;
	SyntaxPattern *sp;
	gint pos;
	
	if (length == 0)
		return NULL;
	
	list = gtk_source_simple_engine_get_syntax_entries (se);

	if (list == NULL)
		return NULL;

	pos = 0;
	do {
		/* check for any of the syntax highlights */
		pos = regex_match (
			se->priv->reg_syntax_all,
			text,
			length,
			pos,
			match_options,
			match);
		if (pos < 0 || !is_escaped (se, text, match->start))
			break;
		pos = match->start + 1;
	} while (pos >= 0);

	if (pos < 0)
		return NULL;

	while (list != NULL) {
		sp = list->data;
		
		if (regex_match (sp->reg_start, text, match->end, pos,
				 match_options | EGG_REGEX_MATCH_ANCHORED,
				 NULL) >= 0)
			return sp;

		list = g_list_next (list);
	}

	return NULL;
}

static gboolean 
get_syntax_end (GtkSourceSimpleEngine *se,
		const gchar           *text,
		gint                   length,
		guint                  match_options,
		const SyntaxPattern   *sp,
		RegexMatch            *match)
{
	RegexMatch tmp;
	gint pos;

	g_return_val_if_fail (text != NULL, FALSE);
	g_return_val_if_fail (length >= 0, FALSE);
	g_return_val_if_fail (sp != NULL, FALSE);

	if (!match)
		match = &tmp;
	
	pos = 0;
	do {
		pos = regex_match (sp->reg_end, text, length, pos,
				   match_options, match);
		if (pos < 0 || !is_escaped (se, text, match->start))
			break;
		pos = match->start + 1;
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
		if (delim->pattern &&
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
		d1->pattern == d2->pattern);
}

/**
 * next_syntax_region:
 * @source_buffer: the GtkSourceBuffer to work on
 * @state: the current SyntaxDelimiter
 * @head: text to analyze
 * @head_length: length in characters of @head
 * @head_offset: offset in the buffer where @head starts
 * @match_options:
 * @match: RegexMatch object to get the results
 * 
 * This function can be seen as a single iteration in the analyzing
 * process.  It takes the current @state, searches for the next syntax
 * pattern in head (starting from offset 0) and if found, updates
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
		    guint                  head_options,
		    RegexMatch            *match)
{
	const SyntaxPattern *pat;
	gboolean found;
	
	if (!state->pattern) {
		/* we come from a non-syntax colored region, so seek
		 * for an opening pattern */
		pat = get_syntax_start (se, head, head_length, head_options, match);

		if (!pat)
			return FALSE;
		
		state->pattern = pat;
		state->offset = match->start + head_offset;
		state->depth = 1;

	} else {
		/* seek the closing pattern for the current syntax
		 * region */
		found = get_syntax_end (se, head, head_length, head_options,
					state->pattern, match);
		
		if (!found)
			return FALSE;
		
		state->offset = match->end + head_offset;
		state->pattern = NULL;
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
	guint slice_options;
	RegexMatch match;
	SyntaxDelimiter delim;
	GTimer *timer;

	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (se->priv->buffer);

	/* we shouldn't have been called if the buffer has no syntax entries */
	g_assert (gtk_source_simple_engine_get_syntax_entries (se) != NULL);
	
	/* make sure the all syntax regexp is sync'ed */
	if (se->priv->reg_syntax_all == NULL)
		sync_reg_syntax_all (se);
	
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
		delim.pattern = NULL;
		delim.depth = 0;

	} else {
		delim = g_array_index (table, SyntaxDelimiter, table->len - 1);
		g_assert (delim.offset <= offset);
	}

	/* get slice of text to work on */
	slice = gtk_text_iter_get_slice (&start, &end);
	head = slice;
	head_length = g_utf8_strlen (head, -1);

	/* we always stop processing at line ends */
	slice_options = (gtk_text_iter_get_line_offset (&start) != 0 ?
			 EGG_REGEX_MATCH_NOTBOL : 0);

	timer = g_timer_new ();

	/* MAIN LOOP: build the table */
	while (head_length > 0) {
		if (!next_syntax_region (se,
					 &delim,
					 head,
					 head_length,
					 offset,
					 slice_options,
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
		head = g_utf8_offset_to_pointer (head, match.end);
		head_length -= match.end;
		offset += match.end;
		
		/* recalculate b-o-l matching options */
		if (match.end > 0)
		{
			GtkTextIter tmp;
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
							    &tmp, offset);
			if (gtk_text_iter_get_line_offset (&tmp) != 0)
				slice_options |= EGG_REGEX_MATCH_NOTBOL;
			else
				slice_options &= ~EGG_REGEX_MATCH_NOTBOL;
		}
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
			gdouble et;
			gint batch_size;

			/* elapsed time in milliseconds */
			et = 1000 * g_timer_elapsed (timer, NULL);

			/* make sure the elapsed time is never 0.
			 * This happens in particular with the GTimer
			 * implementation on win32 which is not accurate.
			 * 1 ms seems to work well enough as a fallback.
			 */
			et = et != 0 ? et : 1.0;

			batch_size = MIN (head_length * WORKER_TIME_SLICE / et,
					  G_MAXINT);

			se->priv->worker_batch_size =
				MAX (batch_size, MINIMUM_WORKER_BATCH);
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
	guint slice_options;
	gint head_length, head_offset;
	GtkTextIter start_iter, end_iter;
	RegexMatch match;
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
		if (region > 0)
		{
			/* Changes to the uncontrolled regions. We can't possibly
			   know if some of the syntax regions changed, so we
			   invalidate the saved information */
			if (se->priv->old_syntax_regions) {
				g_array_free (se->priv->old_syntax_regions, TRUE);
				se->priv->old_syntax_regions = NULL;
			}
		}
		else
		{
			adjust_table_offsets (se->priv->old_syntax_regions,
					      region, delta);
		}
		return;
	}
	
	/* we shall start analyzing from the beginning of the line */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
					    &start_iter, start_offset);
	gtk_text_iter_set_line_offset (&start_iter, 0);
	head_offset = gtk_text_iter_get_offset (&start_iter);
	first_region = bsearch_offset (table, head_offset);

	/* initialize analyzing context */
	delim.pattern = NULL;
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
		if (g_array_index (table, SyntaxDelimiter, first_region - 1).pattern) {
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
		if (g_array_index (table, SyntaxDelimiter, region).pattern)
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
	head_length = g_utf8_strlen (head, -1);

	/* eol match options is constant for this run */
	slice_options = ((gtk_text_iter_get_line_offset (&start_iter) != 0 ?
			  EGG_REGEX_MATCH_NOTBOL : 0) |
			 (!gtk_text_iter_ends_line (&end_iter) ?
			  EGG_REGEX_MATCH_NOTEOL : 0));

	/* We will start analyzing the slice of text and see if it
	 * matches the information from the table.  When we hit a
	 * mismatch, it means we need to invalidate. */
	mismatch = FALSE;
	while (next_syntax_region (se,
				   &delim,
				   head,
				   head_length,
				   head_offset,
				   slice_options,
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
		head = g_utf8_offset_to_pointer (head, match.end);
		head_length -= match.end;
		head_offset += match.end;
		table_index++;
		
		/* recalculate b-o-l matching options */
		if (match.end > 0)
		{
			GtkTextIter tmp;
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (se->priv->buffer),
							    &tmp, head_offset);
			if (gtk_text_iter_get_line_offset (&tmp) != 0)
				slice_options |= EGG_REGEX_MATCH_NOTBOL;
			else
				slice_options &= ~EGG_REGEX_MATCH_NOTBOL;
		}
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
 * @length: the length (in characters) of @text
 * @offset: the offset the beginning of @text is at
 * @match_options: 
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
		 guint        match_options,
		 GList       *patterns)
{
	RegexMatch match;
	PatternMatch *pmatch;
	GList *new_pattern;
	
	new_pattern = patterns;
	while (new_pattern || matches) {
		const SimplePattern *pat;
		gint i;
		
		if (new_pattern) {
			/* process new patterns first */
			pat = new_pattern->data;
			new_pattern = new_pattern->next;
			pmatch = NULL;
		} else {
			/* process patterns already in @matches */
			pmatch = matches->data;
			pat = pmatch->pattern;
			if (pmatch->match.start >= offset) {
				/* pattern is ahead of offset, so our
				 * work is done */
				break;
			}
			/* temporarily remove the PatternMatch from
			 * the list */
			matches = g_list_delete_link (matches, matches);
		}
		
		/* do the regex search on @text */
		i = regex_match (pat->reg_pattern,
				 text,
				 length,
				 0,
				 match_options,
				 &match);
		
		if (i >= 0 && match.end != i) {
			GList *p;
			
			/* create the match structure */
			if (!pmatch) {
				pmatch = g_new0 (PatternMatch, 1);
				pmatch->pattern = pat;
			}
			/* adjust offsets */
			pmatch->match.start = match.start + offset;
			pmatch->match.end = match.end + offset;
			
			/* insert the match in order (prioritize longest match) */
			for (p = matches; p; p = p->next) {
				PatternMatch *tmp = p->data;
				if (tmp->match.start > pmatch->match.start ||
				    (tmp->match.start == pmatch->match.start &&
				     tmp->match.end < pmatch->match.end)) {
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
			if (i >= 0 && i == match.end) {
				g_warning ("The regex for pattern `%s' matched "
					   "a zero length string.  That's probably "
					   "due to a buggy regular expression.", pat->p.id);
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
	       gint                   length,
	       guint                  match_options)
{
	GList *matches;
	gint offset, start_offset;
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
	offset = start_offset = gtk_text_iter_get_offset (start);
	start_iter = end_iter = *start;
	ptr = text;
	
	/* get the initial list of matches */
	matches = search_patterns (NULL, ptr, length, offset, match_options,
				   gtk_source_simple_engine_get_pattern_entries (se));
	
	while (matches && length > 0) {
		/* pick the first (nearest) match... */
		PatternMatch *pmatch = matches->data;
		
		gtk_text_iter_set_offset (&start_iter,
					  pmatch->match.start);
		gtk_text_iter_set_offset (&end_iter,
					  pmatch->match.end);

		/* ... and apply it */
		if (pmatch->pattern->p.tag)
			gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (se->priv->buffer),
						   GTK_TEXT_TAG (pmatch->pattern->p.tag),
						   &start_iter,
						   &end_iter);

		/* now skip it completely */
		offset = pmatch->match.end;
		length += g_utf8_pointer_to_offset (text, ptr)
				+ start_offset - offset;
		ptr = g_utf8_offset_to_pointer (text, offset - start_offset);

		/* and update matches from the new position */
		matches = search_patterns (matches, ptr, length, offset,
					   match_options, NULL);
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
unhighlight_region (GtkSourceSimpleEngine *se,
		    const GtkTextIter     *start,
		    const GtkTextIter     *end)
{
	GList *l, *next_list;
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));

	l = se->priv->pattern_items;
	next_list = se->priv->syntax_items;
	while (l)
	{
		Pattern *pat = l->data;

		if (pat->tag)
			gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (se->priv->buffer),
						    GTK_TEXT_TAG (pat->tag), start, end);

		l = l->next;
		if (l == NULL && next_list)
		{
			l = next_list;
			next_list = NULL;
		}
	}
}

static void 
highlight_region (GtkSourceSimpleEngine *se,
		  GtkTextIter           *start,
		  GtkTextIter           *end)
{
	GtkTextIter b_iter, e_iter;
	gint b_off, e_off, end_offset;
	const SyntaxPattern *current_sp;
	SyntaxDelimiter *delim;
	GArray *table;
	gint region;
	gchar *slice, *slice_ptr;
	guint slice_options;
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
	unhighlight_region (se, start, end);

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
		current_sp = delim ? delim->pattern : NULL;
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
		if (current_sp) {
			/* apply syntax tag from b_iter to e_iter */
			if (current_sp->p.tag)
				gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (se->priv->buffer),
							   GTK_TEXT_TAG (current_sp->p.tag),
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
			
			/* calculate beginning and end of line match options */
			slice_options = ((gtk_text_iter_get_line_offset (&b_iter) != 0 ?
					  EGG_REGEX_MATCH_NOTBOL : 0) |
					 (!gtk_text_iter_ends_line (&e_iter) ?
					  EGG_REGEX_MATCH_NOTEOL : 0));

			check_pattern (se, &b_iter,
				       slice_ptr, tmp - slice_ptr,
				       slice_options);
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
invalidate_highlight (GtkSourceSimpleEngine *se,
		      gboolean               highlight)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (se->priv->buffer), &start, &end);
	if (highlight)
		refresh_range (se, &start, &end);
	else
		unhighlight_region (se, &start, &end);
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
		GtkTextRegionIterator reg_iter;
		
		gtk_text_region_get_iterator (region, &reg_iter, 0);

		/* highlight all subregions from the intersection.
                   hopefully this will only be one subregion */
		while (!gtk_text_region_iterator_is_end (&reg_iter))
		{
			GtkTextIter s, e;

			gtk_text_region_iterator_get_subregion (&reg_iter,
								&s, &e);
			highlight_region (se, &s, &e);

			gtk_text_region_iterator_next (&reg_iter);
		}

		gtk_text_region_destroy (region, TRUE);

		/* remove the just highlighted region */
		gtk_text_region_subtract (se->priv->refresh_region, start, end);
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

gboolean 
gtk_source_simple_engine_add_simple_pattern (GtkSourceSimpleEngine *se,
					     const gchar           *id,
					     const gchar           *style,
					     const gchar           *pattern)
{
	SimplePattern *pat;
	gboolean rval = FALSE;
	
	g_return_val_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (style != NULL, FALSE);
	g_return_val_if_fail (pattern != NULL, FALSE);

	if (!find_pattern (se->priv->pattern_items, id))
	{
		pat = simple_pattern_new (id, style, pattern);
		if (pat)
		{
			if (se->priv->buffer)
			{
				/* lookup the text tag for the pattern
				   and if we're highlighting queue up
				   a refresh */
				if (retrieve_pattern_tag (se, &(pat->p))
				    && se->priv->highlight)
					invalidate_highlight (se, TRUE);
			}
			se->priv->pattern_items = g_list_prepend (se->priv->pattern_items, pat);
			rval = TRUE;
		}
	}
	return rval;
}

gboolean 
gtk_source_simple_engine_add_syntax_pattern (GtkSourceSimpleEngine *se,
					     const gchar           *id,
					     const gchar           *style,
					     const gchar           *pattern_start,
					     const gchar           *pattern_end)
{
	SyntaxPattern *pat;
	gboolean rval = FALSE;

	g_return_val_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se), FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (style != NULL, FALSE);
	g_return_val_if_fail (pattern_start != NULL, FALSE);
	g_return_val_if_fail (pattern_end != NULL, FALSE);

	if (!find_pattern (se->priv->syntax_items, id))
	{
		pat = syntax_pattern_new (id, style, pattern_start, pattern_end);
		if (pat)
		{
			if (se->priv->buffer)
				/* cache the text tag for the syntax pattern */
				retrieve_pattern_tag (se, &(pat->p));

			se->priv->syntax_items = g_list_prepend (se->priv->syntax_items, pat);

			/* destroy the all syntax patterns regex so
			 * it's recreated when needed */
			egg_regex_free (se->priv->reg_syntax_all);
			se->priv->reg_syntax_all = NULL;
			
			/* invalidate syntax */
			if (se->priv->buffer)
				invalidate_syntax_regions (se, NULL, 0);

			rval = TRUE;
		}
	}
	return rval;
}

void 
gtk_source_simple_engine_remove_pattern (GtkSourceSimpleEngine *se,
					 const gchar           *id)
{
	GList *l;
	
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));
	g_return_if_fail (id != NULL);

	/* try simple patterns first */
	l = find_pattern (se->priv->pattern_items, id);
	if (l != NULL)
	{
		SimplePattern *pat = l->data;

		if (pat->p.tag)
		{
			/* we *do* have a buffer... */
			forget_pattern_tag (se, &(pat->p));
			if (se->priv->highlight)
				invalidate_highlight (se, TRUE);
		}
		
		simple_pattern_destroy (pat);
		se->priv->pattern_items = g_list_remove (se->priv->pattern_items, l);
	}
	else
	{
		l = find_pattern (se->priv->syntax_items, id);
		if (l != NULL)
		{
			SyntaxPattern *sp = l->data;

			if (sp->p.tag)
				forget_pattern_tag (se, &(sp->p));
				
			se->priv->syntax_items = g_list_remove_link (se->priv->syntax_items, l);
			syntax_pattern_destroy (sp);

			/* destroy the all syntax patterns regex so
			 * it's recreated when needed */
			egg_regex_free (se->priv->reg_syntax_all);
			se->priv->reg_syntax_all = NULL;
			
			/* invalidate syntax */
			if (se->priv->buffer)
				invalidate_syntax_regions (se, NULL, 0);
		}
	}
}

gunichar 
gtk_source_simple_engine_get_escape_char (GtkSourceSimpleEngine *se)
{
	g_return_val_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se), 0);

	return se->priv->escape_char;
}

void 
gtk_source_simple_engine_set_escape_char (GtkSourceSimpleEngine *se,
					  gunichar               escape_char)
{
	g_return_if_fail (GTK_IS_SOURCE_SIMPLE_ENGINE (se));

	if (escape_char != se->priv->escape_char)
	{
		se->priv->escape_char = escape_char;
		
		if (se->priv->buffer)
			/* regenerate syntax tables */
			invalidate_syntax_regions (se, NULL, 0);
	}
}


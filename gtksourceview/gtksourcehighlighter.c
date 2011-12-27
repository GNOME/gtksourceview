/* -*- mode: c; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcehighlighter.c
 *
 *  Copyright (C) 2003 - Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *  Copyright (C) 2005, 2006 - Marco Barisione, Emanuele Aina
 *  Copyright (C) 2010 Jose Aliste <jose.aliste@gmail.com>
 *  Copyright (C) 2011 Paolo Borelli <pborelli@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <errno.h>
#include <string.h>
#include <glib.h>
#include "gtksourceview-i18n.h"
#include "gtksourcehighlighter.h"
#include "gtktextregion.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcebuffer.h"
#include "gtksourcestyle-private.h"
#include "gtksourcecontextengine-private.h"
#include "gtksourcestylescheme.h"
#include "gtksourcelanguage.h"

#undef ENABLE_DEBUG
#undef ENABLE_PROFILE

G_DEFINE_TYPE (GtkSourceHighlighter, _gtk_source_highlighter, G_TYPE_OBJECT)

struct _GtkSourceHighlighterPrivate
{
	GtkSourceLanguage	*language;
	GtkTextBuffer		*buffer;
	GtkSourceStyleScheme	*style_scheme;

	/* All tags indexed by style name: values are GSList's of tags, ref()'ed. */
	GHashTable		*tags;

	/* Number of all syntax tags created by the highliter, needed to set correct
	 * tag priorities */
	guint			 n_tags;

	/* Whether or not to actually highlight the buffer. */
	gboolean		 highlight;

	/* Region covering the unhighlighted text. */
	GtkTextRegion		*refresh_region;

	/* Pointer to the segment tree created by the syntax analyzer */
	Segment			*segment_tree;
};

struct BufAndIters {
	GtkTextBuffer *buffer;
	const GtkTextIter *start, *end;
};

static void
unhighlight_region_cb (G_GNUC_UNUSED gpointer  style,
		       GSList                 *tags,
		       gpointer                user_data)
{
	struct BufAndIters *data = user_data;

	while (tags != NULL)
	{
		gtk_text_buffer_remove_tag (data->buffer,
					    tags->data,
					    data->start,
					    data->end);
		tags = tags->next;
	}
}

static void
unhighlight_region (GtkSourceHighlighter *highlighter,
		    const GtkTextIter    *start,
		    const GtkTextIter    *end)
{
	struct BufAndIters data;

	data.buffer = highlighter->priv->buffer;
	data.start = start;
	data.end = end;

	if (gtk_text_iter_equal (start, end))
		return;

	g_hash_table_foreach (highlighter->priv->tags, (GHFunc) unhighlight_region_cb, &data);
}

#define MAX_STYLE_DEPENDENCY_DEPTH 50

static void
set_tag_style (GtkSourceHighlighter *highlighter,
	       GtkTextTag           *tag,
	       const gchar          *style_id)
{
	GtkSourceStyle *style;
	const char *map_to;
	int guard = 0;

	g_return_if_fail (GTK_IS_TEXT_TAG (tag));
	g_return_if_fail (style_id != NULL);

	_gtk_source_style_apply (NULL, tag);

	if (highlighter->priv->style_scheme == NULL)
		return;

	map_to = style_id;
	style = gtk_source_style_scheme_get_style (highlighter->priv->style_scheme, style_id);

	while (style == NULL)
	{
		if (guard > MAX_STYLE_DEPENDENCY_DEPTH)
		{
			g_warning ("Potential circular dependency between styles detected for style '%s'", style_id);
			break;
		}

		++guard;

		/* FIXME Style references really must be fixed, both parser for
		 * sane use in lang files, and engine for safe use. */
		map_to = gtk_source_language_get_style_fallback (highlighter->priv->language, map_to);
		if (map_to == NULL)
			break;

		style = gtk_source_style_scheme_get_style (highlighter->priv->style_scheme, map_to);
	}

	/* not having style is fine, since parser checks validity of every style reference,
	 * so we don't need to spit a warning here */
	if (style != NULL)
		_gtk_source_style_apply (style, tag);
}

/* Find tag which has to be overridden. */
static GtkTextTag *
get_parent_tag (Context    *context,
		const char *style)
{
	while (context != NULL)
	{
		/* Lang files may repeat same style for nested contexts,
		 * ignore them. */
		if (context->style &&
		    strcmp (context->style, style) != 0)
		{
			g_assert (context->tag != NULL);
			return context->tag;
		}

		context = context->parent;
	}

	return NULL;
}

static GtkTextTag *
get_tag_for_parent (GtkSourceHighlighter *highlighter,
		    const char           *style,
		    Context              *parent)
{
	GtkTextTag *tag;

	g_return_val_if_fail (style != NULL, NULL);

	tag = get_parent_tag (parent, style);

	return _gtk_source_highlighter_get_tag_for_style (highlighter, style, tag);
}

static GtkTextTag *
get_subpattern_tag (GtkSourceHighlighter *highlighter,
		    Context              *context,
		    SubPatternDefinition *sp_def)
{
	if (sp_def->style == NULL)
		return NULL;

	g_assert (sp_def->index < context->definition->n_sub_patterns);

	if (context->subpattern_tags == NULL)
		context->subpattern_tags = g_new0 (GtkTextTag*, context->definition->n_sub_patterns);

	if (context->subpattern_tags[sp_def->index] == NULL)
		context->subpattern_tags[sp_def->index] = get_tag_for_parent (highlighter, sp_def->style, context);

	return context->subpattern_tags[sp_def->index];
}

static GtkTextTag *
get_context_tag (GtkSourceHighlighter *highlighter,
		 Context              *context)
{
	if (context->style != NULL && context->tag == NULL)
	{
		context->tag = get_tag_for_parent (highlighter, context->style, context->parent);
	}

	return context->tag;
}

static void
apply_tags (GtkSourceHighlighter *highlighter,
	    Segment              *segment,
	    gint                  start_offset,
	    gint                  end_offset)
{
	GtkTextTag *tag;
	GtkTextIter start_iter, end_iter;
	GtkTextBuffer *buffer = highlighter->priv->buffer;
	SubPattern *sp;
	Segment *child;

	g_assert (segment != NULL);

	/* Non-annotated segments are invalid.*/
	if (SEGMENT_IS_INVALID (segment))
		return;

	if (segment->start_at >= end_offset || segment->end_at <= start_offset)
		return;

	start_offset = MAX (start_offset, segment->start_at);
	end_offset = MIN (end_offset, segment->end_at);

	tag = get_context_tag (highlighter, segment->context);
	if (tag != NULL)
	{
		gint style_start_at, style_end_at;

		style_start_at = start_offset;
		style_end_at = end_offset;

		if (_gtk_source_context_get_style_inside (segment->context))
		{
			style_start_at = MAX (segment->start_at + segment->start_len, start_offset);
			style_end_at = MIN (segment->end_at - segment->end_len, end_offset);
		}

		if (style_start_at > style_end_at)
		{
			g_critical ("%s: oops", G_STRLOC);
		}
		else
		{	/* FIXME: We should cache the start_at and end_at so we only apply the tag
			   where is needed, instead of erasing all the highlighting */
			gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, style_start_at);
			end_iter = start_iter;
			gtk_text_iter_forward_chars (&end_iter, style_end_at - style_start_at);
			gtk_text_buffer_apply_tag (highlighter->priv->buffer, tag, &start_iter, &end_iter);
		}
	}

	for (sp = segment->sub_patterns; sp != NULL; sp = sp->next)
	{
		if (sp->start_at >= start_offset && sp->end_at <= end_offset)
		{
			gint start = MAX (start_offset, sp->start_at);
			gint end = MIN (end_offset, sp->end_at);

			tag = get_subpattern_tag (highlighter, segment->context, sp->definition);
			if (tag != NULL)
			{
				gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start);
				end_iter = start_iter;
				gtk_text_iter_forward_chars (&end_iter, end - start);
				gtk_text_buffer_apply_tag (highlighter->priv->buffer, tag, &start_iter, &end_iter);
			}
		}
	}

	for (child = segment->children;
	     child != NULL && child->start_at < end_offset;
	     child = child->next)
	{
		if (child->end_at > start_offset)
			apply_tags (highlighter, child, start_offset, end_offset);
	}
}

/**
 * highlight_region:
 *
 * @highlighter: a #GtkSourceHighlighter.
 * @start: the beginning of the region to highlight.
 * @end: the end of the region to highlight.
 *
 * Highlights the specified region.
 */
static void
highlight_region (GtkSourceHighlighter *highlighter,
		  const GtkTextIter  *start,
		  const GtkTextIter  *end)
{
#ifdef ENABLE_PROFILE
	GTimer *timer;
#endif
	GtkTextIter real_end;

	real_end = *end;
	if (gtk_text_iter_starts_line (&real_end))
		gtk_text_iter_backward_char (&real_end);
	if (gtk_text_iter_compare (start, &real_end) >= 0)
		return;

#ifdef ENABLE_PROFILE
	timer = g_timer_new ();
#endif

	/* First we need to delete tags in the regions. */
	unhighlight_region (highlighter, start, &real_end);

	apply_tags (highlighter, highlighter->priv->segment_tree,
		    gtk_text_iter_get_offset (start),
		    gtk_text_iter_get_offset (&real_end));

	g_signal_emit_by_name (highlighter->priv->buffer,
			       "highlight_updated",
			       start,
			       &real_end);

#ifdef ENABLE_PROFILE
	g_print ("highlight (from %d to %d), %g ms elapsed\n",
		 gtk_text_iter_get_offset (start),
		 gtk_text_iter_get_offset (&real_end),
		 g_timer_elapsed (timer, NULL) * 1000);
	g_timer_destroy (timer);
#endif
}

static GtkTextTag *
create_tag (GtkSourceHighlighter *highlighter,
		   const gchar          *style_id)
{
	GtkTextTag *new_tag;

	g_assert (style_id != NULL);

	new_tag = gtk_text_buffer_create_tag (highlighter->priv->buffer, NULL, NULL);

	/* It must have priority lower than user tags but still
	 * higher than highlighting tags created before */
	gtk_text_tag_set_priority (new_tag, highlighter->priv->n_tags);
	highlighter->priv->n_tags += 1;
	set_tag_style (highlighter, new_tag, style_id);

	return new_tag;
}

GtkTextTag *
_gtk_source_highlighter_get_tag_for_style (GtkSourceHighlighter *highlighter,
				    	   const gchar          *style,
					   GtkTextTag	   	*parent_tag)
{
	GSList *tags;
	GtkTextTag *tag;

	tags = g_hash_table_lookup (highlighter->priv->tags, style);

	if (tags && (!parent_tag ||
	    gtk_text_tag_get_priority (tags->data) > gtk_text_tag_get_priority (parent_tag)))
	{
		GSList *link;

		tag = tags->data;

		/* Now get the tag with lowest priority, so that tag lists do not grow
		 * indefinitely. */
		for (link = tags->next; link != NULL; link = link->next)
		{
			if (parent_tag &&
			    gtk_text_tag_get_priority (link->data) < gtk_text_tag_get_priority (parent_tag))
				break;
			tag = link->data;
		}
	}
	else
	{
		tag = create_tag (highlighter, style);

		tags = g_slist_prepend (tags, g_object_ref (tag));
		g_hash_table_insert (highlighter->priv->tags, g_strdup (style), tags);

#ifdef ENABLE_DEBUG
		{
			GString *style_path = g_string_new (style);
			gint n;

			while (parent != NULL)
			{
				if (parent->style != NULL)
				{
					g_string_prepend (style_path, "/");
					g_string_prepend (style_path,
							  parent->style);
				}

				parent = parent->parent;
			}

			tags = g_hash_table_lookup (highlighter->priv->tags, style);
			n = g_slist_length (tags);
			g_print ("created %d tag for style %s: %s\n", n, style, style_path->str);
			g_string_free (style_path, TRUE);
		}
#endif
	}

	return tag;
}

/**
 * _gtk_source_highlighter_invalidate_region
 * 
 * @highlighter: 
 * @start: the start of the invalidated_region
 * @end: the end of the invalidated_region
 *
 * This method is called by the context_engine to let the highlighter
 * know that the syntax tree has been updated between @start and @end,
 * and hence, the highlighting of the region is invalid.
 */ 
void
_gtk_source_highlighter_invalidate_region (GtkSourceHighlighter *highlighter, 
					   const GtkTextIter    *start,
					   const GtkTextIter    *end)
{
	if (!highlighter->priv->highlight)
		return;

	gtk_text_region_add (highlighter->priv->refresh_region, start, end);
}

/**
 * ensure_highlighted:
 *
 * @highlighter: a #GtkSourceHighlighter.
 * @start: the beginning of the region to highlight.
 * @end: the end of the region to highlight.
 *
 * Updates text tags in reanalyzed parts of given area.
 * It applies tags according to whatever is in the syntax
 * tree currently, so highlighting may not be correct
 * (gtk_source_highlighter_update_highlight is the method
 * that actually ensures correct highlighting).
 */
void
_gtk_source_highlighter_ensure_highlight (GtkSourceHighlighter *highlighter,
					  const GtkTextIter      *start,
					  const GtkTextIter      *end)
{
	GtkTextRegion *region;
	GtkTextRegionIterator reg_iter;

	if (!highlighter->priv->highlight)
		return;

	/* Get the subregions not yet highlighted. */
	region = gtk_text_region_intersect (highlighter->priv->refresh_region, start, end);
	if (region == NULL)
		return;

	gtk_text_region_get_iterator (region, &reg_iter, 0);

	/* Highlight all subregions from the intersection.
	 * hopefully this will only be one subregion. */
	while (!gtk_text_region_iterator_is_end (&reg_iter))
	{
		GtkTextIter s, e;
		gtk_text_region_iterator_get_subregion (&reg_iter, &s, &e);
		highlight_region (highlighter, &s, &e);
		gtk_text_region_iterator_next (&reg_iter);
	}

	gtk_text_region_destroy (region, TRUE);

	/* Remove the just highlighted region. */
	gtk_text_region_subtract (highlighter->priv->refresh_region, start, end);
}

static void
set_tag_style_hash_cb (const char           *style,
		       GSList               *tags,
		       GtkSourceHighlighter *highlighter)
{
	while (tags != NULL)
	{
		set_tag_style (highlighter, tags->data, style);
		tags = tags->next;
	}
}

/*
 * enable_highlight:
 *
 * @highlighter: a #GtkSourceHighlighter.
 * @enable: whether to enable highlighting.
 *
 * Whether to highlight (i.e. apply tags) analyzed area.
 * Note that this does not turn on/off the analysis stuff,
 * it affects only text tags.
 */
static void
enable_highlight (GtkSourceHighlighter *highlighter,
		  gboolean              enable)
{
	GtkTextIter start, end;

	if (!enable == !highlighter->priv->highlight)
		return;

	highlighter->priv->highlight = enable != 0;
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (highlighter->priv->buffer),
				    &start, &end);

	if (enable)
	{
		gtk_text_region_add (highlighter->priv->refresh_region, &start, &end);
		//g_hash_table_foreach (highlighter->priv->tags, (GHFunc) set_tag_style_hash_cb, highlighter);
		//GtkTextIter real_end;

        	//if (gtk_text_iter_equal (start, end))
                //return;

        /* Here we need to make sure we do not make it redraw next line */
       // real_end = *end;
       // if (gtk_text_iter_starts_line (&real_end))
       // {
                /* I don't quite like this here, but at least it won't jump into
 *                  * the middle of \r\n  */
       //         gtk_text_iter_backward_cursor_position (&real_end);
       // }

		g_signal_emit_by_name (highlighter->priv->buffer,
				       "highlight_updated",
				       start,
				       end);
	}
	else
	{
		unhighlight_region (highlighter, &start, &end);	
	}
}

static void
buffer_notify_highlight_syntax_cb (GtkSourceHighlighter *highlighter)
{
	gboolean highlight;

	g_object_get (highlighter->priv->buffer, "highlight-syntax", &highlight, NULL);
	enable_highlight (highlighter, highlight);
}

static void
remove_tags_hash_cb (G_GNUC_UNUSED gpointer  style,
		     GSList                 *tags,
		     GtkTextTagTable        *table)
{
	GSList *l = tags;

	while (l != NULL)
	{
		gtk_text_tag_table_remove (table, l->data);
		g_object_unref (l->data);
		l = l->next;
	}

	g_slist_free (tags);
}

static void
destroy_tags_hash (GtkSourceHighlighter *highlighter)
{
	g_hash_table_foreach (highlighter->priv->tags, (GHFunc) remove_tags_hash_cb,
	                      gtk_text_buffer_get_tag_table (highlighter->priv->buffer));
	g_hash_table_destroy (highlighter->priv->tags);
	highlighter->priv->tags = NULL;
}

/*
 * gtk_source_highlighter_attach_buffer:
 *
 * @highlighter: #GtkSourceHighlighter.
 * @buffer: buffer.
 *
 * Detaches highlighter from previous buffer, and attaches to @buffer if
 * it's not %NULL.
 */
void
_gtk_source_highlighter_attach_buffer (GtkSourceHighlighter *highlighter,
				       GtkTextBuffer        *buffer,
				       Segment		    *root_segment)
{
	g_return_if_fail (GTK_SOURCE_IS_HIGHLIGHTER (highlighter));

	if (highlighter->priv->buffer == buffer)
		return;

	/* Detach previous buffer if there is one. */
	if (highlighter->priv->buffer != NULL)
	{
		g_signal_handlers_disconnect_by_func (highlighter->priv->buffer,
						      (gpointer) buffer_notify_highlight_syntax_cb,
						      highlighter);

		/* the root_segment is owned by the engine. */
		highlighter->priv->segment_tree = NULL;

		if (highlighter->priv->refresh_region != NULL)
			gtk_text_region_destroy (highlighter->priv->refresh_region, FALSE);
		highlighter->priv->refresh_region = NULL;

		/* this deletes tags from the tag table, therefore there is no need
		 * in removing tags from the text (it may be very slow).
		 * FIXME: don't we want to just destroy and forget everything when
		 * the buffer is destroyed? Removing tags is still slower than doing
		 * nothing. Caveat: if tag table is shared with other buffer, we do
		 * need to remove tags. */
		destroy_tags_hash (highlighter);
		highlighter->priv->n_tags = 0;
	}

	highlighter->priv->buffer = buffer;

	if (buffer != NULL)
	{
		highlighter->priv->segment_tree = root_segment;
		highlighter->priv->tags = g_hash_table_new_full (g_str_hash,
								 g_str_equal,
								 g_free, NULL);	
	
		g_object_get (highlighter->priv->buffer, "highlight-syntax", 
							&highlighter->priv->highlight, NULL);
		
		g_signal_connect_swapped (buffer,
					  "notify::highlight-syntax",
					  G_CALLBACK (buffer_notify_highlight_syntax_cb),
					  highlighter);
		highlighter->priv->refresh_region = gtk_text_region_new (buffer);
	}
}

/**
 * gtk_source_highlighter_set_style_scheme:
 * @highlighter: #GtkSourceHighlighter.
 * @scheme: #GtkSourceStyleScheme to set.
 *
 * Sets current style scheme, updates tag styles and everything.
 */
void
_gtk_source_highlighter_set_style_scheme (GtkSourceHighlighter *highlighter,
					  GtkSourceStyleScheme *scheme)
{
	g_return_if_fail (GTK_SOURCE_IS_HIGHLIGHTER (highlighter));
	g_return_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme) || scheme == NULL);

	if (scheme != highlighter->priv->style_scheme)
	{
		if (highlighter->priv->style_scheme != NULL)
			g_object_unref (highlighter->priv->style_scheme);

		highlighter->priv->style_scheme = scheme ? g_object_ref (scheme) : NULL;
		g_hash_table_foreach (highlighter->priv->tags, (GHFunc) set_tag_style_hash_cb, highlighter);
	}
}

static void
gtk_source_highlighter_finalize (GObject *object)
{
	GtkSourceHighlighter *highlighter = GTK_SOURCE_HIGHLIGHTER (object);

	g_assert (!highlighter->priv->tags);
	g_assert (!highlighter->priv->segment_tree);
	
	if (highlighter->priv->style_scheme != NULL)
		g_object_unref (highlighter->priv->style_scheme);

	G_OBJECT_CLASS (_gtk_source_highlighter_parent_class)->finalize (object);
}

static void
_gtk_source_highlighter_class_init (GtkSourceHighlighterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_highlighter_finalize;
	g_type_class_add_private (object_class, sizeof (GtkSourceHighlighterPrivate));
}

static void
_gtk_source_highlighter_init (GtkSourceHighlighter *highlighter)
{
	highlighter->priv = G_TYPE_INSTANCE_GET_PRIVATE (highlighter, GTK_TYPE_SOURCE_HIGHLIGHTER,
						GtkSourceHighlighterPrivate);
}

GtkSourceHighlighter *
_gtk_source_highlighter_new (GtkSourceLanguage *language)
{
	GtkSourceHighlighter *highlighter;

	highlighter = g_object_new (GTK_TYPE_SOURCE_HIGHLIGHTER, NULL);
	highlighter->priv->language = language;

	return highlighter;
}


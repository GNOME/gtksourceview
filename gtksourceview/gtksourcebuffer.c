/*  gtksourcebuffer.c
 *
 *  Copyright (C) 1999,2000,2001,2002 by:
 *          Mikael Hermansson <tyan@linux.se>
 *          Chris Phelps <chicane@reninet.com>
 *          Jeroen Zwartepoorte <jeroen@xs4all.nl>
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

#include <string.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include "gtkundomanager.h"
#include "gtksourceview-marshal.h"
#include "gtktextregion.h"

#undef ENABLE_DEBUG

#ifdef ENABLE_DEBUG
#define DEBUG(x) (x)
#else
#define DEBUG(x)
#endif

#define UNDO_TYPE_INSERT_TEXT 1
#define UNDO_TYPE_REMOVE_RANGE 2
#define SEARCH_CASE_INSENSITIVE 3

/* how many lines will the refresh idle handler process at a time */
#define DEFAULT_IDLE_REFRESH_LINES_PER_RUN  100
#define MINIMUM_IDLE_REFRESH_LINES_PER_RUN  20
/* in milliseconds */
#define IDLE_REFRESH_TIME_SLICE             100


typedef struct _SearchInfo {
	const gchar *searchfor;
	gchar *offset;
	gint matched : 1;
	GtkTextSearchFlags sflags;
} SearchInfo;

typedef struct _MarkerSubList {
	int line;
	GList *marker_list;
} MarkerSubList;

struct _GtkSourceBufferPrivate {
	gint            highlight : 1;
	gint            check_brackets : 1;
	GtkTextTag     *bracket_match_tag;
	GtkTextMark    *mark;
	GHashTable     *line_markers;

	gint            undo_max;
	GList          *undo_redo;
	guint           undo_level;
	guint           undo_redo_processing : 1;
	GList          *syntax_items;
	GList          *pattern_items;
	GList          *embedded_items;
	Regex           reg_syntax_all;

	/* region covering the unhighlighted text */
	GtkTextRegion  *refresh_region;
	guint           refresh_idle_handler;
	guint           refresh_lines;
	
	GtkUndoManager *undo_manager;
};

enum {
	CAN_UNDO,
	CAN_REDO,
	LAST_SIGNAL
};

static GtkTextBufferClass *parent_class = NULL;
static guint buffer_signals[LAST_SIGNAL] = { 0 };

static void gtk_source_buffer_class_init (GtkSourceBufferClass *klass);
static void gtk_source_buffer_init       (GtkSourceBuffer      *klass);
static void gtk_source_buffer_finalize   (GObject              *object);

static void gtk_source_buffer_can_undo_handler (GtkUndoManager  *um,
						gboolean         can_undo,
						GtkSourceBuffer *buffer);
static void gtk_source_buffer_can_redo_handler (GtkUndoManager  *um,
						gboolean         can_redo,
						GtkSourceBuffer *buffer);

static void move_cursor (GtkTextBuffer *buffer,
			 GtkTextIter   *iter,
			 GtkTextMark   *mark,
			 gpointer       data);

static void gtk_source_buffer_real_insert_text  (GtkTextBuffer *buffer,
						 GtkTextIter   *iter,
						 const gchar   *text,
						 gint           len);
static void gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
						 GtkTextIter   *iter,
						 GtkTextIter   *end);

static void check_embedded (GtkSourceBuffer *buffer,
			    GtkTextIter     *start,
			    GtkTextIter     *end);
static void check_syntax   (GtkSourceBuffer *buffer,
			    GtkTextIter     *start,
			    GtkTextIter     *end);
static void check_pattern  (GtkSourceBuffer *buffer,
			    const gchar     *text,
			    gint             length,
			    GtkTextIter     *iter);

static gint get_syntax_end (const gchar          *text,
			    gint                  pos,
			    Regex                *reg,
			    GtkSourceBufferMatch *match);

static gint get_tag_start (GtkTextTag  *tag,
			   GtkTextIter *iter);
static gint get_tag_end   (GtkTextTag  *tag,
			   GtkTextIter *iter);

static void hash_remove_func (gpointer key, gpointer value, gpointer user_data);

static void get_tags_func (GtkTextTag *tag, gpointer data);

static gboolean read_loop (GtkTextBuffer *buffer,
			   const char    *filename,
			   GIOChannel    *io,
			   GError       **error);


/* ----------------------------------------------------------------------
   Private functions
   ---------------------------------------------------------------------- */

static void
gtk_source_buffer_class_init (GtkSourceBufferClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize     = gtk_source_buffer_finalize;
	klass->can_undo            = NULL;
	klass->can_redo            = NULL;

	/* Do not set these signals handlers directly on the parent_class since
	 * that will cause problems (a loop). */
	GTK_TEXT_BUFFER_CLASS (klass)->insert_text  = gtk_source_buffer_real_insert_text;
	GTK_TEXT_BUFFER_CLASS (klass)->delete_range = gtk_source_buffer_real_delete_range;

	buffer_signals[CAN_UNDO] =
		g_signal_new ("can_undo",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GtkSourceBufferClass, can_undo),
			      NULL, NULL,
			      gtksourceview_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);

	buffer_signals[CAN_REDO] =
		g_signal_new ("can_redo",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GtkSourceBufferClass, can_redo),
			      NULL, NULL,
			      gtksourceview_marshal_VOID__BOOLEAN,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_BOOLEAN);
}

static void
gtk_source_buffer_init (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv;

	priv = g_new0 (GtkSourceBufferPrivate, 1);

	buffer->priv = priv;

	priv->undo_manager = gtk_undo_manager_new (buffer);

	priv->check_brackets = FALSE;
	priv->mark = NULL;
	priv->undo_redo = NULL;
	priv->undo_level = 0;
	priv->undo_max = 5;
	priv->undo_redo_processing = FALSE;
	priv->line_markers = g_hash_table_new (NULL, NULL);

	/* highlight data */
	priv->highlight = TRUE;
	priv->refresh_idle_handler = 0;
	priv->refresh_region = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
	priv->refresh_lines = DEFAULT_IDLE_REFRESH_LINES_PER_RUN;
	
	g_signal_connect_closure (G_OBJECT (buffer),
				  "mark_set",
				  g_cclosure_new (G_CALLBACK (move_cursor),
						  NULL, NULL),
				  TRUE);

	g_signal_connect (G_OBJECT (priv->undo_manager),
			  "can_undo",
			  G_CALLBACK (gtk_source_buffer_can_undo_handler), 
			  buffer);

	g_signal_connect (G_OBJECT (priv->undo_manager),
			  "can_redo",
			  G_CALLBACK (gtk_source_buffer_can_redo_handler), 
			  buffer);
}

static void
gtk_source_buffer_finalize (GObject *object)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	buffer = GTK_SOURCE_BUFFER (object);

	if (buffer->priv) {
		if (buffer->priv->line_markers) {
			g_hash_table_foreach_remove (buffer->priv->line_markers,
						     (GHRFunc) hash_remove_func,
						     NULL);
			g_hash_table_destroy (buffer->priv->line_markers);
		}

		gtk_text_region_destroy (buffer->priv->refresh_region);
		
		g_free (buffer->priv);
		buffer->priv = NULL;
	}
}

static void
hash_remove_func (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
	GList *iter = value;

	if (iter) {
		for (; iter; iter = iter->next) {
			if (iter->data)
				g_free (iter->data);
		}
		g_list_free (iter);
	}
}

static void
gtk_source_buffer_can_undo_handler (GtkUndoManager  *um,
				    gboolean         can_undo,
				    GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	g_signal_emit (G_OBJECT (buffer),
		       buffer_signals [CAN_UNDO],
		       0,
		       can_undo);
}

static void
gtk_source_buffer_can_redo_handler (GtkUndoManager  *um,
				    gboolean         can_redo,
				    GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	g_signal_emit (G_OBJECT (buffer),
		       buffer_signals [CAN_REDO],
		       0,
		       can_redo);
}

static gint
get_tag_start (GtkTextTag  *tag,
	       GtkTextIter *iter)
{
	gint oldoff;

	if (gtk_text_iter_begins_tag (iter, tag))
		return 0;
		
	oldoff = gtk_text_iter_get_offset (iter);
	gtk_text_iter_backward_to_tag_toggle (iter, tag);
	return oldoff - gtk_text_iter_get_offset (iter);
}

static gint
get_tag_end (GtkTextTag  *tag,
	     GtkTextIter *iter)
{
	gint oldoff;

	if (gtk_text_iter_ends_tag (iter, tag))
		return 0;
		
	oldoff = gtk_text_iter_get_offset (iter);
	gtk_text_iter_forward_to_tag_toggle (iter, tag);
	return gtk_text_iter_get_offset (iter) - oldoff;
}

static void
get_tags_func (GtkTextTag *tag,
	       gpointer    data)
{
	GList **list = (GList **) data;

	if (GTK_IS_SYNTAX_TAG (tag)  ||
	    GTK_IS_PATTERN_TAG (tag) ||
	    GTK_IS_EMBEDDED_TAG (tag))
		*list = g_list_append (*list, (gpointer) tag);
}

static void
move_cursor (GtkTextBuffer *buffer,
	     GtkTextIter   *iter,
	     GtkTextMark   *mark,
	     gpointer       data)
{
	GtkSourceBuffer *sbuf = GTK_SOURCE_BUFFER (buffer);
	GtkSourceBufferPrivate *priv = sbuf->priv;
	GtkTextIter iter1;
	GtkTextIter iter2;

	if (mark != gtk_text_buffer_get_insert (buffer))
		return;

	if (priv->mark) {
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter1,
						  priv->mark);
		iter2 = iter1;
		gtk_text_iter_forward_char (&iter2);
		gtk_text_buffer_remove_tag (buffer,
					    priv->bracket_match_tag,
					    &iter1,
					    &iter2);
	}

	if (gtk_source_buffer_iter_has_syntax_tag (iter) || !priv->check_brackets)
		return;

	if (gtk_source_buffer_find_bracket_match (iter)) {
		if (!priv->mark)
			priv->mark = gtk_text_buffer_create_mark (buffer,
								  NULL,
								  iter,
								  FALSE);
		else
			gtk_text_buffer_move_mark (buffer, priv->mark, iter);

		iter2 = *iter;
		gtk_text_iter_forward_char (&iter2);
		gtk_text_buffer_apply_tag (buffer,
					   priv->bracket_match_tag,
					   iter,
					   &iter2);
	}
}

static gboolean
idle_refresh_handler (GtkSourceBuffer *sbuf)
{
	GtkTextIter start, end;
	GtkSourceBufferPrivate *priv;
	GtkTextBuffer *buffer;
	gboolean retval;
	
	g_return_val_if_fail (sbuf != NULL, FALSE);
	
	priv = sbuf->priv;
	buffer = GTK_TEXT_BUFFER (sbuf);
	retval = TRUE;
	
	/* make sure the region contains valid data */
	gtk_text_region_clear_zero_length_subregions (priv->refresh_region);

	if (!priv->highlight || gtk_text_region_subregions (priv->refresh_region) == 0) {
		/* nothing to do */
		retval = FALSE;
	} else {
		GTimer *timer;
		gulong  slice;
		
		/* get us some work to do */
		gtk_text_region_nth_subregion (priv->refresh_region, 0, &start, &end);

		if (gtk_text_iter_get_line (&end) - gtk_text_iter_get_line (&start)
		    > priv->refresh_lines) {
			/* region too big, reduce it */
			end = start;
			gtk_text_iter_forward_lines (&end, priv->refresh_lines);
		}
	
		/* profile syntax highlighting */
		timer = g_timer_new ();
		g_timer_start (timer);

		check_embedded (sbuf, &start, &end);

		g_timer_stop (timer);
		g_timer_elapsed (timer, &slice);
		g_timer_destroy (timer);

		/* assume elapsed time is linear with number of lines
                   and make our best guess for next run */
		priv->refresh_lines = (IDLE_REFRESH_TIME_SLICE * 1000
				       * priv->refresh_lines) / slice;
		priv->refresh_lines = MAX (priv->refresh_lines, MINIMUM_IDLE_REFRESH_LINES_PER_RUN);
		
		/* region done */
		gtk_text_region_substract (priv->refresh_region, &start, &end);
	}
	
	if (!retval || gtk_text_region_subregions (priv->refresh_region) == 0) {
		/* nothing else to do */
		retval = FALSE;

		/* handler will be removed */
		priv->refresh_idle_handler = 0;
	}

	return retval;
}

static void
refresh_range (GtkSourceBuffer *sbuf,
	       GtkTextIter     *start,
	       GtkTextIter     *end)
{
	g_return_if_fail (sbuf && GTK_IS_SOURCE_BUFFER (sbuf));

	/* add the region to the refresh region */
	gtk_text_region_add (sbuf->priv->refresh_region, start, end);
	
	if (sbuf->priv->highlight && !sbuf->priv->refresh_idle_handler) {
		/* now add the idle handler if one was not running */
		sbuf->priv->refresh_idle_handler = g_idle_add_full (
			G_PRIORITY_DEFAULT_IDLE,
			(GSourceFunc) idle_refresh_handler,
			sbuf, NULL);
	}
}

static void
gtk_source_buffer_real_insert_text (GtkTextBuffer *buffer,
				    GtkTextIter   *iter,
				    const gchar   *text,
				    gint           len)
{
	GtkSyntaxTag *tag;
	GtkTextIter start, end;
	GtkSourceBuffer *sbuf;
	GtkSourceBufferPrivate *priv;
	gint soff, eoff;
	
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	sbuf = GTK_SOURCE_BUFFER (buffer);
	priv = sbuf->priv;
	start = end = *iter;

	if (!priv->highlight) {
		parent_class->insert_text (buffer, iter, text, len);
		end = start = *iter;
		gtk_text_iter_backward_chars (&start, len);
		/* FIXME */
		/*gtk_source_buffer_undo_insert (sbuf,
					       UNDO_TYPE_REMOVE_RANGE,
					       &start,
					       &end);*/
		return;
	}

	gtk_text_iter_forward_chars (&end, len);

	/* we have connected AFTER insert thats why iterator points to end */
	/* we need to update startiter to point to txt startpos instead :-) */

	if (priv->syntax_items) {
		tag = gtk_source_buffer_iter_has_syntax_tag (&start);
		if (!tag) {
			/* no syntax found we refresh from */
			/*start of first instered line to end of last inserted line */
			gtk_text_iter_set_line_offset (&start, 0);
			gtk_text_iter_forward_line (&end);
		} else {
			gint scount;
			gint ecount;

			scount = get_tag_start (GTK_TEXT_TAG (tag), &start);
			ecount = get_tag_end (GTK_TEXT_TAG (tag), &end);
		}
	} else
		gtk_text_buffer_get_bounds (buffer, &start, &end);

	gtk_text_buffer_remove_all_tags (buffer, &start, &end);
	soff = gtk_text_iter_get_offset (&start);
	eoff = gtk_text_iter_get_offset (&end) + len;
	
	parent_class->insert_text (buffer, iter, text, len);

	/* FIXME */
	/*gtk_source_buffer_undo_insert (sbuf, UNDO_TYPE_REMOVE_RANGE, &start, &end);*/

	gtk_text_buffer_get_iter_at_offset (buffer, &start, soff);
	gtk_text_buffer_get_iter_at_offset (buffer, &end, eoff);
	
	refresh_range (sbuf, &start, &end);
}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *iter,
				     GtkTextIter   *iter2)
{
	GtkTextIter start;
	GtkTextIter end;
	GtkSyntaxTag *tag;
	GtkSourceBuffer *sbuf;
	GtkSourceBufferPrivate *priv;
	gint refresh_start, refresh_length;
	
	sbuf = GTK_SOURCE_BUFFER (buffer);
	priv = sbuf->priv;
	start = *iter;
	end = *iter2;

	/* FIXME */
	/*gtk_source_buffer_undo_insert (sbuf, UNDO_TYPE_INSERT_TEXT, iter, iter2);*/
	if (!priv->highlight) {
		parent_class->delete_range (buffer, iter, iter2);
		return;
	}

	/* we need to work on this */
	/* first check if iter holds a tag */
	/* if true then iterate backward until tag change */
	/* then iterate iter2 forward until tag change */
	/* remove tags and update from new start and end iters */
	/* if no tags, we iterate from line begin to iter2 line end */
	/* also check if syntax tag if true then ignore and let widget take care of it */

	if (priv->syntax_items) {
		tag = gtk_source_buffer_iter_has_syntax_tag (&start);
		if (!tag) {
			/* no syntax found, so we refresh from */
			/* start of first instered line to end of last inserted line */
			gtk_text_iter_set_line_offset (&start, 0);
			end = start;
			gtk_text_iter_forward_line (&end);
			if (gtk_text_iter_get_offset (&end) < gtk_text_iter_get_offset (iter2))
				end = *iter2;
		} else {
			gint scount;
			gint ecount;

			scount = get_tag_start (GTK_TEXT_TAG (tag), &start);
			ecount = get_tag_end (GTK_TEXT_TAG (tag), &end);
			if (scount > tag->reg_start.len && ecount > tag->reg_end.len)  {
				parent_class->delete_range (buffer, iter, iter2);
				return;
			}
		}
	}

	refresh_start = gtk_text_iter_get_offset (&start);
	refresh_length = gtk_text_iter_get_offset (&end) - refresh_start;
	gtk_text_buffer_remove_all_tags (buffer, &start, &end);

	parent_class->delete_range (buffer, iter, iter2);

	if (!refresh_length)
		return;

	gtk_text_buffer_get_iter_at_offset (buffer, &start, refresh_start);
	end = start;
	gtk_text_iter_forward_chars (&end, refresh_length);

	refresh_range (sbuf, &start, &end);
}


/*
   we need to optimize this crap, as it is pretty slow
   if the start and end interval is to big (which it usually is)
   The most important thing is to use the included
   regular expression library because it's massively
   faster than what is coming with distributions right
   now, especially RedHat. Probably 4 to 8 times faster
   even though they are theoretically the same version (12)
*/
static void
check_embedded (GtkSourceBuffer *sbuf,
		GtkTextIter     *iter1,
		GtkTextIter     *iter2)
{
	GtkTextBuffer *buf;
	GList *list;
	gchar *text;
	GtkTextIter start_iter;
	GtkTextIter cur_iter;
	GtkTextIter end_iter;
	GtkEmbeddedTag *tag;
	gint length = 0;
	gint len = 0;
	gint nlen = 0;
	gint i = 0;
	gint j = 0;

	DEBUG (g_message ("check_embedded (%d, %d)",
			  gtk_text_iter_get_offset (iter1),
			  gtk_text_iter_get_offset (iter2)));

	buf = GTK_TEXT_BUFFER (sbuf);
	list = gtk_source_buffer_get_embedded_entries (sbuf);
	if (!list) {
		check_syntax (sbuf, iter1, iter2);
		return;
	}

	text = gtk_text_buffer_get_slice (buf, iter1, iter2, TRUE);
	length = strlen (text);

	start_iter = *iter1;
	cur_iter = start_iter;

	for (i = 0; i < length; i++) {
		for (list = gtk_source_buffer_get_embedded_entries (sbuf); list; list = list->next) {
			tag = GTK_EMBEDDED_TAG (list->data);
			if ((len = gtk_source_buffer_regex_match (text, i, length,
								  &tag->reg_outside)) > 0) {
				/*
				   we have found the outside regex inside the current buffer
				   now we have to go about detecting if there is anything *inside*
				   of out new range that should be highlighted
				 */
				for (j = i; j < (i + len); j++) {
					if ((nlen = gtk_source_buffer_regex_match (text, j, j + len,
										   &tag->reg_inside)) > 0) {
						end_iter = cur_iter;
						gtk_text_iter_forward_chars (&end_iter, nlen);
						g_print ("Embedded item found at position %d with length %d.\n", j, nlen);
						gtk_text_buffer_apply_tag (buf, GTK_TEXT_TAG (tag),
									   &cur_iter, &end_iter);
						/* we're about to j++ so make sure we compensate now */
						j += (nlen - 1);
						/* dont compensate here tho */
						gtk_text_iter_forward_chars (&cur_iter, nlen);
					} else {
						gtk_text_iter_forward_char (&cur_iter);
					}
				}
			}
		}
		gtk_text_iter_forward_char (&start_iter);
	}
	g_free (text);
	check_syntax (sbuf, iter1, iter2);
}

static void
check_syntax (GtkSourceBuffer *sbuf,
	      GtkTextIter     *iter1,
	      GtkTextIter     *iter2)
{
	GtkTextBuffer *buf;
	GList *list;
	gchar *txt;
	GtkTextIter curiter;
	GtkTextIter curiter2;
	GtkTextIter iterrealend;
	GtkSyntaxTag *tag;
	gint s = 0;
	gint pos = 0;
	gint oldpos = 0;
	gint z = 0;
	gint len = 0;
	gint offset = 0;
	gboolean found = FALSE;
	GtkSourceBufferMatch m;
	GtkSourceBufferPrivate *priv;

	buf = GTK_TEXT_BUFFER (sbuf);
	priv = sbuf->priv;

	gtk_text_buffer_get_end_iter (buf, &iterrealend);
	txt = gtk_text_buffer_get_slice (buf, iter1, &iterrealend, TRUE);

	list = gtk_source_buffer_get_syntax_entries (sbuf);
	if (!list) {
		/* FIXME: Here we may need to do some modifications
		   to properly check the pattern entries when no
		   syntax entries exist. However, at this point it
		   is my contention that it cannot be dont properly
		   check_pattern(sbuf, txt, len - pos, &curiter); */
		return;
	}

	curiter = *iter1;
	curiter2 = curiter;
	offset = gtk_text_iter_get_offset (iter1);
	len = gtk_text_iter_get_offset (iter2);
	len -= offset;

	while (pos < len) {
		/* check for any of the syntax highlights */
		s = gtk_source_buffer_regex_search (txt, pos, &priv->reg_syntax_all, TRUE, &m);
		if (s < 0 || s > len)
			break;	/* not found */
		/* if there is text segments before syntax, check pattern too... */
		if (pos < s)
			check_pattern (sbuf, &txt[pos], s - pos, &curiter);
		pos = m.endpos;
		gtk_text_iter_forward_chars (&curiter, m.endpos - oldpos);
		oldpos = pos;

		for (list = gtk_source_buffer_get_syntax_entries (sbuf); list; list = list->next) {
			tag = GTK_SYNTAX_TAG (list->data);
			if (gtk_source_buffer_regex_match (txt, s, len, &tag->reg_start) > 0
			    && txt[s - 1] != '\\') {
				if ((z = get_syntax_end (txt, pos, &tag->reg_end, &m)) >= 0)
					pos = m.endpos;
				else if (z == 0)
					continue;
				else
					pos = gtk_text_buffer_get_char_count (buf) - offset;
				/* g_print("applied syntax tag [%s] at startpos %d endpos %d\n",sentry->key,s,i); */
				gtk_text_iter_set_offset (&curiter, s + offset);
				curiter2 = curiter;
				gtk_text_iter_forward_chars (&curiter2, pos - s);

				/* make sure ALL tags between syntax start/end are removed */
				/* we only remove if syntax start/end is more than endpos */
				if (s > len + offset || pos > len + offset) {
					g_print ("remove all tags between %d and %d\n", s, pos);
					gtk_text_buffer_remove_all_tags (buf, &curiter, &curiter2);
				}
				gtk_text_buffer_apply_tag (buf, GTK_TEXT_TAG (tag), &curiter,
							   &curiter2);
				curiter = curiter2;
				found = TRUE;
				break;
			} else if (txt[s - 1] == '\\') {
				found = TRUE;
			}
		}
		if (!found) {
			pos++;
			gtk_text_iter_forward_chars (&curiter, 1);
		}
	}
	if (pos < len)
		check_pattern (sbuf, g_utf8_offset_to_pointer (txt, pos),
			       len - pos, &curiter);
	if (txt)
		g_free (txt);
}

static void
check_pattern (GtkSourceBuffer *sbuf,
	       const gchar     *text,
	       gint             length,
	       GtkTextIter     *iter)
{
	GtkTextBuffer *buf;
	GList *patterns = NULL, *l;
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	GtkPatternTag *tag = NULL;
	gint i;
	GtkSourceBufferMatch m;

	buf = GTK_TEXT_BUFFER (sbuf);
	patterns = gtk_source_buffer_get_pattern_entries (sbuf);
	if (!patterns)
		return;

	for (l = patterns; l; l = l->next) {
		tag = GTK_PATTERN_TAG (l->data);
		start_iter = *iter;

		i = 0;
		while (i < length && i >= 0) {
			i = gtk_source_buffer_regex_search (text, i, &tag->reg_pattern, TRUE, &m);
			if (i >= 0) {
				if (m.endpos == i) {
					g_warning ("Zero length regex match. "
					           "Probably a buggy syntax specification.");
					i++;
					continue;
				}

				gtk_text_iter_set_offset (&start_iter,
							  gtk_text_iter_get_offset (iter) + i);
				end_iter = start_iter;
				gtk_text_iter_forward_chars (&end_iter, m.endpos - i);
				gtk_text_buffer_apply_tag (buf, GTK_TEXT_TAG (tag),
							   &start_iter,
							   &end_iter);
				i = m.endpos;
			}
		}
	}
}


static gint
get_syntax_end (const gchar          *text,
		gint                  pos,
		Regex                *regex,
		GtkSourceBufferMatch *match)
{
	int ret = pos;

	do {
		ret = gtk_source_buffer_regex_search (text,
						      match->endpos,
						      regex,
						      TRUE,
						      match);
		if (ret < 0)
			return (-1);
	} while (match->endpos && text[match->endpos - 2] == '\\');

	return ret;
}

static gboolean
read_loop (GtkTextBuffer *buffer, 
	   const char    *filename,
	   GIOChannel    *io,
	   GError       **error)
{
	GIOStatus status;
	GtkWidget *widget;
	GtkTextIter end;
	gchar *str = NULL;
	gint size = 0;
	*error = NULL;

	gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (buffer),
					      &end);
	if ((status=g_io_channel_read_line (io, &str, &size, NULL, error)) == G_IO_STATUS_NORMAL && size)  {
		#ifdef DEBUG_SOURCEVIEW
		puts(str);
		#endif
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
						&end, str, size);
		g_free(str);
		return TRUE;
	}
	else if(!*error && (status=g_io_channel_read_to_end (io, &str, &size, error)) == G_IO_STATUS_NORMAL && size)	
	{
		#ifdef DEBUG_SOURCEVIEW
		puts(str);
		#endif
		gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer),
					&end, str, size);
		g_free(str);

		return TRUE;
	}

	if (status == G_IO_STATUS_EOF && !*error)
		return FALSE;

	if (!*error)
		return FALSE;

	widget = gtk_message_dialog_new (NULL,
					 (GtkDialogFlags) 0,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 "%s\nFile %s",
					 (*error)->message,
					 filename);
	gtk_dialog_run (GTK_DIALOG (widget));
	gtk_widget_destroy (widget);

		/* because of error in input we clear already loaded text */
	gtk_text_buffer_set_text(buffer, "", 0);

	return FALSE;
}

static void
add_marker (gpointer data,
	    gpointer user_data)
{
	char *name = data;
	MarkerSubList *sublist = user_data;
	GtkSourceBufferMarker *marker;

	marker = g_new0 (GtkSourceBufferMarker, 1);
	marker->line = sublist->line;
	marker->name = g_strdup (name);

	sublist->marker_list = g_list_append (sublist->marker_list, marker);
}

static void
add_markers (gpointer key,
	     gpointer value,
	     gpointer user_data)
{
	GList **list = user_data;
	GList *markers = value;
	MarkerSubList *sublist;

	sublist = g_new0 (MarkerSubList, 1);
	sublist->line = GPOINTER_TO_INT (key);
	sublist->marker_list = *list;

	g_list_foreach (markers, add_marker, sublist);

	g_free (sublist);
}

/* ----------------------------------------------------------------------
 * Public interface 
 * ---------------------------------------------------------------------- */

GType
gtk_source_buffer_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceBufferClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) gtk_source_buffer_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (GtkSourceBuffer),
			0,              /* n_preallocs */
			(GInstanceInitFunc) gtk_source_buffer_init
		};

		our_type = g_type_register_static (GTK_TYPE_TEXT_BUFFER,
						   "GtkSourceBuffer",
						   &our_info,
						   0);
	}
	return our_type;
}

GtkSourceBuffer *
gtk_source_buffer_new (GtkTextTagTable *table)
{
	GtkSourceBuffer *buffer;
	GtkSourceBufferPrivate *priv;

	buffer = GTK_SOURCE_BUFFER (g_object_new (GTK_TYPE_SOURCE_BUFFER, NULL));
	priv = buffer->priv;

	if (table) {
		GTK_TEXT_BUFFER (buffer)->tag_table = table;
		g_object_ref (G_OBJECT (GTK_TEXT_BUFFER (buffer)->tag_table));
	} else
		GTK_TEXT_BUFFER (buffer)->tag_table = gtk_text_tag_table_new ();

	priv->bracket_match_tag = gtk_text_tag_new ("bracket-match");

	g_object_set (G_OBJECT (priv->bracket_match_tag),
		      "foreground_gdk",
		      "blue",
		      NULL);
	g_object_set (G_OBJECT (priv->bracket_match_tag),
		      "background_gdk",
		      "gray",
		      NULL);

	gtk_text_tag_table_add (GTK_TEXT_BUFFER (buffer)->tag_table,
				priv->bracket_match_tag);

	return buffer;
}

void
gtk_source_buffer_attach_to_view (GtkSourceBuffer *buffer,
				  GtkTextView     *view)
{
	GtkWidget *widget = GTK_WIDGET (view);
	GtkSourceBufferPrivate *priv = buffer->priv;

	if (priv->bracket_match_tag)
		gtk_text_tag_table_remove (GTK_TEXT_BUFFER (buffer)->tag_table,
					   priv->bracket_match_tag);

	priv->bracket_match_tag = gtk_text_tag_new ("bracket-match");

	g_object_set (G_OBJECT (priv->bracket_match_tag),
		      "foreground_gdk",
		      &widget->style->fg[GTK_STATE_SELECTED],
		      NULL);
	g_object_set (G_OBJECT (priv->bracket_match_tag),
		      "background_gdk",
		      &widget->style->bg[GTK_STATE_SELECTED],
		      NULL);

	gtk_text_tag_table_add (GTK_TEXT_BUFFER (buffer)->tag_table,
				priv->bracket_match_tag);
}

void 
gtk_source_buffer_highlight_region (GtkSourceBuffer *sbuf,
				    GtkTextIter     *start,
				    GtkTextIter     *end)
{
	GtkTextRegion *region;
	
	g_return_if_fail (sbuf != NULL && start != NULL && end != NULL);

	if (!sbuf->priv->highlight)
		return;

	/* get the subregions not yet highlighted */
	region = gtk_text_region_intersect (sbuf->priv->refresh_region, start, end);
	if (region) {
		GtkTextIter iter1, iter2;
		gint i;
		
		/* highlight all subregions from the intersection.
                   hopefully this will only be one subregion */
		for (i = 0; i < gtk_text_region_subregions (region); i++) {
			gtk_text_region_nth_subregion (region, i, &iter1, &iter2);
			DEBUG (g_message ("Actually hightlighting from %d to %d",
					  gtk_text_iter_get_offset (&iter1),
					  gtk_text_iter_get_offset (&iter2)));
			check_embedded (sbuf, &iter1, &iter2);
		}
		gtk_text_region_destroy (region);
		/* remove the just highlighted region */
		gtk_text_region_substract (sbuf->priv->refresh_region, start, end);
	}
}

gint
gtk_source_buffer_regex_search (const gchar          *text,
				gint                  pos,
				Regex                *regex,
				gboolean              forward,
				GtkSourceBufferMatch *match)
{
	gint len;
	int diff;

	g_return_val_if_fail (regex != NULL, -1);
	g_return_val_if_fail (match != NULL, -1);

	/* Work around a re_search bug where it returns the number of bytes
	 * instead of the number of characters (unicode characters can be
	 * more than 1 byte) it matched. See redhat bugzilla #73644. */
	len = (gint)g_utf8_strlen (text, -1);
	pos += (pos - (gint)g_utf8_strlen (text, pos));

	match->startpos = re_search (&regex->buf,
				     text,
				     len,
				     pos,
				     (forward ? len - pos : -pos),
				     &regex->reg);

	if (match->startpos > -1) {
		len = (gint)g_utf8_strlen (text, match->startpos);
		diff = match->startpos - len;
		match->startpos = len;
		match->endpos = regex->reg.end[0] - diff;
	}

	return match->startpos;
}

/* regex_match -- tries to match regex at the 'pos' position in the text. It 
 * returns the number of chars matched, or -1 if no match. 
 * Warning: The number matched can be 0, if the regex matches the empty string.
 * The reason for workin on GtkSCText is the same as in regex_search.
 */
gint
gtk_source_buffer_regex_match (const gchar *text,
			       gint         pos,
			       gint         end,
			       Regex       *regex)
{
	g_return_val_if_fail (regex != NULL, -1);

	return re_match (&regex->buf, text, end, pos, &regex->reg);
}

GList *
gtk_source_buffer_get_regex_tags (GtkSourceBuffer *buffer)
{
	GList *list = NULL;
	GtkTextTagTable *table;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	gtk_text_tag_table_foreach (table, get_tags_func, &list);
	list = g_list_first (list);

	return list;
}

void
gtk_source_buffer_purge_regex_tags (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv;
	GtkTextTagTable *table;
	GList *list;
	GList *cur;
	GtkTextIter iter1;
	GtkTextIter iter2;
	gchar *name;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	priv = buffer->priv;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer),
				    &iter1, &iter2);
	gtk_text_buffer_remove_all_tags (GTK_TEXT_BUFFER (buffer),
					 &iter1, &iter2);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	list = gtk_source_buffer_get_regex_tags (buffer);

	for (cur = list; cur; cur = cur->next) {
		g_object_get (G_OBJECT (cur->data), "name", &name, NULL);
		g_free (name);
		gtk_text_tag_table_remove (table, GTK_TEXT_TAG (cur->data));
	}

	g_list_free (list);

	if (priv->syntax_items) {
		g_list_free (priv->syntax_items);
		priv->syntax_items = NULL;
	}
	if (priv->pattern_items) {
		g_list_free (priv->pattern_items);
		priv->pattern_items = NULL;
	}
	if (priv->embedded_items) {
		g_list_free (priv->embedded_items);
		priv->embedded_items = NULL;
	}
}

void
gtk_source_buffer_install_regex_tags (GtkSourceBuffer *buffer,
				      GList           *entries)
{
	GtkSourceBufferPrivate *priv = buffer->priv;
	GtkTextTag *tag;
	GList *cur;
	gchar *name;

	for (cur = entries; cur; cur = cur->next) {
		g_object_get (G_OBJECT (cur->data), "name", &name, NULL);

		if (name) {
			tag = gtk_text_tag_table_lookup (GTK_TEXT_BUFFER (buffer)->tag_table,
							 name);
			if (tag)
				gtk_text_tag_table_remove (GTK_TEXT_BUFFER (buffer)->tag_table,
							   tag);
		}

		if (GTK_IS_SYNTAX_TAG (cur->data)) {
			priv->syntax_items = g_list_append (priv->syntax_items, cur->data);
			gtk_text_tag_table_add (GTK_TEXT_BUFFER (buffer)->tag_table,
						GTK_TEXT_TAG (cur->data));
		} else if (GTK_IS_PATTERN_TAG (cur->data)) {
			priv->pattern_items = g_list_append (priv->pattern_items, cur->data);
			gtk_text_tag_table_add (GTK_TEXT_BUFFER (buffer)->tag_table,
						GTK_TEXT_TAG (cur->data));
			/* lower priority for pattern tags */
			gtk_text_tag_set_priority (GTK_TEXT_TAG (cur->data), 0);
		} else if (GTK_IS_EMBEDDED_TAG (cur->data)) {
			priv->embedded_items = g_list_append (priv->embedded_items, cur->data);
			gtk_text_tag_table_add (GTK_TEXT_BUFFER (buffer)->tag_table,
						GTK_TEXT_TAG (cur->data));
		}

		if (name)
			g_free (name);
	}

	if (priv->syntax_items)
		gtk_source_buffer_sync_syntax_regex (buffer);
}

void
gtk_source_buffer_sync_syntax_regex (GtkSourceBuffer *buffer)
{
	GString *str;
	GList *cur;
	GtkSyntaxTag *tag;

	str = g_string_new ("");
	cur = buffer->priv->syntax_items;

	while (cur) {
		if (!GTK_IS_SYNTAX_TAG (cur->data)) {
			g_warning ("Serious error: there is a member in the"
				   " syntax_items list that is not a syntax tag\n");
			return;
		}

		tag = GTK_SYNTAX_TAG (cur->data);
		g_string_append (str, tag->start);
		cur = cur->next;
		if (cur)
			g_string_append (str, "\\|");
	}

	gtk_source_compile_regex (str->str, &buffer->priv->reg_syntax_all);

	g_string_free (str, TRUE);
}

GtkSyntaxTag *
gtk_source_buffer_iter_has_syntax_tag (GtkTextIter *iter)
{
	GSList *list = gtk_text_iter_get_tags (iter);

	while (list) {
		if (GTK_IS_SYNTAX_TAG (list->data))
			return GTK_SYNTAX_TAG (list->data);
		list = g_slist_next (list);
	}

	return NULL;
}

GList *
gtk_source_buffer_get_syntax_entries (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);

	return buffer->priv->syntax_items;
}

GList *
gtk_source_buffer_get_pattern_entries (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);

	return buffer->priv->pattern_items;
}

GList *
gtk_source_buffer_get_embedded_entries (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);

	return buffer->priv->embedded_items;
}

gchar *
gtk_source_buffer_convert_to_html (GtkSourceBuffer *buffer,
				   const gchar     *title)
{
	gchar txt[3];
	GtkTextIter iter;
	gboolean font = FALSE;
	gboolean bold = FALSE;
	gboolean italic = FALSE;
	gboolean underline = FALSE;
	GString *str = NULL;
	GSList *list = NULL;
	GtkTextTag *tag = NULL;
	GdkColor *col = NULL;
	GValue *value;
	gunichar c = 0;

	txt[1] = 0;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 0);

	str = g_string_new ("<html>\n");
	g_string_append (str, "<head>\n");
	g_string_sprintfa (str, "<title>%s</title>\n",
			   title ? title : "GtkSourceView converter");
	g_string_append (str, "</head>\n");
	g_string_append (str, "<body bgcolor=white>\n");
	g_string_append (str, "<pre>");


	while (!gtk_text_iter_is_end (&iter)) {
		c = gtk_text_iter_get_char (&iter);
		if (!tag) {
			list = gtk_text_iter_get_toggled_tags (&iter, TRUE);
			if (list && g_slist_last (list)->data) {
				tag = GTK_TEXT_TAG (g_slist_last (list)->data);
				g_slist_free (list);
			}
			if (tag && !gtk_text_iter_ends_tag (&iter, tag)) {
				GValue val1 = { 0, };
				GValue val2 = { 0, };
				GValue val3 = { 0, };

				value = &val1;
				g_value_init (value, GDK_TYPE_COLOR);
				g_object_get_property (G_OBJECT (tag), "foreground_gdk", value);
				col = g_value_get_boxed (value);
				if (col) {
					g_string_sprintfa (str, "<font color=#%02X%02X%02X>",
							   col->red / 256, col->green / 256,
							   col->blue / 256);
					font = TRUE;
				}
				value = &val2;
				g_value_init (value, G_TYPE_INT);
				g_object_get_property (G_OBJECT (tag), "weight", value);
				if (g_value_get_int (value) == PANGO_WEIGHT_BOLD) {
					g_string_append (str, "<b>");
					bold = TRUE;
				}

				value = &val3;
				g_value_init (value, PANGO_TYPE_STYLE);
				g_object_get_property (G_OBJECT (tag), "style", value);
				if (g_value_get_enum (value) == PANGO_STYLE_ITALIC) {
					g_string_append (str, "<i>");
					italic = TRUE;
				}

			}
		}

		if (c == '<')
			g_string_append (str, "&lt");
		else if (c == '>')
			g_string_append (str, "&gt");
		else {
			txt[0] = c;
			g_string_append (str, txt);
		}

		gtk_text_iter_forward_char (&iter);
		if (tag && gtk_text_iter_ends_tag (&iter, tag)) {
			if (bold)
				g_string_append (str, "</b>");
			if (italic)
				g_string_append (str, "</i>");
			if (underline)
				g_string_append (str, "</u>");
			if (font)
				g_string_append (str, "</font>");
			tag = NULL;
			bold = italic = underline = font = FALSE;
		}
	}
	g_string_append (str, "</pre>");
	g_string_append (str, "</body>");
	g_string_append (str, "</html>");

	return g_string_free (str, FALSE);
}

gboolean
gtk_source_buffer_find_bracket_match (GtkTextIter *orig)
{
	GtkTextIter iter = *orig;
	gunichar base_char = 0;
	gunichar search_char = 0;
	gunichar cur_char = 0;
	gint addition = -1;
	gint counter = 0;
	gboolean found = FALSE;

	gtk_text_iter_backward_char (&iter);
	cur_char = gtk_text_iter_get_char (&iter);

	base_char = cur_char;
	switch ((int) base_char) {
	case '{':
		addition = 1;
		search_char = '}';
		break;
	case '(':
		addition = 1;
		search_char = ')';
		break;
	case '[':
		addition = 1;
		search_char = ']';
		break;
	case '<':
		addition = 1;
		search_char = '>';
		break;
	case '}':
		addition = -1;
		search_char = '{';
		break;
	case ')':
		addition = -1;
		search_char = '(';
		break;
	case ']':
		addition = -1;
		search_char = '[';
		break;
	case '>':
		addition = -1;
		search_char = '<';
		break;
	default:
		addition = 0;
		break;
	}

	if (!addition)
		return (FALSE);

	do {
		gtk_text_iter_forward_chars (&iter, addition);

		cur_char = gtk_text_iter_get_char (&iter);
		if (cur_char == search_char && !counter) {
			found = TRUE;
			break;
		}
		if (cur_char == base_char)
			counter++;
		else if (cur_char == search_char)
			counter--;
	}
	while (!gtk_text_iter_is_end (&iter) && !gtk_text_iter_is_start (&iter));

	if (found)
		*orig = iter;

	return found;
}

gboolean
gtk_source_buffer_can_undo (const GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);
	g_return_val_if_fail (buffer->priv != NULL, FALSE);

	return gtk_undo_manager_can_undo (buffer->priv->undo_manager);
}

gboolean
gtk_source_buffer_can_redo (const GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);
	g_return_val_if_fail (buffer->priv != NULL, FALSE);

	return gtk_undo_manager_can_redo (buffer->priv->undo_manager);
}

void
gtk_source_buffer_undo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (buffer->priv != NULL);
	g_return_if_fail (gtk_undo_manager_can_undo (buffer->priv->undo_manager));

	gtk_undo_manager_undo (buffer->priv->undo_manager);
}

void
gtk_source_buffer_redo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (buffer->priv != NULL);
	g_return_if_fail (gtk_undo_manager_can_redo (buffer->priv->undo_manager));

	gtk_undo_manager_redo (buffer->priv->undo_manager);
}

int
gtk_source_buffer_get_undo_levels (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), 0);
	g_return_val_if_fail (buffer->priv != NULL, 0);

	return gtk_undo_manager_get_undo_levels (buffer->priv->undo_manager);
}

void
gtk_source_buffer_set_undo_levels (GtkSourceBuffer *buffer,
				   int              undo_levels)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (buffer->priv != NULL);

	gtk_undo_manager_set_undo_levels (buffer->priv->undo_manager, undo_levels);
}

void
gtk_source_buffer_begin_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (buffer->priv != NULL);

	gtk_undo_manager_begin_not_undoable_action (buffer->priv->undo_manager);
}

void
gtk_source_buffer_end_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (buffer->priv != NULL);

	gtk_undo_manager_end_not_undoable_action (buffer->priv->undo_manager);
}

/* Add a marker to a line.
 * If the list doesnt already exist, it will call set_marker. If the list does
 * exist, the new marker will be appended. If the marker already exists, it will
 * be removed from its current order and then prepended.
 */
void
gtk_source_buffer_line_add_marker (GtkSourceBuffer *buffer,
				   gint             line,
				   const gchar     *marker)
{
	GtkSourceBufferPrivate *priv;
	GList *list = NULL;
	GList *iter;
	gint line_count;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	priv = buffer->priv;

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (line_count > line);

	list = (GList *) g_hash_table_lookup (priv->line_markers,
					      GINT_TO_POINTER (line));
	if (list && marker) {
		for (iter = list; iter; iter = iter->next) {
			if (iter->data && !strcmp (marker, (gchar *) iter->data)) {
				list = g_list_remove (list, iter->data);
				g_free (iter->data);
				break;
			}
		}

		g_hash_table_remove (priv->line_markers,
				     GINT_TO_POINTER (line));
		list = g_list_append (list, g_strdup (marker));
		g_hash_table_insert (priv->line_markers,
				     GINT_TO_POINTER (line),
				     list);
	} else if (marker)
		gtk_source_buffer_line_set_marker (buffer, line, marker);
}

void
gtk_source_buffer_line_set_marker (GtkSourceBuffer *buffer,
				   gint             line,
				   const gchar     *marker)
{
	GList *new_list = NULL;
	gint line_count;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (line_count > line);

	gtk_source_buffer_line_remove_markers (buffer, line);
	if (marker) {
		new_list = g_list_append (new_list, g_strdup (marker));
		g_hash_table_insert (buffer->priv->line_markers,
				     GINT_TO_POINTER (line),
				     new_list);
	}
}

gboolean
gtk_source_buffer_line_remove_marker (GtkSourceBuffer *buffer,
				      gint             line,
				      const gchar     *marker)
{
	gboolean removed = FALSE;
	GList *list;
	GList *iter;
	gint line_count;

	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	if (line > line_count)
		return FALSE;

	list = (GList *) g_hash_table_lookup (buffer->priv->line_markers,
					      GINT_TO_POINTER (line));
	for (iter = list; iter; iter = iter->next) {
		if (iter->data && !strcmp (marker, (gchar *) iter->data)) {
			list = g_list_remove (list, iter->data);

			g_hash_table_insert (buffer->priv->line_markers,
					     GINT_TO_POINTER (line),
					     list);
			removed = TRUE;
			break;
		}
	}

	return removed;
}

const GList *
gtk_source_buffer_line_get_markers (GtkSourceBuffer *buffer,
				    gint             line)
{
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	return (const GList *) g_hash_table_lookup (buffer->priv->line_markers,
						    GINT_TO_POINTER (line));
}

gint
gtk_source_buffer_line_has_markers (GtkSourceBuffer *buffer,
				    gint             line)
{
	gpointer data;
	gint count = 0;

	g_return_val_if_fail (buffer != NULL, 0);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), 0);

	data = g_hash_table_lookup (buffer->priv->line_markers,
				    GINT_TO_POINTER (line));

	if (data)
		count = g_list_length ((GList *) data);

	return count;
}

gint
gtk_source_buffer_line_remove_markers (GtkSourceBuffer *buffer,
				       gint             line)
{
	GList *list = NULL;
	GList *iter = NULL;
	gint remove_count = 0;
	gint line_count = 0;

	g_return_val_if_fail (buffer != NULL, 0);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), 0);

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	if (line > line_count)
		return 0;

	list = (GList *) g_hash_table_lookup (buffer->priv->line_markers,
					      GINT_TO_POINTER (line));
	if (list) {
		for (iter = list; iter; iter = iter->next) {
			if (iter->data)
				g_free (iter->data);
			remove_count++;
		}
		g_hash_table_remove (buffer->priv->line_markers,
				     GINT_TO_POINTER (line));
		g_list_free (list);
	}

	return remove_count;
}

GList *
gtk_source_buffer_get_all_markers (GtkSourceBuffer *buffer)
{
	GList *list = NULL;

	g_hash_table_foreach (buffer->priv->line_markers, add_markers, &list);

	return list;
}

gint
gtk_source_buffer_remove_all_markers (GtkSourceBuffer *buffer,
				      gint             line_start,
				      gint line_end)
{
	gint remove_count = 0;
	gint line_count;
	gint counter;

	g_return_val_if_fail (buffer != NULL, 0);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), 0);

	line_count = gtk_text_buffer_get_line_count (GTK_TEXT_BUFFER (buffer));
	line_start = line_start < 0 ? 0 : line_start;
	line_end = line_end > line_count ? line_count : line_end;
	
	for (counter = line_start; counter <= line_end; counter++)
		remove_count += gtk_source_buffer_line_remove_markers (buffer,
								       counter);

	return remove_count;
}

void
gtk_source_buffer_set_check_brackets (GtkSourceBuffer *buffer,
				      gboolean         check_brackets)
{
	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	buffer->priv->check_brackets = check_brackets;
}

gboolean
gtk_source_buffer_get_highlight (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->highlight;
}

void
gtk_source_buffer_set_highlight (GtkSourceBuffer *buffer,
				 gboolean         highlight)
{
	GtkTextIter iter1;
	GtkTextIter iter2;

	g_return_if_fail (buffer != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	buffer->priv->highlight = highlight;

	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &iter1, &iter2);

	if (highlight)
		refresh_range (buffer, &iter1, &iter2);
	else {
		if (buffer->priv->refresh_idle_handler) {
			g_source_remove (buffer->priv->refresh_idle_handler);
			buffer->priv->refresh_idle_handler = 0;
		}
		gtk_text_buffer_remove_all_tags (GTK_TEXT_BUFFER (buffer),
						 &iter1,
						 &iter2);
	}
}

gboolean
gtk_source_buffer_load (GtkSourceBuffer *buffer,
			const gchar     *filename, 
			GError **error)
{
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return gtk_source_buffer_load_with_character_encoding (buffer,
							       filename,
							       NULL, 
								error);  
}

gboolean
gtk_source_buffer_save (GtkSourceBuffer *buffer,
			const gchar     *filename,
			GError **error)
{
	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return gtk_source_buffer_save_with_character_encoding (buffer,
							       filename,
							       "UTF-8", 
								error);  
}

gboolean
gtk_source_buffer_load_with_character_encoding (GtkSourceBuffer *buffer,
						const gchar     *filename,
						const gchar     *encoding,
						GError **error)
{
	GIOChannel *io;
	GtkWidget *widget;
	gboolean highlight = FALSE;
	*error = NULL;

	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	highlight = gtk_source_buffer_get_highlight (buffer);

	io = g_io_channel_new_file (filename, "r", error);
	if (!io) {
		widget = gtk_message_dialog_new (NULL,
						 (GtkDialogFlags) 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 "%s\nFile %s",
						 (*error)->message,
						 filename);
		gtk_dialog_run (GTK_DIALOG (widget));
		gtk_widget_destroy (widget);

		return FALSE;
	}

	if (g_io_channel_set_encoding (io, encoding, error) != G_IO_STATUS_NORMAL)  {
		widget = gtk_message_dialog_new (NULL,
						 (GtkDialogFlags) 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Failed to set encoding:\n%s\n%s"),
						 filename,
						 (*error)->message);

		gtk_dialog_run (GTK_DIALOG (widget));
		gtk_widget_destroy (widget);
		g_io_channel_unref (io);

		return FALSE;
	}

	if (highlight)  
		gtk_source_buffer_set_highlight (buffer, FALSE);

	gtk_source_buffer_begin_not_undoable_action (buffer);

	while (!*error && read_loop (GTK_TEXT_BUFFER(buffer), filename, io, error) );	

	gtk_source_buffer_end_not_undoable_action (buffer);

	g_io_channel_unref (io);
	
	if (*error)
		return FALSE;

	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (buffer), FALSE);

	if (highlight)  
		gtk_source_buffer_set_highlight (buffer, TRUE);

	return TRUE;
}

gboolean
gtk_source_buffer_save_with_character_encoding (GtkSourceBuffer *buffer,
						const gchar     *filename,
						const gchar     *encoding,
						GError	**error)
{
	GIOChannel *io;
	GtkTextIter iter, iterend;
	gchar *buf;
	gboolean more = FALSE;
	gsize length=0;
	*error=NULL;


	g_return_val_if_fail (buffer != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

      if (encoding && !*encoding)
        encoding = NULL;

	io=g_io_channel_new_file(filename, "w+", error);
	if (!io)  {
		GtkWidget *w = gtk_message_dialog_new (NULL,
						       (GtkDialogFlags) 0,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       _("Failed to create file:\n%s\n%s"),
						       filename,
						       (*error)->message);

		gtk_dialog_run (GTK_DIALOG(w));
		gtk_widget_destroy (w);

		return FALSE;
	}

	if (encoding && g_io_channel_set_encoding (io, encoding , error) != G_IO_STATUS_NORMAL)  {
		GtkWidget *w = gtk_message_dialog_new (NULL,
						       (GtkDialogFlags) 0,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       _("Failed to set encoding:\n%s\n%s"),
						       filename,
						       (*error)->message);

		gtk_dialog_run (GTK_DIALOG(w));
		gtk_widget_destroy (w);
		g_io_channel_unref (io);

		return FALSE;
	}

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER (buffer), &iter);
	iterend=iter;
	do {
		more = gtk_text_iter_forward_line(&iterend);
		buf = gtk_text_iter_get_text(&iter, &iterend);
		if (g_io_channel_write_chars(io, buf, -1, &length, error) != G_IO_STATUS_NORMAL) {
			GtkWidget *w = gtk_message_dialog_new (NULL,
							       (GtkDialogFlags) 0,
							       GTK_MESSAGE_ERROR,
							       GTK_BUTTONS_OK,
							       _("Failed to write characters to file:\n%s\n%s"),
							       filename,
							       (*error)->message);

			gtk_dialog_run (GTK_DIALOG (w));
			gtk_widget_destroy (w);
			g_io_channel_unref (io);

			return FALSE;
		}

		g_free (buf);
		iter = iterend;
	} while (more);

	if (g_io_channel_flush(io, error) != G_IO_STATUS_NORMAL) {
		GtkWidget *w = gtk_message_dialog_new (NULL,
						       (GtkDialogFlags) 0,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_OK,
						       _("Failed to write end line to file:\n%s\n%s"),
						       filename,
						       (*error)->message);

		gtk_dialog_run (GTK_DIALOG (w));
		gtk_widget_destroy (w);
		g_io_channel_unref (io);

		return FALSE;
	}

	g_io_channel_unref (io);
	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (buffer), FALSE);

	return TRUE;
}

	

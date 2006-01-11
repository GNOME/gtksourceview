/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcebuffer.c
 *
 *  Copyright (C) 1999,2000,2001,2002 by:
 *          Mikael Hermansson <tyan@linux.se>
 *          Chris Phelps <chicane@reninet.com>
 *          Jeroen Zwartepoorte <jeroen@xs4all.nl>
 *          
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *          Gustavo Giráldez <gustavo.giraldez@gmx.net>
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

#include "gtksourcebuffer.h"
#include "gtksourcetag.h"
#include "gtksourcetag-private.h"
#include "gtksourceundomanager.h"
#include "gtksourceview-marshal.h"
#include "gtktextregion.h"
#include "gtksourceiter.h"

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

/* define this to always highlight in an idle handler, and not
 * possibly in the expose method of the view */
#undef LAZIEST_MODE

/* in milliseconds */
#define WORKER_TIME_SLICE                   30
#define INITIAL_WORKER_BATCH                40960
#define MINIMUM_WORKER_BATCH                1024

#define MAX_CHARS_BEFORE_FINDING_A_MATCH    2000

typedef struct _SyntaxDelimiter      SyntaxDelimiter;
typedef struct _PatternMatch         PatternMatch;

/* Signals */
enum {
	CAN_UNDO = 0,
	CAN_REDO,
	HIGHLIGHT_UPDATED,
	MARKER_UPDATED,
	LAST_SIGNAL
};

/* Properties */
enum {
	PROP_0,
	PROP_ESCAPE_CHAR,
	PROP_CHECK_BRACKETS,
	PROP_HIGHLIGHT,
	PROP_MAX_UNDO_LEVELS,
	PROP_LANGUAGE
};

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

struct _GtkSourceBufferPrivate 
{
	gint                   highlight:1;
	gint                   check_brackets:1;

	GtkTextTag            *bracket_match_tag;
	GtkTextMark           *bracket_mark;
	guint                  bracket_found:1;
	
	GArray                *markers;

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

	GtkSourceLanguage     *language;

	GtkSourceUndoManager  *undo_manager;
};



static GtkTextBufferClass *parent_class = NULL;
static guint 	 buffer_signals[LAST_SIGNAL] = { 0 };

static void 	 gtk_source_buffer_class_init		(GtkSourceBufferClass    *klass);
static void 	 gtk_source_buffer_init			(GtkSourceBuffer         *klass);
static GObject  *gtk_source_buffer_constructor          (GType                    type,
							 guint                    n_construct_properties,
							 GObjectConstructParam   *construct_param);
static void 	 gtk_source_buffer_finalize		(GObject                 *object);
static void      gtk_source_buffer_set_property         (GObject                 *object,
							 guint                    prop_id,
							 const GValue            *value,
							 GParamSpec              *pspec);
static void      gtk_source_buffer_get_property         (GObject                 *object,
							 guint                    prop_id,
							 GValue                  *value,
							 GParamSpec              *pspec);

static void 	 gtk_source_buffer_can_undo_handler 	(GtkSourceUndoManager    *um,
							 gboolean                 can_undo,
							 GtkSourceBuffer         *buffer);
static void 	 gtk_source_buffer_can_redo_handler	(GtkSourceUndoManager    *um,
							 gboolean                 can_redo,
							 GtkSourceBuffer         *buffer);

static void 	 gtk_source_buffer_move_cursor		(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 GtkTextMark             *mark, 
							 gpointer                 data);

static void 	 gtk_source_buffer_real_insert_text 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 const gchar             *text,
							 gint                     len);
static void 	 gtk_source_buffer_real_delete_range 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 GtkTextIter             *end);

static const GtkSyntaxTag *iter_has_syntax_tag 		(const GtkTextIter       *iter);

static void 	 get_tags_func 				(GtkTextTag              *tag, 
		                                         gpointer                 data);

static void	 highlight_region 			(GtkSourceBuffer         *source_buffer,
		   					 GtkTextIter             *start, 
							 GtkTextIter             *end);

static GList 	*gtk_source_buffer_get_syntax_entries 	(const GtkSourceBuffer   *buffer);
static GList 	*gtk_source_buffer_get_pattern_entries 	(const GtkSourceBuffer   *buffer);

static void	 sync_syntax_regex 			(GtkSourceBuffer         *buffer);

static void      build_syntax_regions_table             (GtkSourceBuffer         *buffer,
							 const GtkTextIter       *needed_end);
static void      update_syntax_regions                  (GtkSourceBuffer         *source_buffer,
							 gint                     start,
							 gint                     delta);

static void      invalidate_syntax_regions              (GtkSourceBuffer         *source_buffer,
							 GtkTextIter             *from,
							 gint                     delta);
static void      refresh_range                          (GtkSourceBuffer         *buffer,
							 GtkTextIter             *start,
							 GtkTextIter             *end);
static void      ensure_highlighted                     (GtkSourceBuffer         *source_buffer,
							 const GtkTextIter       *start,
							 const GtkTextIter       *end);

static gboolean	 gtk_source_buffer_find_bracket_match_with_limit (GtkTextIter    *orig, 
								  gint            max_chars);

static void	 gtk_source_buffer_remove_all_source_tags (GtkSourceBuffer   *buffer,
					  		const GtkTextIter *start,
					  		const GtkTextIter *end);

static void	sync_with_tag_table 			(GtkSourceBuffer *buffer);


GType
gtk_source_buffer_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceBufferClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gtk_source_buffer_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceBuffer),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_buffer_init
		};

		our_type = g_type_register_static (GTK_TYPE_TEXT_BUFFER,
						   "GtkSourceBuffer",
						   &our_info, 
						   0);
	}
	
	return our_type;
}
	
static void
gtk_source_buffer_class_init (GtkSourceBufferClass *klass)
{
	GObjectClass        *object_class;
	GtkTextBufferClass  *tb_class;

	object_class 	= G_OBJECT_CLASS (klass);
	parent_class 	= g_type_class_peek_parent (klass);
	tb_class	= GTK_TEXT_BUFFER_CLASS (klass);
		
	object_class->constructor  = gtk_source_buffer_constructor;
	object_class->finalize	   = gtk_source_buffer_finalize;
	object_class->get_property = gtk_source_buffer_get_property;
	object_class->set_property = gtk_source_buffer_set_property;
	
	klass->can_undo 	 = NULL;
	klass->can_redo 	 = NULL;
	klass->highlight_updated = NULL;
	klass->marker_updated    = NULL;
	
	/* Do not set these signals handlers directly on the parent_class since
	 * that will cause problems (a loop). */
	tb_class->insert_text 	= gtk_source_buffer_real_insert_text;
	tb_class->delete_range 	= gtk_source_buffer_real_delete_range;

	g_object_class_install_property (object_class,
					 PROP_ESCAPE_CHAR,
					 g_param_spec_unichar ("escape_char",
							       _("Escape Character"),
							       _("Escaping character "
								 "for syntax patterns"),
							       0,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CHECK_BRACKETS,
					 g_param_spec_boolean ("check_brackets",
							       _("Check Brackets"),
							       _("Whether to check and "
								 "highlight matching brackets"),
							       TRUE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT,
					 g_param_spec_boolean ("highlight",
							       _("Highlight"),
							       _("Whether to highlight syntax "
								 "in the buffer"),
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_MAX_UNDO_LEVELS,
					 g_param_spec_int ("max_undo_levels",
							   _("Maximum Undo Levels"),
							   _("Number of undo levels for "
							     "the buffer"),
							   0,
							   200,
							   25,
							   G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_object ("language",
							      _("Language"),
							      _("Language object to get "
								"highlighting patterns from"),
							      GTK_TYPE_SOURCE_LANGUAGE,
							      G_PARAM_READWRITE));
	
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

	buffer_signals[HIGHLIGHT_UPDATED] =
	    g_signal_new ("highlight_updated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, highlight_updated),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED_BOXED,
			  G_TYPE_NONE, 
			  2, 
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);

	buffer_signals[MARKER_UPDATED] =
	    g_signal_new ("marker_updated",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, marker_updated),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED,
			  G_TYPE_NONE, 
			  1, 
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
gtk_source_buffer_init (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv;

	priv = g_new0 (GtkSourceBufferPrivate, 1);

	buffer->priv = priv;

	priv->undo_manager = gtk_source_undo_manager_new (GTK_TEXT_BUFFER (buffer));

	priv->check_brackets = TRUE;
	priv->bracket_mark = NULL;
	priv->bracket_found = FALSE;
	
	priv->markers = g_array_new (FALSE, FALSE, sizeof (GtkSourceMarker *));

	/* highlight data */
	priv->refresh_region =  gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
	priv->syntax_regions =  g_array_new (FALSE, FALSE,
					     sizeof (SyntaxDelimiter));
	priv->highlight_requests = gtk_text_region_new (GTK_TEXT_BUFFER (buffer));
	priv->worker_handler = 0;

	/* initially the buffer is empty so it's entirely analyzed */
	priv->worker_last_offset = -1;
	priv->worker_batch_size = INITIAL_WORKER_BATCH;
	
	g_signal_connect (G_OBJECT (buffer),
			  "mark_set",
			  G_CALLBACK (gtk_source_buffer_move_cursor),
			  NULL);

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
tag_added_or_removed_cb (GtkTextTagTable *table, GtkTextTag *tag, GtkSourceBuffer *buffer)
{
	sync_with_tag_table (buffer);
	
}

static void 
tag_table_changed_cb (GtkSourceTagTable *table, GtkSourceBuffer *buffer)
{
	sync_with_tag_table (buffer);
}

static GObject *
gtk_source_buffer_constructor (GType                  type,
			       guint                  n_construct_properties,
			       GObjectConstructParam *construct_param)
{
	GObject *g_object;
	gboolean tag_table_specified = FALSE;
	gint i;

	/* Check the construction parameters to see if the user
	 * specified a tag-table, and create and set an empty
	 * GtkSourceTagTable if he didn't */
	for (i = 0; i < n_construct_properties; i++)
	{
		if (!strcmp ("tag-table", construct_param [i].pspec->name))
		{
			if (g_value_get_object (construct_param [i].value) == NULL)
			{
#if (GLIB_MINOR_VERSION <= 2)
				g_value_set_object_take_ownership (construct_param [i].value,
								   gtk_source_tag_table_new ());
#else
				g_value_take_object (construct_param [i].value,
						     gtk_source_tag_table_new ());
#endif
			}
			else
			{
				tag_table_specified = TRUE;
			}

			break;
		}
	}

	g_object = G_OBJECT_CLASS (parent_class)->constructor (type, 
							       n_construct_properties,
							       construct_param);

	if (g_object) 
	{
		GtkSourceTagStyle *tag_style;
		
		GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (g_object);

		tag_style = gtk_source_tag_style_new ();
		
		gdk_color_parse ("white", &tag_style->foreground);
		gdk_color_parse ("gray", &tag_style->background);
		tag_style->mask |= (GTK_SOURCE_TAG_STYLE_USE_BACKGROUND |
				    GTK_SOURCE_TAG_STYLE_USE_FOREGROUND);

		tag_style->italic = FALSE;
		tag_style->bold = TRUE;
		tag_style->underline = FALSE;
		tag_style->strikethrough = FALSE;

		/* Set default bracket match style */
		gtk_source_buffer_set_bracket_match_style (source_buffer, tag_style);

		gtk_source_tag_style_free (tag_style);

		if (GTK_IS_SOURCE_TAG_TABLE (GTK_TEXT_BUFFER (source_buffer)->tag_table))
		{
			/* sync with the tag table if the user passed one */
			if (tag_table_specified)
				sync_with_tag_table (source_buffer);

			g_signal_connect (GTK_TEXT_BUFFER (source_buffer)->tag_table ,
					  "changed",
					  G_CALLBACK (tag_table_changed_cb),
					  source_buffer);
		}
		else
		{
			g_assert (GTK_IS_TEXT_TAG_TABLE (GTK_TEXT_BUFFER (source_buffer)->tag_table));

			g_warning ("Please use GtkSourceTagTable with GtkSourceBuffer.");

			g_signal_connect (GTK_TEXT_BUFFER (source_buffer)->tag_table,
					  "tag_added",
					  G_CALLBACK (tag_added_or_removed_cb),
					  source_buffer);

			g_signal_connect (GTK_TEXT_BUFFER (source_buffer)->tag_table,
					  "tag_removed",
					  G_CALLBACK (tag_added_or_removed_cb),
					  source_buffer);				
		}
	}

	return g_object;
}

static void
gtk_source_buffer_finalize (GObject *object)
{
	GtkSourceBuffer *buffer;
	GtkTextTagTable *tag_table;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	buffer = GTK_SOURCE_BUFFER (object);
	g_return_if_fail (buffer->priv != NULL);
	
	if (buffer->priv->markers)
		g_array_free (buffer->priv->markers, TRUE);

	if (buffer->priv->worker_handler) {
		g_source_remove (buffer->priv->worker_handler);
	}

	/* we can't delete marks if we're finalizing the buffer */
	gtk_text_region_destroy (buffer->priv->refresh_region, FALSE);
	gtk_text_region_destroy (buffer->priv->highlight_requests, FALSE);

	g_object_unref (buffer->priv->undo_manager);

	g_array_free (buffer->priv->syntax_regions, TRUE);
	if (buffer->priv->old_syntax_regions)
		g_array_free (buffer->priv->old_syntax_regions, TRUE);
	
	if (buffer->priv->reg_syntax_all) {
		gtk_source_regex_destroy (buffer->priv->reg_syntax_all);
		buffer->priv->reg_syntax_all = NULL;
	}
	
	g_list_free (buffer->priv->syntax_items);
	g_list_free (buffer->priv->pattern_items);

	if (buffer->priv->language != NULL)
		g_object_unref (buffer->priv->language);

	tag_table = GTK_TEXT_BUFFER (buffer)->tag_table;
	g_signal_handlers_disconnect_by_func (tag_table,
					      (gpointer)tag_table_changed_cb,
					      buffer);
	g_signal_handlers_disconnect_by_func (tag_table,
					      (gpointer)tag_added_or_removed_cb,
					      buffer);	

	g_free (buffer->priv);
	buffer->priv = NULL;
	
	/* TODO: free syntax_items, patterns, etc. - Paolo */
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
gtk_source_buffer_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	GtkSourceBuffer *source_buffer;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	source_buffer = GTK_SOURCE_BUFFER (object);
    
	switch (prop_id)
	{
		case PROP_ESCAPE_CHAR:
			gtk_source_buffer_set_escape_char (source_buffer,
							   g_value_get_uint (value));
			break;
			
		case PROP_CHECK_BRACKETS:
			gtk_source_buffer_set_check_brackets (source_buffer,
							      g_value_get_boolean (value));
			break;
			
		case PROP_HIGHLIGHT:
			gtk_source_buffer_set_highlight (source_buffer,
							 g_value_get_boolean (value));
			break;
			
		case PROP_MAX_UNDO_LEVELS:
			gtk_source_buffer_set_max_undo_levels (source_buffer,
							       g_value_get_int (value));
			break;
			
		case PROP_LANGUAGE:
			gtk_source_buffer_set_language (source_buffer,
							g_value_get_object (value));
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void 
gtk_source_buffer_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	GtkSourceBuffer *source_buffer;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	source_buffer = GTK_SOURCE_BUFFER (object);
    
	switch (prop_id)
	{
		case PROP_ESCAPE_CHAR:
			g_value_set_uint (value, source_buffer->priv->escape_char);
			break;
			
		case PROP_CHECK_BRACKETS:
			g_value_set_boolean (value, source_buffer->priv->check_brackets);
			break;
			
		case PROP_HIGHLIGHT:
			g_value_set_boolean (value, source_buffer->priv->highlight);
			break;
			
		case PROP_MAX_UNDO_LEVELS:
			g_value_set_int (value,
					 gtk_source_buffer_get_max_undo_levels (source_buffer));
			break;
			
		case PROP_LANGUAGE:
			g_value_set_object (value, source_buffer->priv->language);
			break;
			
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * gtk_source_buffer_new:
 * @table: a #GtkSourceTagTable, or %NULL to create a new one.
 * 
 * Creates a new source buffer.
 * 
 * Return value: a new source buffer.
 **/
GtkSourceBuffer *
gtk_source_buffer_new (GtkSourceTagTable *table)
{
	GtkSourceBuffer *buffer;

	buffer = GTK_SOURCE_BUFFER (g_object_new (GTK_TYPE_SOURCE_BUFFER, 
						  "tag-table", table, 
						  NULL));

	return buffer;
}

/**
 * gtk_source_buffer_new_with_language:
 * @language: a #GtkSourceLanguage.
 * 
 * Creates a new source buffer using the highlighting patterns in
 * @language.  This is equivalent to creating a new source buffer with
 * the default tag table and then calling
 * gtk_source_buffer_set_language().
 * 
 * Return value: a new source buffer which will highlight text
 * according to @language.
 **/
GtkSourceBuffer *
gtk_source_buffer_new_with_language (GtkSourceLanguage *language)
{
	GtkSourceBuffer *buffer;

	g_return_val_if_fail (GTK_IS_SOURCE_LANGUAGE (language), NULL);

	buffer = gtk_source_buffer_new (NULL);

	gtk_source_buffer_set_language (buffer, language);

	return buffer;
}

static void
gtk_source_buffer_can_undo_handler (GtkSourceUndoManager  	*um,
				    gboolean			 can_undo,
				    GtkSourceBuffer 		*buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	g_signal_emit (G_OBJECT (buffer),
		       buffer_signals[CAN_UNDO], 
		       0, 
		       can_undo);
}

static void
gtk_source_buffer_can_redo_handler (GtkSourceUndoManager  	*um,
				    gboolean         		 can_redo,
				    GtkSourceBuffer 		*buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	g_signal_emit (G_OBJECT (buffer),
		       buffer_signals[CAN_REDO], 
		       0, 
		       can_redo);
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

static void
gtk_source_buffer_move_cursor (GtkTextBuffer *buffer,
			       GtkTextIter   *iter, 
			       GtkTextMark   *mark, 
			       gpointer       data)
{
	GtkTextIter iter1, iter2;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (mark != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	if (mark != gtk_text_buffer_get_insert (buffer))
		return;

	if (GTK_SOURCE_BUFFER (buffer)->priv->bracket_found) 
	{
		gtk_text_buffer_get_iter_at_mark (buffer,
						  &iter1,
						  GTK_SOURCE_BUFFER (buffer)->priv->bracket_mark);
		iter2 = iter1;
		gtk_text_iter_forward_char (&iter2);
		gtk_text_buffer_remove_tag (buffer,
					    GTK_SOURCE_BUFFER (buffer)->priv->bracket_match_tag,
					    &iter1, 
					    &iter2);
	}

	if (!GTK_SOURCE_BUFFER (buffer)->priv->check_brackets)
		return;

	iter1 = *iter;
	if (gtk_source_buffer_find_bracket_match_with_limit (&iter1, MAX_CHARS_BEFORE_FINDING_A_MATCH)) 
	{
		if (!GTK_SOURCE_BUFFER (buffer)->priv->bracket_mark)
			GTK_SOURCE_BUFFER (buffer)->priv->bracket_mark =
				gtk_text_buffer_create_mark (buffer, 
							     NULL,
							     &iter1, 
							     FALSE);
		else
			gtk_text_buffer_move_mark (buffer,
						   GTK_SOURCE_BUFFER (buffer)->priv->bracket_mark,
						   &iter1);

		iter2 = iter1;
		gtk_text_iter_forward_char (&iter2);
		gtk_text_buffer_apply_tag (buffer,
					   GTK_SOURCE_BUFFER (buffer)->priv->bracket_match_tag,
					   &iter1, 
					   &iter2);
		GTK_SOURCE_BUFFER (buffer)->priv->bracket_found = TRUE;
	}
	else
	{
		GTK_SOURCE_BUFFER (buffer)->priv->bracket_found = FALSE;
	}
}

static void
gtk_source_buffer_real_insert_text (GtkTextBuffer *buffer,
				    GtkTextIter   *iter,
				    const gchar   *text, 
				    gint           len)
{
	gint start_offset;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/*
	 * iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text 
	 */
	parent_class->insert_text (buffer, iter, text, len);

	gtk_source_buffer_move_cursor (buffer, 
				       iter, 
				       gtk_text_buffer_get_insert (buffer),
				       NULL);

	if (!GTK_SOURCE_BUFFER (buffer)->priv->highlight)
		return;

	update_syntax_regions (GTK_SOURCE_BUFFER (buffer), 
			       start_offset,
			       g_utf8_strlen (text, len));

}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *start,
				     GtkTextIter   *end)
{
	gint delta;
	GtkTextMark *mark;
	GtkTextIter iter;
	GSList *markers;
		
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

	gtk_text_iter_order (start, end);
	delta = gtk_text_iter_get_offset (start) - 
			gtk_text_iter_get_offset (end);

	/* remove the markers in the deleted region if deleting more than one character */
	if (ABS (delta) > 1)
	{
		markers = gtk_source_buffer_get_markers_in_region (GTK_SOURCE_BUFFER (buffer),
								   start, end);
		while (markers)
		{
			gtk_source_buffer_delete_marker (GTK_SOURCE_BUFFER (buffer), markers->data);
			markers = g_slist_delete_link (markers, markers);
		}
	}
		
	parent_class->delete_range (buffer, start, end);

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
		
	gtk_source_buffer_move_cursor (buffer,
				       &iter,
				       mark,
				       NULL);

	/* move any markers which moved to this line because of the
	 * deletion to the beginning of the line */
	iter = *start;
	if (!gtk_text_iter_ends_line (&iter))
		gtk_text_iter_forward_to_line_end (&iter);
	markers = gtk_source_buffer_get_markers_in_region (GTK_SOURCE_BUFFER (buffer),
							   start, &iter);
	if (markers)
	{
		GSList *m;
		
		gtk_text_iter_set_line_offset (&iter, 0);
		for (m = markers; m; m = g_slist_next (m))
			gtk_source_buffer_move_marker (GTK_SOURCE_BUFFER (buffer),
						       GTK_SOURCE_MARKER (m->data),
						       &iter);
		g_slist_free (markers);
	}

	if (!GTK_SOURCE_BUFFER (buffer)->priv->highlight) 
		return;

	update_syntax_regions (GTK_SOURCE_BUFFER (buffer),
			       gtk_text_iter_get_offset (start),
			       delta);
}

static GSList *
gtk_source_buffer_get_source_tags (const GtkSourceBuffer *buffer)
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
sync_with_tag_table (GtkSourceBuffer *buffer)
{
	GtkTextTagTable *tag_table;
	GSList *entries;
	GSList *list;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (buffer->priv->syntax_items) {
		g_list_free (buffer->priv->syntax_items);
		buffer->priv->syntax_items = NULL;
	}

	if (buffer->priv->pattern_items) {
		g_list_free (buffer->priv->pattern_items);
		buffer->priv->pattern_items = NULL;
	}

	tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (tag_table != NULL);

	list = entries = gtk_source_buffer_get_source_tags (buffer);
	
	while (entries != NULL) 
	{	
		if (GTK_IS_SYNTAX_TAG (entries->data)) 
		{
			buffer->priv->syntax_items =
			    g_list_prepend (buffer->priv->syntax_items, entries->data);
			
		} 
		else if (GTK_IS_PATTERN_TAG (entries->data)) 
		{
			buffer->priv->pattern_items =
			    g_list_prepend (buffer->priv->pattern_items, entries->data);
			
		}

		entries = g_slist_next (entries);
	}

	g_slist_free (list);

	buffer->priv->syntax_items = g_list_reverse (buffer->priv->syntax_items);
	buffer->priv->pattern_items = g_list_reverse (buffer->priv->pattern_items);
	
	if (buffer->priv->syntax_items != NULL)
	{
		sync_syntax_regex (buffer);
	}
	else
	{
		if (buffer->priv->reg_syntax_all) {
			gtk_source_regex_destroy (buffer->priv->reg_syntax_all);
			buffer->priv->reg_syntax_all = NULL;
		}
	}

	if (buffer->priv->highlight)
		invalidate_syntax_regions (buffer, NULL, 0);
}

static void
sync_syntax_regex (GtkSourceBuffer *buffer)
{
	GString *str;
	GList *cur;
	GtkSyntaxTag *tag;

	str = g_string_new ("");
	cur = buffer->priv->syntax_items;

	while (cur != NULL) {
		g_return_if_fail (GTK_IS_SYNTAX_TAG (cur->data));

		tag = GTK_SYNTAX_TAG (cur->data);
		g_string_append (str, tag->start);
		
		cur = g_list_next (cur);
		
		if (cur != NULL)
			g_string_append (str, "|");
	}

	if (buffer->priv->reg_syntax_all)
		gtk_source_regex_destroy (buffer->priv->reg_syntax_all);
	
	buffer->priv->reg_syntax_all = gtk_source_regex_compile (str->str);

	g_string_free (str, TRUE);
}

static const GtkSyntaxTag *
iter_has_syntax_tag (const GtkTextIter *iter)
{
	const GtkSyntaxTag *tag;
	GSList *list;
	GSList *l;

	g_return_val_if_fail (iter != NULL, NULL);

	list = gtk_text_iter_get_tags (iter);
	tag = NULL;

	l = list;

	while ((list != NULL) && (tag == NULL)) {
		if (GTK_IS_SYNTAX_TAG (list->data))
			tag = GTK_SYNTAX_TAG (list->data);
		list = g_slist_next (list);
	}

	g_slist_free (l);

	return tag;
}


static GList *
gtk_source_buffer_get_syntax_entries (const GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	return buffer->priv->syntax_items;
}

static GList *
gtk_source_buffer_get_pattern_entries (const GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	return buffer->priv->pattern_items;
}

static gboolean
gtk_source_buffer_find_bracket_match_real (GtkTextIter *orig, gint max_chars)
{
	GtkTextIter iter;
	
	gunichar base_char;
	gunichar search_char;
	gunichar cur_char;
	gint addition;
	gint char_cont;
	gint counter;
	
	gboolean found;

	const GtkSyntaxTag *base_tag;

	iter = *orig;

	cur_char = gtk_text_iter_get_char (&iter);

	base_char = search_char = cur_char;
	base_tag = iter_has_syntax_tag (&iter);
	
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

	if (addition == 0)
		return FALSE;

	counter = 0;
	found = FALSE;
	char_cont = 0;
	
	do {
		gtk_text_iter_forward_chars (&iter, addition);
		cur_char = gtk_text_iter_get_char (&iter);
		++char_cont;
		
		if ((cur_char == search_char || cur_char == base_char) &&
		    base_tag == iter_has_syntax_tag (&iter))
		{
			if ((cur_char == search_char) && counter == 0) {
				found = TRUE;
				break;
			}
			if (cur_char == base_char)
				counter++;
			else 
				counter--;
		}
	} 
	while (!gtk_text_iter_is_end (&iter) && !gtk_text_iter_is_start (&iter) && 
		((char_cont < max_chars) || (max_chars < 0)));

	if (found)
		*orig = iter;

	return found;
}

/* Note that we take into account both the character following the cursor and the
 * one preceding it. If there are brackets on both sides the one following the 
 * cursor takes precedence.
 */
static gboolean
gtk_source_buffer_find_bracket_match_with_limit (GtkTextIter *orig, gint max_chars)
{
	GtkTextIter iter;

	if (gtk_source_buffer_find_bracket_match_real (orig, max_chars))
	{
		return TRUE;
	}

	iter = *orig;
	if (!gtk_text_iter_starts_line (&iter) &&
	    gtk_text_iter_backward_char (&iter))
	{
		if (gtk_source_buffer_find_bracket_match_real (&iter, max_chars))
		{
			*orig = iter;
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * gtk_source_iter_find_matching_bracket:
 * @iter: a #GtkTextIter.
 * 
 * Tries to match the bracket character currently at @iter with its
 * opening/closing counterpart, and if found moves @iter to the position
 * where it was found.
 *
 * @iter must be a #GtkTextIter belonging to a #GtkSourceBuffer.
 * 
 * Return value: %TRUE if the matching bracket was found and the @iter
 * iter moved.
 **/
gboolean
gtk_source_iter_find_matching_bracket (GtkTextIter *iter)
{
	g_return_val_if_fail (iter != NULL, FALSE);

	return gtk_source_buffer_find_bracket_match_with_limit (iter, -1);
}

/**
 * gtk_source_buffer_can_undo:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines whether a source buffer can undo the last action.
 * 
 * Return value: %TRUE if it's possible to undo the last action.
 **/
gboolean
gtk_source_buffer_can_undo (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return gtk_source_undo_manager_can_undo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_can_redo:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines whether a source buffer can redo the last action
 * (i.e. if the last operation was an undo).
 * 
 * Return value: %TRUE if a redo is possible.
 **/
gboolean
gtk_source_buffer_can_redo (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return gtk_source_undo_manager_can_redo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_undo:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Undoes the last user action which modified the buffer.  Use
 * gtk_source_buffer_can_undo() to check whether a call to this
 * function will have any effect.
 *
 * Actions are defined as groups of operations between a call to
 * gtk_text_buffer_begin_user_action() and
 * gtk_text_buffer_end_user_action(), or sequences of similar edits
 * (inserts or deletes) on the same line.
 **/
void
gtk_source_buffer_undo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (gtk_source_undo_manager_can_undo (buffer->priv->undo_manager));

	gtk_source_undo_manager_undo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_redo:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Redoes the last undo operation.  Use gtk_source_buffer_can_redo()
 * to check whether a call to this function will have any effect.
 **/
void
gtk_source_buffer_redo (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (gtk_source_undo_manager_can_redo (buffer->priv->undo_manager));

	gtk_source_undo_manager_redo (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_get_max_undo_levels:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines the number of undo levels the buffer will track for
 * buffer edits.
 * 
 * Return value: the maximum number of possible undo levels.
 **/
gint
gtk_source_buffer_get_max_undo_levels (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), 0);

	return gtk_source_undo_manager_get_max_undo_levels (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_set_max_undo_levels:
 * @buffer: a #GtkSourceBuffer.
 * @max_undo_levels: the desired maximum number of undo levels.
 * 
 * Sets the number of undo levels for user actions the buffer will
 * track.  If the number of user actions exceeds the limit set by this
 * function, older actions will be discarded.
 *
 * A new action is started whenever the function
 * gtk_text_buffer_begin_user_action() is called.  In general, this
 * happens whenever the user presses any key which modifies the
 * buffer, but the undo manager will try to merge similar consecutive
 * actions, such as multiple character insertions into one action.
 * But, inserting a newline does start a new action.
 **/
void
gtk_source_buffer_set_max_undo_levels (GtkSourceBuffer *buffer,
				       gint             max_undo_levels)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (gtk_source_undo_manager_get_max_undo_levels (
		    buffer->priv->undo_manager) != max_undo_levels)
	{
		gtk_source_undo_manager_set_max_undo_levels (buffer->priv->undo_manager,
							     max_undo_levels);
		g_object_notify (G_OBJECT (buffer), "max_undo_levels");
	}
}

/**
 * gtk_source_buffer_begin_not_undoable_action:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Marks the beginning of a not undoable action on the buffer,
 * disabling the undo manager.  Typically you would call this function
 * before initially setting the contents of the buffer (e.g. when
 * loading a file in a text editor).
 *
 * You may nest gtk_source_buffer_begin_not_undoable_action() /
 * gtk_source_buffer_end_not_undoable_action() blocks.
 **/
void
gtk_source_buffer_begin_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	gtk_source_undo_manager_begin_not_undoable_action (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_end_not_undoable_action:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Marks the end of a not undoable action on the buffer.  When the
 * last not undoable block is closed through the call to this
 * function, the list of undo actions is cleared and the undo manager
 * is re-enabled.
 **/
void
gtk_source_buffer_end_not_undoable_action (GtkSourceBuffer *buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	gtk_source_undo_manager_end_not_undoable_action (buffer->priv->undo_manager);
}

/**
 * gtk_source_buffer_get_check_brackets:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines whether bracket match highlighting is activated for the
 * source buffer.
 * 
 * Return value: %TRUE if the source buffer will highlight matching
 * brackets.
 **/
gboolean
gtk_source_buffer_get_check_brackets (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->check_brackets;
}

/**
 * gtk_source_buffer_set_check_brackets:
 * @buffer: a #GtkSourceBuffer.
 * @check_brackets: %TRUE if you want matching brackets highlighted.
 * 
 * Controls the bracket match highlighting function in the buffer.  If
 * activated, when you position your cursor over a bracket character
 * (a parenthesis, a square bracket, etc.) the matching opening or
 * closing bracket character will be highlighted.  You can specify the
 * style with the gtk_source_buffer_set_bracket_match_style()
 * function.
 **/
void
gtk_source_buffer_set_check_brackets (GtkSourceBuffer *buffer,
				      gboolean         check_brackets)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	check_brackets = (check_brackets != FALSE);

	if (check_brackets != buffer->priv->check_brackets)
	{
		buffer->priv->check_brackets = check_brackets;
		g_object_notify (G_OBJECT (buffer), "check_brackets");
	}
}

/**
 * gtk_source_buffer_set_bracket_match_style:
 * @source_buffer: a #GtkSourceBuffer.
 * @style: the #GtkSourceTagStyle specifying colors and text
 * attributes.
 * 
 * Sets the style used for highlighting matching brackets.
 **/
void 
gtk_source_buffer_set_bracket_match_style (GtkSourceBuffer         *source_buffer,
					   const GtkSourceTagStyle *style)
{
	GtkTextTag *tag;
	GValue foreground = { 0, };
	GValue background = { 0, };

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (source_buffer));
	g_return_if_fail (style != NULL);

	/* create the tag if not already done so */
	if (source_buffer->priv->bracket_match_tag == NULL)
	{
		source_buffer->priv->bracket_match_tag = gtk_text_tag_new (NULL);
		gtk_text_tag_table_add (gtk_text_buffer_get_tag_table (
						GTK_TEXT_BUFFER (source_buffer)),
					source_buffer->priv->bracket_match_tag);
		g_object_unref (source_buffer->priv->bracket_match_tag);
	}
	
	g_return_if_fail (source_buffer->priv->bracket_match_tag != NULL);
	tag = source_buffer->priv->bracket_match_tag;

	/* Foreground color */
	g_value_init (&foreground, GDK_TYPE_COLOR);
	
	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_FOREGROUND) != 0)
		g_value_set_boxed (&foreground, &style->foreground);
	else
		g_value_set_boxed (&foreground, NULL);
	
	g_object_set_property (G_OBJECT (tag), "foreground_gdk", &foreground);

	/* Background color */
	g_value_init (&background, GDK_TYPE_COLOR);

	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_BACKGROUND) != 0)
		g_value_set_boxed (&background, &style->background);
	else
		g_value_set_boxed (&background, NULL);
	
	g_object_set_property (G_OBJECT (tag), "background_gdk", &background);
	
	g_object_set (G_OBJECT (tag), 
		      "style", style->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
		      "weight", style->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      "strikethrough", style->strikethrough,
		      "underline", style->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
		      NULL);	
}

/**
 * gtk_source_buffer_get_highlight:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines whether text highlighting is activated in the source
 * buffer.
 * 
 * Return value: %TRUE if highlighting is enabled.
 **/
gboolean
gtk_source_buffer_get_highlight (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->highlight;
}

/**
 * gtk_source_buffer_set_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE if you want to activate highlighting.
 * 
 * Controls whether text is highlighted in the buffer.  If @highlight
 * is %TRUE, the text will be highlighted according to the patterns
 * installed in the buffer (either set with
 * gtk_source_buffer_set_language() or by adding individual
 * #GtkSourceTag tags to the buffer's tag table).  Otherwise, any
 * current highlighted text will be restored to the default buffer
 * style.
 *
 * Tags not of #GtkSourceTag type will not be removed by this option,
 * and normal #GtkTextTag priority settings apply when highlighting is
 * enabled.
 *
 * If not using a #GtkSourceLanguage for setting the highlighting
 * patterns in the buffer, it is recommended for performance reasons
 * that you add all the #GtkSourceTag tags with highlighting disabled
 * and enable it when finished.
 **/
void
gtk_source_buffer_set_highlight (GtkSourceBuffer *buffer,
				 gboolean         highlight)
{
	GtkTextIter iter1;
	GtkTextIter iter2;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (buffer->priv->highlight == highlight)
		return;

	buffer->priv->highlight = highlight;

	if (highlight) {
		invalidate_syntax_regions (buffer, NULL, 0);

	} else {
		if (buffer->priv->worker_handler) {
			g_source_remove (buffer->priv->worker_handler);
			buffer->priv->worker_handler = 0;
		}
		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer),
					    &iter1, 
					    &iter2);
		gtk_source_buffer_remove_all_source_tags (buffer,
							  &iter1,
							  &iter2);
	}
	g_object_notify (G_OBJECT (buffer), "highlight");
}

/* Idle worker code ------------ */

static gboolean
idle_worker (GtkSourceBuffer *source_buffer)
{
	GtkTextRegionIterator reg_iter;
	GtkTextIter start_iter, end_iter, last_end_iter;

	if (source_buffer->priv->worker_last_offset >= 0) {
		/* the syntax regions table is incomplete */
		build_syntax_regions_table (source_buffer, NULL);
	}

	/* Now we highlight subregions requested by our views */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer), &last_end_iter, 0);

	gtk_text_region_get_iterator (source_buffer->priv->highlight_requests,
	                              &reg_iter, 0);

	while (!gtk_text_region_iterator_is_end (&reg_iter))
	{
		gtk_text_region_iterator_get_subregion (&reg_iter,
							&start_iter,
							&end_iter);

		if (source_buffer->priv->worker_last_offset < 0 ||
		    source_buffer->priv->worker_last_offset >=
		    gtk_text_iter_get_offset (&end_iter)) {
			ensure_highlighted (source_buffer,
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

	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer), &start_iter, 0);

	if (!gtk_text_iter_equal (&start_iter, &last_end_iter)) {
		/* remove already highlighted subregions from requests */
		gtk_text_region_subtract (source_buffer->priv->highlight_requests,
					  &start_iter, &last_end_iter);
	}
	
	if (source_buffer->priv->worker_last_offset < 0) {
		/* idle handler will be removed */
		source_buffer->priv->worker_handler = 0;
		return FALSE;
	}
	
	return TRUE;
}

static void
install_idle_worker (GtkSourceBuffer *source_buffer)
{
	if (source_buffer->priv->worker_handler == 0) {
		/* use the text view validation priority to get
		 * highlighted text even before complete validation of
		 * the buffer */
		source_buffer->priv->worker_handler =
			g_idle_add_full (GTK_TEXT_VIEW_PRIORITY_VALIDATE,
					 (GSourceFunc) idle_worker,
					 source_buffer, 
					 NULL);
	}
}

/* Syntax analysis code -------------- */

static gboolean
is_escaped (GtkSourceBuffer *source_buffer, const gchar *text, gint index)
{
	gchar *tmp = (gchar *) text + index;
	gboolean retval = FALSE;

	if (source_buffer->priv->escape_char == 0)
		return FALSE;
	
	tmp = g_utf8_find_prev_char (text, tmp);
	while (tmp && g_utf8_get_char (tmp) == source_buffer->priv->escape_char) 
	{
		retval = !retval;
		tmp = g_utf8_find_prev_char (text, tmp);
	}
	return retval;
}

static const GtkSyntaxTag * 
get_syntax_start (GtkSourceBuffer      *source_buffer,
		  const gchar          *text,
		  gint                  length,
		  guint                 match_options,
		  GtkSourceBufferMatch *match)
{
	GList *list;
	GtkSyntaxTag *tag;
	gint pos;
	
	if (length == 0)
		return NULL;
	
	list = gtk_source_buffer_get_syntax_entries (source_buffer);

	if (list == NULL)
		return NULL;

	pos = 0;
	do {
		/* check for any of the syntax highlights */
		pos = gtk_source_regex_search (
			source_buffer->priv->reg_syntax_all,
			text,
			pos,
			length,
			match,
			match_options);
		if (pos < 0 || !is_escaped (source_buffer, text, match->startindex))
			break;
		pos = match->startpos + 1;
	} while (pos >= 0);

	if (pos < 0)
		return NULL;

	while (list != NULL) {
		tag = list->data;
		
		if (gtk_source_regex_match (tag->reg_start, text,
					    pos, match->endindex,
					    match_options))
			return tag;

		list = g_list_next (list);
	}

	return NULL;
}

static gboolean 
get_syntax_end (GtkSourceBuffer      *source_buffer,
		const gchar          *text,
		gint                  length,
		guint                 match_options,
		GtkSyntaxTag         *tag,
		GtkSourceBufferMatch *match)
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
					       length, match, match_options);
		if (pos < 0 || !is_escaped (source_buffer, text, match->startindex))
			break;
		pos = match->startpos + 1;
	} while (pos >= 0);

	return (pos >= 0);
}

/* Syntax regions code ------------- */

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
invalidate_syntax_regions (GtkSourceBuffer *source_buffer,
			   GtkTextIter     *from,
			   gint             delta)
{
	GArray *table, *old_table;
	gint region, saved_region;
	gint offset;
	SyntaxDelimiter *delim;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (source_buffer));
	
	table = source_buffer->priv->syntax_regions;
	g_assert (table != NULL);
	
	if (from) {
		offset = gtk_text_iter_get_offset (from);
	} else {
		offset = 0;
	}

	DEBUG (g_message ("invalidating from %d", offset));
	
	if (!gtk_source_buffer_get_syntax_entries (source_buffer))
	{
		/* Shortcut case: we don't have syntax entries, so we
		 * won't build the table.  OTOH, we do need to refresh
		 * the highilighting in case there are pattern
		 * entries. */
		GtkTextIter start, end;
		
		g_array_set_size (table, 0);
		source_buffer->priv->worker_last_offset = -1;

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (source_buffer), &start, &end);
		if (from)
			start = *from;
		refresh_range (source_buffer, &start, &end);

		return;
	}
	
	/* check if the offset has been analyzed already */
	if ((source_buffer->priv->worker_last_offset >= 0) &&
	    (offset > source_buffer->priv->worker_last_offset))
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
	if (source_buffer->priv->old_syntax_regions) {
		g_array_free (source_buffer->priv->old_syntax_regions, TRUE);
		source_buffer->priv->old_syntax_regions = NULL;
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
		source_buffer->priv->old_syntax_regions = old_table;

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
		source_buffer->priv->worker_last_offset =
			g_array_index (table, SyntaxDelimiter, region - 1).offset;
	} else {
		source_buffer->priv->worker_last_offset = 0;
	}
	
	install_idle_worker (source_buffer);
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
 * @source_buffer: the #GtkSourceBuffer to work on.
 * @state: the current #SyntaxDelimiter.
 * @head: text to analyze.
 * @head_length: length in bytes of @head.
 * @head_offset: offset in the buffer where @head starts.
 * @match: a #GtkSourceBufferMatch object to get the results.
 * 
 * This function can be seen as a single iteration in the analyzing
 * process.  It takes the current @state, searches for the next syntax
 * pattern in head (starting from byte index 0) and if found, updates
 * @state to reflect the new state.  @match is also filled with the
 * matching bounds.
 * 
 * Return value: %TRUE if a syntax pattern was found in @head.
 **/
static gboolean 
next_syntax_region (GtkSourceBuffer      *source_buffer,
		    SyntaxDelimiter      *state,
		    const gchar          *head,
		    gint                  head_length,
		    gint                  head_offset,
		    guint                 head_options,
		    GtkSourceBufferMatch *match)
{
	GtkSyntaxTag *tag;
	gboolean found;
	
	if (!state->tag) {
		/* we come from a non-syntax colored region, so seek
		 * for an opening pattern */
		tag = (GtkSyntaxTag *) get_syntax_start (
			source_buffer, head, head_length, head_options, match);

		if (!tag)
			return FALSE;
		
		state->tag = tag;
		state->offset = match->startpos + head_offset;
		state->depth = 1;

	} else {
		/* seek the closing pattern for the current syntax
		 * region */
		found = get_syntax_end (source_buffer,
					head, head_length, head_options, 
					state->tag, match);
		
		if (!found)
			return FALSE;
		
		state->offset = match->endpos + head_offset;
		state->tag = NULL;
		state->depth = 0;
		
	}
	return TRUE;
}

static void 
build_syntax_regions_table (GtkSourceBuffer   *source_buffer,
			    const GtkTextIter *needed_end)
{
	GArray *table;
	GtkTextIter start, end;
	GArray *old_table;
	gint old_region;
	gboolean use_old_data;
	gchar *slice, *head;
	gint offset, head_length;
	guint slice_options;
	GtkSourceBufferMatch match;
	SyntaxDelimiter delim;
	GTimer *timer;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (source_buffer));
	
	/* we shouldn't have been called if the buffer has no syntax entries */
	g_assert (gtk_source_buffer_get_syntax_entries (source_buffer) != NULL);
	
	/* check if we still have text to analyze */
	if (source_buffer->priv->worker_last_offset < 0)
		return;
	
	/* compute starting iter of the batch */
	offset = source_buffer->priv->worker_last_offset;
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
					    &start, offset);
	
	DEBUG (g_message ("restarting syntax regions from %d", offset));
	
	/* compute ending iter of the batch */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
					    &end, offset + source_buffer->priv->
					    worker_batch_size);
	
	/* extend the range to include needed_end if necessary */
	if (needed_end && gtk_text_iter_compare (&end, needed_end) < 0)
		end = *needed_end;
	
	/* always stop processing at end of lines: this minimizes the
	 * chance of not getting a syntax pattern because it was split
	 * in between batches */
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);

	table = source_buffer->priv->syntax_regions;
	g_assert (table != NULL);
	
	/* get old table information */
	use_old_data = FALSE;
	old_table = source_buffer->priv->old_syntax_regions;
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

	/* we always stop processing at line ends */
	slice_options = (gtk_text_iter_get_line_offset (&start) != 0 ?
			 GTK_SOURCE_REGEX_NOT_BOL : 0);

	timer = g_timer_new ();

	/* MAIN LOOP: build the table */
	while (head_length > 0) {
		if (!next_syntax_region (source_buffer,
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
		head += match.endindex;
		head_length -= match.endindex;
		offset += match.endpos;

		/* recalculate b-o-l matching options */
		if (match.endindex > 0)
		{
			GtkTextIter tmp;
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
							    &tmp, offset);
			if (gtk_text_iter_get_line_offset (&tmp) != 0)
				slice_options |= GTK_SOURCE_REGEX_NOT_BOL;
			else
				slice_options &= ~GTK_SOURCE_REGEX_NOT_BOL;
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
		source_buffer->priv->worker_last_offset = offset;
		gtk_text_iter_set_offset (&end, offset);
		
	} else {
		/* update worker information */
		source_buffer->priv->worker_last_offset =
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

			source_buffer->priv->worker_batch_size = 
				MAX (batch_size, MINIMUM_WORKER_BATCH);
		}
	}

	/* make sure the analyzed region gets highlighted */
	refresh_range (source_buffer, &start, &end);
	
	/* forget saved table if we have already "consumed" at least
	 * two of its delimiters, since that probably means it
	 * contains invalid, useless data */
	if (old_table && (use_old_data ||
			  source_buffer->priv->worker_last_offset < 0 ||
			  old_region > 1)) {
		g_array_free (old_table, TRUE);
		source_buffer->priv->old_syntax_regions = NULL;
	}
	
	PROFILE (g_message ("ended worker batch, %g ms elapsed",
			    g_timer_elapsed (timer, NULL) * 1000));
	DEBUG (g_message ("table has %u entries", table->len));

	g_timer_destroy (timer);
}

static void 
update_syntax_regions (GtkSourceBuffer *source_buffer,
		       gint             start_offset,
		       gint             delta)
{
	GArray *table;
	gint first_region, region;
	gint table_index, expected_end_index;
	gchar *slice, *head;
	guint slice_options;
	gint head_length, head_offset;
	GtkTextIter start_iter, end_iter;
	GtkSourceBufferMatch match;
	SyntaxDelimiter delim;
	gboolean mismatch;
	
	table = source_buffer->priv->syntax_regions;
	g_assert (table != NULL);

	if (!source_buffer->priv->highlight)
		return;
	
	if (!gtk_source_buffer_get_syntax_entries (source_buffer))
	{
		/* Shortcut case: we don't have syntax entries, so we
		 * just refresh_range() the edited area */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
						    &start_iter, start_offset);
		end_iter = start_iter;
		if (delta > 0)
			gtk_text_iter_forward_chars (&end_iter, delta);
	
		gtk_text_iter_set_line_offset (&start_iter, 0);
		gtk_text_iter_forward_to_line_end (&end_iter);

		refresh_range (source_buffer, &start_iter, &end_iter);

		return;
	}
	
	/* check if the offset is at an unanalyzed region */
	if (source_buffer->priv->worker_last_offset >= 0 &&
	    start_offset >= source_buffer->priv->worker_last_offset) {
		/* update saved table offsets which potentially
		 * contain the offset */
		region = bsearch_offset (source_buffer->priv->old_syntax_regions, start_offset);
		if (region > 0)
		{
			/* Changes to the uncontrolled regions. We can't possibly
			   know if some of the syntax regions changed, so we
			   invalidate the saved information */
			if (source_buffer->priv->old_syntax_regions) {
				g_array_free (source_buffer->priv->old_syntax_regions, TRUE);
				source_buffer->priv->old_syntax_regions = NULL;
			}
		}
		else
		{
			adjust_table_offsets (source_buffer->priv->old_syntax_regions,
					      region, delta);
		}
		return;
	}
	
	/* we shall start analyzing from the beginning of the line */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
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
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
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
		gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (source_buffer),
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
			invalidate_syntax_regions (source_buffer, &start_iter, delta);
			
			return;
		}

		/* set ending iter */
		gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
						    &end_iter,
						    end_offset);

		/* calculate expected_end_index */
		if (g_array_index (table, SyntaxDelimiter, region).tag)
			expected_end_index = region;
		else
			expected_end_index = MIN (region + 1, table->len);
		
	} else {
		/* set the ending iter to the end of the buffer */
		gtk_text_buffer_get_end_iter (GTK_TEXT_BUFFER (source_buffer),
					      &end_iter);
		expected_end_index = table->len;
	}

	/* get us the chunk of text to analyze */
	head = slice = gtk_text_iter_get_slice (&start_iter, &end_iter);
	head_length = strlen (head);

	/* eol match options is constant for this run */
	slice_options = ((gtk_text_iter_get_line_offset (&start_iter) != 0 ?
			  GTK_SOURCE_REGEX_NOT_BOL : 0) |
			 (!gtk_text_iter_ends_line (&end_iter) ?
			  GTK_SOURCE_REGEX_NOT_EOL : 0));

	/* We will start analyzing the slice of text and see if it
	 * matches the information from the table.  When we hit a
	 * mismatch, it means we need to invalidate. */
	mismatch = FALSE;
	while (next_syntax_region (source_buffer,
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
		head += match.endindex;
		head_length -= match.endindex;
		head_offset += match.endpos;
		table_index++;

		/* recalculate b-o-l matching options */
		if (match.endindex > 0)
		{
			GtkTextIter tmp;
			gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
							    &tmp, head_offset);
			if (gtk_text_iter_get_line_offset (&tmp) != 0)
				slice_options |= GTK_SOURCE_REGEX_NOT_BOL;
			else
				slice_options &= ~GTK_SOURCE_REGEX_NOT_BOL;
		}
	}

	g_free (slice);

	if (mismatch || table_index < expected_end_index) {
		/* we invalidate if there was a mismatch or we didn't
		 * advance the table_index enough (which means some
		 * delimiter was deleted) */
		DEBUG (g_message ("changed delimiter at %d", delim.offset));
		
		invalidate_syntax_regions (source_buffer, &start_iter, delta);
		
		return;
	}

	/* no syntax regions changed */
	
	/* update trailing offsets with delta ... */
	adjust_table_offsets (table, region, delta);

	/* ... worker data ... */
	if (source_buffer->priv->worker_last_offset >= start_offset + delta)
		source_buffer->priv->worker_last_offset += delta;
	
	/* ... and saved table offsets */
	adjust_table_offsets (source_buffer->priv->old_syntax_regions, 0, delta);

	/* the syntax regions have not changed, so set the refreshing bounds */
	gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (source_buffer),
					    &start_iter, start_offset);
	end_iter = start_iter;
	if (delta > 0)
		gtk_text_iter_forward_chars (&end_iter, delta);
	
	/* adjust bounds to line bounds to correctly highlight
	   non-syntax patterns */
	gtk_text_iter_set_line_offset (&start_iter, 0);
	gtk_text_iter_forward_to_line_end (&end_iter);
	
	refresh_range (source_buffer, &start_iter, &end_iter);
}

/* Beginning of highlighting code ------------ */

/**
 * search_patterns:
 * @matches: the starting list of matches to work from (can be %NULL).
 * @text: the text which will be searched for.
 * @length: the length (in bytes) of @text.
 * @offset: the offset the beginning of @text is at.
 * @index: an index to add the match indexes (usually: @text - base_text).
 * @patterns: additional patterns (can be %NULL).
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
		 guint        match_options,
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
					     &match,
					     match_options);
		
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
check_pattern (GtkSourceBuffer *source_buffer,
	       GtkTextIter     *start,
	       const gchar     *text,
	       gint             length,
	       guint            match_options)
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
	
	if (length == 0 || !gtk_source_buffer_get_pattern_entries (source_buffer))
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
				   match_options,
				   gtk_source_buffer_get_pattern_entries (source_buffer));
	
	while (matches && length > 0) {
		/* pick the first (nearest) match... */
		PatternMatch *pmatch = matches->data;
		
		gtk_text_iter_set_offset (&start_iter,
					  pmatch->match.startpos);
		gtk_text_iter_set_offset (&end_iter,
					  pmatch->match.endpos);

		/* ... and apply it */
		gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (source_buffer),
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
					   match_options,
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
highlight_region (GtkSourceBuffer *source_buffer,
		  GtkTextIter     *start,
		  GtkTextIter     *end)
{
	GtkTextIter b_iter, e_iter;
	gint b_off, e_off, end_offset;
	GtkSyntaxTag *current_tag;
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
	
	table = source_buffer->priv->syntax_regions;
	g_return_if_fail (table != NULL);
	
	/* remove_all_tags is not efficient: for different positions
	   in the buffer it takes different times to complete, taking
	   longer if the slice is at the beginning */
	gtk_source_buffer_remove_all_source_tags (source_buffer, start, end);

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
			gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (source_buffer),
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

			/* calculate beginning and end of line match options */
			slice_options = ((gtk_text_iter_get_line_offset (&b_iter) != 0 ?
					  GTK_SOURCE_REGEX_NOT_BOL : 0) |
					 (!gtk_text_iter_ends_line (&e_iter) ?
					  GTK_SOURCE_REGEX_NOT_EOL : 0));

			check_pattern (source_buffer, &b_iter,
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
refresh_range (GtkSourceBuffer *buffer,
	       GtkTextIter     *start, 
	       GtkTextIter     *end)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	/* Add the region to the refresh region */
	gtk_text_region_add (buffer->priv->refresh_region, start, end);

	/* Notify views of the updated highlight region */
	g_signal_emit (buffer, buffer_signals [HIGHLIGHT_UPDATED], 0, start, end);
}

static void 
ensure_highlighted (GtkSourceBuffer   *source_buffer,
		    const GtkTextIter *start,
		    const GtkTextIter *end)
{
	GtkTextRegion *region;

#if 0
	DEBUG (g_message ("ensure_highlighted %d to %d",
			  gtk_text_iter_get_offset (start),
			  gtk_text_iter_get_offset (end)));
#endif
	
	/* get the subregions not yet highlighted */
	region = gtk_text_region_intersect (
		source_buffer->priv->refresh_region, start, end);
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
			highlight_region (source_buffer, &s, &e);

			gtk_text_region_iterator_next (&reg_iter);
		}

		gtk_text_region_destroy (region, TRUE);

		/* remove the just highlighted region */
		gtk_text_region_subtract (source_buffer->priv->refresh_region,
					  start, end);
	}
}

static void 
highlight_queue (GtkSourceBuffer   *source_buffer,
		 const GtkTextIter *start,
		 const GtkTextIter *end)
{
	gtk_text_region_add (source_buffer->priv->highlight_requests,
			     start,
			     end);

	DEBUG (g_message ("queueing highlight [%d, %d]",
			  gtk_text_iter_get_offset (start),
			  gtk_text_iter_get_offset (end)));
}

void 
_gtk_source_buffer_highlight_region (GtkSourceBuffer   *source_buffer,
				     const GtkTextIter *start,
				     const GtkTextIter *end,
				     gboolean           highlight_now)
{
	g_return_if_fail (source_buffer != NULL);
	g_return_if_fail (start != NULL);
       	g_return_if_fail (end != NULL);

	if (!source_buffer->priv->highlight)
		return;

#ifndef LAZIEST_MODE
	if (source_buffer->priv->worker_last_offset < 0 ||
	    source_buffer->priv->worker_last_offset >= gtk_text_iter_get_offset (end)) {
		ensure_highlighted (source_buffer, start, end);
	} else
#endif
	{
		if (highlight_now)
		{
			build_syntax_regions_table (source_buffer, end);
			ensure_highlighted (source_buffer, start, end);
		}
		else
		{
			highlight_queue (source_buffer, start, end);
			install_idle_worker (source_buffer);
		}
	}
}

/* This is a modified version of the gtk_text_buffer_remove_all_tags
 * function from gtk/gtktextbuffer.c
 *
 * Copyright (C) 2000 Red Hat, Inc.
 */

static gint
pointer_cmp (gconstpointer a, gconstpointer b)
{
	if (a < b)
		return -1;
	else if (a > b)
		return 1;
	else
		return 0;
}

/**
 * gtk_source_buffer_remove_all_source_tags:
 * @buffer: a #GtkSourceBuffer.
 * @start: one bound of range to be untagged.
 * @end: other bound of range to be untagged.
 * 
 * Removes all tags in the range between @start and @end.  Be careful
 * with this function; it could remove tags added in code unrelated to
 * the code you're currently writing. That is, using this function is
 * probably a bad idea if you have two or more unrelated code sections
 * that add tags.
 **/
static void
gtk_source_buffer_remove_all_source_tags (GtkSourceBuffer   *buffer,
					  const GtkTextIter *start,
					  const GtkTextIter *end)
{
	GtkTextIter first, second, tmp;
	GSList *tags;
	GSList *tmp_list;
	GSList *tmp_list2;
	GSList *prev;
	GtkTextTag *tag;
  
	/*
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == GTK_TEXT_BUFFER (buffer));
	g_return_if_fail (gtk_text_iter_get_buffer (end) == GTK_TEXT_BUFFER (buffer));
	*/
	
	first = *start;
	second = *end;

	gtk_text_iter_order (&first, &second);

	/* Get all tags turned on at the start */
	tags = NULL;
	tmp_list = gtk_text_iter_get_tags (&first);
	tmp_list2 = tmp_list;
	
	while (tmp_list2 != NULL)
	{
		if (GTK_IS_SOURCE_TAG (tmp_list2->data))
		{
			tags = g_slist_prepend (tags, tmp_list2->data);
		}

		tmp_list2 = g_slist_next (tmp_list2);
	}
	
	g_slist_free (tmp_list);
	
	/* Find any that are toggled on within the range */
	tmp = first;
	while (gtk_text_iter_forward_to_tag_toggle (&tmp, NULL))
	{
		GSList *toggled;
		
		if (gtk_text_iter_compare (&tmp, &second) >= 0)
			break; /* past the end of the range */
      
		toggled = gtk_text_iter_get_toggled_tags (&tmp, TRUE);

		/* We could end up with a really big-ass list here.
		 * Fix it someday.
		 */
		tmp_list2 = toggled;
		while (tmp_list2 != NULL)
		{
			if (GTK_IS_SOURCE_TAG (tmp_list2->data))
			{
				tags = g_slist_prepend (tags, tmp_list2->data);
			}

			tmp_list2 = g_slist_next (tmp_list2);
		}

		g_slist_free (toggled);
	}
  
	/* Sort the list */
	tags = g_slist_sort (tags, pointer_cmp);

	/* Strip duplicates */
	tag = NULL;
	prev = NULL;
	tmp_list = tags;

	while (tmp_list != NULL)
	{
		if (tag == tmp_list->data)
		{
			/* duplicate */
			if (prev)
				prev->next = tmp_list->next;

			tmp_list->next = NULL;

			g_slist_free (tmp_list);

			tmp_list = prev->next;
			/* prev is unchanged */
		}
		else
		{
			/* not a duplicate */
			tag = GTK_TEXT_TAG (tmp_list->data);
			prev = tmp_list;
			tmp_list = tmp_list->next;
		}
	}

	g_slist_foreach (tags, (GFunc) g_object_ref, NULL);
  
	tmp_list = tags;
	while (tmp_list != NULL)
	{
		tag = GTK_TEXT_TAG (tmp_list->data);

		gtk_text_buffer_remove_tag (GTK_TEXT_BUFFER (buffer), 
					    tag,
					    &first,
					    &second);
      
		tmp_list = tmp_list->next;
	}

	g_slist_foreach (tags, (GFunc) g_object_unref, NULL);
  
	g_slist_free (tags);
}

/**
 * gtk_source_buffer_set_language:
 * @buffer: a #GtkSourceBuffer.
 * @language: a #GtkSourceLanguage to set, or %NULL.
 * 
 * Sets the #GtkSourceLanguage the source buffer will use, adding
 * #GtkSourceTag tags with the language's patterns and setting the
 * escape character with gtk_source_buffer_set_escape_char().  Note
 * that this will remove any #GtkSourceTag tags currently in the
 * buffer's tag table.  The buffer holds a reference to the @language
 * set.
 **/
void
gtk_source_buffer_set_language (GtkSourceBuffer   *buffer, 
				GtkSourceLanguage *language)
{
	GtkSourceTagTable *table;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (buffer->priv->language == language)
		return;

	if (language != NULL)
		g_object_ref (language);

	if (buffer->priv->language != NULL)
		g_object_unref (buffer->priv->language);

	buffer->priv->language = language;

	/* remove previous tags */
	table = GTK_SOURCE_TAG_TABLE (gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer)));
	gtk_source_tag_table_remove_source_tags (table);

	if (language != NULL)
	{
		GSList *list = NULL;
			
		list = gtk_source_language_get_tags (language);		
 		gtk_source_tag_table_add_tags (table, list);

		g_slist_foreach (list, (GFunc)g_object_unref, NULL);
		g_slist_free (list);

		gtk_source_buffer_set_escape_char (
			buffer, gtk_source_language_get_escape_char (language));
	}

	g_object_notify (G_OBJECT (buffer), "language");
}

/**
 * gtk_source_buffer_get_language:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines the #GtkSourceLanguage used by the buffer.  The returned
 * object should not be unreferenced by the user.
 * 
 * Return value: the #GtkSourceLanguage set by
 * gtk_source_buffer_set_language(), or %NULL.
 **/
GtkSourceLanguage *
gtk_source_buffer_get_language (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	return buffer->priv->language;
}

/**
 * gtk_source_buffer_get_escape_char:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Determines the escaping character used by the source buffer
 * highlighting engine.
 * 
 * Return value: the UTF-8 character for the escape character the
 * buffer is using.
 **/
gunichar 
gtk_source_buffer_get_escape_char (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL && GTK_IS_SOURCE_BUFFER (buffer), 0);

	return buffer->priv->escape_char;
}

/**
 * gtk_source_buffer_set_escape_char:
 * @buffer: a #GtkSourceBuffer.
 * @escape_char: the escape character the buffer should use.
 * 
 * Sets the escape character to be used by the highlighting engine.
 *
 * When performing the initial analysis, the engine will discard a
 * matching syntax pattern if it's prefixed with an odd number of
 * escape characters.  This allows for example to correctly highlight
 * strings with escaped quotes embedded.
 *
 * This setting affects only syntax patterns (i.e. those defined in
 * #GtkSyntaxTag tags).
 **/
void 
gtk_source_buffer_set_escape_char (GtkSourceBuffer *buffer,
				   gunichar         escape_char)
{
	g_return_if_fail (buffer != NULL && GTK_IS_SOURCE_BUFFER (buffer));

	if (escape_char != buffer->priv->escape_char)
	{
		buffer->priv->escape_char = escape_char;
		if (buffer->priv->highlight)
			invalidate_syntax_regions (buffer, NULL, 0);
		g_object_notify (G_OBJECT (buffer), "escape_char");
	}
}

/* Markers functionality */

/**
 * markers_binary_search:
 * @buffer: the GtkSourceBuffer where the markers are.
 * @iter: the position to search for.
 * @last_cmp: where to return the value of the last comparision made (optional).
 * 
 * Performs a binary search among the markers in @buffer for the
 * position of the @iter.  Returns the nearest matching marker (its
 * index in the markers array) and optionally the value of the
 * comparision between the returned marker and the given iter.
 * 
 * Return value: an index in the markers array or -1 if the array is
 * empty.
 **/
static gint
markers_binary_search (GtkSourceBuffer *buffer, GtkTextIter *iter, gint *last_cmp)
{
	GtkTextIter check_iter;
	GtkSourceMarker **check, **p;
	GArray *markers = buffer->priv->markers;
	gint n_markers = markers->len;
	gint cmp, i;
	
	if (n_markers == 0)
		return -1;

	check = p = &g_array_index (markers, GtkSourceMarker *, 0);
	p--;
	cmp = 0;
	while (n_markers)
	{
		i = (n_markers + 1) >> 1;
		check = p + i;
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
						  &check_iter,
						  GTK_TEXT_MARK (*check));
		cmp = gtk_text_iter_compare (iter, &check_iter);
		if (cmp > 0)
		{
			n_markers -= i;
			p = check;
		}
		else if (cmp < 0)
			n_markers = i - 1;
		else /* if (cmp == 0) */
			break;
	}

	i = check - &g_array_index (markers, GtkSourceMarker *, 0);
	if (last_cmp)
		*last_cmp = cmp;

	return i;
}

/**
 * markers_linear_lookup:
 * @buffer: the source buffer where the markers are.
 * @marker: which marker to search for.
 * @start: index from where to start looking.
 * @direction: direction to search for.
 * 
 * Searches the markers array of @buffer starting from @start for
 * markers at the same position as the one at @start.  If @marker is
 * non-%NULL search for that marker specifically, otherwise return the
 * first or the last marker at the staring position, depending on
 * @direction.
 *
 * @direction < 0 means left, @direction > 0 means right,
 * 0 means both and is mostly useful when looking for a specific
 * @marker.
 * 
 * Return value: the index of the searched marker.
 **/
static gint 
markers_linear_lookup (GtkSourceBuffer *buffer,
		       GtkSourceMarker *marker,
		       gint             start,
		       gint             direction)
{
	GArray *markers = buffer->priv->markers;
	gint left, right;
	gint cmp;
	GtkTextIter iter;
	GtkSourceMarker *tmp;

	tmp = g_array_index (markers, GtkSourceMarker *, start);
	if (tmp == marker)
		return start;

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &iter,
					  GTK_TEXT_MARK (tmp));
	
	if (direction == 0)
	{
		left = start - 1;
		right = start + 1;
	}
	else if (direction > 0)
	{
		left = -1;
		right = start + 1;
	}
	else
	{
		left = start - 1;
		right = markers->len;
	}
	
	while (left >= 0 || right < markers->len)
	{
		GtkTextIter iter_tmp;
		
		if (left >= 0)
		{
			tmp = g_array_index (markers, GtkSourceMarker *, left);
			if (tmp == marker)
				return left;
			
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
							  &iter_tmp,
							  GTK_TEXT_MARK (tmp));
			cmp = gtk_text_iter_compare (&iter, &iter_tmp);
			if (cmp != 0)
			{
				if (marker)
					/* searching for a particular marker */
					left = -1;
				else
					/* searching for the first
					 * marker at a given
					 * position */
					return left + 1;
			} else
				left--;
		}

		if (right < markers->len)
		{
			tmp = g_array_index (markers, GtkSourceMarker *, right);
			if (tmp == marker)
				return right;
			
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
							  &iter_tmp,
							  GTK_TEXT_MARK (tmp));
			cmp = gtk_text_iter_compare (&iter, &iter_tmp);
			if (cmp != 0)
			{
				if (marker)
					/* searching for a particular marker */
					right = markers->len;
				else
					/* searching for the last
					 * marker at a given
					 * position */
					return right - 1;
			}
			else
				right++;
		}
	}
	if (marker)
		return -1;
	else
		return start;
}

static void
markers_insert (GtkSourceBuffer *buffer, GtkSourceMarker *marker)
{
	GtkTextIter iter;
	gint index, cmp;

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &iter,
					  GTK_TEXT_MARK (marker));
	
	index = markers_binary_search (buffer, &iter, &cmp);
	if (index >= 0)
	{
		_gtk_source_marker_link (marker, g_array_index (buffer->priv->markers,
								GtkSourceMarker *,
								index),	(cmp > 0));
		if (cmp > 0)
			index++;
	}
	else
		index = 0;
	
	g_array_insert_val (buffer->priv->markers, index, marker);
}

/**
 * gtk_source_buffer_create_marker:
 * @buffer: a #GtkSourceBuffer.
 * @name: the name of the marker, or %NULL.
 * @type: a string defining the marker type, or %NULL.
 * @where: location to place the marker.
 * 
 * Creates a marker in the @buffer of type @type.  A marker is
 * semantically very similar to a #GtkTextMark, except it has a type
 * which is used by the #GtkSourceView displaying the buffer to show a
 * pixmap on the left margin, at the line the marker is in.  Because
 * of this, a marker is generally associated to a line and not a
 * character position.  Markers are also accessible through a position
 * or range in the buffer.
 *
 * Markers are implemented using #GtkTextMark, so all characteristics
 * and restrictions to marks apply to markers too.  These includes
 * life cycle issues and "mark-set" and "mark-deleted" signal
 * emissions.
 *
 * Like a #GtkTextMark, a #GtkSourceMarker can be anonymous if the
 * passed @name is %NULL.  Also, the buffer owns the markers so you
 * shouldn't unreference it.
 *
 * Markers always have left gravity and are moved to the beginning of
 * the line when the user deletes the line they were in.  Also, if the
 * user deletes a region of text which contained lines with markers,
 * those are deleted.
 *
 * Typical uses for a marker are bookmarks, breakpoints, current
 * executing instruction indication in a source file, etc..
 * 
 * Return value: a new #GtkSourceMarker, owned by the buffer.
 **/
GtkSourceMarker *
gtk_source_buffer_create_marker (GtkSourceBuffer   *buffer,
				 const gchar       *name,
				 const gchar       *type,
				 const GtkTextIter *where)
{
	GtkTextMark *text_mark;

	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (where != NULL, NULL);

	text_mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer),
						 name,
						 where,
						 TRUE);
	if (text_mark)
	{
		g_object_ref (text_mark);

		gtk_source_marker_set_marker_type (GTK_SOURCE_MARKER (text_mark), type);
		markers_insert (buffer, GTK_SOURCE_MARKER (text_mark));
		_gtk_source_marker_changed (GTK_SOURCE_MARKER (text_mark));

		return GTK_SOURCE_MARKER (text_mark);
	}

	return NULL;
}

static gint
markers_lookup (GtkSourceBuffer *buffer, GtkSourceMarker *marker)
{
	gint index, cmp;
	GtkTextIter iter;
	
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &iter,
					  GTK_TEXT_MARK (marker));
	
	index = markers_binary_search (buffer, &iter, &cmp);
	if (index >= 0 && cmp == 0)
	{
		if (g_array_index (buffer->priv->markers,
				   GtkSourceMarker *,
				   index) == marker)
			return index;
		return markers_linear_lookup (buffer, marker, index, 0);
	}
	return -1;
}

/**
 * gtk_source_buffer_move_marker:
 * @buffer: a #GtkSourceBuffer.
 * @marker: a #GtkSourceMarker in @buffer.
 * @where: the new location for the marker.
 * 
 * Moves @marker to the new location @where.
 **/
void 
gtk_source_buffer_move_marker (GtkSourceBuffer   *buffer,
			       GtkSourceMarker   *marker,
			       const GtkTextIter *where)
{
	gint index;
	
	g_return_if_fail (buffer != NULL && marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));
	g_return_if_fail (where != NULL);

	index = markers_lookup (buffer, marker);
	
	g_return_if_fail (index >= 0);
	
	/* unlink the marker first */
	_gtk_source_marker_changed (marker);
	_gtk_source_marker_unlink (marker);
	g_array_remove_index (buffer->priv->markers, index);

	gtk_text_buffer_move_mark (GTK_TEXT_BUFFER (buffer),
				   GTK_TEXT_MARK (marker),
				   where);

	/* re-link the marker using the new position */
	markers_insert (buffer, marker);

	/* FIXME: emit even the line number hasn't changed? - Gustavo */
	_gtk_source_marker_changed (marker);
}

/**
 * gtk_source_buffer_delete_marker:
 * @buffer: a #GtkSourceBuffer.
 * @marker: a #GtkSourceMarker in the @buffer.
 * 
 * Deletes @marker from the source buffer.  The same conditions as for
 * #GtkTextMark apply here.  The marker is no longer accessible from
 * the buffer, but if you held a reference to it, it will not be
 * destroyed.
 **/
void 
gtk_source_buffer_delete_marker (GtkSourceBuffer *buffer,
				 GtkSourceMarker *marker)
{
	gint index;
	
	g_return_if_fail (buffer != NULL && marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));

	index = markers_lookup (buffer, marker);
	
	g_return_if_fail (index >= 0);
	
	_gtk_source_marker_changed (marker);
	_gtk_source_marker_unlink (marker);
	g_array_remove_index (buffer->priv->markers, index);

	g_object_unref (marker);
	gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer),
				     GTK_TEXT_MARK (marker));
}

/**
 * gtk_source_buffer_get_marker:
 * @buffer: a #GtkSourceBuffer.
 * @name: name of the marker to retrieve.
 * 
 * Looks up the #GtkSourceMarker named @name in @buffer, returning
 * %NULL if it doesn't exists.
 * 
 * Return value: the #GtkSourceMarker whose name is @name, or %NULL.
 **/
GtkSourceMarker *
gtk_source_buffer_get_marker (GtkSourceBuffer *buffer,
			      const gchar     *name)
{
	GtkTextMark *text_mark;
	
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	text_mark = gtk_text_buffer_get_mark (GTK_TEXT_BUFFER (buffer), name);
	
	if (text_mark && markers_lookup (buffer, GTK_SOURCE_MARKER (text_mark)) < 0)
		text_mark = NULL;

	if (text_mark)
		return GTK_SOURCE_MARKER (text_mark);
	else
		return NULL;
}

#ifdef ENABLE_DEBUG
static void
marker_print (GtkSourceBuffer *buffer, GtkSourceMarker *marker)
{
	GtkTextIter iter;
	gchar *type;
	
	type = gtk_source_marker_get_marker_type (marker);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer), &iter, marker);
	
	g_print ("Marker [%p] at %d (type: %s)\n",
		 marker,
		 gtk_text_iter_get_offset (&iter),
		 type);
	g_free (type);
}

static void
markers_debug (GtkSourceBuffer *buffer)
{
	GtkSourceMarker *marker, *tmp;
	GArray *markers = buffer->priv->markers;
	gint i;
	
	if (markers->len == 0)
	{
		g_print ("No markers\n");
		return;
	}
	
	g_print ("Array contents:\n");
	for (i = 0; i < markers->len; i++)
		marker_print (buffer, g_array_index (markers, GtkSourceMarker *, i));

	g_print ("Linked list:\n");
	marker = g_array_index (markers, GtkSourceMarker *, 0);
	
	while ((tmp = gtk_source_marker_prev (marker)) != NULL)
		marker = tmp;
	while (marker)
	{
		marker_print (buffer, marker);
		marker = gtk_source_marker_next (marker);
	}
}
#endif

/**
 * gtk_source_buffer_get_markers_in_region:
 * @buffer: a #GtkSourceBuffer.
 * @begin: beginning of the range.
 * @end: end of the range.
 * 
 * Returns an <emphasis>ordered</emphasis> (by position) #GSList of
 * #GtkSourceMarker objects inside the region delimited by the
 * #GtkTextIter @begin and @end.  The iters may be in any order.
 * 
 * Return value: a #GSList of the #GtkSourceMarker inside the range.
 **/
GSList * 
gtk_source_buffer_get_markers_in_region (GtkSourceBuffer   *buffer,
					 const GtkTextIter *begin,
					 const GtkTextIter *end)
{
	GSList *result;
	GtkTextIter iter1, iter2;
	gint i, j, cmp;
	GArray *markers;
	
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (begin != NULL && end != NULL, NULL);

	DEBUG (g_print ("Getting markers for [%d,%d]\n",
			gtk_text_iter_get_offset (begin),
			gtk_text_iter_get_offset (end)));
	DEBUG (markers_debug (buffer));

	iter1 = *begin;
	iter2 = *end;
	gtk_text_iter_order (&iter1, &iter2);

	result = NULL;
	markers = buffer->priv->markers;

	i = markers_binary_search (buffer, &iter1, &cmp);
	if (i < 0)
		return NULL;
	
	if (cmp == 0)
		/* we got an exact match, which means the iter was at
		 * the position of the returned marker.  now we have
		 * to search backward for other markers at the same
		 * position */
		i = markers_linear_lookup (buffer, NULL, i, -1);
	else if (cmp > 0)
		i++;

	if (i >= markers->len)
		/* no markers to return */
		return NULL;

	j = markers_binary_search (buffer, &iter2, &cmp);
	if (cmp == 0)
		/* we got an exact match, which means the iter was at
		 * the position of the returned marker.  now we have
		 * to search forward for other markers at the same
		 * position */
		j = markers_linear_lookup (buffer, NULL, j, 1);
	else if (cmp < 0)
		j--;

	if (j < 0 || i > j)
		/* no markers to return */
		return NULL;

	/* build the resulting list */
	while (j >= i)
	{
		result = g_slist_prepend (result, g_array_index (markers, GtkSourceMarker *, j));
		j--;
	}

	DEBUG({
		GSList *l;
		
		g_print ("Returned list:\n");
		for (l = result; l; l = l->next)
			marker_print (buffer, l->data);
	});

	return result;
}

/**
 * gtk_source_buffer_get_first_marker:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Returns the first (nearest to the top of the buffer) marker in
 * @buffer.
 * 
 * Return value: a reference to the first #GtkSourceMarker, or %NULL if
 * there are no markers in the buffer.
 **/
GtkSourceMarker *
gtk_source_buffer_get_first_marker (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	if (buffer->priv->markers->len == 0)
		return NULL;

	return g_array_index (buffer->priv->markers, GtkSourceMarker *, 0);
}

/**
 * gtk_source_buffer_get_last_marker:
 * @buffer: a #GtkSourceBuffer.
 * 
 * Returns the last (nearest to the bottom of the buffer) marker in
 * @buffer.
 * 
 * Return value: a reference to the last #GtkSourceMarker, or %NULL if
 * there are no markers in the buffer.
 **/
GtkSourceMarker *
gtk_source_buffer_get_last_marker (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	if (buffer->priv->markers->len == 0)
		return NULL;

	return g_array_index (buffer->priv->markers,
			      GtkSourceMarker *,
			      buffer->priv->markers->len - 1);
}

/**
 * gtk_source_buffer_get_iter_at_marker:
 * @buffer: a #GtkSourceBuffer.
 * @iter: a #GtkTextIter to initialize.
 * @marker: a #GtkSourceMarker of @buffer.
 * 
 * Initializes @iter at the location of @marker.
 **/
void 
gtk_source_buffer_get_iter_at_marker (GtkSourceBuffer *buffer,
				      GtkTextIter     *iter,
				      GtkSourceMarker *marker)
{
	g_return_if_fail (buffer != NULL && marker != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_MARKER (marker));
	g_return_if_fail (!gtk_text_mark_get_deleted (GTK_TEXT_MARK (marker)));

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  iter,
					  GTK_TEXT_MARK (marker));
}

/**
 * gtk_source_buffer_get_next_marker:
 * @buffer: a #GtkSourceBuffer.
 * @iter: the location to start searching from.
 * 
 * Returns the nearest marker to the right of @iter.  If there are
 * multiple markers at the same position, this function will always
 * return the first one (from the internal linked list), even if
 * starting the search exactly at its location.  You can get the
 * others using gtk_source_marker_next().
 * 
 * Return value: the #GtkSourceMarker nearest to the right of @iter,
 * or %NULL if there are no more markers after @iter.
 **/
GtkSourceMarker *
gtk_source_buffer_get_next_marker (GtkSourceBuffer *buffer,
				   GtkTextIter     *iter)
{
	GtkSourceMarker *marker;
	GArray *markers;
	gint i, cmp;
	
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	marker = NULL;
	markers = buffer->priv->markers;

	i = markers_binary_search (buffer, iter, &cmp);
	if (i < 0)
		return NULL;
	
	if (cmp == 0)
		/* return the first marker at the iter position */
		i = markers_linear_lookup (buffer, NULL, i, -1);
	else if (cmp > 0)
		i++;

	if (i < markers->len)
	{
		marker = g_array_index (markers, GtkSourceMarker *, i);
		gtk_source_buffer_get_iter_at_marker (buffer, iter, marker);
	}
		
	return marker;
}

/**
 * gtk_source_buffer_get_prev_marker:
 * @buffer: a #GtkSourceBuffer.
 * @iter: the location to start searching from.
 * 
 * Returns the nearest marker to the left of @iter.  If there are
 * multiple markers at the same position, this function will always
 * return the last one (from the internal linked list), even if
 * starting the search exactly at its location.  You can get the
 * others using gtk_source_marker_prev().
 * 
 * Return value: the #GtkSourceMarker nearest to the left of @iter,
 * or %NULL if there are no more markers before @iter.
 **/
GtkSourceMarker *
gtk_source_buffer_get_prev_marker (GtkSourceBuffer *buffer,
				   GtkTextIter     *iter)
{
	GtkSourceMarker *marker;
	GArray *markers;
	gint i, cmp;
	
	g_return_val_if_fail (buffer != NULL, NULL);
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	marker = NULL;
	markers = buffer->priv->markers;

	i = markers_binary_search (buffer, iter, &cmp);
	if (i < 0)
		return NULL;
	
	if (cmp == 0)
		/* return the last marker at the iter position */
		i = markers_linear_lookup (buffer, NULL, i, 1);
	else if (cmp < 0)
		i--;

	if (i >= 0)
	{
		marker = g_array_index (markers, GtkSourceMarker *, i);
		gtk_source_buffer_get_iter_at_marker (buffer, iter, marker);
	}
		
	return marker;
}


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
#include "gtksourcefold-private.h"

#include "gtksourceundomanager.h"
#include "gtksourceview-marshal.h"

#include "gtksourceiter.h"
#include "gtksourcesimpleengine.h"	
#include "gtksourcetag.h"

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

#define MAX_CHARS_BEFORE_FINDING_A_MATCH    2000

/* Signals */
enum {
	CAN_UNDO = 0,
	CAN_REDO,
	HIGHLIGHT_UPDATED,
	MARKER_UPDATED,
	FOLD_ADDED,
	FOLD_REMOVE,
	TEXT_INSERTED,
	TEXT_DELETED,
	UPDATE_HIGHLIGHT,
	LAST_SIGNAL
};

/* Properties */
enum {
	PROP_0,
	PROP_CHECK_BRACKETS,
	PROP_HIGHLIGHT,
	PROP_MAX_UNDO_LEVELS,
	PROP_LANGUAGE,
	PROP_FOLDS
};

struct _GtkSourceBufferPrivate 
{
	gint                   highlight:1;
	gint                   check_brackets:1;

	GtkTextTag            *bracket_match_tag;
	GtkTextMark           *bracket_mark;
	guint                  bracket_found:1;
	
	GArray                *markers;

	GtkSourceLanguage     *language;

	GtkSourceEngine       *highlight_engine;

	GtkSourceUndoManager  *undo_manager;
	
	gint                   enable_folds:1;
	GList                 *folds;
};



static GtkTextBufferClass *parent_class = NULL;
static guint 	 buffer_signals[LAST_SIGNAL] = { 0 };

static void 	 gtk_source_buffer_class_init		(GtkSourceBufferClass    *klass);
static void 	 gtk_source_buffer_init			(GtkSourceBuffer         *klass);
static GObject  *gtk_source_buffer_constructor          (GType                    type,
							 guint                    n_construct_properties,
							 GObjectConstructParam   *construct_param);
static void 	 gtk_source_buffer_finalize		(GObject                 *object);
static void 	 gtk_source_buffer_dispose		(GObject                 *object);
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
							 const GtkTextIter       *iter,
							 GtkTextMark             *mark, 
							 gpointer                 data);

static void 	 gtk_source_buffer_real_insert_text 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 const gchar             *text,
							 gint                     len);
static void 	 gtk_source_buffer_real_delete_range 	(GtkTextBuffer           *buffer,
							 GtkTextIter             *iter,
							 GtkTextIter             *end);

static gboolean	 gtk_source_buffer_find_bracket_match_real (GtkTextIter          *orig, 
							    gint                  max_chars);

static void	 gtk_source_buffer_real_remove_fold	(GtkSourceBuffer         *buffer,
							 GtkSourceFold           *fold);


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
	object_class->dispose	   = gtk_source_buffer_dispose;
	object_class->get_property = gtk_source_buffer_get_property;
	object_class->set_property = gtk_source_buffer_set_property;
	
	klass->can_undo 	 = NULL;
	klass->can_redo 	 = NULL;
	klass->highlight_updated = NULL;
	klass->marker_updated    = NULL;
	klass->fold_added        = NULL;
	klass->fold_remove	 = gtk_source_buffer_real_remove_fold;
	klass->text_inserted     = NULL;
	klass->text_deleted      = NULL;
	klass->update_highlight  = NULL;
	
	/* Do not set these signals handlers directly on the parent_class since
	 * that will cause problems (a loop). */
	tb_class->insert_text 	= gtk_source_buffer_real_insert_text;
	tb_class->delete_range 	= gtk_source_buffer_real_delete_range;

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
	
	g_object_class_install_property (object_class,
					 PROP_FOLDS,
					 g_param_spec_boolean ("folds",
							       _("Folds"),
							       _("Whether folds are enabled"),
							       FALSE,
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

	buffer_signals[FOLD_ADDED] =
	    g_signal_new ("fold_added",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, fold_added),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED,
			  G_TYPE_NONE, 
			  1, 
			  GTK_TYPE_SOURCE_FOLD | G_SIGNAL_TYPE_STATIC_SCOPE);

	buffer_signals[FOLD_REMOVE] =
	    g_signal_new ("fold_remove",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, fold_remove),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED,
			  G_TYPE_NONE, 
			  1, 
			  GTK_TYPE_SOURCE_FOLD | G_SIGNAL_TYPE_STATIC_SCOPE);

	buffer_signals[TEXT_INSERTED] =
	    g_signal_new ("text_inserted",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, text_inserted),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED_BOXED,
			  G_TYPE_NONE, 
			  2, 
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);

	buffer_signals[TEXT_DELETED] =
	    g_signal_new ("text_deleted",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, text_deleted),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED_INT_STRING_BOOLEAN,
			  G_TYPE_NONE, 
			  4, 
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			  G_TYPE_INT | G_SIGNAL_TYPE_STATIC_SCOPE,
			  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
			  G_TYPE_BOOLEAN | G_SIGNAL_TYPE_STATIC_SCOPE);

	buffer_signals[UPDATE_HIGHLIGHT] =
	    g_signal_new ("update_highlight",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, update_highlight),
			  NULL, NULL,
			  gtksourceview_marshal_VOID__BOXED_BOXED_BOOLEAN,
			  G_TYPE_NONE, 
			  3, 
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
			  G_TYPE_BOOLEAN);
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
	priv->enable_folds = FALSE;
	priv->folds = NULL;
	
	priv->markers = g_array_new (FALSE, FALSE, sizeof (GtkSourceMarker *));

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

static GObject *
gtk_source_buffer_constructor (GType                  type,
			       guint                  n_construct_properties,
			       GObjectConstructParam *construct_param)
{
	GObject *g_object;
	gint i;

	/* Check the construction parameters to see if the user
	 * specified a tag-table, and create and set an empty
	 * GtkSourceTagTable if he didn't */
	for (i = 0; i < n_construct_properties; i++)
	{
		if (!strcmp ("tag-table", construct_param [i].pspec->name) &&
		    g_value_get_object (construct_param [i].value) == NULL)
		{
#if (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION <= 2)
			g_value_set_object_take_ownership (construct_param [i].value,
							   gtk_source_tag_table_new ());
#else
			g_value_take_object (construct_param [i].value,
					     gtk_source_tag_table_new ());
#endif

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

		/* Create invisibility tag for folding lines. */
		gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (source_buffer),
					    INVISIBLE_LINE, "invisible", TRUE, NULL);

	}
	
	return g_object;
}

static void
gtk_source_buffer_finalize (GObject *object)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	buffer = GTK_SOURCE_BUFFER (object);
	g_return_if_fail (buffer->priv != NULL);
	
	if (buffer->priv->markers)
		g_array_free (buffer->priv->markers, TRUE);

	g_free (buffer->priv);
	buffer->priv = NULL;
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void 
gtk_source_buffer_dispose (GObject *object)
{
	GtkSourceBuffer *buffer;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (object));

	buffer = GTK_SOURCE_BUFFER (object);
	g_return_if_fail (buffer->priv != NULL);
	
	if (buffer->priv->undo_manager != NULL)
	{
		g_object_unref (buffer->priv->undo_manager);
		buffer->priv->undo_manager = NULL;
	}

	if (buffer->priv->highlight_engine != NULL)
	{
		gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
		g_object_unref (buffer->priv->highlight_engine);
		buffer->priv->highlight_engine = NULL;
	}

	if (buffer->priv->language != NULL)
	{
		g_object_unref (buffer->priv->language);
		buffer->priv->language = NULL;
	}
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
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
			
		case PROP_FOLDS:
			gtk_source_buffer_set_folds_enabled (source_buffer,
							     g_value_get_boolean (value));
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
		
		case PROP_FOLDS:
			g_value_set_boolean (value, source_buffer->priv->enable_folds);
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
gtk_source_buffer_move_cursor (GtkTextBuffer     *buffer,
			       const GtkTextIter *iter, 
			       GtkTextMark       *mark, 
			       gpointer           data)
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
	if (gtk_source_buffer_find_bracket_match_real (&iter1, MAX_CHARS_BEFORE_FINDING_A_MATCH)) 
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
	gint insert_offset;
	GtkSourceFold *fold;
	GtkTextIter insert_iter;
	
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	insert_offset = gtk_text_iter_get_offset (iter);
	
	/* if the user tries to insert text into a folded section, don't insert
	 * the text, but unfold the region.
	 */
	fold = gtk_source_buffer_get_fold_at_iter (GTK_SOURCE_BUFFER (buffer),
						   iter);
	if (fold != NULL && fold->folded)
	{
		gtk_source_fold_set_folded (fold, FALSE);
		return;
	}
	
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

	gtk_text_buffer_get_iter_at_offset (buffer, &insert_iter, insert_offset);
	g_signal_emit (buffer, buffer_signals [TEXT_INSERTED], 0, &insert_iter, iter);
}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *start,
				     GtkTextIter   *end)
{
	gint delta, end_offset;
	GtkTextMark *mark;
	GtkTextIter iter, insert_iter;
	GSList *markers;
	gchar *text;
	gboolean forward;
		
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

	/* if the delete range intersects a folded region, don't delete any text;
	 * unfold the region instead. */
	if (GTK_SOURCE_BUFFER (buffer)->priv->enable_folds)
	{
		GtkSourceFold *fold;

		/* do we start in a folded region? */
		fold = gtk_source_buffer_get_fold_at_iter (GTK_SOURCE_BUFFER (buffer),
							   start);
		if (fold != NULL && fold->folded)
		{
			GtkTextIter fold_begin;
			
			/* If the start of delete range is the same as the start
			 * of the fold, allow the delete to proceed. */
			gtk_source_fold_get_bounds (fold, &fold_begin, NULL);
			if (!gtk_text_iter_equal (start, &fold_begin))
			{
				gtk_source_fold_set_folded (fold, FALSE);
				return;
			}
		}

		/* do we end in a folded region? */		
		fold = gtk_source_buffer_get_fold_at_iter (GTK_SOURCE_BUFFER (buffer),
							   end);
		if (fold != NULL && fold->folded)
		{
			GtkTextIter fold_end;
			
			/* If the end of delete range is the same as the end
			 * of the fold, allow the delete to proceed. */
			gtk_source_fold_get_bounds (fold, NULL, &fold_end);
			if (!gtk_text_iter_equal (end, &fold_end))
			{
				gtk_source_fold_set_folded (fold, FALSE);
				return;
			}
		}
	}

	/* save slice of deleted text */
	text = gtk_text_buffer_get_slice (buffer, start, end, TRUE);
	
	gtk_text_iter_order (start, end);
	delta = gtk_text_iter_get_offset (start) - 
			gtk_text_iter_get_offset (end);
	end_offset = gtk_text_iter_get_offset (end);

	/* figure out if the user used the Delete or the Backspace key */
	gtk_text_buffer_get_iter_at_mark (buffer, &insert_iter,
					  gtk_text_buffer_get_insert (buffer));
	if (gtk_text_iter_get_offset (&insert_iter) <= gtk_text_iter_get_offset (start))
		forward = TRUE;
	else
		forward = FALSE;

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

	/* emit text deleted for engines and undomanager */
	g_signal_emit (buffer, buffer_signals [TEXT_DELETED], 0,
		       start, end_offset, text, forward);
	g_free (text);
}

/* FIXME: this can't be here, but it's now needed for bracket matching */
static const GtkSourceTag *
iter_has_syntax_tag (const GtkTextIter *iter)
{
	const GtkSourceTag *tag;
	GSList *list;
	GSList *l;

	g_return_val_if_fail (iter != NULL, NULL);

	list = gtk_text_iter_get_tags (iter);
	tag = NULL;

	l = list;

	while ((list != NULL) && (tag == NULL)) {
		if (GTK_IS_SOURCE_TAG (list->data))
			tag = GTK_SOURCE_TAG (list->data);
		list = g_slist_next (list);
	}

	g_slist_free (l);

	return tag;
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

	const GtkSourceTag *base_tag;

	iter = *orig;

	if (!gtk_text_iter_backward_char (&iter))
		return FALSE;
	
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

	return gtk_source_buffer_find_bracket_match_real (iter, -1);
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
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (buffer->priv->highlight == highlight)
		return;

	buffer->priv->highlight = highlight;

	g_object_notify (G_OBJECT (buffer), "highlight");
}


/**
 * gtk_source_buffer_set_language:
 * @buffer: a #GtkSourceBuffer.
 * @language: a #GtkSourceLanguage to set, or %NULL.
 * 
 * Sets the #GtkSourceLanguage the source buffer will use, adding
 * #GtkSourceTag tags with the language's patterns. Note that
 * this will remove any #GtkSourceTag tags currently in the buffer's
 * tag table.  The buffer holds a reference to the @language set.
 **/
void
gtk_source_buffer_set_language (GtkSourceBuffer   *buffer, 
				GtkSourceLanguage *language)
{
	GtkSourceTagTable *table;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (buffer->priv->language == language)
		return;

	table = GTK_SOURCE_TAG_TABLE (gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer)));

	if (buffer->priv->language != NULL)
	{
		/* remove previous tags */
		/* FIXME: should use tag grouping when it's
		 * implemented in GtkSourceTagTable */
		gtk_source_tag_table_remove_source_tags (table);

		g_object_unref (buffer->priv->language);
	}

	if (buffer->priv->highlight_engine)
	{
		/* disconnect the old engine */
		gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
		g_object_unref (buffer->priv->highlight_engine);
		buffer->priv->highlight_engine = NULL;
	}
	
	buffer->priv->language = language;

	if (language != NULL)
	{
		GSList *list = NULL;
			
		g_object_ref (language);

		/* get the style tags */
		/* FIXME: keep the tag group when that's implemented in GtkSourceTagTable */
		list = gtk_source_language_get_tags (language);		
 		gtk_source_tag_table_add_tags (table, list);

		g_slist_foreach (list, (GFunc)g_object_unref, NULL);
		g_slist_free (list);

		/* get a new engine */
		buffer->priv->highlight_engine = gtk_source_language_create_engine (language);
		if (buffer->priv->highlight_engine)
			gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, buffer);
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

static GQuark
gtk_source_buffer_error_quark (void)
{
	static GQuark q = 0;
	
	if (q == 0)
		q = g_quark_from_static_string ("gtk-source-buffer-error-quark");

	return q;
}

gboolean
gtk_source_buffer_get_folds_enabled (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->enable_folds;
}

static void
foreach_fold_region (gpointer data, gpointer user_data)
{
	gtk_source_buffer_real_remove_fold (GTK_SOURCE_BUFFER (user_data), data);
}

void
gtk_source_buffer_set_folds_enabled (GtkSourceBuffer *buffer,
				     gboolean         enable_folds)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	enable_folds = (enable_folds != FALSE);

	if (buffer->priv->enable_folds == enable_folds)
		return;

	buffer->priv->enable_folds = enable_folds;

	/* Remove all existing folds if folds are disabled. */
	if (!enable_folds && buffer->priv->folds != NULL)
	{
		GList *folds = g_list_copy (buffer->priv->folds);
		g_list_foreach (folds, foreach_fold_region, buffer);
		g_list_free (folds);
	}

	g_object_notify (G_OBJECT (buffer), "folds");
}

/**
 * insert_child_fold:
 * @buffer: a #GtkSourceBuffer.
 * @child: the new #GtkSourceFold to insert.
 * @parent: the #GtkSourceFold parent to which we are trying to insert the child.
 * @error: a possible #GError returned through the recursive loop.
 * 
 * Inserts the specified child #GtkSourceFold into the fold tree somewhere or
 * returns FALSE. In the latter case, a #GError is also set to indicate an
 * illegal operation.
 *
 * This method is called from gtk_source_buffer_add_fold initially with a
 * top-level #GtkSourceFold. After that, the method calls itself recursively
 * until it has either found a #GtkSourceFold where it can insert the child or
 * detected an illegal operation and set @error.
 * 
 * Return value: TRUE if the insertion was succesful, FALSE if not. @error is
 * set when FALSE is returned.
 **/
static gboolean
insert_child_fold (GtkSourceBuffer *buffer,
		   GtkSourceFold   *child,
		   GtkSourceFold   *parent,
		   GError         **error)
{
	GtkTextIter begin, end, pbegin, pend, iter;
	
	/* If error is set, then return immediately; don't recurse further. */
	if (*error)
		return FALSE;
	
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &begin, child->start_line);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &end, child->end_line);

	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &pbegin, parent->start_line);
	gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
					  &pend, parent->end_line);

	DEBUG (g_message ("insert child (%d, %d) into parent (%d, %d)",
			  gtk_text_iter_get_line (&begin),
			  gtk_text_iter_get_line (&end),
			  gtk_text_iter_get_line (&pbegin),
			  gtk_text_iter_get_line (&pend)));

	/* There are 3 major codepaths in this method:
	 * 1. The child fold falls completely inside the parent fold. Try to add
	 *    the child fold to children of the parent fold recursively. Otherwise
	 *    append the child fold to the parent.
	 * 2. The child fold overlapses the parent fold. This means that the
	 *    start of the child fold is before the start of the parent and the
	 *    end of the child is after the end of the parent. In this case, 
	 *    parent & child need to be reparented (parent becomes child and 
	 *    child becomes the parent. There's additional logic to check if any
	 *    siblings of the parent need to be reparented.
	 * 3. The child fold intersects with the parent fold. This is an illegal
	 *    operation and will results in FALSE being returned through the
	 *    recursion and the error parameter to be set.
	 */

	/* check if the child fold is within the parent fold. */
	if (gtk_text_iter_compare (&pbegin, &begin) == -1 &&
	    gtk_text_iter_compare (&pend, &end) == 1)
	{
		GList *folds = parent->children;
		GList *last_fold = folds;
		
		/* fold already has children; try inserting it into one. */
		while (folds != NULL)
		{
			GtkSourceFold *child_fold = folds->data;
		
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
							  &iter, child_fold->start_line);
			/* check if the current fold is past the new fold. */
			if (gtk_text_iter_compare (&iter, &end) == 1)
			{
				DEBUG (g_message ("adding fold before child @ %d",
						  gtk_text_iter_get_line (&iter)));
				parent->children = g_list_insert_before (parent->children,
									 folds, child);
				child->parent = parent;
				return TRUE;
			}

			/* try inserting the child fold recursively. */
			if (insert_child_fold (buffer, child, child_fold, error))
				return TRUE;
			
			if (*error)
				return FALSE;
			
			last_fold = folds;
			folds = g_list_next (folds);
		}

		/* Fold is inside parent, but not inside a child of parent.
		 * Append the child fold to the parent. If the child was added
		 * succesfully already, then we never get to this point (we
		 * return in the while loop above). */
		DEBUG (g_message ("adding fold to parent fold @ %d",
				  gtk_text_iter_get_line (&pbegin)));
		if (last_fold == NULL)
			parent->children = g_list_append (NULL, child);
		else
			parent->children = g_list_append (last_fold, child);
		child->parent = parent;
		return TRUE;
	}
	/* check if the child fold overlaps the parent fold. */
	else if (gtk_text_iter_compare (&pbegin, &begin) == 1 &&
		 gtk_text_iter_compare (&pend, &end) == -1)
	{
		GtkSourceFold *sibling;
		GList *siblings, *reparent, *l, *first, *last;
		
		/* If the parent is a root fold, the "siblings" are actually
		 * the other root folds. Else just get the fold children. */
		if (parent->parent != NULL)
			siblings = g_list_find (parent->parent->children, parent);
		else
			siblings = g_list_find (buffer->priv->folds, parent);
		
		reparent = g_list_append (NULL, parent);

		DEBUG (g_message ("child overlaps parent; need to reparent..."));
		
		/* We need to determine which siblings the child overlapses.
		 * Those siblings need to be reparented to the child. If the
		 * child intersects a sibling, then this operation is invalid.
		 *
		 * |		<- parent fold
		 * | |		<- child fold (to be inserted)
		 * | | [	<- first sibling (reparent)
		 * | |
		 * | | [	<- second sibling (reparent)
		 * | |
		 * |
		 * | [		<- third sibling (don't reparent)
		 * |
		 */
		
		/* The parent fold is the first in the list. Since we've already
		 * added it to the reparent list, we skip it here. */
		siblings = g_list_next (siblings);
		
		/* First determine which children to reparent. */
		while (siblings != NULL)
		{
			sibling = siblings->data;
		
			gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
							  &pend, sibling->end_line);
			
			/* check if we are past the last overlapped sibling. */
			if (gtk_text_iter_compare (&end, &pend) == -1)
			{
				gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
								  &pbegin, sibling->start_line);
				if (gtk_text_iter_compare (&end, &pbegin) != -1)
				{
					g_set_error (error,
						     gtk_source_buffer_error_quark (),
						     0,
						     "Cannot add child fold: new fold [%d-%d] intersects with [%d-%d]",
						     gtk_text_iter_get_line (&begin),
						     gtk_text_iter_get_line (&end),
						     gtk_text_iter_get_line (&pbegin),
						     gtk_text_iter_get_line (&pend));
					g_list_free (reparent);
					return FALSE;
				}
				
				break;
			}
			
			DEBUG (g_message ("reparenting @ %d", gtk_text_iter_get_line (&pend)));
			
			reparent = g_list_prepend (reparent, sibling);
			siblings = g_list_next (siblings);
		}

		/* reparent first sibling to child. */
		reparent = g_list_reverse (reparent);
		sibling = reparent->data;
		
		/* if sibling->parent is NULL, then it's a root fold. */
		if (sibling->parent == NULL)
		{
			siblings = g_list_find (buffer->priv->folds, sibling);
			g_return_val_if_fail (siblings != NULL, FALSE);
			siblings->data = child;
			child->children = g_list_append (child->children, sibling);
			child->parent = NULL;
			sibling->parent = child;
		}
		else
		{
			siblings = g_list_find (sibling->parent->children, sibling);
			g_return_val_if_fail (siblings != NULL, FALSE);
			siblings->data = child;
			child->children = g_list_append (child->children, sibling);
			child->parent = sibling->parent;
			sibling->parent = child;
		}
		
		/* reparent all the following siblings as well. */
		l = g_list_next (reparent);
		first = last = NULL;
		while (l != NULL)
		{
			sibling = l->data;
			
			if (first == NULL)
			{
				if (sibling->parent != NULL)
					first = g_list_find (sibling->parent->children,
							     sibling);
				else
					first = g_list_find (buffer->priv->folds, 
							     sibling);
					
				g_return_val_if_fail (first != NULL, FALSE);
											     
				last = first;
			}
			
			sibling->parent = child;
			
			last = g_list_next (last);
			l = g_list_next (l);
		}
		
		/* Check if there are any siblings left to reparent. */
		if (first != NULL)
		{
			if (first->prev)
				first->prev->next = last;
		
			if (last)
			{
				last->prev->next = NULL;
				last->prev = first->prev;
			}
			
			first->prev = NULL;
				
			child->children = g_list_concat (child->children, first);
		}
		
		g_list_free (reparent);
	
		return TRUE;
	}
	else if (gtk_text_iter_in_range (&pbegin, &begin, &end) ||
		 gtk_text_iter_in_range (&pend, &begin, &end) ||
		 gtk_text_iter_equal (&pbegin, &begin) ||
		 gtk_text_iter_equal (&pend, &end))
	{
		g_set_error (error,
			     gtk_source_buffer_error_quark (),
			     0,
			     "Cannot add child fold: new fold [%d-%d] intersects with [%d-%d]",
			     gtk_text_iter_get_line (&begin),
			     gtk_text_iter_get_line (&end),
			     gtk_text_iter_get_line (&pbegin),
			     gtk_text_iter_get_line (&pend));
	}
	
	return FALSE;
}

GtkSourceFold *
gtk_source_buffer_add_fold (GtkSourceBuffer   *buffer,
			    const GtkTextIter *begin,
			    const GtkTextIter *end)
{
	GList *folds, *last_fold;
	GtkSourceFold *fold, *parent;
	GtkTextIter iter;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (begin != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);

	DEBUG (g_message ("add fold @ %d, %d",
			  gtk_text_iter_get_line (begin),
			  gtk_text_iter_get_line (end)));

	fold = _gtk_source_fold_new (buffer, begin, end);
	
	/* Insert the fold either at the root level or as a child of an existing fold. */
	folds = buffer->priv->folds;
	last_fold = folds;
	while (folds != NULL)
	{
		GError *error = NULL;
		
		parent = folds->data;

		/* check if the current fold is past the new fold. */
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
						  &iter, parent->start_line);
		if (gtk_text_iter_compare (&iter, end) == 1)
		{
			DEBUG (g_message ("adding fold before root %d", gtk_text_iter_get_line (&iter)));
			buffer->priv->folds = g_list_insert_before (buffer->priv->folds,
								    folds, fold);
			break;
		}

		/* try adding the child to all folds in the region. */
		if (insert_child_fold (buffer, fold, parent, &error))
			break;
		
		if (error != NULL)
		{
			g_critical (error->message);
			g_error_free (error);
			gtk_source_fold_free (fold);
			return NULL;
		}
	
		last_fold = folds;
		folds = g_list_next (folds);
	}
	
	/* add the fold at the end of the list. */
	if (folds == NULL)
	{
		GList *dummy;
		
		DEBUG (g_message ("adding fold at end of root"));
		if (last_fold == NULL)
			buffer->priv->folds = g_list_append (NULL, fold);
		else
			dummy = g_list_append (last_fold, fold);
	}
	
	g_signal_emit (G_OBJECT (buffer), buffer_signals [FOLD_ADDED], 0, fold);
	
	return fold;
}

static void
gtk_source_buffer_real_remove_fold (GtkSourceBuffer *buffer,
				    GtkSourceFold   *fold)
{
	GList *l;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (fold != NULL);
	
	if (fold->folded)
		gtk_source_fold_set_folded (fold, FALSE);
	
	gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), fold->start_line);
	gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), fold->end_line);
	
	l = g_list_find (buffer->priv->folds, fold);
	if (l != NULL)
		buffer->priv->folds = g_list_delete_link (buffer->priv->folds, l);
	
	g_list_foreach (fold->children, foreach_fold_region, buffer);

	gtk_source_fold_free (fold);
}

void
gtk_source_buffer_remove_fold (GtkSourceBuffer *buffer,
			       GtkSourceFold   *fold)
{
	g_signal_emit (G_OBJECT (buffer), buffer_signals [FOLD_REMOVE], 0, fold);
}

/**
 * get_folds_in_region:
 * @buffer: a #GtkSourceBuffer.
 * @begin: the begin point of the region.
 * @end: the end point of the region.
 * @fold: #GtkSourceFold which might or might not lie in the region.
 * @list: the list of #GtkSourceFold's in the region.
 * 
 * This method is called from gtk_source_buffer_get_folds_in_region to create a
 * list of #GtkSourceFold's of which the *start* lies in the specified region.
 * This method recurses through the fold hierarchy to create the flattened list.
 * See gtk_source_buffer_get_folds_in_region for more information.
 **/
static void
get_folds_in_region (GtkTextBuffer     *buffer,
		     const GtkTextIter *begin,
		     const GtkTextIter *end,
		     GtkSourceFold     *fold,
		     GList            **list)
{
	GtkTextIter fbegin, fend;
	GList *children;

	gtk_text_buffer_get_iter_at_mark (buffer, &fbegin, fold->start_line);
	gtk_text_buffer_get_iter_at_mark (buffer, &fend, fold->end_line);

	/* the region lies in the fold, so add possible children in the region. */
	if (gtk_text_iter_compare (&fbegin, begin) == -1 &&
	    gtk_text_iter_compare (&fend, begin) == 1)
	{
		children = fold->children;
		while (!fold->folded && children != NULL)
		{
			get_folds_in_region (buffer, begin, end, children->data, list);
			children = g_list_next (children);
		}
	}
	/* the entire fold lies in the region, so add the fold + children. */
	else if ((gtk_text_iter_compare (&fbegin, begin) >= 0 &&
	          gtk_text_iter_compare (&fend, end) <= 0) ||
	/* start iter is in the region, so add the fold first. */
	         (gtk_text_iter_compare (&fbegin, begin) >= 0 &&
	          gtk_text_iter_compare (&fbegin, end) <= 0))
	{
		*list = g_list_append (*list, fold);
		children = fold->children;
		while (!fold->folded && children != NULL)
		{
			get_folds_in_region (buffer, begin, end, children->data, list);
			children = g_list_next (children);
		}
	}
}

/**
 * gtk_source_buffer_get_folds_in_region:
 * @buffer: a #GtkSourceBuffer.
 * @begin: the begin point of the region.
 * @end: the end point of the region.
 * 
 * Returns a list of all folds in the specified region. This is a flattened list
 * of the parent->child fold hierarchy. This function is mainly used in 
 * gtk_source_view_get_lines to determine which folds to draw.
 *
 * This method returns the folds of which the start lies in the region. So if
 * a fold begins before the region, the fold itself isn't returned, but its
 * children might.
 * 
 * Return value: a #GList of the #GtkSourceFold's in the region, or %NULL if
 * there are no folds in the region.
 **/
GList *
_gtk_source_buffer_get_folds_in_region (GtkSourceBuffer   *buffer,
					const GtkTextIter *begin,
					const GtkTextIter *end)
{
	GList *result, *folds;

	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (begin != NULL && end != NULL, NULL);

	result = NULL;
	folds = buffer->priv->folds;
	while (folds != NULL)
	{
		GtkSourceFold *fold = folds->data;
		GtkTextIter iter;

		/* break when we are past the end of the region. */
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
						  &iter, fold->start_line);
		if (gtk_text_iter_compare (&iter, end) == 1)
			break;

		get_folds_in_region (GTK_TEXT_BUFFER (buffer), begin, end, fold, &result);

		folds = g_list_next (folds);
	}

	return result;
}

static void
remove_folds_in_region (GtkTextBuffer     *buffer,
			const GtkTextIter *begin,
			const GtkTextIter *end,
			GtkSourceFold     *fold)
{
	GtkTextIter fbegin, fend;
	GList *children;

	gtk_text_buffer_get_iter_at_mark (buffer, &fbegin, fold->start_line);
	gtk_text_buffer_get_iter_at_mark (buffer, &fend, fold->end_line);

	/* the region lies in the fold, so remove possible children in the region. */
	if (gtk_text_iter_compare (&fbegin, begin) == -1 &&
	    gtk_text_iter_compare (&fend, begin) == 1)
	{
		children = fold->children;
		while (!fold->folded && children != NULL)
		{
			remove_folds_in_region (buffer, begin, end, children->data);
			children = g_list_next (children);
		}
	}
	/* start iter is in the region, so add the fold first. */
	else if (gtk_text_iter_compare (&fbegin, begin) >= 0 &&
	         gtk_text_iter_compare (&fbegin, end) <= 0)
	{
		gtk_source_buffer_remove_fold (GTK_SOURCE_BUFFER (buffer), fold);
	}
}

void
gtk_source_buffer_remove_folds_in_region (GtkSourceBuffer   *buffer,
					  const GtkTextIter *begin,
					  const GtkTextIter *end)
{
	GList *folds;

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (begin != NULL && end != NULL);

	folds = buffer->priv->folds;
	while (folds != NULL)
	{
		GtkSourceFold *fold = folds->data;
		GtkTextIter iter;

		/* break when we are past the end of the region. */
		gtk_text_buffer_get_iter_at_mark (GTK_TEXT_BUFFER (buffer),
						  &iter, fold->start_line);
		if (gtk_text_iter_compare (&iter, end) == 1)
			break;

		remove_folds_in_region (GTK_TEXT_BUFFER (buffer), begin, end, fold);

		folds = g_list_next (folds);
	}
}

static GtkSourceFold *
find_fold_at_line (GtkTextBuffer *buffer,
		   GList         *folds,
		   gint           line)
{
	GtkSourceFold *fold;
	GtkTextIter iter;
	gint start_line, end_line;
			
	while (folds != NULL)
	{
		fold = folds->data;
		
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, fold->start_line);
		start_line = gtk_text_iter_get_line (&iter);
		gtk_text_buffer_get_iter_at_mark (buffer, &iter, fold->end_line);
		
		/* The end iter of the fold is on the next line, so if the end
		 * iter is at the start of the line, go back a line. */
		if (gtk_text_iter_starts_line (&iter))
			gtk_text_iter_backward_line (&iter);
		end_line = gtk_text_iter_get_line (&iter);
		
		if (line >= start_line && line <= end_line)
		{
			GtkSourceFold *child;
		
			if (!fold->children)
				return fold;
			
			child = find_fold_at_line (buffer, fold->children, line);
			if (child)
				return child;
			else
				return fold;
		}
		else if (line < start_line)
		{
			return NULL;
		}
	
		folds = g_list_next (folds);
	}
	
	return NULL;
}

GtkSourceFold *
_gtk_source_buffer_get_fold_at_line (GtkSourceBuffer *buffer,
				     gint             line)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (line >= 0, NULL);

	return find_fold_at_line (GTK_TEXT_BUFFER (buffer),
				  buffer->priv->folds,
				  line);
}

static GtkSourceFold *
find_fold_at_iter (GtkTextBuffer     *buffer,
		   GList             *folds,
		   const GtkTextIter *iter)
{
	GtkSourceFold *fold;
	GtkTextIter start_iter, end_iter;
	
	while (folds != NULL)
	{
		fold = folds->data;
		
		gtk_text_buffer_get_iter_at_mark (buffer, &start_iter, fold->start_line);
		gtk_text_buffer_get_iter_at_mark (buffer, &end_iter, fold->end_line);
		
		if (gtk_text_iter_compare (&start_iter, iter) <= 0 &&
		    gtk_text_iter_compare (&end_iter, iter) >= 0)
		{
			GtkSourceFold *child;
		
			if (!fold->children)
				return fold;
			
			child = find_fold_at_iter (buffer, fold->children, iter);
			if (child)
				return child;
			else
				return fold;
		}
		else if (gtk_text_iter_compare (iter, &start_iter) == -1)
		{
			return NULL;
		}
	
		folds = g_list_next (folds);
	}
	
	return NULL;
}

GtkSourceFold *
gtk_source_buffer_get_fold_at_iter (GtkSourceBuffer   *buffer,
				    const GtkTextIter *iter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	return find_fold_at_iter (GTK_TEXT_BUFFER (buffer),
				  buffer->priv->folds,
				  iter);
}

const GList *
gtk_source_buffer_get_root_folds (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	
	return buffer->priv->folds;
}

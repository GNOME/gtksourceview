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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "gtksourceview-i18n.h"
#include "gtksourcelanguage-private.h"
#include "gtksourcebuffer.h"
#include "gtksourceundomanager.h"
#include "gtksourceview-marshal.h"
#include "gtksourceiter.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcestyle-private.h"

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
	HIGHLIGHT_UPDATED,
	LAST_SIGNAL
};

/* Properties */
enum {
	PROP_0,
	PROP_CAN_UNDO,
	PROP_CAN_REDO,
	PROP_HIGHLIGHT_SYNTAX,
	PROP_HIGHLIGHT_MATCHING_BRACKETS,
	PROP_MAX_UNDO_LEVELS,
	PROP_LANGUAGE,
	PROP_STYLE_SCHEME
};

struct _GtkSourceBufferPrivate
{
	gint                   highlight_syntax:1;
	gint                   highlight_brackets:1;

	GtkTextTag            *bracket_match_tag;
	GtkTextMark           *bracket_mark;
	guint                  bracket_found:1;

	GtkSourceLanguage     *language;

	GtkSourceEngine       *highlight_engine;
	GtkSourceStyleScheme  *style_scheme;

	GtkSourceUndoManager  *undo_manager;
};

G_DEFINE_TYPE (GtkSourceBuffer, gtk_source_buffer, GTK_TYPE_TEXT_BUFFER)


static guint 	 buffer_signals[LAST_SIGNAL];

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

static gboolean	 gtk_source_buffer_find_bracket_match_with_limit (GtkTextIter    *orig,
								  gint            max_chars);

static void
gtk_source_buffer_class_init (GtkSourceBufferClass *klass)
{
	GObjectClass        *object_class;
	GtkTextBufferClass  *tb_class;
	GType                param_types[2];

	object_class 	= G_OBJECT_CLASS (klass);
	tb_class	= GTK_TEXT_BUFFER_CLASS (klass);

	object_class->finalize	   = gtk_source_buffer_finalize;
	object_class->dispose	   = gtk_source_buffer_dispose;
	object_class->get_property = gtk_source_buffer_get_property;
	object_class->set_property = gtk_source_buffer_set_property;

	/* Do not set these signals handlers directly on the parent_class since
	 * that will cause problems (a loop). */
	tb_class->insert_text 	= gtk_source_buffer_real_insert_text;
	tb_class->delete_range 	= gtk_source_buffer_real_delete_range;

	/**
	 * GtkSourceBuffer:highlight-syntax:
	 *
	 * Whether to highlight syntax in the buffer.
	 */
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_SYNTAX,
					 g_param_spec_boolean ("highlight-syntax",
							       _("Highlight Syntax"),
							       _("Whether to highlight syntax "
								 "in the buffer"),
							       TRUE,
							       G_PARAM_READWRITE));

	/**
	 * GtkSourceBuffer:highlight-matching-brackets:
	 *
	 * Whether to highlight matching brackets in the buffer.
	 */
	g_object_class_install_property (object_class,
					 PROP_HIGHLIGHT_MATCHING_BRACKETS,
					 g_param_spec_boolean ("highlight-matching-brackets",
							       _("Highlight Matching Brackets"),
							       _("Whether to highlight matching brackets"),
							       TRUE,
							       G_PARAM_READWRITE));

	/**
	 * GtkSourceBuffer:max-undo-levels:
	 *
	 * Number of undo levels for the buffer. -1 means no limit.
	 */
	g_object_class_install_property (object_class,
					 PROP_MAX_UNDO_LEVELS,
					 g_param_spec_int ("max-undo-levels",
							   _("Maximum Undo Levels"),
							   _("Number of undo levels for "
							     "the buffer"),
							   -1,
							   G_MAXINT,
							   1000,
							   G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_LANGUAGE,
					 g_param_spec_object ("language",
							      /* Translators: throughout gtksourceview "language" stands
							       * for "programming language", not "spoken language" */
							      _("Language"),
							      _("Language object to get "
								"highlighting patterns from"),
							      GTK_TYPE_SOURCE_LANGUAGE,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CAN_UNDO,
					 g_param_spec_boolean ("can-undo",
							       _("Can undo"),
							       _("Whether Undo operation is possible"),
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_CAN_REDO,
					 g_param_spec_boolean ("can-redo",
							       _("Can redo"),
							       _("Whether Redo operation is possible"),
							       FALSE,
							       G_PARAM_READABLE));

	/**
	 * GtkSourceBuffer:style-scheme:
	 *
	 * Style scheme. It contains styles for syntax highlighting, optionally
	 * foreground, background, cursor color, current line color, and matching
	 * brackets style.
	 */
	g_object_class_install_property (object_class,
					 PROP_STYLE_SCHEME,
					 g_param_spec_object ("style_scheme",
							      _("Style scheme"),
							      _("Style scheme"),
							      GTK_TYPE_SOURCE_STYLE_SCHEME,
							      G_PARAM_READWRITE));

	param_types[0] = GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE;
	param_types[1] = GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE;

	buffer_signals[HIGHLIGHT_UPDATED] =
	    g_signal_newv ("highlight_updated",
			   G_OBJECT_CLASS_TYPE (object_class),
			   G_SIGNAL_RUN_LAST,
			   NULL,
			   NULL, NULL,
			   _gtksourceview_marshal_VOID__BOXED_BOXED,
			   G_TYPE_NONE,
			   2, param_types);

	g_type_class_add_private (object_class, sizeof(GtkSourceBufferPrivate));
}

static void
gtk_source_buffer_init (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv;

	priv = G_TYPE_INSTANCE_GET_PRIVATE (buffer, GTK_TYPE_SOURCE_BUFFER,
					    GtkSourceBufferPrivate);

	buffer->priv = priv;

	priv->undo_manager = gtk_source_undo_manager_new (GTK_TEXT_BUFFER (buffer));

	priv->highlight_syntax = TRUE;
	priv->highlight_brackets = TRUE;
	priv->bracket_mark = NULL;
	priv->bracket_found = FALSE;

	priv->style_scheme = _gtk_source_style_scheme_get_default ();
	if (priv->style_scheme != NULL)
		g_object_ref (priv->style_scheme);

	g_signal_connect (buffer,
			  "mark_set",
			  G_CALLBACK (gtk_source_buffer_move_cursor),
			  NULL);
	g_signal_connect (priv->undo_manager,
			  "can_undo",
			  G_CALLBACK (gtk_source_buffer_can_undo_handler),
			  buffer);
	g_signal_connect (priv->undo_manager,
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
	g_return_if_fail (buffer->priv != NULL);

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->finalize (object);
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
		_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
		g_object_unref (buffer->priv->highlight_engine);
		buffer->priv->highlight_engine = NULL;
	}

	if (buffer->priv->language != NULL)
	{
		g_object_unref (buffer->priv->language);
		buffer->priv->language = NULL;
	}

	if (buffer->priv->style_scheme != NULL)
	{
		g_object_unref (buffer->priv->style_scheme);
		buffer->priv->style_scheme = NULL;
	}

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->dispose (object);
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
		case PROP_HIGHLIGHT_SYNTAX:
			gtk_source_buffer_set_highlight_syntax (source_buffer,
							      g_value_get_boolean (value));
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			gtk_source_buffer_set_highlight_matching_brackets (source_buffer,
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

		case PROP_STYLE_SCHEME:
			gtk_source_buffer_set_style_scheme (source_buffer,
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
		case PROP_HIGHLIGHT_SYNTAX:
			g_value_set_boolean (value,
					     source_buffer->priv->highlight_syntax);
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			g_value_set_boolean (value,
					     source_buffer->priv->highlight_brackets);
			break;

		case PROP_MAX_UNDO_LEVELS:
			g_value_set_int (value,
					 gtk_source_buffer_get_max_undo_levels (source_buffer));
			break;

		case PROP_LANGUAGE:
			g_value_set_object (value, source_buffer->priv->language);
			break;

		case PROP_STYLE_SCHEME:
			g_value_set_object (value, source_buffer->priv->style_scheme);
			break;

		case PROP_CAN_UNDO:
			g_value_set_boolean (value, gtk_source_buffer_can_undo (source_buffer));
			break;

		case PROP_CAN_REDO:
			g_value_set_boolean (value, gtk_source_buffer_can_redo (source_buffer));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * gtk_source_buffer_new:
 * @table: a #GtkTextTagTable, or %NULL to create a new one.
 *
 * Creates a new source buffer.
 *
 * Return value: a new source buffer.
 **/
GtkSourceBuffer *
gtk_source_buffer_new (GtkTextTagTable *table)
{
	return g_object_new (GTK_TYPE_SOURCE_BUFFER,
			     "tag-table", table,
			     NULL);
}

/**
 * gtk_source_buffer_new_with_language:
 * @language: a #GtkSourceLanguage.
 *
 * Creates a new source buffer using the highlighting patterns in
 * @language.  This is equivalent to creating a new source buffer with
 * a new tag table and then calling gtk_source_buffer_set_language().
 *
 * Return value: a new source buffer which will highlight text
 * according to the highlighting patterns in @language.
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

	g_object_notify (G_OBJECT (buffer), "can-undo");
}

static void
gtk_source_buffer_can_redo_handler (GtkSourceUndoManager  	*um,
				    gboolean         		 can_redo,
				    GtkSourceBuffer 		*buffer)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	g_object_notify (G_OBJECT (buffer), "can-redo");
}

static void
update_bracket_match_style (GtkSourceBuffer *buffer)
{
	if (buffer->priv->bracket_match_tag != NULL)
	{
		GtkSourceStyle *style = NULL;

		if (buffer->priv->style_scheme)
			style = _gtk_source_style_scheme_get_matching_brackets_style (buffer->priv->style_scheme);

		_gtk_source_style_apply (style, buffer->priv->bracket_match_tag);
	}
}

static GtkTextTag *
get_bracket_match_tag (GtkSourceBuffer *buffer)
{
	if (buffer->priv->bracket_match_tag == NULL)
	{
		buffer->priv->bracket_match_tag =
			gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
						    NULL,
						    NULL);
		update_bracket_match_style (buffer);
	}

	return buffer->priv->bracket_match_tag;
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

	if (!GTK_SOURCE_BUFFER (buffer)->priv->highlight_brackets)
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
					   get_bracket_match_tag (GTK_SOURCE_BUFFER (buffer)),
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
	gint start_offset, end_offset;
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);

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
	GTK_TEXT_BUFFER_CLASS(gtk_source_buffer_parent_class)->insert_text (buffer, iter, text, len);

	gtk_source_buffer_move_cursor (buffer,
				       iter,
				       gtk_text_buffer_get_insert (buffer),
				       NULL);

	end_offset = gtk_text_iter_get_offset (iter);

	if (source_buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_text_inserted (source_buffer->priv->highlight_engine,
						  start_offset,
						  end_offset);
}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *start,
				     GtkTextIter   *end)
{
	gint offset, length;
	GtkTextMark *mark;
	GtkTextIter iter;
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);

	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

	gtk_text_iter_order (start, end);
	offset = gtk_text_iter_get_offset (start);
	length = gtk_text_iter_get_offset (end) - offset;

	GTK_TEXT_BUFFER_CLASS(gtk_source_buffer_parent_class)->delete_range (buffer, start, end);

	mark = gtk_text_buffer_get_insert (buffer);
	gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);

	gtk_source_buffer_move_cursor (buffer,
				       &iter,
				       mark,
				       NULL);

	/* emit text deleted for engines */
	if (source_buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_text_deleted (source_buffer->priv->highlight_engine,
						 offset,
						 length);
}

/* FIXME: this can't be here, but it's now needed for bracket matching */
static gpointer
iter_has_syntax_tag (const GtkTextIter *iter)
{
	return NULL;
// 	const GtkSourceTag *tag;
// 	GSList *list;
// 	GSList *l;
//
// 	g_return_val_if_fail (iter != NULL, NULL);
//
// 	list = gtk_text_iter_get_tags (iter);
// 	tag = NULL;
//
// 	l = list;
//
// 	while ((list != NULL) && (tag == NULL)) {
// 		if (GTK_IS_SOURCE_TAG (list->data))
// 			tag = GTK_SOURCE_TAG (list->data);
// 		list = g_slist_next (list);
// 	}
//
// 	g_slist_free (l);
//
// 	return tag;
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

	gpointer base_tag;

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

#if 0
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
#endif

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
 * Return value: the maximum number of possible undo levels or
 *               -1 if no limit is set.
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
 * If @max_undo_levels is -1, no limit is set.
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
		g_object_notify (G_OBJECT (buffer), "max-undo-levels");
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
 * gtk_source_buffer_get_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether bracket match highlighting is activated for the
 * source buffer.
 *
 * Return value: %TRUE if the source buffer will highlight matching
 * brackets.
 **/
gboolean
gtk_source_buffer_get_highlight_matching_brackets (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->highlight_brackets;
}

/**
 * gtk_source_buffer_set_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE if you want matching brackets highlighted.
 *
 * Controls the bracket match highlighting function in the buffer.  If
 * activated, when you position your cursor over a bracket character
 * (a parenthesis, a square bracket, etc.) the matching opening or
 * closing bracket character will be highlighted.  You can specify the
 * style with the gtk_source_buffer_set_bracket_match_style()
 * function.
 **/
void
gtk_source_buffer_set_highlight_matching_brackets (GtkSourceBuffer *buffer,
						   gboolean         highlight)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (highlight != buffer->priv->highlight_brackets)
	{
		buffer->priv->highlight_brackets = highlight;
		g_object_notify (G_OBJECT (buffer), "highlight-matching-brackets");
	}
}

/**
 * gtk_source_buffer_get_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether syntax highlighting is activated in the source
 * buffer.
 *
 * Return value: %TRUE if syntax highlighting is enabled, %FALSE otherwise.
 **/
gboolean
gtk_source_buffer_get_highlight_syntax (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), FALSE);

	return buffer->priv->highlight_syntax;
}

/**
 * gtk_source_buffer_set_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE to enable syntax highlighting, %FALSE to disable it.
 *
 * Controls whether syntax is highlighted in the buffer. If @highlight
 * is %TRUE, the text will be highlighted according to the syntax
 * patterns specified in the language set with
 * gtk_source_buffer_set_language(). If @highlight is %FALSE, syntax highlighting
 * is disabled and all the GtkTextTag objects that have been added by the 
 * syntax highlighting engine are removed from the buffer.
 **/
void
gtk_source_buffer_set_highlight_syntax (GtkSourceBuffer *buffer,
					gboolean         highlight)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	highlight = (highlight != FALSE);

	if (buffer->priv->highlight_syntax != highlight)
	{
	buffer->priv->highlight_syntax = highlight;
	g_object_notify (G_OBJECT (buffer), "highlight-syntax");
	}
}

/**
 * gtk_source_buffer_set_language:
 * @buffer: a #GtkSourceBuffer.
 * @language: a #GtkSourceLanguage to set, or %NULL.
 *
 * Associate a #GtkSourceLanguage with the source buffer. If @language is 
 * not-%NULL and syntax highlighting is enabled (see gtk_source_buffer_set_highlight()),
 * the syntax patterns defined in @language will be used to highlight the text
 * contained in the buffer. If @language is %NULL, the text contained in the 
 * buffer is not highlighted.
 * 
 * The buffer holds a reference to @language.
 **/
void
gtk_source_buffer_set_language (GtkSourceBuffer   *buffer,
				GtkSourceLanguage *language)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (buffer->priv->language == language)
		return;

	if (buffer->priv->highlight_engine != NULL)
	{
		/* disconnect the old engine */
		_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine, NULL);
		g_object_unref (buffer->priv->highlight_engine);
		buffer->priv->highlight_engine = NULL;
	}

	if (buffer->priv->language != NULL)
		g_object_unref (buffer->priv->language);

	buffer->priv->language = language;

	if (language != NULL)
	{
		g_object_ref (language);

		/* get a new engine */
		buffer->priv->highlight_engine = _gtk_source_language_create_engine (language);

		if (buffer->priv->highlight_engine)
		{
			_gtk_source_engine_attach_buffer (buffer->priv->highlight_engine,
							  GTK_TEXT_BUFFER (buffer));

			if (buffer->priv->style_scheme)
				_gtk_source_engine_set_style_scheme (buffer->priv->highlight_engine,
								     buffer->priv->style_scheme);
		}
	}

	g_object_notify (G_OBJECT (buffer), "language");
}

/**
 * gtk_source_buffer_get_language:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the #GtkSourceLanguage associated with the buffer, 
 * see gtk_source_buffer_set_language().  The returned object should not be
 * unreferenced by the user.
 *
 * Return value: #GtkSourceLanguage associated with the buffer, or %NULL.
 **/
GtkSourceLanguage *
gtk_source_buffer_get_language (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);

	return buffer->priv->language;
}

/**
 * _gtk_source_buffer_update_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 * @synchronous: whether the area should be highlighted synchronously.
 *
 * Asks the buffer to analyze and highlight given area.
 **/
void
_gtk_source_buffer_update_highlight (GtkSourceBuffer   *buffer,
				     const GtkTextIter *start,
				     const GtkTextIter *end,
				     gboolean           synchronous)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));

	if (buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_update_highlight (buffer->priv->highlight_engine,
						     start,
						     end,
						     synchronous);
}

/**
 * gtk_source_buffer_ensure_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 *
 * Forces buffer to analyze and highlight the given area synchronously.
 *
 * <note>
 *   <para>
 *     This is a potentially slow operation and should be used only
 *     when you need to make sure that some text not currently
 *     visible is highlighted, for instance before printing.
 *   </para>
 * </note>
 **/
void
gtk_source_buffer_ensure_highlight (GtkSourceBuffer   *buffer,
				    const GtkTextIter *start,
				    const GtkTextIter *end)
{
	_gtk_source_buffer_update_highlight (buffer,
					     start,
					     end,
					     TRUE);
}

/**
 * gtk_source_buffer_set_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 * @scheme: style scheme.
 *
 * Sets style scheme used by the buffer.
 **/
void
gtk_source_buffer_set_style_scheme (GtkSourceBuffer      *buffer,
				    GtkSourceStyleScheme *scheme)
{
	g_return_if_fail (GTK_IS_SOURCE_BUFFER (buffer));
	g_return_if_fail (GTK_IS_SOURCE_STYLE_SCHEME (scheme));

	if (buffer->priv->style_scheme == scheme)
		return;

	if (buffer->priv->style_scheme)
		g_object_unref (buffer->priv->style_scheme);

	buffer->priv->style_scheme = g_object_ref (scheme);
	update_bracket_match_style (buffer);

	if (buffer->priv->highlight_engine != NULL)
		_gtk_source_engine_set_style_scheme (buffer->priv->highlight_engine,
						     scheme);

	g_object_notify (G_OBJECT (buffer), "style-scheme");
}

/**
 * gtk_source_buffer_get_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the #GtkSourceStyleScheme currently used in @buffer.
 *
 * Returns: the #GtkSourceStyleScheme set by
 * gtk_source_buffer_set_style_scheme(), or %NULL.
 **/
GtkSourceStyleScheme *
gtk_source_buffer_get_style_scheme (GtkSourceBuffer *buffer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (buffer), NULL);
	return buffer->priv->style_scheme;
}


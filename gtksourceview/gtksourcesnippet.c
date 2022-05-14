/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gtksourcebuffer-private.h"
#include "gtksourcelanguage.h"
#include "gtksourcesnippet-private.h"
#include "gtksourcesnippetbundle-private.h"
#include "gtksourcesnippetchunk-private.h"
#include "gtksourcesnippetcontext-private.h"

/**
 * GtkSourceSnippet:
 *
 * Quick insertion code snippets.
 *
 * The `GtkSourceSnippet` represents a series of chunks that can quickly be
 * inserted into the [class@View].
 *
 * Snippets are defined in XML files which are loaded by the
 * [class@SnippetManager]. Alternatively, applications can create snippets
 * on demand and insert them into the [class@View] using
 * [method@View.push_snippet].
 *
 * Snippet chunks can reference other snippet chunks as well as post-process
 * the values from other chunks such as capitalization.
 */

G_DEFINE_TYPE (GtkSourceSnippet, gtk_source_snippet, G_TYPE_OBJECT)

enum {
	PROP_0,
	PROP_BUFFER,
	PROP_DESCRIPTION,
	PROP_FOCUS_POSITION,
	PROP_LANGUAGE_ID,
	PROP_NAME,
	PROP_TRIGGER,
	N_PROPS
};

static GParamSpec *properties [N_PROPS];

static void gtk_source_snippet_update_marks (GtkSourceSnippet *snippet);
static void gtk_source_snippet_clear_tags   (GtkSourceSnippet *snippet);

static void
gtk_source_snippet_save_insert (GtkSourceSnippet *snippet)
{
	GtkTextMark *insert;
	GtkTextIter iter;
	GtkTextIter begin;
	GtkTextIter end;

	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	if (snippet->current_chunk == NULL ||
	    !_gtk_source_snippet_chunk_get_bounds (snippet->current_chunk, &begin, &end))
	{
		snippet->saved_insert_pos = 0;
		return;
	}

	insert = gtk_text_buffer_get_insert (snippet->buffer);
	gtk_text_buffer_get_iter_at_mark (snippet->buffer, &iter, insert);

	if (_gtk_source_snippet_chunk_contains (snippet->current_chunk, &iter))
	{
		snippet->saved_insert_pos =
			gtk_text_iter_get_offset (&iter) -
			gtk_text_iter_get_offset (&begin);
	}
}

static void
gtk_source_snippet_restore_insert (GtkSourceSnippet *snippet)
{
	GtkTextIter begin;
	GtkTextIter end;

	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	if (snippet->current_chunk == NULL ||
	    !_gtk_source_snippet_chunk_get_bounds (snippet->current_chunk, &begin, &end))
	{
		snippet->saved_insert_pos = 0;
		return;
	}

	gtk_text_iter_forward_chars (&begin, snippet->saved_insert_pos);
	gtk_text_buffer_select_range (snippet->buffer, &begin, &begin);
	snippet->saved_insert_pos = 0;
}

/**
 * gtk_source_snippet_new:
 * @trigger: (nullable): the trigger word
 * @language_id: (nullable): the source language
 *
 * Creates a new #GtkSourceSnippet
 *
 * Returns: (transfer full): A new #GtkSourceSnippet
 */
GtkSourceSnippet *
gtk_source_snippet_new (const gchar *trigger,
                        const gchar *language_id)
{
	return g_object_new (GTK_SOURCE_TYPE_SNIPPET,
	                     "trigger", trigger,
	                     "language-id", language_id,
	                     NULL);
}

/**
 * gtk_source_snippet_copy:
 * @snippet: a #GtkSourceSnippet
 *
 * Does a deep copy of the snippet.
 *
 * Returns: (transfer full): A new #GtkSourceSnippet
 */
GtkSourceSnippet *
gtk_source_snippet_copy (GtkSourceSnippet *snippet)
{
	GtkSourceSnippet *ret;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	ret = g_object_new (GTK_SOURCE_TYPE_SNIPPET,
	                    "trigger", snippet->trigger,
	                    "language-id", snippet->language_id,
	                    "description", snippet->description,
	                    NULL);

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *old_chunk = l->data;
		GtkSourceSnippetChunk *new_chunk = gtk_source_snippet_chunk_copy (old_chunk);

		gtk_source_snippet_add_chunk (ret, g_steal_pointer (&new_chunk));
	}

	return g_steal_pointer (&ret);
}

/**
 * gtk_source_snippet_get_focus_position:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the current focus for the snippet.
 *
 * This is changed as the user tabs through focus locations.
 *
 * Returns: The focus position, or -1 if unset.
 */
gint
gtk_source_snippet_get_focus_position (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), -1);

	return snippet->focus_position;
}

/**
 * gtk_source_snippet_get_n_chunks:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the number of chunks in the snippet.
 *
 * Note that not all chunks are editable.
 *
 * Returns: The number of chunks.
 */
guint
gtk_source_snippet_get_n_chunks (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), 0);

	return snippet->chunks.length;
}

/**
 * gtk_source_snippet_get_nth_chunk:
 * @snippet: a #GtkSourceSnippet
 * @nth: the nth chunk to get
 *
 * Gets the chunk at @nth.
 *
 * Returns: (transfer none): an #GtkSourceSnippetChunk
 */
GtkSourceSnippetChunk *
gtk_source_snippet_get_nth_chunk (GtkSourceSnippet *snippet,
                                  guint             nth)
{
	GtkSourceSnippetChunk *chunk = NULL;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), 0);

	if (nth < snippet->chunks.length)
	{
		chunk = g_queue_peek_nth (&snippet->chunks, nth);
	}

	g_return_val_if_fail (!chunk || GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return chunk;
}

/**
 * gtk_source_snippet_get_trigger:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the trigger for the source snippet.
 *
 * A trigger is a word that can be expanded into the full snippet when
 * the user presses Tab.
 *
 * Returns: (nullable): A string or %NULL
 */
const gchar *
gtk_source_snippet_get_trigger (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	return snippet->trigger;
}

/**
 * gtk_source_snippet_set_trigger:
 * @snippet: a #GtkSourceSnippet
 * @trigger: the trigger word
 *
 * Sets the trigger for the snippet.
 */
void
gtk_source_snippet_set_trigger (GtkSourceSnippet *snippet,
                                const gchar      *trigger)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	if (g_strcmp0 (trigger, snippet->trigger) != 0)
	{
		g_free (snippet->trigger);
		snippet->trigger = g_strdup (trigger);
		g_object_notify_by_pspec (G_OBJECT (snippet),
		                          properties [PROP_TRIGGER]);
	}
}

/**
 * gtk_source_snippet_get_language_id:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the language-id used for the source snippet.
 *
 * The language identifier should be one that matches a
 * source language [property@Language:id] property.
 *
 * Returns: the language identifier
 */
const gchar *
gtk_source_snippet_get_language_id (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	return snippet->language_id;
}

/**
 * gtk_source_snippet_set_language_id:
 * @snippet: a #GtkSourceSnippet
 * @language_id: the language identifier for the snippet
 *
 * Sets the language identifier for the snippet.
 *
 * This should match the [property@Language:id] identifier.
 */
void
gtk_source_snippet_set_language_id (GtkSourceSnippet *snippet,
                                    const gchar      *language_id)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	language_id = g_intern_string (language_id);

	if (language_id != snippet->language_id)
	{
		snippet->language_id = language_id;
		g_object_notify_by_pspec (G_OBJECT (snippet),
		                          properties [PROP_LANGUAGE_ID]);
	}
}

/**
 * gtk_source_snippet_get_description:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the description for the snippet.
 */
const gchar *
gtk_source_snippet_get_description (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	return snippet->description;
}

/**
 * gtk_source_snippet_set_description:
 * @snippet: a #GtkSourceSnippet
 * @description: the snippet description
 *
 * Sets the description for the snippet.
 */
void
gtk_source_snippet_set_description (GtkSourceSnippet *snippet,
                                    const gchar      *description)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	if (g_strcmp0 (description, snippet->description) != 0)
	{
		g_free (snippet->description);
		snippet->description = g_strdup (description);
		g_object_notify_by_pspec (G_OBJECT (snippet),
		                          properties [PROP_DESCRIPTION]);
	}
}

/**
 * gtk_source_snippet_get_name:
 * @snippet: a #GtkSourceSnippet
 *
 * Gets the name for the snippet.
 */
const gchar *
gtk_source_snippet_get_name (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	return snippet->name;
}

/**
 * gtk_source_snippet_set_name:
 * @snippet: a #GtkSourceSnippet
 * @name: the snippet name
 *
 * Sets the name for the snippet.
 */
void
gtk_source_snippet_set_name (GtkSourceSnippet *snippet,
                             const gchar      *name)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	if (g_strcmp0 (name, snippet->name) != 0)
	{
		g_free (snippet->name);
		snippet->name = g_strdup (name);
		g_object_notify_by_pspec (G_OBJECT (snippet),
		                          properties [PROP_NAME]);
	}
}

static void
gtk_source_snippet_select_chunk (GtkSourceSnippet      *snippet,
                                 GtkSourceSnippetChunk *chunk)
{
	GtkTextIter begin;
	GtkTextIter end;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));
	g_return_if_fail (chunk->focus_position >= 0);

	if (!_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end))
	{
		return;
	}

	g_debug ("Selecting chunk with range %d:%d to %d:%d (offset %d+%d)",
	         gtk_text_iter_get_line (&begin) + 1,
	         gtk_text_iter_get_line_offset (&begin) + 1,
	         gtk_text_iter_get_line (&end) + 1,
	         gtk_text_iter_get_line_offset (&end) + 1,
	         gtk_text_iter_get_offset (&begin),
	         gtk_text_iter_get_offset (&end) - gtk_text_iter_get_offset (&begin));

	snippet->current_chunk = chunk;
	snippet->focus_position = chunk->focus_position;

	gtk_text_buffer_select_range (snippet->buffer, &begin, &end);

	g_object_notify_by_pspec (G_OBJECT (snippet), properties [PROP_FOCUS_POSITION]);

#ifndef G_DISABLE_ASSERT
	{
		GtkTextIter set_begin;
		GtkTextIter set_end;

		gtk_text_buffer_get_selection_bounds (snippet->buffer, &set_begin, &set_end);

		g_assert (gtk_text_iter_equal (&set_begin, &begin));
		g_assert (gtk_text_iter_equal (&set_end, &end));
	}
#endif
}

gboolean
_gtk_source_snippet_insert_set (GtkSourceSnippet *snippet,
                                GtkTextMark      *mark)
{
	GtkTextIter begin;
	GtkTextIter end;
	GtkTextIter iter;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);
	g_return_val_if_fail (GTK_IS_TEXT_MARK (mark), FALSE);
	g_return_val_if_fail (snippet->current_chunk != NULL, FALSE);
	g_return_val_if_fail (snippet->buffer != NULL, FALSE);

	gtk_text_buffer_get_iter_at_mark (snippet->buffer, &iter, mark);

	if (_gtk_source_snippet_chunk_get_bounds (snippet->current_chunk, &begin, &end))
	{
		if (gtk_text_iter_compare (&begin, &iter) <= 0 &&
		    gtk_text_iter_compare (&end, &iter) >= 0)
		{
			/* No change, we're still in the current chunk */
			return TRUE;
		}
	}

	/* See if the insertion position would place us in any of the other
	 * snippet chunks that are a focus position.
	 */
	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;

		if (chunk->focus_position <= 0 || chunk == snippet->current_chunk)
		{
			continue;
		}

		if (_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end))
		{
			/* Ignore this chunk if it is empty. There is no way
			 * to disambiguate between side-by-side empty chunks
			 * to make this a meaningful movement.
			 */
			if (gtk_text_iter_equal (&begin, &end))
			{
				continue;
			}

			/* This chunk contains the focus position, so make it
			 * our new chunk to edit.
			 */
			if (gtk_text_iter_compare (&begin, &iter) <= 0 &&
			    gtk_text_iter_compare (&end, &iter) >= 0)
			{
				gtk_source_snippet_select_chunk (snippet, chunk);
				return TRUE;
			}
		}
	}

	return FALSE;
}

gboolean
_gtk_source_snippet_move_next (GtkSourceSnippet *snippet)
{
	GtkTextIter iter;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);

	snippet->focus_position++;

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;

		if (gtk_source_snippet_chunk_get_focus_position (chunk) == snippet->focus_position)
		{
			gtk_source_snippet_select_chunk (snippet, chunk);
			return TRUE;
		}
	}

	for (const GList *l = snippet->chunks.tail; l; l = l->prev)
	{
		GtkSourceSnippetChunk *chunk = l->data;

		if (gtk_source_snippet_chunk_get_focus_position (chunk) == 0)
		{
			gtk_source_snippet_select_chunk (snippet, chunk);
			return FALSE;
		}
	}

	g_debug ("No more tab stops, moving to end of snippet");

	snippet->current_chunk = NULL;
	gtk_text_buffer_get_iter_at_mark (snippet->buffer, &iter, snippet->end_mark);
	gtk_text_buffer_select_range (snippet->buffer, &iter, &iter);

	return FALSE;
}

gboolean
_gtk_source_snippet_move_previous (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);

	if (snippet->focus_position <= 1)
	{
		GtkTextIter iter;

		/* Nothing to select before this position, just move
		 * the insertion mark to the beginning of the snippet.
		 */
		gtk_text_buffer_get_iter_at_mark (snippet->buffer,
		                                  &iter,
		                                  snippet->begin_mark);
		gtk_text_buffer_select_range (snippet->buffer, &iter, &iter);

		return FALSE;
	}

	snippet->focus_position--;

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;

		if (gtk_source_snippet_chunk_get_focus_position (chunk) == snippet->focus_position)
		{
			gtk_source_snippet_select_chunk (snippet, chunk);
			return TRUE;
		}
	}

	return FALSE;
}

static void
gtk_source_snippet_update_context_pass (GtkSourceSnippet *snippet)
{
	GtkSourceSnippetContext *context;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	context = gtk_source_snippet_get_context (snippet);

	_gtk_source_snippet_context_emit_changed (context);

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		gint focus_position;

		g_assert (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

		focus_position = gtk_source_snippet_chunk_get_focus_position (chunk);

		if (focus_position > 0)
		{
			const gchar *text;

			if ((text = gtk_source_snippet_chunk_get_text (chunk)))
			{
				gchar key[12];

				g_snprintf (key, sizeof key, "%d", focus_position);
				key[sizeof key - 1] = '\0';

				gtk_source_snippet_context_set_variable (context, key, text);
			}
		}
	}

	_gtk_source_snippet_context_emit_changed (context);
}

static void
gtk_source_snippet_update_context (GtkSourceSnippet *snippet,
                                   gboolean          emit_changed)
{
	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	/* First pass */
	gtk_source_snippet_update_context_pass (snippet);

	if (emit_changed)
	{
		GtkSourceSnippetContext *context;

		context = gtk_source_snippet_get_context (snippet);
		_gtk_source_snippet_context_emit_changed (context);
	}

	/* Second pass, to handle possible wrap around cases */
	gtk_source_snippet_update_context_pass (snippet);
}

static void
gtk_source_snippet_setup_context (GtkSourceSnippet        *snippet,
				  GtkSourceSnippetContext *context,
                                  GtkSourceBuffer         *buffer,
                                  const GtkTextIter       *iter)
{
	static const struct {
		const gchar *name;
		const gchar *key;
	} metadata[] = {
		{ "BLOCK_COMMENT_START", "block-comment-start" },
		{ "BLOCK_COMMENT_END", "block-comment-end" },
		{ "LINE_COMMENT", "line-comment-start" },
	};
	GtkSourceLanguage *l;
	GtkTextIter begin;
	GtkTextIter end;
	gchar *text;

	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));
	g_assert (iter != NULL);

	/* This updates a number of snippet variables that are familiar to
	 * users of existing snippet engines. Notably, the TM_ prefixed
	 * variables have been (re)used across numerous text editors.
	 */

	/* TM_CURRENT_LINE */
	begin = end = *iter;
	if (!gtk_text_iter_starts_line (&begin))
		gtk_text_iter_set_offset (&begin, 0);
	if (!gtk_text_iter_ends_line (&end))
		gtk_text_iter_forward_to_line_end (&end);
	text = gtk_text_iter_get_slice (&begin, &end);
	gtk_source_snippet_context_set_constant (context, "TM_CURRENT_LINE", text);
	g_free (text);

	/* TM_SELECTED_TEXT */
	if (gtk_text_buffer_get_selection_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end))
	{
		text = gtk_text_iter_get_slice (&begin, &end);
		gtk_source_snippet_context_set_constant (context, "TM_SELECTED_TEXT", text);
		g_free (text);
	}

	/* TM_LINE_INDEX */
	text = g_strdup_printf ("%u", gtk_text_iter_get_line (iter));
	gtk_source_snippet_context_set_constant (context, "TM_LINE_INDEX", text);
	g_free (text);

	/* TM_LINE_NUMBER */
	text = g_strdup_printf ("%u", gtk_text_iter_get_line (iter) + 1);
	gtk_source_snippet_context_set_constant (context, "TM_LINE_NUMBER", text);
	g_free (text);

	/* Various metadata fields */
	l = gtk_source_buffer_get_language (buffer);
	if (l != NULL)
	{
		for (guint i = 0; i < G_N_ELEMENTS (metadata); i++)
		{
			const gchar *name = metadata[i].name;
			const gchar *val = gtk_source_language_get_metadata (l, metadata[i].key);

			if (val != NULL)
			{
				gtk_source_snippet_context_set_constant (context, name, val);
			}
		}
	}

	gtk_source_snippet_update_context (snippet, TRUE);
}


static void
gtk_source_snippet_clear_tags (GtkSourceSnippet *snippet)
{
	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	if (snippet->begin_mark != NULL && snippet->end_mark != NULL)
	{
		GtkTextBuffer *buffer;
		GtkTextIter begin;
		GtkTextIter end;
		GtkTextTag *tag;

		buffer = gtk_text_mark_get_buffer (snippet->begin_mark);

		gtk_text_buffer_get_iter_at_mark (buffer, &begin, snippet->begin_mark);
		gtk_text_buffer_get_iter_at_mark (buffer, &end, snippet->end_mark);

		tag = _gtk_source_buffer_get_snippet_focus_tag (GTK_SOURCE_BUFFER (buffer));

		gtk_text_buffer_remove_tag (buffer, tag, &begin, &end);
	}
}

static void
gtk_source_snippet_update_tags (GtkSourceSnippet *snippet)
{
	GtkTextBuffer *buffer;
	GtkTextTag *tag;

	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	gtk_source_snippet_clear_tags (snippet);

	buffer = gtk_text_mark_get_buffer (snippet->begin_mark);
	tag = _gtk_source_buffer_get_snippet_focus_tag (GTK_SOURCE_BUFFER (buffer));

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		gint focus_position = gtk_source_snippet_chunk_get_focus_position (chunk);

		if (focus_position >= 0)
		{
			GtkTextIter begin;
			GtkTextIter end;

			_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end);
			gtk_text_buffer_apply_tag (buffer, tag, &begin, &end);
		}
	}
}

gboolean
_gtk_source_snippet_begin (GtkSourceSnippet *snippet,
                           GtkSourceBuffer  *buffer,
                           GtkTextIter      *iter)
{
	GtkSourceSnippetContext *context;
	GtkTextMark *mark;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);
	g_return_val_if_fail (!snippet->buffer, FALSE);
	g_return_val_if_fail (!snippet->begin_mark, FALSE);
	g_return_val_if_fail (!snippet->end_mark, FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	snippet->inserted = TRUE;

	context = gtk_source_snippet_get_context (snippet);
	gtk_source_snippet_setup_context (snippet, context, buffer, iter);

	snippet->buffer = g_object_ref (GTK_TEXT_BUFFER (buffer));

	mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, iter, TRUE);
	snippet->begin_mark = g_object_ref (mark);

	mark = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, iter, FALSE);
	snippet->end_mark = g_object_ref (mark);

	gtk_text_buffer_begin_user_action (GTK_TEXT_BUFFER (buffer));

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		GtkTextMark *begin;
		GtkTextMark *end;
		const gchar *text;

		text = gtk_source_snippet_chunk_get_text (chunk);

		begin = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, iter, TRUE);
		end = gtk_text_buffer_create_mark (GTK_TEXT_BUFFER (buffer), NULL, iter, FALSE);

		g_set_object (&chunk->begin_mark, begin);
		g_set_object (&chunk->end_mark, end);

		if (text != NULL && text[0] != 0)
		{
			snippet->current_chunk = chunk;
			gtk_text_buffer_insert (GTK_TEXT_BUFFER (buffer), iter, text, -1);
			gtk_source_snippet_update_marks (snippet);
		}
	}

	snippet->current_chunk = NULL;

	gtk_text_buffer_end_user_action (GTK_TEXT_BUFFER (buffer));

	gtk_source_snippet_update_tags (snippet);

	return _gtk_source_snippet_move_next (snippet);
}

void
_gtk_source_snippet_finish (GtkSourceSnippet *snippet)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (snippet->buffer != NULL);

	gtk_source_snippet_clear_tags (snippet);

	if (snippet->begin_mark != NULL)
	{
		gtk_text_buffer_delete_mark (snippet->buffer, snippet->begin_mark);
		g_clear_object (&snippet->begin_mark);
	}

	if (snippet->end_mark != NULL)
	{
		gtk_text_buffer_delete_mark (snippet->buffer, snippet->end_mark);
		g_clear_object (&snippet->end_mark);
	}

	g_clear_object (&snippet->buffer);
}

/**
 * gtk_source_snippet_add_chunk:
 * @snippet: a #GtkSourceSnippet
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Appends @chunk to the @snippet.
 *
 * This may only be called before the snippet has been expanded.
 */
void
gtk_source_snippet_add_chunk (GtkSourceSnippet      *snippet,
                              GtkSourceSnippetChunk *chunk)
{
	gint focus_position;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));
	g_return_if_fail (!snippet->inserted);
	g_return_if_fail (chunk->link.data == chunk);
	g_return_if_fail (chunk->link.prev == NULL);
	g_return_if_fail (chunk->link.next == NULL);

	g_object_ref (chunk);

	g_queue_push_tail_link (&snippet->chunks, &chunk->link);

	gtk_source_snippet_chunk_set_context (chunk, snippet->context);

	focus_position = gtk_source_snippet_chunk_get_focus_position (chunk);
	snippet->max_focus_position = MAX (snippet->max_focus_position, focus_position);
}

static void
gtk_source_snippet_update_marks (GtkSourceSnippet *snippet)
{
	GtkSourceSnippetChunk *current;
	GtkTextBuffer *buffer;
	GtkTextIter current_begin;
	GtkTextIter current_end;

	g_assert (GTK_SOURCE_IS_SNIPPET (snippet));

	buffer = GTK_TEXT_BUFFER (snippet->buffer);
	current = snippet->current_chunk;

	if (current == NULL ||
	    !_gtk_source_snippet_chunk_get_bounds (current, &current_begin, &current_end))
	{
		return;
	}

	/* If the begin of this chunk has come before the end
	 * of the last chunk, then that mights we are empty and
	 * the right gravity of the begin mark was greedily taken
	 * when inserting into a previous mark. This can happen
	 * when you (often intermittently) have empty chunks.
	 *
	 * For example, imagine 4 empty chunks:
	 *
	 *   [][][][]
	 *
	 * Except in reality to GtkTextBuffer, that's more like:
	 *
	 *   [[[[]]]]
	 *
	 * When the user types 't' into the first chunk we'll end up
	 * with something like this:
	 *
	 *   [[[[t]]]]
	 *
	 * and we need to modify things to look like this:
	 *
	 *   [t][[[]]]
	 *
	 * We also must worry about the situation where text
	 * is inserted into the second position like:
	 *
	 *   [t[t]][[]]
	 *
	 * and detect the situation to move the end mark for the
	 * first item backwards into:
	 *
	 *   [t][t][[]]
	 */

	for (const GList *l = current->link.prev; l; l = l->prev)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		GtkTextIter begin;
		GtkTextIter end;

		if (_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end))
		{
			if (gtk_text_iter_compare (&end, &current_begin) > 0)
			{
				gtk_text_buffer_move_mark (buffer, chunk->end_mark, &current_begin);
				end = current_begin;
			}

			if (gtk_text_iter_compare (&begin, &end) > 0)
			{
				gtk_text_buffer_move_mark (buffer, chunk->begin_mark, &end);
				begin = end;
			}
		}
	}

	for (const GList *l = current->link.next; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		GtkTextIter begin;
		GtkTextIter end;

		if (_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end))
		{
			if (gtk_text_iter_compare (&begin, &current_end) < 0)
			{
				gtk_text_buffer_move_mark (buffer, chunk->begin_mark, &current_end);
				begin = current_end;
			}

			if (gtk_text_iter_compare (&end, &begin) < 0)
			{
				gtk_text_buffer_move_mark (buffer, chunk->end_mark, &begin);
				end = begin;
			}
		}
	}
}

static void
gtk_source_snippet_rewrite_updated_chunks (GtkSourceSnippet *snippet)
{
	GtkSourceSnippetChunk *saved;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	saved = snippet->current_chunk;

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		GtkTextIter begin;
		GtkTextIter end;
		const gchar *text;
		gchar *real_text;

		/* Temporarily set current chunk to help other utilities
		 * to adjust marks appropriately.
		 */
		snippet->current_chunk = chunk;

		_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end);
		real_text = gtk_text_iter_get_slice (&begin, &end);

		text = gtk_source_snippet_chunk_get_text (chunk);

		if (g_strcmp0 (text, real_text) != 0)
		{
			gtk_text_buffer_delete (snippet->buffer, &begin, &end);
			gtk_text_buffer_insert (snippet->buffer, &begin, text, -1);
			gtk_source_snippet_update_marks (snippet);
		}

		g_free (real_text);
	}

	snippet->current_chunk = saved;
}

void
_gtk_source_snippet_after_insert_text (GtkSourceSnippet *snippet,
                                       GtkTextBuffer    *buffer,
                                       GtkTextIter      *iter,
                                       const gchar      *text,
                                       gint              len)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (snippet->current_chunk != NULL);
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (snippet->current_chunk != NULL);

	/* This function is guaranteed to only be called once for the
	 * actual insert by gtksourceview-snippets.c. That allows us
	 * to update marks, update the context for shared variables, and
	 * delete/insert text in linked chunks.
	 */

	/* Save our insert position so we can restore it after updating
	 * linked chunks (which could be rewritten).
	 */
	gtk_source_snippet_save_insert (snippet);

	/* Now save the modified text for the iter in question */
	_gtk_source_snippet_chunk_save_text (snippet->current_chunk);

	/* First we want to update marks from the inserted text */
	gtk_source_snippet_update_marks (snippet);

	/* Update the context (two passes to ensure that we handle chunks
	 * referencing chunks which come after themselves in the array).
	 */
	gtk_source_snippet_update_context (snippet, FALSE);

	/* Now go and rewrite each chunk that has changed. This may also
	 * update marks after each pass so that text marks don't overlap.
	 */
	gtk_source_snippet_rewrite_updated_chunks (snippet);

	/* Now we can apply tags for the given chunks */
	gtk_source_snippet_update_tags (snippet);

	/* Place the insertion cursor back where the user expects it */
	gtk_source_snippet_restore_insert (snippet);
}

void
_gtk_source_snippet_after_delete_range (GtkSourceSnippet *snippet,
                                        GtkTextBuffer    *buffer,
                                        GtkTextIter      *begin,
                                        GtkTextIter      *end)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));
	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
	g_return_if_fail (begin != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (snippet->current_chunk != NULL);

	/* Now save the modified text for the iter in question */
	_gtk_source_snippet_chunk_save_text (snippet->current_chunk);

	/* Stash our cursor position so we can restore it after changes */
	gtk_source_snippet_save_insert (snippet);

	/* First update mark positions based on the deletions */
	gtk_source_snippet_update_marks (snippet);

	/* Update the context (two passes to ensure that we handle chunks
	 * referencing chunks which come after themselves in the array).
	 */
	gtk_source_snippet_update_context (snippet, FALSE);

	/* Now go and rewrite each chunk that has changed. This may also
	 * update marks after each pass so that text marks don't overlap.
	 */
	gtk_source_snippet_rewrite_updated_chunks (snippet);

	/* Now update any scheme styling for focus positions */
	gtk_source_snippet_update_tags (snippet);

	/* Place the insertion cursor back where the user expects it */
	gtk_source_snippet_restore_insert (snippet);
}

gboolean
_gtk_source_snippet_contains_range (GtkSourceSnippet  *snippet,
                                    const GtkTextIter *begin,
                                    const GtkTextIter *end)
{
	GtkTextIter snippet_begin;
	GtkTextIter snippet_end;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);
	g_return_val_if_fail (snippet->buffer != NULL, FALSE);
	g_return_val_if_fail (snippet->begin_mark != NULL, FALSE);
	g_return_val_if_fail (snippet->end_mark != NULL, FALSE);

	gtk_text_buffer_get_iter_at_mark (snippet->buffer,
	                                  &snippet_begin,
	                                  snippet->begin_mark);
	gtk_text_buffer_get_iter_at_mark (snippet->buffer,
	                                  &snippet_end,
	                                  snippet->end_mark);

	return gtk_text_iter_compare (begin, &snippet_begin) >= 0 &&
	       gtk_text_iter_compare (end, &snippet_end) <= 0;
}

guint
_gtk_source_snippet_count_affected_chunks (GtkSourceSnippet  *snippet,
                                           const GtkTextIter *begin,
                                           const GtkTextIter *end)
{
	guint count = 0;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	if (gtk_text_iter_equal (begin, end))
	{
		return 0;
	}

	for (const GList *l = snippet->chunks.head; l; l = l->next)
	{
		GtkSourceSnippetChunk *chunk = l->data;
		GtkTextIter chunk_begin;
		GtkTextIter chunk_end;

		if (_gtk_source_snippet_chunk_get_bounds (chunk, &chunk_begin, &chunk_end))
		{
			/* We only care about this chunk if it's non-empty. As
			 * we may have multiple "empty" chunks if they are right
			 * next to each other. Those can be safely ignored
			 * unless we have a chunk after them which is also
			 * overlapped.
			 */
			if (gtk_text_iter_equal (&chunk_begin, &chunk_end))
			{
				continue;
			}

			/* Special case when we are deleting a whole chunk
			 * content that is non-empty.
			 */
			if (gtk_text_iter_equal (begin, &chunk_begin) &&
			    gtk_text_iter_equal (end, &chunk_end))
			{
				return 1;
			}

			if (gtk_text_iter_compare (end, &chunk_begin) >= 0 &&
			    gtk_text_iter_compare (begin, &chunk_end) <= 0)
			{
				count++;
			}
		}
	}

	return count;
}

/**
 * gtk_source_snippet_get_context:
 * @snippet: an #GtkSourceSnippet
 *
 * Gets the context used for expanding the snippet.
 *
 * Returns: (nullable) (transfer none): an #GtkSourceSnippetContext
 */
GtkSourceSnippetContext *
gtk_source_snippet_get_context (GtkSourceSnippet *snippet)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	if (snippet->context == NULL)
	{
		snippet->context = gtk_source_snippet_context_new ();

		for (const GList *l = snippet->chunks.head; l; l = l->next)
		{
			GtkSourceSnippetChunk *chunk = l->data;

			gtk_source_snippet_chunk_set_context (chunk, snippet->context);
		}
	}

	return snippet->context;
}

static void
gtk_source_snippet_dispose (GObject *object)
{
	GtkSourceSnippet *snippet = (GtkSourceSnippet *)object;

	if (snippet->begin_mark != NULL)
	{
		gtk_text_buffer_delete_mark (snippet->buffer, snippet->begin_mark);
		g_clear_object (&snippet->begin_mark);
	}

	if (snippet->end_mark != NULL)
	{
		gtk_text_buffer_delete_mark (snippet->buffer, snippet->end_mark);
		g_clear_object (&snippet->end_mark);
	}

	while (snippet->chunks.length > 0)
	{
		GtkSourceSnippetChunk *chunk = snippet->chunks.head->data;

		g_queue_unlink (&snippet->chunks, &chunk->link);
		g_object_unref (chunk);
	}

	g_clear_object (&snippet->buffer);
	g_clear_object (&snippet->context);

	G_OBJECT_CLASS (gtk_source_snippet_parent_class)->dispose (object);
}

static void
gtk_source_snippet_finalize (GObject *object)
{
	GtkSourceSnippet *snippet = (GtkSourceSnippet *)object;

	g_clear_pointer (&snippet->description, g_free);
	g_clear_pointer (&snippet->name, g_free);
	g_clear_pointer (&snippet->trigger, g_free);
	g_clear_object (&snippet->buffer);

	G_OBJECT_CLASS (gtk_source_snippet_parent_class)->finalize (object);
}

static void
gtk_source_snippet_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
	GtkSourceSnippet *snippet = GTK_SOURCE_SNIPPET (object);

	switch (prop_id)
	{
	case PROP_BUFFER:
		g_value_set_object (value, snippet->buffer);
		break;

	case PROP_TRIGGER:
		g_value_set_string (value, snippet->trigger);
		break;

	case PROP_LANGUAGE_ID:
		g_value_set_string (value, snippet->language_id);
		break;

	case PROP_DESCRIPTION:
		g_value_set_string (value, snippet->description);
		break;

	case PROP_NAME:
		g_value_set_string (value, snippet->name);
		break;

	case PROP_FOCUS_POSITION:
		g_value_set_uint (value, snippet->focus_position);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_snippet_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
	GtkSourceSnippet *snippet = GTK_SOURCE_SNIPPET (object);

	switch (prop_id)
	{
	case PROP_TRIGGER:
		gtk_source_snippet_set_trigger (snippet, g_value_get_string (value));
		break;

	case PROP_LANGUAGE_ID:
		gtk_source_snippet_set_language_id (snippet, g_value_get_string (value));
		break;

	case PROP_DESCRIPTION:
		gtk_source_snippet_set_description (snippet, g_value_get_string (value));
		break;

	case PROP_NAME:
		gtk_source_snippet_set_name (snippet, g_value_get_string (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_snippet_class_init (GtkSourceSnippetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_snippet_dispose;
	object_class->finalize = gtk_source_snippet_finalize;
	object_class->get_property = gtk_source_snippet_get_property;
	object_class->set_property = gtk_source_snippet_set_property;

	properties[PROP_BUFFER] =
		g_param_spec_object ("buffer",
		                     "Buffer",
		                     "The GtkTextBuffer for the snippet",
		                     GTK_TYPE_TEXT_BUFFER,
		                     (G_PARAM_READABLE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_TRIGGER] =
		g_param_spec_string ("trigger",
		                     "Trigger",
		                     "The trigger for the snippet",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_LANGUAGE_ID] =
		g_param_spec_string ("language-id",
		                     "Language Id",
		                     "The language-id for the snippet",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_DESCRIPTION] =
		g_param_spec_string ("description",
		                     "Description",
		                     "The description for the snippet",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_NAME] =
		g_param_spec_string ("name",
		                     "Name",
		                     "The name for the snippet",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_FOCUS_POSITION] =
		g_param_spec_int ("focus-position",
		                  "Focus Position",
		                  "The currently focused chunk",
		                  -1,
		                  G_MAXINT,
		                  -1,
		                  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_snippet_init (GtkSourceSnippet *snippet)
{
	snippet->max_focus_position = -1;
}

gchar *
_gtk_source_snippet_get_edited_text (GtkSourceSnippet *snippet)
{
	GtkTextIter begin;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET (snippet), NULL);

	if (snippet->begin_mark == NULL || snippet->end_mark == NULL)
	{
		return NULL;
	}

	gtk_text_buffer_get_iter_at_mark (snippet->buffer, &begin, snippet->begin_mark);
	gtk_text_buffer_get_iter_at_mark (snippet->buffer, &end, snippet->end_mark);

	return gtk_text_iter_get_slice (&begin, &end);
}

void
_gtk_source_snippet_replace_current_chunk_text (GtkSourceSnippet *snippet,
                                                const gchar      *new_text)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET (snippet));

	if (snippet->current_chunk != NULL)
	{
		gtk_source_snippet_chunk_set_text (snippet->current_chunk, new_text);
		gtk_source_snippet_chunk_set_text_set (snippet->current_chunk, TRUE);
	}
}

/**
 * gtk_source_snippet_new_parsed:
 * @text: the formatted snippet text to parse
 * @error: a location for a #GError or %NULL
 *
 * Parses the snippet formatted @text into a series of chunks and adds them
 * to a new #GtkSourceSnippet.
 *
 * Returns: (transfer full): the newly parsed #GtkSourceSnippet, or %NULL upon
 *   failure and @error is set.
 *
 * Since: 5.6
 */
GtkSourceSnippet *
gtk_source_snippet_new_parsed (const char  *text,
                               GError     **error)
{
	GtkSourceSnippet *snippet;
	GPtrArray *chunks;

	g_return_val_if_fail (text != NULL, NULL);

	chunks = _gtk_source_snippet_bundle_parse_text (text, error);

	if (chunks == NULL)
	{
		return NULL;
	}

	if (chunks->len == 0)
	{
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "Failed to parse any content from snippet text");
		g_ptr_array_unref (chunks);
		return NULL;
	}

	snippet = gtk_source_snippet_new (NULL, NULL);

	for (guint i = 0; i < chunks->len; i++)
	{
		gtk_source_snippet_add_chunk (snippet, g_ptr_array_index (chunks, i));
	}

	g_ptr_array_unref (chunks);

	return g_steal_pointer (&snippet);
}

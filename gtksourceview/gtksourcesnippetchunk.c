/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014-2020 Christian Hergert <chergert@redhat.com>
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

#include "gtksourcesnippetchunk-private.h"
#include "gtksourcesnippetcontext.h"

/**
 * GtkSourceSnippetChunk:
 * 
 * A chunk of text within the source snippet.
 *
 * The `GtkSourceSnippetChunk` represents a single chunk of text that
 * may or may not be an edit point within the snippet. Chunks that are
 * an edit point (also called a tab stop) have the
 * [property@SnippetChunk:focus-position] property set.
 */

G_DEFINE_TYPE (GtkSourceSnippetChunk, gtk_source_snippet_chunk, G_TYPE_INITIALLY_UNOWNED)

enum {
	PROP_0,
	PROP_CONTEXT,
	PROP_SPEC,
	PROP_FOCUS_POSITION,
	PROP_TEXT,
	PROP_TEXT_SET,
	PROP_TOOLTIP_TEXT,
	N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * gtk_source_snippet_chunk_new:
 *
 * Create a new `GtkSourceSnippetChunk` that can be added to
 * a [class@Snippet].
 */
GtkSourceSnippetChunk *
gtk_source_snippet_chunk_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_SNIPPET_CHUNK, NULL);
}

/**
 * gtk_source_snippet_chunk_copy:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Copies the source snippet.
 *
 * Returns: (transfer full): A #GtkSourceSnippetChunk
 */
GtkSourceSnippetChunk *
gtk_source_snippet_chunk_copy (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return g_object_new (GTK_SOURCE_TYPE_SNIPPET_CHUNK,
	                     "spec", chunk->spec,
	                     "focus-position", chunk->focus_position,
	                     NULL);
}

static void
on_context_changed (GtkSourceSnippetContext *context,
                    GtkSourceSnippetChunk   *chunk)
{
	g_assert (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));
	g_assert (GTK_SOURCE_IS_SNIPPET_CONTEXT (context));

	if (!chunk->text_set)
	{
		gchar *text;

		text = gtk_source_snippet_context_expand (context, chunk->spec);
		gtk_source_snippet_chunk_set_text (chunk, text);
		g_free (text);
	}
}

/**
 * gtk_source_snippet_chunk_get_context:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Gets the context for the snippet insertion.
 *
 * Returns: (transfer none): A #GtkSourceSnippetContext
 */
GtkSourceSnippetContext *
gtk_source_snippet_chunk_get_context (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return chunk->context;
}

void
gtk_source_snippet_chunk_set_context (GtkSourceSnippetChunk   *chunk,
                                      GtkSourceSnippetContext *context)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));
	g_return_if_fail (!context || GTK_SOURCE_IS_SNIPPET_CONTEXT (context));

	if (context != chunk->context)
	{
		g_clear_signal_handler (&chunk->context_changed_handler,
		                        chunk->context);
		g_clear_object (&chunk->context);

		if (context != NULL)
		{
			chunk->context = g_object_ref (context);
			chunk->context_changed_handler =
				g_signal_connect_object (chunk->context,
				                         "changed",
				                         G_CALLBACK (on_context_changed),
				                         chunk,
				                         0);
		}

		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_CONTEXT]);
	}
}

/**
 * gtk_source_snippet_chunk_get_spec:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Gets the specification for the chunk.
 *
 * The specification is evaluated for variables when other chunks are edited
 * within the snippet context. If the user has changed the text, the
 * [property@SnippetChunk:text] and [property@SnippetChunk:text-set] properties
 * are updated.
 *
 * Returns: (transfer none) (nullable): the specification, if any
 */
const gchar *
gtk_source_snippet_chunk_get_spec (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return chunk->spec;
}

/**
 * gtk_source_snippet_chunk_set_spec:
 * @chunk: a #GtkSourceSnippetChunk
 * @spec: the new specification for the chunk
 *
 * Sets the specification for the chunk.
 *
 * The specification is evaluated for variables when other chunks are edited
 * within the snippet context. If the user has changed the text, the
 * [property@SnippetChunk:text and] [property@SnippetChunk:text-set] properties
 * are updated.
 */
void
gtk_source_snippet_chunk_set_spec (GtkSourceSnippetChunk *chunk,
                                   const gchar           *spec)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	if (g_strcmp0 (spec, chunk->spec) != 0)
	{
		g_free (chunk->spec);
		chunk->spec = g_strdup (spec);
		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_SPEC]);
	}
}

/**
 * gtk_source_snippet_chunk_get_focus_position:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Gets the [property@SnippetChunk:focus-position].
 *
 * The focus-position is used to determine how many tabs it takes for the
 * snippet to advanced to this chunk.
 *
 * A focus-position of zero will be the last focus position of the snippet
 * and snippet editing ends when it has been reached.
 *
 * A focus-position of -1 means the chunk cannot be focused by the user.
 *
 * Returns: the focus-position
 */
gint
gtk_source_snippet_chunk_get_focus_position (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), 0);

	return chunk->focus_position;
}

/**
 * gtk_source_snippet_chunk_set_focus_position:
 * @chunk: a #GtkSourceSnippetChunk
 * @focus_position: the focus-position
 *
 * Sets the [property@SnippetChunk:focus-position] property.
 *
 * The focus-position is used to determine how many tabs it takes for the
 * snippet to advanced to this chunk.
 *
 * A focus-position of zero will be the last focus position of the snippet
 * and snippet editing ends when it has been reached.
 *
 * A focus-position of -1 means the chunk cannot be focused by the user.
 */
void
gtk_source_snippet_chunk_set_focus_position (GtkSourceSnippetChunk *chunk,
                                             gint                   focus_position)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	focus_position = MAX (focus_position, -1);

	if (chunk->focus_position != focus_position)
	{
		chunk->focus_position = focus_position;
		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_FOCUS_POSITION]);
	}
}

/**
 * gtk_source_snippet_chunk_get_text:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Gets the [property@SnippetChunk:text] property.
 *
 * The text property is updated when the user edits the text of the chunk.
 * If it has not been edited, the [property@SnippetChunk:spec] property is
 * returned.
 *
 * Returns: (not nullable): the text of the chunk
 */
const gchar *
gtk_source_snippet_chunk_get_text (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return chunk->text ? chunk->text : "";
}

/**
 * gtk_source_snippet_chunk_set_text:
 * @chunk: a #GtkSourceSnippetChunk
 * @text: the text of the property
 *
 * Sets the text for the snippet chunk.
 *
 * This is usually used by the snippet engine to update the text, but may
 * be useful when creating custom snippets to avoid expansion of any
 * specification.
 */
void
gtk_source_snippet_chunk_set_text (GtkSourceSnippetChunk *chunk,
                                   const gchar           *text)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	if (g_strcmp0 (chunk->text, text) != 0)
	{
		g_free (chunk->text);
		chunk->text = g_strdup (text);
		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_TEXT]);
	}
}

/**
 * gtk_source_snippet_chunk_get_text_set:
 * @chunk: a #GtkSourceSnippetChunk
 *
 * Gets the [property@SnippetChunk:text-set] property.
 *
 * This is typically set when the user has edited a snippet chunk.
 */
gboolean
gtk_source_snippet_chunk_get_text_set (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), FALSE);

	return chunk->text_set;
}

/**
 * gtk_source_snippet_chunk_set_text_set:
 * @chunk: a #GtkSourceSnippetChunk
 * @text_set: the property value
 *
 * Sets the [property@SnippetChunk:text-set] property.
 *
 * This is typically set when the user has edited a snippet chunk by the
 * snippet engine.
 */
void
gtk_source_snippet_chunk_set_text_set (GtkSourceSnippetChunk *chunk,
                                       gboolean               text_set)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	text_set = !!text_set;

	if (chunk->text_set != text_set)
	{
		chunk->text_set = text_set;
		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_TEXT_SET]);
	}
}

static void
delete_and_unref_mark (GtkTextMark *mark)
{
	g_assert (!mark || GTK_IS_TEXT_MARK (mark));

	if (mark != NULL)
	{
		gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (mark), mark);
		g_object_unref (mark);
	}
}

static void
gtk_source_snippet_chunk_finalize (GObject *object)
{
	GtkSourceSnippetChunk *chunk = (GtkSourceSnippetChunk *)object;

	g_assert (chunk->link.prev == NULL);
	g_assert (chunk->link.next == NULL);

	g_clear_pointer (&chunk->begin_mark, delete_and_unref_mark);
	g_clear_pointer (&chunk->end_mark, delete_and_unref_mark);
	g_clear_pointer (&chunk->spec, g_free);
	g_clear_pointer (&chunk->text, g_free);
	g_clear_pointer (&chunk->tooltip_text, g_free);
	g_clear_object (&chunk->context);

	G_OBJECT_CLASS (gtk_source_snippet_chunk_parent_class)->finalize (object);
}

static void
gtk_source_snippet_chunk_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	GtkSourceSnippetChunk *chunk = GTK_SOURCE_SNIPPET_CHUNK (object);

	switch (prop_id)
	{
	case PROP_CONTEXT:
		g_value_set_object (value, gtk_source_snippet_chunk_get_context (chunk));
		break;

	case PROP_SPEC:
		g_value_set_string (value, gtk_source_snippet_chunk_get_spec (chunk));
		break;

	case PROP_FOCUS_POSITION:
		g_value_set_int (value, gtk_source_snippet_chunk_get_focus_position (chunk));
		break;

	case PROP_TEXT:
		g_value_set_string (value, gtk_source_snippet_chunk_get_text (chunk));
		break;

	case PROP_TEXT_SET:
		g_value_set_boolean (value, gtk_source_snippet_chunk_get_text_set (chunk));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_snippet_chunk_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	GtkSourceSnippetChunk *chunk = GTK_SOURCE_SNIPPET_CHUNK (object);

	switch (prop_id)
	{
	case PROP_CONTEXT:
		gtk_source_snippet_chunk_set_context (chunk, g_value_get_object (value));
		break;

	case PROP_FOCUS_POSITION:
		gtk_source_snippet_chunk_set_focus_position (chunk, g_value_get_int (value));
		break;

	case PROP_SPEC:
		gtk_source_snippet_chunk_set_spec (chunk, g_value_get_string (value));
		break;

	case PROP_TEXT:
		gtk_source_snippet_chunk_set_text (chunk, g_value_get_string (value));
		break;

	case PROP_TEXT_SET:
		gtk_source_snippet_chunk_set_text_set (chunk, g_value_get_boolean (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_snippet_chunk_class_init (GtkSourceSnippetChunkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_snippet_chunk_finalize;
	object_class->get_property = gtk_source_snippet_chunk_get_property;
	object_class->set_property = gtk_source_snippet_chunk_set_property;

	properties[PROP_CONTEXT] =
		g_param_spec_object ("context",
		                     "Context",
		                     "The snippet context.",
		                     GTK_SOURCE_TYPE_SNIPPET_CONTEXT,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_SPEC] =
		g_param_spec_string ("spec",
		                     "Spec",
		                     "The specification to expand using the context.",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_FOCUS_POSITION] =
		g_param_spec_int ("focus-position",
		                  "Focus Position",
		                  "The focus position for the chunk.",
		                  -1,
		                  G_MAXINT,
		                  -1,
		                  (G_PARAM_READWRITE |
		                   G_PARAM_EXPLICIT_NOTIFY |
		                   G_PARAM_STATIC_STRINGS));

	properties[PROP_TEXT] =
		g_param_spec_string ("text",
		                     "Text",
		                     "The text for the chunk.",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	properties[PROP_TEXT_SET] =
		g_param_spec_boolean ("text-set",
		                      "If text property is set",
		                      "If the text property has been manually set.",
		                      FALSE,
		                      (G_PARAM_READWRITE |
		                       G_PARAM_EXPLICIT_NOTIFY |
		                       G_PARAM_STATIC_STRINGS));

	properties[PROP_TOOLTIP_TEXT] =
		g_param_spec_string ("tooltip-text",
		                     "Tooltip Text",
		                     "The tooltip text for the chunk.",
		                     NULL,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_snippet_chunk_init (GtkSourceSnippetChunk *chunk)
{
	chunk->link.data = chunk;
	chunk->focus_position = -1;
	chunk->spec = g_strdup ("");
}

gboolean
_gtk_source_snippet_chunk_get_bounds (GtkSourceSnippetChunk *chunk,
                                      GtkTextIter           *begin,
                                      GtkTextIter           *end)
{
	GtkTextBuffer *buffer;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);

	if (chunk->begin_mark == NULL || chunk->end_mark == NULL)
	{
		return FALSE;
	}

	buffer = gtk_text_mark_get_buffer (chunk->begin_mark);

	gtk_text_buffer_get_iter_at_mark (buffer, begin, chunk->begin_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, end, chunk->end_mark);

	return TRUE;
}

void
_gtk_source_snippet_chunk_save_text (GtkSourceSnippetChunk *chunk)
{
	GtkTextBuffer *buffer;
	GtkTextIter begin;
	GtkTextIter end;

	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	buffer = gtk_text_mark_get_buffer (chunk->begin_mark);

	gtk_text_buffer_get_iter_at_mark (buffer, &begin, chunk->begin_mark);
	gtk_text_buffer_get_iter_at_mark (buffer, &end, chunk->end_mark);

	g_free (chunk->text);
	chunk->text = gtk_text_iter_get_slice (&begin, &end);
	g_object_notify_by_pspec (G_OBJECT (chunk),
	                          properties [PROP_TEXT]);

	if (chunk->text_set != TRUE)
	{
		chunk->text_set = TRUE;
		g_object_notify_by_pspec (G_OBJECT (chunk),
		                          properties [PROP_TEXT_SET]);
	}
}

gboolean
_gtk_source_snippet_chunk_contains (GtkSourceSnippetChunk *chunk,
                                    const GtkTextIter     *iter)
{
	GtkTextIter begin;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	if (_gtk_source_snippet_chunk_get_bounds (chunk, &begin, &end))
	{
		return gtk_text_iter_compare (&begin, iter) <= 0 &&
		       gtk_text_iter_compare (iter, &end) <= 0;
	}

	return FALSE;
}

const char *
gtk_source_snippet_chunk_get_tooltip_text (GtkSourceSnippetChunk *chunk)
{
	g_return_val_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk), NULL);

	return chunk->tooltip_text;
}

void
gtk_source_snippet_chunk_set_tooltip_text (GtkSourceSnippetChunk *chunk,
                                           const char            *tooltip_text)
{
	g_return_if_fail (GTK_SOURCE_IS_SNIPPET_CHUNK (chunk));

	if (g_strcmp0 (tooltip_text, chunk->tooltip_text) != 0)
	{
		g_free (chunk->tooltip_text);
		chunk->tooltip_text = g_strdup (tooltip_text);
		g_object_notify_by_pspec (G_OBJECT (chunk), properties [PROP_TOOLTIP_TEXT]);
	}
}

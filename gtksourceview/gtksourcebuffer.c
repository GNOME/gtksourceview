/*
 * This file is part of GtkSourceView
 *
 * Copyright 1999-2002 - Mikael Hermansson <tyan@linux.se>,
 *                       Chris Phelps <chicane@reninet.com> and
 *                       Jeroen Zwartepoorte <jeroen@xs4all.nl>
 * Copyright 2003 - Paolo Maggi <paolo.maggi@polito.it> and
 *                  Gustavo Giráldez <gustavo.giraldez@gmx.net>
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

#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtksourcelanguage.h"
#include "gtksourcelanguage-private.h"
#include "gtksource-marshal.h"
#include "gtksourcestyle.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcestyleschememanager.h"
#include "gtksourcemark.h"
#include "gtksourcemarkssequence-private.h"
#include "gtksourcesearchcontext-private.h"
#include "gtksourcetag.h"
#include "gtksource-enumtypes.h"
#include "gtksourcetrace.h"

/**
 * GtkSourceBuffer:
 *
 * Subclass of [class@Gtk.TextBuffer].
 *
 * A `GtkSourceBuffer` object is the model for [class@View] widgets.
 * It extends the [class@Gtk.TextBuffer] class by adding features useful to display
 * and edit source code such as syntax highlighting and bracket matching.
 *
 * To create a `GtkSourceBuffer` use [ctor@GtkSource.Buffer.new] or
 * [ctor@GtkSource.Buffer.new_with_language]. The second form is just a convenience
 * function which allows you to initially set a [class@Language]. You can also
 * directly create a [class@View] and get its [class@Buffer] with
 * [method@Gtk.TextView.get_buffer].
 *
 * The highlighting is enabled by default, but you can disable it with
 * [method@Buffer.set_highlight_syntax].
 *
 * # Context Classes:
 *
 * It is possible to retrieve some information from the syntax highlighting
 * engine. The default context classes that are applied to regions of a
 * `GtkSourceBuffer`:
 *
 *  - **comment**: the region delimits a comment;
 *  - **no-spell-check**: the region should not be spell checked;
 *  - **path**: the region delimits a path to a file;
 *  - **string**: the region delimits a string.
 *
 * Custom language definition files can create their own context classes,
 * since the functions like [method@Buffer.iter_has_context_class] take
 * a string parameter as the context class.
 *
 * `GtkSourceBuffer` provides an API to access the context classes:
 * [method@Buffer.iter_has_context_class],
 * [method@Buffer.get_context_classes_at_iter],
 * [method@Buffer.iter_forward_to_context_class_toggle] and
 * [method@Buffer.iter_backward_to_context_class_toggle].
 *
 * And the [signal@GtkSource.Buffer::highlight-updated] signal permits to be notified
 * when a context class region changes.
 *
 * Each context class has also an associated [class@Gtk.TextTag] with the name
 * `gtksourceview:context-classes:<name>`. For example to
 * retrieve the [class@Gtk.TextTag] for the string context class, one can write:
 * ```c
 * GtkTextTagTable *tag_table;
 * GtkTextTag *tag;
 *
 * tag_table = gtk_text_buffer_get_tag_table (buffer);
 * tag = gtk_text_tag_table_lookup (tag_table, "gtksourceview:context-classes:string");
 * ```
 * ```python
 * buffer = GtkSource.Buffer()
 *
 * tag_table = buffer.get_tag_table()
 * tag = tag_table.lookup(name="gtksourceview:context-classes:string")
 * ```
 *
 * The tag must be used for read-only purposes.
 *
 * Accessing a context class via the associated [class@Gtk.TextTag] is less
 * convenient than the `GtkSourceBuffer` API, because:
 *
 *  - The tag doesn't always exist, you need to listen to the
 *    [signal@Gtk.TextTagTable::tag-added] and [signal@Gtk.TextTagTable::tag-removed] signals.
 *  - Instead of the [signal@GtkSource.Buffer::highlight-updated] signal, you can listen
 *    to the [signal@Gtk.TextBuffer::apply-tag] and [signal@Gtk.TextBuffer::remove-tag] signals.
 *
 * A possible use-case for accessing a context class via the associated
 * [class@Gtk.TextTag] is to read the region but without adding a hard dependency on the
 * GtkSourceView library (for example for a spell-checking library that wants to
 * read the no-spell-check region).
 */

#define UPDATE_BRACKET_DELAY_MSEC     50
#define BRACKET_MATCHING_CHARS_LIMIT  10000
#define CONTEXT_CLASSES_PREFIX        "gtksourceview:context-classes:"

enum
{
	CURSOR_MOVED,
	HIGHLIGHT_UPDATED,
	SOURCE_MARK_UPDATED,
	UNDO,
	REDO,
	BRACKET_MATCHED,
	N_SIGNALS
};

enum
{
	PROP_0,
	PROP_HIGHLIGHT_MATCHING_BRACKETS,
	PROP_HIGHLIGHT_SYNTAX,
	PROP_IMPLICIT_TRAILING_NEWLINE,
	PROP_LANGUAGE,
	PROP_LOADING,
	PROP_STYLE_SCHEME,
	N_PROPERTIES
};

typedef struct
{
	GtkTextTag *bracket_match_tag;
	GtkSourceBracketMatchType bracket_match_state;
	guint bracket_highlighting_timeout_id;

	/* Hash table: category -> MarksSequence */
	GHashTable *source_marks;
	GtkSourceMarksSequence *all_source_marks;

	GtkSourceStyleScheme *style_scheme;
	GtkSourceLanguage *language;
	GtkSourceEngine *highlight_engine;

	GtkTextMark *tmp_insert_mark;
	GtkTextMark *tmp_selection_bound_mark;

	GtkTextTag *snippet_focus_tag;

	GList *search_contexts;

	GtkTextTag *invalid_char_tag;

	gint64 insertion_count;

	guint cursor_moved_block_count;

	int loading_count;

	guint has_draw_spaces_tag : 1;
	guint highlight_syntax : 1;
	guint highlight_brackets : 1;
	guint implicit_trailing_newline : 1;
} GtkSourceBufferPrivate;

static GParamSpec *buffer_properties[N_PROPERTIES];
static guint       buffer_signals[N_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceBuffer, gtk_source_buffer, GTK_TYPE_TEXT_BUFFER)

static void gtk_source_buffer_dispose                  (GObject            *object);
static void gtk_source_buffer_set_property             (GObject            *object,
                                                        guint               prop_id,
                                                        const GValue       *value,
                                                        GParamSpec         *pspec);
static void gtk_source_buffer_get_property             (GObject            *object,
                                                        guint               prop_id,
                                                        GValue             *value,
                                                        GParamSpec         *pspec);
static void gtk_source_buffer_real_insert_text         (GtkTextBuffer      *buffer,
                                                        GtkTextIter        *iter,
                                                        const gchar        *text,
                                                        gint                len);
static void gtk_source_buffer_real_insert_paintable    (GtkTextBuffer      *buffer,
                                                        GtkTextIter        *pos,
                                                        GdkPaintable       *paintable);
static void gtk_source_buffer_real_insert_child_anchor (GtkTextBuffer      *buffer,
                                                        GtkTextIter        *pos,
                                                        GtkTextChildAnchor *anchor);
static void gtk_source_buffer_real_delete_range        (GtkTextBuffer      *buffer,
                                                        GtkTextIter        *iter,
                                                        GtkTextIter        *end);
static void gtk_source_buffer_real_mark_set            (GtkTextBuffer      *buffer,
                                                        const GtkTextIter  *location,
                                                        GtkTextMark        *mark);
static void gtk_source_buffer_real_mark_deleted        (GtkTextBuffer      *buffer,
                                                        GtkTextMark        *mark);
static void gtk_source_buffer_real_highlight_updated   (GtkSourceBuffer    *buffer,
                                                        GtkTextIter        *start,
                                                        GtkTextIter        *end);

static void
gtk_source_buffer_check_tag_for_spaces (GtkSourceBuffer *buffer,
                                        GtkSourceTag    *tag)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (!priv->has_draw_spaces_tag)
	{
		gboolean draw_spaces_set;

		g_object_get (tag,
		              "draw-spaces-set", &draw_spaces_set,
		              NULL);

		if (draw_spaces_set)
		{
			priv->has_draw_spaces_tag = TRUE;
		}
	}
}

static void
gtk_source_buffer_tag_changed_cb (GtkTextTagTable *table,
                                  GtkTextTag      *tag,
                                  gboolean         size_changed,
                                  GtkSourceBuffer *buffer)
{
	if (GTK_SOURCE_IS_TAG (tag))
	{
		gtk_source_buffer_check_tag_for_spaces (buffer, GTK_SOURCE_TAG (tag));
	}
}

static void
gtk_source_buffer_tag_added_cb (GtkTextTagTable *table,
                                GtkTextTag      *tag,
                                GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (GTK_SOURCE_IS_TAG (tag))
	{
		gtk_source_buffer_check_tag_for_spaces (buffer, GTK_SOURCE_TAG (tag));
	}

	if (priv->snippet_focus_tag != NULL)
	{
		gtk_text_tag_set_priority (priv->snippet_focus_tag,
		                           gtk_text_tag_table_get_size (table) - 1);
	}
}

static void
gtk_source_buffer_constructed (GObject *object)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (object);
	GtkTextTagTable *table;

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->constructed (object);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	g_signal_connect_object (table,
	                         "tag-changed",
	                         G_CALLBACK (gtk_source_buffer_tag_changed_cb),
	                         buffer, 0);
	g_signal_connect_object (table,
	                         "tag-added",
	                         G_CALLBACK (gtk_source_buffer_tag_added_cb),
	                         buffer, 0);
}

static void
gtk_source_buffer_class_init (GtkSourceBufferClass *klass)
{
	GObjectClass *object_class;
	GtkTextBufferClass *text_buffer_class;

	object_class = G_OBJECT_CLASS (klass);
	text_buffer_class = GTK_TEXT_BUFFER_CLASS (klass);

	object_class->constructed = gtk_source_buffer_constructed;
	object_class->dispose = gtk_source_buffer_dispose;
	object_class->get_property = gtk_source_buffer_get_property;
	object_class->set_property = gtk_source_buffer_set_property;

	text_buffer_class->delete_range = gtk_source_buffer_real_delete_range;
	text_buffer_class->insert_text = gtk_source_buffer_real_insert_text;
	text_buffer_class->insert_paintable = gtk_source_buffer_real_insert_paintable;
	text_buffer_class->insert_child_anchor = gtk_source_buffer_real_insert_child_anchor;
	text_buffer_class->mark_set = gtk_source_buffer_real_mark_set;
	text_buffer_class->mark_deleted = gtk_source_buffer_real_mark_deleted;

	/**
	 * GtkSourceBuffer:highlight-syntax:
	 *
	 * Whether to highlight syntax in the buffer.
	 */
	buffer_properties[PROP_HIGHLIGHT_SYNTAX] =
		g_param_spec_boolean ("highlight-syntax",
				      "Highlight Syntax",
				      "Whether to highlight syntax in the buffer",
				      TRUE,
				      G_PARAM_READWRITE |
				      G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceBuffer:highlight-matching-brackets:
	 *
	 * Whether to highlight matching brackets in the buffer.
	 */
	buffer_properties[PROP_HIGHLIGHT_MATCHING_BRACKETS] =
		g_param_spec_boolean ("highlight-matching-brackets",
				      "Highlight Matching Brackets",
				      "Whether to highlight matching brackets",
				      TRUE,
				      G_PARAM_READWRITE |
				      G_PARAM_STATIC_STRINGS);

	buffer_properties[PROP_LANGUAGE] =
		g_param_spec_object ("language",
				     "Language",
				     "Language object to get highlighting patterns from",
				     GTK_SOURCE_TYPE_LANGUAGE,
				     G_PARAM_READWRITE |
				     G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceBuffer:loading:
	 *
	 * The "loading" property denotes that a `GtkSourceFileLoader` is
	 * currently loading the buffer.
	 *
	 * Applications may want to use this setting to avoid doing work
	 * while the buffer is loading such as spellchecking.
	 *
	 * Since: 5.10
	 */
	buffer_properties[PROP_LOADING] =
		g_param_spec_boolean ("loading",
		                      "Loading",
		                      "If a GtkSourceFileLoader is loading the buffer",
		                      FALSE,
		                      G_PARAM_READABLE |
		                      G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceBuffer:style-scheme:
	 *
	 * Style scheme. It contains styles for syntax highlighting, optionally
	 * foreground, background, cursor color, current line color, and matching
	 * brackets style.
	 */
	buffer_properties[PROP_STYLE_SCHEME] =
		g_param_spec_object ("style-scheme",
				     "Style scheme",
				     "Style scheme",
				     GTK_SOURCE_TYPE_STYLE_SCHEME,
				     G_PARAM_READWRITE |
				     G_PARAM_STATIC_STRINGS);

	/**
	 * GtkSourceBuffer:implicit-trailing-newline:
	 *
	 * Whether the buffer has an implicit trailing newline. See
	 * [method@Buffer.set_implicit_trailing_newline].
	 */
	buffer_properties[PROP_IMPLICIT_TRAILING_NEWLINE] =
		g_param_spec_boolean ("implicit-trailing-newline",
				      "Implicit trailing newline",
				      "",
				      TRUE,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT |
				      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (object_class, N_PROPERTIES, buffer_properties);

	/**
	 * GtkSourceBuffer::cursor-moved:
	 * @buffer: a #GtkSourceBuffer
	 *
	 * The "cursor-moved" signal is emitted when then insertion mark has moved.
	 */
	buffer_signals[CURSOR_MOVED] =
		g_signal_new_class_handler ("cursor-moved",
		                            G_OBJECT_CLASS_TYPE (object_class),
		                            G_SIGNAL_RUN_LAST,
		                            NULL, NULL, NULL, NULL,
		                            G_TYPE_NONE, 0);

	/**
	 * GtkSourceBuffer::highlight-updated:
	 * @buffer: the buffer that received the signal
	 * @start: the start of the updated region
	 * @end: the end of the updated region
	 *
	 * The ::highlight-updated signal is emitted when the syntax
	 * highlighting and [context classes](./class.Buffer.html#context-classes) are updated in a
	 * certain region of the @buffer.
	 */
	buffer_signals[HIGHLIGHT_UPDATED] =
	    g_signal_new_class_handler ("highlight-updated",
	                                G_OBJECT_CLASS_TYPE (object_class),
	                                G_SIGNAL_RUN_LAST,
	                                G_CALLBACK (gtk_source_buffer_real_highlight_updated),
					NULL, NULL,
					_gtk_source_marshal_VOID__BOXED_BOXED,
	                                G_TYPE_NONE,
	                                2,
	                                GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
	                                GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
	g_signal_set_va_marshaller (buffer_signals[HIGHLIGHT_UPDATED],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__BOXED_BOXEDv);

	/**
	 * GtkSourceBuffer::source-mark-updated:
	 * @buffer: the buffer that received the signal
	 * @mark: the [class@Mark]
	 *
	 * The ::source-mark-updated signal is emitted each time
	 * a mark is added to, moved or removed from the @buffer.
	 */
	buffer_signals[SOURCE_MARK_UPDATED] =
	    g_signal_new ("source-mark-updated",
			   G_OBJECT_CLASS_TYPE (object_class),
			   G_SIGNAL_RUN_LAST,
			   0,
			   NULL, NULL,
			   g_cclosure_marshal_VOID__OBJECT,
			   G_TYPE_NONE,
			   1, GTK_TYPE_TEXT_MARK);
	g_signal_set_va_marshaller (buffer_signals[SOURCE_MARK_UPDATED],
	                            G_TYPE_FROM_CLASS (klass),
	                            g_cclosure_marshal_VOID__OBJECTv);

	/**
	 * GtkSourceBuffer::bracket-matched:
	 * @buffer: a #GtkSourceBuffer.
	 * @iter: (nullable): if found, the location of the matching bracket.
	 * @state: state of bracket matching.
	 *
	 * @iter is set to a valid iterator pointing to the matching bracket
	 * if @state is %GTK_SOURCE_BRACKET_MATCH_FOUND. Otherwise @iter is
	 * meaningless.
	 *
	 * The signal is emitted only when the @state changes, typically when
	 * the cursor moves.
	 *
	 * A use-case for this signal is to show messages in a [class@Gtk.Statusbar].
	 */
	buffer_signals[BRACKET_MATCHED] =
	    g_signal_new ("bracket-matched",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (GtkSourceBufferClass, bracket_matched),
			  NULL, NULL,
			  _gtk_source_marshal_VOID__BOXED_ENUM,
			  G_TYPE_NONE, 2,
			  GTK_TYPE_TEXT_ITER,
			  GTK_SOURCE_TYPE_BRACKET_MATCH_TYPE);
	g_signal_set_va_marshaller (buffer_signals[BRACKET_MATCHED],
	                            G_TYPE_FROM_CLASS (klass),
	                            _gtk_source_marshal_VOID__BOXED_ENUMv);
}

static void
search_context_weak_notify_cb (GtkSourceBuffer *buffer,
			       GObject         *search_context)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	priv->search_contexts = g_list_remove (priv->search_contexts, search_context);
}

static void
gtk_source_buffer_init (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	priv->highlight_syntax = TRUE;
	priv->highlight_brackets = TRUE;
	priv->bracket_match_state = GTK_SOURCE_BRACKET_MATCH_NONE;

	priv->source_marks = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify)g_free,
						    (GDestroyNotify)g_object_unref);

	priv->all_source_marks = _gtk_source_marks_sequence_new (GTK_TEXT_BUFFER (buffer));

	priv->style_scheme = _gtk_source_style_scheme_get_default ();

	if (priv->style_scheme != NULL)
	{
		g_object_ref (priv->style_scheme);
	}
}

static void
gtk_source_buffer_dispose (GObject *object)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (object);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GList *l;

	if (priv->bracket_highlighting_timeout_id != 0)
	{
		g_source_remove (priv->bracket_highlighting_timeout_id);
		priv->bracket_highlighting_timeout_id = 0;
	}

	if (priv->highlight_engine != NULL)
	{
		_gtk_source_engine_attach_buffer (priv->highlight_engine, NULL);
	}

	g_clear_object (&priv->highlight_engine);
	g_clear_object (&priv->language);
	g_clear_object (&priv->style_scheme);

	for (l = priv->search_contexts; l != NULL; l = l->next)
	{
		GtkSourceSearchContext *search_context = l->data;

		g_object_weak_unref (G_OBJECT (search_context),
				     (GWeakNotify)search_context_weak_notify_cb,
				     buffer);
	}

	g_list_free (priv->search_contexts);
	priv->search_contexts = NULL;

	g_clear_object (&priv->all_source_marks);

	if (priv->source_marks != NULL)
	{
		g_hash_table_unref (priv->source_marks);
		priv->source_marks = NULL;
	}

	G_OBJECT_CLASS (gtk_source_buffer_parent_class)->dispose (object);
}

static void
gtk_source_buffer_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (object);

	switch (prop_id)
	{
		case PROP_HIGHLIGHT_SYNTAX:
			gtk_source_buffer_set_highlight_syntax (buffer, g_value_get_boolean (value));
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			gtk_source_buffer_set_highlight_matching_brackets (buffer, g_value_get_boolean (value));
			break;

		case PROP_LANGUAGE:
			gtk_source_buffer_set_language (buffer, g_value_get_object (value));
			break;

		case PROP_STYLE_SCHEME:
			gtk_source_buffer_set_style_scheme (buffer, g_value_get_object (value));
			break;

		case PROP_IMPLICIT_TRAILING_NEWLINE:
			gtk_source_buffer_set_implicit_trailing_newline (buffer, g_value_get_boolean (value));
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
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (object);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	switch (prop_id)
	{
		case PROP_HIGHLIGHT_SYNTAX:
			g_value_set_boolean (value, priv->highlight_syntax);
			break;

		case PROP_HIGHLIGHT_MATCHING_BRACKETS:
			g_value_set_boolean (value, priv->highlight_brackets);
			break;

		case PROP_LANGUAGE:
			g_value_set_object (value, priv->language);
			break;

		case PROP_LOADING:
			g_value_set_boolean (value, gtk_source_buffer_get_loading (buffer));
			break;

		case PROP_STYLE_SCHEME:
			g_value_set_object (value, priv->style_scheme);
			break;

		case PROP_IMPLICIT_TRAILING_NEWLINE:
			g_value_set_boolean (value, priv->implicit_trailing_newline);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

/**
 * gtk_source_buffer_new:
 * @table: (nullable): a #GtkTextTagTable, or %NULL to create a new one.
 *
 * Creates a new source buffer.
 *
 * Returns: a new source buffer.
 */
GtkSourceBuffer *
gtk_source_buffer_new (GtkTextTagTable *table)
{
	g_return_val_if_fail (table == NULL || GTK_IS_TEXT_TAG_TABLE (table), NULL);

	return g_object_new (GTK_SOURCE_TYPE_BUFFER,
			     "tag-table", table,
			     NULL);
}

/**
 * gtk_source_buffer_new_with_language:
 * @language: a #GtkSourceLanguage.
 *
 * Creates a new source buffer using the highlighting patterns in `language`.
 *
 * This is equivalent to creating a new source buffer with
 * a new tag table and then calling [method@Buffer.set_language].
 *
 * Returns: a new source buffer which will highlight text
 * according to the highlighting patterns in `language`.
 */
GtkSourceBuffer *
gtk_source_buffer_new_with_language (GtkSourceLanguage *language)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LANGUAGE (language), NULL);

	return g_object_new (GTK_SOURCE_TYPE_BUFFER,
			     "tag-table", NULL,
			     "language", language,
			     NULL);
}

static void
update_snippet_focus_style (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GtkSourceStyle *style = NULL;

	if (priv->snippet_focus_tag == NULL)
	{
		return;
	}

	if (priv->style_scheme != NULL)
	{
		style = _gtk_source_style_scheme_get_snippet_focus_style (priv->style_scheme);
	}

	gtk_source_style_apply (style, priv->snippet_focus_tag);
}

GtkTextTag *
_gtk_source_buffer_get_snippet_focus_tag (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (priv->snippet_focus_tag == NULL)
	{
		priv->snippet_focus_tag =
			gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
						    NULL,
						    NULL);
		update_snippet_focus_style (buffer);
	}

	return priv->snippet_focus_tag;
}

static void
update_bracket_match_style (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GtkSourceStyle *style = NULL;

	if (priv->bracket_match_tag == NULL)
	{
		return;
	}

	if (priv->style_scheme != NULL)
	{
		style = _gtk_source_style_scheme_get_matching_brackets_style (priv->style_scheme);
	}

	gtk_source_style_apply (style, priv->bracket_match_tag);
}

static GtkTextTag *
get_bracket_match_tag (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (priv->bracket_match_tag == NULL)
	{
		priv->bracket_match_tag =
			gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
						    NULL,
						    NULL);
		update_bracket_match_style (buffer);
	}

	return priv->bracket_match_tag;
}

/* This is private, just used by the print compositor to not print bracket
 * matches. Note that unlike get_bracket_match_tag() it returns NULL
 * if the tag is not set.
 */
GtkTextTag *
_gtk_source_buffer_get_bracket_match_tag (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	return priv->bracket_match_tag;
}

static gunichar
bracket_pair (gunichar  base_char,
	      gint     *direction)
{
	gint dir;
	gunichar pair;

	switch (base_char)
	{
		case '{':
			dir = 1;
			pair = '}';
			break;

		case '(':
			dir = 1;
			pair = ')';
			break;

		case '[':
			dir = 1;
			pair = ']';
			break;

		case '<':
			dir = 1;
			pair = '>';
			break;

		case '}':
			dir = -1;
			pair = '{';
			break;

		case ')':
			dir = -1;
			pair = '(';
			break;

		case ']':
			dir = -1;
			pair = '[';
			break;

		case '>':
			dir = -1;
			pair = '<';
			break;

		default:
			dir = 0;
			pair = 0;
			break;
	}

	if (direction != NULL)
	{
		*direction = dir;
	}

	return pair;
}

/*
 * This function works similar to gtk_text_buffer_remove_tag() except that
 * instead of taking the optimization to make removing tags fast in terms
 * of wall clock time, it tries to avoiding to much time of the screen
 * by minimizing the damage regions. This results in fewer full-redraws
 * when updating the text marks. To see the difference, compare this to
 * gtk_text_buffer_remove_tag() and enable the "show pixel cache" feature
 * the GTK inspector.
 */
static void
remove_tag_with_minimal_damage (GtkTextBuffer     *buffer,
                                GtkTextTag        *tag,
                                const GtkTextIter *begin,
                                const GtkTextIter *end)
{
	GtkTextIter tag_begin = *begin;
	GtkTextIter tag_end;

	if (!gtk_text_iter_starts_tag (&tag_begin, tag))
	{
		if (!gtk_text_iter_forward_to_tag_toggle (&tag_begin, tag))
		{
			return;
		}
	}

	while (gtk_text_iter_starts_tag (&tag_begin, tag) &&
	       gtk_text_iter_compare (&tag_begin, end) < 0)
	{
		gint count = 1;

		tag_end = tag_begin;

		/*
		 * We might have found the start of another tag embedded
		 * inside this tag. So keep scanning forward until we have
		 * reached the right number of end tags.
		 */

		while (gtk_text_iter_forward_to_tag_toggle (&tag_end, tag))
		{
			if (gtk_text_iter_starts_tag (&tag_end, tag))
			{
				count++;
			}
			else if (gtk_text_iter_ends_tag (&tag_end, tag))
			{
				if (--count == 0)
				{
					break;
				}
			}
		}

		if (gtk_text_iter_ends_tag (&tag_end, tag))
		{
			gtk_text_buffer_remove_tag (buffer, tag, &tag_begin, &tag_end);

			tag_begin = tag_end;

			/*
			 * Move to the next start tag. It's possible to have an overlapped
			 * end tag, which would be non-ideal, but possible.
			 */
			if (!gtk_text_iter_starts_tag (&tag_begin, tag))
			{
				while (gtk_text_iter_forward_to_tag_toggle (&tag_begin, tag))
				{
					if (gtk_text_iter_starts_tag (&tag_begin, tag))
					{
						break;
					}
				}
			}
		}
	}
}

static void
update_bracket_highlighting (GtkSourceBuffer *source_buffer)
{
	GtkTextBuffer *buffer = GTK_TEXT_BUFFER (source_buffer);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (source_buffer);
	GtkTextIter insert_iter;
	GtkTextIter bracket;
	GtkTextIter bracket_match;
	GtkSourceBracketMatchType previous_state;

	if (priv->bracket_match_tag != NULL)
	{
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_bounds (buffer, &start, &end);

		remove_tag_with_minimal_damage (GTK_TEXT_BUFFER (source_buffer),
		                                priv->bracket_match_tag,
		                                &start,
		                                &end);
	}

	if (!priv->highlight_brackets)
	{
		if (priv->bracket_match_tag != NULL)
		{
			GtkTextTagTable *table;

			table = gtk_text_buffer_get_tag_table (buffer);
			gtk_text_tag_table_remove (table, priv->bracket_match_tag);
			priv->bracket_match_tag = NULL;
		}

		return;
	}

	gtk_text_buffer_get_iter_at_mark (buffer,
					  &insert_iter,
					  gtk_text_buffer_get_insert (buffer));

	previous_state = priv->bracket_match_state;
	priv->bracket_match_state =
		_gtk_source_buffer_find_bracket_match (source_buffer,
						       &insert_iter,
						       &bracket,
						       &bracket_match);

	if (priv->bracket_match_state == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		GtkTextIter next_iter;

		g_signal_emit (source_buffer,
			       buffer_signals[BRACKET_MATCHED],
			       0,
			       &bracket_match,
			       GTK_SOURCE_BRACKET_MATCH_FOUND);

		next_iter = bracket_match;
		gtk_text_iter_forward_char (&next_iter);
		gtk_text_buffer_apply_tag (buffer,
					   get_bracket_match_tag (source_buffer),
					   &bracket_match,
					   &next_iter);

		next_iter = bracket;
		gtk_text_iter_forward_char (&next_iter);
		gtk_text_buffer_apply_tag (buffer,
					   get_bracket_match_tag (source_buffer),
					   &bracket,
					   &next_iter);
		return;
	}

	/* Don't emit the signal at all if chars at previous and current
	 * positions are nonbrackets.
	 */
	if (previous_state != GTK_SOURCE_BRACKET_MATCH_NONE ||
	    priv->bracket_match_state != GTK_SOURCE_BRACKET_MATCH_NONE)
	{
		g_signal_emit (source_buffer,
			       buffer_signals[BRACKET_MATCHED],
			       0,
			       NULL,
			       priv->bracket_match_state);
	}
}

static gboolean
bracket_highlighting_timeout_cb (gpointer user_data)
{
	GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (user_data);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	update_bracket_highlighting (buffer);

	priv->bracket_highlighting_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

static void
queue_bracket_highlighting_update (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	/* Short-circuit unless we're processing bracket highlighting. Since
	 * gtk_source_buffer_set_highlight_matching_brackets() will call
	 * update_bracket_highlighting() already, this short-circuit is safe
	 * as we'll already be in stable state.
	 */
	if (priv->highlight_brackets == FALSE)
	{
		return;
	}

	/* Rearm existing GSource when possible */
	if (priv->bracket_highlighting_timeout_id != 0)
	{
		GSource *source;
		gint64 ready_time;

		ready_time = g_get_monotonic_time () +
			     (G_USEC_PER_SEC / 1000L) * UPDATE_BRACKET_DELAY_MSEC;

		source = g_main_context_find_source_by_id (NULL, priv->bracket_highlighting_timeout_id);
		g_source_set_ready_time (source, ready_time);
		return;
	}

	/* Queue an update to the bracket location instead of doing it
	 * immediately. We are likely going to be servicing a draw deadline
	 * immediately, so blocking to find the match and invalidating
	 * visible regions causes animations to stutter. Instead, give
	 * ourself just a little bit of a delay to catch up.
	 *
	 * The value for this delay was found experimentally, as 25msec
	 * resulted in continuing to see frame stutter, but 50 was not
	 * distinguishable from having matching-brackets disabled.
	 * The animation in gtkscrolledwindow is 200, but that creates
	 * an undesirable delay before the match is shown to the user.
	 * 50msec errors on the side of "immediate", but without the
	 * frame stutter.
	 *
	 * If we had access to a GdkFrameClock, we might consider using
	 * ::update() or ::after-paint() to synchronize this.
	 */
	priv->bracket_highlighting_timeout_id =
		g_timeout_add_full (G_PRIORITY_LOW,
		                    UPDATE_BRACKET_DELAY_MSEC,
		                    bracket_highlighting_timeout_cb,
		                    buffer,
		                    NULL);
}

/* Although this function is not really useful
 * (queue_bracket_highlighting_update() could be called directly), the name
 * cursor_moved() is more meaningful.
 */
static void
cursor_moved (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (priv->cursor_moved_block_count > 0)
	{
		return;
	}

	queue_bracket_highlighting_update (buffer);

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	g_signal_emit (buffer, buffer_signals[CURSOR_MOVED], 0);

	GTK_SOURCE_PROFILER_END_MARK ("GtkSourceBuffer::cursor-moved", NULL);
}

static void
gtk_source_buffer_real_highlight_updated (GtkSourceBuffer *buffer,
					  GtkTextIter     *start,
					  GtkTextIter     *end)
{
	queue_bracket_highlighting_update (buffer);
}

static void
gtk_source_buffer_content_inserted (GtkTextBuffer *buffer,
				    gint           start_offset,
				    gint           end_offset)
{
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (source_buffer);

	cursor_moved (source_buffer);

	if (priv->highlight_engine != NULL)
	{
		_gtk_source_engine_text_inserted (priv->highlight_engine,
						  start_offset,
						  end_offset);
	}
}

static void
gtk_source_buffer_real_insert_text (GtkTextBuffer *buffer,
				    GtkTextIter   *iter,
				    const gchar   *text,
				    gint           len)
{
	GtkSourceBuffer *source_buffer = (GtkSourceBuffer *)buffer;
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (source_buffer);
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (text != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/* iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text.
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_text (buffer, iter, text, len);

	priv->insertion_count++;

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

/* insert_paintable and insert_child_anchor do nothing except notifying
 * the highlighting engine about the change, because engine's idea
 * of buffer char count must be correct at all times.
 */
static void
gtk_source_buffer_real_insert_paintable (GtkTextBuffer *buffer,
                                         GtkTextIter   *iter,
                                         GdkPaintable  *paintable)
{
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/* iter is invalidated when
	 * insertion occurs (because the buffer contents change), but the
	 * default signal handler revalidates it to point to the end of the
	 * inserted text.
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_paintable (buffer, iter, paintable);

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

static void
gtk_source_buffer_real_insert_child_anchor (GtkTextBuffer      *buffer,
					    GtkTextIter        *iter,
					    GtkTextChildAnchor *anchor)
{
	gint start_offset;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

	start_offset = gtk_text_iter_get_offset (iter);

	/* iter is invalidated when insertion occurs (because the buffer
	 * contents change), but the default signal handler revalidates it to
	 * point to the end of the inserted text.
	 */
	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->insert_child_anchor (buffer, iter, anchor);

	gtk_source_buffer_content_inserted (buffer,
					    start_offset,
					    gtk_text_iter_get_offset (iter));
}

static void
gtk_source_buffer_real_delete_range (GtkTextBuffer *buffer,
				     GtkTextIter   *start,
				     GtkTextIter   *end)
{
	gint offset, length;
	GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (source_buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);
	g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
	g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

	gtk_text_iter_order (start, end);
	offset = gtk_text_iter_get_offset (start);
	length = gtk_text_iter_get_offset (end) - offset;

	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->delete_range (buffer, start, end);

	cursor_moved (source_buffer);

	/* emit text deleted for engines */
	if (priv->highlight_engine != NULL)
	{
		_gtk_source_engine_text_deleted (priv->highlight_engine,
						 offset, length);
	}
}

static gint
get_bracket_matching_context_class_mask (GtkSourceBuffer *buffer,
					 GtkTextIter     *iter)
{
	gint mask = 0;
	guint i;

	/* This describes a mask of relevant context classes for highlighting
	 * matching brackets.
	 */
	const gchar *cclass_mask_definitions[] = {
		"comment",
		"string",
	};

	for (i = 0; i < G_N_ELEMENTS (cclass_mask_definitions); ++i)
	{
		gboolean has_class;

		has_class = gtk_source_buffer_iter_has_context_class (buffer,
								      iter,
								      cclass_mask_definitions[i]);

		mask |= has_class << i;
	}

	return mask;
}

/* Note that we only look BRACKET_MATCHING_CHARS_LIMIT at most.
 * @pos is moved to the bracket match, if found.
 */
static GtkSourceBracketMatchType
find_bracket_match_real (GtkSourceBuffer *buffer,
			 GtkTextIter     *pos)
{
	GtkTextIter iter;
	gunichar base_char;
	gunichar search_char;
	gint direction;
	gint bracket_count;
	gint char_count;
	gint cclass_mask;
	gboolean found;

	base_char = gtk_text_iter_get_char (pos);
	search_char = bracket_pair (base_char, &direction);

	if (direction == 0)
	{
		return GTK_SOURCE_BRACKET_MATCH_NONE;
	}

	cclass_mask = get_bracket_matching_context_class_mask (buffer, pos);

	iter = *pos;
	bracket_count = 0;
	char_count = 0;
	found = FALSE;

	do
	{
		gunichar cur_char;
		gint cur_mask;

		gtk_text_iter_forward_chars (&iter, direction);
		cur_char = gtk_text_iter_get_char (&iter);
		char_count++;

		cur_mask = get_bracket_matching_context_class_mask (buffer, &iter);

		/* Check if we lost a class, which means we don't look any
		 * further.
		 */
		if ((cclass_mask & cur_mask) != cclass_mask)
		{
			found = FALSE;
			break;
		}

		if (cclass_mask != cur_mask)
		{
			continue;
		}

		if (cur_char == search_char)
		{
			if (bracket_count == 0)
			{
				found = TRUE;
				break;
			}

			bracket_count--;
		}
		else if (cur_char == base_char)
		{
			bracket_count++;
		}
	}
	while (!gtk_text_iter_is_end (&iter) &&
	       !gtk_text_iter_is_start (&iter) &&
	       char_count < BRACKET_MATCHING_CHARS_LIMIT);

	if (found)
	{
		*pos = iter;
		return GTK_SOURCE_BRACKET_MATCH_FOUND;
	}

	if (char_count >= BRACKET_MATCHING_CHARS_LIMIT)
	{
		return GTK_SOURCE_BRACKET_MATCH_OUT_OF_RANGE;
	}

	return GTK_SOURCE_BRACKET_MATCH_NOT_FOUND;
}

/* Note that we take into account both the character following @pos and the one
 * preceding it. If there are brackets on both sides, the one following @pos
 * takes precedence.
 * @bracket and @bracket_match are valid only if GTK_SOURCE_BRACKET_MATCH_FOUND
 * is returned. @bracket is set either to @pos or @pos-1.
 */
GtkSourceBracketMatchType
_gtk_source_buffer_find_bracket_match (GtkSourceBuffer   *buffer,
				       const GtkTextIter *pos,
				       GtkTextIter       *bracket,
				       GtkTextIter       *bracket_match)
{
	GtkSourceBracketMatchType result_right;
	GtkSourceBracketMatchType result_left;
	GtkTextIter prev;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), GTK_SOURCE_BRACKET_MATCH_NONE);
	g_return_val_if_fail (pos != NULL, GTK_SOURCE_BRACKET_MATCH_NONE);
	g_return_val_if_fail (bracket_match != NULL, GTK_SOURCE_BRACKET_MATCH_NONE);

	*bracket_match = *pos;
	result_right = find_bracket_match_real (buffer, bracket_match);

	if (result_right == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		if (bracket != NULL)
		{
			*bracket = *pos;
		}

		return GTK_SOURCE_BRACKET_MATCH_FOUND;
	}

	prev = *pos;
	if (!gtk_text_iter_starts_line (&prev) &&
	    gtk_text_iter_backward_cursor_position (&prev))
	{
		*bracket_match = prev;
		result_left = find_bracket_match_real (buffer, bracket_match);
	}
	else
	{
		result_left = GTK_SOURCE_BRACKET_MATCH_NONE;
	}

	if (result_left == GTK_SOURCE_BRACKET_MATCH_FOUND)
	{
		if (bracket != NULL)
		{
			*bracket = prev;
		}

		return GTK_SOURCE_BRACKET_MATCH_FOUND;
	}

	/* If there is a bracket, the expected return value is for the bracket,
	 * not the other character.
	 */
	if (result_right == GTK_SOURCE_BRACKET_MATCH_NONE)
	{
		return result_left;
	}
	if (result_left == GTK_SOURCE_BRACKET_MATCH_NONE)
	{
		return result_right;
	}

	/* There are brackets on both sides, and none was successful. The one on
	 * the right takes precedence.
	 */
	return result_right;
}

/**
 * gtk_source_buffer_get_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether bracket match highlighting is activated for the
 * source buffer.
 *
 * Returns: %TRUE if the source buffer will highlight matching
 * brackets.
 */
gboolean
gtk_source_buffer_get_highlight_matching_brackets (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return priv->highlight_brackets;
}

/**
 * gtk_source_buffer_set_highlight_matching_brackets:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE if you want matching brackets highlighted.
 *
 * Controls the bracket match highlighting function in the buffer.
 *
 * If activated, when you position your cursor over a bracket character
 * (a parenthesis, a square bracket, etc.) the matching opening or
 * closing bracket character will be highlighted.
 */
void
gtk_source_buffer_set_highlight_matching_brackets (GtkSourceBuffer *buffer,
						   gboolean         highlight)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	highlight = highlight != FALSE;

	if (highlight != priv->highlight_brackets)
	{
		priv->highlight_brackets = highlight;

		update_bracket_highlighting (buffer);

		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_HIGHLIGHT_MATCHING_BRACKETS]);
	}
}

/**
 * gtk_source_buffer_get_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 *
 * Determines whether syntax highlighting is activated in the source
 * buffer.
 *
 * Returns: %TRUE if syntax highlighting is enabled, %FALSE otherwise.
 */
gboolean
gtk_source_buffer_get_highlight_syntax (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return priv->highlight_syntax;
}

/**
 * gtk_source_buffer_set_highlight_syntax:
 * @buffer: a #GtkSourceBuffer.
 * @highlight: %TRUE to enable syntax highlighting, %FALSE to disable it.
 *
 * Controls whether syntax is highlighted in the buffer.
 *
 * If @highlight is %TRUE, the text will be highlighted according to the syntax
 * patterns specified in the [class@Language] set with [method@Buffer.set_language].
 *
 * If @highlight is %FALSE, syntax highlighting is disabled and all the
 * [class@Gtk.TextTag] objects that have been added by the syntax highlighting engine
 * are removed from the buffer.
 */
void
gtk_source_buffer_set_highlight_syntax (GtkSourceBuffer *buffer,
					gboolean         highlight)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	highlight = highlight != FALSE;

	if (priv->highlight_syntax != highlight)
	{
		priv->highlight_syntax = highlight;
		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_HIGHLIGHT_SYNTAX]);
	}
}

/**
 * gtk_source_buffer_set_language:
 * @buffer: a #GtkSourceBuffer.
 * @language: (nullable): a #GtkSourceLanguage to set, or %NULL.
 *
 * Associates a [class@Language] with the buffer.
 *
 * Note that a [class@Language] affects not only the syntax highlighting, but
 * also the [context classes](./class.Buffer.html#context-classes). If you want to disable just the
 * syntax highlighting, see [method@Buffer.set_highlight_syntax].
 *
 * The buffer holds a reference to @language.
 */
void
gtk_source_buffer_set_language (GtkSourceBuffer   *buffer,
				GtkSourceLanguage *language)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_LANGUAGE (language) || language == NULL);

	if (!g_set_object (&priv->language, language))
	{
		return;
	}

	if (priv->highlight_engine != NULL)
	{
		/* disconnect the old engine */
		_gtk_source_engine_attach_buffer (priv->highlight_engine, NULL);
		g_object_unref (priv->highlight_engine);
		priv->highlight_engine = NULL;
	}

	if (language != NULL)
	{
		/* get a new engine */
		priv->highlight_engine = _gtk_source_language_create_engine (language);

		if (priv->highlight_engine != NULL)
		{
			_gtk_source_engine_attach_buffer (priv->highlight_engine,
							  GTK_TEXT_BUFFER (buffer));

			if (priv->style_scheme != NULL)
			{
				_gtk_source_engine_set_style_scheme (priv->highlight_engine,
								     priv->style_scheme);
			}
		}
	}

	g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_LANGUAGE]);
}

/**
 * gtk_source_buffer_get_language:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the [class@Language] associated with the buffer,
 * see [method@Buffer.set_language].
 *
 * The returned object should not be unreferenced by the user.
 *
 * Returns: (nullable) (transfer none): the [class@Language] associated
 * with the buffer, or %NULL.
 */
GtkSourceLanguage *
gtk_source_buffer_get_language (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return priv->language;
}

/*
 * _gtk_source_buffer_update_syntax_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 * @synchronous: whether the area should be highlighted synchronously.
 *
 * Asks the buffer to analyze and highlight given area.
 */
void
_gtk_source_buffer_update_syntax_highlight (GtkSourceBuffer   *buffer,
					    const GtkTextIter *start,
					    const GtkTextIter *end,
					    gboolean           synchronous)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	if (priv->highlight_engine != NULL)
	{
		_gtk_source_engine_update_highlight (priv->highlight_engine,
						     start,
						     end,
						     synchronous);
	}
}

void
_gtk_source_buffer_update_search_highlight (GtkSourceBuffer   *buffer,
					    const GtkTextIter *start,
					    const GtkTextIter *end,
					    gboolean           synchronous)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	for (GList *l = priv->search_contexts; l != NULL; l = l->next)
	{
		GtkSourceSearchContext *search_context = l->data;

		_gtk_source_search_context_update_highlight (search_context,
							     start,
							     end,
							     synchronous);
	}
}

gboolean
_gtk_source_buffer_has_search_highlights (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	for (GList *l = priv->search_contexts; l != NULL; l = l->next)
	{
		GtkSourceSearchContext *search_context = l->data;

		if (gtk_source_search_context_get_highlight (search_context))
			return TRUE;
	}

	return FALSE;
}

/**
 * gtk_source_buffer_ensure_highlight:
 * @buffer: a #GtkSourceBuffer.
 * @start: start of the area to highlight.
 * @end: end of the area to highlight.
 *
 * Forces buffer to analyze and highlight the given area synchronously.
 *
 * **Note**:
 *
 * This is a potentially slow operation and should be used only
 * when you need to make sure that some text not currently
 * visible is highlighted, for instance before printing.
 **/
void
gtk_source_buffer_ensure_highlight (GtkSourceBuffer   *buffer,
				    const GtkTextIter *start,
				    const GtkTextIter *end)
{
	_gtk_source_buffer_update_syntax_highlight (buffer, start, end, TRUE);
	_gtk_source_buffer_update_search_highlight (buffer, start, end, TRUE);
}

/**
 * gtk_source_buffer_set_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 * @scheme: (nullable): a #GtkSourceStyleScheme or %NULL.
 *
 * Sets a [class@StyleScheme] to be used by the buffer and the view.
 *
 * Note that a [class@StyleScheme] affects not only the syntax highlighting,
 * but also other [class@View] features such as highlighting the current line,
 * matching brackets, the line numbers, etc.
 *
 * Instead of setting a %NULL @scheme, it is better to disable syntax
 * highlighting with [method@Buffer.set_highlight_syntax], and setting the
 * [class@StyleScheme] with the "classic" or "tango" ID, because those two
 * style schemes follow more closely the GTK theme (for example for the
 * background color).
 *
 * The buffer holds a reference to @scheme.
 */
void
gtk_source_buffer_set_style_scheme (GtkSourceBuffer      *buffer,
				    GtkSourceStyleScheme *scheme)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_STYLE_SCHEME (scheme) || scheme == NULL);

	if (g_set_object (&priv->style_scheme, scheme))
	{
		update_bracket_match_style (buffer);

		if (priv->highlight_engine != NULL)
		{
			_gtk_source_engine_set_style_scheme (priv->highlight_engine, scheme);
		}

		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_STYLE_SCHEME]);
	}
}

/**
 * gtk_source_buffer_get_style_scheme:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns the [class@StyleScheme] associated with the buffer,
 * see [method@Buffer.set_style_scheme].
 *
 * The returned object should not be unreferenced by the user.
 *
 * Returns: (nullable) (transfer none): the [class@StyleScheme]
 * associated with the buffer, or %NULL.
 */
GtkSourceStyleScheme *
gtk_source_buffer_get_style_scheme (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	return priv->style_scheme;
}

static void
add_source_mark (GtkSourceBuffer *buffer,
		 GtkSourceMark   *mark)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	const gchar *category;
	GtkSourceMarksSequence *seq;

	_gtk_source_marks_sequence_add (priv->all_source_marks,
					GTK_TEXT_MARK (mark));

	category = gtk_source_mark_get_category (mark);
	seq = g_hash_table_lookup (priv->source_marks, category);

	if (seq == NULL)
	{
		seq = _gtk_source_marks_sequence_new (GTK_TEXT_BUFFER (buffer));

		g_hash_table_insert (priv->source_marks,
				     g_strdup (category),
				     seq);
	}

	_gtk_source_marks_sequence_add (seq, GTK_TEXT_MARK (mark));
}

static void
gtk_source_buffer_real_mark_set	(GtkTextBuffer     *buffer,
				 const GtkTextIter *location,
				 GtkTextMark       *mark)
{
	if (GTK_SOURCE_IS_MARK (mark))
	{
		add_source_mark (GTK_SOURCE_BUFFER (buffer),
				 GTK_SOURCE_MARK (mark));

		g_signal_emit (buffer, buffer_signals[SOURCE_MARK_UPDATED], 0, mark);
	}
	else if (mark == gtk_text_buffer_get_insert (buffer))
	{
		cursor_moved (GTK_SOURCE_BUFFER (buffer));
	}

	GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_set (buffer, location, mark);
}

static void
gtk_source_buffer_real_mark_deleted (GtkTextBuffer *buffer,
				     GtkTextMark   *mark)
{
	if (GTK_SOURCE_IS_MARK (mark))
	{
		GtkSourceBuffer *source_buffer = GTK_SOURCE_BUFFER (buffer);
		GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (source_buffer);
		const gchar *category;
		GtkSourceMarksSequence *seq;

		category = gtk_source_mark_get_category (GTK_SOURCE_MARK (mark));
		seq = g_hash_table_lookup (priv->source_marks, category);

		if (_gtk_source_marks_sequence_is_empty (seq))
		{
			g_hash_table_remove (priv->source_marks, category);
		}

		g_signal_emit (buffer, buffer_signals[SOURCE_MARK_UPDATED], 0, mark);
	}

	if (GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_deleted != NULL)
	{
		GTK_TEXT_BUFFER_CLASS (gtk_source_buffer_parent_class)->mark_deleted (buffer, mark);
	}
}

/**
 * gtk_source_buffer_create_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @name: (nullable): the name of the mark, or %NULL.
 * @category: a string defining the mark category.
 * @where: location to place the mark.
 *
 * Creates a source mark in the `buffer` of category `category`.
 *
 * A source mark is a [class@Gtk.TextMark] but organized into categories.
 * Depending on the category a pixbuf can be specified that will be displayed
 * along the line of the mark.
 *
 * Like a [class@Gtk.TextMark], a [class@Mark] can be anonymous if the
 * passed `name` is %NULL.  Also, the buffer owns the marks so you
 * shouldn't unreference it.
 *
 * Marks always have left gravity and are moved to the beginning of
 * the line when the user deletes the line they were in.
 *
 * Typical uses for a source mark are bookmarks, breakpoints, current
 * executing instruction indication in a source file, etc..
 *
 * Returns: (transfer none): a new [class@Mark], owned by the buffer.
 */
GtkSourceMark *
gtk_source_buffer_create_source_mark (GtkSourceBuffer   *buffer,
				      const gchar       *name,
				      const gchar       *category,
				      const GtkTextIter *where)
{
	GtkSourceMark *mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (category != NULL, NULL);
	g_return_val_if_fail (where != NULL, NULL);

	mark = gtk_source_mark_new (name, category);
	gtk_text_buffer_add_mark (GTK_TEXT_BUFFER (buffer),
				  GTK_TEXT_MARK (mark),
				  where);

	/* We want to return a borrowed reference and the mark is already
	 * owned by @buffer due to gtk_text_buffer_add_mark(). Therefore
	 * it is safe to unref immediately.
	 */
	g_object_unref (mark);

	return mark;
}

static GtkSourceMarksSequence *
get_marks_sequence (GtkSourceBuffer *buffer,
		    const gchar     *category)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	return category == NULL ?
		priv->all_source_marks :
		g_hash_table_lookup (priv->source_marks, category);
}

GtkSourceMark *
_gtk_source_buffer_source_mark_next (GtkSourceBuffer *buffer,
				     GtkSourceMark   *mark,
				     const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextMark *next_mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	next_mark = _gtk_source_marks_sequence_next (seq, GTK_TEXT_MARK (mark));

	return next_mark == NULL ? NULL : GTK_SOURCE_MARK (next_mark);
}

GtkSourceMark *
_gtk_source_buffer_source_mark_prev (GtkSourceBuffer *buffer,
				     GtkSourceMark   *mark,
				     const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextMark *prev_mark;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	prev_mark = _gtk_source_marks_sequence_prev (seq, GTK_TEXT_MARK (mark));

	return prev_mark == NULL ? NULL : GTK_SOURCE_MARK (prev_mark);
}

/**
 * gtk_source_buffer_forward_iter_to_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): an iterator.
 * @category: (nullable): category to search for, or %NULL
 *
 * Moves `iter` to the position of the next [class@Mark] of the given
 * `category`.
 *
 * Returns %TRUE if `iter` was moved. If `category` is NULL, the
 * next source mark can be of any category.
 *
 * Returns: whether `iter` was moved.
 */
gboolean
gtk_source_buffer_forward_iter_to_source_mark (GtkSourceBuffer *buffer,
					       GtkTextIter     *iter,
					       const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return FALSE;
	}

	return _gtk_source_marks_sequence_forward_iter (seq, iter);
}

/**
 * gtk_source_buffer_backward_iter_to_source_mark:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): an iterator.
 * @category: (nullable): category to search for, or %NULL
 *
 * Moves `iter` to the position of the previous [class@Mark] of the given
 * category.
 *
 * Returns %TRUE if `iter` was moved. If `category` is NULL, the
 * previous source mark can be of any category.
 *
 * Returns: whether `iter` was moved.
 */
gboolean
gtk_source_buffer_backward_iter_to_source_mark (GtkSourceBuffer *buffer,
						GtkTextIter     *iter,
						const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return FALSE;
	}

	return _gtk_source_marks_sequence_backward_iter (seq, iter);
}

/**
 * gtk_source_buffer_get_source_marks_at_iter:
 * @buffer: a #GtkSourceBuffer.
 * @iter: an iterator.
 * @category: (nullable): category to search for, or %NULL
 *
 * Returns the list of marks of the given category at @iter.
 *
 * If @category is %NULL it returns all marks at @iter.
 *
 * Returns: (element-type GtkSource.Mark) (transfer container):
 * a newly allocated #GSList.
 */
GSList *
gtk_source_buffer_get_source_marks_at_iter (GtkSourceBuffer *buffer,
					    GtkTextIter     *iter,
					    const gchar     *category)
{
	GtkSourceMarksSequence *seq;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	return _gtk_source_marks_sequence_get_marks_at_iter (seq, iter);
}

/**
 * gtk_source_buffer_get_source_marks_at_line:
 * @buffer: a #GtkSourceBuffer.
 * @line: a line number.
 * @category: (nullable): category to search for, or %NULL
 *
 * Returns the list of marks of the given category at @line.
 *
 * If @category is %NULL, all marks at @line are returned.
 *
 * Returns: (element-type GtkSource.Mark) (transfer container):
 * a newly allocated #GSList.
 */
GSList *
gtk_source_buffer_get_source_marks_at_line (GtkSourceBuffer *buffer,
					    gint             line,
					    const gchar     *category)
{
	GtkSourceMarksSequence *seq;
	GtkTextIter start;
	GtkTextIter end;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return NULL;
	}

	gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (buffer),
					  &start,
					  line);

	end = start;

	if (!gtk_text_iter_ends_line (&end))
	{
		gtk_text_iter_forward_to_line_end (&end);
	}

	return _gtk_source_marks_sequence_get_marks_in_range (seq, &start, &end);
}

/**
 * gtk_source_buffer_remove_source_marks:
 * @buffer: a #GtkSourceBuffer.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 * @category: (nullable): category to search for, or %NULL.
 *
 * Remove all marks of @category between @start and @end from the buffer.
 *
 * If @category is NULL, all marks in the range will be removed.
 */
void
gtk_source_buffer_remove_source_marks (GtkSourceBuffer   *buffer,
				       const GtkTextIter *start,
				       const GtkTextIter *end,
				       const gchar       *category)
{
	GtkSourceMarksSequence *seq;
	GSList *list;
	GSList *l;

 	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
 	g_return_if_fail (start != NULL);
 	g_return_if_fail (end != NULL);

	seq = get_marks_sequence (buffer, category);

	if (seq == NULL)
	{
		return;
	}

	list = _gtk_source_marks_sequence_get_marks_in_range (seq, start, end);

	for (l = list; l != NULL; l = l->next)
	{
		gtk_text_buffer_delete_mark (GTK_TEXT_BUFFER (buffer), l->data);
	}

	g_slist_free (list);
}

gboolean
_gtk_source_buffer_has_source_marks (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	if (priv->all_source_marks != NULL)
	{
		return !_gtk_source_marks_sequence_is_empty (priv->all_source_marks);
	}

	return FALSE;
}

static GtkTextTag *
get_context_class_tag (GtkSourceBuffer *buffer,
		       const gchar     *context_class)
{
	gchar *tag_name;
	GtkTextTagTable *tag_table;
	GtkTextTag *tag;

	tag_name = g_strdup_printf (CONTEXT_CLASSES_PREFIX "%s", context_class);

	tag_table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	tag = gtk_text_tag_table_lookup (tag_table, tag_name);

	g_free (tag_name);
	return tag;
}

/**
 * gtk_source_buffer_iter_has_context_class:
 * @buffer: a #GtkSourceBuffer.
 * @iter: a #GtkTextIter.
 * @context_class: class to search for.
 *
 * Check if the class @context_class is set on @iter.
 *
 * See the [class@Buffer] description for the list of default context classes.
 *
 * Returns: whether @iter has the context class.
 */
gboolean
gtk_source_buffer_iter_has_context_class (GtkSourceBuffer   *buffer,
                                          const GtkTextIter *iter,
                                          const gchar       *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	tag = get_context_class_tag (buffer, context_class);

	if (tag != NULL)
	{
		return gtk_text_iter_has_tag (iter, tag);
	}

	return FALSE;
}

/**
 * gtk_source_buffer_get_context_classes_at_iter:
 * @buffer: a #GtkSourceBuffer.
 * @iter: a #GtkTextIter.
 *
 * Get all defined context classes at @iter.
 *
 * See the [class@Buffer] description for the list of default context classes.
 *
 * Returns: (array zero-terminated=1) (transfer full): a new %NULL
 * terminated array of context class names.
 * Use g_strfreev() to free the array if it is no longer needed.
 */
gchar **
gtk_source_buffer_get_context_classes_at_iter (GtkSourceBuffer   *buffer,
                                               const GtkTextIter *iter)
{
	const gsize prefix_len = strlen (CONTEXT_CLASSES_PREFIX);
	GSList *tags;
	GSList *l;
	GPtrArray *ret;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	tags = gtk_text_iter_get_tags (iter);
	ret = g_ptr_array_new ();

	for (l = tags; l != NULL; l = l->next)
	{
		GtkTextTag *tag = l->data;
		gchar *tag_name;

		g_object_get (tag, "name", &tag_name, NULL);

		if (tag_name != NULL &&
		    g_str_has_prefix (tag_name, CONTEXT_CLASSES_PREFIX))
		{
			gchar *context_class_name = g_strdup (tag_name + prefix_len);

			g_ptr_array_add (ret, context_class_name);
		}

		g_free (tag_name);
	}

	g_slist_free (tags);

	g_ptr_array_add (ret, NULL);
	return (gchar **) g_ptr_array_free (ret, FALSE);
}

/**
 * gtk_source_buffer_iter_forward_to_context_class_toggle:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): a #GtkTextIter.
 * @context_class: the context class.
 *
 * Moves forward to the next toggle (on or off) of the context class.
 *
 * If no matching context class toggles are found, returns %FALSE, otherwise %TRUE.
 * Does not return toggles located at @iter, only toggles after @iter. Sets
 * @iter to the location of the toggle, or to the end of the buffer if no
 * toggle is found.
 *
 * See the [class@Buffer] description for the list of default context classes.
 *
 * Returns: whether we found a context class toggle after @iter
 */
gboolean
gtk_source_buffer_iter_forward_to_context_class_toggle (GtkSourceBuffer *buffer,
                                                        GtkTextIter     *iter,
                                                        const gchar     *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	tag = get_context_class_tag (buffer, context_class);

	if (tag != NULL)
	{
		return gtk_text_iter_forward_to_tag_toggle (iter, tag);
	}

	return FALSE;
}

/**
 * gtk_source_buffer_iter_backward_to_context_class_toggle:
 * @buffer: a #GtkSourceBuffer.
 * @iter: (inout): a #GtkTextIter.
 * @context_class: the context class.
 *
 * Moves backward to the next toggle (on or off) of the context class.
 *
 * If no matching context class toggles are found, returns %FALSE, otherwise %TRUE.
 * Does not return toggles located at @iter, only toggles after @iter. Sets
 * @iter to the location of the toggle, or to the end of the buffer if no
 * toggle is found.
 *
 * See the [class@Buffer] description for the list of default context classes.
 *
 * Returns: whether we found a context class toggle before @iter
 */
gboolean
gtk_source_buffer_iter_backward_to_context_class_toggle (GtkSourceBuffer *buffer,
                                                         GtkTextIter     *iter,
                                                         const gchar     *context_class)
{
	GtkTextTag *tag;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (context_class != NULL, FALSE);

	tag = get_context_class_tag (buffer, context_class);

	if (tag != NULL)
	{
		return gtk_text_iter_backward_to_tag_toggle (iter, tag);
	}

	return FALSE;
}

/*
 * GtkTextView wastes a lot of time tracking the clipboard content if
 * we do insert/delete operations while there is a selection.
 * These two utilities store the current selection with marks before
 * doing an edit operation and restore it at the end.
 */
void
_gtk_source_buffer_save_and_clear_selection (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GtkTextBuffer *buf;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	buf = GTK_TEXT_BUFFER (buffer);

	/* Note we cannot use buffer_get_selection_bounds since it
	 * orders the iters while we want to know the position of
	 * each mark.
	 */
	if (gtk_text_buffer_get_has_selection (GTK_TEXT_BUFFER (buffer)))
	{
		GtkTextIter insert_iter;
		GtkTextIter selection_bound_iter;

		g_assert (priv->tmp_insert_mark == NULL);
		g_assert (priv->tmp_selection_bound_mark == NULL);

		gtk_text_buffer_get_iter_at_mark (buf, &insert_iter, gtk_text_buffer_get_insert (buf));
		gtk_text_buffer_get_iter_at_mark (buf, &selection_bound_iter, gtk_text_buffer_get_selection_bound (buf));
		priv->tmp_insert_mark = gtk_text_buffer_create_mark (buf, NULL, &insert_iter, FALSE);
		priv->tmp_selection_bound_mark = gtk_text_buffer_create_mark (buf, NULL, &selection_bound_iter, FALSE);

		gtk_text_buffer_place_cursor (buf, &insert_iter);
	}
}

void
_gtk_source_buffer_restore_selection (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	if (priv->tmp_insert_mark != NULL &&
	    priv->tmp_selection_bound_mark != NULL)
	{
		GtkTextBuffer *buf;
		GtkTextIter insert_iter;
		GtkTextIter selection_bound_iter;

		buf = GTK_TEXT_BUFFER (buffer);

		gtk_text_buffer_get_iter_at_mark (buf, &insert_iter, priv->tmp_insert_mark);
		gtk_text_buffer_get_iter_at_mark (buf, &selection_bound_iter, priv->tmp_selection_bound_mark);

		gtk_text_buffer_select_range (buf, &insert_iter, &selection_bound_iter);

		gtk_text_buffer_delete_mark (buf, priv->tmp_insert_mark);
		gtk_text_buffer_delete_mark (buf, priv->tmp_selection_bound_mark);
		priv->tmp_insert_mark = NULL;
		priv->tmp_selection_bound_mark = NULL;
	}
}

static gchar *
do_lower_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	gchar *text;
	gchar *new_text;

	text = gtk_text_buffer_get_text (buffer, start, end, TRUE);
	new_text = g_utf8_strdown (text, -1);

	g_free (text);
	return new_text;
}

static gchar *
do_upper_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	gchar *text;
	gchar *new_text;

	text = gtk_text_buffer_get_text (buffer, start, end, TRUE);
	new_text = g_utf8_strup (text, -1);

	g_free (text);
	return new_text;
}

static gchar *
do_toggle_case (GtkTextBuffer     *buffer,
		const GtkTextIter *start,
		const GtkTextIter *end)
{
	GString *str;
	GtkTextIter iter_start;

	str = g_string_new (NULL);
	iter_start = *start;

	while (!gtk_text_iter_is_end (&iter_start))
	{
		GtkTextIter iter_end;
		gchar *text;
		gchar *text_down;
		gchar *text_up;

		iter_end = iter_start;
		gtk_text_iter_forward_cursor_position (&iter_end);

		if (gtk_text_iter_compare (end, &iter_end) < 0)
		{
			break;
		}

		text = gtk_text_buffer_get_text (buffer, &iter_start, &iter_end, TRUE);
		text_down = g_utf8_strdown (text, -1);
		text_up = g_utf8_strup (text, -1);

		if (g_strcmp0 (text, text_down) == 0)
		{
			g_string_append (str, text_up);
		}
		else if (g_strcmp0 (text, text_up) == 0)
		{
			g_string_append (str, text_down);
		}
		else
		{
			g_string_append (str, text);
		}

		g_free (text);
		g_free (text_down);
		g_free (text_up);

		iter_start = iter_end;
	}

	return g_string_free (str, FALSE);
}

static gchar *
do_title_case (GtkTextBuffer     *buffer,
	       const GtkTextIter *start,
	       const GtkTextIter *end)
{
	GString *str;
	GtkTextIter iter_start;

	str = g_string_new (NULL);
	iter_start = *start;

	while (!gtk_text_iter_is_end (&iter_start))
	{
		GtkTextIter iter_end;
		gchar *text;

		iter_end = iter_start;
		gtk_text_iter_forward_cursor_position (&iter_end);

		if (gtk_text_iter_compare (end, &iter_end) < 0)
		{
			break;
		}

		text = gtk_text_buffer_get_text (buffer, &iter_start, &iter_end, TRUE);

		if (gtk_text_iter_starts_word (&iter_start))
		{
			gchar *text_normalized;

			text_normalized = g_utf8_normalize (text, -1, G_NORMALIZE_DEFAULT);

			if (g_utf8_strlen (text_normalized, -1) == 1)
			{
				gunichar c;
				gunichar new_c;

				c = gtk_text_iter_get_char (&iter_start);
				new_c = g_unichar_totitle (c);

				g_string_append_unichar (str, new_c);
			}
			else
			{
				gchar *text_up;

				text_up = g_utf8_strup (text, -1);
				g_string_append (str, text_up);

				g_free (text_up);
			}

			g_free (text_normalized);
		}
		else
		{
			gchar *text_down;

			text_down = g_utf8_strdown (text, -1);
			g_string_append (str, text_down);

			g_free (text_down);
		}

		g_free (text);
		iter_start = iter_end;
	}

	return g_string_free (str, FALSE);
}

/**
 * gtk_source_buffer_change_case:
 * @buffer: a #GtkSourceBuffer.
 * @case_type: how to change the case.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 *
 * Changes the case of the text between the specified iterators.
 *
 * Since 5.4, this function will update the position of `start` and
 * `end` to surround the modified text.
 */
void
gtk_source_buffer_change_case (GtkSourceBuffer         *buffer,
                               GtkSourceChangeCaseType  case_type,
                               GtkTextIter             *start,
                               GtkTextIter             *end)
{
	GtkTextBuffer *text_buffer;
	gchar *new_text;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	gtk_text_iter_order (start, end);

	text_buffer = GTK_TEXT_BUFFER (buffer);

	switch (case_type)
	{
		case GTK_SOURCE_CHANGE_CASE_LOWER:
			new_text = do_lower_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_UPPER:
			new_text = do_upper_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_TOGGLE:
			new_text = do_toggle_case (text_buffer, start, end);
			break;

		case GTK_SOURCE_CHANGE_CASE_TITLE:
			new_text = do_title_case (text_buffer, start, end);
			break;

		default:
			g_return_if_reached ();
	}

	gtk_text_buffer_begin_user_action (text_buffer);
	gtk_text_buffer_delete (text_buffer, start, end);
	gtk_text_buffer_insert (text_buffer, start, new_text, -1);
	gtk_text_buffer_end_user_action (text_buffer);

        /* Leave @start and @end in valid positions around the text */
	*end = *start;
	gtk_text_iter_backward_chars (start, g_utf8_strlen (new_text, -1));

	g_free (new_text);
}

/* Move to the end of the line excluding trailing spaces. */
static void
move_to_line_text_end(GtkTextIter *iter)
{
	gint line;

	line = gtk_text_iter_get_line (iter);

	if (!gtk_text_iter_ends_line (iter))
	{
		gtk_text_iter_forward_to_line_end (iter);
	}

	while (gtk_text_iter_backward_char (iter) &&
	       (gtk_text_iter_get_line (iter) == line))
	{
		gunichar ch;

		ch = gtk_text_iter_get_char (iter);
		if (!g_unichar_isspace (ch))
		{
			break;
		}
	}

	gtk_text_iter_forward_char (iter);
}

/**
 * gtk_source_buffer_join_lines:
 * @buffer: a #GtkSourceBuffer.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 *
 * Joins the lines of text between the specified iterators.
 */
void
gtk_source_buffer_join_lines (GtkSourceBuffer *buffer,
                              GtkTextIter     *start,
                              GtkTextIter     *end)
{
	GtkTextBuffer *text_buffer;
	GtkTextMark *end_mark;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	gtk_text_iter_order (start, end);

	text_buffer = GTK_TEXT_BUFFER (buffer);
	end_mark = gtk_text_buffer_create_mark (text_buffer, NULL, end, FALSE);

	_gtk_source_buffer_save_and_clear_selection (buffer);
	gtk_text_buffer_begin_user_action (text_buffer);

	move_to_line_text_end (start);
	if (!gtk_text_iter_ends_line (end))
	{
		gtk_text_iter_forward_to_line_end (end);
	}

	while (gtk_text_iter_compare (start, end) < 0)
	{
		GtkTextIter iter;
		gunichar ch;

		iter = *start;

		do
		{
			ch = gtk_text_iter_get_char (&iter);
			if (!g_unichar_isspace (ch))
			{
				break;
			}
		} while (gtk_text_iter_forward_char (&iter) &&
			 gtk_text_iter_compare (&iter, end) < 0);

		if (!gtk_text_iter_is_end (&iter))
		{
			gtk_text_buffer_delete (text_buffer, start, &iter);
			if (!gtk_text_iter_ends_line (start))
			{
				gtk_text_buffer_insert (text_buffer, start, " ", 1);
			}
		}

		move_to_line_text_end (start);

		gtk_text_buffer_get_iter_at_mark (text_buffer, end, end_mark);
	}

	gtk_text_buffer_end_user_action (text_buffer);
	_gtk_source_buffer_restore_selection (buffer);

	gtk_text_buffer_delete_mark (text_buffer, end_mark);
}

static gchar *
get_line_slice (GtkTextBuffer *buf,
		gint           line)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_iter_at_line (buf, &start, line);
	end = start;

	if (!gtk_text_iter_ends_line (&start))
	{
		gtk_text_iter_forward_to_line_end (&end);
	}

	return gtk_text_buffer_get_slice (buf, &start, &end, TRUE);
}

typedef struct {
	gchar *line; /* the original text to re-insert */
	gchar *key;  /* the key to use for the comparison */
} SortLine;

static gint
compare_line (gconstpointer aptr,
              gconstpointer bptr)
{
	const SortLine *a = aptr;
	const SortLine *b = bptr;

	return g_strcmp0 (a->key, b->key);
}

static gint
compare_line_reversed (gconstpointer aptr,
                       gconstpointer bptr)
{
	const SortLine *a = aptr;
	const SortLine *b = bptr;

	return g_strcmp0 (b->key, a->key);
}

typedef char *(*SortKeyFunc) (const char *str,
                              gssize      column);

static char *
sort_collate_key (const char *str,
                  gssize      column)
{
	gchar *casefolded, *collated;

	while (column > 0 && *str)
	{
		str = g_utf8_next_char (str);
		column--;
	}

	casefolded = g_utf8_casefold (str, -1);
	collated = g_utf8_collate_key (casefolded, -1);
	g_free (casefolded);

	return collated;
}

static char *
sort_filename_key (const char *str,
                   gssize      column)
{
	while (column > 0 && *str)
	{
		str = g_utf8_next_char (str);
		column--;
	}

	return g_utf8_collate_key_for_filename (str, -1);
}

static char *
sort_raw_key (const char *str,
              gssize      column)
{
	while (column > 0 && *str)
	{
		str = g_utf8_next_char (str);
		column--;
	}

	return g_strdup (str);
}

/**
 * gtk_source_buffer_sort_lines:
 * @buffer: a #GtkSourceBuffer.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 * @flags: #GtkSourceSortFlags specifying how the sort should behave
 * @column: sort considering the text starting at the given column
 *
 * Sort the lines of text between the specified iterators.
 */
void
gtk_source_buffer_sort_lines (GtkSourceBuffer    *buffer,
                              GtkTextIter        *start,
                              GtkTextIter        *end,
                              GtkSourceSortFlags  flags,
                              gint                column)
{
	SortKeyFunc key_func = NULL;
	GtkTextBuffer *text_buffer;
	GHashTable *dedup = NULL;
	gint start_line;
	gint end_line;
	gint num_lines;
	SortLine *lines;
	gint i;
	gint j;

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	text_buffer = GTK_TEXT_BUFFER (buffer);

	gtk_text_iter_order (start, end);

	start_line = gtk_text_iter_get_line (start);
	end_line = gtk_text_iter_get_line (end);

	/* Required for gtk_text_buffer_delete() */
	if (!gtk_text_iter_starts_line (start))
	{
		gtk_text_iter_set_line_offset (start, 0);
	}

	/* if we are at line start our last line is the previous one.
	 * Otherwise the last line is the current one but we try to
	 * move the iter after the line terminator */
	if (gtk_text_iter_starts_line (end))
	{
		end_line = MAX (start_line, end_line - 1);
	}
	else
	{
		gtk_text_iter_forward_line (end);
	}

	if (start_line == end_line)
	{
		return;
	}

	dedup = g_hash_table_new (g_str_hash, g_str_equal);

	if ((flags & GTK_SOURCE_SORT_FLAGS_CASE_SENSITIVE) != 0)
	{
		key_func = sort_raw_key;
	}
	else if ((flags & GTK_SOURCE_SORT_FLAGS_FILENAME) != 0)
	{
		key_func = sort_filename_key;
	}
	else
	{
		key_func = sort_collate_key;
	}

	num_lines = end_line - start_line + 1;
	lines = g_new0 (SortLine, num_lines);

	for (i = 0, j = 0; i < num_lines; i++)
	{
		lines[j].line = get_line_slice (text_buffer, start_line + i);
		lines[j].key = key_func (lines[j].line, column);

		if ((flags & GTK_SOURCE_SORT_FLAGS_REMOVE_DUPLICATES) != 0)
		{
			if (g_hash_table_contains (dedup, lines[j].line))
			{
				g_free (lines[j].line);
				g_free (lines[j].key);
				continue;
			}

			g_hash_table_add (dedup, lines[j].line);
		}

		j++;
	}

	num_lines = j;

	g_clear_pointer (&dedup, g_hash_table_unref);

	if ((flags & GTK_SOURCE_SORT_FLAGS_REVERSE_ORDER) != 0)
	{
		qsort (lines, num_lines, sizeof (SortLine), compare_line_reversed);
	}
	else
	{
		qsort (lines, num_lines, sizeof (SortLine), compare_line);
	}

	_gtk_source_buffer_save_and_clear_selection (buffer);
	gtk_text_buffer_begin_user_action (text_buffer);

	gtk_text_buffer_delete (text_buffer, start, end);

	for (i = 0; i < num_lines; i++)
	{
		gtk_text_buffer_insert (text_buffer, start, lines[i].line, -1);
		gtk_text_buffer_insert (text_buffer, start, "\n", -1);
	}

	gtk_text_buffer_end_user_action (text_buffer);
	_gtk_source_buffer_restore_selection (buffer);

	for (i = 0; i < num_lines; i++)
	{
		g_free (lines[i].line);
		g_free (lines[i].key);
	}

	g_free (lines);
}

void
_gtk_source_buffer_add_search_context (GtkSourceBuffer        *buffer,
				       GtkSourceSearchContext *search_context)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (GTK_SOURCE_IS_SEARCH_CONTEXT (search_context));
	g_return_if_fail (gtk_source_search_context_get_buffer (search_context) == buffer);

	if (g_list_find (priv->search_contexts, search_context) != NULL)
	{
		return;
	}

	priv->search_contexts = g_list_prepend (priv->search_contexts,
							search_context);

	g_object_weak_ref (G_OBJECT (search_context),
			   (GWeakNotify)search_context_weak_notify_cb,
			   buffer);
}

static void
sync_invalid_char_tag (GtkSourceBuffer *buffer,
		       GParamSpec      *pspec,
		       gpointer         data)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GtkSourceStyle *style = NULL;

	if (priv->style_scheme != NULL)
	{
		style = gtk_source_style_scheme_get_style (priv->style_scheme, "def:error");
	}

	gtk_source_style_apply (style, priv->invalid_char_tag);
}

static void
text_tag_set_highest_priority (GtkTextTag    *tag,
			       GtkTextBuffer *buffer)
{
	GtkTextTagTable *table;
	gint n;

	table = gtk_text_buffer_get_tag_table (buffer);
	n = gtk_text_tag_table_get_size (table);
	gtk_text_tag_set_priority (tag, n - 1);
}

void
_gtk_source_buffer_set_as_invalid_character (GtkSourceBuffer   *buffer,
					     const GtkTextIter *start,
					     const GtkTextIter *end)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	if (priv->invalid_char_tag == NULL)
	{
		priv->invalid_char_tag = gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (buffer),
									     "invalid-char-style",
									     NULL);

		sync_invalid_char_tag (buffer, NULL, NULL);

		g_signal_connect (buffer,
		                  "notify::style-scheme",
		                  G_CALLBACK (sync_invalid_char_tag),
		                  NULL);
	}

	/* Make sure the 'error' tag has the priority over
	 * syntax highlighting tags.
	 */
	text_tag_set_highest_priority (priv->invalid_char_tag,
	                               GTK_TEXT_BUFFER (buffer));

	gtk_text_buffer_apply_tag (GTK_TEXT_BUFFER (buffer),
	                           priv->invalid_char_tag,
	                           start,
	                           end);
}

gboolean
_gtk_source_buffer_has_invalid_chars (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);
	GtkTextIter start;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	if (priv->invalid_char_tag == NULL)
	{
		return FALSE;
	}

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (buffer), &start);

	if (gtk_text_iter_starts_tag (&start, priv->invalid_char_tag) ||
	    gtk_text_iter_forward_to_tag_toggle (&start, priv->invalid_char_tag))
	{
		return TRUE;
	}

	return FALSE;
}

/**
 * gtk_source_buffer_set_implicit_trailing_newline:
 * @buffer: a #GtkSourceBuffer.
 * @implicit_trailing_newline: the new value.
 *
 * Sets whether the @buffer has an implicit trailing newline.
 *
 * If an explicit trailing newline is present in a [class@Gtk.TextBuffer], [class@Gtk.TextView]
 * shows it as an empty line. This is generally not what the user expects.
 *
 * If @implicit_trailing_newline is %TRUE (the default value):
 *  - when a [class@FileLoader] loads the content of a file into the @buffer,
 *    the trailing newline (if present in the file) is not inserted into the
 *    @buffer.
 *  - when a [class@FileSaver] saves the content of the @buffer into a file, a
 *    trailing newline is added to the file.
 *
 * On the other hand, if @implicit_trailing_newline is %FALSE, the file's
 * content is not modified when loaded into the @buffer, and the @buffer's
 * content is not modified when saved into a file.
 */
void
gtk_source_buffer_set_implicit_trailing_newline (GtkSourceBuffer *buffer,
						 gboolean         implicit_trailing_newline)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	implicit_trailing_newline = implicit_trailing_newline != FALSE;

	if (priv->implicit_trailing_newline != implicit_trailing_newline)
	{
		priv->implicit_trailing_newline = implicit_trailing_newline;
		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_IMPLICIT_TRAILING_NEWLINE]);
	}
}

/**
 * gtk_source_buffer_get_implicit_trailing_newline:
 * @buffer: a #GtkSourceBuffer.
 *
 * Returns: whether the @buffer has an implicit trailing newline.
 */
gboolean
gtk_source_buffer_get_implicit_trailing_newline (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), TRUE);

	return priv->implicit_trailing_newline;
}

/**
 * gtk_source_buffer_create_source_tag:
 * @buffer: a #GtkSourceBuffer
 * @tag_name: (nullable): name of the new tag, or %NULL
 * @first_property_name: (nullable): name of first property to set, or %NULL
 * @...: %NULL-terminated list of property names and values
 *
 * In short, this is the same function as [method@Gtk.TextBuffer.create_tag] but
 * instead of creating a [class@Gtk.TextTag], this function creates a [class@Tag].
 *
 * This function creates a [class@Tag] and adds it to the tag table for
 * `buffer`.  Equivalent to calling [ctor@Gtk.TextTag.new] and then adding the tag to
 * the buffer’s tag table. The returned tag is owned by the buffer’s tag table,
 * so the ref count will be equal to one.
 *
 * If `tag_name` is %NULL, the tag is anonymous.
 *
 * If `tag_name` is non-%NULL, a tag called `tag_name` must not already
 * exist in the tag table for this buffer.
 *
 * The `first_property_name` argument and subsequent arguments are a list
 * of properties to set on the tag, as with [method@GObject.Object.set].
 *
 * Returns: (transfer none): a new [class@Tag].
 */
GtkTextTag *
gtk_source_buffer_create_source_tag (GtkSourceBuffer *buffer,
				     const gchar     *tag_name,
				     const gchar     *first_property_name,
				     ...)
{
	GtkTextTag *tag;
	GtkTextTagTable *table;
	va_list list;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);

	tag = gtk_source_tag_new (tag_name);

	table = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (buffer));
	if (!gtk_text_tag_table_add (table, tag))
	{
		g_object_unref (tag);
		return NULL;
	}

	if (first_property_name != NULL)
	{
		va_start (list, first_property_name);
		g_object_set_valist (G_OBJECT (tag), first_property_name, list);
		va_end (list);
	}

	g_object_unref (tag);

	return tag;
}

gboolean
_gtk_source_buffer_has_spaces_tag (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return priv->has_draw_spaces_tag;
}

gint64
_gtk_source_buffer_get_insertion_count (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);

	return priv->insertion_count;
}

void
_gtk_source_buffer_block_cursor_moved (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));

	priv->cursor_moved_block_count++;
}

void
_gtk_source_buffer_unblock_cursor_moved (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (priv->cursor_moved_block_count > 0);

	priv->cursor_moved_block_count--;

	if (priv->cursor_moved_block_count == 0)
	{
		cursor_moved (buffer);
	}
}

void
_gtk_source_buffer_begin_loading (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (priv->loading_count >= 0);

	priv->loading_count++;

	if (priv->loading_count == 1)
	{
		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_LOADING]);
	}
}

void
_gtk_source_buffer_end_loading (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_if_fail (GTK_SOURCE_IS_BUFFER (buffer));
	g_return_if_fail (priv->loading_count > 0);

	priv->loading_count--;

	if (priv->loading_count == 0)
	{
		g_object_notify_by_pspec (G_OBJECT (buffer), buffer_properties[PROP_LOADING]);
	}
}

gboolean
gtk_source_buffer_get_loading (GtkSourceBuffer *buffer)
{
	GtkSourceBufferPrivate *priv = gtk_source_buffer_get_instance_private (buffer);

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), FALSE);
	g_return_val_if_fail (priv->loading_count >= 0, FALSE);

	return priv->loading_count > 0;
}

enum {
	FLAGS_0,
	FLAGS_BGCOLOR         = 1 << 0,
	FLAGS_COLOR           = 1 << 1,
	FLAGS_UNDERLINE_COLOR = 1 << 2,
	FLAGS_WEIGHT          = 1 << 3,
	FLAGS_STYLE           = 1 << 4,
	FLAGS_UNDERLINE       = 1 << 5,
	FLAGS_STRIKETHROUGH   = 1 << 6,
	FLAGS_SCALE           = 1 << 7,
};

#define GET_FLAG(f, F) (*(f) & (F))
#define SET_FLAG(f, F) (*(f) |= (F))

static void
add_attributes_for_tag (GtkTextTag *tag,
                        GString    *attrs,
			guint      *flags)
{
	if (gtk_text_tag_get_priority (tag) < 0)
		return;

	/* Handle background color */
	if (!GET_FLAG (flags, FLAGS_BGCOLOR))
	{
		gboolean background_rgba_set;

		g_object_get (tag,
		              "background-set", &background_rgba_set,
		              NULL);

		if (background_rgba_set)
		{
			GdkRGBA *bg_color = NULL;

			g_object_get (tag,
			              "background-rgba", &bg_color,
			              NULL);

			if (bg_color != NULL)
			{
				if (attrs->len > 0)
					g_string_append_c (attrs, ' ');

				g_string_printf (attrs,
				                 "bgcolor=\"#%02x%02x%02x%02x\"",
				                 (int)(bg_color->red * 255),
				                 (int)(bg_color->green * 255),
				                 (int)(bg_color->blue * 255),
				                 (int)(bg_color->alpha * 255));

				SET_FLAG (flags, FLAGS_BGCOLOR);

				gdk_rgba_free (bg_color);
			}
		}
	}

	/* Handle foreground color */
	if (!GET_FLAG (flags, FLAGS_COLOR))
	{
		gboolean foreground_rgba_set;

		g_object_get (tag,
		              "foreground-set", &foreground_rgba_set,
		              NULL);

		if (foreground_rgba_set)
		{
			GdkRGBA *fg_color = NULL;

			g_object_get (tag,
			              "foreground-rgba", &fg_color,
			              NULL);

			if (fg_color != NULL)
			{
				if (attrs->len > 0)
					g_string_append_c (attrs, ' ');

				g_string_printf (attrs,
				                 "color=\"#%02x%02x%02x%02x\"",
				                 (int)(fg_color->red * 255),
				                 (int)(fg_color->green * 255),
				                 (int)(fg_color->blue * 255),
				                 (int)(fg_color->alpha * 255));

				SET_FLAG (flags, FLAGS_COLOR);

				gdk_rgba_free (fg_color);
			}
		}
	}

	/* Handle underline color */
	if (!GET_FLAG (flags, FLAGS_UNDERLINE_COLOR))
	{
		gboolean underline_rgba_set;

		g_object_get (tag,
		              "underline-rgba-set", &underline_rgba_set,
		              NULL);

		if (underline_rgba_set)
		{
			GdkRGBA *underline_color = NULL;

			g_object_get (tag,
			              "underline-rgba", &underline_color,
			              NULL);

			if (underline_color != NULL)
			{
				if (attrs->len > 0)
					g_string_append_c (attrs, ' ');

				g_string_printf (attrs,
				                 "underline_color=\"#%02x%02x%02x%02x\"",
				                 (int)(underline_color->red * 255),
				                 (int)(underline_color->green * 255),
				                 (int)(underline_color->blue * 255),
				                 (int)(underline_color->alpha * 255));

				SET_FLAG (flags, FLAGS_UNDERLINE_COLOR);

				gdk_rgba_free (underline_color);
			}
		}
	}

	/* Handle font weight */
	if (!GET_FLAG (flags, FLAGS_WEIGHT))
	{
		gboolean weight_set;

		g_object_get (tag,
			      "weight-set", &weight_set,
			      NULL);

		if (weight_set)
		{
			PangoWeight weight;

			g_object_get (tag,
			              "weight", &weight,
			              NULL);

			if (attrs->len > 0)
				g_string_append_c (attrs, ' ');

			if (weight == PANGO_WEIGHT_THIN)
				g_string_append (attrs, "weight=\"thin\"");
			else if (weight == PANGO_WEIGHT_ULTRALIGHT)
				g_string_append (attrs, "weight=\"ultralight\"");
			else if (weight == PANGO_WEIGHT_LIGHT)
				g_string_append (attrs, "weight=\"light\"");
			else if (weight == PANGO_WEIGHT_SEMILIGHT)
				g_string_append (attrs, "weight=\"semilight\"");
			else if (weight == PANGO_WEIGHT_BOOK)
				g_string_append (attrs, "weight=\"book\"");
			else if (weight == PANGO_WEIGHT_NORMAL)
				g_string_append (attrs, "weight=\"normal\"");
			else if (weight == PANGO_WEIGHT_MEDIUM)
				g_string_append (attrs, "weight=\"medium\"");
			else if (weight == PANGO_WEIGHT_SEMIBOLD)
				g_string_append (attrs, "weight=\"semibold\"");
			else if (weight == PANGO_WEIGHT_BOLD)
				g_string_append (attrs, "weight=\"bold\"");
			else if (weight == PANGO_WEIGHT_ULTRABOLD)
				g_string_append (attrs, "weight=\"ultrabold\"");
			else if (weight == PANGO_WEIGHT_HEAVY)
				g_string_append (attrs, "weight=\"heavy\"");
			else if (weight == PANGO_WEIGHT_ULTRAHEAVY)
				g_string_append (attrs, "weight=\"ultraheavy\"");
			else
				g_string_append_printf (attrs, "weight=\"%u\"", weight);

			SET_FLAG (flags, FLAGS_WEIGHT);
		}
	}

	/* Handle font style */
	if (!GET_FLAG (flags, FLAGS_STYLE))
	{
		gboolean style_set;

		g_object_get (tag,
		              "style-set", &style_set,
		              NULL);

		if (style_set)
		{
			PangoStyle style;

			g_object_get (tag,
			              "style", &style,
			              NULL);

			if (attrs->len > 0)
				g_string_append_c (attrs, ' ');

			if (style == PANGO_STYLE_NORMAL)
				g_string_append (attrs, "style=\"normal\"");
			else if (style == PANGO_STYLE_OBLIQUE)
				g_string_append (attrs, "style=\"oblique\"");
			else if (style == PANGO_STYLE_ITALIC)
				g_string_append (attrs, "style=\"italic\"");

			SET_FLAG (flags, FLAGS_STYLE);
		}
	}

	/* Handle underline */
	if (!GET_FLAG (flags, FLAGS_UNDERLINE))
	{
		gboolean underline_set;

		g_object_get (tag,
		              "underline-set", &underline_set,
		              NULL);

		if (underline_set)
		{
			PangoUnderline underline;

			g_object_get (tag,
			              "underline", &underline,
			              NULL);

			if (attrs->len > 0)
				g_string_append_c (attrs, ' ');

			if (underline == PANGO_UNDERLINE_NONE)
				g_string_append (attrs, "underline=\"none\"");
			else if (underline == PANGO_UNDERLINE_DOUBLE)
				g_string_append (attrs, "underline=\"double\"");
			else if (underline == PANGO_UNDERLINE_SINGLE)
				g_string_append (attrs, "underline=\"single\"");
			else if (underline == PANGO_UNDERLINE_LOW)
				g_string_append (attrs, "underline=\"low\"");
			else if (underline == PANGO_UNDERLINE_ERROR)
				g_string_append (attrs, "underline=\"error\"");

			SET_FLAG (flags, FLAGS_UNDERLINE);
		}
	}

	/* Handle strikethrough */
	if (!GET_FLAG (flags, FLAGS_STRIKETHROUGH))
	{
		gboolean strikethrough_set;

		g_object_get (tag,
		              "strikethrough-set", &strikethrough_set,
		              NULL);

		if (strikethrough_set)
		{
			gboolean strikethrough;

			g_object_get (tag,
			              "strikethrough", &strikethrough,
			              NULL);

			if (strikethrough)
			{
				if (attrs->len > 0)
					g_string_append_c (attrs, ' ');

				g_string_append (attrs, "strikethrough=\"true\"");

				SET_FLAG (flags, FLAGS_STRIKETHROUGH);
			}
		}
	}

	/* Handle font scale */
	if (!GET_FLAG (flags, FLAGS_SCALE))
	{
		gboolean scale_set;

		g_object_get (tag,
		              "scale-set", &scale_set,
		              NULL);

		if (scale_set)
		{
			double scale;

			g_object_get (tag,
			              "scale", &scale,
			              NULL);

			if (attrs->len > 0)
				g_string_append_c (attrs, ' ');

			g_string_append_printf (attrs, "size=%d%%", (int)(scale * 100));

			SET_FLAG (flags, FLAGS_SCALE);
		}
	}
}

static char *
get_attrs_at_iter (GtkTextIter *iter)
{
	GSList *tags_at_iter = g_slist_reverse (gtk_text_iter_get_tags (iter));
	GString *combined_attrs = g_string_new (NULL);
	guint flags = 0;

	for (const GSList *cur = tags_at_iter; cur; cur = cur->next)
		add_attributes_for_tag (cur->data, combined_attrs, &flags);

	g_slist_free (tags_at_iter);

	return g_string_free (combined_attrs, FALSE);
}

static void
add_styled_segment (GtkTextBuffer *buffer,
                    GtkTextIter   *start,
                    GtkTextIter   *end,
                    const char    *attrs,
                    GString       *result)
{
	char *text = gtk_text_buffer_get_text (buffer, start, end, FALSE);
	char *escaped = g_markup_escape_text (text, -1);

	if (attrs && attrs[0])
		g_string_append_printf (result, "<span %s>%s</span>", attrs, escaped);
	else
		g_string_append (result, escaped);

	g_free (text);
	g_free (escaped);
}

/**
 * gtk_source_buffer_get_markup:
 * @buffer: a #GtkSourceBuffer
 * @start: start of range as a #GtkTextIter
 * @end: end of range as a #GtkTextIter
 *
 * Returns the text in the specified range converting any text formatting
 * to equivalent Pango markup tags.
 * This allows the styled text to be displayed in other widgets that support
 * Pango markup, such as #GtkLabel.
 *
 * For very long ranges this function can take long enough that you could
 * potentially miss frame renderings.
 *
 * Returns: (transfer full): a newly-allocated string containing the text
 *   with Pango markup, or %NULL if @start and @end are invalid.
 *
 * Since: 5.18
 */
char *
gtk_source_buffer_get_markup (GtkSourceBuffer *buffer,
                              GtkTextIter     *start,
                              GtkTextIter     *end)
{
	GtkTextBuffer *text_buffer;
	GtkTextIter current_iter, segment_start;
	char *prev_attrs = NULL;
	GString *result;

	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (start != NULL, NULL);
	g_return_val_if_fail (end != NULL, NULL);

	gtk_source_buffer_ensure_highlight (buffer, start, end);

	text_buffer = GTK_TEXT_BUFFER (buffer);
	result = g_string_new (NULL);

	current_iter = *start;
	segment_start = *start;
	prev_attrs = get_attrs_at_iter (&current_iter);

	while (gtk_text_iter_compare (&current_iter, end) < 0)
	{
		GtkTextIter next_iter = current_iter;
		GtkTextIter scan_iter;
		char *curr_attrs = NULL;

		if (!gtk_text_iter_forward_char (&next_iter))
			break;

		curr_attrs = get_attrs_at_iter (&next_iter);

		if ((prev_attrs && !curr_attrs) ||
		    (!prev_attrs && curr_attrs) ||
		    (prev_attrs && curr_attrs && g_strcmp0 (prev_attrs, curr_attrs) != 0))
		{
			add_styled_segment (text_buffer, &segment_start, &next_iter, prev_attrs, result);

			segment_start = next_iter;

			g_free (prev_attrs);
			prev_attrs = g_steal_pointer (&curr_attrs);
		}

		g_free (curr_attrs);

		/* Scan forward until there is a tag toggled on/off */
		scan_iter = next_iter;
		while (gtk_text_iter_forward_char (&scan_iter))
		{
			GSList *tags;

			if (!(tags = gtk_text_iter_get_toggled_tags (&scan_iter, TRUE)) &&
			    !(tags = gtk_text_iter_get_toggled_tags (&scan_iter, FALSE)))
			{
				next_iter = scan_iter;
				continue;
			}

			g_slist_free (tags);

			break;
		}

		current_iter = next_iter;
	}

	if (gtk_text_iter_compare (&segment_start, end) < 0)
		add_styled_segment (text_buffer, &segment_start, end, prev_attrs, result);

	g_free (prev_attrs);

	return g_string_free (result, result->len == 0);
}

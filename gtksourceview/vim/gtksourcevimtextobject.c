/*
 * This file is part of GtkSourceView
 *
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gtksourcevimmotion.h"
#include "gtksourcevimtextobject.h"

typedef gboolean (*TextObjectCheck)  (const GtkTextIter *iter);
typedef gboolean (*TextObjectMotion) (GtkTextIter       *iter);
typedef gboolean (*TextObjectExtend) (const GtkTextIter *origin,
                                      GtkTextIter       *inner_begin,
                                      GtkTextIter       *inner_end,
                                      GtkTextIter       *a_begin,
                                      GtkTextIter       *a_end,
                                      guint              mode);

enum {
	TEXT_OBJECT_INNER,
	TEXT_OBJECT_A,
};

struct _GtkSourceVimTextObject
{
	GtkSourceVimState parent_instance;

	TextObjectCheck   ends;
	TextObjectCheck   starts;
	TextObjectMotion  forward_end;
	TextObjectMotion  backward_start;
	TextObjectExtend  extend;

	guint             inner_or_a : 1;
	guint             is_linewise : 1;
};

G_DEFINE_TYPE (GtkSourceVimTextObject, gtk_source_vim_text_object, GTK_SOURCE_TYPE_VIM_STATE)

static gboolean
gtk_source_vim_iter_always_false (const GtkTextIter *iter)
{
	return FALSE;
}

#define DEFINE_ITER_CHECK(name, char)                 \
static gboolean                                       \
gtk_source_vim_iter_##name (const GtkTextIter *iter)  \
{                                                     \
	return gtk_text_iter_get_char (iter) == char; \
}
DEFINE_ITER_CHECK (starts_paren, '(')
DEFINE_ITER_CHECK (ends_paren, ')')
DEFINE_ITER_CHECK (starts_brace, '{')
DEFINE_ITER_CHECK (ends_brace, '}')
DEFINE_ITER_CHECK (starts_bracket, '[')
DEFINE_ITER_CHECK (ends_bracket, ']')
DEFINE_ITER_CHECK (starts_lt_gt, '<')
DEFINE_ITER_CHECK (ends_lt_gt, '>')
#undef DEFINE_ITER_CHECK

static inline gboolean
iter_isspace (const GtkTextIter *iter)
{
	return g_unichar_isspace (gtk_text_iter_get_char (iter));
}

static inline gboolean
is_empty_line (const GtkTextIter *iter)
{
	return gtk_text_iter_starts_line (iter) && gtk_text_iter_ends_line (iter);
}

static inline gboolean
can_trail_sentence (const GtkTextIter *iter)
{
	switch (gtk_text_iter_get_char (iter))
	{
	case '.': case '!': case '?':
	case ']': case ')': case '"': case '\'':
		return TRUE;

	default:
		return FALSE;
	}
}

static inline gboolean
is_end_sentence_char (const GtkTextIter *iter)
{
	switch (gtk_text_iter_get_char (iter))
	{
	case '.': case '!': case '?':
		return TRUE;

	default:
		return FALSE;
	}
}

static gboolean
gtk_source_vim_iter_ends_sentence (const GtkTextIter *iter)
{
	GtkTextIter next, cur;

	if (!can_trail_sentence (iter))
	{
		return FALSE;
	}

	next = *iter;
	if (gtk_text_iter_forward_char (&next) &&
	    !gtk_text_iter_ends_line (&next) &&
	    !iter_isspace (&next))
	{
		return FALSE;
	}

	cur = *iter;
	while (!is_end_sentence_char (&cur) && can_trail_sentence (&cur))
	{
		gtk_text_iter_backward_char (&cur);
	}

	return is_end_sentence_char (&cur);
}

static gboolean
gtk_source_vim_iter__forward_sentence_end (GtkTextIter *iter)
{
	if (gtk_text_iter_is_end (iter) || !gtk_text_iter_forward_char (iter))
	{
		return FALSE;
	}

	do
	{
		if (is_empty_line (iter))
			return TRUE;

		if (is_end_sentence_char (iter))
		{
			GtkTextIter next = *iter;

			while (gtk_text_iter_forward_char (&next))
			{
				if (!can_trail_sentence (&next))
					break;
				*iter = next;
			}

			return TRUE;
		}
	} while (gtk_text_iter_forward_char (iter));

	return FALSE;
}

static gboolean
gtk_source_vim_iter_is_paragraph_break (const GtkTextIter *iter)
{
	return gtk_text_iter_is_end (iter) || is_empty_line (iter);
}

static gboolean
gtk_source_vim_iter__backward_paragraph_start (GtkTextIter *iter)
{
	while (!is_empty_line (iter))
	{
		if (gtk_text_iter_is_start (iter))
			return TRUE;

		gtk_text_iter_backward_line (iter);

		if (is_empty_line (iter))
		{
			gtk_text_iter_forward_char (iter);
			break;
		}
	}

	return TRUE;
}

static gboolean
gtk_source_vim_iter__forward_paragraph_end (GtkTextIter *iter)
{
	gtk_text_iter_forward_char (iter);

	while (!is_empty_line (iter))
	{
		if (gtk_text_iter_is_end (iter))
			return TRUE;

		gtk_text_iter_forward_line (iter);

		if (is_empty_line (iter))
		{
			/* Place at the end of the previous non-empty line */
			gtk_text_iter_backward_char (iter);
			return TRUE;
		}
	}

	return TRUE;
}

static void
backward_to_first_space (GtkTextIter *iter)
{
	while (!gtk_text_iter_starts_line (iter))
	{
		gtk_text_iter_backward_char (iter);

		if (!iter_isspace (iter))
		{
			gtk_text_iter_forward_char (iter);
			return;
		}
	}
}

static void
forward_to_nonspace (GtkTextIter *iter)
{
	while (!gtk_text_iter_ends_line (iter))
	{
		if (!iter_isspace (iter))
		{
			break;
		}

		gtk_text_iter_forward_char (iter);
	}
}

static gboolean
text_object_extend_word (const GtkTextIter *origin,
                         GtkTextIter       *inner_begin,
                         GtkTextIter       *inner_end,
                         GtkTextIter       *a_begin,
                         GtkTextIter       *a_end,
                         guint              mode)
{
	if (!gtk_text_iter_ends_line (inner_end))
	{
		gtk_text_iter_forward_char (inner_end);
	}

	if (gtk_text_iter_compare (origin, inner_begin) < 0)
	{
		*a_begin = *inner_begin;
		*a_end = *inner_end;
		backward_to_first_space (a_begin);
		*inner_end = *inner_begin;
		*inner_begin = *a_begin;
	}
	else
	{
		*a_begin = *inner_begin;
		*a_end = *inner_end;
		forward_to_nonspace (a_end);
	}

	return TRUE;
}

static gboolean
text_object_extend_one (const GtkTextIter *origin,
                        GtkTextIter       *inner_begin,
                        GtkTextIter       *inner_end,
                        GtkTextIter       *a_begin,
                        GtkTextIter       *a_end,
                        guint              mode)
{
	*a_begin = *inner_begin;
	gtk_text_iter_forward_char (inner_begin);

	*a_end = *inner_end;
	gtk_text_iter_forward_char (a_end);

	return TRUE;
}

static gboolean
text_object_extend_paragraph (const GtkTextIter *origin,
                              GtkTextIter       *inner_begin,
                              GtkTextIter       *inner_end,
                              GtkTextIter       *a_begin,
                              GtkTextIter       *a_end,
                              guint              mode)
{
	GtkTextIter next;
	gboolean started_on_empty;

	started_on_empty = is_empty_line (inner_begin);

	if (is_empty_line (a_begin))
	{
		GtkTextIter prev = *a_begin;

		while (gtk_text_iter_backward_line (&prev) ||
		       gtk_text_iter_is_start (&prev))
		{
			if (!is_empty_line (&prev))
			{
				gtk_text_iter_forward_to_line_end (&prev);
				gtk_text_iter_forward_char (&prev);
				*a_begin = prev;
				break;
			}
			else if (gtk_text_iter_is_start (&prev))
			{
				*a_begin = prev;
				break;
			}
		}
	}

	next = *a_end;

	while (gtk_text_iter_forward_line (&next) ||
	       gtk_text_iter_is_end (&next))
	{
		if (!is_empty_line (&next))
			break;

		*a_end = next;

		if (gtk_text_iter_is_end (&next))
			break;
	}

	if (started_on_empty)
	{
		*inner_begin = *a_begin;
		*inner_end = *a_end;

		/* If the original position is empty, then `ap` should
		 * place @a_end at the end of the next found paragraph.
		 */
		next = *a_end;
		gtk_text_iter_forward_line (&next);
		while (!is_empty_line (&next) && !gtk_text_iter_is_end (&next))
			gtk_text_iter_forward_line (&next);
		if (gtk_text_iter_compare (&next, a_end) > 0)
			gtk_text_iter_backward_char (&next);
		*a_end = next;
	}

	/* If we didn't actually advance, then we failed to find
	 * a paragraph and we should fail the extension to match
	 * what VIM does. (Test with `cap` at position 0 w/ "\n\n").
	 */
	if (mode == TEXT_OBJECT_A &&
	    started_on_empty &&
	    gtk_text_iter_equal (a_end, inner_end))
	{
		return FALSE;
	}

	return TRUE;
}

static gboolean
text_object_extend_sentence (const GtkTextIter *origin,
                             GtkTextIter       *inner_begin,
                             GtkTextIter       *inner_end,
                             GtkTextIter       *a_begin,
                             GtkTextIter       *a_end,
                             guint              mode)
{
	if (gtk_text_iter_starts_line (inner_begin) &&
	    gtk_text_iter_ends_line (inner_begin))
	{
		/* swallow up to next none empty line */
		while (is_empty_line (a_end))
			gtk_text_iter_forward_line (a_end);
	}
	else if (!gtk_text_iter_ends_line (inner_end))
	{
		/* swallow trailing character */
		gtk_text_iter_forward_char (inner_end);

		*a_end = *inner_end;

		/* swallow up to next sentence for a */
		while (!gtk_text_iter_ends_line (a_end) && iter_isspace (a_end))
		{
			gtk_text_iter_forward_char (a_end);
		}
	}

	return TRUE;
}

static GtkSourceVimState *
gtk_source_vim_text_object_new (TextObjectCheck  ends,
                                TextObjectCheck  starts,
                                TextObjectMotion forward_end,
                                TextObjectMotion backward_start,
                                TextObjectExtend extend,
                                guint            inner_or_a,
                                gboolean         is_linewise)
{
	GtkSourceVimTextObject *self;

	self = g_object_new (GTK_SOURCE_TYPE_VIM_TEXT_OBJECT, NULL);
	self->ends = ends;
	self->starts = starts;
	self->forward_end = forward_end;
	self->backward_start = backward_start;
	self->extend = extend;
	self->inner_or_a = inner_or_a;
	self->is_linewise = !!is_linewise;

	return GTK_SOURCE_VIM_STATE (self);
}

#define TEXT_OBJECT_CTOR(name, ends, starts, forward, backward, extend, inner_or_a, linewise) \
GtkSourceVimState *                                                                           \
gtk_source_vim_text_object_new_##name (void)                                                  \
{                                                                                             \
	return gtk_source_vim_text_object_new (gtk_source_vim_iter_##ends,                    \
	                                       gtk_source_vim_iter_##starts,                  \
	                                       gtk_source_vim_iter_##forward,                 \
	                                       gtk_source_vim_iter_##backward,                \
					       text_object_extend_##extend,                   \
					       TEXT_OBJECT_##inner_or_a,                      \
					       linewise);                                     \
}

TEXT_OBJECT_CTOR (inner_word, ends_word, starts_word, forward_word_end, backward_word_start, word, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_WORD, always_false, starts_WORD, forward_WORD_end, backward_WORD_start, word, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_sentence, ends_sentence, always_false, _forward_sentence_end, backward_sentence_start, sentence, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_paragraph, is_paragraph_break, is_paragraph_break, _forward_paragraph_end, _backward_paragraph_start, paragraph, INNER, TRUE);
TEXT_OBJECT_CTOR (inner_block_paren, ends_paren, starts_paren, forward_block_paren_end, backward_block_paren_start, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_block_brace, ends_brace, starts_brace, forward_block_brace_end, backward_block_brace_start, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_block_bracket, ends_bracket, starts_bracket, forward_block_bracket_end, backward_block_bracket_start, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_block_lt_gt, ends_lt_gt, starts_lt_gt, forward_block_lt_gt_end, backward_block_lt_gt_start, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_quote_double, ends_quote_double, always_false, forward_quote_double, backward_quote_double, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_quote_single, ends_quote_single, always_false, forward_quote_single, backward_quote_single, one, INNER, FALSE);
TEXT_OBJECT_CTOR (inner_quote_grave, ends_quote_grave, always_false, forward_quote_grave, backward_quote_grave, one, INNER, FALSE);

TEXT_OBJECT_CTOR (a_word, ends_word, starts_word, forward_word_end, backward_word_start, word, A, FALSE);
TEXT_OBJECT_CTOR (a_WORD, ends_WORD, starts_WORD, forward_WORD_end, backward_WORD_start, word, A, FALSE);
TEXT_OBJECT_CTOR (a_sentence, ends_sentence, always_false, _forward_sentence_end, backward_sentence_start, sentence, A, FALSE);
TEXT_OBJECT_CTOR (a_paragraph, is_paragraph_break, is_paragraph_break, _forward_paragraph_end, _backward_paragraph_start, paragraph, A, TRUE);
TEXT_OBJECT_CTOR (a_block_paren, ends_paren, starts_paren, forward_block_paren_end, backward_block_paren_start, one, A, FALSE);
TEXT_OBJECT_CTOR (a_block_brace, ends_brace, starts_brace, forward_block_brace_end, backward_block_brace_start, one, A, FALSE);
TEXT_OBJECT_CTOR (a_block_bracket, ends_bracket, starts_bracket, forward_block_bracket_end, backward_block_bracket_start, one, A, FALSE);
TEXT_OBJECT_CTOR (a_block_lt_gt, ends_lt_gt, starts_lt_gt, forward_block_lt_gt_end, backward_block_lt_gt_start, one, A, FALSE);
TEXT_OBJECT_CTOR (a_quote_double, ends_quote_double, always_false, forward_quote_double, backward_quote_double, one, A, FALSE);
TEXT_OBJECT_CTOR (a_quote_single, ends_quote_single, always_false, forward_quote_single, backward_quote_single, one, A, FALSE);
TEXT_OBJECT_CTOR (a_quote_grave, ends_quote_grave, always_false, forward_quote_grave, backward_quote_grave, one, A, FALSE);

#undef TEXT_OBJECT_CTOR

static void
gtk_source_vim_text_object_dispose (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_vim_text_object_parent_class)->dispose (object);
}

static void
gtk_source_vim_text_object_class_init (GtkSourceVimTextObjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_vim_text_object_dispose;
}

static void
gtk_source_vim_text_object_init (GtkSourceVimTextObject *self)
{
}

gboolean
gtk_source_vim_text_object_select (GtkSourceVimTextObject *self,
                                   GtkTextIter            *begin,
                                   GtkTextIter            *end)
{
	GtkTextIter inner_begin;
	GtkTextIter inner_end;
	GtkTextIter a_begin;
	GtkTextIter a_end;
	int count;

	g_return_val_if_fail (GTK_SOURCE_IS_VIM_TEXT_OBJECT (self), FALSE);
	g_return_val_if_fail (begin != NULL, FALSE);
	g_return_val_if_fail (end != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TEXT_BUFFER (gtk_text_iter_get_buffer (begin)), FALSE);
	g_return_val_if_fail (self->forward_end != NULL, FALSE);
	g_return_val_if_fail (self->backward_start != NULL, FALSE);
	g_return_val_if_fail (self->extend != NULL, FALSE);

	inner_end = *begin;
	if (!self->ends (&inner_end) && !self->forward_end (&inner_end))
		return FALSE;

	inner_begin = inner_end;
	if (!self->starts (&inner_begin) && !self->backward_start (&inner_begin))
		return FALSE;

	count = gtk_source_vim_state_get_count (GTK_SOURCE_VIM_STATE (self));
	for (int i = 1; i < count; i++)
	{
		if (!self->forward_end (&inner_end))
			return FALSE;
	}

	a_begin = inner_begin;
	a_end = inner_end;

	if (!self->extend (begin, &inner_begin, &inner_end, &a_begin, &a_end, self->inner_or_a))
	{
		return FALSE;
	}

	if (self->inner_or_a == TEXT_OBJECT_INNER)
	{
		*begin = inner_begin;
		*end = inner_end;
	}
	else
	{
		*begin = a_begin;
		*end = a_end;
	}

	return TRUE;
}

gboolean
gtk_source_vim_text_object_is_linewise (GtkSourceVimTextObject *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_VIM_TEXT_OBJECT (self), FALSE);

	return self->is_linewise;
}

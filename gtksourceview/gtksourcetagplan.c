/*
 * This file is part of GtkSourceView
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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

#include "gtksourcetagplan-private.h"

/* Build the final membership of each tag before touching GtkTextBuffer. The
 * order of additions matters: a disabled context class in a child overrides
 * an enabled class on its parent. Different tags remain independent; their
 * priorities and properties are not part of this representation.
 */

typedef struct
{
	int start;
	int end;
} Span;

typedef struct
{
	Span span;
	guint enabled : 1;
	guint active : 1;
} Operation;

typedef struct
{
	int offset;
	guint operation;
	gboolean start;
} Event;

typedef struct
{
	GArray   *operations;
	GArray   *current;
	gboolean  only_enable;
	gboolean  clear;
} TagPlan;

typedef struct
{
	GtkTextTag *tag;
	Span span;
	gboolean enabled;
} Change;

struct _GtkSourceTagPlan
{
	GtkTextBuffer *buffer;
	GHashTable *tags;
	int start;
	int end;
};

static void
tag_plan_free (gpointer data)
{
	TagPlan *tag_plan = data;

	g_array_unref (tag_plan->operations);
	g_array_unref (tag_plan->current);
	g_free (tag_plan);
}

GtkSourceTagPlan *
_gtk_source_tag_plan_new (GtkTextBuffer *buffer,
                          int            start,
                          int            end)
{
	GtkSourceTagPlan *plan;

	g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
	g_return_val_if_fail (start >= 0 && end >= start, NULL);

	plan = g_new0 (GtkSourceTagPlan, 1);
	plan->buffer = g_object_ref (buffer);
	plan->start = start;
	plan->end = end;
	plan->tags = g_hash_table_new_full (NULL, NULL, g_object_unref, tag_plan_free);

	return plan;
}

static TagPlan *
ensure_tag (GtkSourceTagPlan *plan,
            GtkTextTag       *tag)
{
	TagPlan *tag_plan = g_hash_table_lookup (plan->tags, tag);

	if (tag_plan == NULL)
	{
		tag_plan = g_new0 (TagPlan, 1);
		tag_plan->operations = g_array_new (FALSE, FALSE, sizeof (Operation));
		tag_plan->current = g_array_new (FALSE, FALSE, sizeof (Span));
		tag_plan->only_enable = TRUE;
		g_hash_table_insert (plan->tags, g_object_ref (tag), tag_plan);
	}

	return tag_plan;
}

/* Reset a managed tag over the plan's range, before its ordered writes. Tags
 * encountered only through add() retain their existing membership elsewhere.
 */
void
_gtk_source_tag_plan_add_tag (GtkSourceTagPlan *plan,
                              GtkTextTag       *tag)
{
	ensure_tag (plan, tag)->clear = TRUE;
}

void
_gtk_source_tag_plan_add (GtkSourceTagPlan *plan,
                          GtkTextTag       *tag,
                          int               start,
                          int               end,
                          gboolean          enabled)
{
	TagPlan *tag_plan;
	Operation operation = { { MAX (start, plan->start), MIN (end, plan->end) }, !!enabled, FALSE };

	if (operation.span.start >= operation.span.end)
		return;

	tag_plan = ensure_tag (plan, tag);
	tag_plan->only_enable &= enabled;

	/* Consecutive writes of the same value can be combined immediately.
	 * In particular, a uniform literal represented by one segment per
	 * character should not need one temporary operation per character.
	 */
	if (tag_plan->operations->len > 0)
	{
		Operation *last = &g_array_index (tag_plan->operations, Operation, tag_plan->operations->len - 1);

		if (last->enabled == operation.enabled &&
		    last->span.end >= operation.span.start &&
		    operation.span.end >= last->span.start)
		{
			last->span.start = MIN (last->span.start, operation.span.start);
			last->span.end = MAX (last->span.end, operation.span.end);
			return;
		}
	}

	g_array_append_val (tag_plan->operations, operation);
}

static void
append_span (GArray *spans,
             int     start,
             int     end)
{
	Span span = { start, end };

	if (start >= end)
		return;

	if (spans->len > 0)
	{
		Span *last = &g_array_index (spans, Span, spans->len - 1);

		if (last->end >= start)
		{
			last->end = MAX (last->end, end);
			return;
		}
	}

	g_array_append_val (spans, span);
}

static int
compare_operation (gconstpointer a,
                   gconstpointer b)
{
	const Operation *op_a = a;
	const Operation *op_b = b;

	return (op_a->span.start > op_b->span.start) - (op_a->span.start < op_b->span.start);
}

static int
compare_event (gconstpointer a,
               gconstpointer b)
{
	const Event *event_a = a;
	const Event *event_b = b;

	return (event_a->offset > event_b->offset) - (event_a->offset < event_b->offset);
}

static void
heap_push (GArray *heap,
           guint   operation)
{
	guint child = heap->len;

	g_array_append_val (heap, operation);

	while (child > 0)
	{
		guint parent = (child - 1) / 2;
		guint value = g_array_index (heap, guint, parent);

		if (value >= operation)
			break;

		g_array_index (heap, guint, child) = value;
		child = parent;
	}

	g_array_index (heap, guint, child) = operation;
}

static void
heap_pop (GArray *heap)
{
	guint replacement = g_array_index (heap, guint, heap->len - 1);
	guint parent = 0;

	g_array_set_size (heap, heap->len - 1);

	if (heap->len == 0)
		return;

	while (parent < heap->len / 2)
	{
		guint child = parent * 2 + 1;

		if (child + 1 < heap->len &&
		    g_array_index (heap, guint, child + 1) > g_array_index (heap, guint, child))
			child++;

		if (g_array_index (heap, guint, child) <= replacement)
			break;

		g_array_index (heap, guint, parent) = g_array_index (heap, guint, child);
		parent = child;
	}

	g_array_index (heap, guint, parent) = replacement;
}

static GArray *
resolve_spans (TagPlan *tag_plan,
               GArray  *current)
{
	GArray *spans = g_array_new (FALSE, FALSE, sizeof (Span));
	GArray *operations = tag_plan->operations;
	GArray *seeded_operations = NULL;
	GArray *events = NULL;
	GArray *heap = NULL;
	guint i;

	/* A named context-class tag may have been supplied by the application.
	 * The engine overlays those tags without clearing their other ranges.
	 * Seed their current membership before replaying the explicit writes.
	 */
	if (!tag_plan->clear && current->len > 0)
	{
		seeded_operations = g_array_sized_new (FALSE, FALSE, sizeof (Operation), current->len + operations->len);

		for (i = 0; i < current->len; i++)
		{
			Operation operation = { g_array_index (current, Span, i), TRUE, FALSE };
			g_array_append_val (seeded_operations, operation);
		}

		g_array_append_vals (seeded_operations, operations->data, operations->len);
		operations = seeded_operations;
	}

	/* Styles, and most context classes, only add membership. This path has
	 * no event or heap allocation, and coalesces adjacent one-token spans.
	 */
	if (tag_plan->only_enable)
	{
		g_array_sort (operations, compare_operation);

		for (i = 0; i < operations->len; i++)
		{
			Operation *operation = &g_array_index (operations, Operation, i);
			append_span (spans, operation->span.start, operation->span.end);
		}

		g_clear_pointer (&seeded_operations, g_array_unref);
		return spans;
	}

	events = g_array_sized_new (FALSE, FALSE, sizeof (Event), operations->len * 2);
	heap = g_array_sized_new (FALSE, FALSE, sizeof (guint), operations->len);

	for (i = 0; i < operations->len; i++)
	{
		Operation *operation = &g_array_index (operations, Operation, i);
		Event start = { operation->span.start, i, TRUE };
		Event end = { operation->span.end, i, FALSE };

		g_array_append_val (events, start);
		g_array_append_val (events, end);
	}

	g_array_sort (events, compare_event);

	for (i = 0; i < events->len;)
	{
		int offset = g_array_index (events, Event, i).offset;

		do
		{
			Event *event = &g_array_index (events, Event, i);
			Operation *operation = &g_array_index (operations, Operation, event->operation);

			operation->active = event->start;
			if (event->start)
				heap_push (heap, event->operation);

			i++;
		}
		while (i < events->len && g_array_index (events, Event, i).offset == offset);

		while (heap->len > 0 &&
		       !g_array_index (operations, Operation, g_array_index (heap, guint, 0)).active)
		{
			heap_pop (heap);
		}

		if (heap->len > 0 && i < events->len &&
		    g_array_index (operations, Operation, g_array_index (heap, guint, 0)).enabled)
		{
			append_span (spans, offset, g_array_index (events, Event, i).offset);
		}
	}

	g_array_unref (heap);
	g_array_unref (events);
	g_clear_pointer (&seeded_operations, g_array_unref);

	return spans;
}

/* Consumes the list of tags, whose elements belong to the buffer. */
static void
collect_tags (GtkSourceTagPlan *plan,
              GSList           *tags,
              int               offset,
              gboolean          enabled)
{
	for (GSList *l = tags; l != NULL; l = l->next)
	{
		TagPlan *tag_plan = g_hash_table_lookup (plan->tags, l->data);

		if (tag_plan == NULL)
			continue;

		/* Leave each span open to the range end until its tag turns off. */
		if (enabled)
		{
			append_span (tag_plan->current, offset, plan->end);
		}
		else
		{
			g_assert (tag_plan->current->len > 0);
			g_array_index (tag_plan->current, Span, tag_plan->current->len - 1).end = offset;
		}
	}

	g_slist_free (tags);
}

static void
collect_spans (GtkSourceTagPlan *plan)
{
	GtkTextIter iter;
	GtkTextIter end;

	gtk_text_buffer_get_iter_at_offset (plan->buffer, &iter, plan->start);
	gtk_text_buffer_get_iter_at_offset (plan->buffer, &end, plan->end);
	collect_tags (plan, gtk_text_iter_get_tags (&iter), plan->start, TRUE);

	/* Share one bounded traversal across all managed tags. */
	while (gtk_text_iter_forward_to_tag_toggle_with_limit (&iter, NULL, &end))
	{
		int offset = gtk_text_iter_get_offset (&iter);

		collect_tags (plan, gtk_text_iter_get_toggled_tags (&iter, FALSE), offset, FALSE);
		collect_tags (plan, gtk_text_iter_get_toggled_tags (&iter, TRUE), offset, TRUE);
	}
}

static void
append_change (GArray     *changes,
               GtkTextTag *tag,
               int         start,
               int         end,
               gboolean    enabled)
{
	Change change = { tag, { start, end }, enabled };

	if (start < end)
		g_array_append_val (changes, change);
}

static void
append_difference (GArray     *changes,
                   GtkTextTag *tag,
                   GArray     *a,
                   GArray     *b,
                   gboolean    enabled)
{
	guint j = 0;

	for (guint i = 0; i < a->len; i++)
	{
		Span span = g_array_index (a, Span, i);
		int start = span.start;

		while (j < b->len && g_array_index (b, Span, j).end <= start)
			j++;

		for (guint k = j; k < b->len; k++)
		{
			Span other = g_array_index (b, Span, k);

			if (other.start >= span.end)
				break;

			append_change (changes, tag, start, MIN (other.start, span.end), enabled);
			start = MAX (start, other.end);
			if (start >= span.end)
				break;
		}

		append_change (changes, tag, start, span.end, enabled);
	}
}

static int
compare_change (gconstpointer a,
                gconstpointer b)
{
	const Change *change_a = a;
	const Change *change_b = b;

	if (change_a->enabled != change_b->enabled)
		return change_a->enabled - change_b->enabled;

	if (change_a->span.start != change_b->span.start)
		return (change_a->span.start > change_b->span.start) - (change_a->span.start < change_b->span.start);

	return (change_a->span.end < change_b->span.end) - (change_a->span.end > change_b->span.end);
}

static void
buffer_changed (GtkTextBuffer *buffer,
                gboolean      *changed)
{
	*changed = TRUE;
}

/* A partial application is idempotent: the next plan collects the current
 * membership again and only commits the differences still outstanding.
 */
gboolean
_gtk_source_tag_plan_apply (GtkSourceTagPlan *plan,
                            gint64            deadline)
{
	GHashTableIter iter;
	GArray *changes;
	gpointer key;
	gpointer value;
	gboolean changed = FALSE;
	gboolean complete = TRUE;
	gulong handler;

	if (plan->start == plan->end || g_hash_table_size (plan->tags) == 0)
		return TRUE;

	collect_spans (plan);
	changes = g_array_new (FALSE, FALSE, sizeof (Change));
	g_hash_table_iter_init (&iter, plan->tags);

	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		GArray *current = ((TagPlan *)value)->current;
		GArray *wanted = resolve_spans (value, current);

		append_difference (changes, key, current, wanted, FALSE);
		append_difference (changes, key, wanted, current, TRUE);

		g_array_unref (wanted);
		g_array_set_size (current, 0);
	}

	/* Commit in buffer order, rather than making one pass through the text
	 * per tag. This also keeps new, adjacent runs together for the B-tree.
	 */
	g_array_sort (changes, compare_change);

	handler = g_signal_connect (plan->buffer, "changed", G_CALLBACK (buffer_changed), &changed);

	for (guint i = 0; i < changes->len; i++)
	{
		Change *change = &g_array_index (changes, Change, i);
		GtkTextIter start;
		GtkTextIter end;

		gtk_text_buffer_get_iter_at_offset (plan->buffer, &start, change->span.start);
		end = start;
		gtk_text_iter_forward_chars (&end, change->span.end - change->span.start);

		if (change->enabled)
			gtk_text_buffer_apply_tag (plan->buffer, change->tag, &start, &end);
		else
			gtk_text_buffer_remove_tag (plan->buffer, change->tag, &start, &end);

		/* A signal handler may have edited the buffer. Never apply another
		 * change using offsets computed before that edit.
		 */
		if (changed)
		{
			complete = FALSE;
			break;
		}

		/* Reconcile unfinished work on the next pass. Checking every four
		 * mutations amortizes clock reads without committing a whole dense
		 * paragraph's worth of tags after the deadline.
		 */
		if (deadline != 0 && (i + 1) % 4 == 0 && i + 1 < changes->len &&
		    g_get_monotonic_time () >= deadline)
		{
			complete = FALSE;
			break;
		}
	}

	g_clear_signal_handler (&handler, plan->buffer);
	g_array_unref (changes);

	return complete;
}

void
_gtk_source_tag_plan_free (GtkSourceTagPlan *plan)
{
	g_clear_object (&plan->buffer);
	g_hash_table_unref (plan->tags);
	g_free (plan);
}

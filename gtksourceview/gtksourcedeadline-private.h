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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * GtkSourceDeadline:
 *
 * A cooperative deadline with amortized monotonic clock observations.
 *
 * Initialize on the stack with GTK_SOURCE_DEADLINE_INIT(). The timestamp is
 * absolute monotonic time in microseconds, as returned by g_get_monotonic_time(),
 * or zero for an unlimited deadline.
 *
 * For uniform processing, gtk_source_deadline_step() admits and counts one unit:
 *
 * |[<!-- language="C" -->
 * GtkSourceDeadline deadline = GTK_SOURCE_DEADLINE_INIT (end_time, 32);
 *
 * while (has_pending_items (self) && gtk_source_deadline_step (&deadline))
 *   process_one (self);
 * ]|
 *
 * For weighted processing, poll at a safe stopping point and count the units
 * processed. Count fixed overhead too, so empty items cannot postpone the next
 * clock observation indefinitely:
 *
 * |[<!-- language="C" -->
 * GtkSourceDeadline deadline = GTK_SOURCE_DEADLINE_INIT (end_time, 1024);
 *
 * while (has_pending_paragraphs (self) && gtk_source_deadline_poll (&deadline))
 *   {
 *     gsize characters = process_paragraph (self);
 *
 *     gtk_source_deadline_count (&deadline, MAX (64, characters));
 *   }
 * ]|
 *
 * The first poll observes the clock. Once expiry is observed, further polls
 * return FALSE. If a caller must make progress even with an expired deadline,
 * it can deliberately process an initial item or group before polling, using
 * a do/while loop. This remains the caller's policy.
 *
 * Share a pointer through helpers that use the same units. Different processing
 * phases can share the absolute timestamp and use separate GtkSourceDeadline
 * instances. Use gtk_source_deadline_check_now() after uncounted processing,
 * callbacks of unknown cost, or a suspension before relying on unused units.
 * Ordinarily, initialize a fresh instance for each synchronous dispatch.
 *
 * Processing can overrun between observations or within one indivisible item.
 * Callers retain responsibility for consistent stopping points, cancellation,
 * continuation state, and scheduling. Finishing without observing expiry does
 * not prove that completion preceded the timestamp.
 *
 * Instances are not thread-safe. Their fields are private and must not be
 * modified directly.
 */
typedef struct
{
	/*< private >*/
	gint64   _end_time;
	guint    _quantum;
	guint    _remaining;
	gboolean _expired;
} GtkSourceDeadline;

/**
 * GTK_SOURCE_DEADLINE_FOREVER:
 *
 * An explicit unlimited deadline. Operations on an instance initialized with
 * this timestamp do not read the clock.
 *
 * Zero means unlimited. Negative timestamps are already expired.
 */
#define GTK_SOURCE_DEADLINE_FOREVER 0

/**
 * GTK_SOURCE_DEADLINE_INIT:
 * @end_time: absolute monotonic timestamp in microseconds, or zero
 *   (GTK_SOURCE_DEADLINE_FOREVER) for an unlimited deadline
 * @quantum: number of processing units between observations, greater than zero
 *
 * Initializes a GtkSourceDeadline without reading the clock. Each argument is
 * evaluated once. The first poll or step observes the deadline before admitting
 * processing; subsequent observations occur after the quantum is counted.
 *
 * For example, step() with a quantum of 32 observes time before units 1, 33,
 * 65, and so on. A quantum of 1 observes time before every unit.
 */
#define GTK_SOURCE_DEADLINE_INIT(end_time, quantum) \
	{ (end_time), (quantum), 0, FALSE }

/**
 * gtk_source_deadline_check_now:
 * @deadline: an initialized GtkSourceDeadline
 *
 * Observes the clock even if uncounted units remain. If the current time is
 * before the timestamp, replenishes the quantum and returns TRUE. Equality
 * counts as expiry. On expiry, clears remaining units and latches the result.
 *
 * Known expiry and GTK_SOURCE_DEADLINE_FOREVER require no clock observation.
 *
 * Returns: TRUE if processing may continue
 */
static inline gboolean
gtk_source_deadline_check_now (GtkSourceDeadline *deadline)
{
	g_assert (deadline->_quantum > 0);

	if (deadline->_expired)
		return FALSE;

	if (deadline->_end_time != GTK_SOURCE_DEADLINE_FOREVER &&
	    g_get_monotonic_time () >= deadline->_end_time)
	{
		deadline->_remaining = 0;
		deadline->_expired = TRUE;
		return FALSE;
	}

	deadline->_remaining = deadline->_quantum;

	return TRUE;
}

/**
 * gtk_source_deadline_poll:
 * @deadline: an initialized GtkSourceDeadline
 *
 * Checks whether processing may continue. Observes the clock only on the first
 * call or when the previously observed quantum has been counted. Does not count
 * any units itself, allowing several helpers to poll while the helper doing the
 * processing calls gtk_source_deadline_count().
 *
 * A TRUE result can rely on an earlier observation. Repeated polling without
 * counting units does not require another observation; use
 * gtk_source_deadline_check_now() when a fresh observation is needed.
 *
 * Returns: TRUE if processing may continue under the sampling policy
 */
static inline gboolean
gtk_source_deadline_poll (GtkSourceDeadline *deadline)
{
	return deadline->_remaining != 0 ||
	       gtk_source_deadline_check_now (deadline);
}

/**
 * gtk_source_deadline_count:
 * @deadline: an initialized GtkSourceDeadline
 * @units: number of processing units to count
 *
 * Counts units toward the next clock observation without reading the clock.
 * Subtraction saturates at zero, including when @units exceeds the quantum.
 * Zero units have no effect. Counting units never changes the timestamp or
 * revives an expired instance.
 *
 * Count on every path that performs processing, including paths that continue
 * a loop early. Units can be counted before processing if already known, or
 * afterwards when their number becomes available.
 */
static inline void
gtk_source_deadline_count (GtkSourceDeadline *deadline,
                           gsize              units)
{
	deadline->_remaining = (units >= deadline->_remaining)
	                     ? 0
	                     : deadline->_remaining - (guint) units;
}

/**
 * gtk_source_deadline_step:
 * @deadline: an initialized GtkSourceDeadline
 *
 * Polls the deadline and, if processing may continue, counts one unit before
 * returning TRUE. Counting before the loop body ensures that an early continue
 * still contributes toward the next observation.
 *
 * Returns: TRUE if another unit may begin under the sampling policy
 */
static inline gboolean
gtk_source_deadline_step (GtkSourceDeadline *deadline)
{
	if (!gtk_source_deadline_poll (deadline))
		return FALSE;

	deadline->_remaining--;

	return TRUE;
}

G_END_DECLS

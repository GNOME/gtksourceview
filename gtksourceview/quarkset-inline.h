/*
 * This file is part of GtkSourceView
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _QuarkSet
{
	gint32 len;
	union {
		GQuark  embed[2];
		GQuark *alloc;
	} u;
} QuarkSet;

static inline gboolean
quark_set_is_embed (QuarkSet *set)
{
	return set->len >= 0;
}

static inline void
quark_set_clear (QuarkSet *set)
{
	if (set->len < 0)
	{
		g_free (set->u.alloc);
	}

	set->len = 0;
	set->u.alloc = NULL;
}

static inline gboolean
quark_set_contains (QuarkSet *set,
                    GQuark    quark)
{
	GQuark *quarks;
	guint i;
	guint len;

	if (set->len == 0)
	{
		return FALSE;
	}

	if (quark_set_is_embed (set))
	{
		quarks = set->u.embed;
		len = set->len;
	}
	else
	{
		quarks = set->u.alloc;
		len = ABS (set->len);
	}

	for (i = 0; i < len; i++)
	{
		if (quarks[i] == quark)
		{
			return TRUE;
		}
	}

	return FALSE;
}

static inline void
quark_set_add (QuarkSet *set,
               GQuark    quark)
{
	if (quark_set_contains (set, quark))
	{
		return;
	}

	if G_LIKELY (set->len == 0 || set->len == 1)
	{
		G_STATIC_ASSERT (G_N_ELEMENTS (set->u.embed) == 2);

		set->u.embed[set->len++] = quark;
	}
	else if (set->len == G_N_ELEMENTS (set->u.embed))
	{
		GQuark *alloc = g_new (GQuark, set->len + 1);
		guint i;

		for (i = 0; i < set->len; i++)
		{
			alloc[i] = set->u.embed[i];
		}

		alloc[set->len] = quark;
		set->len = -(set->len + 1);
		set->u.alloc = alloc;
	}
	else if (set->len < 0)
	{
		guint len = ABS (set->len);

		set->u.alloc = g_realloc_n (set->u.alloc, len + 1, sizeof (GQuark));
		set->u.alloc[len] = quark;
		set->len--; /* = -(len + 1) */
	}
	else
	{
		g_assert_not_reached ();
	}
}

static inline void
quark_set_remove (QuarkSet *set,
                  GQuark    quark)
{
	if (set->len == 0)
	{
		return;
	}
	else if (set->len == -1 && set->u.alloc[0] == quark)
	{
		quark_set_clear (set);
		return;
	}
	else if (set->len > 0)
	{
		G_STATIC_ASSERT (G_N_ELEMENTS (set->u.embed) == 2);

		if (set->u.embed[0] == quark)
		{
			set->u.embed[0] = set->u.embed[1];
			set->len--;
		}
		else if (set->u.embed[1] == quark)
		{
			set->len--;
		}
	}
	else if (set->len < 0)
	{
		guint len = ABS (set->len);
		guint i;

		for (i = 0; i < len; i++)
		{
			if (set->u.alloc[i] == quark)
			{
				if (i + 1 < len)
				{
					set->u.alloc[i] = set->u.alloc[len - 1];
				}

				set->len++; /* = -(len - 1) */

				break;
			}
		}
	}
	else
	{
		g_assert_not_reached ();
	}
}

G_END_DECLS

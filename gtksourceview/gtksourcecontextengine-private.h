/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcecontextengine-private.h
 * This file is part of gtksourceview
 *
 * Copyright (C) 2010 - Jose Aliste <jose.aliste@gmail.com>
 *
 * gtksourceview is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * gtksourceview is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GTK_SOURCE_CONTEXT_ENGINE_PRIVATE_H__
#define __GTK_SOURCE_CONTEXT_ENGINE_PRIVATE_H__

#define SEGMENT_IS_INVALID(s) ((s)->context == NULL)

typedef enum {
	SUB_PATTERN_WHERE_DEFAULT = 0,
	SUB_PATTERN_WHERE_START,
	SUB_PATTERN_WHERE_END
} SubPatternWhere;

typedef struct _SubPatternDefinition SubPatternDefinition;
typedef struct _SubPattern SubPattern;

typedef struct _Segment Segment;
typedef struct _Context Context;

struct _Segment
{
	Segment			*parent;
	Segment			*next;
	Segment			*prev;
	Segment			*children;
	Segment			*last_child;

	/* This is NULL if and only if it's a dummy segment which denotes
	 * inserted or deleted text. */
	Context			*context;

	/* Subpatterns found in this segment. */
	SubPattern		*sub_patterns;

	/* The context is used in the interval [start_at; end_at). */
	gint			 start_at;
	gint			 end_at;

	/* In case of container contexts, start_len/end_len is length in chars
	 * of start/end match. */
	gint			 start_len;
	gint			 end_len;

	/* Whether this segment is a whole good segment, or it's an
	 * an end of bigger one left after erase_segments() call. */
	guint			 is_start : 1;
};

struct _SubPattern
{
	SubPatternDefinition	*definition;
	gint			 start_at;
	gint			 end_at;
	SubPattern		*next;
};

/* Context methods */
gboolean        _gtk_source_context_get_style_inside          (Context *context);

/* ContextEngine methods */
GtkTextTag *    _gtk_source_context_engine_get_context_tag    (GtkSourceContextEngine *ce,
							       Context                *context);
GtkTextTag *    _gtk_source_context_engine_get_subpattern_tag (GtkSourceContextEngine *ce,
							       Context                *context,
							       SubPatternDefinition   *sp_def);

#endif /* __GTK_SOURCE_CONTEXT_ENGINE_PRIVATE_H__ */

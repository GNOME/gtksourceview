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

#include "gtksourceregex.h"

#define SEGMENT_IS_INVALID(s) ((s)->context == NULL)

typedef enum {
	SUB_PATTERN_WHERE_DEFAULT = 0,
	SUB_PATTERN_WHERE_START,
	SUB_PATTERN_WHERE_END
} SubPatternWhere;

typedef struct _ContextPtr ContextPtr;
typedef struct _SubPatternDefinition SubPatternDefinition;
typedef struct _SubPattern SubPattern;
typedef struct _Segment Segment;
typedef struct _ContextDefinition ContextDefinition;
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

struct _SubPatternDefinition
{
#ifdef NEED_DEBUG_ID
	/* We need the id only for debugging. */
	gchar			*id;
#endif
	gchar			*style;
	SubPatternWhere		 where;

	/* List of class definitions */
	GSList                  *context_classes;

	/* index in the ContextDefinition's list */
	guint			 index;

	union
	{
		gint	 	 num;
		gchar		*name;
	} u;
	guint			 is_named : 1;
};

struct _SubPattern
{
	SubPatternDefinition	*definition;
	gint			 start_at;
	gint			 end_at;
	SubPattern		*next;
};

typedef enum {
	CONTEXT_TYPE_SIMPLE = 0,
	CONTEXT_TYPE_CONTAINER
} ContextType;

struct _ContextDefinition
{
	gchar			*id;

	ContextType		 type;
	union
	{
		GtkSourceRegex *match;
		struct {
			GtkSourceRegex *start;
			GtkSourceRegex *end;
		} start_end;
	} u;

	/* Name of the style used for contexts of this type. */
	gchar			*default_style;

	/* This is a list of DefinitionChild pointers. */
	GSList			*children;

	/* Sub patterns (list of SubPatternDefinition pointers.) */
	GSList			*sub_patterns;
	guint			 n_sub_patterns;

	/* List of class definitions */
	GSList			*context_classes;

	/* Union of every regular expression we can find from this
	 * context. */
	GtkSourceRegex		*reg_all;

	guint			 flags : 8;
	guint			 ref_count : 24;
};

struct _Context
{
	/* Definition for the context. */
	ContextDefinition	*definition;

	Context			*parent;
	ContextPtr		*children;

	/* This is the regex returned by regex_resolve() called on
	 * definition->start_end.end. */
	GtkSourceRegex		*end;
	/* The regular expression containing every regular expression that
	 * could be matched in this context. */
	GtkSourceRegex		*reg_all;

	/* Either definition->default_style or child_def->style, not copied. */
	const gchar		*style;
	GtkTextTag		*tag;
	GtkTextTag	       **subpattern_tags;

	/* Cache for generated list of class tags */
	GSList			*context_classes;

	/* Cache for generated list of subpattern class tags */
	GSList                 **subpattern_context_classes;

	guint			 ref_count;
	/* see context_freeze() */
	guint			 frozen : 1;
	/* Do all the ancestors extend their parent? */
	guint			 all_ancestors_extend : 1;
	/* Do not apply styles to children contexts */
	guint			 ignore_children_style : 1;
};

/* Context methods */
gboolean	 _gtk_source_context_get_style_inside		(Context        *context);

#endif /* __GTK_SOURCE_CONTEXT_ENGINE_PRIVATE_H__ */

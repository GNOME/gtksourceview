/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourceindenter.c
 *
 *  Copyright (C) 2009 - Ignacio Casal Quinteiro <icq@gnome.org>
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

#include "gtksourceindenter.h"
#include "gtksourceview.h"
#include "gtksourceview-utils.h"


G_DEFINE_TYPE(GtkSourceIndenter, gtk_source_indenter, G_TYPE_OBJECT)

static const gchar * relocatables[] =
{
	"0{",
	"0}",
	"0)",
	"0#",
	":",
	NULL
};

static const gchar * extra_indent_words[] =
{
	"if",
	"else",
	"while",
	"do",
	"for",
	"switch",
	NULL
};

/* Default implementation */
static const gchar * const *
gtk_source_indenter_get_relocatables_default (GtkSourceIndenter *self)
{
	return relocatables;
}

/* Default implementation */
static const gchar * const *
gtk_source_indenter_get_extra_indent_words_default (GtkSourceIndenter *self)
{
	return extra_indent_words;
}

/* Default implementation */
static gfloat
gtk_source_indenter_get_indentation_level_default (GtkSourceIndenter *self,
						   GtkTextView       *view,
						   GtkTextIter       *iter,
						   gboolean           relocating)
{
	g_return_val_if_reached (0);
}

static void
gtk_source_indenter_finalize (GObject *object)
{
	/* Empty */
}

static void 
gtk_source_indenter_class_init (GtkSourceIndenterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	klass->get_relocatables = gtk_source_indenter_get_relocatables_default;
	klass->get_extra_indent_words = gtk_source_indenter_get_extra_indent_words_default;
	klass->get_indentation_level = gtk_source_indenter_get_indentation_level_default;

	object_class->finalize = gtk_source_indenter_finalize;
}

static void 
gtk_source_indenter_init (GtkSourceIndenter *self)
{
	/* Empty */
}

/**
 * gtk_source_indenter_get_indentation_level:
 * @self: a #GtkSourceIndenter
 * @view: a #GtkTextView
 * @iter: a #GtkTextIter
 *
 * Gets the indentation level for a line determined by @iter.
 * This func is expensive so it must be called only when it is neccessary,
 * for example when is tiped a trigger, or when the user moves to a determined
 * line.
 * Always this func is called the indenter is going to search for a context
 * to know the indentation level.
 *
 * Returns: the indentation level for a line determined by @iter.
 */
gfloat
gtk_source_indenter_get_indentation_level (GtkSourceIndenter *self,
					   GtkTextView       *view,
					   GtkTextIter       *iter,
					   gboolean           relocating)
{
	g_return_val_if_fail (GTK_IS_SOURCE_INDENTER (self), 0);
	
	return GTK_SOURCE_INDENTER_GET_CLASS (self)->get_indentation_level (self, view, iter, relocating);
}

/**
 * gtk_source_indenter_get_extra_indent_words:
 * @self: a #GtkSourceIndenter
 *
 * Gets the words that start an extra indent in the next line.
 * This will not change the indentation level.
 *
 * Returns: the words that start an extra indent in the next line.
 */
const gchar * const *
gtk_source_indenter_get_extra_indent_words (GtkSourceIndenter *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_INDENTER (self), NULL);
	
	return GTK_SOURCE_INDENTER_GET_CLASS (self)->get_extra_indent_words (self);
}

/**
 * gtk_source_indenter_get_relocatables:
 * @self: a #GtkSourceIndenter
 *
 * Gets the words that should be relocated on indentation. For example: {,}...
 * This will trigger a gtk_source_indenter_get_indentation_level() to know
 * where must be relocated.
 * This is going to be detected only when a extra indent is added,
 * see gtk_source_indenter_get_extra_indent_words(), and in case you add a \t
 * or a \n the detection is finished.
 *
 * Returns: the words that should be relocated on indentation.
 */
const gchar * const *
gtk_source_indenter_get_relocatables (GtkSourceIndenter *self)
{
	g_return_val_if_fail (GTK_IS_SOURCE_INDENTER (self), NULL);
	
	return GTK_SOURCE_INDENTER_GET_CLASS (self)->get_relocatables (self);
}

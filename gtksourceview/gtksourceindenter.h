/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourceindenter.h
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

#ifndef __GTK_SOURCE_INDENTER_H__
#define __GTK_SOURCE_INDENTER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define GTK_TYPE_SOURCE_INDENTER              (gtk_source_indenter_get_type())
#define GTK_SOURCE_INDENTER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SOURCE_INDENTER, GtkSourceIndenter))
#define GTK_SOURCE_INDENTER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SOURCE_INDENTER, GtkSourceIndenterClass))
#define GTK_IS_SOURCE_INDENTER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SOURCE_INDENTER))
#define GTK_IS_SOURCE_INDENTER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_INDENTER))
#define GTK_SOURCE_INDENTER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SOURCE_INDENTER, GtkSourceIndenterClass))

typedef struct _GtkSourceIndenter GtkSourceIndenter;

struct _GtkSourceIndenter
{
	GObject parent;
};

/*
 * Class definition
 */
typedef struct _GtkSourceIndenterClass GtkSourceIndenterClass;

struct _GtkSourceIndenterClass
{
	GObjectClass parent_class;
	
	const gchar * const * (*get_relocatables)         (GtkSourceIndenter *self);
	
	const gchar * const * (*get_extra_indent_words)   (GtkSourceIndenter *self);
	
	gfloat		      (*get_indentation_level)    (GtkSourceIndenter *self,
							   GtkTextView       *view,
							   GtkTextIter       *iter,
							   gboolean           relocating);
};

GType		 gtk_source_indenter_get_type		(void);

const gchar * const *
		 gtk_source_indenter_get_relocatables	(GtkSourceIndenter *self);

const gchar * const *
		 gtk_source_indenter_get_extra_indent_words
		 					(GtkSourceIndenter *self);

gfloat		 gtk_source_indenter_get_indentation_level
							(GtkSourceIndenter *self,
							 GtkTextView       *buf,
							 GtkTextIter       *iter,
							 gboolean           relocating);

gfloat		 gtk_source_indenter_get_amount_indents (GtkTextView *view,
							 GtkTextIter *cur);

gfloat		 gtk_source_indenter_get_amount_indents_from_position
							(GtkTextView *view,
							 GtkTextIter *cur);

G_END_DECLS

#endif


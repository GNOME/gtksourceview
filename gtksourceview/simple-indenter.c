/*
 * c-indenter.c
 * This file is part of gedit
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "simple-indenter.h"
#include "gtksourceindenter.h"
#include "gtksourceindenter-utils.h"

#define SIMPLE_INDENTER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object),\
					    SIMPLE_TYPE_INDENTER, SimpleIndenterPrivate))

struct _SimpleIndenterPrivate
{
};

G_DEFINE_TYPE (SimpleIndenter, simple_indenter, GTK_TYPE_SOURCE_INDENTER)

static gint
simple_indenter_get_indentation_level (GtkSourceIndenter *indenter,
				       GtkTextView *view,
				       GtkTextIter *iter,
				       gboolean relocating)
{
	return gtk_source_indenter_get_amount_indents (view, iter);
}

static const gchar * const *
simple_indenter_get_relocatables (GtkSourceIndenter *self)
{
	return NULL;
}

static void
simple_indenter_finalize (GObject *object)
{
	G_OBJECT_CLASS (simple_indenter_parent_class)->finalize (object);
}

static void
simple_indenter_class_init (SimpleIndenterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceIndenterClass *indenter_class = GTK_SOURCE_INDENTER_CLASS (klass);
	
	object_class->finalize = simple_indenter_finalize;
	
	indenter_class->get_indentation_level = simple_indenter_get_indentation_level;
	indenter_class->get_relocatables = simple_indenter_get_relocatables;

	//g_type_class_add_private (object_class, sizeof (SimpleIndenterPrivate));
}

static void
simple_indenter_init (SimpleIndenter *self)
{
	//self->priv = SIMPLE_INDENTER_GET_PRIVATE (self);
}

SimpleIndenter *
simple_indenter_new ()
{
	return g_object_new (SIMPLE_TYPE_INDENTER, NULL);
}

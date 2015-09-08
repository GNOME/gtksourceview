/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourcetag.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2015 - Université Catholique de Louvain
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Sébastien Wilmet
 */

#include "gtksourcetag.h"

/**
 * SECTION:tag
 * @Short_description: A tag that can be applied to text in a GtkSourceBuffer
 * @Title: GtkSourceTag
 * @See_also: #GtkSourceBuffer
 *
 * #GtkSourceTag is a subclass of #GtkTextTag that adds properties useful for
 * the GtkSourceView library.
 */

typedef struct _GtkSourceTagPrivate GtkSourceTagPrivate;

struct _GtkSourceTagPrivate
{
	gint something;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceTag, gtk_source_tag, GTK_TYPE_TEXT_TAG)

static void
gtk_source_tag_class_init (GtkSourceTagClass *klass)
{
	/*GObjectClass *object_class = G_OBJECT_CLASS (klass);*/
}

static void
gtk_source_tag_init (GtkSourceTag *tag)
{
}

/**
 * gtk_source_tag_new:
 * @name: (nullable): tag name, or %NULL.
 *
 * Creates a #GtkSourceTag. Configure the tag using object arguments,
 * i.e. using g_object_set().
 *
 * For usual cases, gtk_source_buffer_create_tag() is more convenient to use.
 *
 * Returns: a new #GtkSourceTag.
 */
GtkTextTag *
gtk_source_tag_new (const gchar *name)
{
	return g_object_new (GTK_SOURCE_TYPE_TAG,
			     "name", name,
			     NULL);
}

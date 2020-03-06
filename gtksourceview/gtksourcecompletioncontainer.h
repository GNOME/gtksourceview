/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2013 - Sébastien Wilmet <swilmet@gnome.org>
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

#include <gtk/gtk.h>
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_CONTAINER             (_gtk_source_completion_container_get_type ())
#define GTK_SOURCE_COMPLETION_CONTAINER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_SOURCE_TYPE_COMPLETION_CONTAINER, GtkSourceCompletionContainer))
#define GTK_SOURCE_COMPLETION_CONTAINER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_SOURCE_TYPE_COMPLETION_CONTAINER, GtkSourceCompletionContainerClass)
#define GTK_SOURCE_IS_COMPLETION_CONTAINER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_SOURCE_TYPE_COMPLETION_CONTAINER))
#define GTK_SOURCE_IS_COMPLETION_CONTAINER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_SOURCE_TYPE_COMPLETION_CONTAINER))
#define GTK_SOURCE_COMPLETION_CONTAINER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_SOURCE_TYPE_COMPLETION_CONTAINER, GtkSourceCompletionContainerClass))

typedef struct _GtkSourceCompletionContainerClass	GtkSourceCompletionContainerClass;

struct _GtkSourceCompletionContainer
{
	GtkScrolledWindow parent;
};

struct _GtkSourceCompletionContainerClass
{
	GtkScrolledWindowClass parent_class;
};

G_GNUC_INTERNAL
GType		 _gtk_source_completion_container_get_type		(void) G_GNUC_CONST;

G_GNUC_INTERNAL
GtkSourceCompletionContainer *
		 _gtk_source_completion_container_new			(void);

G_END_DECLS

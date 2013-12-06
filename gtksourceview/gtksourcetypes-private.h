/*
 * gtksourcetypes-private.h
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2012 - Sébastien Wilmet <swilmet@gnome.org>
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
 */

#ifndef __GTK_SOURCE_TYPES_PRIVATE_H__
#define __GTK_SOURCE_TYPES_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GtkSourceBufferInputStream	GtkSourceBufferInputStream;
typedef struct _GtkSourceBufferOutputStream	GtkSourceBufferOutputStream;
typedef struct _GtkSourceCompletionContainer	GtkSourceCompletionContainer;
typedef struct _GtkSourceCompletionModel	GtkSourceCompletionModel;
typedef struct _GtkSourceContextEngine		GtkSourceContextEngine;
typedef struct _GtkSourceEngine			GtkSourceEngine;
typedef struct _GtkSourceGutterRendererLines	GtkSourceGutterRendererLines;
typedef struct _GtkSourceGutterRendererMarks	GtkSourceGutterRendererMarks;
typedef struct _GtkSourceMarksSequence		GtkSourceMarksSequence;
typedef struct _GtkSourcePixbufHelper		GtkSourcePixbufHelper;
typedef struct _GtkSourceRegex			GtkSourceRegex;
typedef struct _GtkSourceUndoManagerDefault	GtkSourceUndoManagerDefault;

G_END_DECLS

#endif /* __GTK_SOURCE_TYPES_PRIVATE_H__ */

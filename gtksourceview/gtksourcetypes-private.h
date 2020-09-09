/*
 * This file is part of GtkSourceView
 *
 * Copyright 2012, 2013, 2016 - Sébastien Wilmet <swilmet@gnome.org>
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

typedef struct _GtkSourceAssistant              GtkSourceAssistant;
typedef struct _GtkSourceAssistantChild         GtkSourceAssistantChild;
typedef struct _GtkSourceBufferInputStream      GtkSourceBufferInputStream;
typedef struct _GtkSourceBufferOutputStream     GtkSourceBufferOutputStream;
typedef struct _GtkSourceCompletionInfo         GtkSourceCompletionInfo;
typedef struct _GtkSourceCompletionList         GtkSourceCompletionList;
typedef struct _GtkSourceCompletionListBox      GtkSourceCompletionListBox;
typedef struct _GtkSourceCompletionListBoxRow   GtkSourceCompletionListBoxRow;
typedef struct _GtkSourceContextEngine          GtkSourceContextEngine;
typedef struct _GtkSourceEngine                 GtkSourceEngine;
typedef struct _GtkSourceGutterRendererLines    GtkSourceGutterRendererLines;
typedef struct _GtkSourceGutterRendererMarks    GtkSourceGutterRendererMarks;
typedef struct _GtkSourceMarksSequence          GtkSourceMarksSequence;
typedef struct _GtkSourcePixbufHelper           GtkSourcePixbufHelper;
typedef struct _GtkSourceRegex                  GtkSourceRegex;
typedef struct _GtkSourceSnippetBundle          GtkSourceSnippetBundle;

#ifdef _MSC_VER
/* For Visual Studio, we need to export the symbols used by the unit tests */
# define GTK_SOURCE_INTERNAL __declspec(dllexport)
#else
# define GTK_SOURCE_INTERNAL G_GNUC_INTERNAL
#endif

G_END_DECLS

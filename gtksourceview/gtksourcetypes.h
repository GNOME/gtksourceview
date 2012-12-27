/*
 * gtksourcetypes.h
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

#ifndef __GTK_SOURCE_TYPES_H__
#define __GTK_SOURCE_TYPES_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GtkSourceBuffer			GtkSourceBuffer;
typedef struct _GtkSourceCompletionContext	GtkSourceCompletionContext;
typedef struct _GtkSourceCompletion		GtkSourceCompletion;
typedef struct _GtkSourceCompletionInfo		GtkSourceCompletionInfo;
typedef struct _GtkSourceCompletionItem		GtkSourceCompletionItem;
typedef struct _GtkSourceCompletionProposal	GtkSourceCompletionProposal;
typedef struct _GtkSourceCompletionProvider	GtkSourceCompletionProvider;
typedef struct _GtkSourceGutter			GtkSourceGutter;
typedef struct _GtkSourceGutterRenderer		GtkSourceGutterRenderer;
typedef struct _GtkSourceGutterRendererPixbuf	GtkSourceGutterRendererPixbuf;
typedef struct _GtkSourceGutterRendererText	GtkSourceGutterRendererText;
typedef struct _GtkSourceLanguage		GtkSourceLanguage;
typedef struct _GtkSourceLanguageManager	GtkSourceLanguageManager;
typedef struct _GtkSourceMarkAttributes		GtkSourceMarkAttributes;
typedef struct _GtkSourceMark			GtkSourceMark;
typedef struct _GtkSourcePrintCompositor	GtkSourcePrintCompositor;
typedef struct _GtkSourceStyle			GtkSourceStyle;
typedef struct _GtkSourceStyleScheme		GtkSourceStyleScheme;
typedef struct _GtkSourceStyleSchemeManager	GtkSourceStyleSchemeManager;
typedef struct _GtkSourceUndoManager		GtkSourceUndoManager;
typedef struct _GtkSourceView			GtkSourceView;

G_END_DECLS

#endif /* __GTK_SOURCE_TYPES_H__ */

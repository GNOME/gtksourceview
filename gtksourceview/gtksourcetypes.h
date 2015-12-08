/*
 * gtksourcetypes.h
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2012-2014 - Sébastien Wilmet <swilmet@gnome.org>
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

#include <gio/gio.h>
#include <gtksourceview/gtksourceversion.h>

G_BEGIN_DECLS

typedef struct _GtkSourceBuffer			GtkSourceBuffer;
typedef struct _GtkSourceCompletionContext	GtkSourceCompletionContext;
typedef struct _GtkSourceCompletion		GtkSourceCompletion;
typedef struct _GtkSourceCompletionInfo		GtkSourceCompletionInfo;
typedef struct _GtkSourceCompletionItem		GtkSourceCompletionItem;
typedef struct _GtkSourceCompletionProposal	GtkSourceCompletionProposal;
typedef struct _GtkSourceCompletionProvider	GtkSourceCompletionProvider;
typedef struct _GtkSourceEncoding		GtkSourceEncoding;
typedef struct _GtkSourceFile			GtkSourceFile;
typedef struct _GtkSourceFileLoader		GtkSourceFileLoader;
typedef struct _GtkSourceFileSaver		GtkSourceFileSaver;
typedef struct _GtkSourceGutter			GtkSourceGutter;
typedef struct _GtkSourceGutterRenderer		GtkSourceGutterRenderer;
typedef struct _GtkSourceGutterRendererPixbuf	GtkSourceGutterRendererPixbuf;
typedef struct _GtkSourceGutterRendererText	GtkSourceGutterRendererText;
typedef struct _GtkSourceLanguage		GtkSourceLanguage;
typedef struct _GtkSourceLanguageManager	GtkSourceLanguageManager;
typedef struct _GtkSourceMarkAttributes		GtkSourceMarkAttributes;
typedef struct _GtkSourceMark			GtkSourceMark;
typedef struct _GtkSourcePrintCompositor	GtkSourcePrintCompositor;
typedef struct _GtkSourceSearchContext		GtkSourceSearchContext;
typedef struct _GtkSourceSearchSettings		GtkSourceSearchSettings;
typedef struct _GtkSourceStyle			GtkSourceStyle;
typedef struct _GtkSourceStyleScheme		GtkSourceStyleScheme;
typedef struct _GtkSourceStyleSchemeManager	GtkSourceStyleSchemeManager;
typedef struct _GtkSourceTag			GtkSourceTag;
typedef struct _GtkSourceUndoManager		GtkSourceUndoManager;
typedef struct _GtkSourceView			GtkSourceView;

/**
 * GtkSourceNewlineType:
 * @GTK_SOURCE_NEWLINE_TYPE_LF: line feed, used on UNIX.
 * @GTK_SOURCE_NEWLINE_TYPE_CR: carriage return, used on Mac.
 * @GTK_SOURCE_NEWLINE_TYPE_CR_LF: carriage return followed by a line feed, used
 *   on Windows.
 *
 * Since: 3.14
 */
typedef enum
{
	GTK_SOURCE_NEWLINE_TYPE_LF,
	GTK_SOURCE_NEWLINE_TYPE_CR,
	GTK_SOURCE_NEWLINE_TYPE_CR_LF
} GtkSourceNewlineType;

/**
 * GTK_SOURCE_NEWLINE_TYPE_DEFAULT:
 *
 * The default newline type on the current OS.
 *
 * Since: 3.14
 */
#ifdef G_OS_WIN32
#define GTK_SOURCE_NEWLINE_TYPE_DEFAULT GTK_SOURCE_NEWLINE_TYPE_CR_LF
#else
#define GTK_SOURCE_NEWLINE_TYPE_DEFAULT GTK_SOURCE_NEWLINE_TYPE_LF
#endif

/**
 * GtkSourceCompressionType:
 * @GTK_SOURCE_COMPRESSION_TYPE_NONE: plain text.
 * @GTK_SOURCE_COMPRESSION_TYPE_GZIP: gzip compression.
 *
 * Since: 3.14
 */
typedef enum
{
	GTK_SOURCE_COMPRESSION_TYPE_NONE,
	GTK_SOURCE_COMPRESSION_TYPE_GZIP
} GtkSourceCompressionType;

/**
 * GtkSourceBackgroundPatternType:
 * @GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE: no pattern
 * @GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID: grid pattern
 *
 * Since: 3.16
 */
typedef enum
{
	GTK_SOURCE_BACKGROUND_PATTERN_TYPE_NONE,
	GTK_SOURCE_BACKGROUND_PATTERN_TYPE_GRID
} GtkSourceBackgroundPatternType;

G_END_DECLS

#endif /* __GTK_SOURCE_TYPES_H__ */

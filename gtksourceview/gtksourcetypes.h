/*
 * This file is part of GtkSourceView
 *
 * Copyright 2012-2016 - Sébastien Wilmet <swilmet@gnome.org>
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

#if !defined (GTK_SOURCE_H_INSIDE) && !defined (GTK_SOURCE_COMPILATION)
#error "Only <gtksourceview/gtksource.h> can be included directly."
#endif

#include <glib.h>

#include "gtksourceversion.h"

G_BEGIN_DECLS

/* This header exists to avoid cycles in header inclusions, when header A needs
 * the type B and header B needs the type A. For an alternative way to solve
 * this problem (in C11), see:
 * https://bugzilla.gnome.org/show_bug.cgi?id=679424#c20
 */

typedef struct _GtkSourceAnnotation                GtkSourceAnnotation;
typedef struct _GtkSourceAnnotations               GtkSourceAnnotations;
typedef struct _GtkSourceAnnotationProvider        GtkSourceAnnotationProvider;
typedef struct _GtkSourceBuffer                    GtkSourceBuffer;
typedef struct _GtkSourceCompletion                GtkSourceCompletion;
typedef struct _GtkSourceCompletionCell            GtkSourceCompletionCell;
typedef struct _GtkSourceCompletionContext         GtkSourceCompletionContext;
typedef struct _GtkSourceCompletionProposal        GtkSourceCompletionProposal;
typedef struct _GtkSourceCompletionProvider        GtkSourceCompletionProvider;
typedef struct _GtkSourceEncoding                  GtkSourceEncoding;
typedef struct _GtkSourceFile                      GtkSourceFile;
typedef struct _GtkSourceFileLoader                GtkSourceFileLoader;
typedef struct _GtkSourceFileSaver                 GtkSourceFileSaver;
typedef struct _GtkSourceGutter                    GtkSourceGutter;
typedef struct _GtkSourceGutterLines               GtkSourceGutterLines;
typedef struct _GtkSourceGutterRenderer            GtkSourceGutterRenderer;
typedef struct _GtkSourceGutterRendererPixbuf      GtkSourceGutterRendererPixbuf;
typedef struct _GtkSourceGutterRendererText        GtkSourceGutterRendererText;
typedef struct _GtkSourceHover                     GtkSourceHover;
typedef struct _GtkSourceHoverContext              GtkSourceHoverContext;
typedef struct _GtkSourceHoverDisplay              GtkSourceHoverDisplay;
typedef struct _GtkSourceHoverProvider             GtkSourceHoverProvider;
typedef struct _GtkSourceVimIMContext              GtkSourceVimIMContext;
typedef struct _GtkSourceIndenter                  GtkSourceIndenter;
typedef struct _GtkSourceLanguage                  GtkSourceLanguage;
typedef struct _GtkSourceLanguageManager           GtkSourceLanguageManager;
typedef struct _GtkSourceMap                       GtkSourceMap;
typedef struct _GtkSourceMarkAttributes            GtkSourceMarkAttributes;
typedef struct _GtkSourceMark                      GtkSourceMark;
typedef struct _GtkSourcePrintCompositor           GtkSourcePrintCompositor;
typedef struct _GtkSourceSearchContext             GtkSourceSearchContext;
typedef struct _GtkSourceSearchSettings            GtkSourceSearchSettings;
typedef struct _GtkSourceSnippet                   GtkSourceSnippet;
typedef struct _GtkSourceSnippetChunk              GtkSourceSnippetChunk;
typedef struct _GtkSourceSnippetContext            GtkSourceSnippetContext;
typedef struct _GtkSourceSnippetManager            GtkSourceSnippetManager;
typedef struct _GtkSourceSpaceDrawer               GtkSourceSpaceDrawer;
typedef struct _GtkSourceStyle                     GtkSourceStyle;
typedef struct _GtkSourceStyleSchemeChooserButton  GtkSourceStyleSchemeChooserButton;
typedef struct _GtkSourceStyleSchemeChooser        GtkSourceStyleSchemeChooser;
typedef struct _GtkSourceStyleSchemeChooserWidget  GtkSourceStyleSchemeChooserWidget;
typedef struct _GtkSourceStyleScheme               GtkSourceStyleScheme;
typedef struct _GtkSourceStyleSchemeManager        GtkSourceStyleSchemeManager;
typedef struct _GtkSourceStyleSchemePreview        GtkSourceStyleSchemePreview;
typedef struct _GtkSourceView                      GtkSourceView;

G_END_DECLS

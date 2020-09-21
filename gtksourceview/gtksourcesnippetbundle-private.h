/*
 * This file is part of GtkSourceView
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "gtksourcetypes.h"
#include "gtksourcetypes-private.h"

G_BEGIN_DECLS

typedef struct
{
	int          identifier;
	const gchar *group;
	const gchar *name;
	const gchar *trigger;
	const gchar *language;
	const gchar *description;
	const gchar *text;
} GtkSourceSnippetInfo;

#define GTK_SOURCE_TYPE_SNIPPET_BUNDLE (_gtk_source_snippet_bundle_get_type())

G_DECLARE_FINAL_TYPE (GtkSourceSnippetBundle, _gtk_source_snippet_bundle, GTK_SOURCE, SNIPPET_BUNDLE, GObject)

GtkSourceSnippetBundle  *_gtk_source_snippet_bundle_new           (void);
GtkSourceSnippetBundle  *_gtk_source_snippet_bundle_new_from_file (const gchar              *path,
                                                                   GtkSourceSnippetManager  *manager);
void                     _gtk_source_snippet_bundle_merge         (GtkSourceSnippetBundle   *self,
                                                                   GtkSourceSnippetBundle   *other);
const gchar            **_gtk_source_snippet_bundle_list_groups   (GtkSourceSnippetBundle   *self);
GtkSourceSnippet        *_gtk_source_snippet_bundle_get_snippet   (GtkSourceSnippetBundle   *self,
                                                                   const gchar              *group,
                                                                   const gchar              *language_id,
                                                                   const gchar              *trigger);
GListModel              *_gtk_source_snippet_bundle_list_matching (GtkSourceSnippetBundle   *self,
                                                                   const gchar              *group,
                                                                   const gchar              *language_id,
                                                                   const gchar              *trigger_prefix);
GPtrArray               *_gtk_source_snippet_bundle_parse_text    (const gchar              *text,
                                                                   GError                  **error);

G_END_DECLS

/*
 * This file is part of GtkSourceView
 *
 * Copyright 2009 - Jesse van den Kieboom <jessevdk@gnome.org>
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

#include <gtk/gtk.h>

#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_CONTEXT (gtk_source_completion_context_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceCompletionContext, gtk_source_completion_context, GTK_SOURCE, COMPLETION_CONTEXT, GInitiallyUnowned)

/**
 * GtkSourceCompletionActivation:
 * @GTK_SOURCE_COMPLETION_ACTIVATION_NONE: None.
 * @GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE: Interactive activation. By
 * default, it occurs on each insertion in the #GtkTextBuffer. This can be
 * blocked temporarily with gtk_source_completion_block_interactive().
 * @GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED: User requested activation.
 * By default, it occurs when the user presses
 * <keycombo><keycap>Control</keycap><keycap>space</keycap></keycombo>.
 */
typedef enum _GtkSourceCompletionActivation
{
	GTK_SOURCE_COMPLETION_ACTIVATION_NONE           = 0,
	GTK_SOURCE_COMPLETION_ACTIVATION_INTERACTIVE    = 1 << 0,
	GTK_SOURCE_COMPLETION_ACTIVATION_USER_REQUESTED = 1 << 1
} GtkSourceCompletionActivation;

GTK_SOURCE_AVAILABLE_IN_ALL
void                          gtk_source_completion_context_add_proposals  (GtkSourceCompletionContext  *context,
                                                                            GtkSourceCompletionProvider *provider,
                                                                            GList                       *proposals,
                                                                            gboolean                     finished);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                      gtk_source_completion_context_get_iter       (GtkSourceCompletionContext  *context,
                                                                            GtkTextIter                 *iter);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionActivation gtk_source_completion_context_get_activation (GtkSourceCompletionContext  *context);

G_END_DECLS

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *
 * This file is part of GtkSourceView
 *
 * Copyright 2007 - 2009 Jesús Barbero Rodríguez <chuchiperriman@gmail.com>
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

/*
 * Type checking and casting macros
 */
#define GTK_SOURCE_TYPE_COMPLETION (gtk_source_completion_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkSourceCompletion, gtk_source_completion, GTK_SOURCE, COMPLETION, GObject)

/**
 * GTK_SOURCE_COMPLETION_ERROR:
 *
 * Error domain for the completion. Errors in this domain will be from the
 * #GtkSourceCompletionError enumeration. See #GError for more information on
 * error domains.
 */
#define GTK_SOURCE_COMPLETION_ERROR (gtk_source_completion_error_quark())

/**
 * GtkSourceCompletionError:
 * @GTK_SOURCE_COMPLETION_ERROR_ALREADY_BOUND: The #GtkSourceCompletionProvider
 * is already bound to the #GtkSourceCompletion object.
 * @GTK_SOURCE_COMPLETION_ERROR_NOT_BOUND: The #GtkSourceCompletionProvider is
 * not bound to the #GtkSourceCompletion object.
 *
 * An error code used with %GTK_SOURCE_COMPLETION_ERROR in a #GError returned
 * from a completion-related function.
 */
typedef enum _GtkSourceCompletionError
{
	GTK_SOURCE_COMPLETION_ERROR_ALREADY_BOUND = 0,
	GTK_SOURCE_COMPLETION_ERROR_NOT_BOUND
} GtkSourceCompletionError;

GTK_SOURCE_AVAILABLE_IN_ALL
GQuark                      gtk_source_completion_error_quark         (void);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                    gtk_source_completion_add_provider        (GtkSourceCompletion          *completion,
                                                                       GtkSourceCompletionProvider  *provider,
                                                                       GError                      **error);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                    gtk_source_completion_remove_provider     (GtkSourceCompletion          *completion,
                                                                       GtkSourceCompletionProvider  *provider,
                                                                       GError                      **error);
GTK_SOURCE_AVAILABLE_IN_ALL
GList                      *gtk_source_completion_get_providers       (GtkSourceCompletion          *completion);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                    gtk_source_completion_start               (GtkSourceCompletion          *completion,
                                                                       GList                        *providers,
                                                                       GtkSourceCompletionContext   *context);
GTK_SOURCE_AVAILABLE_IN_ALL
void                        gtk_source_completion_hide                (GtkSourceCompletion          *completion);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionInfo    *gtk_source_completion_get_info_window     (GtkSourceCompletion          *completion);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceView              *gtk_source_completion_get_view            (GtkSourceCompletion          *completion);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionContext *gtk_source_completion_create_context      (GtkSourceCompletion          *completion,
                                                                       GtkTextIter                  *position);
GTK_SOURCE_AVAILABLE_IN_ALL
void                        gtk_source_completion_block_interactive   (GtkSourceCompletion          *completion);
GTK_SOURCE_AVAILABLE_IN_ALL
void                        gtk_source_completion_unblock_interactive (GtkSourceCompletion          *completion);

G_END_DECLS

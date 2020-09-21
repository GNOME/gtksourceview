/*
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

#include "gtksourcecompletioncontext.h"
#include "gtksourcetypes.h"

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_COMPLETION_PROVIDER (gtk_source_completion_provider_get_type())

GTK_SOURCE_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkSourceCompletionProvider, gtk_source_completion_provider, GTK_SOURCE, COMPLETION_PROVIDER, GObject)

/**
 * GtkSourceCompletionProviderInterface:
 * @g_iface: The parent interface.
 * @get_name: The virtual function pointer for gtk_source_completion_provider_get_name().
 * Must be implemented.
 * @get_icon: The virtual function pointer for gtk_source_completion_provider_get_icon().
 * By default, %NULL is returned.
 * @get_icon_name: The virtual function pointer for gtk_source_completion_provider_get_icon_name().
 * By default, %NULL is returned.
 * @get_gicon: The virtual function pointer for gtk_source_completion_provider_get_gicon().
 * By default, %NULL is returned.
 * @populate: The virtual function pointer for gtk_source_completion_provider_populate().
 * Add no proposals by default.
 * @match: The virtual function pointer for gtk_source_completion_provider_match().
 * By default, %TRUE is returned.
 * @get_activation: The virtual function pointer for gtk_source_completion_provider_get_activation().
 * The combination of all #GtkSourceCompletionActivation is returned by default.
 * @get_info_widget: The virtual function pointer for gtk_source_completion_provider_get_info_widget().
 * By default, %NULL is returned.
 * @update_info: The virtual function pointer for gtk_source_completion_provider_update_info().
 * Does nothing by default.
 * @get_start_iter: The virtual function pointer for gtk_source_completion_provider_get_start_iter().
 * By default, %FALSE is returned.
 * @activate_proposal: The virtual function pointer for gtk_source_completion_provider_activate_proposal().
 * By default, %FALSE is returned.
 * @get_interactive_delay: The virtual function pointer for gtk_source_completion_provider_get_interactive_delay().
 * By default, -1 is returned.
 * @get_priority: The virtual function pointer for gtk_source_completion_provider_get_priority().
 * By default, 0 is returned.
 *
 * The virtual function table for #GtkSourceCompletionProvider.
 */
struct _GtkSourceCompletionProviderInterface
{
	GTypeInterface g_iface;

	gchar                         *(*get_name)              (GtkSourceCompletionProvider *provider);
	GdkTexture                    *(*get_icon)              (GtkSourceCompletionProvider *provider);
	const gchar                   *(*get_icon_name)         (GtkSourceCompletionProvider *provider);
	GIcon                         *(*get_gicon)             (GtkSourceCompletionProvider *provider);
	void                           (*populate)              (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionContext  *context);
	gboolean                       (*match)                 (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionContext  *context);
	GtkSourceCompletionActivation  (*get_activation)        (GtkSourceCompletionProvider *provider);
	GtkWidget                     *(*get_info_widget)       (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionProposal *proposal);
	void                           (*update_info)           (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionProposal *proposal,
	                                                         GtkSourceCompletionInfo     *info);
	gboolean                       (*get_start_iter)        (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionContext  *context,
	                                                         GtkSourceCompletionProposal *proposal,
	                                                         GtkTextIter                 *iter);
	gboolean                       (*activate_proposal)     (GtkSourceCompletionProvider *provider,
	                                                         GtkSourceCompletionProposal *proposal,
	                                                         GtkTextIter                 *iter);
	gint                           (*get_interactive_delay) (GtkSourceCompletionProvider *provider);
	gint                           (*get_priority)          (GtkSourceCompletionProvider *provider);
};

GTK_SOURCE_AVAILABLE_IN_ALL
gchar                         *gtk_source_completion_provider_get_name              (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_ALL
GdkTexture                    *gtk_source_completion_provider_get_icon              (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_3_18
const gchar                   *gtk_source_completion_provider_get_icon_name         (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_3_18
GIcon                         *gtk_source_completion_provider_get_gicon             (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_ALL
void                           gtk_source_completion_provider_populate              (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionContext  *context);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkSourceCompletionActivation  gtk_source_completion_provider_get_activation        (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_provider_match                 (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionContext  *context);
GTK_SOURCE_AVAILABLE_IN_ALL
GtkWidget                     *gtk_source_completion_provider_get_info_widget       (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionProposal *proposal);
GTK_SOURCE_AVAILABLE_IN_ALL
void                           gtk_source_completion_provider_update_info           (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionProposal *proposal,
                                                                                     GtkSourceCompletionInfo     *info);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_provider_get_start_iter        (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionContext  *context,
                                                                                     GtkSourceCompletionProposal *proposal,
                                                                                     GtkTextIter                 *iter);
GTK_SOURCE_AVAILABLE_IN_ALL
gboolean                       gtk_source_completion_provider_activate_proposal     (GtkSourceCompletionProvider *provider,
                                                                                     GtkSourceCompletionProposal *proposal,
                                                                                     GtkTextIter                 *iter);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                           gtk_source_completion_provider_get_interactive_delay (GtkSourceCompletionProvider *provider);
GTK_SOURCE_AVAILABLE_IN_ALL
gint                           gtk_source_completion_provider_get_priority          (GtkSourceCompletionProvider *provider);

G_END_DECLS

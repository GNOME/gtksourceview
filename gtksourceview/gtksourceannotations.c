/*
 * gtksourceannotations.c
 *
 * Copyright 2025 Nokse <nokse@posteo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */


#include "config.h"

#include "gtksourceannotation.h"
#include "gtksourceannotation-private.h"
#include "gtksourceannotations.h"
#include "gtksourceannotations-private.h"
#include "gtksourceannotationprovider.h"
#include "gtksourceannotationprovider-private.h"
#include "gtksourcebuffer.h"
#include "gtksourcebuffer-private.h"
#include "gtksourceiter-private.h"
#include "gtksourcestylescheme.h"
#include "gtksourcestylescheme-private.h"
#include "gtksourcetag.h"
#include "gtksourcetrace.h"
#include "gtksourceview.h"

/**
 * GtkSourceAnnotations:
 *
 * Use this object to manage [class@Annotation]s. Each [class@View] has a single annotation
 * manager and it is guaranteed to be the same for the lifetime of [class@View].
 *
 * Add [class@AnnotationProvider]s with [method@Annotations.add_provider] to
 * display all the annotations added to each [class@AnnotationProvider].
 *
 * Since: 5.18
 */

struct _GtkSourceAnnotations
{
	GObject     parent_instance;
	GdkRGBA     color;
	GPtrArray  *providers;
	int         next_id;
	guint       color_set : 1;
};

enum {
	CHANGED,
	N_SIGNALS
};

G_DEFINE_TYPE (GtkSourceAnnotations, gtk_source_annotations, G_TYPE_OBJECT)

static guint signals[N_SIGNALS];

static void
gtk_source_annotations_finalize (GObject *object)
{
	GtkSourceAnnotations *self = GTK_SOURCE_ANNOTATIONS (object);

	g_clear_pointer (&self->providers, g_ptr_array_unref);

	G_OBJECT_CLASS (gtk_source_annotations_parent_class)->finalize (object);
}

static void
gtk_source_annotations_class_init (GtkSourceAnnotationsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_annotations_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 0);
}

static void
gtk_source_annotations_init (GtkSourceAnnotations *self)
{
	self->providers = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
on_provider_changed (GtkSourceAnnotations *self)
{
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self));

	g_signal_emit (self, signals[CHANGED], 0);
}

/**
 * gtk_source_annotations_add_provider:
 * @self: a #GtkSourceAnnotations
 * @provider: a #GtkSourceAnnotationProvider.
 *
 * Adds a new annotation provider.
 */
void
gtk_source_annotations_add_provider (GtkSourceAnnotations  *self,
                                     GtkSourceAnnotationProvider *provider)
{
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self));
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (provider));

	for (guint i = 0; i < self->providers->len; i++)
	{
		if (g_ptr_array_index (self->providers, i) == (gpointer)provider)
		{
			return;
		}
	}

	g_ptr_array_add (self->providers, g_object_ref (provider));

	g_signal_connect_swapped (provider,
	                          "changed",
	                          G_CALLBACK (on_provider_changed),
	                          self);

	g_signal_emit (self, signals[CHANGED], 0);
}

/**
 * gtk_source_annotations_remove_provider:
 * @self: a #GtkSourceAnnotations
 * @provider: a #GtkSourceAnnotationProvider.
 *
 * Removes a provider.
 *
 * Returns: %TRUE if the provider was found and removed
 */
gboolean
gtk_source_annotations_remove_provider (GtkSourceAnnotations  *self,
                                        GtkSourceAnnotationProvider *provider)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (provider), FALSE);

	for (guint i = 0; i < self->providers->len; i++)
	{
		if (g_ptr_array_index (self->providers, i) == (gpointer)provider)
		{
			g_ptr_array_remove_index (self->providers, i);

			g_signal_emit (self, signals[CHANGED], 0);

			return TRUE;
		}
	}

	return FALSE;
}

void
_gtk_source_annotations_update_color (GtkSourceAnnotations *self,
                                      GtkSourceView              *view)
{
	GtkSourceBuffer *buffer;
	GtkSourceStyleScheme *style_scheme;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));

	self->color_set = FALSE;

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
	style_scheme = gtk_source_buffer_get_style_scheme (buffer);

	if (style_scheme != NULL)
	{
		GtkSourceStyle *style;

		style = _gtk_source_style_scheme_get_draw_spaces_style (style_scheme);

		if (style != NULL)
		{
			gchar *color_str = NULL;
			gboolean color_set;
			GdkRGBA color;

			g_object_get (style,
			              "foreground", &color_str,
			              "foreground-set", &color_set,
			              NULL);

			if (color_set &&
			    color_str != NULL &&
			    gdk_rgba_parse (&color, color_str))
			{
				self->color = color;
				self->color_set = TRUE;
			}

			g_free (color_str);
		}
	}

	if (!self->color_set)
	{
		gtk_widget_get_color (GTK_WIDGET (view), &self->color);
		self->color.alpha *= .5;
		self->color_set = TRUE;
	}
}

static void
_gtk_source_annotations_draw_annotation (GtkSourceAnnotations *manager,
                                         GtkSourceView              *view,
                                         GtkSnapshot                *snapshot,
                                         GtkSourceAnnotation        *annotation)
{
	GtkTextView *text_view;
	GtkTextBuffer *buffer;
	GtkTextIter line_start_iter;
	GtkTextIter line_end_iter;
	GdkRectangle rect;
	int line_number;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATIONS (manager));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (annotation));

	text_view = GTK_TEXT_VIEW (view);
	buffer = gtk_text_view_get_buffer (text_view);

	line_number = gtk_source_annotation_get_line (annotation);

	if (line_number >= gtk_text_buffer_get_line_count (buffer))
	{
		return;
	}

	gtk_text_buffer_get_iter_at_line (buffer, &line_start_iter, line_number);

	line_end_iter = line_start_iter;

	if (gtk_text_iter_ends_line (&line_start_iter)) {
		gtk_text_view_get_iter_location (text_view, &line_start_iter, &rect);
	} else {
		gtk_text_iter_forward_to_line_end (&line_end_iter);
		gtk_text_view_get_iter_location (text_view, &line_end_iter, &rect);
	}

	/* Ensure the annotation is not drawn over the space drawer new line */
	rect.x += rect.height * 2;

	_gtk_source_annotation_draw (annotation,
	                             snapshot,
	                             GTK_SOURCE_VIEW (view),
	                             rect,
	                             &manager->color);
}

void
_gtk_source_annotations_draw (GtkSourceAnnotations *self,
                              GtkSourceView              *view,
                              GtkSnapshot                *snapshot)
{
	GdkRectangle visible_rect;
	GtkTextIter start_visible, end_visible;
	gint first_visible_line, last_visible_line;
	guint i, j;

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self));
	g_return_if_fail (GTK_SOURCE_IS_VIEW (view));
	g_return_if_fail (snapshot != NULL);

	gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view), &visible_rect);

	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
	                                    &start_visible,
	                                    visible_rect.x,
	                                    visible_rect.y);
	gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view),
	                                    &end_visible,
	                                    visible_rect.x,
	                                    visible_rect.y + visible_rect.height);

	first_visible_line = gtk_text_iter_get_line (&start_visible);
	last_visible_line = gtk_text_iter_get_line (&end_visible);

	for (i = 0; i < self->providers->len; i++)
	{
		GtkSourceAnnotationProvider *provider = g_ptr_array_index (self->providers, i);
		GPtrArray *annotations = _gtk_source_annotation_provider_get_annotations (provider);

		for (j = 0; j < annotations->len; j++)
		{
			GtkSourceAnnotation *annotation = g_ptr_array_index (annotations, j);

			gint annotation_line = gtk_source_annotation_get_line (annotation);

			if (annotation_line >= first_visible_line &&
			    annotation_line <= last_visible_line)
			{
				_gtk_source_annotations_draw_annotation (self, view, snapshot, annotation);
			}
		}
	}
}

GPtrArray *
_gtk_source_annotations_get_providers (GtkSourceAnnotations *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATIONS (self), NULL);

	return self->providers;
}

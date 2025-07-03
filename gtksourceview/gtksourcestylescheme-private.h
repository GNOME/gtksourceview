/*
 * This file is part of GtkSourceView
 *
 * Copyright 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtksourcestylescheme.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtkSourceStyleScheme *_gtk_source_style_scheme_new_from_file                            (const gchar          *filename);
G_GNUC_INTERNAL
GtkSourceStyleScheme *_gtk_source_style_scheme_get_default                              (void);
G_GNUC_INTERNAL
const gchar          *_gtk_source_style_scheme_get_parent_id                            (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
void                  _gtk_source_style_scheme_set_parent                               (GtkSourceStyleScheme *scheme,
                                                                                         GtkSourceStyleScheme *parent_scheme);
G_GNUC_INTERNAL
void                  _gtk_source_style_scheme_apply                                    (GtkSourceStyleScheme *scheme,
                                                                                         GtkWidget            *widget);
G_GNUC_INTERNAL
void                  _gtk_source_style_scheme_unapply                                  (GtkSourceStyleScheme *scheme,
                                                                                         GtkWidget            *widget);
G_GNUC_INTERNAL
GtkSourceStyle       *_gtk_source_style_scheme_get_matching_brackets_style              (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
GtkSourceStyle       *_gtk_source_style_scheme_get_snippet_focus_style                  (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
GtkSourceStyle       *_gtk_source_style_scheme_get_right_margin_style                   (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
GtkSourceStyle       *_gtk_source_style_scheme_get_draw_spaces_style                    (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_current_line_number_bold             (GtkSourceStyleScheme *scheme);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_current_line_background_color        (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_current_line_number_color            (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_current_line_number_background_color (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_background_pattern_color             (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_background_color                     (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_text_color                           (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_warning_color                        (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_error_color                          (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);
G_GNUC_INTERNAL
gboolean              _gtk_source_style_scheme_get_accent_color                         (GtkSourceStyleScheme *scheme,
                                                                                         GdkRGBA              *color);

G_END_DECLS

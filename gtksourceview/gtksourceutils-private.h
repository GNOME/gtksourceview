/*
 * This file is part of GtkSourceView
 *
 * Copyright 2007 - Gustavo Giráldez
 * Copyright 2007 - Paolo Maggi
 * Copyright 2017 - Sébastien Wilmet <swilmet@gnome.org>
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
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <pango/pango.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
gchar  **_gtk_source_utils_get_default_dirs              (const gchar                 *basename);
G_GNUC_INTERNAL
GSList  *_gtk_source_utils_get_file_list                 (gchar                      **path,
                                                          const gchar                 *suffix,
                                                          gboolean                     only_dirs);
G_GNUC_INTERNAL
gint     _gtk_source_utils_string_to_int                 (const gchar                 *str);
G_GNUC_INTERNAL
gint     _gtk_source_utils_int_to_string                 (guint                        value,
                                                          const gchar                **outstr);
G_GNUC_INTERNAL
gchar   *_gtk_source_utils_pango_font_description_to_css (const PangoFontDescription  *font_desc);

/* Note: it returns duplicated string. */
G_GNUC_INTERNAL
gchar   *_gtk_source_utils_dgettext                      (const gchar                 *domain,
                                                          const gchar                 *msgid) G_GNUC_FORMAT(2);

G_GNUC_INTERNAL
void     _gtk_source_view_jump_to_iter                   (GtkTextView                 *view,
                                                          const GtkTextIter           *iter,
                                                          double                       within_margin,
                                                          gboolean                     use_align,
                                                          double                       xalign,
                                                          double                       yalign);

G_GNUC_INTERNAL
char    *_gtk_source_utils_aligned_alloc                 (gsize                        size,
                                                          gsize                        number,
                                                          gsize                        alignment);
G_GNUC_INTERNAL
void     _gtk_source_utils_aligned_free                  (gpointer                     data);
G_GNUC_INTERNAL
gsize    _gtk_source_utils_get_page_size                 (void) G_GNUC_CONST;
G_GNUC_INTERNAL
PangoFontMap *_gtk_source_utils_get_builder_blocks       (void);
G_GNUC_INTERNAL
gsize    _gtk_source_utils_strnlen                       (const char                  *str,
                                                          gsize                        maxlen);
G_GNUC_INTERNAL
void     _gtk_source_widget_add_css_provider             (GtkWidget                   *widget,
                                                          GtkCssProvider              *provider,
							  guint                        priority);
G_GNUC_INTERNAL
void     _gtk_source_widget_remove_css_provider          (GtkWidget                   *widget,
                                                          GtkCssProvider              *provider);
G_GNUC_INTERNAL
void     _gtk_source_add_css_provider                    (GdkDisplay                  *display,
                                                          GtkCssProvider              *provider,
							  guint                        priority);

static inline void
premix_colors (GdkRGBA       *dest,
               const GdkRGBA *fg,
               const GdkRGBA *bg,
               gboolean       bg_set,
               double         alpha)
{
	g_assert (dest != NULL);
	g_assert (fg != NULL);
	g_assert (bg != NULL || bg_set == FALSE);
	g_assert (alpha >= 0.0 && alpha <= 1.0);

	if (bg_set)
	{
		dest->red = ((1 - alpha) * bg->red) + (alpha * fg->red);
		dest->green = ((1 - alpha) * bg->green) + (alpha * fg->green);
		dest->blue = ((1 - alpha) * bg->blue) + (alpha * fg->blue);
		dest->alpha = 1.0;
	}
	else
	{
		*dest = *fg;
		dest->alpha = alpha;
	}
}

G_END_DECLS

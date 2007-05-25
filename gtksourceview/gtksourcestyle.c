/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourcestyle.c
 *
 *  Copyright (C) 2003 - Paolo Maggi <paolo.maggi@polito.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "gtksourcestyle.h"

GType
gtk_source_style_get_type (void)
{
	static GType type;

	if (G_UNLIKELY (type == 0))
		type = g_boxed_type_register_static ("GtkSourceStyle",
						     (GBoxedCopyFunc) gtk_source_style_copy,
						     (GBoxedFreeFunc) gtk_source_style_free);

	return type;
}

/**
 * gtk_source_style_new:
 * @mask: a #GtkSourceStyleMask which defines what fields will be used.
 *
 * Returns: newly allocated #GtkSourceStyle structure, free with
 * gtk_source_style_free().
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_new (GtkSourceStyleMask mask)
{
	GtkSourceStyle *style = g_new0 (GtkSourceStyle, 1);
	style->mask = mask;
	return style;
}

/**
 * gtk_source_style_copy:
 * @style: a #GtkSourceStyle structure to copy.
 *
 * Returns: copy of @style, free it with gtk_source_style_free().
 *
 * Since: 2.0
 */
GtkSourceStyle *
gtk_source_style_copy (const GtkSourceStyle *style)
{
	return g_memdup (style, sizeof (GtkSourceStyle));
}

/**
 * gtk_source_style_free:
 * @style: a #GtkSourceStyle structure to free.
 *
 * Frees #GtkSourceStyle structure allocated with gtk_source_style_new() or
 * gtk_source_style_copy().
 *
 * Since: 2.0
 */
void
gtk_source_style_free (GtkSourceStyle *style)
{
	g_free (style);
}

/**
 * _gtk_source_style_apply:
 * @style: a #GtkSourceStyle to apply.
 * @tag: a #GtkTextTag to apply styles to.
 *
 * Applies text styles set in @style if it's not %NULL, or
 * unsets style fields in @tag set with _gtk_source_style_apply()
 * if @style is %NULL.
 *
 * Since: 2.0
 */
void
_gtk_source_style_apply (const GtkSourceStyle *style,
			 GtkTextTag           *tag)
{
	g_return_if_fail (GTK_IS_TEXT_TAG (tag));

	if (style != NULL)
	{
		g_object_freeze_notify (G_OBJECT (tag));

		if (style->mask & GTK_SOURCE_STYLE_USE_BACKGROUND)
			g_object_set (tag, "background-gdk", &style->background, NULL);
		if (style->mask & GTK_SOURCE_STYLE_USE_FOREGROUND)
			g_object_set (tag, "foreground-gdk", &style->foreground, NULL);
		if (style->mask & GTK_SOURCE_STYLE_USE_ITALIC)
			g_object_set (tag, "style", style->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL, NULL);
		if (style->mask & GTK_SOURCE_STYLE_USE_BOLD)
			g_object_set (tag, "weight", style->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, NULL);
		if (style->mask & GTK_SOURCE_STYLE_USE_UNDERLINE)
			g_object_set (tag, "underline", style->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE, NULL);
		if (style->mask & GTK_SOURCE_STYLE_USE_STRIKETHROUGH)
			g_object_set (tag, "strikethrough", style->strikethrough != 0, NULL);

		g_object_thaw_notify (G_OBJECT (tag));
	}
	else
	{
		g_object_set (tag,
			      "background-set", FALSE,
			      "foreground-set", FALSE,
			      "style-set", FALSE,
			      "weight-set", FALSE,
			      "underline-set", FALSE,
			      "strikethrough-set", FALSE,
			      NULL);
	}
}

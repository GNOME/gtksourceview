/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 *
 * gtktextregion.h - GtkTextMark based region utility functions
 *
 * This file is part of the GtkSourceView widget
 *
 * Copyright (C) 2002 Gustavo Giráldez <gustavo.giraldez@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.  
 */

#ifndef __GTK_TEXT_REGION_H__
#define __GTK_TEXT_REGION_H__

#include <gtk/gtktextbuffer.h>

G_BEGIN_DECLS

typedef struct _GtkTextRegion GtkTextRegion;

GtkTextRegion *gtk_text_region_new                          (GtkTextBuffer *buffer);
void           gtk_text_region_destroy                      (GtkTextRegion *region,
							     gboolean       delete_marks);

GtkTextBuffer *gtk_text_region_get_buffer                   (GtkTextRegion *region);

void           gtk_text_region_clear_zero_length_subregions (GtkTextRegion *region);

void           gtk_text_region_add                          (GtkTextRegion     *region,
							     const GtkTextIter *_start,
							     const GtkTextIter *_end);

void           gtk_text_region_substract                    (GtkTextRegion     *region,
							     const GtkTextIter *_start,
							     const GtkTextIter *_end);

gint           gtk_text_region_subregions                   (GtkTextRegion *region);

gboolean       gtk_text_region_nth_subregion                (GtkTextRegion *region,
							     guint          subregion,
							     GtkTextIter   *start,
							     GtkTextIter   *end);

GtkTextRegion *gtk_text_region_intersect                    (GtkTextRegion     *region,
							     const GtkTextIter *_start,
							     const GtkTextIter *_end);

void           gtk_text_region_debug_print                  (GtkTextRegion *region);

G_END_DECLS

#endif /* __GTK_TEXT_REGION_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourceencoding.h
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2002-2005 - Paolo Maggi
 * Copyright (C) 2014 - Sébastien Wilmet <swilmet@gnome.org>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GTK_SOURCE_ENCODING_H__
#define __GTK_SOURCE_ENCODING_H__

#include <glib.h>
#include <glib-object.h>
#include <gtksourceview/gtksourcetypes.h>

G_BEGIN_DECLS

#define GTK_SOURCE_TYPE_ENCODING (gtk_source_encoding_get_type ())

GType			 gtk_source_encoding_get_type		(void) G_GNUC_CONST;

const GtkSourceEncoding	*gtk_source_encoding_get_from_charset	(const gchar             *charset);

gchar			*gtk_source_encoding_to_string		(const GtkSourceEncoding *enc);

const gchar		*gtk_source_encoding_get_name		(const GtkSourceEncoding *enc);
const gchar		*gtk_source_encoding_get_charset	(const GtkSourceEncoding *enc);

const GtkSourceEncoding	*gtk_source_encoding_get_utf8		(void);
const GtkSourceEncoding	*gtk_source_encoding_get_current	(void);

GSList			*gtk_source_encoding_get_all		(void);

/* These should not be used, they are just to make python bindings happy */
GtkSourceEncoding	*gtk_source_encoding_copy		(const GtkSourceEncoding *enc);
void			 gtk_source_encoding_free		(GtkSourceEncoding       *enc);

G_END_DECLS

#endif  /* __GTK_SOURCE_ENCODING_H__ */

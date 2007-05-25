/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourceviewutils.h
 *
 *  Copyright (C) 2007 - Gustavo Giráldez and Paolo Maggi
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
 *  You should have received a copy of the GNU Library General Public
 *  License* along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 */

#ifndef __GTK_SOURCE_VIEW_UTILS_H__
#define __GTK_SOURCE_VIEW_UTILS_H__

#include <glib.h>

G_BEGIN_DECLS

gchar **_gtk_source_view_get_default_dirs (const char *basename,
					   gboolean    compat);
GSList *_gtk_source_view_get_file_list    (char      **dirs,
					   const char *suffix);

G_END_DECLS

#endif /* __GTK_SOURCE_VIEW_UTILS_H__ */

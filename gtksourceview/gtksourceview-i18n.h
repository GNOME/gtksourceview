/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 *
 * This file is part of GtkSourceView
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * Copyright (C) 2016, 2017 - Sébastien Wilmet <swilmet@gnome.org>
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

/* Author: Tom Tromey <tromey@creche.cygnus.com> */

#ifndef GTKSOURCEVIEW_18N_H
#define GTKSOURCEVIEW_18N_H 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

G_BEGIN_DECLS

#define GD_(Domain,String) _gtksourceview_dgettext (Domain, String)

/* NOTE: it returns duplicated string */
G_GNUC_INTERNAL
gchar *		_gtksourceview_dgettext		(const gchar *domain,
						 const gchar *msgid) G_GNUC_FORMAT(2);

G_END_DECLS

#endif /* GTKSOURCEVIEW_I18N_H */

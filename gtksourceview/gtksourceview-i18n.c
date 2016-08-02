/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- *
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of GtkSourceView
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gtksourceview-i18n.h"

const gchar *
_gtksourceview_gettext (const gchar *msgid)
{
	return g_dgettext (GETTEXT_PACKAGE, msgid);
}

/**
 * _gtksourceview_dgettext:
 *
 * Try to translate string from given domain. It returns
 * duplicated string which must be freed with g_free().
 */
#ifdef ENABLE_NLS
char *
_gtksourceview_dgettext (const char *domain,
                         const char *string)
{
	gchar *tmp;
	const gchar *translated;

	g_return_val_if_fail (string != NULL, NULL);

	if (domain == NULL)
		return g_strdup (_gtksourceview_gettext (string));

	translated = dgettext (domain, string);

	if (strcmp (translated, string) == 0)
		return g_strdup (_gtksourceview_gettext (string));

	if (g_utf8_validate (translated, -1, NULL))
		return g_strdup (translated);

	tmp = g_locale_to_utf8 (translated, -1, NULL, NULL, NULL);

	if (tmp == NULL)
		return g_strdup (string);
	else
		return tmp;
}
#else
char *
_gtksourceview_dgettext (const char *domain,
                         const char *string)
{
	return g_strdup (string);
}
#endif

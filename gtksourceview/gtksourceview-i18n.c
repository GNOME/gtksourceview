/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the GtkSourceView widget.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourceview-i18n.h"
#include <string.h>

/*
 * Small hack since we don't have a proper place where
 * do gettext initialization.
 */
char *
_gtksourceview_gettext (const char *msgid)
{
	static gboolean initialized = FALSE;

	if (G_UNLIKELY (!initialized))
	{
		bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		initialized = TRUE;
	}

	return dgettext (GETTEXT_PACKAGE, msgid);
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

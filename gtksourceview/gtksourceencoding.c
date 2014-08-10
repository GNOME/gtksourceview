/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */
/* gtksourceencoding.c
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

#include "gtksourceencoding.h"
#include "gtksourceview-i18n.h"

/**
 * SECTION:encoding
 * @Short_description: Character encoding
 * @Title: GtkSourceEncoding
 * @See_also: #GtkSourceFileSaver, #GtkSourceFileLoader
 *
 * The #GtkSourceEncoding boxed type represents a character encoding. It is used
 * for example by #GtkSourceFile. Note that the text in GTK+ widgets is always
 * encoded in UTF-8.
 */

struct _GtkSourceEncoding
{
	gint index;
	const gchar *charset;
	const gchar *name;
};

G_DEFINE_BOXED_TYPE (GtkSourceEncoding, gtk_source_encoding,
                     gtk_source_encoding_copy,
                     gtk_source_encoding_free)

/*
 * The original versions of the following tables are taken from profterm
 *
 * Copyright (C) 2002 Red Hat, Inc.
 */

typedef enum
{
	GTK_SOURCE_ENCODING_ISO_8859_1,
	GTK_SOURCE_ENCODING_ISO_8859_2,
	GTK_SOURCE_ENCODING_ISO_8859_3,
	GTK_SOURCE_ENCODING_ISO_8859_4,
	GTK_SOURCE_ENCODING_ISO_8859_5,
	GTK_SOURCE_ENCODING_ISO_8859_6,
	GTK_SOURCE_ENCODING_ISO_8859_7,
	GTK_SOURCE_ENCODING_ISO_8859_8,
	GTK_SOURCE_ENCODING_ISO_8859_9,
	GTK_SOURCE_ENCODING_ISO_8859_10,
	GTK_SOURCE_ENCODING_ISO_8859_13,
	GTK_SOURCE_ENCODING_ISO_8859_14,
	GTK_SOURCE_ENCODING_ISO_8859_15,
	GTK_SOURCE_ENCODING_ISO_8859_16,

	GTK_SOURCE_ENCODING_UTF_7,
	GTK_SOURCE_ENCODING_UTF_16,
	GTK_SOURCE_ENCODING_UTF_16_BE,
	GTK_SOURCE_ENCODING_UTF_16_LE,
	GTK_SOURCE_ENCODING_UTF_32,
	GTK_SOURCE_ENCODING_UCS_2,
	GTK_SOURCE_ENCODING_UCS_4,

	GTK_SOURCE_ENCODING_ARMSCII_8,
	GTK_SOURCE_ENCODING_BIG5,
	GTK_SOURCE_ENCODING_BIG5_HKSCS,
	GTK_SOURCE_ENCODING_CP_866,

	GTK_SOURCE_ENCODING_EUC_JP,
	GTK_SOURCE_ENCODING_EUC_JP_MS,
	GTK_SOURCE_ENCODING_CP932,
	GTK_SOURCE_ENCODING_EUC_KR,
	GTK_SOURCE_ENCODING_EUC_TW,

	GTK_SOURCE_ENCODING_GB18030,
	GTK_SOURCE_ENCODING_GB2312,
	GTK_SOURCE_ENCODING_GBK,
	GTK_SOURCE_ENCODING_GEOSTD8,

	GTK_SOURCE_ENCODING_IBM_850,
	GTK_SOURCE_ENCODING_IBM_852,
	GTK_SOURCE_ENCODING_IBM_855,
	GTK_SOURCE_ENCODING_IBM_857,
	GTK_SOURCE_ENCODING_IBM_862,
	GTK_SOURCE_ENCODING_IBM_864,

	GTK_SOURCE_ENCODING_ISO_2022_JP,
	GTK_SOURCE_ENCODING_ISO_2022_KR,
	GTK_SOURCE_ENCODING_ISO_IR_111,
	GTK_SOURCE_ENCODING_JOHAB,
	GTK_SOURCE_ENCODING_KOI8_R,
	GTK_SOURCE_ENCODING_KOI8__R,
	GTK_SOURCE_ENCODING_KOI8_U,

	GTK_SOURCE_ENCODING_SHIFT_JIS,
	GTK_SOURCE_ENCODING_TCVN,
	GTK_SOURCE_ENCODING_TIS_620,
	GTK_SOURCE_ENCODING_UHC,
	GTK_SOURCE_ENCODING_VISCII,

	GTK_SOURCE_ENCODING_WINDOWS_1250,
	GTK_SOURCE_ENCODING_WINDOWS_1251,
	GTK_SOURCE_ENCODING_WINDOWS_1252,
	GTK_SOURCE_ENCODING_WINDOWS_1253,
	GTK_SOURCE_ENCODING_WINDOWS_1254,
	GTK_SOURCE_ENCODING_WINDOWS_1255,
	GTK_SOURCE_ENCODING_WINDOWS_1256,
	GTK_SOURCE_ENCODING_WINDOWS_1257,
	GTK_SOURCE_ENCODING_WINDOWS_1258,

	GTK_SOURCE_ENCODING_LAST,

	GTK_SOURCE_ENCODING_UTF_8,
	GTK_SOURCE_ENCODING_UNKNOWN
} GtkSourceEncodingIndex;

static const GtkSourceEncoding utf8_encoding =
{
	GTK_SOURCE_ENCODING_UTF_8,
	"UTF-8",
	N_("Unicode")
};

/* Initialized in gtk_source_encoding_lazy_init(). */
static GtkSourceEncoding unknown_encoding =
{
	GTK_SOURCE_ENCODING_UNKNOWN,
	NULL,
	NULL
};

static const GtkSourceEncoding encodings[] =
{
	{ GTK_SOURCE_ENCODING_ISO_8859_1,
	  "ISO-8859-1", N_("Western") },
	{ GTK_SOURCE_ENCODING_ISO_8859_2,
	 "ISO-8859-2", N_("Central European") },
	{ GTK_SOURCE_ENCODING_ISO_8859_3,
	  "ISO-8859-3", N_("South European") },
	{ GTK_SOURCE_ENCODING_ISO_8859_4,
	  "ISO-8859-4", N_("Baltic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_5,
	  "ISO-8859-5", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_6,
	  "ISO-8859-6", N_("Arabic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_7,
	  "ISO-8859-7", N_("Greek") },
	{ GTK_SOURCE_ENCODING_ISO_8859_8,
	  "ISO-8859-8", N_("Hebrew Visual") },
	{ GTK_SOURCE_ENCODING_ISO_8859_9,
	  "ISO-8859-9", N_("Turkish") },
	{ GTK_SOURCE_ENCODING_ISO_8859_10,
	  "ISO-8859-10", N_("Nordic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_13,
	  "ISO-8859-13", N_("Baltic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_14,
	  "ISO-8859-14", N_("Celtic") },
	{ GTK_SOURCE_ENCODING_ISO_8859_15,
	  "ISO-8859-15", N_("Western") },
	{ GTK_SOURCE_ENCODING_ISO_8859_16,
	  "ISO-8859-16", N_("Romanian") },

	{ GTK_SOURCE_ENCODING_UTF_7,
	  "UTF-7", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UTF_16,
	  "UTF-16", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UTF_16_BE,
	  "UTF-16BE", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UTF_16_LE,
	  "UTF-16LE", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UTF_32,
	  "UTF-32", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UCS_2,
	  "UCS-2", N_("Unicode") },
	{ GTK_SOURCE_ENCODING_UCS_4,
	  "UCS-4", N_("Unicode") },

	{ GTK_SOURCE_ENCODING_ARMSCII_8,
	  "ARMSCII-8", N_("Armenian") },
	{ GTK_SOURCE_ENCODING_BIG5,
	  "BIG5", N_("Chinese Traditional") },
	{ GTK_SOURCE_ENCODING_BIG5_HKSCS,
	  "BIG5-HKSCS", N_("Chinese Traditional") },
	{ GTK_SOURCE_ENCODING_CP_866,
	  "CP866", N_("Cyrillic/Russian") },

	{ GTK_SOURCE_ENCODING_EUC_JP,
	  "EUC-JP", N_("Japanese") },
	{ GTK_SOURCE_ENCODING_EUC_JP_MS,
	  "EUC-JP-MS", N_("Japanese") },
	{ GTK_SOURCE_ENCODING_CP932,
	  "CP932", N_("Japanese") },

	{ GTK_SOURCE_ENCODING_EUC_KR,
	  "EUC-KR", N_("Korean") },
	{ GTK_SOURCE_ENCODING_EUC_TW,
	  "EUC-TW", N_("Chinese Traditional") },

	{ GTK_SOURCE_ENCODING_GB18030,
	  "GB18030", N_("Chinese Simplified") },
	{ GTK_SOURCE_ENCODING_GB2312,
	  "GB2312", N_("Chinese Simplified") },
	{ GTK_SOURCE_ENCODING_GBK,
	  "GBK", N_("Chinese Simplified") },
	{ GTK_SOURCE_ENCODING_GEOSTD8,
	  "GEORGIAN-ACADEMY", N_("Georgian") }, /* FIXME GEOSTD8 ? */

	{ GTK_SOURCE_ENCODING_IBM_850,
	  "IBM850", N_("Western") },
	{ GTK_SOURCE_ENCODING_IBM_852,
	  "IBM852", N_("Central European") },
	{ GTK_SOURCE_ENCODING_IBM_855,
	  "IBM855", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_IBM_857,
	  "IBM857", N_("Turkish") },
	{ GTK_SOURCE_ENCODING_IBM_862,
	  "IBM862", N_("Hebrew") },
	{ GTK_SOURCE_ENCODING_IBM_864,
	  "IBM864", N_("Arabic") },

	{ GTK_SOURCE_ENCODING_ISO_2022_JP,
	  "ISO-2022-JP", N_("Japanese") },
	{ GTK_SOURCE_ENCODING_ISO_2022_KR,
	  "ISO-2022-KR", N_("Korean") },
	{ GTK_SOURCE_ENCODING_ISO_IR_111,
	  "ISO-IR-111", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_JOHAB,
	  "JOHAB", N_("Korean") },
	{ GTK_SOURCE_ENCODING_KOI8_R,
	  "KOI8R", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_KOI8__R,
	  "KOI8-R", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_KOI8_U,
	  "KOI8U", N_("Cyrillic/Ukrainian") },

	{ GTK_SOURCE_ENCODING_SHIFT_JIS,
	  "SHIFT_JIS", N_("Japanese") },
	{ GTK_SOURCE_ENCODING_TCVN,
	  "TCVN", N_("Vietnamese") },
	{ GTK_SOURCE_ENCODING_TIS_620,
	  "TIS-620", N_("Thai") },
	{ GTK_SOURCE_ENCODING_UHC,
	  "UHC", N_("Korean") },
	{ GTK_SOURCE_ENCODING_VISCII,
	  "VISCII", N_("Vietnamese") },

	{ GTK_SOURCE_ENCODING_WINDOWS_1250,
	  "WINDOWS-1250", N_("Central European") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1251,
	  "WINDOWS-1251", N_("Cyrillic") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1252,
	  "WINDOWS-1252", N_("Western") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1253,
	  "WINDOWS-1253", N_("Greek") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1254,
	  "WINDOWS-1254", N_("Turkish") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1255,
	  "WINDOWS-1255", N_("Hebrew") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1256,
	  "WINDOWS-1256", N_("Arabic") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1257,
	  "WINDOWS-1257", N_("Baltic") },
	{ GTK_SOURCE_ENCODING_WINDOWS_1258,
	  "WINDOWS-1258", N_("Vietnamese") }
};

static void
gtk_source_encoding_lazy_init (void)
{
	static gboolean initialized = FALSE;
	const gchar *locale_charset;

	if (G_LIKELY (initialized))
	{
		return;
	}

	if (g_get_charset (&locale_charset) == FALSE)
	{
		unknown_encoding.charset = g_strdup (locale_charset);
	}

	initialized = TRUE;
}

/**
 * gtk_source_encoding_get_from_charset:
 * @charset: a character set.
 *
 * Gets a #GtkSourceEncoding from a character set such as "UTF-8" or
 * "ISO-8859-1".
 *
 * Returns: the corresponding #GtkSourceEncoding, or %NULL if not found.
 * Since: 3.14
 */
const GtkSourceEncoding *
gtk_source_encoding_get_from_charset (const gchar *charset)
{
	gint i;

	g_return_val_if_fail (charset != NULL, NULL);

	if (g_ascii_strcasecmp (charset, "UTF-8") == 0)
	{
		return gtk_source_encoding_get_utf8 ();
	}

	for (i = 0; i < GTK_SOURCE_ENCODING_LAST; i++)
	{
		if (g_ascii_strcasecmp (charset, encodings[i].charset) == 0)
		{
			return &encodings[i];
		}
	}

	gtk_source_encoding_lazy_init ();

	if (unknown_encoding.charset != NULL &&
	    g_ascii_strcasecmp (charset, unknown_encoding.charset) == 0)
	{
		return &unknown_encoding;
	}

	return NULL;
}

/**
 * gtk_source_encoding_foreach:
 * @func: (scope call): the function to call for each encoding
 * @user_data: user data to pass to the function
 *
 * Calls a function for each encoding.
 *
 * Since: 3.14
 */
void
gtk_source_encoding_foreach (GtkSourceEncodingForeachFunc func,
			     gpointer                     user_data)
{
	gint i;

	func (&utf8_encoding, user_data);

	for (i = 0; i < GTK_SOURCE_ENCODING_LAST; i++)
	{
		func (&encodings[i], user_data);
	}
}

/**
 * gtk_source_encoding_get_utf8:
 *
 * Returns: the UTF-8 encoding.
 * Since: 3.14
 */
const GtkSourceEncoding *
gtk_source_encoding_get_utf8 (void)
{
	return &utf8_encoding;
}

/**
 * gtk_source_encoding_get_current:
 *
 * Gets the #GtkSourceEncoding for the current locale. See also g_get_charset().
 *
 * Returns: the current locale encoding.
 * Since: 3.14
 */
const GtkSourceEncoding *
gtk_source_encoding_get_current (void)
{
	static gboolean initialized = FALSE;
	static const GtkSourceEncoding *locale_encoding = NULL;

	const gchar *locale_charset;

	gtk_source_encoding_lazy_init ();

	if (G_LIKELY (initialized))
	{
		return locale_encoding;
	}

	if (g_get_charset (&locale_charset))
	{
		locale_encoding = &utf8_encoding;
	}
	else
	{
		locale_encoding = gtk_source_encoding_get_from_charset (locale_charset);
	}

	if (locale_encoding == NULL)
	{
		locale_encoding = &unknown_encoding;
	}

	initialized = TRUE;

	return locale_encoding;
}

/**
 * gtk_source_encoding_to_string:
 * @enc: a #GtkSourceEncoding.
 *
 * Returns: a string representation. Free with g_free() when no longer needed.
 * Since: 3.14
 */
gchar *
gtk_source_encoding_to_string (const GtkSourceEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	gtk_source_encoding_lazy_init ();

	g_return_val_if_fail (enc->charset != NULL, NULL);

	if (enc->name != NULL)
	{
	    	return g_strdup_printf ("%s (%s)", _(enc->name), enc->charset);
	}
	else if (g_ascii_strcasecmp (enc->charset, "ANSI_X3.4-1968") == 0)
	{
		return g_strdup_printf ("US-ASCII (%s)", enc->charset);
	}
	else
	{
		return g_strdup (enc->charset);
	}
}

/**
 * gtk_source_encoding_get_charset:
 * @enc: a #GtkSourceEncoding.
 *
 * Gets the character set of the #GtkSourceEncoding, such as "UTF-8" or
 * "ISO-8859-1".
 *
 * Returns: the character set of the #GtkSourceEncoding.
 * Since: 3.14
 */
const gchar *
gtk_source_encoding_get_charset (const GtkSourceEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	gtk_source_encoding_lazy_init ();

	g_return_val_if_fail (enc->charset != NULL, NULL);

	return enc->charset;
}

/**
 * gtk_source_encoding_get_name:
 * @enc: a #GtkSourceEncoding.
 *
 * Gets the name of the #GtkSourceEncoding such as "Unicode" or "Western".
 *
 * Returns: the name of the #GtkSourceEncoding.
 * Since: 3.14
 */
const gchar *
gtk_source_encoding_get_name (const GtkSourceEncoding* enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	gtk_source_encoding_lazy_init ();

	return (enc->name == NULL) ? _("Unknown") : _(enc->name);
}

/**
 * gtk_source_encoding_get_default_candidates:
 *
 * Gets the list of default candidate encodings to try when loading a file. See
 * gtk_source_file_loader_set_candidate_encodings().
 *
 * This function returns a different list depending on the current locale (i.e.
 * language, country and default encoding).
 *
 * Returns: (transfer container) (element-type GtkSource.Encoding): the list of
 * default candidate encodings. Free with g_slist_free().
 * Since: 3.14
 */
GSList *
gtk_source_encoding_get_default_candidates (void)
{
	const gchar *encodings_str;
	GVariant *encodings_variant;
	const gchar **encodings_strv;
	GSList *encodings_list;

	/* Translators: This is the sorted list of encodings used by
	 * GtkSourceView for automatic detection of the file encoding. You may
	 * want to customize it adding encodings that are common in your
	 * country, for instance the GB18030 encoding for the Chinese
	 * translation. You may also want to remove the ISO-8859-15 encoding
	 * (covering English and most Western European languages) if you think
	 * people in your country will rarely use it.  "CURRENT" is a magic
	 * value used by GtkSourceView and it represents the encoding for the
	 * current locale, so please don't translate the "CURRENT" term.  Only
	 * recognized encodings are used. See
	 * https://git.gnome.org/browse/gtksourceview/tree/gtksourceview/gtksourceencoding.c#n147
	 * for a list of supported encodings.
	 */
	encodings_str = _("['UTF-8', 'CURRENT', 'ISO-8859-15', 'UTF-16']");

	encodings_variant = g_variant_new_parsed (encodings_str);
	g_variant_ref_sink (encodings_variant);

	encodings_strv = g_variant_get_strv (encodings_variant, NULL);

	encodings_list = _gtk_source_encoding_strv_to_list (encodings_strv);

	g_variant_unref (encodings_variant);
	return encodings_list;
}

/**
 * gtk_source_encoding_copy:
 * @enc: a #GtkSourceEncoding.
 *
 * Used by language bindings.
 *
 * Returns: a copy of @enc.
 * Since: 3.14
 */
GtkSourceEncoding *
gtk_source_encoding_copy (const GtkSourceEncoding *enc)
{
	g_return_val_if_fail (enc != NULL, NULL);

	return (GtkSourceEncoding *) enc;
}

/**
 * gtk_source_encoding_free:
 * @enc: a #GtkSourceEncoding.
 *
 * Used by language bindings.
 *
 * Since: 3.14
 */
void
gtk_source_encoding_free (GtkSourceEncoding *enc)
{
	g_return_if_fail (enc != NULL);
}

/* Will probably be used in the future. */
static gboolean
data_exists (GSList         *list,
	     const gpointer  data)
{
	for (; list != NULL; list = g_slist_next (list))
	{
		if (list->data == data)
		{
			return TRUE;
		}
	}

	return FALSE;
}

GSList *
_gtk_source_encoding_strv_to_list (const gchar * const *enc_str)
{
	GSList *res = NULL;
	gchar **p;

	for (p = (gchar **)enc_str; p != NULL && *p != NULL; p++)
	{
		const gchar *charset = *p;
		const GtkSourceEncoding *enc;

		if (g_str_equal (charset, "CURRENT"))
		{
			g_get_charset (&charset);
		}

		g_return_val_if_fail (charset != NULL, NULL);
		enc = gtk_source_encoding_get_from_charset (charset);

		if (enc != NULL &&
		    !data_exists (res, (gpointer)enc))
		{
			res = g_slist_prepend (res, (gpointer)enc);
		}
	}

	return g_slist_reverse (res);
}

#if 0
gchar **
_gtk_source_encoding_list_to_strv (const GSList *enc_list)
{
	GSList *l;
	GPtrArray *array;

	array = g_ptr_array_sized_new (g_slist_length ((GSList *)enc_list) + 1);

	for (l = (GSList *)enc_list; l != NULL; l = g_slist_next (l))
	{
		const GtkSourceEncoding *enc = l->data;
		const gchar *charset = gtk_source_encoding_get_charset (enc);

		g_return_val_if_fail (charset != NULL, NULL);

		g_ptr_array_add (array, g_strdup (charset));
	}

	g_ptr_array_add (array, NULL);

	return (gchar **)g_ptr_array_free (array, FALSE);
}
#endif

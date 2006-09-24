/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcetag.c
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gtksourceview-i18n.h"

#include "gtksourcetag.h"
#include "gtksourcetag-private.h"

static GtkTextTagClass          *parent_class = NULL;
static GtkSourceTagClass 	*parent_syntax_class  = NULL;
static GtkSourceTagClass 	*parent_pattern_class = NULL;

static void		 gtk_source_tag_init 		(GtkSourceTag       *text_tag);
static void		 gtk_source_tag_class_init 	(GtkSourceTagClass  *text_tag);
static void		 gtk_source_tag_finalize	(GObject            *object);

static void		 gtk_syntax_tag_init 		(GtkSyntaxTag       *text_tag);
static void		 gtk_syntax_tag_class_init 	(GtkSyntaxTagClass  *text_tag);
static void		 gtk_syntax_tag_finalize 	(GObject            *object);

static void		 gtk_pattern_tag_init 		(GtkPatternTag      *text_tag);
static void		 gtk_pattern_tag_class_init 	(GtkPatternTagClass *text_tag);
static void		 gtk_pattern_tag_finalize 	(GObject            *object);

static void 		 gtk_source_tag_set_property	(GObject            *object,
							 guint               prop_id,
							 const GValue       *value,
							 GParamSpec         *pspec);
static void 		 gtk_source_tag_get_property 	(GObject            *object,
							 guint               prop_id,
							 GValue             *value,
							 GParamSpec         *pspec);

enum {
	PROP_0,
	PROP_ID,
	PROP_TAG_STYLE
};

/* Source tag */

GType
gtk_source_tag_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceTagClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_source_tag_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceTag),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_tag_init
		};

		our_type =
		    g_type_register_static (GTK_TYPE_TEXT_TAG,
					    "GtkSourceTag", &our_info, 0);
	}

	return our_type;
}

static void
gtk_source_tag_class_init (GtkSourceTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize	= gtk_source_tag_finalize;

	object_class->set_property = gtk_source_tag_set_property;
	object_class->get_property = gtk_source_tag_get_property;
  
	/* Construct */
	g_object_class_install_property (object_class,
        	                         PROP_ID,
                                   	 g_param_spec_string ("id",
                                                        _("Tag ID"),
                                                        _("ID used to refer to the source tag"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
        	                         PROP_TAG_STYLE,
                                   	 g_param_spec_boxed ("tag_style",
                                                       _("Tag style"),
                                                       _("The style associated with the source tag"),
                                                       GTK_TYPE_SOURCE_TAG_STYLE,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

}

static void
gtk_source_tag_init (GtkSourceTag *text_tag)
{
	text_tag->id = NULL;
	text_tag->style = NULL;
}

static void
gtk_source_tag_finalize (GObject *object)
{
	GtkSourceTag *tag;

	tag = GTK_SOURCE_TAG (object);
	
	g_free (tag->style);
	g_free (tag->id);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtk_source_tag_get_style:
 * @tag: a #GtkSourceTag.
 * 
 * Gets the style associated with the given @tag.
 *
 * Return value: a #GtkSourceTagStyle if found, or %NULL if not found. 
 **/
GtkSourceTagStyle *
gtk_source_tag_get_style (GtkSourceTag *tag)
{
	g_return_val_if_fail (GTK_IS_SOURCE_TAG (tag), NULL);

	if (tag->style != NULL)
		return gtk_source_tag_style_copy (tag->style);
	else
		return NULL;
}

/**
 * gtk_source_tag_set_style:
 * @tag: a #GtkSourceTag.
 * @style: a #GtkSourceTagStyle.
 * 
 * Associates a given @style with the given @tag.
 **/
void 
gtk_source_tag_set_style (GtkSourceTag *tag, const GtkSourceTagStyle *style)
{
	GValue foreground = { 0, };
	GValue background = { 0, };

	g_return_if_fail (GTK_IS_SOURCE_TAG (tag));
	g_return_if_fail (style != NULL);

	/* Foreground color */
	g_value_init (&foreground, GDK_TYPE_COLOR);
	
	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_FOREGROUND) != 0)
		g_value_set_boxed (&foreground, &style->foreground);
	else
		g_value_set_boxed (&foreground, NULL);
	
	g_object_set_property (G_OBJECT (tag), "foreground_gdk", &foreground);

	/* Background color */
	g_value_init (&background, GDK_TYPE_COLOR);

	if ((style->mask & GTK_SOURCE_TAG_STYLE_USE_BACKGROUND) != 0)
		g_value_set_boxed (&background, &style->background);
	else
		g_value_set_boxed (&background, NULL);

	g_object_set_property (G_OBJECT (tag), "background_gdk", &background);
		
	g_object_set (G_OBJECT (tag), 
		      "style", style->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
		      "weight", style->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      "strikethrough", style->strikethrough,
		      "underline", style->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
		      NULL);

	g_free (tag->style);

	tag->style = g_new0 (GtkSourceTagStyle, 1);

	*tag->style = *style;
}


/* Syntax tag */

GType
gtk_syntax_tag_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkSyntaxTagClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_syntax_tag_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSyntaxTag),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_syntax_tag_init
		};

		our_type =
		    g_type_register_static (GTK_TYPE_SOURCE_TAG,
					    "GtkSyntaxTag", &our_info, 0);
	}

	return our_type;
}

static void
gtk_syntax_tag_class_init (GtkSyntaxTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_syntax_class	= g_type_class_peek_parent (klass);
	object_class->finalize	= gtk_syntax_tag_finalize;
}

static void
gtk_syntax_tag_init (GtkSyntaxTag *text_tag)
{
}

/**
 * gtk_syntax_tag_new:
 * @id: the ID for the tag.
 * @name: the name of the tag.
 * @pattern_start: the starting pattern.
 * @pattern_end: the ending pattern.
 *
 * Creates a new syntax tag object with the provided arguments.
 * 
 * Return value: a new syntax tag, as a #GtkTextTag.
 **/
GtkTextTag *
gtk_syntax_tag_new (const gchar *id,	
		    const gchar *name, 
		    const gchar *pattern_start,
		    const gchar *pattern_end)
{
	GtkSyntaxTag *tag;

	g_return_val_if_fail (pattern_start != NULL, NULL);
	g_return_val_if_fail (pattern_end != NULL, NULL);

	tag = GTK_SYNTAX_TAG (g_object_new (GTK_TYPE_SYNTAX_TAG, 
					    "id", id,
					    "name", name,
					    NULL));
	
	tag->start = g_strdup (pattern_start);

	tag->reg_start = gtk_source_regex_compile (pattern_start);
	if (tag->reg_start == NULL) {
		g_warning ("Regex syntax start pattern failed [%s]", pattern_start);
		g_object_unref (tag);
		return NULL;
	}
	
	tag->reg_end = gtk_source_regex_compile (pattern_end);
	if (tag->reg_end == NULL) {
		g_warning ("Regex syntax end pattern failed [%s]\n", pattern_end);
		g_object_unref (tag);
		return NULL;
	}

	return GTK_TEXT_TAG (tag);
}

static void
gtk_syntax_tag_finalize (GObject *object)
{
	GtkSyntaxTag *tag;

	tag = GTK_SYNTAX_TAG (object);
	
	g_free (tag->start);
	
	gtk_source_regex_destroy (tag->reg_start);
	gtk_source_regex_destroy (tag->reg_end);

	G_OBJECT_CLASS (parent_syntax_class)->finalize (object);
}


/* Pattern Tag */

GType
gtk_pattern_tag_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkPatternTagClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_pattern_tag_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkPatternTag),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_pattern_tag_init
		};

		our_type =
		    g_type_register_static (GTK_TYPE_SOURCE_TAG,
					    "GtkPatternTag", &our_info, 0);
	}
	return (our_type);
}

static void
gtk_pattern_tag_init (GtkPatternTag *text_tag)
{
}

static void
gtk_pattern_tag_class_init (GtkPatternTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_pattern_class	= g_type_class_peek_parent (klass);
	object_class->finalize	= gtk_pattern_tag_finalize;
}

/**
 * gtk_pattern_tag_new:
 * @id: the ID for the tag.
 * @name: the name of the tag.
 * @pattern: the pattern.
 *
 * Creates a new pattern tag object with the provided arguments.
 * 
 * Return value: a new pattern tag, as a #GtkTextTag.
 **/
GtkTextTag *
gtk_pattern_tag_new (const gchar *id, 
		     const gchar *name, 
		     const gchar *pattern)
{
	GtkPatternTag *tag;

	g_return_val_if_fail (pattern != NULL, NULL);

	tag = GTK_PATTERN_TAG (g_object_new (GTK_TYPE_PATTERN_TAG, 
					     "id", id,
					     "name", name,
					     NULL));
	
	tag->reg_pattern = gtk_source_regex_compile (pattern);
	if (tag->reg_pattern == NULL) {
		g_warning ("Regex pattern failed [%s]\n", pattern);
		g_object_unref (tag);
		return NULL;
	}

	return GTK_TEXT_TAG (tag);
}

static void
gtk_pattern_tag_finalize (GObject *object)
{
	GtkPatternTag *tag;

	tag = GTK_PATTERN_TAG (object);
	
	gtk_source_regex_destroy (tag->reg_pattern);
	
	G_OBJECT_CLASS (parent_pattern_class)->finalize (object);
}


static gchar *
case_insesitive_keyword (const gchar *keyword)
{
	GString *str;
	gint length;
	
	const gchar *cur;
	const gchar *end;

	g_return_val_if_fail (keyword != NULL, NULL);

	length = strlen (keyword);

	str = g_string_new ("");

	cur = keyword;
	end = keyword + length;
	
	while (cur != end) 
	{
		gunichar cur_char;
		cur_char = g_utf8_get_char (cur);
		
		if (((cur_char >= 'A') && (cur_char <= 'Z')) ||
		    ((cur_char >= 'a') && (cur_char <= 'z')))
		{
			gunichar cur_char_upper;
		       	gunichar cur_char_lower;
	
			cur_char_upper = g_unichar_toupper (cur_char);
			cur_char_lower = g_unichar_tolower (cur_char);
		
			g_string_append_c (str, '[');
			g_string_append_unichar (str, cur_char_lower);
			g_string_append_unichar (str, cur_char_upper);
			g_string_append_c (str, ']');
		}
		else
			g_string_append_unichar (str, cur_char);

		cur = g_utf8_next_char (cur);
	}
			
	return g_string_free (str, FALSE);
}
 
/**
 * gtk_keyword_list_tag_new:
 * @id: the ID for the tag.
 * @name: the name of the tag.
 * @keywords: a list of keywords.
 * @case_sensitive: whether the tag should be case sensitive.
 * @match_empty_string_at_beginning: whether the tag should match empty
 * string at the beginning.
 * @match_empty_string_at_end: whether the tag should match empty
 * string at the end. 
 * @beginning_regex: the beginning regular expression.
 * @end_regex: the ending regular expression.

 * Creates a new keyword list tag object with the provided arguments.
 * 
 * Return value: a new keyword list tag, as a #GtkTextTag.
 **/
GtkTextTag *
gtk_keyword_list_tag_new (const gchar  *id,
			  const gchar  *name, 
			  const GSList *keywords,
			  gboolean      case_sensitive,
			  gboolean      match_empty_string_at_beginning,
			  gboolean      match_empty_string_at_end,
			  const gchar  *beginning_regex,
			  const gchar  *end_regex)
{
	
	GtkTextTag *tag;
	GString *str;
	gint keyword_count;
	
	g_return_val_if_fail (keywords != NULL, NULL);

	str =  g_string_new ("");

	if (keywords != NULL)
	{
		if (match_empty_string_at_beginning)
			g_string_append (str, "\\b");

		if (beginning_regex != NULL)
			g_string_append (str, beginning_regex);

		g_string_append (str, "(");
	
		keyword_count = 0;
		/* Due to a bug in GNU libc regular expressions
		 * implementation we can't have keyword lists of more
		 * than 250 or so elements, so we truncate such a
		 * list.  This is a temporary solution, as the correct
		 * approach would be to generate multiple keyword
		 * lists.  (See bug #110991) */

#define KEYWORD_LIMIT 250

		while (keywords != NULL && keyword_count < KEYWORD_LIMIT)
		{
			gchar *k;
			
			if (case_sensitive)
				k = (gchar*)keywords->data;
			else
				k = case_insesitive_keyword ((gchar*)keywords->data);
			
			g_string_append (str, k);
			
			if (!case_sensitive)
				g_free (k);
			
			keywords = g_slist_next (keywords);
			
			keyword_count++;
			
			if (keywords != NULL && keyword_count < KEYWORD_LIMIT)
				g_string_append (str, "|");
		}
		g_string_append (str, ")");
		
		if (keyword_count >= KEYWORD_LIMIT)
		{
			g_warning ("Keyword list '%s' too long. Only the first %d "
				   "elements will be highlighted. See bug #110991 for "
				   "further details.", id, KEYWORD_LIMIT);
		}

		if (end_regex != NULL)
			g_string_append (str, end_regex);
		
		if (match_empty_string_at_end)
			g_string_append (str, "\\b");
	}

	tag = gtk_pattern_tag_new (id, name, str->str);

	g_string_free (str, TRUE);
	
	return tag;
}

/**
 * gtk_line_comment_tag_new:
 * @id: the ID for the tag.
 * @name: the name of the tag.
 * @pattern_start: the starting pattern.
 *
 * Creates a new line comment tag object with the provided arguments.
 * 
 * Return value: a new line comment tag, as a #GtkTextTag.
 **/
GtkTextTag *
gtk_line_comment_tag_new (const gchar *id, 
			  const gchar *name, 
			  const gchar *pattern_start)
{
	g_return_val_if_fail (pattern_start != NULL, NULL);

	return gtk_syntax_tag_new (id, name, pattern_start, "\n");
}

/**
 * gtk_string_tag_new:
 * @id: the ID for the tag.
 * @name: the name of the tag.
 * @pattern_start: the starting pattern.
 * @pattern_end: the ending pattern.
 * @end_at_line_end: whether the ending pattern should be suffixed by an end-of-line character.
 *
 * Creates a new string tag object with the provided arguments.
 * 
 * Return value: a new string tag, as a #GtkTextTag.
 **/
GtkTextTag *
gtk_string_tag_new (const gchar    *id,
		    const gchar    *name,
	            const gchar    *pattern_start,
		    const gchar    *pattern_end,
		    gboolean        end_at_line_end)
{
	g_return_val_if_fail (pattern_start != NULL, NULL);
	g_return_val_if_fail (pattern_end != NULL, NULL);

	if (!end_at_line_end)
		return gtk_syntax_tag_new (id, name, pattern_start, pattern_end);
	else
	{
		GtkTextTag *tag;
		gchar *end;
		
		end = g_strdup_printf ("%s|\n", pattern_end);

		tag = gtk_syntax_tag_new (id, name, pattern_start, end);

		g_free (end);

		return tag;
	}
}

static void
gtk_source_tag_set_property (GObject            *object,
			     guint               prop_id,
			     const GValue       *value,
			     GParamSpec         *pspec)
{
	GtkSourceTag *tag;

	g_return_if_fail (GTK_IS_SOURCE_TAG (object));

	tag = GTK_SOURCE_TAG (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_return_if_fail (tag->id == NULL);
			tag->id = g_strdup (g_value_get_string (value));
			break;

		case PROP_TAG_STYLE:
			{
				const GtkSourceTagStyle *style;

				style = g_value_get_boxed (value);

				if (style != NULL)
					gtk_source_tag_set_style (tag, style);
			}
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    	}
}

static void
gtk_source_tag_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
	GtkSourceTag *tag;

	g_return_if_fail (GTK_IS_SOURCE_TAG (object));

	tag = GTK_SOURCE_TAG (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, tag->id);
			break;

		case PROP_TAG_STYLE:
			{
				GtkSourceTagStyle *style;
				
				style = gtk_source_tag_get_style (tag);

				g_value_set_boxed (value, style);

				if (style != NULL)
					gtk_source_tag_style_free (style);
				
				break;
			}
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

gchar *
gtk_source_tag_get_id (GtkSourceTag *tag)
{
	g_return_val_if_fail (GTK_IS_SOURCE_TAG (tag), NULL);
	g_return_val_if_fail (tag->id != NULL, NULL);

	return g_strdup (tag->id);
}
	

/* GtkSourceTagStyle functions ------------- */

/**
 * gtk_source_tag_style_get_type:
 * 
 * Retrieves the GType object which is associated with the
 * #GtkSourceTagStyle class.
 * 
 * Return value: the GType associated with #GtkSourceTagStyle.
 **/
GType 
gtk_source_tag_style_get_type (void)
{
	static GType our_type = 0;

	if (!our_type)
		our_type = g_boxed_type_register_static (
			"GtkSourceTagStyle",
			(GBoxedCopyFunc) gtk_source_tag_style_copy,
			(GBoxedFreeFunc) gtk_source_tag_style_free);

	return our_type;
} 

/**
 * gtk_source_tag_style_new:
 * 
 * Creates a new tag style object.
 * 
 * Return value: a new #GtkSourceTagStyle.
 **/
GtkSourceTagStyle *
gtk_source_tag_style_new (void)
{
	GtkSourceTagStyle *style;

	style = g_new0 (GtkSourceTagStyle, 1);

	return style;
}

/**
 * gtk_source_tag_style_copy:
 * @style: a #GtkSourceTagStyle.
 * 
 * Makes a copy of the given @style.
 * 
 * Return value: a new #GtkSourceTagStyle.
 **/
GtkSourceTagStyle *
gtk_source_tag_style_copy (const GtkSourceTagStyle *style)
{
	GtkSourceTagStyle *new_style;

	g_return_val_if_fail (style != NULL, NULL);
	
	new_style = gtk_source_tag_style_new ();
	*new_style = *style;

	return new_style;
}


/**
 * gtk_source_tag_style_free:
 * @style: a #GtkSourceTagStyle.
 * 
 * Frees the resources allocated by the given @style.
 * 
 **/
void 
gtk_source_tag_style_free (GtkSourceTagStyle *style)
{
	g_return_if_fail (style != NULL);
	
	g_free (style);
}



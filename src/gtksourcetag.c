/*  gtksourcetag.c
 *
 *  Copyright (C) 2001
 *  Mikael Hermansson<tyan@linux.se>
 *  Chris Phelps <chicane@reninet.com>
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
 *  You should have received a copy of the GNU Library General Public License*  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include "gtksourcetag.h"

static GObjectClass *parent_syntax_class = NULL;
static GObjectClass *parent_pattern_class = NULL;
static GObjectClass *parent_embedded_class = NULL;

static void gtk_syntax_tag_init (GtkSyntaxTag *text_tag);
static void gtk_syntax_tag_class_init (GtkSyntaxTagClass *text_tag);
static void gtk_syntax_tag_finalize (GObject *object);

static void gtk_pattern_tag_init (GtkPatternTag *text_tag);
static void gtk_pattern_tag_class_init (GtkPatternTagClass *text_tag);
static void gtk_pattern_tag_finalize (GObject *object);

static void gtk_embedded_tag_init (GtkEmbeddedTag *text_tag);
static void gtk_embedded_tag_class_init (GtkEmbeddedTagClass *text_tag);
static void gtk_embedded_tag_finalize (GObject *object);


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

		our_type = g_type_register_static (GTK_TYPE_TEXT_TAG, "GtkSyntaxTag", &our_info, 0);
	}
	return our_type;
}

static void
gtk_syntax_tag_class_init (GtkSyntaxTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_syntax_class = g_type_class_peek_parent (klass);
	object_class->finalize = gtk_syntax_tag_finalize;
}

static void
gtk_syntax_tag_init (GtkSyntaxTag *text_tag)
{
	GTK_TEXT_TAG (text_tag)->table = NULL;
}

GtkTextTag *
gtk_syntax_tag_new (const gchar *name, const gchar *patternstart, const gchar *patternend)
{
	GtkSyntaxTag *tag;

	tag = GTK_SYNTAX_TAG (g_object_new (gtk_syntax_tag_get_type (), "name", name, NULL));
	tag->start = g_strdup (patternstart);
	if (!gtk_source_compile_regex (patternstart, &tag->reg_start))
		g_print ("Regex syntax start pattern failed [%s]\n", patternstart);
	if (!gtk_source_compile_regex (patternend, &tag->reg_end))
		g_print ("Regex syntax end pattern failed [%s]\n", patternend);
	return GTK_TEXT_TAG (tag);
}

static void
gtk_syntax_tag_finalize (GObject *object)
{
	GtkSyntaxTag *tag;

	tag = GTK_SYNTAX_TAG (object);
	if (tag->start)
		g_free (tag->start);
	g_free (tag->reg_start.buf.fastmap);
	tag->reg_start.buf.fastmap = NULL;
	regfree (&tag->reg_start.buf);
	g_free (tag->reg_end.buf.fastmap);
	tag->reg_end.buf.fastmap = NULL;
	regfree (&tag->reg_end.buf);
	(*G_OBJECT_CLASS (parent_syntax_class)->finalize) (object);
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
			g_type_register_static (GTK_TYPE_TEXT_TAG, "GtkPatternTag", &our_info, 0);
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

	parent_pattern_class = g_type_class_peek_parent (klass);
	object_class->finalize = gtk_pattern_tag_finalize;
}

GtkTextTag *
gtk_pattern_tag_new (const gchar *name, const gchar *pattern)
{
	GtkPatternTag *tag;

	tag = GTK_PATTERN_TAG (g_object_new (gtk_pattern_tag_get_type (), "name", name, NULL));
	if (!gtk_source_compile_regex (pattern, &tag->reg_pattern))
		g_print ("Regex pattern failed [%s]\n", pattern);
	return GTK_TEXT_TAG (tag);
}

void
gtk_pattern_tag_finalize (GObject *object)
{
	GtkPatternTag *tag;

	tag = GTK_PATTERN_TAG (object);
	g_free (tag->reg_pattern.buf.fastmap);
	tag->reg_pattern.buf.fastmap = NULL;
	regfree (&tag->reg_pattern.buf);
	(*G_OBJECT_CLASS (parent_pattern_class)->finalize) (object);
}

/* Embedded Tags */

GType
gtk_embedded_tag_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GtkEmbeddedTagClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gtk_embedded_tag_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkEmbeddedTag),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_embedded_tag_init
		};

		our_type =
			g_type_register_static (GTK_TYPE_TEXT_TAG, "GtkEmbeddedTag", &our_info, 0);
	}
	return our_type;
}

static void
gtk_embedded_tag_class_init (GtkEmbeddedTagClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_embedded_class = g_type_class_peek_parent (klass);
	object_class->finalize = gtk_embedded_tag_finalize;
}

static void
gtk_embedded_tag_init (GtkEmbeddedTag *text_tag)
{

}

GtkTextTag *
gtk_embedded_tag_new (const gchar *name, const gchar *outside, const gchar *inside)
{
	GtkEmbeddedTag *tag;

	tag = GTK_EMBEDDED_TAG (g_object_new (gtk_syntax_tag_get_type (), "name", name, NULL));
	if (!gtk_source_compile_regex (outside, &tag->reg_outside))
		g_print ("Regex embedded outside pattern failed [%s]\n", outside);
	if (!gtk_source_compile_regex (inside, &tag->reg_inside))
		g_print ("Regex embedded inside pattern failed [%s]\n", inside);
	return GTK_TEXT_TAG (tag);
}

static void
gtk_embedded_tag_finalize (GObject *object)
{
	GtkEmbeddedTag *tag;

	tag = GTK_EMBEDDED_TAG (object);
	g_free (tag->reg_outside.buf.fastmap);
	tag->reg_outside.buf.fastmap = NULL;
	regfree (&tag->reg_outside.buf);
	g_free (tag->reg_inside.buf.fastmap);
	tag->reg_inside.buf.fastmap = NULL;
	regfree (&tag->reg_inside.buf);
	(*G_OBJECT_CLASS (parent_embedded_class)->finalize) (object);
}

/* Generic Functions */

gboolean
gtk_source_compile_regex (const gchar *pattern, Regex *regex)
{
	if (!pattern)
		return (FALSE);
	memset (&regex->buf, 0, sizeof (regex->buf));
	regex->len = strlen (pattern);
	regex->buf.translate = NULL;
	regex->buf.fastmap = g_malloc (256);
	regex->buf.allocated = 0;
	regex->buf.buffer = NULL;
	regex->buf.can_be_null = 0;	/* so we wont allow that in patterns! */
	regex->buf.no_sub = 0;
	if (re_compile_pattern (pattern, strlen (pattern), &regex->buf) == 0) {
		/* success...now try to compile a fastmap */
		if (re_compile_fastmap (&regex->buf) != 0) {
			g_warning ("IMPORTANT REGEX FAILED TO CREASTE FASTMAP\n");
			/* error...no fastmap */
			g_free (regex->buf.fastmap);
			regex->buf.fastmap = NULL;
		}
		return (TRUE);
	} else {
		g_warning ("IMPORTANT REGEX FAILED TO COMPILE\n");
		return (FALSE);
	}
}

/*  gtksourcetag.h
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

#ifndef __GTK_SOURCE_TAG_H__
#define __GTK_SOURCE_TAG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <regex.h>
#include <gtk/gtk.h>
#include <gtk/gtktexttag.h>

#define GTK_TYPE_SYNTAX_TAG            (gtk_syntax_tag_get_type ())
#define GTK_SYNTAX_TAG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTag))
#define GTK_SYNTAX_TAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTagClass))
#define GTK_IS_SYNTAX_TAG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SYNTAX_TAG))
#define GTK_IS_SYNTAX_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SYNTAX_TAG))
#define GTK_SYNTAX_TAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTagClass))

#define GTK_TYPE_PATTERN_TAG            (gtk_pattern_tag_get_type ())
#define GTK_PATTERN_TAG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATTERN_TAG, GtkPatternTag))
#define GTK_PATTERN_TAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATTERN_TAG, GtkPatternTagClass))
#define GTK_IS_PATTERN_TAG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATTERN_TAG))
#define GTK_IS_PATTERN_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATTERN_TAG))
#define GTK_PATTERN_TAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATTERN_TAG, GtkPatternTagClass))

#define GTK_TYPE_EMBEDDED_TAG            (gtk_embedded_tag_get_type ())
#define GTK_EMBEDDED_TAG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_EMBEDDED_TAG, GtkEmbeddedTag))
#define GTK_EMBEDDED_TAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_EMBEDDED_TAG, GtkEmbeddedTagClass))
#define GTK_IS_EMBEDDED_TAG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_EMBEDDED_TAG))
#define GTK_IS_EMBEDDED_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_EMBEDDED_TAG))
#define GTK_EMBEDDED_TAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EMBEDDED_TAG, GtkEmbeddedTagClass))

typedef struct _GtkSourceBufferMatch
{
  gint startpos;
  gint endpos;
}GtkSourceBufferMatch;

/* FIXME: regex routines is NOT UTF8 compat... */

typedef struct _Regex {
	struct re_pattern_buffer buf;
	struct re_registers reg;
	gint len;
}Regex;

typedef struct _GtkSyntaxTag GtkSyntaxTag;
typedef struct _GtkSyntaxTagClass GtkSyntaxTagClass;
typedef struct _GtkPatternTag GtkPatternTag;
typedef struct _GtkPatternTagClass GtkPatternTagClass;
typedef struct _GtkEmbeddedTag GtkEmbeddedTag;
typedef struct _GtkEmbeddedTagClass GtkEmbeddedTagClass;

struct _GtkSyntaxTag {
	GtkTextTag parent_instance;
	gchar *start;  
	Regex reg_start;
	Regex reg_end;
};

struct _GtkSyntaxTagClass {
	GtkTextTagClass parent_class; 
};

struct _GtkPatternTag {
	GtkTextTag parent_instance;
	Regex reg_pattern;
};

struct _GtkPatternTagClass {
	GtkTextTagClass parent_class; 
};

struct _GtkEmbeddedTag {
	GtkTextTag parent_instance;
	Regex reg_outside;
	Regex reg_inside;
};

struct _GtkEmbeddedTagClass {
	GtkTextTagClass parent_class; 
};


GType      gtk_syntax_tag_get_type(void) G_GNUC_CONST;
GtkTextTag* gtk_syntax_tag_new(const gchar *name, const gchar *patternstart,
			       const gchar *patternend);

GType      gtk_pattern_tag_get_type(void) G_GNUC_CONST;
GtkTextTag* gtk_pattern_tag_new(const gchar *name, const gchar *pattern);

GType      gtk_embedded_tag_get_type(void) G_GNUC_CONST;
GtkTextTag* gtk_embedded_tag_new(const gchar *name, const gchar *outside,
				 const gchar *inside);

gboolean gtk_source_compile_regex (const gchar *pattern, Regex *regex);

#ifdef __cplusplus
};
#endif

#endif

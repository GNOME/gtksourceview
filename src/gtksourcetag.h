/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 *  gtksourcetag.h
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

#ifndef __GTK_SOURCE_TAG_H__
#define __GTK_SOURCE_TAG_H__

#include <gtk/gtktexttag.h>
#include <gtksourcetagstyle.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_TAG             (gtk_source_tag_get_type ())
#define GTK_SOURCE_TAG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTag))
#define GTK_SOURCE_TAG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))
#define GTK_IS_SOURCE_TAG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_TAG))
#define GTK_IS_SOURCE_TAG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_TAG))
#define GTK_SOURCE_TAG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_TAG, GtkSourceTagClass))

#define GTK_TYPE_SYNTAX_TAG             (gtk_syntax_tag_get_type ())
#define GTK_SYNTAX_TAG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTag))
#define GTK_SYNTAX_TAG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTagClass))
#define GTK_IS_SYNTAX_TAG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SYNTAX_TAG))
#define GTK_IS_SYNTAX_TAG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SYNTAX_TAG))
#define GTK_SYNTAX_TAG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SYNTAX_TAG, GtkSyntaxTagClass))

#define GTK_TYPE_PATTERN_TAG            (gtk_pattern_tag_get_type ())
#define GTK_PATTERN_TAG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATTERN_TAG, GtkPatternTag))
#define GTK_PATTERN_TAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATTERN_TAG, GtkPatternTagClass))
#define GTK_IS_PATTERN_TAG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATTERN_TAG))
#define GTK_IS_PATTERN_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATTERN_TAG))
#define GTK_PATTERN_TAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATTERN_TAG, GtkPatternTagClass))


typedef struct _GtkSourceTag        GtkSourceTag;
typedef struct _GtkSourceTagClass   GtkSourceTagClass;

typedef struct _GtkSyntaxTag        GtkSyntaxTag;
typedef struct _GtkSyntaxTagClass   GtkSyntaxTagClass;

typedef struct _GtkPatternTag       GtkPatternTag;
typedef struct _GtkPatternTagClass  GtkPatternTagClass;


GType 	           gtk_source_tag_get_type	(void) G_GNUC_CONST;
GtkSourceTagStyle *gtk_source_tag_get_style	(GtkSourceTag            *tag);
void               gtk_source_tag_set_style	(GtkSourceTag            *tag,
						 const GtkSourceTagStyle *style);

GType              gtk_syntax_tag_get_type	(void) G_GNUC_CONST;
GtkTextTag        *gtk_syntax_tag_new		(const gchar 	*name, 
						 const gchar 	*pattern_start,
						 const gchar 	*pattern_end);

GType              gtk_pattern_tag_get_type	(void) G_GNUC_CONST;
GtkTextTag        *gtk_pattern_tag_new		(const gchar 	*name, 
						 const gchar 	*pattern);

GtkTextTag        *gtk_keyword_list_tag_new	(const gchar 	*name, 
						 const GSList 	*keywords,
						 gboolean	 case_sensitive,
						 gboolean	 match_empty_string_at_beginning,
						 gboolean	 match_empty_string_at_end,
						 const gchar    *beginning_regex,
						 const gchar    *end_regex);

#define gtk_block_comment_tag_new	gtk_syntax_tag_new

GtkTextTag        *gtk_line_comment_tag_new	(const gchar    *name,
						 const gchar    *pattern_start);

GtkTextTag        *gtk_string_tag_new		(const gchar    *name,
						 const gchar    *pattern_start,
						 const gchar	*pattern_end,
						 gboolean        end_at_line_end);

G_END_DECLS

#endif

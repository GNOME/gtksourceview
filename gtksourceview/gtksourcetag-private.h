/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- 
 *  gtksourcetag-private.h
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

#ifndef __GTK_SOURCE_TAG_PRIVATE_H__
#define __GTK_SOURCE_TAG_PRIVATE_H__

#include <gtk/gtktexttag.h>

#include <gtksourceview/gtksourceregex.h>
#include <gtksourceview/gtksourcetagstyle.h>

G_BEGIN_DECLS

struct _GtkSourceTag 
{
	GtkTextTag		 parent_instance;

	gchar			*id;
	GtkSourceTagStyle	*style;
};

struct _GtkSourceTagClass 
{
	GtkTextTagClass		 parent_class; 
};

struct _GtkSyntaxTag 
{
	GtkSourceTag		 parent_instance;

	gchar			*start;  
	GtkSourceRegex		*reg_start;
	GtkSourceRegex          *reg_end;
};

struct _GtkSyntaxTagClass 
{
	GtkSourceTagClass	 parent_class; 
};

struct _GtkPatternTag 
{
	GtkSourceTag		 parent_instance;

	GtkSourceRegex		*reg_pattern;
};

struct _GtkPatternTagClass 
{
	GtkSourceTagClass	 parent_class; 
};

G_END_DECLS

#endif

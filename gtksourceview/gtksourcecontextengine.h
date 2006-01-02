/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcecontextengine.h
 *
 *  Copyright (C) 2003 - Gustavo Giráldez
 *  Copyright (C) 2005 - Marco Barisione, Emanuele Aina
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_SOURCE_CONTEXT_ENGINE_H__
#define __GTK_SOURCE_CONTEXT_ENGINE_H__

#include <gtksourceview/gtksourceengine.h>
#include "libegg/regex/eggregex.h"

G_BEGIN_DECLS

/* FIXME: cut and pasted here: need a reorganization. */

enum _ContextType
{
	CONTEXT_TYPE_SIMPLE = 0,
	CONTEXT_TYPE_CONTAINER,
};
typedef enum _ContextType ContextType;

#define GTK_SOURCE_CONTEXT_ENGINE_ERROR gtk_source_context_engine_error_quark ()

typedef enum {
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_DUPLICATED_ID = 0,
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_ARGS,
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_PARENT,
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_REF,
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_WHERE,
	GTK_SOURCE_CONTEXT_ENGINE_ERROR_INVALID_START_REF,
} GtkSourceContextEngineError;

#define GTK_TYPE_SOURCE_CONTEXT_ENGINE            (gtk_source_context_engine_get_type ())
#define GTK_SOURCE_CONTEXT_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_CONTEXT_ENGINE, GtkSourceContextEngine))
#define GTK_SOURCE_CONTEXT_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_CONTEXT_ENGINE, GtkSourceContextEngineClass))
#define GTK_IS_SOURCE_CONTEXT_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_CONTEXT_ENGINE))
#define GTK_IS_SOURCE_CONTEXT_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_CONTEXT_ENGINE))
#define GTK_SOURCE_CONTEXT_ENGINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_CONTEXT_ENGINE, GtkSourceContextEngineClass))

typedef struct _GtkSourceContextEngine        GtkSourceContextEngine;
typedef struct _GtkSourceContextEngineClass   GtkSourceContextEngineClass;

struct _GtkSourceContextEngine 
{
	GtkSourceEngine engine;
};

struct _GtkSourceContextEngineClass 
{
	GtkSourceEngineClass parent_class;
};

GType		 gtk_source_context_engine_get_type	(void) G_GNUC_CONST;

GtkSourceEngine	*gtk_source_context_engine_new		(const gchar *id);

gboolean	 gtk_source_context_engine_define_context
							(GtkSourceContextEngine	 *ce,
							 gchar			 *id,
							 gchar			 *parent_id,
							 gchar			 *match_regex,
							 gchar			 *start_regex,
							 gchar			 *end_regex,
							 gchar			 *style,
							 gboolean		  extend_parent,
							 gboolean		  end_at_line_end,
							 gboolean		  foldable,
							 GError			**error);

gboolean	 gtk_source_context_engine_add_sub_pattern
							(GtkSourceContextEngine	 *ce,
							 gchar			 *id,
							 gchar			 *parent_id,
							 gchar			 *name,
							 gchar			 *where,
							 gchar			 *style,
							 GError			**error);

gboolean	 gtk_source_context_engine_add_ref 	(GtkSourceContextEngine	 *ce,
							 gchar			 *parent_id,
							 gchar			 *ref_id,
							 gboolean		  all,
							 GError			**error);

GQuark		 gtk_source_context_engine_error_quark	(void);

G_END_DECLS

#endif /* __GTK_SOURCE_CONTEXT_ENGINE_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *  gtksourcesimpleengine.h
 *
 *  Copyright (C) 2003 - Gustavo Giráldez
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

#ifndef __GTK_SOURCE_SIMPLE_ENGINE_H__
#define __GTK_SOURCE_SIMPLE_ENGINE_H__

#include <gtksourceview/gtksourceengine.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_SIMPLE_ENGINE            (gtk_source_simple_engine_get_type ())
#define GTK_SOURCE_SIMPLE_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_SIMPLE_ENGINE, GtkSourceSimpleEngine))
#define GTK_SOURCE_SIMPLE_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_SIMPLE_ENGINE, GtkSourceSimpleEngineClass))
#define GTK_IS_SOURCE_SIMPLE_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_SIMPLE_ENGINE))
#define GTK_IS_SOURCE_SIMPLE_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_SIMPLE_ENGINE))
#define GTK_SOURCE_SIMPLE_ENGINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_SIMPLE_ENGINE, GtkSourceSimpleEngineClass))

typedef struct _GtkSourceSimpleEngine        GtkSourceSimpleEngine;
typedef struct _GtkSourceSimpleEngineClass   GtkSourceSimpleEngineClass;
typedef struct _GtkSourceSimpleEnginePrivate GtkSourceSimpleEnginePrivate;

struct _GtkSourceSimpleEngine 
{
	GtkSourceEngine engine;

	GtkSourceSimpleEnginePrivate *priv;
};

struct _GtkSourceSimpleEngineClass 
{
	GtkSourceEngineClass parent_class;
};

GType           	 gtk_source_simple_engine_get_type 		(void) G_GNUC_CONST;

GtkSourceEngine         *gtk_source_simple_engine_new                   (void);

G_END_DECLS

#endif /* __GTK_SOURCE_SIMPLE_ENGINE_H__ */

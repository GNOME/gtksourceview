/*
 * c-indenter.h
 * This file is part of gedit
 *
 * Copyright (C) 2009 - Ignacio Casal Quinteiro
 *
 * gedit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * gedit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with gedit; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __SIMPLE_INDENTER_H__
#define __SIMPLE_INDENTER_H__

#include <glib-object.h>
#include "gtksourceindenter.h"

G_BEGIN_DECLS

#define SIMPLE_TYPE_INDENTER			(simple_indenter_get_type ())
#define SIMPLE_INDENTER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), SIMPLE_TYPE_INDENTER, SimpleIndenter))
#define SIMPLE_INDENTER_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SIMPLE_TYPE_INDENTER, SimpleIndenter const))
#define SIMPLE_INDENTER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), SIMPLE_TYPE_INDENTER, SimpleIndenterClass))
#define SIMPLE_IS_INDENTER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SIMPLE_TYPE_INDENTER))
#define SIMPLE_IS_INDENTER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SIMPLE_TYPE_INDENTER))
#define SIMPLE_INDENTER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), SIMPLE_TYPE_INDENTER, SimpleIndenterClass))

typedef struct _SimpleIndenter		SimpleIndenter;
typedef struct _SimpleIndenterClass	SimpleIndenterClass;
typedef struct _SimpleIndenterPrivate	SimpleIndenterPrivate;

struct _SimpleIndenter
{
	GtkSourceIndenter parent;
	
	SimpleIndenterPrivate *priv;
};

struct _SimpleIndenterClass
{
	GtkSourceIndenterClass parent_class;
};

GType simple_indenter_get_type (void) G_GNUC_CONST;
SimpleIndenter *simple_indenter_new (void);


G_END_DECLS

#endif /* __SIMPLE_INDENTER_H__ */

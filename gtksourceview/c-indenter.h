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

#ifndef __C_INDENTER_H__
#define __C_INDENTER_H__

#include <glib-object.h>
#include "gtksourceindenter.h"

G_BEGIN_DECLS

#define C_TYPE_INDENTER			(c_indenter_get_type ())
#define C_INDENTER(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), C_TYPE_INDENTER, CIndenter))
#define C_INDENTER_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), C_TYPE_INDENTER, CIndenter const))
#define C_INDENTER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), C_TYPE_INDENTER, CIndenterClass))
#define C_IS_INDENTER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), C_TYPE_INDENTER))
#define C_IS_INDENTER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), C_TYPE_INDENTER))
#define C_INDENTER_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), C_TYPE_INDENTER, CIndenterClass))

typedef struct _CIndenter		CIndenter;
typedef struct _CIndenterClass		CIndenterClass;
typedef struct _CIndenterPrivate	CIndenterPrivate;

struct _CIndenter
{
	GtkSourceIndenter parent;
	
	CIndenterPrivate *priv;
};

struct _CIndenterClass
{
	GtkSourceIndenterClass parent_class;
};

GType c_indenter_get_type (void) G_GNUC_CONST;
CIndenter *c_indenter_new (void);


G_END_DECLS

#endif /* __C_INDENTER_H__ */

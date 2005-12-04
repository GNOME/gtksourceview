/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- 
 *
 *  gtksourceengine.c - Abstract base class for highlighting engines
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtksourcebuffer.h"
#include "gtksourceengine.h"

static void gtk_source_engine_class_init (GtkSourceEngineClass *klass);
static void gtk_source_engine_init (GtkSourceEngine *engine);

static GObjectClass *parent_class = NULL;

GType
gtk_source_engine_get_type (void)
{
	static GType our_type = 0;

	if (!our_type) {
		static const GTypeInfo our_info = {
			sizeof (GtkSourceEngineClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) gtk_source_engine_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (GtkSourceEngine),
			0,	/* n_preallocs */
			(GInstanceInitFunc) gtk_source_engine_init
		};

		our_type = g_type_register_static (G_TYPE_OBJECT,
						   "GtkSourceEngine",
						   &our_info, 
						   0);
	}
	
	return our_type;
}
	
static void
gtk_source_engine_class_init (GtkSourceEngineClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	klass->attach_buffer = NULL;
}

static void
gtk_source_engine_init (GtkSourceEngine *engine)
{
}

void 
gtk_source_engine_attach_buffer (GtkSourceEngine *engine,
				 GtkSourceBuffer *buffer)
{
	g_return_if_fail (engine != NULL);
	g_return_if_fail (GTK_IS_SOURCE_ENGINE (engine));
	g_return_if_fail (GTK_SOURCE_ENGINE_GET_CLASS (engine)->attach_buffer);

	GTK_SOURCE_ENGINE_GET_CLASS (engine)->attach_buffer (engine, buffer);
}


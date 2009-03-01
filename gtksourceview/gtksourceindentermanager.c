/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 *  gtksourceindentermanager.c
 *
 *  Copyright (C) 2009 - Ignacio Casal Quinteiro <icq@gnome.org>
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

#include "gtksourceindentermanager.h"
#include "c-indenter.h"
#include "simple-indenter.h"
#include <string.h>

#define GTK_SOURCE_INDENTER_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_INDENTER_MANAGER, GtkSourceIndenterManagerPrivate))

struct _GtkSourceIndenterManagerPrivate
{
};

G_DEFINE_TYPE (GtkSourceIndenterManager, gtk_source_indenter_manager, G_TYPE_OBJECT)

static void
gtk_source_indenter_manager_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_indenter_manager_parent_class)->finalize (object);
}

static void
gtk_source_indenter_manager_class_init (GtkSourceIndenterManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = gtk_source_indenter_manager_finalize;

	//g_type_class_add_private (object_class, sizeof(GtkSourceIndenterManagerPrivate));
}

static void
gtk_source_indenter_manager_init (GtkSourceIndenterManager *self)
{
	//self->priv = GTK_SOURCE_INDENTER_MANAGER_GET_PRIVATE (self);
}

GtkSourceIndenterManager *
gtk_source_indenter_manager_get_default ()
{
	static GtkSourceIndenterManager *manager = NULL;

	if (manager == NULL)
	{
		manager = g_object_new (GTK_TYPE_SOURCE_INDENTER_MANAGER, NULL);
	}

	return manager;
}

GtkSourceIndenter *
gtk_source_indenter_manager_get_indenter_by_id (GtkSourceIndenterManager *manager,
						const gchar *id)
{
	GtkSourceIndenter *indenter;

	if (id != NULL && strcmp (id, "c") == 0)
	{
		indenter = GTK_SOURCE_INDENTER (c_indenter_new ());
	}
	else
	{
		indenter = GTK_SOURCE_INDENTER (simple_indenter_new ());
	}

	return indenter;
}

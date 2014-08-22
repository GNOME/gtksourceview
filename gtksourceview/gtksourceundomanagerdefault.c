/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gtksourceundomanagerdefault.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 1998, 1999 - Alex Roberts, Evan Lawrence
 * Copyright (C) 2000, 2001 - Chema Celorio, Paolo Maggi
 * Copyright (C) 2002-2005  - Paolo Maggi
 * Copyright (C) 2014 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourceundomanagerdefault.h"
#include "gtksourceundomanager.h"

typedef struct _Action		Action;
typedef struct _ActionInsert	ActionInsert;
typedef struct _ActionDelete	ActionDelete;
typedef struct _ActionGroup	ActionGroup;

typedef enum
{
	ACTION_TYPE_INSERT,
	ACTION_TYPE_DELETE
} ActionType;

/* We use character offsets instead of GtkTextMarks because the latter require
 * too much memory in this context without giving us any advantage.
 */

struct _ActionInsert
{
	gint pos;
	gchar *text;
};

struct _ActionDelete
{
	gint start;
	gint end;
	gchar *text;
};

struct _Action
{
	ActionType action_type;

	union
	{
		ActionInsert insert;
		ActionDelete delete;
	} action;
};

struct _ActionGroup
{
	/* One or several Action's that forms a single undo or redo step. The
	 * most recent action is at the end of the list.
	 * In fact, actions can be grouped with
	 * gtk_text_buffer_begin_user_action() and
	 * gtk_text_buffer_end_user_action().
	 */
	GQueue *actions;
};

struct _GtkSourceUndoManagerDefaultPrivate
{
	/* Weak ref to the buffer. */
	GtkTextBuffer *buffer;

	/* List of ActionGroup's. The most recent ActionGroup is at the end of
	 * the list.
	 */
	GQueue *action_groups;

	/* Current location in 'action_groups', where we are located in the
	 * history. The redo steps are on the right of the pointer, and the undo
	 * steps are on the left. In other words, the next redo step is
	 * location->data. The next undo step is location->prev->data. But the
	 * location should not be seen as a node, it should be seen as a
	 * vertical bar between two nodes, like a GtkTextIter between two
	 * characters.
	 */
	GList *location;

	/* The number of nested calls to
	 * gtk_source_buffer_begin_not_undoable_action().
	 */
	guint running_not_undoable_actions;

	/* Max number of action groups. */
	gint max_undo_levels;

	/* The location in 'action_groups' where the buffer is saved. I.e. when
	 * gtk_text_buffer_set_modified (buffer, FALSE) was called for the last
	 * time.
	 * NULL is for the end of 'action_groups'.
	 * 'has_saved_location' is FALSE if the history doesn't contain a saved
	 * location.
	 */
	GList *saved_location;
	guint has_saved_location : 1;

	guint can_undo : 1;
	guint can_redo : 1;
};

enum
{
	PROP_0,
	PROP_BUFFER,
	PROP_MAX_UNDO_LEVELS
};

static void gtk_source_undo_manager_iface_init (GtkSourceUndoManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSourceUndoManagerDefault,
			 gtk_source_undo_manager_default,
			 G_TYPE_OBJECT,
			 G_ADD_PRIVATE (GtkSourceUndoManagerDefault)
                         G_IMPLEMENT_INTERFACE (GTK_SOURCE_TYPE_UNDO_MANAGER,
                                                gtk_source_undo_manager_iface_init))

/* Buffer signal handlers */

static void
insert_text_cb (GtkTextBuffer               *buffer,
		GtkTextIter                 *location,
		const gchar                 *text,
		gint                         length,
		GtkSourceUndoManagerDefault *manager)
{
}

static void
delete_range_cb (GtkTextBuffer               *buffer,
		 GtkTextIter                 *start,
		 GtkTextIter                 *end,
		 GtkSourceUndoManagerDefault *manager)
{
}

static void
begin_user_action_cb (GtkTextBuffer               *buffer,
		      GtkSourceUndoManagerDefault *manager)
{
}

static void
modified_changed_cb (GtkTextBuffer               *buffer,
		     GtkSourceUndoManagerDefault *manager)
{
}

static void
set_buffer (GtkSourceUndoManagerDefault *manager,
            GtkTextBuffer               *buffer)
{
	g_assert (manager->priv->buffer == NULL);

	if (buffer == NULL)
	{
		return;
	}

	manager->priv->buffer = buffer;

	g_object_add_weak_pointer (G_OBJECT (buffer),
				   (gpointer *)&manager->priv->buffer);

	g_signal_connect_object (buffer,
				 "insert-text",
				 G_CALLBACK (insert_text_cb),
				 manager,
				 0);

	g_signal_connect_object (buffer,
				 "delete-range",
				 G_CALLBACK (delete_range_cb),
				 manager,
				 0);

	g_signal_connect_object (buffer,
				 "begin-user-action",
				 G_CALLBACK (begin_user_action_cb),
				 manager,
				 0);

	g_signal_connect_object (buffer,
				 "modified-changed",
				 G_CALLBACK (modified_changed_cb),
				 manager,
				 0);
}

/* GObject construction, destruction and properties */

static void
gtk_source_undo_manager_default_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
	GtkSourceUndoManagerDefault *manager = GTK_SOURCE_UNDO_MANAGER_DEFAULT (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			set_buffer (manager, g_value_get_object (value));
			break;

		case PROP_MAX_UNDO_LEVELS:
			gtk_source_undo_manager_default_set_max_undo_levels (manager, g_value_get_int (value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_undo_manager_default_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
	GtkSourceUndoManagerDefault *manager = GTK_SOURCE_UNDO_MANAGER_DEFAULT (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			g_value_set_object (value, manager->priv->buffer);
			break;

		case PROP_MAX_UNDO_LEVELS:
			g_value_set_int (value, manager->priv->max_undo_levels);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_undo_manager_default_dispose (GObject *object)
{
	GtkSourceUndoManagerDefault *manager = GTK_SOURCE_UNDO_MANAGER_DEFAULT (object);

	if (manager->priv->buffer != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (manager->priv->buffer),
					      (gpointer *)&manager->priv->buffer);

		manager->priv->buffer = NULL;
	}

	G_OBJECT_CLASS (gtk_source_undo_manager_default_parent_class)->dispose (object);
}

static void
gtk_source_undo_manager_default_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_undo_manager_default_parent_class)->finalize (object);
}

static void
gtk_source_undo_manager_default_class_init (GtkSourceUndoManagerDefaultClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gtk_source_undo_manager_default_set_property;
	object_class->get_property = gtk_source_undo_manager_default_get_property;
	object_class->dispose = gtk_source_undo_manager_default_dispose;
	object_class->finalize = gtk_source_undo_manager_default_finalize;

	g_object_class_install_property (object_class,
	                                 PROP_BUFFER,
	                                 g_param_spec_object ("buffer",
	                                                      "Buffer",
	                                                      "The text buffer to add undo support on",
	                                                      GTK_TYPE_TEXT_BUFFER,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_MAX_UNDO_LEVELS,
	                                 g_param_spec_int ("max-undo-levels",
	                                                   "Max Undo Levels",
	                                                   "Number of undo levels for the buffer",
	                                                   -1,
	                                                   G_MAXINT,
	                                                   -1, /* unlimited by default */
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
gtk_source_undo_manager_default_init (GtkSourceUndoManagerDefault *manager)
{
	manager->priv = gtk_source_undo_manager_default_get_instance_private (manager);
}

/* Interface implementation */

static gboolean
gtk_source_undo_manager_can_undo_impl (GtkSourceUndoManager *manager)
{
	return GTK_SOURCE_UNDO_MANAGER_DEFAULT (manager)->priv->can_undo;
}

static gboolean
gtk_source_undo_manager_can_redo_impl (GtkSourceUndoManager *manager)
{
	return GTK_SOURCE_UNDO_MANAGER_DEFAULT (manager)->priv->can_redo;
}

static void
gtk_source_undo_manager_undo_impl (GtkSourceUndoManager *manager)
{
}

static void
gtk_source_undo_manager_redo_impl (GtkSourceUndoManager *manager)
{
}

static void
gtk_source_undo_manager_begin_not_undoable_action_impl (GtkSourceUndoManager *manager)
{
}

static void
gtk_source_undo_manager_end_not_undoable_action_impl (GtkSourceUndoManager *manager)
{
}

static void
gtk_source_undo_manager_iface_init (GtkSourceUndoManagerIface *iface)
{
	iface->can_undo = gtk_source_undo_manager_can_undo_impl;
	iface->can_redo = gtk_source_undo_manager_can_redo_impl;
	iface->undo = gtk_source_undo_manager_undo_impl;
	iface->redo = gtk_source_undo_manager_redo_impl;
	iface->begin_not_undoable_action = gtk_source_undo_manager_begin_not_undoable_action_impl;
	iface->end_not_undoable_action = gtk_source_undo_manager_end_not_undoable_action_impl;
}

/* Public functions */

void
gtk_source_undo_manager_default_set_max_undo_levels (GtkSourceUndoManagerDefault *manager,
                                                     gint                         max_undo_levels)
{
	g_return_if_fail (GTK_SOURCE_IS_UNDO_MANAGER_DEFAULT (manager));
	g_return_if_fail (max_undo_levels >= -1);
}

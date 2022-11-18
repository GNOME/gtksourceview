/* gtksourcelistsnapshot.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gtksourcelistsnapshot-private.h"

/*
 * GtkSourceListSnapshot:
 *
 * This classes purpose is to allow snapshoting a range of a GListModel
 * and ensuring that no changes to the underlying model will cause that
 * range to invalidate.
 *
 * The "hold" be done at the point where you want to avoid any model
 * changes causing the widgetry to invalidate, and released after you've
 * completed your snapshot work.
 *
 * If :model changes, or ::items-changed on the current model is emitted,
 * that will be supressed until the hold is released. Objects for the
 * hold range are retained so they may be returned from
 * g_list_model_get_item().
 */

struct _GtkSourceListSnapshot
{
	GObject       parent_instance;
	GListModel   *model;
	GSignalGroup *signal_group;
	GPtrArray    *held_items;
	guint         held_position;
	guint         held_n_items;
	guint         real_n_items;
	guint         invalid : 1;
};

enum {
	PROP_0,
	PROP_MODEL,
	N_PROPS
};

static GType
gtk_source_list_snapshot_get_item_type (GListModel *model)
{
	GtkSourceListSnapshot *self = GTK_SOURCE_LIST_SNAPSHOT (model);

	if (self->model != NULL)
	{
		return g_list_model_get_item_type (self->model);
	}

	return G_TYPE_OBJECT;
}

static guint
gtk_source_list_snapshot_get_n_items (GListModel *model)
{
	GtkSourceListSnapshot *self = GTK_SOURCE_LIST_SNAPSHOT (model);

	if (self->held_position == GTK_INVALID_LIST_POSITION)
	{
		return self->real_n_items;
	}

	return self->held_n_items;
}

static gpointer
gtk_source_list_snapshot_get_item (GListModel *model,
				   guint       position)
{
	GtkSourceListSnapshot *self = GTK_SOURCE_LIST_SNAPSHOT (model);

	if (self->held_position == GTK_INVALID_LIST_POSITION)
	{
		if (self->model == NULL)
		{
			return NULL;
		}

		return g_list_model_get_item (self->model, position);
	}

	if (position >= self->held_position &&
	    position < self->held_position + self->held_items->len)
	{
		return g_object_ref (g_ptr_array_index (self->held_items, position - self->held_position));
	}

	return NULL;
}

static void
list_model_iface_init (GListModelInterface *iface)
{
	iface->get_item_type = gtk_source_list_snapshot_get_item_type;
	iface->get_n_items = gtk_source_list_snapshot_get_n_items;
	iface->get_item = gtk_source_list_snapshot_get_item;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkSourceListSnapshot, gtk_source_list_snapshot, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, list_model_iface_init))

static GParamSpec *properties [N_PROPS];

static void
gtk_source_list_snapshot_bind_cb (GtkSourceListSnapshot *self,
                                  GListModel            *model,
                                  GSignalGroup          *signal_group)
{
	guint old_n_items;
	guint new_n_items;

	g_assert (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_assert (G_IS_LIST_MODEL (model));
	g_assert (G_IS_SIGNAL_GROUP (signal_group));

	old_n_items = self->real_n_items;
	new_n_items = g_list_model_get_n_items (model);

	if (self->held_position == GTK_INVALID_LIST_POSITION)
	{
		if (old_n_items || new_n_items)
		{
			g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);
		}
	}
	else
	{
		self->invalid = TRUE;
	}

	self->real_n_items = new_n_items;
}

static void
gtk_source_list_snapshot_unbind_cb (GtkSourceListSnapshot *self,
                                    GSignalGroup          *signal_group)
{
	guint old_n_items;
	guint new_n_items;

	g_assert (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_assert (G_IS_SIGNAL_GROUP (signal_group));

	old_n_items = self->real_n_items;
	new_n_items = 0;

	self->real_n_items = new_n_items;

	if (self->held_position == GTK_INVALID_LIST_POSITION)
	{
		if (old_n_items || new_n_items)
		{
			g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);
		}
	}
	else
	{
		self->invalid = TRUE;
	}
}

static void
gtk_source_list_snapshot_items_changed_cb (GtkSourceListSnapshot *self,
                                           guint                  position,
                                           guint                  removed,
                                           guint                  added,
                                           GListModel            *model)
{
	g_assert (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_assert (G_IS_LIST_MODEL (model));

	self->real_n_items -= removed;
	self->real_n_items += added;

	if (self->held_position == GTK_INVALID_LIST_POSITION)
	{
		if (removed || added)
		{
			g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
		}
	}
	else
	{
		self->invalid = TRUE;
	}
}

static void
gtk_source_list_snapshot_dispose (GObject *object)
{
	GtkSourceListSnapshot *self = (GtkSourceListSnapshot *)object;

	g_clear_pointer (&self->held_items, g_ptr_array_unref);

	if (self->signal_group != NULL)
	{
		g_signal_group_set_target (self->signal_group, NULL);
		g_clear_object (&self->signal_group);
	}

	g_clear_object (&self->model);

	G_OBJECT_CLASS (gtk_source_list_snapshot_parent_class)->dispose (object);
}

static void
gtk_source_list_snapshot_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
	GtkSourceListSnapshot *self = GTK_SOURCE_LIST_SNAPSHOT (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		g_value_set_object (value, self->model);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_list_snapshot_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
	GtkSourceListSnapshot *self = GTK_SOURCE_LIST_SNAPSHOT (object);

	switch (prop_id)
	{
	case PROP_MODEL:
		gtk_source_list_snapshot_set_model (self, g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
gtk_source_list_snapshot_class_init (GtkSourceListSnapshotClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_list_snapshot_dispose;
	object_class->get_property = gtk_source_list_snapshot_get_property;
	object_class->set_property = gtk_source_list_snapshot_set_property;

	properties [PROP_MODEL] =
		g_param_spec_object ("model", NULL, NULL,
		                     G_TYPE_LIST_MODEL,
		                     (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_list_snapshot_init (GtkSourceListSnapshot *self)
{
	self->signal_group = g_signal_group_new (G_TYPE_LIST_MODEL);
	self->held_items = g_ptr_array_new_with_free_func (g_object_unref);
	self->held_position = GTK_INVALID_LIST_POSITION;

	g_signal_connect_object (self->signal_group,
	                         "bind",
	                         G_CALLBACK (gtk_source_list_snapshot_bind_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	g_signal_connect_object (self->signal_group,
	                         "unbind",
	                         G_CALLBACK (gtk_source_list_snapshot_unbind_cb),
	                         self,
	                         G_CONNECT_SWAPPED);
	g_signal_group_connect_object (self->signal_group,
	                               "items-changed",
	                               G_CALLBACK (gtk_source_list_snapshot_items_changed_cb),
	                               self,
	                               G_CONNECT_SWAPPED);
}

GtkSourceListSnapshot *
gtk_source_list_snapshot_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_LIST_SNAPSHOT, NULL);
}

/**
 * gtk_source_list_snapshot_get_model:
 * @self: a #GtkSourceListSnapshot
 *
 * Gets the underlying model, if any.
 *
 * Returns: (transfer none) (nullable): a #GListModel or %NULL
 */
GListModel *
gtk_source_list_snapshot_get_model (GtkSourceListSnapshot *self)
{
	g_return_val_if_fail (GTK_SOURCE_IS_LIST_SNAPSHOT (self), NULL);

	return self->model;
}

void
gtk_source_list_snapshot_set_model (GtkSourceListSnapshot *self,
                                    GListModel            *model)
{
	g_return_if_fail (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_return_if_fail (!model || G_IS_LIST_MODEL (model));

	if (g_set_object (&self->model, model))
	{
		g_signal_group_set_target (self->signal_group, model);
		g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_MODEL]);
	}
}

void
gtk_source_list_snapshot_hold (GtkSourceListSnapshot *self,
                               guint                  position,
                               guint                  length)
{
	g_return_if_fail (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_return_if_fail (self->held_position == GTK_INVALID_LIST_POSITION);
	g_return_if_fail (self->held_items != NULL);
	g_return_if_fail (self->held_items->len == 0);
	g_return_if_fail (self->held_n_items == 0);

	self->held_position = position;

	if (self->model != NULL)
	{
		self->held_n_items = g_list_model_get_n_items (self->model);
	}

	if (position > self->held_n_items)
	{
		position = self->held_n_items;
	}

	if (position + length > self->held_n_items)
	{
		length = self->held_n_items - position;
	}

	for (guint i = 0; i < length; i++)
	{
		g_ptr_array_add (self->held_items,
		                 g_list_model_get_item (self->model, position + i));
	}
}

void
gtk_source_list_snapshot_release (GtkSourceListSnapshot *self)
{
	gboolean was_invalid;
	guint old_n_items;
	guint new_n_items;

	g_return_if_fail (GTK_SOURCE_IS_LIST_SNAPSHOT (self));
	g_return_if_fail (self->held_position != GTK_INVALID_LIST_POSITION);
	g_return_if_fail (self->held_items != NULL);

	was_invalid = self->invalid;
	old_n_items = self->held_n_items;
	new_n_items = self->model ? g_list_model_get_n_items (self->model) : 0;

	self->invalid = FALSE;
	self->held_n_items = 0;
	self->held_position = GTK_INVALID_LIST_POSITION;

	if (self->held_items->len > 0)
	{
		g_ptr_array_remove_range (self->held_items, 0, self->held_items->len);
	}

	if (was_invalid)
	{
		g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, new_n_items);
	}
}

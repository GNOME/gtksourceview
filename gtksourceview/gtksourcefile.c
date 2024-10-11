/*
 * This file is part of GtkSourceView
 *
 * Copyright 2014, 2015 - Sébastien Wilmet <swilmet@gnome.org>
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtksourcefile-private.h"
#include "gtksourceencoding.h"
#include "gtksource-enumtypes.h"

/**
 * GtkSourceFile:
 *
 * On-disk representation of a [class@Buffer].
 *
 * A `GtkSourceFile` object is the on-disk representation of a [class@Buffer].
 * With a `GtkSourceFile`, you can create and configure a [class@FileLoader]
 * and [class@FileSaver] which take by default the values of the
 * `GtkSourceFile` properties (except for the file loader which auto-detect some
 * properties). On a successful load or save operation, the `GtkSourceFile`
 * properties are updated. If an operation fails, the `GtkSourceFile` properties
 * have still the previous valid values.
 */

enum
{
	PROP_0,
	PROP_LOCATION,
	PROP_ENCODING,
	PROP_NEWLINE_TYPE,
	PROP_COMPRESSION_TYPE,
	PROP_READ_ONLY,
	N_PROPS
};

typedef struct
{
	GFile *location;
	const GtkSourceEncoding *encoding;
	GtkSourceNewlineType newline_type;
	GtkSourceCompressionType compression_type;

	GtkSourceMountOperationFactory mount_operation_factory;
	gpointer mount_operation_userdata;
	GDestroyNotify mount_operation_notify;

	/* Last known modification time of 'location'. The value is updated on a
	 * file loading or file saving.
	 */
	gint64 modification_time;

	guint modification_time_set : 1;

	guint externally_modified : 1;
	guint deleted : 1;
	guint readonly : 1;
} GtkSourceFilePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceFile, gtk_source_file, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS];

static void
gtk_source_file_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	GtkSourceFile *file = GTK_SOURCE_FILE (object);
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	switch (prop_id)
	{
	case PROP_LOCATION:
		g_value_set_object (value, priv->location);
		break;

	case PROP_ENCODING:
		g_value_set_boxed (value, priv->encoding);
		break;

	case PROP_NEWLINE_TYPE:
		g_value_set_enum (value, priv->newline_type);
		break;

	case PROP_COMPRESSION_TYPE:
		g_value_set_enum (value, priv->compression_type);
		break;

	case PROP_READ_ONLY:
		g_value_set_boolean (value, priv->readonly);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_file_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	GtkSourceFile *file = GTK_SOURCE_FILE (object);

	switch (prop_id)
	{
	case PROP_LOCATION:
		gtk_source_file_set_location (file, g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_file_dispose (GObject *object)
{
	GtkSourceFile *file = GTK_SOURCE_FILE (object);
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_clear_object (&priv->location);

	if (priv->mount_operation_notify != NULL)
	{
		priv->mount_operation_notify (priv->mount_operation_userdata);
		priv->mount_operation_notify = NULL;
	}

	G_OBJECT_CLASS (gtk_source_file_parent_class)->dispose (object);
}

static void
gtk_source_file_class_init (GtkSourceFileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = gtk_source_file_get_property;
	object_class->set_property = gtk_source_file_set_property;
	object_class->dispose = gtk_source_file_dispose;

	/**
	 * GtkSourceFile:location:
	 *
	 * The location.
	 */
	properties [PROP_LOCATION] =
		g_param_spec_object ("location",
		                     "Location",
		                     "",
		                     G_TYPE_FILE,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFile:encoding:
	 *
	 * The character encoding, initially %NULL. After a successful file
	 * loading or saving operation, the encoding is non-%NULL.
	 */
	properties[PROP_ENCODING] =
		g_param_spec_boxed ("encoding",
		                    "Encoding",
		                    "",
		                    GTK_SOURCE_TYPE_ENCODING,
		                    (G_PARAM_READABLE |
		                     G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFile:newline-type:
	 *
	 * The line ending type.
	 */
	properties[PROP_NEWLINE_TYPE] =
		g_param_spec_enum ("newline-type",
		                   "Newline type",
		                   "",
		                   GTK_SOURCE_TYPE_NEWLINE_TYPE,
		                   GTK_SOURCE_NEWLINE_TYPE_LF,
		                   (G_PARAM_READABLE |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFile:compression-type:
	 *
	 * The compression type.
	 */
	properties [PROP_COMPRESSION_TYPE] =
		g_param_spec_enum ("compression-type",
		                   "Compression type",
		                   "",
		                   GTK_SOURCE_TYPE_COMPRESSION_TYPE,
		                   GTK_SOURCE_COMPRESSION_TYPE_NONE,
		                   (G_PARAM_READABLE |
		                    G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFile:read-only:
	 *
	 * Whether the file is read-only or not. The value of this property is
	 * not updated automatically (there is no file monitors).
	 */
	properties [PROP_READ_ONLY] =
		g_param_spec_boolean ("read-only",
		                      "Read Only",
		                      "",
		                      FALSE,
		                      (G_PARAM_READABLE |
		                       G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_source_file_init (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	priv->encoding = NULL;
	priv->newline_type = GTK_SOURCE_NEWLINE_TYPE_LF;
	priv->compression_type = GTK_SOURCE_COMPRESSION_TYPE_NONE;
}

/**
 * gtk_source_file_new:
 *
 * Returns: a new #GtkSourceFile object.
 */
GtkSourceFile *
gtk_source_file_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_FILE, NULL);
}

/**
 * gtk_source_file_set_location:
 * @file: a #GtkSourceFile.
 * @location: (nullable): the new #GFile, or %NULL.
 *
 * Sets the location.
 */
void
gtk_source_file_set_location (GtkSourceFile *file,
                              GFile         *location)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));
	g_return_if_fail (location == NULL || G_IS_FILE (location));

	if (g_set_object (&priv->location, location))
	{
		g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_LOCATION]);

		/* The modification_time is for the old location. */
		priv->modification_time_set = FALSE;

		priv->externally_modified = FALSE;
		priv->deleted = FALSE;
	}
}

/**
 * gtk_source_file_get_location:
 * @file: a #GtkSourceFile.
 *
 * Returns: (transfer none): the #GFile.
 */
GFile *
gtk_source_file_get_location (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), NULL);

	return priv->location;
}

void
_gtk_source_file_set_encoding (GtkSourceFile           *file,
                               const GtkSourceEncoding *encoding)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	if (priv->encoding != encoding)
	{
		priv->encoding = encoding;
		g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_ENCODING]);
	}
}

/**
 * gtk_source_file_get_encoding:
 * @file: a #GtkSourceFile.
 *
 * The encoding is initially %NULL. After a successful file loading or saving
 * operation, the encoding is non-%NULL.
 *
 * Returns: the character encoding.
 */
const GtkSourceEncoding *
gtk_source_file_get_encoding (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), NULL);

	return priv->encoding;
}

void
_gtk_source_file_set_newline_type (GtkSourceFile        *file,
                                   GtkSourceNewlineType  newline_type)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	if (priv->newline_type != newline_type)
	{
		priv->newline_type = newline_type;
		g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_NEWLINE_TYPE]);
	}
}

/**
 * gtk_source_file_get_newline_type:
 * @file: a #GtkSourceFile.
 *
 * Returns: the newline type.
 */
GtkSourceNewlineType
gtk_source_file_get_newline_type (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), GTK_SOURCE_NEWLINE_TYPE_DEFAULT);

	return priv->newline_type;
}

void
_gtk_source_file_set_compression_type (GtkSourceFile            *file,
                                       GtkSourceCompressionType  compression_type)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	if (priv->compression_type != compression_type)
	{
		priv->compression_type = compression_type;
		g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_COMPRESSION_TYPE]);
	}
}

/**
 * gtk_source_file_get_compression_type:
 * @file: a #GtkSourceFile.
 *
 * Returns: the compression type.
 */
GtkSourceCompressionType
gtk_source_file_get_compression_type (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), GTK_SOURCE_COMPRESSION_TYPE_NONE);

	return priv->compression_type;
}

/**
 * gtk_source_file_set_mount_operation_factory:
 * @file: a #GtkSourceFile.
 * @callback: (scope notified) (closure user_data): a
 *   #GtkSourceMountOperationFactory to call when a #GMountOperation is needed.
 * @user_data: the data to pass to the @callback function.
 * @notify: (nullable): function to call on @user_data when the @callback is no
 *   longer needed, or %NULL.
 *
 * Sets a [callback@MountOperationFactory] function that will be called when a
 * [class@Gio.MountOperation] must be created.
 *
 * This is useful for creating a [class@Gtk.MountOperation] with the parent [class@Gtk.Window].
 *
 * If a mount operation factory isn't set, [ctor@Gio.MountOperation.new] will be
 * called.
 */
void
gtk_source_file_set_mount_operation_factory (GtkSourceFile                  *file,
                                             GtkSourceMountOperationFactory  callback,
                                             gpointer                        user_data,
                                             GDestroyNotify                  notify)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	if (priv->mount_operation_notify != NULL)
	{
		priv->mount_operation_notify (priv->mount_operation_userdata);
	}

	priv->mount_operation_factory = callback;
	priv->mount_operation_userdata = user_data;
	priv->mount_operation_notify = notify;
}

GMountOperation *
_gtk_source_file_create_mount_operation (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	return (file != NULL && priv->mount_operation_factory != NULL) ?
		priv->mount_operation_factory (file, priv->mount_operation_userdata) :
		g_mount_operation_new ();
}

gboolean
_gtk_source_file_get_modification_time (GtkSourceFile *file,
					gint64        *modification_time)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_assert (modification_time != NULL);

	if (file == NULL)
	{
		return FALSE;
	}

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), FALSE);

	if (priv->modification_time_set)
	{
		*modification_time = priv->modification_time;
	}

	return priv->modification_time_set;
}

void
_gtk_source_file_set_modification_time (GtkSourceFile *file,
					gint64         modification_time)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	if (file != NULL)
	{
		g_return_if_fail (GTK_SOURCE_IS_FILE (file));

		priv->modification_time = modification_time;
		priv->modification_time_set = TRUE;
	}
}

/**
 * gtk_source_file_is_local:
 * @file: a #GtkSourceFile.
 *
 * Returns whether the file is local. If the [property@File:location] is %NULL,
 * returns %FALSE.
 *
 * Returns: whether the file is local.
 */
gboolean
gtk_source_file_is_local (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), FALSE);

	if (priv->location == NULL)
	{
		return FALSE;
	}

	return g_file_has_uri_scheme (priv->location, "file");
}

/**
 * gtk_source_file_check_file_on_disk:
 * @file: a #GtkSourceFile.
 *
 * Checks synchronously the file on disk, to know whether the file is externally
 * modified, or has been deleted, and whether the file is read-only.
 *
 * #GtkSourceFile doesn't create a [class@Gio.FileMonitor] to track those properties, so
 * this function needs to be called instead. Creating lots of [class@Gio.FileMonitor]'s
 * would take lots of resources.
 *
 * Since this function is synchronous, it is advised to call it only on local
 * files. See [method@File.is_local].
 */
void
gtk_source_file_check_file_on_disk (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);
	GFileInfo *info;

	if (priv->location == NULL)
	{
		return;
	}

	info = g_file_query_info (priv->location,
				  G_FILE_ATTRIBUTE_TIME_MODIFIED ","
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  NULL);

	if (info == NULL)
	{
		priv->deleted = TRUE;
		return;
	}

	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED) &&
	    priv->modification_time_set)
	{
		GDateTime *dt;
		gint64 mtime;

		dt = g_file_info_get_modification_date_time (info);
		mtime = g_date_time_to_unix (dt);

		/* Note that the modification time can even go backwards if the
		 * user is copying over an old file.
		 */
		if (mtime != priv->modification_time)
		{
			priv->externally_modified = TRUE;
		}

		g_date_time_unref (dt);
	}

	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
	{
		gboolean readonly;

		readonly = !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

		_gtk_source_file_set_readonly (file, readonly);
	}

	g_object_unref (info);
}

void
_gtk_source_file_set_externally_modified (GtkSourceFile *file,
                                          gboolean       externally_modified)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	priv->externally_modified = externally_modified != FALSE;
}

/**
 * gtk_source_file_is_externally_modified:
 * @file: a #GtkSourceFile.
 *
 * Returns whether the file is externally modified. If the
 * [property@File:location] is %NULL, returns %FALSE.
 *
 * To have an up-to-date value, you must first call
 * [method@File.check_file_on_disk].
 *
 * Returns: whether the file is externally modified.
 */
gboolean
gtk_source_file_is_externally_modified (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), FALSE);

	return priv->externally_modified;
}

void
_gtk_source_file_set_deleted (GtkSourceFile *file,
                              gboolean       deleted)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	priv->deleted = deleted != FALSE;
}

/**
 * gtk_source_file_is_deleted:
 * @file: a #GtkSourceFile.
 *
 * Returns whether the file has been deleted. If the
 * [property@File:location] is %NULL, returns %FALSE.
 *
 * To have an up-to-date value, you must first call
 * [method@File.check_file_on_disk].
 *
 * Returns: whether the file has been deleted.
 */
gboolean
gtk_source_file_is_deleted (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), FALSE);

	return priv->deleted;
}

void
_gtk_source_file_set_readonly (GtkSourceFile *file,
                               gboolean       readonly)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_if_fail (GTK_SOURCE_IS_FILE (file));

	readonly = readonly != FALSE;

	if (priv->readonly != readonly)
	{
		priv->readonly = readonly;
		g_object_notify_by_pspec (G_OBJECT (file), properties[PROP_READ_ONLY]);
	}
}

/**
 * gtk_source_file_is_readonly:
 * @file: a #GtkSourceFile.
 *
 * Returns whether the file is read-only. If the
 * [property@File:location] is %NULL, returns %FALSE.
 *
 * To have an up-to-date value, you must first call
 * [method@File.check_file_on_disk].
 *
 * Returns: whether the file is read-only.
 */
gboolean
gtk_source_file_is_readonly (GtkSourceFile *file)
{
	GtkSourceFilePrivate *priv = gtk_source_file_get_instance_private (file);

	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), FALSE);

	return priv->readonly;
}

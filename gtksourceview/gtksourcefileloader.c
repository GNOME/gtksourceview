/*
 * This file is part of GtkSourceView
 *
 * Copyright 2005 - Paolo Maggi
 * Copyright 2007 - Paolo Maggi, Steve Frécinaux
 * Copyright 2008 - Jesse van den Kieboom
 * Copyright 2014, 2016 - Sébastien Wilmet
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

#include <glib/gi18n-lib.h>

#include "gtksourcefileloader.h"
#include "gtksourcebuffer-private.h"
#include "gtksourcefile-private.h"
#include "gtksourcebufferoutputstream-private.h"
#include "gtksourceencoding.h"
#include "gtksourceencoding-private.h"
#include "gtksource-enumtypes.h"
#include "gtksourcetrace.h"
#include "gtksourceutils-private.h"

/**
 * GtkSourceFileLoader:
 *
 * Load a file into a GtkSourceBuffer.
 *
 * A `GtkSourceFileLoader` object permits to load the contents of a [iface@Gio.File] or a
 * [class@Gio.InputStream] into a [class@Buffer].
 *
 * A file loader should be used only for one load operation, including errors
 * handling. If an error occurs, you can reconfigure the loader and relaunch the
 * operation with [method@FileLoader.load_async].
 *
 * Running a `GtkSourceFileLoader` is an undoable action for the
 * [class@Buffer].
 *
 * After a file loading, the buffer is reset to the contents provided by the
 * [iface@Gio.File] or [class@Gio.InputStream], so the buffer is set as “unmodified”, that is,
 * [method@Gtk.TextBuffer.set_modified] is called with %FALSE. If the contents isn't
 * saved somewhere (for example if you load from stdin), then you should
 * probably call [method@Gtk.TextBuffer.set_modified] with %TRUE after calling
 * [method@FileLoader.load_finish].
 */

enum
{
	PROP_0,
	PROP_BUFFER,
	PROP_FILE,
	PROP_LOCATION,
	PROP_INPUT_STREAM
};

#define READ_N_PAGES 2
#define READ_CHUNK_SIZE (_gtk_source_utils_get_page_size()*READ_N_PAGES)
#define LOADER_QUERY_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
				G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
				G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
				G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE

struct _GtkSourceFileLoader
{
	GObject parent_instance;

	/* Weak ref to the GtkSourceBuffer. A strong ref could create a
	 * reference cycle in an application. For example a subclass of
	 * GtkSourceBuffer can have a strong ref to the FileLoader.
	 */
	GtkSourceBuffer *source_buffer;

	/* Weak ref to the GtkSourceFile. A strong ref could create a reference
	 * cycle in an application. For example a subclass of GtkSourceFile can
	 * have a strong ref to the FileLoader.
	 */
	GtkSourceFile *file;

	GFile *location;

	/* The value of the :input-stream property. Do not confuse with the
	 * input_stream field in TaskData.
	 */
	GInputStream *input_stream_property;

	GSList *candidate_encodings;

	const GtkSourceEncoding *auto_detected_encoding;
	GtkSourceNewlineType auto_detected_newline_type;
	GtkSourceCompressionType auto_detected_compression_type;

	GTask *task;

	gint64 load_begin_time;
};

typedef struct
{
	/* The two streams cannot be spliced directly, because:
	 * (1) We need to call the progress callback.
	 * (2) Sync methods must be used for the output stream, and async
	 *     methods for the input stream.
	 */
	GInputStream *input_stream;
	GtkSourceBufferOutputStream *output_stream;

	GFileInfo *info;

	GFileProgressCallback progress_cb;
	gpointer progress_cb_data;
	GDestroyNotify progress_cb_notify;

	goffset total_bytes_read;
	goffset total_size;

	gssize chunk_bytes_read;
	gchar *chunk_buffer;

	guint guess_content_type_from_content : 1;
	guint tried_mount : 1;
} TaskData;

G_DEFINE_TYPE (GtkSourceFileLoader, gtk_source_file_loader, G_TYPE_OBJECT)

static void open_file       (GTask *task);
static void read_file_chunk (GTask *task);

static TaskData *
task_data_new (void)
{
	TaskData *task_data = g_new0 (TaskData, 1);

	task_data->chunk_buffer =
		_gtk_source_utils_aligned_alloc (_gtk_source_utils_get_page_size (),
		                                 READ_N_PAGES,
		                                 _gtk_source_utils_get_page_size ());

	return task_data;
}

static void
task_data_free (gpointer data)
{
	TaskData *task_data = data;

	if (task_data == NULL)
	{
		return;
	}

	g_clear_object (&task_data->input_stream);
	g_clear_object (&task_data->output_stream);
	g_clear_object (&task_data->info);

	if (task_data->progress_cb_notify != NULL)
	{
		task_data->progress_cb_notify (task_data->progress_cb_data);
	}

	_gtk_source_utils_aligned_free (task_data->chunk_buffer);

	g_free (task_data);
}

static GtkSourceCompressionType
get_compression_type_from_content_type (const gchar *content_type)
{
	if (content_type == NULL)
	{
		return GTK_SOURCE_COMPRESSION_TYPE_NONE;
	}

	if (g_content_type_is_a (content_type, "application/x-gzip"))
	{
		return GTK_SOURCE_COMPRESSION_TYPE_GZIP;
	}

	return GTK_SOURCE_COMPRESSION_TYPE_NONE;
}

static void
gtk_source_file_loader_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
	GtkSourceFileLoader *loader = GTK_SOURCE_FILE_LOADER (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			g_assert (loader->source_buffer == NULL);
			loader->source_buffer = g_value_get_object (value);
			g_object_add_weak_pointer (G_OBJECT (loader->source_buffer),
						   (gpointer *)&loader->source_buffer);
			break;

		case PROP_FILE:
			g_assert (loader->file == NULL);
			loader->file = g_value_get_object (value);
			g_object_add_weak_pointer (G_OBJECT (loader->file),
						   (gpointer *)&loader->file);
			break;

		case PROP_LOCATION:
			g_assert (loader->location == NULL);
			loader->location = g_value_dup_object (value);
			break;

		case PROP_INPUT_STREAM:
			g_assert (loader->input_stream_property == NULL);
			loader->input_stream_property = g_value_dup_object (value);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_file_loader_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
	GtkSourceFileLoader *loader = GTK_SOURCE_FILE_LOADER (object);

	switch (prop_id)
	{
		case PROP_BUFFER:
			g_value_set_object (value, loader->source_buffer);
			break;

		case PROP_FILE:
			g_value_set_object (value, loader->file);
			break;

		case PROP_LOCATION:
			g_value_set_object (value, loader->location);
			break;

		case PROP_INPUT_STREAM:
			g_value_set_object (value, loader->input_stream_property);
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_file_loader_dispose (GObject *object)
{
	GtkSourceFileLoader *loader = GTK_SOURCE_FILE_LOADER (object);

	if (loader->source_buffer != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (loader->source_buffer),
					      (gpointer *)&loader->source_buffer);

		loader->source_buffer = NULL;
	}

	if (loader->file != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (loader->file),
					      (gpointer *)&loader->file);

		loader->file = NULL;
	}

	g_clear_object (&loader->location);
	g_clear_object (&loader->input_stream_property);
	g_clear_object (&loader->task);

	g_slist_free (loader->candidate_encodings);
	loader->candidate_encodings = NULL;

	G_OBJECT_CLASS (gtk_source_file_loader_parent_class)->dispose (object);
}

static void
set_default_candidate_encodings (GtkSourceFileLoader *loader)
{
	GSList *list;
	GSList *l;
	const GtkSourceEncoding *file_encoding;

	/* Get first the default candidates from GtkSourceEncoding. If the
	 * GtkSourceFile's encoding has been set by a FileLoader or FileSaver,
	 * put it at the beginning of the list.
	 */
	list = gtk_source_encoding_get_default_candidates ();

	if (loader->file == NULL)
	{
		goto end;
	}

	file_encoding = gtk_source_file_get_encoding (loader->file);

	if (file_encoding == NULL)
	{
		goto end;
	}

	/* Remove file_encoding from the list, if already present, and prepend
	 * it to the list.
	 */
	for (l = list; l != NULL; l = l->next)
	{
		const GtkSourceEncoding *cur_encoding = l->data;

		if (cur_encoding == file_encoding)
		{
			list = g_slist_delete_link (list, l);

			/* The list doesn't contain duplicates, normally. */
			break;
		}
	}

	list = g_slist_prepend (list, (gpointer) file_encoding);

end:
	g_slist_free (loader->candidate_encodings);
	loader->candidate_encodings = list;
}

static void
gtk_source_file_loader_constructed (GObject *object)
{
	GtkSourceFileLoader *loader = GTK_SOURCE_FILE_LOADER (object);

	if (loader->file != NULL)
	{
		set_default_candidate_encodings (loader);

		if (loader->location == NULL &&
		    loader->input_stream_property == NULL)
		{
			loader->location = gtk_source_file_get_location (loader->file);

			if (loader->location != NULL)
			{
				g_object_ref (loader->location);
			}
			else
			{
				g_warning ("GtkSourceFileLoader: the GtkSourceFile's location is NULL. "
					   "Call gtk_source_file_set_location() or read from a GInputStream.");
			}
		}
	}

	G_OBJECT_CLASS (gtk_source_file_loader_parent_class)->constructed (object);
}

static void
gtk_source_file_loader_class_init (GtkSourceFileLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gtk_source_file_loader_dispose;
	object_class->set_property = gtk_source_file_loader_set_property;
	object_class->get_property = gtk_source_file_loader_get_property;
	object_class->constructed = gtk_source_file_loader_constructed;

	/**
	 * GtkSourceFileLoader:buffer:
	 *
	 * The #GtkSourceBuffer to load the contents into. The
	 * #GtkSourceFileLoader object has a weak reference to the buffer.
	 */
	g_object_class_install_property (object_class, PROP_BUFFER,
					 g_param_spec_object ("buffer",
							      "GtkSourceBuffer",
							      "",
							      GTK_SOURCE_TYPE_BUFFER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:file:
	 *
	 * The #GtkSourceFile. The #GtkSourceFileLoader object has a weak
	 * reference to the file.
	 */
	g_object_class_install_property (object_class, PROP_FILE,
					 g_param_spec_object ("file",
							      "GtkSourceFile",
							      "",
							      GTK_SOURCE_TYPE_FILE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:location:
	 *
	 * The #GFile to load. If the #GtkSourceFileLoader:input-stream is
	 * %NULL, by default the location is taken from the #GtkSourceFile at
	 * construction time.
	 */
	g_object_class_install_property (object_class, PROP_LOCATION,
					 g_param_spec_object ("location",
							      "Location",
							      "",
							      G_TYPE_FILE,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:input-stream:
	 *
	 * The #GInputStream to load. Useful for reading stdin. If this property
	 * is set, the #GtkSourceFileLoader:location property is ignored.
	 */
	g_object_class_install_property (object_class, PROP_INPUT_STREAM,
					 g_param_spec_object ("input-stream",
							      "Input stream",
							      "",
							      G_TYPE_INPUT_STREAM,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));
}

static void
gtk_source_file_loader_init (GtkSourceFileLoader *loader)
{
	loader = gtk_source_file_loader_get_instance_private (loader);
}

static void
close_input_stream_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
	GInputStream *input_stream = G_INPUT_STREAM (source_object);
	GTask *task = G_TASK (user_data);
	TaskData *task_data;
	GError *error = NULL;
	G_GNUC_UNUSED gint64 begin_time;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	task_data = g_task_get_task_data (task);

	begin_time = GTK_SOURCE_PROFILER_CURRENT_TIME;
	g_input_stream_close_finish (input_stream, result, &error);
	GTK_SOURCE_PROFILER_MARK (GTK_SOURCE_PROFILER_CURRENT_TIME - begin_time,
	                          "FileLoader.close-input", "");

	if (error != NULL)
	{
		GTK_SOURCE_PROFILER_LOG ("Error closing input stream: %s", error->message);
		g_task_return_error (task, error);
		goto cleanup;
	}

	begin_time = GTK_SOURCE_PROFILER_CURRENT_TIME;
	g_output_stream_close (G_OUTPUT_STREAM (task_data->output_stream),
	                       g_task_get_cancellable (task),
	                       &error);
	GTK_SOURCE_PROFILER_MARK (GTK_SOURCE_PROFILER_CURRENT_TIME - begin_time,
	                          "FileLoader.close-output", "");

	if (error != NULL)
	{
		GTK_SOURCE_PROFILER_LOG ("Error closing output stream: %s", error->message);
		g_task_return_error (task, error);
		goto cleanup;
	}

	/* Check if we needed some fallback char, if so, check if there was a
	 * previous error and if not set a fallback used error.
	 */
	if (gtk_source_buffer_output_stream_get_num_fallbacks (task_data->output_stream) > 0)
	{
		g_task_return_new_error (task,
					 GTK_SOURCE_FILE_LOADER_ERROR,
					 GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK,
					 _("There was an encoding conversion error so a fallback character was used."));
		goto cleanup;
	}

	/* The task completion here will result in the task's callback
	 * getting called. So it is important that function does relatively
	 * little amount of work initially or we could end up blocking the
	 * main loop for long enough that we drop a frame.
	 *
	 * Therefore, we add a mark here to make it easier to find that
	 * in applications. Often it is things like setting syntax or
	 * other slightly intenstive operations that should be made async.
	 */
	begin_time = GTK_SOURCE_PROFILER_CURRENT_TIME;
	g_task_return_boolean (task, TRUE);
	GTK_SOURCE_PROFILER_MARK (GTK_SOURCE_PROFILER_CURRENT_TIME - begin_time,
	                          "FileLoader.task-complete-cb", "");

cleanup:
	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
write_complete (GTask *task)
{
	TaskData *task_data;

	task_data = g_task_get_task_data (task);

	g_input_stream_close_async (task_data->input_stream,
				    g_task_get_priority (task),
				    g_task_get_cancellable (task),
				    close_input_stream_cb,
				    task);
}

static void
write_file_chunk (GTask *task)
{
	TaskData *task_data;
	gssize chunk_bytes_written = 0;

	task_data = g_task_get_task_data (task);

	while (chunk_bytes_written < task_data->chunk_bytes_read)
	{
		gssize bytes_written;
		GError *error = NULL;

		/* We use sync methods on the buffer stream since it is in memory. Using
		 * async would be racy and we can end up with invalidated iters.
		 */
		bytes_written = g_output_stream_write (G_OUTPUT_STREAM (task_data->output_stream),
						       task_data->chunk_buffer + chunk_bytes_written,
						       task_data->chunk_bytes_read - chunk_bytes_written,
						       g_task_get_cancellable (task),
						       &error);

		GTK_SOURCE_PROFILER_LOG ("Written: %" G_GSSIZE_FORMAT, bytes_written);

		if (error != NULL)
		{
			GTK_SOURCE_PROFILER_LOG ("Write error: %s", error->message);
			g_task_return_error (task, error);
			return;
		}

		chunk_bytes_written += bytes_written;
	}

	/* FIXME: note that calling the progress callback blocks the read...
	 * Check if it isn't a performance problem.
	 */
	if (task_data->progress_cb != NULL &&
	    task_data->total_size > 0)
	{
		task_data->progress_cb (task_data->total_bytes_read,
					task_data->total_size,
					task_data->progress_cb_data);
	}

	read_file_chunk (task);
}

static void
read_cb (GObject      *source_object,
         GAsyncResult *result,
         gpointer      user_data)
{
	GInputStream *input_stream = G_INPUT_STREAM (source_object);
	GTask *task = G_TASK (user_data);
	GtkSourceFileLoader *loader;
	TaskData *task_data;
	GError *error = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	loader = g_task_get_source_object (task);
	task_data = g_task_get_task_data (task);

	task_data->chunk_bytes_read = g_input_stream_read_finish (input_stream, result, &error);

	if (error != NULL)
	{
		g_task_return_error (task, error);
		goto cleanup;
	}

	/* Check for the extremely unlikely case where the file size overflows. */
	if (task_data->total_bytes_read + task_data->chunk_bytes_read < task_data->total_bytes_read)
	{
		g_task_return_new_error (task,
		                         GTK_SOURCE_FILE_LOADER_ERROR,
		                         GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
		                         _("File too big."));
		goto cleanup;
	}

	if (task_data->guess_content_type_from_content &&
	    task_data->chunk_bytes_read > 0 &&
	    task_data->total_bytes_read == 0)
	{
		gchar *guessed;

		guessed = g_content_type_guess (NULL,
		                                (guchar *)task_data->chunk_buffer,
		                                task_data->chunk_bytes_read,
		                                NULL);

		if (guessed != NULL)
		{
			g_file_info_set_attribute_string (task_data->info,
			                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
			                                  guessed);

			g_free (guessed);
		}
	}

	/* End of the file, we are done! */
	if (task_data->chunk_bytes_read == 0)
	{
		/* Flush the stream to ensure proper line ending detection. */
		g_output_stream_flush (G_OUTPUT_STREAM (task_data->output_stream), NULL, NULL);

		loader->auto_detected_encoding =
			gtk_source_buffer_output_stream_get_guessed (task_data->output_stream);

		loader->auto_detected_newline_type =
			gtk_source_buffer_output_stream_detect_newline_type (task_data->output_stream);

		write_complete (task);
		goto cleanup;
	}

	task_data->total_bytes_read += task_data->chunk_bytes_read;

	write_file_chunk (task);

cleanup:
	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
read_file_chunk (GTask *task)
{
	TaskData *task_data;

	task_data = g_task_get_task_data (task);

	g_input_stream_read_async (task_data->input_stream,
				   task_data->chunk_buffer,
				   READ_CHUNK_SIZE,
				   g_task_get_priority (task),
				   g_task_get_cancellable (task),
				   read_cb,
				   task);
}

static void
add_gzip_decompressor_stream (GTask *task)
{
	TaskData *task_data;
	GZlibDecompressor *decompressor;
	GInputStream *new_input_stream;

	task_data = g_task_get_task_data (task);

	decompressor = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP);

	new_input_stream = g_converter_input_stream_new (task_data->input_stream,
							 G_CONVERTER (decompressor));

	g_object_unref (task_data->input_stream);
	g_object_unref (decompressor);

	task_data->input_stream = new_input_stream;
}

static void
create_input_stream (GTask *task)
{
	GtkSourceFileLoader *loader;
	TaskData *task_data;

	loader = g_task_get_source_object (task);
	task_data = g_task_get_task_data (task);

	loader->auto_detected_compression_type = GTK_SOURCE_COMPRESSION_TYPE_NONE;

	if (loader->input_stream_property != NULL)
	{
		task_data->input_stream = g_object_ref (loader->input_stream_property);
	}
	else if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE))
	{
		const gchar *content_type = g_file_info_get_content_type (task_data->info);

		switch (get_compression_type_from_content_type (content_type))
		{
			case GTK_SOURCE_COMPRESSION_TYPE_GZIP:
				add_gzip_decompressor_stream (task);
				loader->auto_detected_compression_type = GTK_SOURCE_COMPRESSION_TYPE_GZIP;
				break;

			case GTK_SOURCE_COMPRESSION_TYPE_NONE:
				/* NOOP */
				break;

			default:
				g_assert_not_reached ();
		}
	}

	g_return_if_fail (task_data->input_stream != NULL);

	/* start reading */
	read_file_chunk (task);
}

static void
query_info_cb (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
	GFile *location = G_FILE (source_object);
	GTask *task = G_TASK (user_data);
	TaskData *task_data;
	GError *error = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	task_data = g_task_get_task_data (task);

	g_clear_object (&task_data->info);
	task_data->info = g_file_query_info_finish (location, result, &error);

	if (error != NULL)
	{
		g_task_return_error (task, error);
		goto cleanup;
	}

	if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
	    g_file_info_get_file_type (task_data->info) != G_FILE_TYPE_REGULAR)
	{
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_NOT_REGULAR_FILE,
		                         _("Not a regular file."));
		goto cleanup;
	}

	if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
	{
		task_data->total_size = g_file_info_get_attribute_uint64 (task_data->info,
									  G_FILE_ATTRIBUTE_STANDARD_SIZE);
	}

	create_input_stream (task);

cleanup:
	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
mount_cb (GObject      *source_object,
          GAsyncResult *result,
          gpointer      user_data)
{
	GFile *location = G_FILE (source_object);
	GTask *task = G_TASK (user_data);
	GError *error = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	g_file_mount_enclosing_volume_finish (location, result, &error);

	if (error != NULL)
	{
		g_task_return_error (task, error);
	}
	else
	{
		/* Try again to open the file for reading. */
		open_file (task);
	}

	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
recover_not_mounted (GTask *task)
{
	GtkSourceFileLoader *loader;
	TaskData *task_data;
	GMountOperation *mount_operation;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	loader = g_task_get_source_object (task);
	task_data = g_task_get_task_data (task);

	mount_operation = _gtk_source_file_create_mount_operation (loader->file);

	task_data->tried_mount = TRUE;

	g_file_mount_enclosing_volume (loader->location,
				       G_MOUNT_MOUNT_NONE,
				       mount_operation,
				       g_task_get_cancellable (task),
				       mount_cb,
				       task);

	g_object_unref (mount_operation);

	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
open_file_cb (GObject      *source_object,
              GAsyncResult *result,
              gpointer      user_data)
{
	GFile *location = G_FILE (source_object);
	GTask *task = G_TASK (user_data);
	TaskData *task_data;
	GError *error = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	task_data = g_task_get_task_data (task);

	g_clear_object (&task_data->input_stream);
	task_data->input_stream = G_INPUT_STREAM (g_file_read_finish (location, result, &error));

	if (error != NULL)
	{
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED) &&
		    !task_data->tried_mount)
		{
			recover_not_mounted (task);
			g_error_free (error);
			return;
		}

		g_task_return_error (task, error);
		goto cleanup;
	}

	/* Get the file info: note we cannot use
	 * g_file_input_stream_query_info_async since it is not able to get the
	 * content type etc, beside it is not supported by gvfs.
	 * Using the file instead of the stream is slightly racy, but for
	 * loading this is not too bad...
	 */
	g_file_query_info_async (location,
				 LOADER_QUERY_ATTRIBUTES,
                                 G_FILE_QUERY_INFO_NONE,
				 g_task_get_priority (task),
				 g_task_get_cancellable (task),
				 query_info_cb,
				 task);

cleanup:
	GTK_SOURCE_PROFILER_END_MARK (G_STRFUNC, "");
}

static void
open_file (GTask *task)
{
	GtkSourceFileLoader *loader;

	loader = g_task_get_source_object (task);

	g_file_read_async (loader->location,
	                   g_task_get_priority (task),
			   g_task_get_cancellable (task),
	                   open_file_cb,
	                   task);
}

GQuark
gtk_source_file_loader_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
	{
		quark = g_quark_from_static_string ("gtk-source-file-loader-error");
	}

	return quark;
}

/**
 * gtk_source_file_loader_new:
 * @buffer: the #GtkSourceBuffer to load the contents into.
 * @file: the #GtkSourceFile.
 *
 * Creates a new `GtkSourceFileLoader` object. The contents is read from the
 * [class@File]'s location.
 *
 * If not already done, call [method@File.set_location] before calling this constructor.
 * The previous location is anyway not needed, because as soon as the file loading begins,
 * the @buffer is emptied.
 *
 * Returns: a new #GtkSourceFileLoader object.
 */
GtkSourceFileLoader *
gtk_source_file_loader_new (GtkSourceBuffer *buffer,
                            GtkSourceFile   *file)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), NULL);

	return g_object_new (GTK_SOURCE_TYPE_FILE_LOADER,
			     "buffer", buffer,
			     "file", file,
			     NULL);
}

/**
 * gtk_source_file_loader_new_from_stream:
 * @buffer: the #GtkSourceBuffer to load the contents into.
 * @file: the #GtkSourceFile.
 * @stream: the #GInputStream to load, e.g. stdin.
 *
 * Creates a new #GtkSourceFileLoader object. The contents is read from @stream.
 *
 * Returns: a new #GtkSourceFileLoader object.
 */
GtkSourceFileLoader *
gtk_source_file_loader_new_from_stream (GtkSourceBuffer *buffer,
                                        GtkSourceFile   *file,
                                        GInputStream    *stream)
{
	g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (buffer), NULL);
	g_return_val_if_fail (GTK_SOURCE_IS_FILE (file), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);

	return g_object_new (GTK_SOURCE_TYPE_FILE_LOADER,
			     "buffer", buffer,
			     "file", file,
			     "input-stream", stream,
			     NULL);
}

/**
 * gtk_source_file_loader_set_candidate_encodings:
 * @loader: a #GtkSourceFileLoader.
 * @candidate_encodings: (element-type GtkSourceEncoding): a list of
 *   #GtkSourceEncoding<!-- -->s.
 *
 * Sets the candidate encodings for the file loading.
 *
 * The encodings are tried in the same order as the list.
 *
 * For convenience, @candidate_encodings can contain duplicates. Only the first
 * occurrence of a duplicated encoding is kept in the list.
 *
 * By default the candidate encodings are (in that order in the list):
 *
 * 1. If set, the [class@File]'s encoding as returned by [method@File.get_encoding].
 * 2. The default candidates as returned by [func@Encoding.get_default_candidates].
 */
void
gtk_source_file_loader_set_candidate_encodings (GtkSourceFileLoader *loader,
                                                GSList              *candidate_encodings)
{
	GSList *list;

	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (loader->task == NULL);

	list = g_slist_copy (candidate_encodings);
	list = _gtk_source_encoding_remove_duplicates (list, GTK_SOURCE_ENCODING_DUPLICATES_KEEP_FIRST);

	g_slist_free (loader->candidate_encodings);
	loader->candidate_encodings = list;
}

/**
 * gtk_source_file_loader_get_buffer:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: (transfer none): the #GtkSourceBuffer to load the contents into.
 */
GtkSourceBuffer *
gtk_source_file_loader_get_buffer (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), NULL);

	return loader->source_buffer;
}

/**
 * gtk_source_file_loader_get_file:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: (transfer none): the #GtkSourceFile.
 */
GtkSourceFile *
gtk_source_file_loader_get_file (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), NULL);

	return loader->file;
}

/**
 * gtk_source_file_loader_get_location:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: (nullable) (transfer none): the #GFile to load, or %NULL
 * if an input stream is used.
 */
GFile *
gtk_source_file_loader_get_location (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), NULL);

	return loader->location;
}

/**
 * gtk_source_file_loader_get_input_stream:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: (nullable) (transfer none): the #GInputStream to load, or %NULL
 * if a #GFile is used.
 */
GInputStream *
gtk_source_file_loader_get_input_stream (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), NULL);

	return loader->input_stream_property;
}

/**
 * gtk_source_file_loader_load_async:
 * @loader: a #GtkSourceFileLoader.
 * @io_priority: the I/O priority of the request. E.g. %G_PRIORITY_LOW,
 *   %G_PRIORITY_DEFAULT or %G_PRIORITY_HIGH.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @progress_callback: (scope notified) (closure progress_callback_data) (nullable):
 *   function to call back with progress information, or %NULL if progress
 *   information is not needed.
 * @progress_callback_data: user data to pass to @progress_callback.
 * @progress_callback_notify: (nullable): function to call on
 *   @progress_callback_data when the @progress_callback is no longer needed, or
 *   %NULL.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is
 *   satisfied.
 * @user_data: user data to pass to @callback.
 *
 * Loads asynchronously the file or input stream contents into the [class@Buffer].
 *
 * See the [iface@Gio.AsyncResult] documentation to know how to use this
 * function.
 */

/* The GDestroyNotify is needed, currently the following bug is not fixed:
 * https://bugzilla.gnome.org/show_bug.cgi?id=616044
 */
void
gtk_source_file_loader_load_async (GtkSourceFileLoader   *loader,
                                   gint                   io_priority,
                                   GCancellable          *cancellable,
                                   GFileProgressCallback  progress_callback,
                                   gpointer               progress_callback_data,
                                   GDestroyNotify         progress_callback_notify,
                                   GAsyncReadyCallback    callback,
                                   gpointer               user_data)
{
	TaskData *task_data;
	gboolean implicit_trailing_newline;

	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (loader->task == NULL);

	loader->task = g_task_new (loader, cancellable, callback, user_data);
	g_task_set_priority (loader->task, io_priority);

	task_data = task_data_new ();
	g_task_set_task_data (loader->task, task_data, task_data_free);

	task_data->progress_cb = progress_callback;
	task_data->progress_cb_data = progress_callback_data;
	task_data->progress_cb_notify = progress_callback_notify;

	if (loader->source_buffer == NULL ||
	    loader->file == NULL ||
	    (loader->location == NULL && loader->input_stream_property == NULL))
	{
		g_task_return_new_error (loader->task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_INVALID_ARGUMENT,
		                         "Invalid argument");
		return;
	}

  /* Leave an internal mark about the buffer loading during the
   * progreess of the operation so external tooling can find it.
   */
  _gtk_source_buffer_begin_loading (loader->source_buffer);
  g_signal_connect_object (loader->task,
                           "notify::completed",
                           G_CALLBACK (_gtk_source_buffer_end_loading),
                           loader->source_buffer,
                           G_CONNECT_SWAPPED);

	loader->load_begin_time = GTK_SOURCE_PROFILER_CURRENT_TIME;

	/* Update GtkSourceFile location directly. The other GtkSourceFile
	 * properties are updated when the operation is finished. But since the
	 * file is loaded, the previous contents is lost, so the previous
	 * location is anyway not needed. And for display purposes, the new
	 * location is directly needed (for example to display the filename in a
	 * tab or an info bar with the progress information).
	 */
	if (loader->input_stream_property != NULL)
	{
		gtk_source_file_set_location (loader->file, NULL);
	}
	else
	{
		gtk_source_file_set_location (loader->file,
					      loader->location);
	}

	implicit_trailing_newline = gtk_source_buffer_get_implicit_trailing_newline (loader->source_buffer);

	/* The BufferOutputStream has a strong reference to the buffer.
         * We create the BufferOutputStream here so we are sure that the
         * buffer will not be destroyed during the file loading.
         */
	task_data->output_stream = gtk_source_buffer_output_stream_new (loader->source_buffer,
									loader->candidate_encodings,
									implicit_trailing_newline);

	if (loader->input_stream_property != NULL)
	{
		task_data->guess_content_type_from_content = TRUE;
		task_data->info = g_file_info_new ();

		create_input_stream (loader->task);
	}
	else
	{
		open_file (loader->task);
	}
}

/**
 * gtk_source_file_loader_load_finish:
 * @loader: a #GtkSourceFileLoader.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL.
 *
 * Finishes a file loading started with [method@FileLoader.load_async].
 *
 * If the contents has been loaded, the following [class@File] properties will
 * be updated: the location, the encoding, the newline type and the compression
 * type.
 *
 * Returns: whether the contents has been loaded successfully.
 */
gboolean
gtk_source_file_loader_load_finish (GtkSourceFileLoader  *loader,
                                    GAsyncResult         *result,
                                    GError              **error)
{
	gboolean ok;
	gboolean update_file_properties;
	GError *real_error = NULL;

	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
	g_return_val_if_fail (g_task_is_valid (result, loader), FALSE);

	ok = g_task_propagate_boolean (G_TASK (result), &real_error);

	if (error != NULL && real_error != NULL)
	{
		*error = g_error_copy (real_error);
	}

	/* Update the file properties if the contents has been loaded. The
	 * contents can be loaded successfully, or there can be encoding
	 * conversion errors with fallback characters. In the latter case, the
	 * encoding may be wrong, but since the contents has anyway be loaded,
	 * the file properties must be updated.
	 * With the other errors, normally the contents hasn't been loaded into
	 * the buffer, i.e. the buffer is still empty.
	 */
	update_file_properties = ok || (real_error != NULL &&
					real_error->domain == GTK_SOURCE_FILE_LOADER_ERROR &&
					real_error->code == GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK);

	if (update_file_properties && loader->file != NULL)
	{
		TaskData *task_data;

		task_data = g_task_get_task_data (G_TASK (result));

		/* The location is already updated at the beginning of the
		 * operation.
		 */

		_gtk_source_file_set_encoding (loader->file,
					       loader->auto_detected_encoding);

		_gtk_source_file_set_newline_type (loader->file,
						   loader->auto_detected_newline_type);

		_gtk_source_file_set_compression_type (loader->file,
						       loader->auto_detected_compression_type);

		_gtk_source_file_set_externally_modified (loader->file, FALSE);
		_gtk_source_file_set_deleted (loader->file, FALSE);

		if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_TIME_MODIFIED))
		{
			GDateTime *dt;
			gint64 mtime = 0;

			dt = g_file_info_get_modification_date_time (task_data->info);

			if (dt != NULL)
			{
				mtime = g_date_time_to_unix (dt);
				g_date_time_unref (dt);
			}

			_gtk_source_file_set_modification_time (loader->file, mtime);
		}

		if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
		{
			gboolean readonly;

			readonly = !g_file_info_get_attribute_boolean (task_data->info,
								       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

			_gtk_source_file_set_readonly (loader->file, readonly);
		}
		else
		{
			_gtk_source_file_set_readonly (loader->file, FALSE);
		}
	}

	g_clear_object (&loader->task);

	if (real_error != NULL)
	{
		g_error_free (real_error);
	}

	GTK_SOURCE_PROFILER_MARK (GTK_SOURCE_PROFILER_CURRENT_TIME - loader->load_begin_time,
				  "GtkSourceFileLoader.load",
				  g_file_peek_path (loader->location));

	return ok;
}

/**
 * gtk_source_file_loader_get_encoding:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: the detected file encoding.
 */
const GtkSourceEncoding *
gtk_source_file_loader_get_encoding (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), NULL);

	return loader->auto_detected_encoding;
}

/**
 * gtk_source_file_loader_get_newline_type:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: the detected newline type.
 */
GtkSourceNewlineType
gtk_source_file_loader_get_newline_type (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader),
			      GTK_SOURCE_NEWLINE_TYPE_LF);

	return loader->auto_detected_newline_type;
}

/**
 * gtk_source_file_loader_get_compression_type:
 * @loader: a #GtkSourceFileLoader.
 *
 * Returns: the detected compression type.
 */
GtkSourceCompressionType
gtk_source_file_loader_get_compression_type (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader),
			      GTK_SOURCE_COMPRESSION_TYPE_NONE);

	return loader->auto_detected_compression_type;
}

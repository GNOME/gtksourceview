/*
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
 * Copyright (C) 2014 - Sébastien Wilmet
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

#include <gtksourceview/gtksource.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef G_OS_UNIX
# include <sys/stat.h>
#endif

typedef struct
{
	const gchar *expected_buffer_contents;
	gint newline_type;
} LoaderTestData;

static GMainLoop *main_loop;

typedef struct
{
	GInputStream parent_instance;

	const guint8 *contents;
	gsize length;
	gsize offset;
	gsize first_chunk_size;
	guint n_reads;

	guint fail_after_first_read : 1;
} TestInputStream;

typedef struct
{
	GInputStreamClass parent_class;
} TestInputStreamClass;

GType test_input_stream_get_type (void);

G_DEFINE_TYPE (TestInputStream, test_input_stream, G_TYPE_INPUT_STREAM)

static gssize
test_input_stream_read (GInputStream  *stream,
                        void          *buffer,
                        gsize          count,
                        GCancellable  *cancellable,
                        GError       **error)
{
	TestInputStream *self = (TestInputStream *)stream;
	gsize to_read;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
	{
		return -1;
	}

	if (self->fail_after_first_read && self->n_reads > 0)
	{
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_FAILED,
		                     "Test stream read failure");
		return -1;
	}

	if (self->offset == self->length)
	{
		return 0;
	}

	to_read = MIN (count, self->length - self->offset);

	if (self->n_reads == 0 && self->first_chunk_size > 0)
	{
		to_read = MIN (to_read, self->first_chunk_size);
	}

	memcpy (buffer, self->contents + self->offset, to_read);
	self->offset += to_read;
	self->n_reads++;

	return to_read;
}

static gboolean
test_input_stream_close (GInputStream  *stream,
                         GCancellable  *cancellable,
                         GError       **error)
{
	return TRUE;
}

static void
test_input_stream_class_init (TestInputStreamClass *klass)
{
	GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);

	stream_class->read_fn = test_input_stream_read;
	stream_class->close_fn = test_input_stream_close;
}

static void
test_input_stream_init (TestInputStream *self)
{
}

static GInputStream *
test_input_stream_new (const guint8 *contents,
                       gsize         length,
                       gsize         first_chunk_size,
                       gboolean      fail_after_first_read)
{
	TestInputStream *stream;

	stream = g_object_new (test_input_stream_get_type (), NULL);
	stream->contents = contents;
	stream->length = length;
	stream->first_chunk_size = first_chunk_size;
	stream->fail_after_first_read = fail_after_first_read;

	return G_INPUT_STREAM (stream);
}

typedef struct
{
	GCancellable *cancellable;
	GtkSourceBuffer *buffer;
	GQuark expected_domain;
	gint expected_code;
	guint begin_user_action_count;
	guint end_user_action_count;
} LoaderFailureTestData;

static void
begin_user_action_cb (GtkTextBuffer         *buffer,
                      LoaderFailureTestData *data)
{
	data->begin_user_action_count++;
}

static void
end_user_action_cb (GtkTextBuffer         *buffer,
                    LoaderFailureTestData *data)
{
	data->end_user_action_count++;
}

static void
cancel_on_insert_text_cb (GtkTextBuffer         *buffer,
                          GtkTextIter           *location,
                          char                  *text,
                          int                    len,
                          LoaderFailureTestData *data)
{
	g_cancellable_cancel (data->cancellable);
}

static gboolean
load_failure_after_completion_cb (LoaderFailureTestData *data);

static void
load_failure_cb (GtkSourceFileLoader  *loader,
                 GAsyncResult         *result,
                 LoaderFailureTestData *data)
{
	GtkSourceBuffer *buffer;
	GError *error = NULL;

	g_assert_false (gtk_source_file_loader_load_finish (loader, result, &error));
	g_assert_error (error, data->expected_domain, data->expected_code);
	g_clear_error (&error);

	buffer = gtk_source_file_loader_get_buffer (loader);
	data->buffer = g_object_ref (buffer);

	g_idle_add ((GSourceFunc) load_failure_after_completion_cb, data);
}

static gboolean
load_failure_after_completion_cb (LoaderFailureTestData *data)
{
	GtkSourceBuffer *buffer = g_steal_pointer (&data->buffer);

	g_assert_false (gtk_source_buffer_get_loading (buffer));
	g_assert_cmpuint (data->begin_user_action_count, ==, data->end_user_action_count);

	g_object_unref (buffer);
	g_main_loop_quit (main_loop);

	return G_SOURCE_REMOVE;
}

static void
delete_file (GFile *location)
{
	if (g_file_query_exists (location, NULL))
	{
		GError *error = NULL;

		g_file_delete (location, NULL, &error);
		g_assert_no_error (error);
	}
}

static GBytes *
create_gzip_data (const char *contents,
                  gsize       length)
{
	GOutputStream *memory;
	GOutputStream *converter;
	GZlibCompressor *compressor;
	GError *error = NULL;
	gpointer data;
	gsize data_size;

	memory = g_memory_output_stream_new_resizable ();
	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	converter = g_converter_output_stream_new (memory, G_CONVERTER (compressor));

	g_assert_true (g_output_stream_write_all (converter,
	                                          contents,
	                                          length,
	                                          NULL,
	                                          NULL,
	                                          &error));
	g_assert_no_error (error);

	g_assert_true (g_output_stream_close (converter, NULL, &error));
	g_assert_no_error (error);

	data_size = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (memory));
	data = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (memory));

	g_object_unref (converter);
	g_object_unref (compressor);
	g_object_unref (memory);

	return g_bytes_new_take (data, data_size);
}

#ifdef G_OS_UNIX
typedef struct
{
	guint timeout_id;
} LoaderErrorTestData;

static gboolean
load_should_not_block_cb (gpointer user_data)
{
	g_assert_not_reached ();
	return G_SOURCE_REMOVE;
}

static void
load_non_regular_file_cb (GtkSourceFileLoader *loader,
                          GAsyncResult        *result,
                          LoaderErrorTestData *data)
{
	GtkSourceBuffer *buffer;
	GtkTextIter start;
	GtkTextIter end;
	GError *error = NULL;

	g_source_remove (data->timeout_id);

	g_assert_false (gtk_source_file_loader_load_finish (loader, result, &error));
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_REGULAR_FILE);
	g_clear_error (&error);

	buffer = gtk_source_file_loader_get_buffer (loader);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	g_assert_true (gtk_text_iter_equal (&start, &end));

	g_main_loop_quit (main_loop);
}

static void
test_non_regular_file_rejected_before_read (void)
{
	GFile *location;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	LoaderErrorTestData data;
	GError *error = NULL;
	char *dir;
	char *path;

	main_loop = g_main_loop_new (NULL, FALSE);
	dir = g_dir_make_tmp ("gtksourceview-file-loader-XXXXXX", &error);
	g_assert_no_error (error);
	path = g_build_filename (dir, "fifo", NULL);

	g_assert_cmpint (mkfifo (path, 0600), ==, 0);

	location = g_file_new_for_path (path);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	gtk_source_file_set_location (file, location);
	loader = gtk_source_file_loader_new (buffer, file);

	data.timeout_id = g_timeout_add_seconds (5, load_should_not_block_cb, NULL);

	gtk_source_file_loader_load_async (loader,
					   G_PRIORITY_DEFAULT,
					   NULL, NULL, NULL, NULL,
					   (GAsyncReadyCallback) load_non_regular_file_cb,
					   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_assert_cmpint (g_unlink (path), ==, 0);
	g_assert_cmpint (g_rmdir (dir), ==, 0);
	g_free (path);
	g_free (dir);
	g_object_unref (location);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}
#endif

static void
load_file_cb (GtkSourceFileLoader *loader,
	      GAsyncResult        *result,
	      LoaderTestData      *data)
{
	GError *error = NULL;

	gtk_source_file_loader_load_finish (loader, result, &error);
	g_assert_no_error (error);

	if (data->expected_buffer_contents != NULL)
	{
		GtkSourceBuffer *buffer;
		GtkTextIter start;
		GtkTextIter end;
		gchar *buffer_contents;

		buffer = gtk_source_file_loader_get_buffer (loader);

		gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
		buffer_contents = gtk_text_iter_get_slice (&start, &end);

		g_assert_cmpstr (buffer_contents, ==, data->expected_buffer_contents);

		g_free (buffer_contents);
	}

	if (data->newline_type != -1)
	{
		g_assert_cmpint (gtk_source_file_loader_get_newline_type (loader),
		                 ==,
		                 data->newline_type);
	}

	/* finished */
	g_main_loop_quit (main_loop);
}

static void
test_loader (const gchar *filename,
             const gchar *contents,
             const gchar *expected_buffer_contents,
             gint         newline_type)
{
	GFile *location;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings;
	LoaderTestData *data;
	GError *error = NULL;

	main_loop = g_main_loop_new (NULL, FALSE);

	g_file_set_contents (filename, contents, -1, &error);
	g_assert_no_error (error);

	location = g_file_new_for_path (filename);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	gtk_source_file_set_location (file, location);
	loader = gtk_source_file_loader_new (buffer, file);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);

	data = g_slice_new (LoaderTestData);
	data->expected_buffer_contents = expected_buffer_contents;
	data->newline_type = newline_type;

	gtk_source_file_loader_load_async (loader,
					   G_PRIORITY_DEFAULT,
					   NULL, NULL, NULL, NULL,
					   (GAsyncReadyCallback) load_file_cb,
					   data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_slice_free (LoaderTestData, data);
	delete_file (location);
	g_object_unref (location);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
	g_slist_free (candidate_encodings);
}

static void
test_end_line_stripping (void)
{
	test_loader ("file-loader.txt",
	             "hello world\n",
	             "hello world",
	             -1);

	test_loader ("file-loader.txt",
	             "hello world",
	             "hello world",
	             -1);

	test_loader ("file-loader.txt",
	             "\nhello world",
	             "\nhello world",
	             -1);

	test_loader ("file-loader.txt",
	             "\nhello world\n",
	             "\nhello world",
	             -1);

	test_loader ("file-loader.txt",
	             "hello world\n\n",
	             "hello world\n",
	             -1);

	test_loader ("file-loader.txt",
	             "hello world\r\n",
	             "hello world",
	             -1);

	test_loader ("file-loader.txt",
	             "hello world\r\n\r\n",
	             "hello world\r\n",
	             -1);

	test_loader ("file-loader.txt",
	             "\n",
	             "",
	             -1);

	test_loader ("file-loader.txt",
	             "\r\n",
	             "",
	             -1);

	test_loader ("file-loader.txt",
	             "\n\n",
	             "\n",
	             -1);

	test_loader ("file-loader.txt",
	             "\r\n\r\n",
	             "\r\n",
	             -1);
}

static void
test_end_new_line_detection (void)
{
	test_loader ("file-loader.txt",
	             "hello world\n",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_LF);

	test_loader ("file-loader.txt",
	             "hello world\r\n",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_CR_LF);

	test_loader ("file-loader.txt",
	             "hello world\r",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_CR);
}

static void
test_begin_new_line_detection (void)
{
	test_loader ("file-loader.txt",
	             "\nhello world",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_LF);

	test_loader ("file-loader.txt",
	             "\r\nhello world",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_CR_LF);

	test_loader ("file-loader.txt",
	             "\rhello world",
	             NULL,
	             GTK_SOURCE_NEWLINE_TYPE_CR);
}

static void
test_failure_after_partial_read (void)
{
	static const guint8 contents[] = "first chunk\nsecond chunk\n";
	GInputStream *stream;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };

	main_loop = g_main_loop_new (NULL, FALSE);

	stream = test_input_stream_new (contents, sizeof contents - 1, 12, TRUE);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);

	data.expected_domain = G_IO_ERROR;
	data.expected_code = G_IO_ERROR_FAILED;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_slist_free (candidate_encodings);
	g_object_unref (stream);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

static void
test_cancellation_after_partial_insertion (void)
{
	static const guint8 contents[] = "first chunk\nsecond chunk\n";
	GInputStream *stream;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GCancellable *cancellable;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };

	main_loop = g_main_loop_new (NULL, FALSE);

	stream = test_input_stream_new (contents, sizeof contents - 1, 12, FALSE);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);
	cancellable = g_cancellable_new ();

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);

	data.cancellable = cancellable;
	data.expected_domain = G_IO_ERROR;
	data.expected_code = G_IO_ERROR_CANCELLED;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "insert-text",
	                  G_CALLBACK (cancel_on_insert_text_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   cancellable, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_slist_free (candidate_encodings);
	g_object_unref (cancellable);
	g_object_unref (stream);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

static void
test_conversion_failure_after_stream_initialization (void)
{
	static const guint8 contents[] = {
		0x41, 0x00,
		0x00, 0xd8, 0x41, 0x00
	};
	GInputStream *stream;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };

	main_loop = g_main_loop_new (NULL, FALSE);

	stream = test_input_stream_new (contents, sizeof contents, 2, FALSE);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_from_charset ("UTF-16LE"));
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);

	data.expected_domain = G_CONVERT_ERROR;
	data.expected_code = G_CONVERT_ERROR_ILLEGAL_SEQUENCE;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_slist_free (candidate_encodings);
	g_object_unref (stream);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

static void
test_max_size_stream (void)
{
	static const guint8 contents[] = "0123456789abcdef\n";
	GInputStream *stream;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };

	main_loop = g_main_loop_new (NULL, FALSE);

	stream = test_input_stream_new (contents, sizeof contents - 1, 0, FALSE);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);
	gtk_source_file_loader_set_max_size (loader, 8);
	g_assert_cmpuint (gtk_source_file_loader_get_max_size (loader), ==, 8);

	data.expected_domain = GTK_SOURCE_FILE_LOADER_ERROR;
	data.expected_code = GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	g_slist_free (candidate_encodings);
	g_object_unref (stream);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

static void
test_max_size_file (void)
{
	GFile *location;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };
	GError *error = NULL;
	char *filename;

	main_loop = g_main_loop_new (NULL, FALSE);

	filename = g_build_filename (g_get_tmp_dir (), "gtksourceview-file-loader-too-big.txt", NULL);
	g_file_set_contents (filename, "0123456789abcdef\n", -1, &error);
	g_assert_no_error (error);

	location = g_file_new_for_path (filename);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	gtk_source_file_set_location (file, location);
	loader = gtk_source_file_loader_new (buffer, file);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);
	gtk_source_file_loader_set_max_size (loader, 8);

	data.expected_domain = GTK_SOURCE_FILE_LOADER_ERROR;
	data.expected_code = GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	delete_file (location);
	g_free (filename);
	g_slist_free (candidate_encodings);
	g_object_unref (location);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

static void
test_max_size_gzip_file (void)
{
	GBytes *gzip_data;
	GFile *location;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };
	GError *error = NULL;
	const char *contents;
	const char *gzip_contents;
	gsize gzip_length;
	char *filename;

	main_loop = g_main_loop_new (NULL, FALSE);

	contents = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	           "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	gzip_data = create_gzip_data (contents, strlen (contents));
	gzip_contents = g_bytes_get_data (gzip_data, &gzip_length);

	filename = g_build_filename (g_get_tmp_dir (), "gtksourceview-file-loader-too-big.txt.gz", NULL);
	g_file_set_contents (filename, gzip_contents, gzip_length, &error);
	g_assert_no_error (error);

	location = g_file_new_for_path (filename);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	gtk_source_file_set_location (file, location);
	loader = gtk_source_file_loader_new (buffer, file);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);
	gtk_source_file_loader_set_max_size (loader, 32);

	data.expected_domain = GTK_SOURCE_FILE_LOADER_ERROR;
	data.expected_code = GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG;

	g_signal_connect (buffer,
	                  "begin-user-action",
	                  G_CALLBACK (begin_user_action_cb),
	                  &data);
	g_signal_connect (buffer,
	                  "end-user-action",
	                  G_CALLBACK (end_user_action_cb),
	                  &data);

	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_failure_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	delete_file (location);
	g_bytes_unref (gzip_data);
	g_free (filename);
	g_slist_free (candidate_encodings);
	g_object_unref (location);
	g_object_unref (buffer);
	g_object_unref (file);
	g_object_unref (loader);
}

gint
main (gint   argc,
      gchar *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/file-loader/end-line-stripping", test_end_line_stripping);
	g_test_add_func ("/file-loader/end-new-line-detection", test_end_new_line_detection);
	g_test_add_func ("/file-loader/begin-new-line-detection", test_begin_new_line_detection);
	g_test_add_func ("/file-loader/failure-after-partial-read", test_failure_after_partial_read);
	g_test_add_func ("/file-loader/cancellation-after-partial-insertion", test_cancellation_after_partial_insertion);
	g_test_add_func ("/file-loader/conversion-failure-after-stream-initialization", test_conversion_failure_after_stream_initialization);
	g_test_add_func ("/file-loader/max-size-stream", test_max_size_stream);
	g_test_add_func ("/file-loader/max-size-file", test_max_size_file);
	g_test_add_func ("/file-loader/max-size-gzip-file", test_max_size_gzip_file);
#ifdef G_OS_UNIX
	g_test_add_func ("/file-loader/non-regular-file-rejected-before-read",
	                 test_non_regular_file_rejected_before_read);
#endif

	return g_test_run ();
}

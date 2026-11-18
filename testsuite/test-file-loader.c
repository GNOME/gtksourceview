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
	guint overflow_after_first_read : 1;
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

	if (self->overflow_after_first_read && self->n_reads > 0)
	{
		self->n_reads++;
		return G_MAXSSIZE;
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

static GInputStream *
test_input_stream_new_overflow_after_first_read (const guint8 *contents,
                                                 gsize         length)
{
	TestInputStream *stream;

	stream = (TestInputStream *)test_input_stream_new (contents, length, 1, FALSE);
	stream->overflow_after_first_read = TRUE;

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
test_utf16le_crlf_chunk_boundary (void)
{
	const GtkSourceEncoding *encoding;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GInputStream *stream;
	GSList *candidate_encodings;
	LoaderTestData data;
	guint8 contents[8196];
	char expected[4099];
	unsigned int i;

	for (i = 0; i < 4095; i++)
	{
		contents[2 * i] = 'A';
		contents[1 + 2 * i] = 0;
	}
	contents[8190] = '\r';
	contents[8191] = 0;
	contents[8192] = '\n';
	contents[8193] = 0;
	contents[8194] = 'B';
	contents[8195] = 0;

	memset (expected, 'A', 4095);
	memcpy (expected + 4095, "\r\nB", 4);

	main_loop = g_main_loop_new (NULL, FALSE);
	stream = test_input_stream_new (contents, sizeof contents, 8192, FALSE);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);

	encoding = gtk_source_encoding_get_from_charset ("UTF-16LE");
	candidate_encodings = g_slist_prepend (NULL, (gpointer)encoding);
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);

	data.expected_buffer_contents = expected;
	data.newline_type = -1;
	gtk_source_file_loader_load_async (loader,
	                                   G_PRIORITY_DEFAULT,
	                                   NULL, NULL, NULL, NULL,
	                                   (GAsyncReadyCallback) load_file_cb,
	                                   &data);

	g_main_loop_run (main_loop);
	g_assert_true (gtk_source_file_loader_get_encoding (loader) == encoding);

	g_main_loop_unref (main_loop);
	g_slist_free (candidate_encodings);
	g_object_unref (loader);
	g_object_unref (file);
	g_object_unref (buffer);
	g_object_unref (stream);
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
test_overflow_size_stream (void)
{
	static const guint8 contents[] = "xy";
	GInputStream *stream;
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GSList *candidate_encodings = NULL;
	LoaderFailureTestData data = { 0 };

	main_loop = g_main_loop_new (NULL, FALSE);

	stream = test_input_stream_new_overflow_after_first_read (contents, sizeof contents - 1);
	buffer = gtk_source_buffer_new (NULL);
	file = gtk_source_file_new ();
	loader = gtk_source_file_loader_new_from_stream (buffer, file, stream);

	candidate_encodings = g_slist_prepend (NULL, (gpointer) gtk_source_encoding_get_utf8 ());
	gtk_source_file_loader_set_candidate_encodings (loader, candidate_encodings);
	gtk_source_file_loader_set_max_size (loader, 0);

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

typedef enum
{
	POLICY_DEFAULT,
	POLICY_USE,
	POLICY_USE_LATER,
	POLICY_IGNORE,
	POLICY_IGNORE_LATER,
	POLICY_CANCEL
} PolicyAnswer;

typedef struct
{
	GtkSourceBuffer *buffer;
	GtkSourceFile *file;
	GtkSourceFileLoader *loader;
	GInputStream *stream;
	GFile *location;
	GCancellable *cancellable;
	GtkSourceFileLoaderPolicyDecision *held;
	PolicyAnswer answer;
	GtkSourceFileLoaderPolicyReason reasons[3];
	guint64 values[3];
	guint64 thresholds[3];
	GError *error;
	guint n_decisions;
} PolicyTestData;

static const char policy_contents[] = "abcd\nx";

static void
policy_test_setup (PolicyTestData *data,
                   gconstpointer   user_data)
{
	const char *filename = user_data;
	GSList encodings = { (gpointer)gtk_source_encoding_get_utf8 (), NULL };

	data->buffer = gtk_source_buffer_new (NULL);
	data->file = gtk_source_file_new ();
	data->cancellable = g_cancellable_new ();
	data->answer = POLICY_USE;
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (data->buffer), "original", -1);
	gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (data->buffer), FALSE);

	if (filename != NULL)
	{
		GBytes *bytes;
		GError *error = NULL;

		bytes = g_str_has_suffix (filename, ".gz")
		      ? create_gzip_data (policy_contents, strlen (policy_contents))
		      : g_bytes_new_static (policy_contents, strlen (policy_contents));
		data->location = g_file_new_build_filename (g_get_tmp_dir (), filename, NULL);
		g_file_set_contents (g_file_peek_path (data->location),
		                     g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error);
		g_assert_no_error (error);
		g_bytes_unref (bytes);

		gtk_source_file_set_location (data->file, data->location);
		data->loader = gtk_source_file_loader_new (data->buffer, data->file);
	}
	else
	{
		data->stream = test_input_stream_new ((const guint8 *)policy_contents,
		                                      strlen (policy_contents), 2, FALSE);
		data->loader = gtk_source_file_loader_new_from_stream (data->buffer, data->file, data->stream);
	}

	gtk_source_file_loader_set_candidate_encodings (data->loader, &encodings);
	gtk_source_file_loader_set_size_threshold (data->loader, 2);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 0);
}

static void
policy_test_teardown (PolicyTestData *data,
                      gconstpointer   user_data)
{
	if (data->location != NULL)
		delete_file (data->location);

	g_clear_object (&data->held);
	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	g_clear_object (&data->file);
	g_clear_object (&data->buffer);
	g_clear_object (&data->location);
	g_clear_object (&data->cancellable);
	g_clear_error (&data->error);
}

static gboolean
answer_policy_cb (gpointer user_data)
{
	PolicyTestData *data = user_data;

	if (data->answer == POLICY_CANCEL)
	{
		g_cancellable_cancel (data->cancellable);
	}
	else if (data->answer == POLICY_IGNORE || data->answer == POLICY_IGNORE_LATER)
	{
		gtk_source_file_loader_policy_decision_ignore (data->held);
		gtk_source_file_loader_policy_decision_use (data->held);
	}
	else
	{
		gtk_source_file_loader_policy_decision_use (data->held);
		gtk_source_file_loader_policy_decision_ignore (data->held);
	}

	g_clear_object (&data->held);

	return G_SOURCE_REMOVE;
}

static gboolean
decide_policy_cb (GtkSourceFileLoader               *loader,
                  GtkSourceFileLoaderPolicyDecision *decision,
                  PolicyTestData                    *data)
{
	guint index = data->n_decisions++;

	g_assert_cmpuint (index, <, G_N_ELEMENTS (data->reasons));
	g_assert_null (data->held);

	data->reasons[index] = gtk_source_file_loader_policy_decision_get_reason (decision);
	data->values[index] = gtk_source_file_loader_policy_decision_get_value (decision);
	data->thresholds[index] = gtk_source_file_loader_policy_decision_get_threshold (decision);
	data->held = g_object_ref (decision);

	if (data->answer == POLICY_USE_LATER ||
	    data->answer == POLICY_IGNORE_LATER ||
	    data->answer == POLICY_CANCEL)
		g_idle_add (answer_policy_cb, data);
	else
		answer_policy_cb (data);

	return TRUE;
}

static void
policy_load_cb (GtkSourceFileLoader *loader,
                GAsyncResult        *result,
                PolicyTestData      *data)
{
	gboolean loaded = gtk_source_file_loader_load_finish (loader, result, &data->error);

	g_assert_cmpint (loaded, ==, data->error == NULL);
	g_main_loop_quit (main_loop);
}

static void
policy_load (PolicyTestData *data,
             const char     *expected_text)
{
	char *text;

	if (data->answer != POLICY_DEFAULT)
		g_signal_connect (data->loader, "decide-policy", G_CALLBACK (decide_policy_cb), data);

	main_loop = g_main_loop_new (NULL, FALSE);
	gtk_source_file_loader_load_async (data->loader, G_PRIORITY_DEFAULT, data->cancellable,
	                                   NULL, NULL, NULL, (GAsyncReadyCallback)policy_load_cb, data);
	g_main_loop_run (main_loop);
	g_main_loop_unref (main_loop);

	if (expected_text != NULL)
	{
		g_object_get (data->buffer, "text", &text, NULL);
		g_assert_cmpstr (text, ==, expected_text);
		g_free (text);
	}
}

static void
test_policy_stream (PolicyTestData *data,
                    gconstpointer   user_data)
{
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 3);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 1);
	data->answer = POLICY_USE_LATER;

	policy_load (data, policy_contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 2);
	g_assert_cmpint (data->reasons[0], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH);
	g_assert_cmpint (data->reasons[1], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT);
	g_assert_cmpuint (data->values[0], ==, 4);
	g_assert_cmpuint (data->thresholds[0], ==, 3);
	g_assert_cmpuint (data->values[1], ==, 2);
	g_assert_cmpuint (data->thresholds[1], ==, 1);
}

static void
test_policy_line_count_use (PolicyTestData *data,
                            gconstpointer   user_data)
{
	const char *contents = "abcd\nabc";

	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	data->stream = test_input_stream_new ((const guint8 *)contents, strlen (contents), 0, FALSE);
	data->loader = gtk_source_file_loader_new_from_stream (data->buffer, data->file, data->stream);
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 5);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 1);
	data->answer = POLICY_USE_LATER;

	policy_load (data, contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpint (data->reasons[0], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT);
	g_assert_cmpuint (data->values[0], ==, 2);
}

static void
test_policy_early_reject (PolicyTestData *data,
                          gconstpointer   user_data)
{
	data->answer = POLICY_IGNORE;

	policy_load (data, "original");

	g_assert_error (data->error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpuint (data->values[0], ==, strlen (policy_contents));
	g_assert_true (g_file_equal (gtk_source_file_get_location (data->file), data->location));
	g_assert_false (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (data->buffer)));
}

static void
test_policy_stream_answers (void)
{
	PolicyAnswer answer;

	for (answer = POLICY_DEFAULT; answer <= POLICY_CANCEL; answer++)
	{
		PolicyTestData data = { 0 };
		gboolean reject = answer == POLICY_IGNORE || answer == POLICY_IGNORE_LATER;

		policy_test_setup (&data, NULL);
		data.answer = answer;
		policy_load (&data, reject || answer == POLICY_CANCEL ? "ab" : policy_contents);

		g_assert_cmpuint (data.n_decisions, ==, answer == POLICY_DEFAULT ? 0 : 1);

		if (answer != POLICY_DEFAULT)
		{
			g_assert_cmpint (data.reasons[0], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE);
			g_assert_cmpuint (data.values[0], ==, strlen (policy_contents));
			g_assert_cmpuint (data.thresholds[0], ==, 2);
		}

		if (reject)
		{
			g_assert_error (data.error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG);
			g_assert_true (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (data.buffer)));
			g_assert_null (gtk_source_file_get_location (data.file));
		}
		else if (answer == POLICY_CANCEL)
		{
			g_assert_error (data.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
			g_assert_cmpuint (((TestInputStream *)data.stream)->n_reads, ==, 2);
		}
		else
		{
			g_assert_no_error (data.error);
		}

		policy_test_teardown (&data, NULL);
	}
}

static void
test_policy_file_line_reject (PolicyTestData *data,
                              gconstpointer   user_data)
{
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 3);
	data->answer = POLICY_IGNORE;

	policy_load (data, policy_contents);

	g_assert_error (data->error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_LINE_TOO_LONG);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpuint (data->values[0], ==, 4);
	g_assert_true (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (data->buffer)));
	g_assert_null (gtk_source_file_get_location (data->file));
}

static void
test_policy_size_use (PolicyTestData *data,
                      gconstpointer   user_data)
{
	data->answer = POLICY_USE_LATER;

	policy_load (data, policy_contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpint (data->reasons[0], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE);
	g_assert_cmpuint (data->values[0], ==, strlen (policy_contents));
	g_assert_cmpuint (data->thresholds[0], ==, 2);
	g_assert_true (g_file_equal (gtk_source_file_get_location (data->file), data->location));
}

static void
test_policy_line_count_reject (PolicyTestData *data,
                               gconstpointer   user_data)
{
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 1);
	data->answer = POLICY_IGNORE;

	policy_load (data, policy_contents);

	g_assert_error (data->error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_TOO_MANY_LINES);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpuint (data->values[0], ==, 2);
	g_assert_true (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (data->buffer)));
}

static void
test_policy_exact_thresholds (PolicyTestData *data,
                              gconstpointer   user_data)
{
	gtk_source_file_loader_set_size_threshold (data->loader, strlen (policy_contents));
	gtk_source_file_loader_set_line_length_threshold (data->loader, 4);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 2);

	policy_load (data, policy_contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 0);
}

static void
test_policy_line_boundaries (PolicyTestData *data,
                             gconstpointer   user_data)
{
	const char *contents = "abc\rdef\r\nghi";
	GError *error = NULL;

	g_file_set_contents (g_file_peek_path (data->location), contents, -1, &error);
	g_assert_no_error (error);
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 3);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 3);

	policy_load (data, contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 0);
}

static void
test_policy_trailing_newline (PolicyTestData *data,
                              gconstpointer   user_data)
{
	const char *contents = "abc\ndef\n";
	GError *error = NULL;

	g_file_set_contents (g_file_peek_path (data->location), contents, -1, &error);
	g_assert_no_error (error);
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 2);

	policy_load (data, "abc\ndef");

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 0);
}

static void
test_policy_line_length_boundaries (void)
{
	const struct
	{
		const char *contents;
		gsize first_chunk_size;
	} cases[] = {
		{ "\nabc", 0 },
		{ "abc\n\ndef", 0 },
		{ "abc\ndef", 3 },
		{ "abc\ndef", 4 },
		{ "abc\n\ndef", 4 },
		{ "\nαβγ", 0 },
	};

	for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
	{
		PolicyTestData data = { 0 };
		const char *contents = cases[i].contents;

		g_test_message ("Line boundary case %u", i);
		policy_test_setup (&data, NULL);
		g_clear_object (&data.loader);
		g_clear_object (&data.stream);
		data.stream = test_input_stream_new ((const guint8 *)contents, strlen (contents),
		                                     cases[i].first_chunk_size, FALSE);
		data.loader = gtk_source_file_loader_new_from_stream (data.buffer, data.file, data.stream);
		gtk_source_file_loader_set_size_threshold (data.loader, 0);
		gtk_source_file_loader_set_line_length_threshold (data.loader, 3);
		gtk_source_file_loader_set_line_count_threshold (data.loader, 0);

		policy_load (&data, contents);

		g_assert_no_error (data.error);
		g_assert_cmpuint (data.n_decisions, ==, 0);
		policy_test_teardown (&data, NULL);
	}
}

static void
test_policy_newline_chunks (PolicyTestData *data,
                            gconstpointer   user_data)
{
	char *contents;

	contents = g_strnfill (10 * 8192, 'x');
	for (guint i = 0; i < 10; i++)
		contents[(i + 1) * 8192 - 1] = '\n';

	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	data->stream = test_input_stream_new ((const guint8 *)contents, strlen (contents), 0, FALSE);
	data->loader = gtk_source_file_loader_new_from_stream (data->buffer, data->file, data->stream);
	gtk_source_buffer_set_implicit_trailing_newline (data->buffer, FALSE);
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 0);

	policy_load (data, contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 0);
	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	g_free (contents);
}

static void
test_policy_long_line_chunks (PolicyTestData *data,
                              gconstpointer   user_data)
{
	char *contents = g_strnfill (25000, 'x');

	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	data->stream = test_input_stream_new ((const guint8 *)contents, strlen (contents), 0, FALSE);
	data->loader = gtk_source_file_loader_new_from_stream (data->buffer, data->file, data->stream);
	gtk_source_file_loader_set_size_threshold (data->loader, 0);
	gtk_source_file_loader_set_line_length_threshold (data->loader, 20000);
	gtk_source_file_loader_set_line_count_threshold (data->loader, 0);

	policy_load (data, contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpint (data->reasons[0], ==, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH);
	g_assert_cmpuint (data->values[0], >, 20000);
	g_clear_object (&data->loader);
	g_clear_object (&data->stream);
	g_free (contents);
}

static gboolean
decide_policy_release_buffer_cb (GtkSourceFileLoader               *loader,
                                 GtkSourceFileLoaderPolicyDecision *decision,
                                 PolicyTestData                    *data)
{
	g_assert_cmpint (gtk_source_file_loader_policy_decision_get_reason (decision), ==,
	                 GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE);
	g_clear_object (&data->buffer);
	g_assert_nonnull (gtk_source_file_loader_get_buffer (loader));
	gtk_source_file_loader_policy_decision_use (decision);
	data->n_decisions++;

	return TRUE;
}

static void
test_policy_buffer_lifetime (PolicyTestData *data,
                             gconstpointer   user_data)
{
	data->answer = POLICY_DEFAULT;
	g_signal_connect (data->loader, "decide-policy",
	                  G_CALLBACK (decide_policy_release_buffer_cb), data);
	policy_load (data, NULL);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 1);
}

static gboolean
decide_policy_release_file_cb (GtkSourceFileLoader               *loader,
                               GtkSourceFileLoaderPolicyDecision *decision,
                               PolicyTestData                    *data)
{
	g_assert_cmpint (gtk_source_file_loader_policy_decision_get_reason (decision), ==,
	                 GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE);
	g_clear_object (&data->file);
	g_assert_nonnull (gtk_source_file_loader_get_file (loader));
	data->held = g_object_ref (decision);
	g_idle_add (answer_policy_cb, data);
	data->n_decisions++;

	return TRUE;
}

static void
test_policy_file_lifetime (PolicyTestData *data,
                           gconstpointer   user_data)
{
	data->answer = POLICY_USE_LATER;
	g_signal_connect (data->loader, "decide-policy",
	                  G_CALLBACK (decide_policy_release_file_cb), data);
	policy_load (data, policy_contents);

	g_assert_no_error (data->error);
	g_assert_cmpuint (data->n_decisions, ==, 1);
}

static void
test_policy_hard_cap_after_use (PolicyTestData *data,
                                gconstpointer   user_data)
{
	gtk_source_file_loader_set_size_threshold (data->loader, 1);
	gtk_source_file_loader_set_max_size (data->loader, 4);

	policy_load (data, "ab");

	g_assert_error (data->error, GTK_SOURCE_FILE_LOADER_ERROR, GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG);
	g_assert_cmpuint (data->n_decisions, ==, 1);
	g_assert_cmpuint (data->values[0], ==, 2);
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
	g_test_add_func ("/file-loader/utf16le-crlf-chunk-boundary", test_utf16le_crlf_chunk_boundary);
	g_test_add_func ("/file-loader/max-size-stream", test_max_size_stream);
	g_test_add_func ("/file-loader/overflow-size-stream", test_overflow_size_stream);
	g_test_add_func ("/file-loader/max-size-file", test_max_size_file);
	g_test_add_func ("/file-loader/max-size-gzip-file", test_max_size_gzip_file);
	g_test_add ("/file-loader/policy-stream", PolicyTestData, NULL, policy_test_setup, test_policy_stream, policy_test_teardown);
	g_test_add ("/file-loader/policy-line-count-use", PolicyTestData, NULL, policy_test_setup, test_policy_line_count_use, policy_test_teardown);
	g_test_add ("/file-loader/policy-early-reject", PolicyTestData, "gtksourceview-policy.txt", policy_test_setup, test_policy_early_reject, policy_test_teardown);
	g_test_add_func ("/file-loader/policy-stream-answers", test_policy_stream_answers);
	g_test_add ("/file-loader/policy-file-line-reject", PolicyTestData, "gtksourceview-policy.txt", policy_test_setup, test_policy_file_line_reject, policy_test_teardown);
	g_test_add ("/file-loader/policy-gzip", PolicyTestData, "gtksourceview-policy.txt.gz", policy_test_setup, test_policy_size_use, policy_test_teardown);
	g_test_add ("/file-loader/policy-line-count-reject", PolicyTestData, NULL, policy_test_setup, test_policy_line_count_reject, policy_test_teardown);
	g_test_add ("/file-loader/policy-exact-thresholds", PolicyTestData, NULL, policy_test_setup, test_policy_exact_thresholds, policy_test_teardown);
	g_test_add ("/file-loader/policy-line-boundaries", PolicyTestData, "gtksourceview-policy-lines.txt", policy_test_setup, test_policy_line_boundaries, policy_test_teardown);
	g_test_add ("/file-loader/policy-trailing-newline", PolicyTestData, "gtksourceview-policy-lines.txt", policy_test_setup, test_policy_trailing_newline, policy_test_teardown);
	g_test_add ("/file-loader/policy-long-line-chunks", PolicyTestData, NULL, policy_test_setup, test_policy_long_line_chunks, policy_test_teardown);
	g_test_add_func ("/file-loader/policy-line-length-boundaries", test_policy_line_length_boundaries);
	g_test_add ("/file-loader/policy-newline-chunks", PolicyTestData, NULL, policy_test_setup, test_policy_newline_chunks, policy_test_teardown);
	g_test_add ("/file-loader/policy-buffer-lifetime", PolicyTestData, "gtksourceview-policy-lines.txt", policy_test_setup, test_policy_buffer_lifetime, policy_test_teardown);
	g_test_add ("/file-loader/policy-file-lifetime", PolicyTestData, "gtksourceview-policy.txt", policy_test_setup, test_policy_file_lifetime, policy_test_teardown);
	g_test_add ("/file-loader/policy-file-deferred-use", PolicyTestData, "gtksourceview-policy.txt", policy_test_setup, test_policy_size_use, policy_test_teardown);
	g_test_add ("/file-loader/policy-hard-cap-after-use", PolicyTestData, NULL, policy_test_setup, test_policy_hard_cap_after_use, policy_test_teardown);
#ifdef G_OS_UNIX
	g_test_add_func ("/file-loader/non-regular-file-rejected-before-read",
	                 test_non_regular_file_rejected_before_read);
#endif

	return g_test_run ();
}

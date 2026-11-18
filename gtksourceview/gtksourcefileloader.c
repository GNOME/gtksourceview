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
 *
 * A load asks for an application decision when its expanded size, decoded line
 * length, or line count exceeds the configured threshold. The
 * [signal@FileLoader::decide-policy] handler may answer immediately or retain
 * the decision while showing a dialog. If no handler claims it, loading
 * continues. The default thresholds are 32 MiB, 65,536 characters, and
 * 750,000 lines. Setting a threshold to zero disables that policy check.
 * The existing `max-size` limit always rejects oversized input,
 * regardless of policy decisions.
 *
 * For example, a dialog handler can retain the decision until its callback:
 *
 * ```c
 * static void
 * policy_dialog_done (GObject      *source,
 *                     GAsyncResult *result,
 *                     gpointer      user_data)
 * {
 *   GtkSourceFileLoaderPolicyDecision *decision = user_data;
 *   int response = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (source), result, NULL);
 *
 *   if (response == 0)
 *     gtk_source_file_loader_policy_decision_use (decision);
 *   else
 *     gtk_source_file_loader_policy_decision_ignore (decision);
 *
 *   g_object_unref (decision);
 * }
 *
 * static gboolean
 * decide_policy (GtkSourceFileLoader               *loader,
 *                GtkSourceFileLoaderPolicyDecision *decision,
 *                GtkWindow                         *parent)
 * {
 *   GtkAlertDialog *dialog = gtk_alert_dialog_new ("Continue loading this file?");
 *   const char *buttons[] = { "Continue", "Cancel", NULL };
 *
 *   gtk_alert_dialog_set_buttons (dialog, buttons);
 *   gtk_alert_dialog_choose (dialog, parent, NULL, policy_dialog_done,
 *                            g_object_ref (decision));
 *   g_object_unref (dialog);
 *   return TRUE;
 * }
 * ```
 *
 * A declined decision after insertion leaves partial text marked modified and
 * clears the [class@File] location. The application should offer Save As if the
 * user wants to keep that text.
 */

enum
{
	PROP_0,
	PROP_BUFFER,
	PROP_FILE,
	PROP_LOCATION,
	PROP_INPUT_STREAM,
	PROP_MAX_SIZE,
	PROP_SIZE_THRESHOLD,
	PROP_LINE_LENGTH_THRESHOLD,
	PROP_LINE_COUNT_THRESHOLD,
	N_PROPS
};

enum
{
	DECIDE_POLICY,
	N_SIGNALS
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

#define READ_N_PAGES 2
#define READ_CHUNK_SIZE (_gtk_source_utils_get_page_size()*READ_N_PAGES)
#define DEFAULT_MAX_SIZE ((guint64)1024 * 1024 * 1024)
#define DEFAULT_SIZE_THRESHOLD ((guint64)32 * 1024 * 1024)
#define DEFAULT_LINE_LENGTH_THRESHOLD 65536
#define DEFAULT_LINE_COUNT_THRESHOLD 750000
#define LOADER_QUERY_ATTRIBUTES G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
				G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
				G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
				G_FILE_ATTRIBUTE_TIME_CREATED "," \
				G_FILE_ATTRIBUTE_TIME_CREATED_USEC "," \
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
	guint64 max_size;
	guint64 size_threshold;
	guint64 line_length_threshold;
	guint64 line_count_threshold;

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
	GtkSourceBuffer *source_buffer;
	GtkSourceFile *file;

	GFileInfo *info;

	GFileProgressCallback progress_cb;
	gpointer progress_cb_data;
	GDestroyNotify progress_cb_notify;

	goffset total_bytes_read;
	goffset total_size;

	gssize chunk_bytes_read;
	gchar *chunk_buffer;
	GtkSourceFileLoaderPolicyDecision *decision;
	GSource *next_source;
	gulong cancelled_handler;
	guint checked_offset;
	guint64 checked_line_length;
	guint seen_reasons;
	guint destination_started : 1;
	guint finished : 1;
	guint next_step : 3;

	guint guess_content_type_from_content : 1;
	guint tried_mount : 1;
} TaskData;

struct _GtkSourceFileLoaderPolicyDecision
{
	GObject parent_instance;
	GWeakRef task;
	GtkSourceFileLoaderPolicyReason reason;
	guint64 value;
	guint64 threshold;
	guint answered : 1;
	guint ignored : 1;
};

/**
 * GtkSourceFileLoaderPolicyDecision:
 *
 * An application decision requested by [class@FileLoader].
 *
 * The signal argument is borrowed. Retain it with g_object_ref() before
 * returning %TRUE from the signal handler if it will be answered later.
 * Answer with [method@FileLoaderPolicyDecision.use] or
 * [method@FileLoaderPolicyDecision.ignore]. The decision becomes inert after
 * the load completes.
 *
 * Since: 5.24
 */
G_DEFINE_TYPE (GtkSourceFileLoaderPolicyDecision, gtk_source_file_loader_policy_decision, G_TYPE_OBJECT)

static void
gtk_source_file_loader_policy_decision_finalize (GObject *object)
{
	GtkSourceFileLoaderPolicyDecision *decision = GTK_SOURCE_FILE_LOADER_POLICY_DECISION (object);

	g_weak_ref_clear (&decision->task);
	G_OBJECT_CLASS (gtk_source_file_loader_policy_decision_parent_class)->finalize (object);
}

static void
gtk_source_file_loader_policy_decision_class_init (GtkSourceFileLoaderPolicyDecisionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = gtk_source_file_loader_policy_decision_finalize;
}

static void
gtk_source_file_loader_policy_decision_init (GtkSourceFileLoaderPolicyDecision *decision)
{
	g_weak_ref_init (&decision->task, NULL);
}

G_DEFINE_TYPE (GtkSourceFileLoader, gtk_source_file_loader, G_TYPE_OBJECT)

static void open_file           (GTask *task);
static void read_file           (GTask *task);
static void read_file_chunk     (GTask *task);
static void write_file_chunk    (GTask *task);
static void write_complete      (GTask *task);
static void recover_not_mounted (GTask *task);
static void schedule_next_step (GTask *task);
static void load_task_return_error (GTask *task, GError *error);
static void start_destination (GTask *task);
static gboolean check_text_policy (GTask *task);

enum
{
	NEXT_OPEN,
	NEXT_WRITE,
	NEXT_READ,
	NEXT_CLOSE
};

static void
finish_policy_error (GTask *task,
                     GtkSourceFileLoaderPolicyReason reason)
{
	GtkSourceFileLoader *loader = g_task_get_source_object (task);
	TaskData *task_data = g_task_get_task_data (task);
	GtkSourceFileLoaderError code;
	const char *message;

	switch (reason)
	{
	case GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE:
		code = GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG;
		message = _("File too big.");
		break;
	case GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH:
		code = GTK_SOURCE_FILE_LOADER_ERROR_LINE_TOO_LONG;
		message = _("File has a line that is too long.");
		break;
	case GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT:
		code = GTK_SOURCE_FILE_LOADER_ERROR_TOO_MANY_LINES;
		message = _("File has too many lines.");
		break;
	default:
		g_assert_not_reached ();
	}

	if (task_data->destination_started)
	{
		gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (loader->source_buffer), TRUE);
		gtk_source_file_set_location (loader->file, NULL);
	}

	load_task_return_error (task, g_error_new_literal (GTK_SOURCE_FILE_LOADER_ERROR, code, message));
}

static void
load_task_return_error (GTask  *task,
                        GError *error)
{
	TaskData *task_data;

	g_assert (G_IS_TASK (task));
	g_assert (error != NULL);

	task_data = g_task_get_task_data (task);
	if (task_data != NULL)
		task_data->finished = TRUE;

	if (task_data != NULL && task_data->output_stream != NULL)
	{
		gtk_source_buffer_output_stream_abort (task_data->output_stream);
	}

	if (task_data != NULL && task_data->decision != NULL)
	{
		if (task_data->cancelled_handler != 0)
		{
			g_cancellable_disconnect (g_task_get_cancellable (task), task_data->cancelled_handler);
			task_data->cancelled_handler = 0;
		}
		g_weak_ref_set (&task_data->decision->task, NULL);
		g_clear_object (&task_data->decision);
	}

	g_task_return_error (task, error);
}

static gboolean
next_step_cb (gpointer user_data)
{
	GTask *task = G_TASK (user_data);
	TaskData *task_data = g_task_get_task_data (task);
	GError *error = NULL;

	g_clear_pointer (&task_data->next_source, g_source_unref);
	if (task_data->finished)
		return G_SOURCE_REMOVE;

	if (g_cancellable_set_error_if_cancelled (g_task_get_cancellable (task), &error))
	{
		load_task_return_error (task, error);
		return G_SOURCE_REMOVE;
	}

	if (task_data->decision != NULL)
	{
		GtkSourceFileLoaderPolicyReason reason = task_data->decision->reason;

		if (!task_data->decision->answered)
			return G_SOURCE_REMOVE;

		if (task_data->decision->ignored)
		{
			finish_policy_error (task, reason);
			return G_SOURCE_REMOVE;
		}

		g_weak_ref_set (&task_data->decision->task, NULL);
		g_clear_object (&task_data->decision);

		if (task_data->cancelled_handler != 0)
		{
			g_cancellable_disconnect (g_task_get_cancellable (task), task_data->cancelled_handler);
			task_data->cancelled_handler = 0;
		}
	}

	switch (task_data->next_step)
	{
	case NEXT_OPEN:
		start_destination (task);
		read_file (task);
		break;
	case NEXT_WRITE:
		write_file_chunk (task);
		break;
	case NEXT_READ:
		if (check_text_policy (task))
			read_file_chunk (task);
		break;
	case NEXT_CLOSE:
		if (check_text_policy (task))
			write_complete (task);
		break;
	default:
		g_assert_not_reached ();
	}

	return G_SOURCE_REMOVE;
}

static void
schedule_next_step (GTask *task)
{
	TaskData *task_data = g_task_get_task_data (task);
	GSource *source;

	if (task_data->finished || task_data->next_source != NULL)
		return;

	source = g_idle_source_new ();
	g_source_set_priority (source, g_task_get_priority (task));
	g_source_set_callback (source, next_step_cb, g_object_ref (task), g_object_unref);
	g_source_attach (source, g_task_get_context (task));
	task_data->next_source = source;
}

static gboolean
cancel_pending_cb (gpointer user_data)
{
	GTask *task = G_TASK (user_data);

	schedule_next_step (task);
	return G_SOURCE_REMOVE;
}

static void
cancel_pending (GCancellable *cancellable,
                gpointer      user_data)
{
	GTask *task = G_TASK (user_data);

	g_main_context_invoke_full (g_task_get_context (task),
	                            g_task_get_priority (task),
	                            cancel_pending_cb,
	                            g_object_ref (task),
	                            g_object_unref);
}

static void
policy_decision_answer (GtkSourceFileLoaderPolicyDecision *decision,
                        gboolean                           ignore)
{
	GTask *task;

	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER_POLICY_DECISION (decision));

	if (decision->answered)
		return;

	decision->answered = TRUE;
	decision->ignored = ignore;
	task = g_weak_ref_get (&decision->task);

	if (task != NULL)
	{
		schedule_next_step (task);
		g_object_unref (task);
	}
}

/**
 * gtk_source_file_loader_policy_decision_use:
 * @decision: a policy decision.
 *
 * Continues loading. Only the first answer has an effect.
 *
 * Since: 5.24
 */
void
gtk_source_file_loader_policy_decision_use (GtkSourceFileLoaderPolicyDecision *decision)
{
	policy_decision_answer (decision, FALSE);
}

/**
 * gtk_source_file_loader_policy_decision_ignore:
 * @decision: a policy decision.
 *
 * Stops loading. Only the first answer has an effect.
 *
 * Since: 5.24
 */
void
gtk_source_file_loader_policy_decision_ignore (GtkSourceFileLoaderPolicyDecision *decision)
{
	policy_decision_answer (decision, TRUE);
}

/**
 * gtk_source_file_loader_policy_decision_get_reason:
 * @decision: a policy decision.
 *
 * Returns: the reason for this decision.
 *
 * Since: 5.24
 */
GtkSourceFileLoaderPolicyReason
gtk_source_file_loader_policy_decision_get_reason (GtkSourceFileLoaderPolicyDecision *decision)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER_POLICY_DECISION (decision), 0);

	return decision->reason;
}

/**
 * gtk_source_file_loader_policy_decision_get_value:
 * @decision: a policy decision.
 *
 * Returns: the observed value in the units of the reason.
 *
 * Since: 5.24
 */
guint64
gtk_source_file_loader_policy_decision_get_value (GtkSourceFileLoaderPolicyDecision *decision)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER_POLICY_DECISION (decision), 0);

	return decision->value;
}

/**
 * gtk_source_file_loader_policy_decision_get_threshold:
 * @decision: a policy decision.
 *
 * Returns: the configured threshold in the units of the reason.
 *
 * Since: 5.24
 */
guint64
gtk_source_file_loader_policy_decision_get_threshold (GtkSourceFileLoaderPolicyDecision *decision)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER_POLICY_DECISION (decision), 0);

	return decision->threshold;
}

static void
request_policy (GTask                               *task,
                GtkSourceFileLoaderPolicyReason       reason,
                guint64                               value,
                guint64                               threshold)
{
	GtkSourceFileLoader *loader = g_task_get_source_object (task);
	TaskData *task_data = g_task_get_task_data (task);
	gboolean handled = FALSE;

	g_assert (task_data->decision == NULL);

	task_data->seen_reasons |= (1u << reason);

	task_data->decision = g_object_new (GTK_SOURCE_TYPE_FILE_LOADER_POLICY_DECISION, NULL);
	task_data->decision->reason = reason;
	task_data->decision->value = value;
	task_data->decision->threshold = threshold;
	g_weak_ref_set (&task_data->decision->task, task);

	if (g_task_get_cancellable (task) != NULL)
		task_data->cancelled_handler = g_cancellable_connect (g_task_get_cancellable (task),
		                                                      G_CALLBACK (cancel_pending),
		                                                      g_object_ref (task),
		                                                      g_object_unref);

	g_signal_emit (loader, signals[DECIDE_POLICY], 0, task_data->decision, &handled);

	if (!handled)
		gtk_source_file_loader_policy_decision_use (task_data->decision);
}

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
	g_clear_object (&task_data->source_buffer);
	g_clear_object (&task_data->file);
	g_clear_object (&task_data->info);

	if (task_data->next_source != NULL)
	{
		g_source_destroy (task_data->next_source);
		g_clear_pointer (&task_data->next_source, g_source_unref);
	}

	if (task_data->decision != NULL)
	{
		g_weak_ref_set (&task_data->decision->task, NULL);
		g_clear_object (&task_data->decision);
	}

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

static gboolean
check_file_size (GtkSourceFileLoader  *loader,
                 TaskData             *task_data,
                 GError              **error)
{
	if (loader->max_size == 0 ||
	    !g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
	{
		return TRUE;
	}

	if (get_compression_type_from_content_type (g_file_info_get_content_type (task_data->info)) ==
	    GTK_SOURCE_COMPRESSION_TYPE_GZIP)
	{
		return TRUE;
	}

	if (g_file_info_get_attribute_uint64 (task_data->info, G_FILE_ATTRIBUTE_STANDARD_SIZE) >
	    loader->max_size)
	{
		g_set_error_literal (error,
		                     GTK_SOURCE_FILE_LOADER_ERROR,
		                     GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
		                     _("File too big."));
		return FALSE;
	}

	return TRUE;
}

static gboolean
check_expanded_size (GtkSourceFileLoader  *loader,
                     TaskData             *task_data,
                     goffset              *next_total_bytes_read,
                     GError              **error)
{
	goffset chunk_bytes_read;

	g_assert (next_total_bytes_read != NULL);

	*next_total_bytes_read = task_data->total_bytes_read;

	if (task_data->chunk_bytes_read <= 0)
	{
		return TRUE;
	}

	if ((gsize)task_data->chunk_bytes_read > READ_CHUNK_SIZE)
	{
		g_set_error_literal (error,
		                     GTK_SOURCE_FILE_LOADER_ERROR,
		                     GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
		                     _("File too big."));
		return FALSE;
	}

	if (!_gtk_source_utils_checked_gssize_to_goffset (task_data->chunk_bytes_read,
	                                                  &chunk_bytes_read) ||
	    !_gtk_source_utils_checked_add_goffset (task_data->total_bytes_read,
	                                            chunk_bytes_read,
	                                            next_total_bytes_read))
	{
		g_set_error_literal (error,
		                     GTK_SOURCE_FILE_LOADER_ERROR,
		                     GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
		                     _("File too big."));
		return FALSE;
	}

	if (loader->max_size > 0 &&
	    (guint64)*next_total_bytes_read > loader->max_size)
	{
		g_set_error_literal (error,
		                     GTK_SOURCE_FILE_LOADER_ERROR,
		                     GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
		                     _("File too big."));
		return FALSE;
	}

	return TRUE;
}

static gboolean
check_text_policy (GTask *task)
{
	GtkSourceFileLoader *loader = g_task_get_source_object (task);
	TaskData *task_data = g_task_get_task_data (task);
	GtkTextBuffer *buffer = GTK_TEXT_BUFFER (loader->source_buffer);
	GtkTextIter iter;
	GtkTextIter end;
	guint64 longest = 0;
	guint64 line_length = task_data->checked_line_length;
	guint64 line_count;

	if (loader->line_length_threshold > 0 &&
	    !(task_data->seen_reasons & (1u << GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH)))
	{
		gtk_text_buffer_get_iter_at_offset (buffer, &iter, task_data->checked_offset);
		gtk_text_buffer_get_end_iter (buffer, &end);

		while (gtk_text_iter_compare (&iter, &end) < 0)
		{
			GtkTextIter line_end = iter;
			guint64 length;

			if (!gtk_text_iter_ends_line (&line_end))
				gtk_text_iter_forward_to_line_end (&line_end);
			length = gtk_text_iter_get_offset (&line_end) - gtk_text_iter_get_offset (&iter);
			line_length += length;
			longest = MAX (longest, line_length);

			if (gtk_text_iter_equal (&line_end, &end))
				break;

			gtk_text_iter_forward_line (&iter);
			line_length = 0;
		}

		task_data->checked_line_length = line_length;

		if (longest > loader->line_length_threshold)
		{
			request_policy (task, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_LENGTH,
			                longest, loader->line_length_threshold);
			return FALSE;
		}
	}

	task_data->checked_offset = gtk_text_buffer_get_char_count (buffer);

	line_count = gtk_text_buffer_get_line_count (buffer);
	gtk_text_buffer_get_end_iter (buffer, &end);
	if (gtk_source_buffer_get_implicit_trailing_newline (loader->source_buffer) &&
	    line_count > 1 &&
	    gtk_text_iter_starts_line (&end))
		line_count--;

	if (loader->line_count_threshold > 0 &&
	    !(task_data->seen_reasons & (1u << GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT)) &&
	    line_count > loader->line_count_threshold)
	{
		request_policy (task, GTK_SOURCE_FILE_LOADER_POLICY_REASON_LINE_COUNT,
		                line_count, loader->line_count_threshold);
		return FALSE;
	}

	return TRUE;
}

static void
start_destination (GTask *task)
{
	GtkSourceFileLoader *loader = g_task_get_source_object (task);
	TaskData *task_data = g_task_get_task_data (task);
	gboolean implicit_trailing_newline;

	if (task_data->destination_started)
		return;

	if (loader->input_stream_property != NULL)
		gtk_source_file_set_location (loader->file, NULL);
	else
		gtk_source_file_set_location (loader->file, loader->location);

	implicit_trailing_newline = gtk_source_buffer_get_implicit_trailing_newline (loader->source_buffer);
	task_data->output_stream = gtk_source_buffer_output_stream_new (loader->source_buffer,
	                                                                loader->candidate_encodings,
	                                                                implicit_trailing_newline);
	task_data->destination_started = TRUE;
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

		case PROP_MAX_SIZE:
			gtk_source_file_loader_set_max_size (loader, g_value_get_uint64 (value));
			break;
		case PROP_SIZE_THRESHOLD:
			gtk_source_file_loader_set_size_threshold (loader, g_value_get_uint64 (value));
			break;
		case PROP_LINE_LENGTH_THRESHOLD:
			gtk_source_file_loader_set_line_length_threshold (loader, g_value_get_uint64 (value));
			break;
		case PROP_LINE_COUNT_THRESHOLD:
			gtk_source_file_loader_set_line_count_threshold (loader, g_value_get_uint64 (value));
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

		case PROP_MAX_SIZE:
			g_value_set_uint64 (value, loader->max_size);
			break;
		case PROP_SIZE_THRESHOLD:
			g_value_set_uint64 (value, loader->size_threshold);
			break;
		case PROP_LINE_LENGTH_THRESHOLD:
			g_value_set_uint64 (value, loader->line_length_threshold);
			break;
		case PROP_LINE_COUNT_THRESHOLD:
			g_value_set_uint64 (value, loader->line_count_threshold);
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
	properties[PROP_BUFFER] =
		g_param_spec_object ("buffer", NULL, NULL,
		                     GTK_SOURCE_TYPE_BUFFER,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:file:
	 *
	 * The #GtkSourceFile. The #GtkSourceFileLoader object has a weak
	 * reference to the file.
	 */
	properties[PROP_FILE] =
		g_param_spec_object ("file", NULL, NULL,
		                     GTK_SOURCE_TYPE_FILE,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:location:
	 *
	 * The #GFile to load. If the #GtkSourceFileLoader:input-stream is
	 * %NULL, by default the location is taken from the #GtkSourceFile at
	 * construction time.
	 */
	properties[PROP_LOCATION] =
		g_param_spec_object ("location", NULL, NULL,
		                     G_TYPE_FILE,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:input-stream:
	 *
	 * The #GInputStream to load. Useful for reading stdin. If this property
	 * is set, the #GtkSourceFileLoader:location property is ignored.
	 */
	properties[PROP_INPUT_STREAM] =
		g_param_spec_object ("input-stream", NULL, NULL,
		                     G_TYPE_INPUT_STREAM,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_CONSTRUCT_ONLY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:max-size:
	 *
	 * The maximum expanded size, in bytes, that the loader will insert into
	 * the buffer. A value of 0 disables the limit.
	 *
	 * Since: 5.22
	 */
	properties[PROP_MAX_SIZE] =
		g_param_spec_uint64 ("max-size", NULL, NULL,
		                     0, G_MAXINT64, DEFAULT_MAX_SIZE,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:size-threshold:
	 *
	 * The expanded size in bytes at which a policy decision is requested.
	 * Zero disables the check.
	 *
	 * Since: 5.24
	 */
	properties[PROP_SIZE_THRESHOLD] =
		g_param_spec_uint64 ("size-threshold", NULL, NULL,
		                     0, G_MAXUINT64, DEFAULT_SIZE_THRESHOLD,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:line-length-threshold:
	 *
	 * The decoded character count on one line at which a policy decision is
	 * requested. Zero disables the check.
	 *
	 * Since: 5.24
	 */
	properties[PROP_LINE_LENGTH_THRESHOLD] =
		g_param_spec_uint64 ("line-length-threshold", NULL, NULL,
		                     0, G_MAXUINT64, DEFAULT_LINE_LENGTH_THRESHOLD,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	/**
	 * GtkSourceFileLoader:line-count-threshold:
	 *
	 * The buffer line count at which a policy decision is requested.
	 * Zero disables the check.
	 *
	 * Since: 5.24
	 */
	properties[PROP_LINE_COUNT_THRESHOLD] =
		g_param_spec_uint64 ("line-count-threshold", NULL, NULL,
		                     0, G_MAXUINT64, DEFAULT_LINE_COUNT_THRESHOLD,
		                     (G_PARAM_READWRITE |
		                      G_PARAM_EXPLICIT_NOTIFY |
		                      G_PARAM_STATIC_STRINGS));

	g_object_class_install_properties (object_class, N_PROPS, properties);

	/**
	 * GtkSourceFileLoader::decide-policy:
	 * @loader: the file loader.
	 * @decision: (transfer none): the decision to answer.
	 *
	 * Return %TRUE to claim the decision. The handler may retain @decision and
	 * answer later with [method@FileLoaderPolicyDecision.use] or
	 * [method@FileLoaderPolicyDecision.ignore]. Return %FALSE to continue loading.
	 * With no handler, loading continues. Only the first answer takes effect.
	 *
	 * Returns: %TRUE if the decision was claimed.
	 *
	 * Since: 5.24
	 */
	signals[DECIDE_POLICY] = g_signal_new ("decide-policy",
	                                       G_TYPE_FROM_CLASS (klass),
	                                       G_SIGNAL_RUN_LAST,
	                                       0,
	                                       g_signal_accumulator_true_handled,
	                                       NULL,
	                                       NULL,
	                                       G_TYPE_BOOLEAN,
	                                       1,
	                                       GTK_SOURCE_TYPE_FILE_LOADER_POLICY_DECISION);
}

static void
gtk_source_file_loader_init (GtkSourceFileLoader *loader)
{
	loader = gtk_source_file_loader_get_instance_private (loader);
	loader->max_size = DEFAULT_MAX_SIZE;
	loader->size_threshold = DEFAULT_SIZE_THRESHOLD;
	loader->line_length_threshold = DEFAULT_LINE_LENGTH_THRESHOLD;
	loader->line_count_threshold = DEFAULT_LINE_COUNT_THRESHOLD;
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
		load_task_return_error (task, error);
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
		load_task_return_error (task, error);
		goto cleanup;
	}

	/* Check if we needed some fallback char, if so, check if there was a
	 * previous error and if not set a fallback used error.
	 */
	if (gtk_source_buffer_output_stream_get_num_fallbacks (task_data->output_stream) > 0)
	{
		load_task_return_error (task,
		                        g_error_new_literal (GTK_SOURCE_FILE_LOADER_ERROR,
		                                             GTK_SOURCE_FILE_LOADER_ERROR_CONVERSION_FALLBACK,
		                                             _("There was an encoding conversion error so a fallback character was used.")));
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
	task_data->finished = TRUE;
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
			load_task_return_error (task, error);
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
					MAX (task_data->total_bytes_read, task_data->total_size),
					task_data->progress_cb_data);
	}

	task_data->next_step = NEXT_READ;
	schedule_next_step (task);
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
	goffset next_total_bytes_read = 0;
	GError *error = NULL;

	GTK_SOURCE_PROFILER_BEGIN_MARK;

	loader = g_task_get_source_object (task);
	task_data = g_task_get_task_data (task);

	task_data->chunk_bytes_read = g_input_stream_read_finish (input_stream, result, &error);

	if (error != NULL)
	{
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (!check_expanded_size (loader, task_data, &next_total_bytes_read, &error))
	{
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (task_data->chunk_bytes_read > 0 &&
	    loader->size_threshold > 0 &&
	    !(task_data->seen_reasons & (1u << GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE)) &&
	    (guint64)next_total_bytes_read > loader->size_threshold)
	{
		task_data->total_bytes_read = next_total_bytes_read;
		task_data->next_step = NEXT_WRITE;
		request_policy (task, GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE,
		                next_total_bytes_read, loader->size_threshold);
		goto cleanup;
	}

	if (task_data->guess_content_type_from_content &&
	    task_data->chunk_bytes_read > 0 &&
	    task_data->total_bytes_read == 0)
	{
		gchar *guessed;
		gsize chunk_bytes_read;

		if (!_gtk_source_utils_checked_gssize_to_gsize (task_data->chunk_bytes_read,
		                                                &chunk_bytes_read))
		{
			g_set_error_literal (&error,
			                     GTK_SOURCE_FILE_LOADER_ERROR,
			                     GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG,
			                     _("File too big."));
			load_task_return_error (task, error);
			goto cleanup;
		}

		guessed = g_content_type_guess (NULL,
		                                (guchar *)task_data->chunk_buffer,
		                                chunk_bytes_read,
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
		if (!g_output_stream_flush (G_OUTPUT_STREAM (task_data->output_stream),
		                            g_task_get_cancellable (task),
		                            &error))
		{
			load_task_return_error (task, error);
			goto cleanup;
		}

		loader->auto_detected_encoding =
			gtk_source_buffer_output_stream_get_guessed (task_data->output_stream);

		loader->auto_detected_newline_type =
			gtk_source_buffer_output_stream_detect_newline_type (task_data->output_stream);

		task_data->next_step = NEXT_CLOSE;
		schedule_next_step (task);
		goto cleanup;
	}

	task_data->total_bytes_read = next_total_bytes_read;

	task_data->next_step = NEXT_WRITE;
	schedule_next_step (task);

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
	task_data->next_step = NEXT_READ;
	schedule_next_step (task);
}

static gboolean
check_file_is_regular (GFileInfo  *info,
                       GError    **error)
{
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_TYPE) &&
	    g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
	{
		g_set_error_literal (error,
		                     G_IO_ERROR,
		                     G_IO_ERROR_NOT_REGULAR_FILE,
		                     _("Not a regular file."));
		return FALSE;
	}

	return TRUE;
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
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (!check_file_is_regular (task_data->info, &error))
	{
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (!check_file_size (g_task_get_source_object (task), task_data, &error))
	{
		load_task_return_error (task, error);
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
pre_open_query_info_cb (GObject      *source_object,
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
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED) &&
		    !task_data->tried_mount)
		{
			recover_not_mounted (task);
			g_error_free (error);
			goto cleanup;
		}

		load_task_return_error (task, error);
		goto cleanup;
	}

	if (!check_file_is_regular (task_data->info, &error))
	{
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (!check_file_size (g_task_get_source_object (task), task_data, &error))
	{
		load_task_return_error (task, error);
		goto cleanup;
	}

	if (g_file_info_has_attribute (task_data->info, G_FILE_ATTRIBUTE_STANDARD_SIZE) &&
	    get_compression_type_from_content_type (g_file_info_get_content_type (task_data->info)) != GTK_SOURCE_COMPRESSION_TYPE_GZIP)
	{
		GtkSourceFileLoader *loader = g_task_get_source_object (task);
		guint64 size = g_file_info_get_attribute_uint64 (task_data->info,
		                                                 G_FILE_ATTRIBUTE_STANDARD_SIZE);

		if (loader->size_threshold > 0 && size > loader->size_threshold)
		{
			task_data->next_step = NEXT_OPEN;
			request_policy (task, GTK_SOURCE_FILE_LOADER_POLICY_REASON_SIZE,
			                size, loader->size_threshold);
			goto cleanup;
		}
	}

	task_data->next_step = NEXT_OPEN;
	schedule_next_step (task);

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
		load_task_return_error (task, error);
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

		load_task_return_error (task, error);
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

	/* Path-based GFile loads remain subject to TOCTOU races: a local path
	 * can be replaced after this metadata check and before it is opened.
	 * Querying before g_file_read_async() still avoids opening files that
	 * are already known to be non-regular.
	 */
	g_file_query_info_async (loader->location,
				 LOADER_QUERY_ATTRIBUTES,
				 G_FILE_QUERY_INFO_NONE,
				 g_task_get_priority (task),
				 g_task_get_cancellable (task),
				 pre_open_query_info_cb,
				 task);
}

static void
read_file (GTask *task)
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
 * The buffer is emptied after the preopen metadata checks and any size policy
 * decision have completed.
 *
 * For path-based [iface@Gio.File] instances, the regular-file safety check is
 * subject to time-of-check/time-of-use races if the path is replaced while the
 * load operation is running.
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
 * gtk_source_file_loader_set_max_size:
 * @loader: a #GtkSourceFileLoader.
 * @max_size: maximum expanded size in bytes, or 0 to disable the limit.
 *
 * Sets the maximum expanded size that @loader will insert into the buffer.
 *
 * The limit is checked after decompression, so compressed files are limited by
 * the size of their uncompressed contents rather than their on-disk size.
 *
 * Since: 5.22
 */
void
gtk_source_file_loader_set_max_size (GtkSourceFileLoader *loader,
                                     guint64              max_size)
{
	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (loader->task == NULL);
	g_return_if_fail (max_size <= G_MAXINT64);

	if (loader->max_size != max_size)
	{
		loader->max_size = max_size;
		g_object_notify_by_pspec (G_OBJECT (loader), properties[PROP_MAX_SIZE]);
	}
}

/**
 * gtk_source_file_loader_get_max_size:
 * @loader: a #GtkSourceFileLoader.
 *
 * Gets the maximum expanded size that @loader will insert into the buffer.
 *
 * Returns: the maximum expanded size in bytes, or 0 if the limit is disabled.
 *
 * Since: 5.22
 */
guint64
gtk_source_file_loader_get_max_size (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), 0);

	return loader->max_size;
}

/**
 * gtk_source_file_loader_set_size_threshold:
 * @loader: a file loader.
 * @threshold: expanded bytes, or 0 to disable the policy check.
 *
 * Sets the size at which an application decision is requested.
 *
 * Since: 5.24
 */
void
gtk_source_file_loader_set_size_threshold (GtkSourceFileLoader *loader,
                                           guint64              threshold)
{
	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (loader->task == NULL);

	if (loader->size_threshold != threshold)
	{
		loader->size_threshold = threshold;
		g_object_notify_by_pspec (G_OBJECT (loader), properties[PROP_SIZE_THRESHOLD]);
	}
}

/**
 * gtk_source_file_loader_get_size_threshold:
 * @loader: a file loader.
 *
 * Returns: the size threshold in expanded bytes.
 *
 * Since: 5.24
 */
guint64
gtk_source_file_loader_get_size_threshold (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), 0);

	return loader->size_threshold;
}

/**
 * gtk_source_file_loader_set_line_length_threshold:
 * @loader: a file loader.
 * @threshold: decoded characters on a line, or 0 to disable the policy check.
 *
 * Sets the line length at which an application decision is requested.
 *
 * Since: 5.24
 */
void
gtk_source_file_loader_set_line_length_threshold (GtkSourceFileLoader *loader,
                                                  guint64              threshold)
{
	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (loader->task == NULL);

	if (loader->line_length_threshold != threshold)
	{
		loader->line_length_threshold = threshold;
		g_object_notify_by_pspec (G_OBJECT (loader), properties[PROP_LINE_LENGTH_THRESHOLD]);
	}
}

/**
 * gtk_source_file_loader_get_line_length_threshold:
 * @loader: a file loader.
 *
 * Returns: the line length threshold in decoded characters.
 *
 * Since: 5.24
 */
guint64
gtk_source_file_loader_get_line_length_threshold (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), 0);

	return loader->line_length_threshold;
}

/**
 * gtk_source_file_loader_set_line_count_threshold:
 * @loader: a file loader.
 * @threshold: lines, or 0 to disable the policy check.
 *
 * Sets the line count at which an application decision is requested.
 *
 * Since: 5.24
 */
void
gtk_source_file_loader_set_line_count_threshold (GtkSourceFileLoader *loader,
                                                 guint64              threshold)
{
	g_return_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader));
	g_return_if_fail (loader->task == NULL);

	if (loader->line_count_threshold != threshold)
	{
		loader->line_count_threshold = threshold;
		g_object_notify_by_pspec (G_OBJECT (loader), properties[PROP_LINE_COUNT_THRESHOLD]);
	}
}

/**
 * gtk_source_file_loader_get_line_count_threshold:
 * @loader: a file loader.
 *
 * Returns: the line count threshold.
 *
 * Since: 5.24
 */
guint64
gtk_source_file_loader_get_line_count_threshold (GtkSourceFileLoader *loader)
{
	g_return_val_if_fail (GTK_SOURCE_IS_FILE_LOADER (loader), 0);

	return loader->line_count_threshold;
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
 * @progress_callback: (scope notified) (closure progress_callback_data) (destroy progress_callback_notify) (nullable):
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
		load_task_return_error (loader->task,
		                        g_error_new_literal (G_IO_ERROR,
		                                             G_IO_ERROR_INVALID_ARGUMENT,
		                                             "Invalid argument"));
		return;
	}

	task_data->source_buffer = g_object_ref (loader->source_buffer);
	task_data->file = g_object_ref (loader->file);

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

	if (loader->input_stream_property != NULL)
	{
		start_destination (loader->task);
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
 * Declining a size policy decision returns %GTK_SOURCE_FILE_LOADER_ERROR_TOO_BIG.
 * Declining a line length or line count decision returns
 * %GTK_SOURCE_FILE_LOADER_ERROR_LINE_TOO_LONG or
 * %GTK_SOURCE_FILE_LOADER_ERROR_TOO_MANY_LINES, respectively. A decision made
 * after text insertion leaves the partial buffer modified and clears the file
 * location.
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
		GDateTime *creation_time;

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

		creation_time = g_file_info_get_creation_date_time (task_data->info);
		_gtk_source_file_set_creation_time (loader->file, creation_time);
		g_clear_pointer (&creation_time, g_date_time_unref);

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
				  loader->location != NULL ? g_file_peek_path (loader->location) : NULL);

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

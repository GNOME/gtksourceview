/*
 * Copyright 2026 Christian Hergert
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <glib/gstdio.h>
#include <gtksourceview/gtksource.h>

#include "gtksourceview/gtksourcelanguage-private.h"
#include "test-highlight-utils.h"

typedef struct
{
	GThread  *thread;
	GError   *error;
	gboolean  done;
	gboolean  success;
} Request;

typedef struct
{
	GThread  *thread;
	gboolean  finalized;
} Lifetime;

static GtkSourceLanguageManager *
new_manager (void)
{
	GtkSourceLanguageManager *manager = gtk_source_language_manager_new ();
	const char *paths[] = { TOP_SRCDIR "/data/language-specs", NULL };

	gtk_source_language_manager_set_search_path (manager, paths);

	return manager;
}

static void
prepared_cb (GObject      *object,
	     GAsyncResult *result,
	     gpointer      user_data)
{
	Request *request = user_data;

	g_assert_true (g_thread_self () == request->thread);
	request->success = gtk_source_language_prepare_finish (GTK_SOURCE_LANGUAGE (object),
							       result, &request->error);
	request->done = TRUE;
}

static void
wait_request (Request *request)
{
	while (!request->done)
		g_main_context_iteration (NULL, TRUE);
}

static void
finalized_cb (gpointer  data,
	      GObject  *object)
{
	Lifetime *lifetime = data;

	g_assert_true (g_thread_self () == lifetime->thread);
	lifetime->finalized = TRUE;
}

static void
test_pending_edits (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, "html");
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	Request request = { .thread = g_thread_self () };
	GtkTextIter begin, end;

	gtk_source_buffer_set_language (buffer, language);
	g_assert_true (gtk_source_buffer_get_language (buffer) == language);
	g_assert_null (_gtk_source_language_create_engine (language));
	g_assert_null (gtk_source_language_get_style_ids (language));

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "<p>old</p>", -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &begin, &end);
	gtk_source_buffer_ensure_highlight (buffer, &begin, &end);
	g_assert_null (_gtk_source_language_create_engine (language));
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "<!-- edited while pending -->\n<p>ready</p>", -1);

	gtk_source_language_prepare_async (language, NULL, prepared_cb, &request);
	g_object_unref (manager);
	wait_request (&request);
	g_assert_true (request.success);
	g_assert_no_error (request.error);

	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 7, "comment", TRUE);
	test_highlight_assert_class (buffer, 35, "comment", FALSE);
	g_assert_nonnull (gtk_source_language_get_style_fallback (language, "html:comment"));
	g_object_unref (buffer);
}

static void
test_cancel_shared (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, "html");
	GCancellable *cancellable = g_cancellable_new ();
	Request cancelled = { .thread = g_thread_self () };
	Request success = { .thread = g_thread_self () };
	Request cached = { .thread = g_thread_self () };

	gtk_source_language_prepare_async (language, cancellable, prepared_cb, &cancelled);
	gtk_source_language_prepare_async (language, NULL, prepared_cb, &success);
	g_cancellable_cancel (cancellable);
	wait_request (&cancelled);
	wait_request (&success);
	g_assert_false (cancelled.success);
	g_assert_error (cancelled.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_assert_true (success.success);
	g_assert_no_error (success.error);

	gtk_source_language_prepare_async (language, NULL, prepared_cb, &cached);
	wait_request (&cached);
	g_assert_true (cached.success);
	g_assert_no_error (cached.error);
	g_clear_error (&cancelled.error);
	g_object_unref (cancellable);
	g_object_unref (manager);
}

static void
test_switch_and_destroy (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *html = gtk_source_language_manager_get_language (manager, "html");
	GtkSourceLanguage *c = gtk_source_language_manager_get_language (manager, "c");
	GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
	GtkSourceBuffer *discarded = gtk_source_buffer_new (NULL);
	GtkSourceBuffer *cleared = gtk_source_buffer_new (NULL);
	Request html_request = { .thread = g_thread_self () };
	Request c_request = { .thread = g_thread_self () };
	Lifetime buffer_lifetime = { .thread = g_thread_self () };
	Lifetime html_lifetime = { .thread = g_thread_self () };

	g_object_weak_ref (G_OBJECT (discarded), finalized_cb, &buffer_lifetime);
	g_object_weak_ref (G_OBJECT (html), finalized_cb, &html_lifetime);
	gtk_source_buffer_set_language (discarded, html);
	g_object_unref (discarded);
	g_assert_true (buffer_lifetime.finalized);

	gtk_source_buffer_set_language (cleared, html);
	gtk_source_buffer_set_language (cleared, NULL);
	gtk_source_buffer_set_language (buffer, html);
	gtk_source_buffer_set_language (buffer, c);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "/* current language */\nint value;", -1);
	gtk_source_language_prepare_async (html, NULL, prepared_cb, &html_request);
	gtk_source_language_prepare_async (c, NULL, prepared_cb, &c_request);
	g_object_unref (manager);

	wait_request (&html_request);
	wait_request (&c_request);
	g_assert_true (html_request.success);
	g_assert_true (c_request.success);
	g_assert_no_error (html_request.error);
	g_assert_no_error (c_request.error);
	g_assert_true (gtk_source_buffer_get_language (buffer) == c);
	g_assert_null (gtk_source_buffer_get_language (cleared));
	g_object_unref (cleared);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 4, "comment", TRUE);

	while (!html_lifetime.finalized)
		g_main_context_iteration (NULL, TRUE);

	gtk_source_buffer_set_language (buffer, NULL);
	g_assert_null (gtk_source_buffer_get_language (buffer));
	test_highlight_assert_class (buffer, 4, "comment", FALSE);
	g_object_unref (buffer);
}

static void
test_failure_retry (void)
{
	const char *invalid = "<language id='broken' name='Broken' version='2.0'>"
	"<definitions><context id='broken'><include><context id='word'>"
	"<match>[</match></context></include></context></definitions></language>";
	const char *valid = "<language id='broken' name='Broken' version='2.0'>"
		      "<definitions><context id='broken'><include><context id='word'>"
		      "<match>word</match></context></include></context></definitions></language>";
	GtkSourceLanguageManager *manager = gtk_source_language_manager_new ();
	GtkSourceLanguage *language;
	GError *error = NULL;
	char *directory = g_dir_make_tmp ("gtksource-prepare-XXXXXX", &error);
	char *filename;
	const char *paths[] = { directory, NULL };
	Request failed = { .thread = g_thread_self () };
	Request retry = { .thread = g_thread_self () };

	g_assert_no_error (error);
	filename = g_build_filename (directory, "broken.lang", NULL);
	g_assert_true (g_file_set_contents (filename, invalid, -1, &error));
	g_assert_no_error (error);
	gtk_source_language_manager_set_search_path (manager, paths);
	language = gtk_source_language_manager_get_language (manager, "broken");
	g_assert_nonnull (language);

	gtk_source_language_prepare_async (language, NULL, prepared_cb, &failed);
	wait_request (&failed);
	g_assert_false (failed.success);
	g_assert_nonnull (failed.error);
	g_assert_null (_gtk_source_language_create_engine (language));
	g_clear_error (&failed.error);

	g_assert_true (g_file_set_contents (filename, valid, -1, &error));
	g_assert_no_error (error);
	gtk_source_language_prepare_async (language, NULL, prepared_cb, &retry);
	wait_request (&retry);
	g_assert_no_error (retry.error);
	g_assert_true (retry.success);
	g_object_unref (manager);
	g_assert_cmpint (g_unlink (filename), ==, 0);
	g_assert_cmpint (g_rmdir (directory), ==, 0);
	g_free (filename);
	g_free (directory);
}

static void
test_parallel (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	const char *ids[] = { "html", "markdown", "json", "c", "python3", "js" };
	Request requests[G_N_ELEMENTS (ids)] = { 0 };

	for (guint i = 0; i < G_N_ELEMENTS (ids); i++)
	{
		GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, ids[i]);

		requests[i].thread = g_thread_self ();
		g_assert_nonnull (language);
		gtk_source_language_prepare_async (language, NULL, prepared_cb, &requests[i]);
	}

	for (guint i = 0; i < G_N_ELEMENTS (ids); i++)
	{
		wait_request (&requests[i]);
		g_assert_true (requests[i].success);
		g_assert_no_error (requests[i].error);
	}

	g_object_unref (manager);
}

static void
test_thread_default_context (void)
{
	GMainContext *context = g_main_context_new ();
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, "html");
	Request request = { .thread = g_thread_self () };
	GtkSourceBuffer *buffer;

	g_main_context_push_thread_default (context);
	gtk_source_language_prepare_async (language, NULL, prepared_cb, &request);
	buffer = gtk_source_buffer_new_with_language (language);
	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), "<!-- ready -->", -1);

	while (!request.done)
		g_main_context_iteration (context, TRUE);

	g_assert_true (request.success);
	g_assert_no_error (request.error);
	test_highlight_buffer_ensure (buffer);
	test_highlight_assert_class (buffer, 6, "comment", TRUE);
	g_object_unref (buffer);

	while (g_main_context_pending (context))
		g_main_context_iteration (context, FALSE);

	g_main_context_pop_thread_default (context);
	g_main_context_unref (context);
	g_object_unref (manager);
}

static void
test_no_callback (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, "html");
	Lifetime lifetime = { .thread = g_thread_self () };

	g_object_weak_ref (G_OBJECT (language), finalized_cb, &lifetime);
	gtk_source_language_prepare_async (language, NULL, NULL, NULL);
	g_object_unref (manager);

	while (!lifetime.finalized)
		g_main_context_iteration (NULL, TRUE);
}

static void
test_already_cancelled (void)
{
	GtkSourceLanguageManager *manager = new_manager ();
	GtkSourceLanguage *language = gtk_source_language_manager_get_language (manager, "html");
	GCancellable *cancellable = g_cancellable_new ();
	Request request = { .thread = g_thread_self () };

	g_cancellable_cancel (cancellable);
	gtk_source_language_prepare_async (language, cancellable, prepared_cb, &request);
	wait_request (&request);
	g_assert_error (request.error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
	g_assert_false (request.success);
	g_assert_null (_gtk_source_language_create_engine (language));
	g_clear_error (&request.error);
	g_object_unref (cancellable);
	g_object_unref (manager);
}

int
main (int    argc,
      char **argv)
{
	int ret;

	gtk_test_init (&argc, &argv);
	gtk_source_init ();

	g_test_add_func ("/language/prepare/pending-edits", test_pending_edits);
	g_test_add_func ("/language/prepare/cancel-shared", test_cancel_shared);
	g_test_add_func ("/language/prepare/switch-and-destroy", test_switch_and_destroy);
	g_test_add_func ("/language/prepare/failure-retry", test_failure_retry);
	g_test_add_func ("/language/prepare/parallel", test_parallel);

	g_test_add_func ("/language/prepare/thread-default-context", test_thread_default_context);
	g_test_add_func ("/language/prepare/no-callback", test_no_callback);
	g_test_add_func ("/language/prepare/already-cancelled", test_already_cancelled);

	ret = g_test_run ();
	gtk_source_finalize ();

	return ret;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

static void
test_get_buffer (void)
{
	GtkWidget* view;
	GtkSourceBuffer* buffer;

	view = gtk_source_view_new ();

	buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));

	g_assert (buffer != NULL);
	g_assert (GTK_SOURCE_IS_BUFFER (buffer));

	if (g_object_is_floating (view))
	{
		g_object_ref_sink (view);
	}

	/* Here we check if notify_buffer recreates the buffer while view is being
	 * destroyed, which causes assertion failure in GtkTextView's finalize ()
	 * function.
	 * Please see: https://bugzilla.gnome.org/show_bug.cgi?id=634510 */
	g_object_unref (view);
}

static void
do_test_change_case (GtkSourceBuffer         *buffer,
                     GtkSourceChangeCaseType  case_type,
                     const char              *text,
                     const char              *expected)
{
	GtkTextIter start, end;
	char *changed;

	gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer), text, -1);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	gtk_source_buffer_change_case (buffer, case_type, &start, &end);
	gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
	changed = gtk_text_buffer_get_text (GTK_TEXT_BUFFER (buffer), &start, &end, TRUE);
	g_assert_cmpstr (changed, ==, expected);
	g_free (changed);
}

static void
test_change_case (void)
{
	GtkSourceBuffer* buffer;

	buffer = gtk_source_buffer_new (NULL);

	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_LOWER, "some TEXT", "some text");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_UPPER, "some TEXT", "SOME TEXT");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TOGGLE, "some TEXT", "SOME text");
	do_test_change_case (buffer, GTK_SOURCE_CHANGE_CASE_TITLE, "some TEXT", "Some Text");
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Buffer/bug-634510", test_get_buffer);
	g_test_add_func ("/Buffer/change-case", test_change_case);

	return g_test_run();
}

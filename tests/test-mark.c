#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

static void
test_create (void)
{
	GtkSourceMark *m;

	m = gtk_source_mark_new ("Mark 1", "test");
	g_assert_cmpstr ("Mark 1", ==, gtk_text_mark_get_name (GTK_TEXT_MARK (m)));
	g_assert_cmpstr ("test", ==, gtk_source_mark_get_category (m));
	g_assert (NULL == gtk_text_mark_get_buffer (GTK_TEXT_MARK (m)));
	g_assert (NULL == gtk_source_mark_next (m, NULL));
	g_assert (NULL == gtk_source_mark_prev (m, NULL));
	g_object_unref (m);
}

static void
test_add_to_buffer (void)
{
	GtkSourceBuffer *b;
	GtkSourceMark *m1, *m2, *m3;
	GtkTextIter i;

	b = gtk_source_buffer_new (NULL);
	m1 = gtk_source_mark_new ("Mark 1", "test");
	m2 = gtk_source_mark_new ("Mark 2", "test2");
	m3 = gtk_source_mark_new ("Mark 3", "test");

	gtk_text_buffer_get_start_iter (GTK_TEXT_BUFFER (b), &i);
	gtk_text_buffer_add_mark (GTK_TEXT_BUFFER (b), GTK_TEXT_MARK (m1), &i);
	gtk_text_buffer_add_mark (GTK_TEXT_BUFFER (b), GTK_TEXT_MARK (m2), &i);
	gtk_text_buffer_add_mark (GTK_TEXT_BUFFER (b), GTK_TEXT_MARK (m3), &i);

	g_assert (m2 == gtk_source_mark_next (m1, NULL));
	g_assert (m3 == gtk_source_mark_next (m1, "test"));
	g_assert (NULL == gtk_source_mark_next (m2, "test2"));
	g_assert (NULL == gtk_source_mark_next (m3, NULL));

	g_assert (m1 == gtk_source_mark_prev (m2, NULL));
	g_assert (m1 == gtk_source_mark_prev (m3, "test"));
	g_assert (NULL == gtk_source_mark_prev (m2, "test2"));
	g_assert (NULL == gtk_source_mark_prev (m1, NULL));

	g_object_unref (b);
}

int
main (int argc, char** argv)
{
	gtk_test_init (&argc, &argv);

	g_test_add_func ("/Mark/create", test_create);
	g_test_add_func ("/Mark/add-to-buffer", test_add_to_buffer);

	return g_test_run();
}

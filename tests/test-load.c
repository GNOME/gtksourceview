#include <gtksourceview/gtksource.h>
#include <stdlib.h>

static void
finished_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  GMainLoop *loop = user_data;

  if (!gtk_source_file_loader_load_finish (GTK_SOURCE_FILE_LOADER (object), result, &error))
    g_printerr ("Error loading file: %s\n", error->message);

  g_main_loop_quit (loop);
}

int
main (int argc,
      char *argv[])
{
  g_autoptr(GFile) file = NULL;
  g_autoptr(GMainLoop) loop = NULL;
  g_autoptr(GtkSourceBuffer) buffer = NULL;
  g_autoptr(GtkSourceFile) sfile = NULL;
  g_autoptr(GtkSourceFileLoader) loader = NULL;

  if (argc != 2)
    {
      g_printerr ("usage: %s FILENAME\n", argv[0]);
      return EXIT_FAILURE;
    }

  gtk_source_init ();

  loop = g_main_loop_new (NULL, FALSE);
  file = g_file_new_for_commandline_arg (argv[1]);
  buffer = gtk_source_buffer_new (NULL);
  sfile = gtk_source_file_new ();
  gtk_source_file_set_location (sfile, file);
  loader = gtk_source_file_loader_new (buffer, sfile);

  gtk_source_file_loader_load_async (loader, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, finished_cb, loop);

  g_main_loop_run (loop);

  return EXIT_SUCCESS;
}

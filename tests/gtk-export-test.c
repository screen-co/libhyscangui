#include <hyscan-gtk-export.h>

GtkWidget *gtk_export;


int
main (int argc, char **argv)
{
  GtkWidget * window;
  GtkWidget * box;
  GtkWidget * button_exit;

  gchar *db_uri = NULL;
  gchar *project_name = NULL;
  HyScanDB *db = NULL;
  gchar **tracks = NULL;
  gint pid;
  
  gtk_init (&argc, &argv);

  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "project-name", 'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name", NULL },
        { "db-uri", 'd', 0, G_OPTION_ARG_STRING, &db_uri, "Database uri", NULL},
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message ("%s", error->message);
        return -1;
      }

    g_option_context_free (context);
   }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("Can't create DB");
  g_free (db_uri);

  pid = hyscan_db_project_open (db, project_name);
  tracks = hyscan_db_track_list (db, pid);
  g_debug ("%s", tracks[0]);
  if (tracks == NULL)
    g_error ("DB Invalid");

  if ((gtk_export = hyscan_gtk_export_new (db, project_name, tracks[0])) == NULL)
    g_error ("Can't create export widget");

  hyscan_db_close (db, pid);

  hyscan_gtk_export_set_watch_period (HYSCAN_GTK_EXPORT (gtk_export), 100);
 
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  button_exit = gtk_button_new_with_label ("Exit"); 
  g_signal_connect (button_exit, "clicked",G_CALLBACK (gtk_main_quit), NULL);

  gtk_box_pack_start (GTK_BOX (box), gtk_export, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), button_exit, FALSE, FALSE, 3);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);
  gtk_main ();

  return 0;
}
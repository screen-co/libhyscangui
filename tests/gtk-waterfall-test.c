#include <hyscan-gtk-waterfall-last.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *wf;

  gtk_init (&argc, &argv);

  gchar *project_name,
        *track_name,
        *db_uri,
        *cache_prefix = "wf-test",
        *title;
  HyScanDB *db;
  gdouble speed = 1.0;
  gdouble black = 0.0;
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));
  HyScanCache *cache2 = HYSCAN_CACHE (hyscan_cached_new (512));

  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name;",          NULL},
      { "track",       't', 0, G_OPTION_ARG_STRING, &track_name,   "Track name;",            NULL},
      { "speed",       's', 0, G_OPTION_ARG_DOUBLE, &speed,        "Speed of ship;",         NULL},
      { "black",       'b', 0, G_OPTION_ARG_DOUBLE, &black,        "black level (0.0-1.0);", NULL},
      { "white",       'w', 0, G_OPTION_ARG_DOUBLE, &white,        "white level (0.0-1.0);", NULL},
      { "gamma",       'g', 0, G_OPTION_ARG_DOUBLE, &gamma,        "gamma level (0.0-1.0);", NULL},
      {NULL}
    };

    #ifdef G_OS_WIN32
        args = g_win32_get_command_line ();
    #else
        args = g_strdupv (argv);
    #endif

    context = g_option_context_new ("<db-uri>");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message (error->message);
        return -1;
      }

    if (g_strv_length (args) != 2)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    db_uri = g_strdup (args[1]);
    g_strfreev (args);
  }

  db = hyscan_db_new (db_uri);
  if (db == NULL)
    g_error ("can't open db at: %s", db_uri);
  /* Основное окно программы. */
  title = g_strdup_printf ("%s/%s/%s", db_uri, project_name, track_name);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  /* Водопад. */
  wf = hyscan_gtk_waterfall_new ();
   //hyscan_gtk_waterfall_echosounder (HYSCAN_GTK_WATERFALL(wf), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  hyscan_gtk_waterfall_set_cache  (HYSCAN_GTK_WATERFALL(wf), cache, cache2, cache_prefix);

  hyscan_gtk_waterfall_drawer_set_upsample (HYSCAN_GTK_WATERFALL_DRAWER(wf), 4);

  hyscan_gtk_waterfall_set_depth_source (HYSCAN_GTK_WATERFALL(wf), HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);
  hyscan_gtk_waterfall_set_depth_filter_size (HYSCAN_GTK_WATERFALL(wf), 2);
  hyscan_gtk_waterfall_set_depth_time (HYSCAN_GTK_WATERFALL(wf), 100000);

  hyscan_gtk_waterfall_drawer_set_levels (HYSCAN_GTK_WATERFALL_DRAWER(wf), black, gamma, white);

  /* Кладем виджет в основное окно. */
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (wf));
  gtk_widget_show_all (window);
  hyscan_gtk_waterfall_open (HYSCAN_GTK_WATERFALL(wf), db, project_name, track_name, TRUE);

  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_main ();

  g_clear_object (&cache);
  g_clear_object (&db);
  g_clear_object (&cache2);
  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);
  g_free (title);

  xmlCleanupParser ();

  return 0;
}

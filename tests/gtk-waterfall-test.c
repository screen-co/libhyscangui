#include <hyscan-gtk-waterfall.h>
#include <hyscan-gtk-waterfallgrid.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>


void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *wfgrid;

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
  gint upsample = 2;

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
  wfgrid = hyscan_gtk_waterfallgrid_new ();
  hyscan_gtk_waterfall_set_cache  (HYSCAN_GTK_WATERFALL(wfgrid), cache, cache2, cache_prefix);
  hyscan_gtk_waterfall_set_levels (HYSCAN_GTK_WATERFALL(wfgrid), black, gamma, white);
  //hyscan_gtk_waterfall_automove   (HYSCAN_GTK_WATERFALL(wfgrid), TRUE);

  /* Кладем виджет в основное окно. */
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (wfgrid));
  gtk_widget_show_all (window);
  hyscan_gtk_waterfall_open (HYSCAN_GTK_WATERFALL(wfgrid), db, project_name, track_name, HYSCAN_TILE_SIDESCAN, TRUE);

  g_signal_connect( G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

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

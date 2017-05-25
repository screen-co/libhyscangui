#include <hyscan-gtk-waterfall-last.h>
#include <hyscan-tile-color.h>
#include <hyscan-cached.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <glib.h>

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *wf_widget;
  HyScanGtkWaterfall *wf;
  HyScanGtkWaterfallDrawer *wfd;

  gchar *project_name;
  gchar *track_name;
  gchar *db_uri;
  gchar *cache_prefix = "wf-test";
  gchar *title;
  gboolean use_computed = FALSE;
  HyScanDB *db;
  gdouble speed = 1.0;
  gdouble black = 0.0;
  gdouble white = 0.2;
  gdouble gamma = 1.0;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));
  HyScanCache *cache2 = HYSCAN_CACHE (hyscan_cached_new (512));

  guint32 background, colors[2], *colormap;
  guint cmap_len;

  gtk_init (&argc, &argv);

  /* Парсим аргументы*/
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] = {
      { "project",     'p', 0, G_OPTION_ARG_STRING, &project_name, "Project name;",          NULL},
      { "track",       't', 0, G_OPTION_ARG_STRING, &track_name,   "Track name;",            NULL},
      { "computed",    'c', 0, G_OPTION_ARG_NONE  , &use_computed, "Use computed data;",     NULL},
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
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

  title = g_strdup_printf ("%s/%s/%s", db_uri, project_name, track_name);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 700);

  /* Водопад. */
  wf_widget = hyscan_gtk_waterfall_new ();

  /* Для удобства приведем типы. */
  wfd = HYSCAN_GTK_WATERFALL_DRAWER (wf_widget);
  wf  = HYSCAN_GTK_WATERFALL (wf_widget);
  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);

  hyscan_gtk_waterfall_set_cache (wf, cache, cache2, cache_prefix);

  hyscan_gtk_waterfall_set_depth_source (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD, 1);
  hyscan_gtk_waterfall_set_depth_filter_size (wf, 2);
  hyscan_gtk_waterfall_set_depth_time (wf, 100000);

  hyscan_gtk_waterfall_drawer_set_upsample (wfd, 2);

  background = hyscan_tile_color_converter_d2i (0.15, 0.15, 0.15, 1.0);
  colors[0]  = hyscan_tile_color_converter_d2i (0.0, 0.0, 0.0, 1.0);
  colors[1]  = hyscan_tile_color_converter_d2i (0.0, 1.0, 1.0, 1.0);
  colormap   = hyscan_tile_color_compose_colormap (colors, 2, &cmap_len);

  /* Устанавливаем цветовую схему и подложку. */
  hyscan_gtk_waterfall_drawer_set_colormap_for_all (wfd, colormap, cmap_len, background);
  hyscan_gtk_waterfall_drawer_set_levels_for_all (wfd, black, gamma, white);
  hyscan_gtk_waterfall_drawer_set_substrate (wfd, background);

  /* Кладем виджет в основное окно. */
  gtk_container_add (GTK_CONTAINER (window), wf_widget);
  gtk_widget_show_all (window);

  //hyscan_gtk_waterfall_echosounder (wf, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  /* !use_computed == use_raw. */
  hyscan_gtk_waterfall_open (wf, db, project_name, track_name, !use_computed);

  /* Начинаем работу. */
  gtk_main ();

  g_clear_object (&cache);
  g_clear_object (&cache2);
  g_clear_object (&db);

  g_free (project_name);
  g_free (track_name);
  g_free (db_uri);
  g_free (title);
  g_free (colormap);

  xmlCleanupParser ();

  return 0;
}

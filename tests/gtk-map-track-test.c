#include <hyscan-cached.h>
#include <hyscan-gtk-map-track.h>
#include <hyscan-profile-map.h>
#include <hyscan-gtk-map-base.h>
#include <hyscan-gtk-map-control.h>

int
main (int    argc,
      char **argv)
{
  gchar *origin = NULL;
  gchar *db_uri = NULL;
  gchar *project_name = NULL;
  gchar **track_names = NULL;

  GtkWidget *window, *box, *map;
  HyScanCache *cache;
  HyScanProfileMap *profile;
  HyScanMapTrackModel *track_model;
  HyScanGtkLayer *track_layer;
  HyScanGeoPoint center = { .lat = 55.571, .lon = 38.103 };
  HyScanDB *db = NULL;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "db-uri",          'd', 0, G_OPTION_ARG_STRING, &db_uri,            "Database uri", NULL},
        { "tracks",          't', 0, G_OPTION_ARG_STRING_ARRAY, &track_names, "Track names", NULL},
        { "project-name",    'p', 0, G_OPTION_ARG_STRING, &project_name,      "Project name", NULL},
        { "origin",          '0', 0, G_OPTION_ARG_STRING, &origin,            "Map origin, \"lat,lon\"", NULL},
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_summary (context, "Test map track layer");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message ("%s", error->message);
        return -1;
      }

    if (db_uri == NULL || project_name == NULL || track_names == NULL)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    if (origin != NULL)
      {
        gdouble lat, lon;

        if (sscanf (origin, "%lf,%lf", &lat, &lon) == 2)
          {
            center.lat = lat;
            center.lon = lon;
          }
      }

    g_option_context_free (context);
  }

  db = hyscan_db_new (db_uri);
  cache = HYSCAN_CACHE (hyscan_cached_new (500));

  /* Карта и допустимые масштабы. */
  gint scales_len;
  gdouble *scales;
  map = hyscan_gtk_map_new (center);
  scales = hyscan_gtk_map_create_scales2 (1.0 / 1000, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
  hyscan_gtk_map_set_scales_meter (HYSCAN_GTK_MAP (map), scales, scales_len);
  g_free (scales);

  /* Слои. */
  track_model = hyscan_map_track_model_new (db, cache);
  track_layer = hyscan_gtk_map_track_new (track_model);
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), hyscan_gtk_map_base_new (cache), "base");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), track_layer, "track");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), hyscan_gtk_map_control_new (), "control");
  profile = hyscan_profile_map_new_default (NULL);
  hyscan_profile_map_apply (profile, HYSCAN_GTK_MAP (map), "base");
  {
    HyScanGeoProjection *projection;
    projection = hyscan_gtk_map_get_projection (HYSCAN_GTK_MAP (map));
    hyscan_map_track_model_set_projection (track_model, projection);
    g_object_unref (projection);
  }

  /* Настройка слоя с галсами. */
  hyscan_gtk_layer_set_visible (track_layer, TRUE);
  hyscan_map_track_model_set_project (track_model, project_name);
  hyscan_map_track_model_set_tracks (track_model, track_names);
  if (track_names != NULL)
    hyscan_gtk_map_track_view (HYSCAN_GTK_MAP_TRACK (track_layer), track_names[0], TRUE, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_start (GTK_BOX (box), map, TRUE, TRUE, 0);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), project_name);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 600);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);

  /* Main loop. */
  g_signal_connect_swapped (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_main ();

  /* Cleanup. */
  g_free (db_uri);
  g_free (project_name);
  g_free (track_names);
  g_free (origin);
  g_clear_object (&track_model);
  g_clear_object (&db);
  g_clear_object (&cache);

  return 0;
}

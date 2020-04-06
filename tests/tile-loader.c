#include <hyscan-map-tile-loader.h>
#include <hyscan-profile-map.h>

static gdouble loader_fraction;
static gint64 loader_time;

static void
loader_progress (HyScanMapTileLoader *loader,
                 gdouble              fraction)
{
  gint hour, min, sec;
  gdouble speed, dtime;

  dtime = (gdouble) (g_get_monotonic_time () - loader_time) / G_TIME_SPAN_SECOND;
  if (fraction == 0 || dtime == 0)
    return;

  speed = (fraction - loader_fraction) / dtime;

  sec = (1.0 - fraction) / speed;

  hour = sec / 3600;
  sec -= hour * 3600;

  min = sec / 60;
  sec -= min * 60;

  g_print ("\rSpeed: %f Progress: %.4f%% TTG: %02d:%02d:%02d", speed, fraction * 100.0, hour, min, sec);

  /* Сбрасываем счётчик средней скорости загрузки каждые 20 секунд. */
  if (dtime > 20)
    {
       loader_fraction = fraction;
       loader_time = g_get_monotonic_time ();
    }
}

static void
parse_zooms (const gchar *zooms,
             guint *min_zoom,
             guint *max_zoom)
{
  gchar **zoom_arr;

  *min_zoom = *max_zoom = 0;
  zoom_arr = g_strsplit (zooms, ":", 2);
  if (g_strv_length (zoom_arr) == 2)
    {
      *min_zoom = g_ascii_strtoull (zoom_arr[0], NULL, 10);
      *max_zoom = g_ascii_strtoull (zoom_arr[1], NULL, 10);
    }

  g_strfreev (zoom_arr);
}

static void
parse_latlon (const gchar       *string,
              HyScanGeoPoint *target)
{
  gchar **lat_lon;

  target->lat = 0.0;
  target->lon = 0.0;

  lat_lon = g_strsplit (string, ",", 2);
  if (g_strv_length (lat_lon) == 2)
    {
      target->lat = g_ascii_strtod (lat_lon[0], NULL);
      target->lon = g_ascii_strtod (lat_lon[1], NULL);
    }

  g_strfreev (lat_lon);
}

int
main (int argc,
      char **argv)
{
  HyScanMapTileLoader *loader;
  HyScanMapTileSource *source;
  GThread *thread;
  gchar *url_format, *proj_name;
  gchar *cache_dir, *subdir, *zooms;
  gchar *from_str, *to_str;
  guint min_zoom, max_zoom;
  HyScanGeoProjection *projection;
  HyScanProfileMap *profile;
  gchar **headers;
  HyScanGeoPoint from, to;
  HyScanGeoCartesian2D from_c2d, to_c2d;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "url",     'u', 0, G_OPTION_ARG_STRING,       &url_format, "URL format", NULL },
        { "dir",     'd', 0, G_OPTION_ARG_STRING,       &cache_dir,  "Dir to save files", NULL },
        { "proj",    'p', 0, G_OPTION_ARG_STRING,       &proj_name,  "Projection name", NULL },
        { "name",    'n', 0, G_OPTION_ARG_STRING,       &subdir,     "Name of source", NULL },
        { "zoom",    'z', 0, G_OPTION_ARG_STRING,       &zooms,      "Zooms MIN:MAX", NULL },
        { "from",    'a', 0, G_OPTION_ARG_STRING,       &from_str,   "Coordinates from, LAT,LON", NULL },
        { "to",      'b', 0, G_OPTION_ARG_STRING,       &to_str,     "Coordinates to, LAT,LON", NULL },
        { "headers", 'r', 0, G_OPTION_ARG_STRING_ARRAY, &headers,    "Request headers", NULL },
        { NULL }
      };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_summary (context, "Download tiles from the given URL");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if (url_format == NULL || cache_dir == NULL || subdir == NULL || proj_name == NULL || zooms == NULL ||
        from_str == NULL || to_str == NULL)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);
    g_strfreev (args);
  }

  /* Парсим уровни детализации, широту и долготу. */
  parse_zooms (zooms, &min_zoom, &max_zoom);
  parse_latlon (from_str, &from);
  parse_latlon (to_str, &to);

  g_print ("Loading tiles from \"%s\" into %s/%s\n",
           url_format, cache_dir, subdir);

  g_print ("\n");
  g_print ("Area boundary:\n"
           "from    (%f, %f)\n"
           "to      (%f, %f)\n"
           "zooms    %d - %d\n",
           from.lat, from.lon,
           to.lat, to.lon,
           min_zoom, max_zoom);

  profile = hyscan_profile_map_new_full (subdir, url_format, cache_dir, subdir, headers, proj_name, min_zoom, max_zoom);

  /* Создаем загрузчик. */
  loader = hyscan_map_tile_loader_new ();
  g_signal_connect (loader, "progress", G_CALLBACK (loader_progress), NULL);

  /* Запускаем загрузку. */
  source = hyscan_profile_map_get_source (profile);
  projection = hyscan_map_tile_source_get_projection (source);
  hyscan_geo_projection_geo_to_value (projection, from, &from_c2d);
  hyscan_geo_projection_geo_to_value (projection, to, &to_c2d);
  loader_time = g_get_monotonic_time ();
  thread = hyscan_map_tile_loader_start (loader, source, from_c2d.x, to_c2d.x, from_c2d.y, to_c2d.y);

  g_object_unref (source);
  g_object_unref (profile);
  g_object_unref (projection);
  g_object_unref (loader);

  g_thread_join (thread);

  /* Проверяем, что все тайлы загрузились. */
  g_print ("\n");
  g_print ("Area cached successfully!\n");

  return 0;
}

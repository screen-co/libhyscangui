#include <hyscan-gtk-map-track-item.h>
#include <hyscan-proj.h>
#include <hyscan-cached.h>

/* Тест проверяет производительность класса, готовящего данные для отрисовки галса на карте.
 * Также можно проверить утечки памяти. */
int
main (int    argc,
      char **argv)
{
  HyScanGtkMapTrackItem *item;
  HyScanGtkMapTiled *tiled;
  HyScanGeoProjection *projection;
  HyScanDB *db;
  HyScanCache *cache;
  GTimer *timer;

  HyScanGtkMapTrackDrawData data;

  const gchar *uri;
  const gchar *project;
  const gchar *track;

  if (argc != 4)
    {
      g_print ("Usage: gtk-map-track-item-test <db-uri> <project> <track>\n");
      return 1;
    }

  uri = argv[1];
  project = argv[2];
  track = argv[3];

  tiled = g_object_new (HYSCAN_TYPE_GTK_MAP_TILED, NULL);
  projection = hyscan_proj_new (HYSCAN_PROJ_WEBMERC);

  db = hyscan_db_new (uri);
  if (db == NULL)
    g_error ("Failed to open db uri: %s", uri);

  cache = HYSCAN_CACHE (hyscan_cached_new (500));
  item = hyscan_gtk_map_track_item_new (db, NULL, project, track, tiled, projection);

  timer = g_timer_new ();
  hyscan_gtk_map_track_item_points_lock (item, &data);
  g_print ("Time spent: %.2f seconds\n", g_timer_elapsed (timer, NULL));
  g_print ("Track extent is (%f, %f) to (%f, %f)\n", data.from.x, data.from.y, data.to.x, data.to.y);
  g_print ("N points:\n");
  g_print ("  starboard:  %d\n", g_list_length (data.starboard));
  g_print ("  port:       %d\n", g_list_length (data.port));
  g_print ("  navigation: %d\n", g_list_length (data.nav));
  hyscan_gtk_map_track_item_points_unlock (item);

  g_timer_destroy (timer);
  g_object_unref (tiled);
  g_object_unref (projection);
  g_object_unref (db);
  g_object_unref (cache);
  g_object_unref (item);

  return 0;
}

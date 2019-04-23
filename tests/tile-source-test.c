#include <hyscan-network-map-tile-source.h>
#include <math.h>
#include <hyscan-pseudo-mercator.h>

struct
{
  const gchar *url_format;
  guint        min_zoom;
  guint        max_zoom;
  gboolean     valid;
} test_data[] = {
  { "https://c.tile.openstreetmap.org/{z}/{x}/{y}.png",  0, 19, TRUE},
  { "https://c.tile.openstreetmap.org/12/{x}/{y}.png",   0, 19, FALSE},
};

int
main (int    argc,
      char **argv)
{
  HyScanNetworkMapTileSource *source;
  HyScanGeoProjection *mercator;
  HyScanGtkMapTileGrid *grid;
  HyScanGtkMapTile *tile;
  guint i;

  mercator = hyscan_pseudo_mercator_new ();
  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      gboolean result;

      g_message ("Test data %d: %s", i, test_data[i].url_format);

      source = hyscan_network_map_tile_source_new (test_data[i].url_format, mercator,
                                                   test_data[i].min_zoom,
                                                   test_data[i].max_zoom);

      grid = hyscan_gtk_map_tile_source_get_grid (HYSCAN_GTK_MAP_TILE_SOURCE (source));
      tile = hyscan_gtk_map_tile_new (grid, 10, 10, 5);

      result = hyscan_gtk_map_tile_source_fill (HYSCAN_GTK_MAP_TILE_SOURCE (source), tile, NULL);
      g_assert_true (test_data[i].valid == result);

      g_clear_object (&grid);
      g_clear_object (&tile);
      g_clear_object (&source);
    }

  g_object_unref (mercator);
  g_message ("Tests done successfully!");

  return 0;
}

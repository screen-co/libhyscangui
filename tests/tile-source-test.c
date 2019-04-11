#include <hyscan-network-map-tile-source.h>
#include <math.h>

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

/* Создает тайловую сетку для источника тайлов. */
HyScanGtkMapTileGrid *
grid_new (HyScanGtkMapTileSource *source)
{
  gdouble *nums;
  gint nums_len;
  guint min_zoom, max_zoom, zoom;
  HyScanGtkMapTileGrid *grid;

  hyscan_gtk_map_tile_source_get_zoom_limits (source, &min_zoom, &max_zoom);

  nums_len = max_zoom - min_zoom + 1;
  nums = g_newa (gdouble, nums_len);
  for (zoom = min_zoom; zoom < max_zoom + 1; zoom++)
    nums[zoom - min_zoom] = pow (2, zoom);

  grid = hyscan_gtk_map_tile_grid_new (-1.0, 1.0, -1.0, 1.0, min_zoom,
                                       hyscan_gtk_map_tile_source_get_tile_size (source));
  hyscan_gtk_map_tile_grid_set_xnums (grid, nums, nums_len);

  return grid;
}

int
main (int    argc,
      char **argv)
{
  HyScanNetworkMapTileSource *source;
  HyScanGtkMapTileGrid *grid;
  HyScanGtkMapTile *tile;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (test_data); i++)
    {
      gboolean result;

      g_message ("Test data %d: %s", i, test_data[i].url_format);

      source = hyscan_network_map_tile_source_new (test_data[i].url_format,
                                                   test_data[i].min_zoom,
                                                   test_data[i].max_zoom);

      grid = grid_new (HYSCAN_GTK_MAP_TILE_SOURCE (source));
      tile = hyscan_gtk_map_tile_new (grid, 10, 10, 5);

      result = hyscan_gtk_map_tile_source_fill (HYSCAN_GTK_MAP_TILE_SOURCE (source), tile, NULL);
      g_assert_true (test_data[i].valid == result);

      g_clear_object (&grid);
      g_clear_object (&tile);
      g_clear_object (&source);
    }

  g_message ("Tests done successfully!");

  return 0;
}

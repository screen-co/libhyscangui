#include "hyscan-gtk-map-tile-source.h"

G_DEFINE_INTERFACE (HyScanGtkMapTileSource, hyscan_gtk_map_tile_source, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tile_source_default_init (HyScanGtkMapTileSourceInterface *iface)
{
}

/**
 * hyscan_gtk_map_tile_source_find:
 * @tsource:
 * @tile:
 *
 * Поиск тайла @tile в источнике @tsource.
 */
gboolean hyscan_gtk_map_tile_source_find (HyScanGtkMapTileSource *tsource,
                                          HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (tsource), FALSE);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (tsource);
  if (iface->find_tile != NULL)
    return (* iface->find_tile) (tsource, tile);

  return FALSE;
}

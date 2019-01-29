#include "hyscan-gtk-map-tile-source.h"

G_DEFINE_INTERFACE (HyScanGtkMapTileSource, hyscan_gtk_map_tile_source, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tile_source_default_init (HyScanGtkMapTileSourceInterface *iface)
{
}

/**
 * hyscan_gtk_map_tile_source_create:
 * @source:
 * @tile:
 *
 * Заполнение тайла @tile из источника тайлов @source.
 */
gboolean hyscan_gtk_map_tile_source_fill (HyScanGtkMapTileSource *source,
                                          HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), FALSE);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  if (iface->fill_tile != NULL)
    return (* iface->fill_tile) (source, tile);

  return FALSE;
}

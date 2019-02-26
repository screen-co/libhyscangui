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
gboolean
hyscan_gtk_map_tile_source_fill (HyScanGtkMapTileSource *source,
                                 HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), FALSE);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  if (iface->fill_tile != NULL)
    return (* iface->fill_tile) (source, tile);

  return FALSE;
}

/**
 * hyscan_gtk_map_tile_source_get_zoom_limits:
 * @source: указатель на #HyScanGtkMapTileSource
 * @min_zoom: (out): минимальный уровень детализации
 * @max_zoom: (out): максимальный уровень детализации
 *
 * Возвращает уровни детализации, доступные в указанном источнике.
 */
void
hyscan_gtk_map_tile_source_get_zoom_limits (HyScanGtkMapTileSource *source,
                                            guint                  *min_zoom,
                                            guint                  *max_zoom)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source));

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_if_fail (iface->get_zoom_limits != NULL);

  (* iface->get_zoom_limits) (source, min_zoom, max_zoom);
}

/**
 * hyscan_gtk_map_tile_source_get_tile_size:
 * @source: указатель на #HyScanGtkMapTileSource
 *
 * Возвращает размер тайла по высоте и ширине. Тайлы квадратные.
 *
 * Returns: размер тайла в пикселах
 */
guint
hyscan_gtk_map_tile_source_get_tile_size (HyScanGtkMapTileSource *source)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), 0);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->get_tile_size != NULL, 0);

  return (* iface->get_tile_size) (source);
}

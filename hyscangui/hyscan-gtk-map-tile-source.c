/* hyscan-gtk-map-tile-source.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

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
                                 HyScanGtkMapTile       *tile,
                                 GCancellable           *cancellable)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), FALSE);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  if (iface->fill_tile != NULL)
    return (* iface->fill_tile) (source, tile, cancellable);

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
 * Returns: размер тайла в пикселях
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

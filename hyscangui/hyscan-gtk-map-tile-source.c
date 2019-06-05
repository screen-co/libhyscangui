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

/**
 * SECTION: hyscan-gtk-map-tile-source
 * @Short_description: Источник тайлов
 * @Title: HyScanGtkMapTileSource
 *
 * Интерфейс источника тайлов. Основная задача источника тайлов - это получение
 * изображения тайла по его координатам. Для заполнения тайла используется
 * функция hyscan_gtk_map_tile_source_fill()
 *
 * Геометрические параметры источника определяется двумя параметрами:
 * - картографическая проекция, которая устанавливает взаимосвязь координат
 *   точки на карте и на местности,
 * - тайловая сетка, которая определяет по координатам тайла (x, y, z) изображаемую
 *   им область местности.
 *
 * Для получения этих параметров используются соответственно функции
 * hyscan_gtk_map_tile_source_get_projection() и hyscan_gtk_map_tile_source_get_grid().
 *
 */

#include "hyscan-gtk-map-tile-source.h"

G_DEFINE_INTERFACE (HyScanGtkMapTileSource, hyscan_gtk_map_tile_source, G_TYPE_OBJECT)

static void
hyscan_gtk_map_tile_source_default_init (HyScanGtkMapTileSourceInterface *iface)
{
}

/**
 * hyscan_gtk_map_tile_source_create:
 * @source: указатель на #HyScanGtkMapTileSource
 * @tile: тайл
 * @cancellable: #GCancellable для отмены заполнения тайла
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
 * hyscan_gtk_map_tile_source_get_grid:
 * @source: указатель на #HyScanGtkMapTileSource
 *
 * Возвращает тайловую сетку источника тайлов
 *
 * Returns: (transfer full): тайловая сетка источника
 */
HyScanGtkMapTileGrid *
hyscan_gtk_map_tile_source_get_grid (HyScanGtkMapTileSource *source)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), NULL);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->get_grid != NULL, NULL);

  return (* iface->get_grid) (source);
}

/**
 * hyscan_gtk_map_tile_source_get_projection:
 * @source: указатель на #HyScanGtkMapTileSource
 *
 * Возвращает картографическую проекцию источника тайлов
 *
 * Returns: (transfer full): картографическая проекция источника тайлов
 */
HyScanGeoProjection *
hyscan_gtk_map_tile_source_get_projection (HyScanGtkMapTileSource *source)
{
  HyScanGtkMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TILE_SOURCE (source), NULL);

  iface = HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->get_projection != NULL, NULL);

  return (* iface->get_projection) (source);
}

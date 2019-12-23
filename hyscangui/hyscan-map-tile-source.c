/* hyscan-map-tile-source.c
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
 * SECTION: hyscan-map-tile-source
 * @Short_description: Интерфейс источника тайлов
 * @Title: HyScanMapTileSource
 *
 * Источник тайлов загружает в тайл его изображение, т.е. заполняет тайл.
 * Для заполнения тайла используется функция hyscan_map_tile_source_fill()
 *
 * В реализациях источника тайлов следует предусматривать возможность отмены
 * заполнения, если этот процесс занимает значительное время (например, чтение
 * из файла или загрузка файла по сети). Для возможности отмены загрузки
 * в hyscan_map_tile_source_fill() передаётся объект #GCancellable.
 *
 * Источник тайлов характеризуется двумя параметрами:
 * 1. картографическая проекция, которая устанавливает взаимосвязь координат
 *    точки на карте (т.е. в логической СК) и на местности,
 * 2. тайловая сетка, которая определяет по координатам тайла (x, y, z) изображаемую
 *    тайлом область местности.
 *
 * Для получения этих параметров используются соответственно функции
 * hyscan_map_tile_source_get_projection() и hyscan_map_tile_source_get_grid().
 *
 * Если оба параметра у двух источников совпадают, то источники будут изображать на
 * тайле одну и ту же область местности.
 *
 */

#include "hyscan-map-tile-source.h"

G_DEFINE_INTERFACE (HyScanMapTileSource, hyscan_map_tile_source, G_TYPE_OBJECT)

static void
hyscan_map_tile_source_default_init (HyScanMapTileSourceInterface *iface)
{
}

/**
 * hyscan_map_tile_source_create:
 * @source: указатель на #HyScanMapTileSource
 * @tile: тайл
 * @cancellable: #GCancellable для отмены заполнения тайла
 *
 * Заполняет тайл @tile его изображением. Для отмены заполнения необходимо
 * использовать объект @cancellable.
 *
 */
gboolean
hyscan_map_tile_source_fill (HyScanMapTileSource *source,
                             HyScanMapTile       *tile,
                             GCancellable        *cancellable)
{
  HyScanMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_SOURCE (source), FALSE);

  iface = HYSCAN_MAP_TILE_SOURCE_GET_IFACE (source);
  if (iface->fill_tile != NULL)
    return (* iface->fill_tile) (source, tile, cancellable);

  return FALSE;
}

/**
 * hyscan_map_tile_source_get_grid:
 * @source: указатель на #HyScanMapTileSource
 *
 * Возвращает тайловую сетку источника тайлов
 *
 * Returns: (transfer full): тайловая сетка источника
 */
HyScanMapTileGrid *
hyscan_map_tile_source_get_grid (HyScanMapTileSource *source)
{
  HyScanMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_SOURCE (source), NULL);

  iface = HYSCAN_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->get_grid != NULL, NULL);

  return (* iface->get_grid) (source);
}

/**
 * hyscan_map_tile_source_get_projection:
 * @source: указатель на #HyScanMapTileSource
 *
 * Возвращает картографическую проекцию источника тайлов
 *
 * Returns: (transfer full): картографическая проекция источника тайлов
 */
HyScanGeoProjection *
hyscan_map_tile_source_get_projection (HyScanMapTileSource *source)
{
  HyScanMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_SOURCE (source), NULL);

  iface = HYSCAN_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->get_projection != NULL, NULL);

  return (* iface->get_projection) (source);
}

/**
 * hyscan_map_tile_source_hash:
 * @source: указатель на #HyScanMapTileSource
 *
 * Получает хэш источника тайлов. Источники, которые дают на выходе разные
 * изображения тайлов имеют разное значение хэша. При этом один и тот же объект
 * источника не меняет значение хэша в течение всего своего времени жизни.
 *
 * Returns: хэш источника тайлов
 */
guint
hyscan_map_tile_source_hash (HyScanMapTileSource *source)
{
  HyScanMapTileSourceInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_MAP_TILE_SOURCE (source), 0);

  iface = HYSCAN_MAP_TILE_SOURCE_GET_IFACE (source);
  g_return_val_if_fail (iface->hash != NULL, 0);

  return (* iface->hash) (source);
}

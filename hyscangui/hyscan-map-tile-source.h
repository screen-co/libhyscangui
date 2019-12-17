/* hyscan-map-tile-source.h
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

#ifndef __HYSCAN_MAP_TILE_SOURCE_H__
#define __HYSCAN_MAP_TILE_SOURCE_H__

#include <glib-object.h>
#include <cairo.h>
#include <hyscan-api.h>
#include <hyscan-map-tile.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TILE_SOURCE            (hyscan_map_tile_source_get_type ())
#define HYSCAN_MAP_TILE_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE, HyScanMapTileSource))
#define HYSCAN_IS_MAP_TILE_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE))
#define HYSCAN_MAP_TILE_SOURCE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_MAP_TILE_SOURCE, HyScanMapTileSourceInterface))

typedef struct _HyScanMapTileSource HyScanMapTileSource;
typedef struct _HyScanMapTileSourceInterface HyScanMapTileSourceInterface;

/**
 * HyScanMapTileSourceInterface:
 * @fill_tile: заполняет тайл изображением
 * @get_grid: возвращает тайловую сетку источника
 * @get_projection: возвращает картографическую проекцию источника
 * @hash: возвращает хэш источника
 */
struct _HyScanMapTileSourceInterface
{
  GTypeInterface           g_iface;

  gboolean               (*fill_tile)               (HyScanMapTileSource        *source,
                                                     HyScanMapTile              *tile,
                                                     GCancellable               *cancellable);

  HyScanMapTileGrid *    (*get_grid)                (HyScanMapTileSource        *source);

  HyScanGeoProjection *  (*get_projection)          (HyScanMapTileSource        *source);

  guint                  (*hash)                    (HyScanMapTileSource        *source);
};

HYSCAN_API
GType                   hyscan_map_tile_source_get_type                  (void);

HYSCAN_API
HyScanMapTileGrid *     hyscan_map_tile_source_get_grid                  (HyScanMapTileSource *source);

HYSCAN_API
HyScanGeoProjection *   hyscan_map_tile_source_get_projection            (HyScanMapTileSource *source);

HYSCAN_API
gboolean                hyscan_map_tile_source_fill                      (HyScanMapTileSource *source,
                                                                          HyScanMapTile       *tile,
                                                                          GCancellable        *cancellable);

HYSCAN_API
guint                   hyscan_map_tile_source_hash                      (HyScanMapTileSource *source);


G_END_DECLS

#endif /* __HYSCAN_MAP_TILE_SOURCE_H__ */

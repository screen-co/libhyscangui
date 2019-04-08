/* hyscan-gtk-map-tile-source.h
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

#ifndef __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__
#define __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__

#include <hyscan-gtk-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE             (hyscan_network_map_tile_source_get_type ())
#define HYSCAN_NETWORK_MAP_TILE_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSource))
#define HYSCAN_IS_NETWORK_MAP_TILE_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE))
#define hyscan_network_map_tile_source_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSourceClass))
#define HYSCAN_IS_NETWORK_MAP_TILE_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE))
#define hyscan_network_map_tile_source_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSourceClass))

typedef struct _HyScanNetworkMapTileSource HyScanNetworkMapTileSource;
typedef struct _HyScanNetworkMapTileSourcePrivate HyScanNetworkMapTileSourcePrivate;
typedef struct _HyScanNetworkMapTileSourceClass HyScanNetworkMapTileSourceClass;

struct _HyScanNetworkMapTileSource
{
  GObject parent_instance;

  HyScanNetworkMapTileSourcePrivate *priv;
};

struct _HyScanNetworkMapTileSourceClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                          hyscan_network_map_tile_source_get_type         (void);

HYSCAN_API
HyScanNetworkMapTileSource *   hyscan_network_map_tile_source_new              (const gchar *url_format,
                                                                                guint        min_zoom,
                                                                                guint        max_zoom);

G_END_DECLS

#endif /* __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__ */

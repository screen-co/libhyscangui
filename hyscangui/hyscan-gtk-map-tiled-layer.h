/* hyscan-gtk-map-tiled-layer.h
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

#ifndef __HYSCAN_GTK_MAP_TILED_LAYER_H__
#define __HYSCAN_GTK_MAP_TILED_LAYER_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-gtk-map-tile.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILED_LAYER             (hyscan_gtk_map_tiled_layer_get_type ())
#define HYSCAN_GTK_MAP_TILED_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayer))
#define HYSCAN_IS_GTK_MAP_TILED_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER))
#define HYSCAN_GTK_MAP_TILED_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayerClass))
#define HYSCAN_IS_GTK_MAP_TILED_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILED_LAYER))
#define HYSCAN_GTK_MAP_TILED_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayerClass))

typedef struct _HyScanGtkMapTiledLayer HyScanGtkMapTiledLayer;
typedef struct _HyScanGtkMapTiledLayerPrivate HyScanGtkMapTiledLayerPrivate;
typedef struct _HyScanGtkMapTiledLayerClass HyScanGtkMapTiledLayerClass;

struct _HyScanGtkMapTiledLayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapTiledLayerPrivate *priv;
};

/**
 * HyScanGtkMapTiledLayerClass:
 * @fill_tile: заполняет указанный тайл изображением
 */
struct _HyScanGtkMapTiledLayerClass
{
  GInitiallyUnownedClass parent_class;

  void                (*fill_tile)                     (HyScanGtkMapTiledLayer *tiled_layer,
                                                        HyScanGtkMapTile       *tile);
};

HYSCAN_API
GType                hyscan_gtk_map_tiled_layer_get_type            (void);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_draw                (HyScanGtkMapTiledLayer *tiled_layer,
                                                                     cairo_t                *cairo);


HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_set_area_mod        (HyScanGtkMapTiledLayer *tiled_layer,
                                                                     HyScanGeoCartesian2D   *point0,
                                                                     HyScanGeoCartesian2D   *point1);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_set_param_mod       (HyScanGtkMapTiledLayer *tiled_layer);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_request_draw        (HyScanGtkMapTiledLayer *tiled_layer);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILED_LAYER_H__ */

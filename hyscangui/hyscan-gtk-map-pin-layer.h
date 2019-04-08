/* hyscan-gtk-map-pin-layer.h
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

#ifndef __HYSCAN_GTK_MAP_PIN_LAYER_H__
#define __HYSCAN_GTK_MAP_PIN_LAYER_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_PIN_LAYER             (hyscan_gtk_map_pin_layer_get_type ())
#define HYSCAN_GTK_MAP_PIN_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayer))
#define HYSCAN_IS_GTK_MAP_PIN_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER))
#define HYSCAN_GTK_MAP_PIN_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayerClass))
#define HYSCAN_IS_GTK_MAP_PIN_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_PIN_LAYER))
#define HYSCAN_GTK_MAP_PIN_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_PIN_LAYER, HyScanGtkMapPinLayerClass))

typedef struct _HyScanGtkMapPinLayer HyScanGtkMapPinLayer;
typedef struct _HyScanGtkMapPinLayerPrivate HyScanGtkMapPinLayerPrivate;
typedef struct _HyScanGtkMapPinLayerClass HyScanGtkMapPinLayerClass;

typedef enum
{
  HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE,
  HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN,
} HyScanGtkMapPinLayerPinShape;

struct _HyScanGtkMapPinLayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapPinLayerPrivate *priv;
};

struct _HyScanGtkMapPinLayerClass
{
  GInitiallyUnownedClass parent_class;

  void       (*draw)                     (HyScanGtkMapPinLayer *layer,
                                          cairo_t              *cairo);
};

HYSCAN_API
GType                  hyscan_gtk_map_pin_layer_get_type         (void);

HYSCAN_API
HyScanGtkLayer *       hyscan_gtk_map_pin_layer_new              (void);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_clear            (HyScanGtkMapPinLayer         *layer);

HYSCAN_API
GList*                 hyscan_gtk_map_pin_layer_get_points       (HyScanGtkMapPinLayer         *layer);

HYSCAN_API
HyScanGtkMapPoint *    hyscan_gtk_map_pin_layer_insert_before    (HyScanGtkMapPinLayer         *layer,
                                                                  HyScanGtkMapPoint            *point,
                                                                  GList                        *sibling);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_set_pin_size     (HyScanGtkMapPinLayer         *layer,
                                                                  guint                         size);

HYSCAN_API
gconstpointer          hyscan_gtk_map_pin_layer_start_drag       (HyScanGtkMapPinLayer         *layer,
                                                                  HyScanGtkMapPoint            *handle_point);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_set_color_prime  (HyScanGtkMapPinLayer         *layer,
                                                                  GdkRGBA                       color);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_set_color_second (HyScanGtkMapPinLayer         *layer,
                                                                  GdkRGBA                       color);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_set_color_stroke (HyScanGtkMapPinLayer         *layer,
                                                                  GdkRGBA                       color);

HYSCAN_API
void                   hyscan_gtk_map_pin_layer_set_pin_shape    (HyScanGtkMapPinLayer         *layer,
                                                                  HyScanGtkMapPinLayerPinShape  shape);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_PIN_LAYER_H__ */

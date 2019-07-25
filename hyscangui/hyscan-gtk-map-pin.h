/* hyscan-gtk-map-pin.h
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

#ifndef __HYSCAN_GTK_MAP_PIN_H__
#define __HYSCAN_GTK_MAP_PIN_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_PIN             (hyscan_gtk_map_pin_get_type ())
#define HYSCAN_GTK_MAP_PIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_PIN, HyScanGtkMapPin))
#define HYSCAN_IS_GTK_MAP_PIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_PIN))
#define HYSCAN_GTK_MAP_PIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_PIN, HyScanGtkMapPinClass))
#define HYSCAN_IS_GTK_MAP_PIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_PIN))
#define HYSCAN_GTK_MAP_PIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_PIN, HyScanGtkMapPinClass))

typedef struct _HyScanGtkMapPin HyScanGtkMapPin;
typedef struct _HyScanGtkMapPinPrivate HyScanGtkMapPinPrivate;
typedef struct _HyScanGtkMapPinClass HyScanGtkMapPinClass;
typedef struct _HyScanGtkMapPinItem HyScanGtkMapPinItem;

/**
 * HyScanGtkMapPinMarkerShape:
 * @HYSCAN_GTK_MAP_PIN_SHAPE_CIRCLE: маркера в форме круга, целевая точка
 *   расположена в центре круга
 * @HYSCAN_GTK_MAP_PIN_SHAPE_PIN: маркер в форме булавки, целевая точка
 *   расположена в месте протыкания карты булавкой
 */
typedef enum
{
  HYSCAN_GTK_MAP_PIN_SHAPE_CIRCLE,
  HYSCAN_GTK_MAP_PIN_SHAPE_PIN,
} HyScanGtkMapPinMarkerShape;

struct _HyScanGtkMapPin
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapPinPrivate *priv;
};

/**
 * HyScanGtkMapPinClass:
 * @draw: функция вызывается в момент отрисовки слоя
 * @changed: функция вызывается при добавлении или удалении точек слоя
 */
struct _HyScanGtkMapPinClass
{
  GInitiallyUnownedClass parent_class;

  void       (*draw)                     (HyScanGtkMapPin *layer,
                                          cairo_t         *cairo);

  void       (*changed)                  (HyScanGtkMapPin *layer);
};

HYSCAN_API
GType                  hyscan_gtk_map_pin_get_type         (void);

HYSCAN_API
HyScanGtkLayer *       hyscan_gtk_map_pin_new              (void);

HYSCAN_API
void                   hyscan_gtk_map_pin_clear            (HyScanGtkMapPin            *layer);

HYSCAN_API
GList*                 hyscan_gtk_map_pin_get_points       (HyScanGtkMapPin            *layer);

HYSCAN_API
HyScanGtkMapPinItem *  hyscan_gtk_map_pin_insert_before    (HyScanGtkMapPin            *layer,
                                                            HyScanGtkMapPoint          *point,
                                                            GList                      *sibling);

HYSCAN_API
void                   hyscan_gtk_map_pin_set_marker_size  (HyScanGtkMapPin            *layer,
                                                            guint                       size);

HYSCAN_API
void                   hyscan_gtk_map_pin_set_marker_shape (HyScanGtkMapPin            *layer,
                                                            HyScanGtkMapPinMarkerShape  shape);

HYSCAN_API
gconstpointer          hyscan_gtk_map_pin_start_drag       (HyScanGtkMapPin            *layer,
                                                            HyScanGtkMapPinItem        *handle_point);

HYSCAN_API
void                   hyscan_gtk_map_pin_set_color_prime  (HyScanGtkMapPin            *layer,
                                                            GdkRGBA                     color);

HYSCAN_API
void                   hyscan_gtk_map_pin_set_color_second (HyScanGtkMapPin            *layer,
                                                            GdkRGBA                     color);

HYSCAN_API
void                   hyscan_gtk_map_pin_set_color_stroke (HyScanGtkMapPin            *layer,
                                                            GdkRGBA                     color);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_PIN_H__ */

/* hyscan-gtk-map.h
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

#ifndef __HYSCAN_GTK_MAP_H__
#define __HYSCAN_GTK_MAP_H__

#include <hyscan-geo-projection.h>
#include <hyscan-gtk-layer-container.h>

/**
 * HYSCAN_GTK_MAP_EQUATOR_LENGTH: (value 40075696)
 * Длина экватора в метрах
 */
#define HYSCAN_GTK_MAP_EQUATOR_LENGTH 40075696.0
/*
 * HYSCAN_GTK_MAP_DEFAULT_PPI: (value 96.0)
 * Разрешающая способность монитора по умолчанию
 * (количество пикселей на дюйм).
 */
#define HYSCAN_GTK_MAP_DEFAULT_PPI 96.0
/*
 * HYSCAN_GTK_MAP_MM_PER_INCH: (value 25.4)
 * Размер дюйма в миллиметрах.
 */
#define HYSCAN_GTK_MAP_MM_PER_INCH 25.4

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP             (hyscan_gtk_map_get_type ())
#define HYSCAN_GTK_MAP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP, HyScanGtkMap))
#define HYSCAN_IS_GTK_MAP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP))
#define HYSCAN_GTK_MAP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP, HyScanGtkMapClass))
#define HYSCAN_IS_GTK_MAP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP))
#define HYSCAN_GTK_MAP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP, HyScanGtkMapClass))

typedef struct _HyScanGtkMap HyScanGtkMap;
typedef struct _HyScanGtkMapPrivate HyScanGtkMapPrivate;
typedef struct _HyScanGtkMapClass HyScanGtkMapClass;

/**
 * HyScanGtkMapPoint:
 * @geo: географические координаты точки
 * @c2d: декартовы координаты точки на картографической проекции
 *
 * Структура хранит координаты точки в двух форматах:
 * - географические (широта и долгота)
 * - декартовы (x, y), которые зависят от выбранной картографической проекции.
 */
typedef struct
{
  HyScanGeoPoint       geo;
  HyScanGeoCartesian2D c2d;
} HyScanGtkMapPoint;

struct _HyScanGtkMap
{
  HyScanGtkLayerContainer parent_instance;

  HyScanGtkMapPrivate *priv;
};

struct _HyScanGtkMapClass
{
  HyScanGtkLayerContainerClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_new              (HyScanGeoPoint          center);

HYSCAN_API
void                   hyscan_gtk_map_set_projection   (HyScanGtkMap           *map,
                                                        HyScanGeoProjection    *projection);

HYSCAN_API
HyScanGeoProjection *  hyscan_gtk_map_get_projection   (HyScanGtkMap           *map);

HYSCAN_API
void                   hyscan_gtk_map_move_to          (HyScanGtkMap           *map,
                                                        HyScanGeoPoint          center);

HYSCAN_API
gdouble *              hyscan_gtk_map_create_scales2  (gdouble                  min_scale,
                                                       gdouble                  max_scale,
                                                       gint                     steps,
                                                       gint                    *scales_len);

HYSCAN_API
void                   hyscan_gtk_map_set_scale_px    (HyScanGtkMap            *map,
                                                       gdouble                  scale);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale_px    (HyScanGtkMap            *map);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale_ratio (HyScanGtkMap            *map);

HYSCAN_API
gdouble                hyscan_gtk_map_get_scale_value (HyScanGtkMap            *map,
                                                       HyScanGeoPoint           coord);

HYSCAN_API
void                   hyscan_gtk_map_set_scales_meter(HyScanGtkMap            *map,
                                                       const gdouble           *scales,
                                                       gsize                    scales_len);

HYSCAN_API
gint                   hyscan_gtk_map_get_scale_idx   (HyScanGtkMap            *map,
                                                       gdouble                 *scale);

HYSCAN_API
void                   hyscan_gtk_map_set_scale_idx   (HyScanGtkMap            *map,
                                                       guint                    idx,
                                                       gdouble                  center_x,
                                                       gdouble                  center_y);

HYSCAN_API
gdouble *              hyscan_gtk_map_get_scales      (HyScanGtkMap            *map,
                                                       guint                   *scales_len);

HYSCAN_API
void                   hyscan_gtk_map_point_to_geo     (HyScanGtkMap           *map,
                                                        HyScanGeoPoint         *coords,
                                                        gdouble                 x,
                                                        gdouble                 y);

HYSCAN_API
void                   hyscan_gtk_map_value_to_geo     (HyScanGtkMap           *map,
                                                        HyScanGeoPoint         *coords,
                                                        HyScanGeoCartesian2D    c2d);

HYSCAN_API
void                   hyscan_gtk_map_geo_to_value     (HyScanGtkMap           *map,
                                                        HyScanGeoPoint          coords,
                                                        HyScanGeoCartesian2D   *c2d);

HYSCAN_API
HyScanGtkMapPoint *    hyscan_gtk_map_point_copy       (HyScanGtkMapPoint      *point);

HYSCAN_API
void                   hyscan_gtk_map_point_free       (HyScanGtkMapPoint      *point);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_H__ */

/* hyscan-gtk-map-track-draw.h
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

#ifndef __HYSCAN_GTK_MAP_TRACK_DRAW_H__
#define __HYSCAN_GTK_MAP_TRACK_DRAW_H__

#include <hyscan-geo.h>
#include <cairo.h>
#include <hyscan-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK_DRAW            (hyscan_gtk_map_track_draw_get_type ())
#define HYSCAN_GTK_MAP_TRACK_DRAW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDraw))
#define HYSCAN_IS_GTK_MAP_TRACK_DRAW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW))
#define HYSCAN_GTK_MAP_TRACK_DRAW_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDrawInterface))

typedef struct _HyScanGtkMapTrackDraw HyScanGtkMapTrackDraw;
typedef struct _HyScanGtkMapTrackDrawInterface HyScanGtkMapTrackDrawInterface;
typedef struct _HyScanGtkMapTrackDrawData HyScanGtkMapTrackDrawData;
typedef struct _HyScanGtkMapTrackPoint HyScanGtkMapTrackPoint;
typedef enum _HyScanGtkMapTrackDrawSource HyScanGtkMapTrackDrawSource;

/**
 * HyScanGtkMapTrackDrawSource:
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV: навигационные данные
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT: данные левого борта
 * @HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT: данные правого борта
 *
 * Тип источника данных точки галса.
 */
enum _HyScanGtkMapTrackDrawSource
{
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV,
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT,
  HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT,
};

/**
 * HyScanGtkMapTrackDrawData:
 * @starboard: (element-type HyScanGtkMapTrackPoint): точки правого борта
 * @port: (element-type HyScanGtkMapTrackPoint): точки левого борта
 * @nav: (element-type HyScanGtkMapTrackPoint): точки навигации
 * @from: минимальные координаты области, внути которой лежат все точки
 * @to: максимальные координаты области, внути которой лежат все точки
 *
 * Данные для отрисовки галса.
 */
struct _HyScanGtkMapTrackDrawData
{
  GList                  *starboard;
  GList                  *port;
  GList                  *nav;
  HyScanGeoCartesian2D    from;
  HyScanGeoCartesian2D    to;
};

/**
 * HyScanGtkMapTrackPoint:
 * @source: источник данных
 * @index: индекс записи в канале источника данных
 * @time: время фиксации данных
 * @geo: географические координаты точки и курс движения
 * @b_angle: курс с поправкой на смещение антенн GPS и ГЛ, рад
 * @b_length_m: длина луча, метры
 * @nr_length_m: длина ближней зоны диаграммы направленности, метры
 * @scale: масштаб картографической проекции в текущей точке
 * @aperture: апертура антенны, ед. проекции
 * @ship_c2d: координаты судна
 * @start_c2d: координаты начала луча
 * @nr_c2d: координаты точки на конце ближней зоны
 * @fr_c2d: координаты центральной точки на конце дальней зоны
 * @fr1_c2d: координаты одной крайней точки на конце дальней зоны
 * @fr2_c2d: координаты второй крайней точки на конце дальней зоны
 * @dist_along: расстояние от начала галса до текущей точки вдоль линии галса, ед. проекции
 * @b_dist: длина луча, ед. проекции
 * @straight: признак того, что точка находится на относительно прямолинейном участке
 *
 * Информация о точке на галсе, соотвествующая данным по индексу @index из источника @source.
 * Положение судна, антенна гидролокатора, характерные точки диаграммы направленности указаны
 * в координатах картографической проекции.
 *
 */
struct _HyScanGtkMapTrackPoint
{
  HyScanGtkMapTrackDrawSource     source;
  guint32                         index;
  gint64                          time;

  HyScanGeoGeodetic               geo;
  gdouble                         b_angle;
  gdouble                         b_length_m;
  gdouble                         nr_length_m;

  gdouble                         scale;
  gdouble                         aperture;
  HyScanGeoCartesian2D            ship_c2d;
  HyScanGeoCartesian2D            start_c2d;
  HyScanGeoCartesian2D            nr_c2d;
  HyScanGeoCartesian2D            fr_c2d;
  HyScanGeoCartesian2D            fr1_c2d;
  HyScanGeoCartesian2D            fr2_c2d;
  gdouble                         dist_along;
  gdouble                         b_dist;
  gboolean                        straight;
};

/**
 * HyScanGtkMapTrackDrawInterface:
 * @g_iface: базовый интерфейс
 * @get_param: функция считывает параметры объекта отрисовки
 * @draw_region: функция рисует галс на указанной области поверхности cairo.
 */
struct _HyScanGtkMapTrackDrawInterface
{
  GTypeInterface       g_iface;

  HyScanParam *        (*get_param)                  (HyScanGtkMapTrackDraw      *track_draw);

  void                 (*draw_region)                (HyScanGtkMapTrackDraw      *track_draw,
                                                      HyScanGtkMapTrackDrawData  *data,
                                                      cairo_t                    *cairo,
                                                      gdouble                     scale,
                                                      HyScanGeoCartesian2D       *from,
                                                      HyScanGeoCartesian2D       *to);
};

HYSCAN_API
GType                    hyscan_gtk_map_track_draw_get_type                  (void);

HYSCAN_API
void                     hyscan_gtk_map_track_draw_region                    (HyScanGtkMapTrackDraw        *track_draw,
                                                                              HyScanGtkMapTrackDrawData    *data,
                                                                              cairo_t                      *cairo,
                                                                              gdouble                       scale,
                                                                              HyScanGeoCartesian2D         *from,
                                                                              HyScanGeoCartesian2D         *to);

HYSCAN_API
HyScanParam *            hyscan_gtk_map_track_draw_get_param                 (HyScanGtkMapTrackDraw        *track_draw);

void                     hyscan_gtk_map_track_draw_scale                     (HyScanGeoCartesian2D         *global,
                                                                              HyScanGeoCartesian2D         *from,
                                                                              HyScanGeoCartesian2D         *to,
                                                                              gdouble                       scale,
                                                                              HyScanGeoCartesian2D         *local);

HYSCAN_API
void                     hyscan_gtk_map_track_point_free                     (HyScanGtkMapTrackPoint       *point);

HYSCAN_API
HyScanGtkMapTrackPoint * hyscan_gtk_map_track_point_copy                     (const HyScanGtkMapTrackPoint *point);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_DRAW_H__ */

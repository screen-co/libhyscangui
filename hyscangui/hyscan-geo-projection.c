/* hyscan-geo-projection.c
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
 * SECTION: hyscan-geo-projection
 * @Short_description: Картографическая проекция
 * @Title: HyScanGeoProjection
 *
 * Картографическая проекция отображает поверхность Земли на плоскость.
 *
 * Для перевода географических координат (широта, долгота) в координаты на
 * плоскости проекции (x, y) и обратно используются функции
 * hyscan_geo_projection_geo_to_value()  и hyscan_geo_projection_value_to_geo()
 * соответственно.
 *
 * Чтобы получить значение масштаба в опредленной точке используйте
 * hyscan_geo_projection_get_scale().
 *
 * Границы проекции на плоскости можно получить с помощью
 * hyscan_geo_projection_get_limits().
 *
 * Проекция Меркатора реализована в классах #HyScanMercator и #HyScanPseudoMercator.
 *
 */

#include "hyscan-geo-projection.h"

G_DEFINE_INTERFACE (HyScanGeoProjection, hyscan_geo_projection, G_TYPE_OBJECT)

static void
hyscan_geo_projection_default_init (HyScanGeoProjectionInterface *iface)
{
}

/**
 * hyscan_geo_projection_value_to_geo:
 * @mercator: указатель на проекцию #HyScanGeoProjection
 * @coords: (out): соответствующая географическая координата
 * @x: координата x в прямоугольной СК проекции
 * @y: координата y в прямоугольной СК проекции
 *
 * Переводит координаты на карте (@x, @y) в географические координаты @coords.
 */
void
hyscan_geo_projection_value_to_geo (HyScanGeoProjection *geo_projection,
                                    HyScanGeoGeodetic   *coords,
                                    gdouble              x,
                                    gdouble              y)
{
  HyScanGeoProjectionInterface *iface;

  g_return_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection));

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->value_to_geo != NULL)
    (* iface->value_to_geo) (geo_projection, coords, x, y);
}

/**
 * hyscan_geo_projection_geo_to_value:
 * @mercator: указатель на проекцию #HyScanGeoProjection
 * @coords: географические координаты точки
 * @c2d: (out): координаты в прямоугольной СК
 *
 * Переводит географические координаты @coords в координаты (@x, @y)
 * прямоугольной СК карты.
 */
void
hyscan_geo_projection_geo_to_value (HyScanGeoProjection  *geo_projection,
                                    HyScanGeoGeodetic     coords,
                                    HyScanGeoCartesian2D *c2d)
{
  HyScanGeoProjectionInterface *iface;

  g_return_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection));

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->geo_to_value != NULL)
    (* iface->geo_to_value) (geo_projection, coords, c2d);
}

/**
 * hyscan_geo_projection_get_limits:
 * @mercator: указатель на проекцию #HyScanGeoProjection
 * @min_x: (out): минимальное значение x проекции
 * @max_x: (out): максимальное значение x проекции
 * @min_y: (out): минимальное значение y проекции
 * @max_y: (out): максимальное значение y проекции
 *
 * Определяет границы проекции. Может быть использована для реализации
 * функции get_limits в #GtkCifroAreaClass.
 */
void
hyscan_geo_projection_get_limits (HyScanGeoProjection *geo_projection,
                                  gdouble             *min_x,
                                  gdouble             *max_x,
                                  gdouble             *min_y,
                                  gdouble             *max_y)
{
  HyScanGeoProjectionInterface *iface;

  g_return_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection));

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->get_limits != NULL)
    (* iface->get_limits) (geo_projection, min_x, max_x, min_y, max_y);
}

/**
 * hyscan_geo_projection_get_scale:
 * @mercator: указатель на проекцию #geo_projection
 * @coords: географические координаты #HyScanGeoGeodetic
 *
 * Определяет масштаб проекции в указнной точке @coords. Масштаб показывает,
 * сколько метров местности соответствует единичному отрезку на проекции.
 *
 * todo: scale_x, scale_y
 *
 * Returns: масштаб проекции (метры/единичный отрезок)
 */
gdouble
hyscan_geo_projection_get_scale (HyScanGeoProjection *geo_projection,
                                 HyScanGeoGeodetic    coords)
{
  HyScanGeoProjectionInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection), -1.0);

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->get_scale != NULL)
    return (* iface->get_scale) (geo_projection, coords);

  return -1.0;
}

/**
 * hyscan_geo_projection_hash:
 * @geo_projection: указатель на проекцию #HyScanGeoProjection
 *
 * Возвращает хэш проекции. Проекции с равными значениями хэша одинаковые, то есть
 * переводят географические координаты точки в одни и те же координаты на карте.
 * Разные проекции имеют разный хэш.
 *
 * Returns: хэш проекции
 */
guint
hyscan_geo_projection_hash (HyScanGeoProjection *geo_projection)
{
  HyScanGeoProjectionInterface *iface;

  g_return_val_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection), 0);

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->hash != NULL)
    return (* iface->hash) (geo_projection);

  return 0;
}

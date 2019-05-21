/* hyscan-cartesian.h
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

#ifndef __HYSCAN_CARTESIAN_H__
#define __HYSCAN_CARTESIAN_H__

#include <hyscan-geo.h>

HYSCAN_API
gboolean        hyscan_cartesian_is_point_inside     (HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to);

HYSCAN_API
gboolean         hyscan_cartesian_is_inside          (HyScanGeoCartesian2D *segment_end,
                                                      HyScanGeoCartesian2D *segment_start,
                                                      HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to);

HYSCAN_API
gdouble          hyscan_cartesian_distance_to_line   (HyScanGeoCartesian2D *p1,
                                                      HyScanGeoCartesian2D *p2,
                                                      HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *nearest_point);

HYSCAN_API
gdouble          hyscan_cartesian_distance           (HyScanGeoCartesian2D *p1,
                                                      HyScanGeoCartesian2D *p2);

HYSCAN_API
void             hyscan_cartesian_rotate             (HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *center,
                                                      gdouble               angle,
                                                      HyScanGeoCartesian2D *rotated);

HYSCAN_API
void             hyscan_cartesian_rotate_area        (HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to,
                                                      HyScanGeoCartesian2D *center,
                                                      gdouble               angle,
                                                      HyScanGeoCartesian2D *rotated_from,
                                                      HyScanGeoCartesian2D *rotated_to);

#endif /* __HYSCAN_CARTESIAN_H__ */

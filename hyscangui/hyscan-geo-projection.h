/* hyscan-geo-projection.h
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

#ifndef __HYSCAN_GEO_PROJECTION_H__
#define __HYSCAN_GEO_PROJECTION_H__

#include <hyscan-geo.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GEO_PROJECTION            (hyscan_geo_projection_get_type ())
#define HYSCAN_GEO_PROJECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GEO_PROJECTION, HyScanGeoProjection))
#define HYSCAN_IS_GEO_PROJECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GEO_PROJECTION))
#define HYSCAN_GEO_PROJECTION_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GEO_PROJECTION, HyScanGeoProjectionInterface))

typedef struct _HyScanGeoProjection HyScanGeoProjection;
typedef struct _HyScanGeoProjectionInterface HyScanGeoProjectionInterface;

struct _HyScanGeoProjectionInterface
{
  GTypeInterface       g_iface;

  void                 (*value_to_geo)         (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic      *coords,
                                                gdouble                 x,
                                                gdouble                 y);

  void                 (*geo_to_value)         (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic       coords,
                                                HyScanGeoCartesian2D   *c2d);

  void                 (*get_limits)           (HyScanGeoProjection    *geo_projection,
                                                gdouble                *min_x,
                                                gdouble                *max_x,
                                                gdouble                *min_y,
                                                gdouble                *max_y);

  gdouble              (*get_scale)            (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic       coords);

  guint                (*hash)                 (HyScanGeoProjection    *geo_projection);
};

HYSCAN_API
GType                  hyscan_geo_projection_get_type         (void);



HYSCAN_API
void                   hyscan_geo_projection_value_to_geo     (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic      *coords,
                                                               gdouble                 x,
                                                               gdouble                 y);

HYSCAN_API
void                   hyscan_geo_projection_geo_to_value     (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic       coords,
                                                               HyScanGeoCartesian2D   *c2d);

HYSCAN_API
void                   hyscan_geo_projection_get_limits       (HyScanGeoProjection    *geo_projection,
                                                               gdouble                *min_x,
                                                               gdouble                *max_x,
                                                               gdouble                *min_y,
                                                               gdouble                *max_y);

HYSCAN_API
gdouble                hyscan_geo_projection_get_scale        (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic       coords);

HYSCAN_API
guint                  hyscan_geo_projection_hash             (HyScanGeoProjection    *geo_projection);

G_END_DECLS

#endif /* __HYSCAN_GEO_PROJECTION_H__ */

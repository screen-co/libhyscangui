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
 * @param mercator: указатель на проекцию #HyScanGeoProjection
 * @param coords: географическая координата точки
 * @param x: (out): координата x прямоугольной СК проекции
 * @param y: (out): координата y прямоугольной СК проекции
 *
 * Переводит географические координаты @coords в координаты (@x, @y)
 * прямоугольной СК карты.
 */
void
hyscan_geo_projection_geo_to_value (HyScanGeoProjection *geo_projection,
                                    HyScanGeoGeodetic    coords,
                                    gdouble             *x,
                                    gdouble             *y)
{
  HyScanGeoProjectionInterface *iface;

  g_return_if_fail (HYSCAN_IS_GEO_PROJECTION (geo_projection));

  iface = HYSCAN_GEO_PROJECTION_GET_IFACE (geo_projection);
  if (iface->geo_to_value != NULL)
    (* iface->geo_to_value) (geo_projection, coords, x, y);
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

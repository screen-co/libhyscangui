#include "hyscan-mercator.h"
#include <math.h>

enum
{
  PROP_O,
  PROP_ELLIPSOID,
};

struct _HyScanMercatorPrivate
{
  HyScanGeoEllipsoidParam      ellipsoid;
  gdouble                      max_value;
};

static void    hyscan_mercator_set_property             (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_mercator_object_constructed       (GObject               *object);
static void    hyscan_mercator_object_finalize          (GObject               *object);
static void    hyscan_mercator_set_ellipsoid            (HyScanMercator          *mercator,
                                                         HyScanGeoEllipsoidParam  p);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMercator, hyscan_mercator, G_TYPE_OBJECT)

static void
hyscan_mercator_class_init (HyScanMercatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mercator_set_property;

  object_class->constructed = hyscan_mercator_object_constructed;
  object_class->finalize = hyscan_mercator_object_finalize;

}

static void
hyscan_mercator_init (HyScanMercator *mercator)
{
  mercator->priv = hyscan_mercator_get_instance_private (mercator);
}

static void
hyscan_mercator_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  HyScanMercator *mercator = HYSCAN_MERCATOR (object);
  HyScanMercatorPrivate *priv = mercator->priv;

  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_mercator_object_constructed (GObject *object)
{
  HyScanMercator *mercator = HYSCAN_MERCATOR (object);
  HyScanMercatorPrivate *priv = mercator->priv;

  G_OBJECT_CLASS (hyscan_mercator_parent_class)->constructed (object);

  priv->max_value = 1.0;
}

static void
hyscan_mercator_object_finalize (GObject *object)
{
  HyScanMercator *mercator = HYSCAN_MERCATOR (object);
  HyScanMercatorPrivate *priv = mercator->priv;

  G_OBJECT_CLASS (hyscan_mercator_parent_class)->finalize (object);
}

/* Установка парметров эллипсоида проекции. */
static void
hyscan_mercator_set_ellipsoid (HyScanMercator          *mercator,
                               HyScanGeoEllipsoidParam  p)
{
  mercator->priv->ellipsoid = p;
}

/**
 * hyscan_mercator_new:
 * @p: параметры земного эллипсоида
 *
 * Создаёт объект проекции Меркатора, который определяет связь географических
 * координат карты с координатами её проекции.
 *
 * Returns: новый объект #HyScanMercator. Для удаления g_object_unref().
 */
HyScanMercator *
hyscan_mercator_new (HyScanGeoEllipsoidParam p)
{
  HyScanMercator *mercator;

  mercator = g_object_new (HYSCAN_TYPE_MERCATOR, NULL);
  hyscan_mercator_set_ellipsoid (mercator, p);

  return mercator;
}

/**
 * hyscan_mercator_get_scale:
 * @mercator
 * @zoom
 * @coords
 *
 * Определяет масштаб проекции в указнной точке (метры на единичный отрезок на проекции).
 *
 * Returns:
 */
gdouble
hyscan_mercator_get_scale (HyScanMercator    *mercator,
                           HyScanGeoGeodetic  coords)
{
  HyScanMercatorPrivate *priv;
  gdouble R; /* Радиус Земли на экваторе. */

  g_return_val_if_fail (HYSCAN_IS_MERCATOR (mercator), -1.0);

  priv = mercator->priv;
  R = priv->ellipsoid.a;

  return (2 * M_PI * R) * cos (M_PI / 180 * coords.lat) / priv->max_value;
}

/**
 * hyscan_mercator_geo_to_value:
 * @param mercator
 * @param coords
 * @param x
 * @param y
 *
 * Переводит географические координаты в координаты на карте.
 */
void
hyscan_mercator_geo_to_value (HyScanMercator    *mercator,
                              HyScanGeoGeodetic  coords,
                              gdouble           *x,
                              gdouble           *y)
{
  HyScanMercatorPrivate *priv;
  gdouble lat_rad;

  g_return_if_fail (HYSCAN_IS_MERCATOR (mercator));
  priv = mercator->priv;

  /* todo: set projection boundary to 0..1, 0..1 */

  lat_rad = coords.lat / 180.0 * M_PI;

  (x != NULL) ? *x = priv->max_value * (coords.lon + 180.0) / 360.0 : 0;
  (y != NULL) ? *y = priv->max_value - priv->max_value * (1 - log (tan (lat_rad) + (1 / cos (lat_rad))) / M_PI) / 2 : 0;
}

/**
 * hyscan_mercator_value_to_geo:
 * @mercator
 * @coords
 * @x
 * @y
 *
 * Переводит координаты на карте в географические координаты.
 */
void
hyscan_mercator_value_to_geo (HyScanMercator    *mercator,
                              HyScanGeoGeodetic *coords,
                              gdouble            x,
                              gdouble            y)
{
  HyScanMercatorPrivate *priv;
  gdouble lat_tmp;

  g_return_if_fail (HYSCAN_IS_MERCATOR (mercator));
  priv = mercator->priv;

  /* todo: set projection boundary to 0..1, 0..1 */

  y = priv->max_value - y;

  coords->lon = x / priv->max_value * 360.0 - 180.0;

  lat_tmp = M_PI - 2.0 * M_PI * y / priv->max_value;
  coords->lat = 180.0 / M_PI * atan (0.5 * (exp (lat_tmp) - exp (-lat_tmp)));
}

/**
 * hyscan_mercator_get_limits:
 * @mercator
 * @min_x
 * @max_x
 * @min_y
 * @max_y
 *
 * Границы проекции.
 */
void
hyscan_mercator_get_limits (HyScanMercator *mercator,
                            gdouble        *min_x,
                            gdouble        *max_x,
                            gdouble        *min_y,
                            gdouble        *max_y)
{
  HyScanMercatorPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MERCATOR (mercator));

  priv = mercator->priv;

  (min_x != NULL) ? *min_x = 0 : 0;
  (min_y != NULL) ? *min_y = 0 : 0;
  (max_x != NULL) ? *max_x = priv->max_value : 0;
  (max_y != NULL) ? *max_y = priv->max_value : 0;
}
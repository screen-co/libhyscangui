/**
 * SECTION: hyscan-mercator
 * @Short_description: Картографическая проекция Меркатора
 * @Title: HyScanMercator
 *
 * Класс реализует интерфейс #HyScanGeoProjection.
 *
 * Равноугольная цилиндрическая проекция Меркатора — одна из основных
 * картографических проекций, позволяющая отобразить поверхность Земли на плоскость.
 * Равноугольная означает, что проекция сохраняет углы между направлениями.
 *
 * Проекция Меркатора сильно искажает полярные регионы, находящиеся дальше параллели 85°.
 *
 * Считается, что Земля имеет форму эллипсоида вращения, парметры эллипсоида
 * можно указать при создании объекта через hyscan_mercator_new().
 *
 */

#include "hyscan-mercator.h"
#include <math.h>

#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)
#define LAT_OUT_OF_RANGE(x) (fabs (x) > 180.0)

enum
{
  PROP_O,
};

struct _HyScanMercatorPrivate
{
  HyScanGeoEllipsoidParam      ellipsoid;  /* Референц эллипсоид. */

  gdouble                      min_x;      /* Минимальная координата по оси OX. */
  gdouble                      max_x;      /* Максимальная координата по оси OX. */
  gdouble                      min_y;      /* Минимальная координата по оси OY. */
  gdouble                      max_y;      /* Максимальная координата по оси OY. */
};

static void          hyscan_mercator_interface_init       (HyScanGeoProjectionInterface *iface);
static void          hyscan_mercator_set_ellipsoid        (HyScanMercator               *mercator,
                                                           HyScanGeoEllipsoidParam       p);
static gdouble       hyscan_mercator_ellipsoid_y          (HyScanMercatorPrivate        *priv,
                                                           gdouble                       lat);
static gdouble       hyscan_mercator_ellipsoid_lat        (HyScanMercatorPrivate        *priv,
                                                           gdouble                       y);

static void          hyscan_mercator_value_to_geo         (HyScanGeoProjection          *mercator,
                                                           HyScanGeoGeodetic            *coords,
                                                           gdouble                       x,
                                                           gdouble                       y);

static void          hyscan_mercator_geo_to_value         (HyScanGeoProjection          *mercator,
                                                           HyScanGeoGeodetic             coords,
                                                           gdouble                      *x,
                                                           gdouble                      *y);

static void          hyscan_mercator_get_limits           (HyScanGeoProjection          *mercator,
                                                           gdouble                      *min_x,
                                                           gdouble                      *max_x,
                                                           gdouble                      *min_y,
                                                           gdouble                      *max_y);

static gdouble       hyscan_mercator_get_scale            (HyScanGeoProjection          *mercator,
                                                           HyScanGeoGeodetic             coords);

G_DEFINE_TYPE_WITH_CODE (HyScanMercator, hyscan_mercator, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanMercator)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GEO_PROJECTION, hyscan_mercator_interface_init))

static void
hyscan_mercator_class_init (HyScanMercatorClass *klass)
{
}

static void
hyscan_mercator_init (HyScanMercator *mercator)
{
  mercator->priv = hyscan_mercator_get_instance_private (mercator);
}

static void
hyscan_mercator_interface_init (HyScanGeoProjectionInterface  *iface)
{
  iface->geo_to_value = hyscan_mercator_geo_to_value;
  iface->value_to_geo = hyscan_mercator_value_to_geo;
  iface->get_limits = hyscan_mercator_get_limits;
  iface->get_scale = hyscan_mercator_get_scale;
}

/* Установка парметров эллипсоида проекции. */
static void
hyscan_mercator_set_ellipsoid (HyScanMercator          *mercator,
                               HyScanGeoEllipsoidParam  p)
{
  HyScanMercatorPrivate *priv = mercator->priv;

  priv->ellipsoid = p;
}

/* Устанавливает границы определения проекции по широте. */
static void
hyscan_mercator_set_limits (HyScanMercator *mercator,
                            gdouble         min_lat,
                            gdouble         max_lat)
{
  HyScanMercatorPrivate *priv = mercator->priv;
  HyScanGeoGeodetic coord;

  coord.lat = min_lat;
  coord.lon = -180.0;
  hyscan_geo_projection_geo_to_value (HYSCAN_GEO_PROJECTION (mercator), coord, &priv->min_x, &priv->min_y);

  coord.lat = max_lat;
  coord.lon = 180.0;
  hyscan_geo_projection_geo_to_value (HYSCAN_GEO_PROJECTION (mercator), coord, &priv->max_x, &priv->max_y);
}

/* Определение координаты Y по широте для проекции произвольного эллипсоида вращения.
 * Источник: https://wiki.openstreetmap.org/wiki/Mercator#C_implementation. */
static gdouble
hyscan_mercator_ellipsoid_y (HyScanMercatorPrivate *priv,
                             gdouble                lat)
{
  gdouble con;
  gdouble ts;
  gdouble e;
  gdouble lat_rad;

  e = priv->ellipsoid.e;
  lat_rad = DEG2RAD (lat);
  con = e * sin (lat_rad);
  con = pow ((1.0 - con) / (1.0 + con), 0.5 * e);
  ts = tan (0.5 * (M_PI * 0.5 - lat_rad)) / con;

  return 0 - priv->ellipsoid.a * log (ts);
}

/* Общий вариант определения широты для эллипсоидального меркатора (в т.ч. EPSG:3395).
 * На основе https://wiki.openstreetmap.org/wiki/Mercator#C_implementation. */
static gdouble
hyscan_mercator_ellipsoid_lat (HyScanMercatorPrivate *priv,
                               gdouble                y)
{
  gdouble ts;
  gdouble lat;
  gdouble d_lat;
  gdouble e;
  gint i;

  e = priv->ellipsoid.e;
  ts = exp (-y / priv->ellipsoid.a);
  lat = M_PI_2 - 2 * atan (ts);
  d_lat = 1.0;

  for (i = 0; fabs (d_lat) > 1e-8 && i < 15; i++)
    {
      gdouble con = e * sin (lat);

      d_lat = M_PI_2 - 2.0 * atan (ts * pow ((1.0 - con) / (1.0 + con), e * 0.5)) - lat;
      lat += d_lat;
    }

  return RAD2DEG (lat);
}

/* Переводит географические координаты @coords в координаты (@x, @y) проекции. */
static void
hyscan_mercator_geo_to_value (HyScanGeoProjection *mercator,
                              HyScanGeoGeodetic    coords,
                              gdouble             *x,
                              gdouble             *y)
{
  HyScanMercatorPrivate *priv = HYSCAN_MERCATOR (mercator)->priv;

  (x != NULL) ? *x = priv->ellipsoid.a * DEG2RAD (coords.lon): 0;
  (y != NULL) ? *y = hyscan_mercator_ellipsoid_y (priv, coords.lat) : 0;
}

/* Переводит координаты на карте (@x, @y) в географические координаты @coords. */
static void
hyscan_mercator_value_to_geo (HyScanGeoProjection *mercator,
                              HyScanGeoGeodetic   *coords,
                              gdouble              x,
                              gdouble              y)
{
  HyScanMercatorPrivate *priv = HYSCAN_MERCATOR (mercator)->priv;

  coords->lon = RAD2DEG (x / priv->ellipsoid.a) ;
  coords->lat = hyscan_mercator_ellipsoid_lat (priv, y);
}

/* Определяет границы проекции. */
static void
hyscan_mercator_get_limits (HyScanGeoProjection *mercator,
                            gdouble             *min_x,
                            gdouble             *max_x,
                            gdouble             *min_y,
                            gdouble             *max_y)
{
  HyScanMercatorPrivate *priv = HYSCAN_MERCATOR (mercator)->priv;

  (min_x != NULL) ? *min_x = priv->min_x : 0;
  (min_y != NULL) ? *min_y = priv->min_y : 0;
  (max_x != NULL) ? *max_x = priv->max_x : 0;
  (max_y != NULL) ? *max_y = priv->max_y : 0;
}

/* Определяет масштаб проекции в указанной точке @coords. */
static gdouble
hyscan_mercator_get_scale (HyScanGeoProjection *mercator,
                           HyScanGeoGeodetic    coords)
{
  (void) mercator;

  return cos (DEG2RAD (coords.lat));
}

/**
 * hyscan_mercator_new:
 * @p: параметры земного эллипсоида
 * @min_lat: минимальная широта в градусах
 * @max_lat: максимальная широта в градусах
 *
 * Создаёт объект картографической проекции Меркатора, которая определяет связь
 * географических координат поверхности Земли с координатами на карте.
 *
 * Границы проекции по широте можно задать парметрами @min_lat и @max_lat.
 *
 * Returns: новый объект #HyScanMercator. Для удаления g_object_unref().
 */
HyScanGeoProjection *
hyscan_mercator_new (HyScanGeoEllipsoidParam p,
                     gdouble                 min_lat,
                     gdouble                 max_lat)
{
  HyScanMercator *mercator;

  g_return_val_if_fail (!LAT_OUT_OF_RANGE (min_lat), NULL);
  g_return_val_if_fail (!LAT_OUT_OF_RANGE (max_lat), NULL);
  g_return_val_if_fail (max_lat > min_lat, NULL);

  mercator = g_object_new (HYSCAN_TYPE_MERCATOR, NULL);
  hyscan_mercator_set_ellipsoid (mercator, p);
  hyscan_mercator_set_limits (mercator, min_lat, max_lat);

  return HYSCAN_GEO_PROJECTION (mercator);
}

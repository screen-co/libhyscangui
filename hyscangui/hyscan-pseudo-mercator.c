#include "hyscan-pseudo-mercator.h"
#include <math.h>

#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)
#define LAT_OUT_OF_RANGE(x) (fabs (x) > 180.0)

enum
{
  PROP_O,
};

struct _HyScanPseudoMercatorPrivate
{
  gdouble                      equator_length;             /* Длина экватора. */
};

static void    hyscan_pseudo_mercator_interface_init     (HyScanGeoProjectionInterface *iface);
static void    hyscan_pseudo_mercator_set_property       (GObject               *object,
                                                          guint                  prop_id,
                                                          const GValue          *value,
                                                          GParamSpec            *pspec);
static void    hyscan_pseudo_mercator_get_property               (GObject               *object,
                                                          guint                  prop_id,
                                                          GValue                *value,
                                                          GParamSpec            *pspec);
static void    hyscan_pseudo_mercator_object_constructed (GObject               *object);
static void    hyscan_pseudo_mercator_object_finalize    (GObject               *object);

static void    hyscan_pseudo_mercator_value_to_geo       (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic            *coords,
                                                          gdouble                       x,
                                                          gdouble                       y);

static void    hyscan_pseudo_mercator_geo_to_value       (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic             coords,
                                                          gdouble                      *x,
                                                          gdouble                      *y);

static void    hyscan_pseudo_mercator_get_limits         (HyScanGeoProjection          *projection,
                                                          gdouble                      *min_x,
                                                          gdouble                      *max_x,
                                                          gdouble                      *min_y,
                                                          gdouble                      *max_y);

static gdouble hyscan_pseudo_mercator_get_scale          (HyScanGeoProjection          *projection,
                                                          HyScanGeoGeodetic             coords);

G_DEFINE_TYPE_WITH_CODE (HyScanPseudoMercator, hyscan_pseudo_mercator, G_TYPE_OBJECT,                                        
                         G_ADD_PRIVATE (HyScanPseudoMercator)                                                         
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GEO_PROJECTION, hyscan_pseudo_mercator_interface_init))

static void
hyscan_pseudo_mercator_class_init (HyScanPseudoMercatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_pseudo_mercator_set_property;
  object_class->get_property = hyscan_pseudo_mercator_get_property;

  object_class->constructed = hyscan_pseudo_mercator_object_constructed;
  object_class->finalize = hyscan_pseudo_mercator_object_finalize;
}

static void
hyscan_pseudo_mercator_init (HyScanPseudoMercator *pseudo_mercator)
{
  pseudo_mercator->priv = hyscan_pseudo_mercator_get_instance_private (pseudo_mercator);
}

static void
hyscan_pseudo_mercator_interface_init (HyScanGeoProjectionInterface *iface)
{
  iface->geo_to_value = hyscan_pseudo_mercator_geo_to_value;
  iface->value_to_geo = hyscan_pseudo_mercator_value_to_geo;
  iface->get_limits = hyscan_pseudo_mercator_get_limits;
  iface->get_scale = hyscan_pseudo_mercator_get_scale;
}

static void
hyscan_pseudo_mercator_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  HyScanPseudoMercator *pseudo_mercator = HYSCAN_PSEUDO_MERCATOR (object);
  HyScanPseudoMercatorPrivate *priv = pseudo_mercator->priv;

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_pseudo_mercator_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  HyScanPseudoMercator *pseudo_mercator = HYSCAN_PSEUDO_MERCATOR (object);
  HyScanPseudoMercatorPrivate *priv = pseudo_mercator->priv;

  switch ( prop_id )
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_pseudo_mercator_object_constructed (GObject *object)
{
  HyScanPseudoMercator *pseudo_mercator = HYSCAN_PSEUDO_MERCATOR (object);
  HyScanPseudoMercatorPrivate *priv = pseudo_mercator->priv;

  G_OBJECT_CLASS (hyscan_pseudo_mercator_parent_class)->constructed (object);

  priv->equator_length = 2 * M_PI * 6378137.0;
}

static void
hyscan_pseudo_mercator_object_finalize (GObject *object)
{
  HyScanPseudoMercator *pseudo_mercator = HYSCAN_PSEUDO_MERCATOR (object);
  HyScanPseudoMercatorPrivate *priv = pseudo_mercator->priv;

  G_OBJECT_CLASS (hyscan_pseudo_mercator_parent_class)->finalize (object);
}

/* Переводит географические координаты @coords в координаты (@x, @y) проекции. */
static void
hyscan_pseudo_mercator_geo_to_value (HyScanGeoProjection *projection,
                                     HyScanGeoGeodetic    coords,
                                     gdouble             *x,
                                     gdouble             *y)
{
  gdouble lat_rad;

  (void) projection;

  lat_rad = DEG2RAD (coords.lat);

  (x != NULL) ? *x = (coords.lon + 180) / 360: 0;
  (y != NULL) ? *y = 1 - (1 - log(tan (lat_rad) + 1 / cos (lat_rad)) / M_PI) / 2 : 0;
}

/* Переводит координаты на карте (@x, @y) в географические координаты @coords. */
static void
hyscan_pseudo_mercator_value_to_geo (HyScanGeoProjection *projection,
                                     HyScanGeoGeodetic   *coords,
                                     gdouble              x,
                                     gdouble              y)
{
  (void) projection;

  /* Меняем направление оси OY. */
  y = 1 - y;

  coords->lon = x * 360.0 - 180.0 ;
  coords->lat = RAD2DEG (atan (sinh (M_PI * (1 - 2 * y))));
}

/* Определяет границы проекции. */
static void
hyscan_pseudo_mercator_get_limits (HyScanGeoProjection *projection,
                                   gdouble             *min_x,
                                   gdouble             *max_x,
                                   gdouble             *min_y,
                                   gdouble             *max_y)
{
  (void) projection;

  (min_x != NULL) ? *min_x = 0 : 0;
  (max_x != NULL) ? *max_x = 1 : 0;
  (min_y != NULL) ? *min_y = 0 : 0;
  (max_y != NULL) ? *max_y = 1 : 0;
}

/* Определяет масштаб проекции в указанной точке @coords. */
static gdouble
hyscan_pseudo_mercator_get_scale (HyScanGeoProjection *projection,
                                  HyScanGeoGeodetic    coords)
{
  HyScanPseudoMercatorPrivate *priv = HYSCAN_PSEUDO_MERCATOR (projection)->priv;

  return cos (M_PI / 180 * coords.lat) * priv->equator_length;
}

HyScanGeoProjection *
hyscan_pseudo_mercator_new (void)
{
  return g_object_new (HYSCAN_TYPE_PSEUDO_MERCATOR, NULL);
}
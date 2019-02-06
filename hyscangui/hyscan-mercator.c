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
 * hyscan_mercator_tile_to_geo:
 * @mercator
 * @zoom
 * @coords
 * @tile_x
 * @tile_y
 *
 * Переводит координаты проекции Меркатора в геокоординаты.
 */
void
hyscan_mercator_tile_to_geo (HyScanMercator    *mercator,
                             guint              zoom,
                             HyScanGeoGeodetic *coords,
                             gdouble            tile_x,
                             gdouble            tile_y)
{
  guint n;
  gdouble lat_tmp;

  n = (guint) ((zoom != 0) ? 2 << (zoom - 1) : 1);

  coords->lon = tile_x / n * 360.0 - 180.0;

  lat_tmp = M_PI - 2.0 * M_PI * tile_y / n;
  coords->lat = 180.0 / M_PI * atan (0.5 * (exp (lat_tmp) - exp (-lat_tmp)));
}

void
hyscan_mercator_geo_to_tile (HyScanMercator    *mercator,
                             guint              zoom,
                             HyScanGeoGeodetic  coords,
                             gdouble           *tile_x,
                             gdouble           *tile_y)
{
  gint total_tiles;
  gdouble lat_rad;

  lat_rad = coords.lat / 180.0 * M_PI;

  total_tiles = (guint) ((zoom != 0) ? 2 << (zoom - 1) : 1);

  (tile_x != NULL) ? *tile_x = total_tiles * (coords.lon + 180.0) / 360.0 : 0;
  (tile_y != NULL) ? *tile_y = total_tiles * (1 - log (tan (lat_rad) + (1 / cos (lat_rad))) / M_PI) / 2 : 0;
}

/**
 * hyscan_mercator_get_scale:
 * @mercator
 * @zoom
 * @coords
 *
 * Определяет ширину одного тайла в метрах в указнной точке.
 *
 * Returns:
 */
gdouble
hyscan_mercator_get_scale (HyScanMercator    *mercator,
                           guint              zoom,
                           HyScanGeoGeodetic  coords)
{
  HyScanMercatorPrivate *priv;
  gdouble R; /* Радиус Земли на экваторе. */

  g_return_val_if_fail (HYSCAN_IS_MERCATOR (mercator), -1.0);

  priv = mercator->priv;
  R = priv->ellipsoid.a;

  return (2 * M_PI * R) * cos (M_PI / 180 * coords.lat) / pow (2, zoom);
}
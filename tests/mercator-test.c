#include <hyscan-mercator.h>
#include <hyscan-pseudo-mercator.h>
#include <math.h>

typedef struct {
  HyScanGeoGeodetic geo;
  gdouble           x;
  gdouble           y;
} TestData;

void test_projection (HyScanGeoProjection *projection,
                      TestData            *data,
                      guint                n_elements,
                      gdouble              eps)
{
  guint i;
  for (i = 0; i < n_elements; ++i)
    {
      HyScanGeoGeodetic coord;
      HyScanGeoCartesian2D c2d;
      gdouble lat_err, lon_err;

      /* Переводим из гео в проекцию. */
      hyscan_geo_projection_geo_to_value (projection, data[i].geo, &c2d);
      g_message ("Projection coordinates: %f, %f", c2d.x, c2d.y);

      g_assert_cmpfloat (fabs (c2d.x - data[i].x) + fabs (c2d.y - data[i].y), <, eps);

      /* Переводим из проекции в гео. */
      hyscan_geo_projection_value_to_geo (projection, &coord, c2d.x, c2d.y);
      g_message ("Geo coordinates: %f, %f", coord.lat, coord.lon);

      lat_err = fabs (coord.lat - data[i].geo.lat);
      lon_err = fabs (coord.lon - data[i].geo.lon);

      g_message ("Error lat: %.2e, lon: %.2e", lat_err, lon_err);

      g_assert_true (lat_err < 1e-6);
      g_assert_true (lon_err < 1e-6);
    }
}

int main (int     argc,
          gchar **argv)
{
  HyScanGeoProjection *projection;
  HyScanGeoEllipsoidParam p;

  TestData data_sphere[] = {
    { {.lat = 52.36, .lon = 4.9}, .x = 545465.50, .y = 6865481.66},
    { {.lat = 55.75, .lon = 37.61}, .x = 4186726.05, .y = 7508807.85},
  };

  TestData data_spheroid[] = {
    { {.lat = 52.36, .lon = 4.9}, .x = 545465.50, .y = 6831623.50},
    { {.lat = 55.75, .lon = 37.61}, .x = 4186726.05, .y = 7473460.43},
  };

  g_message ("EPSG:3857: WGS84 pseudo-Mercator (sphere) [https://epsg.io/3857]");
  hyscan_geo_init_ellipsoid_user (&p, 6378137.0, 0.0);
  projection = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  test_projection (projection, data_sphere, G_N_ELEMENTS (data_sphere), 1e-2);
  g_object_unref (projection);

  g_message ("EPSG:3395: WGS84 Mercator projection (spheroid) [https://epsg.io/3395]");
  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  projection = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  test_projection (projection, data_spheroid, G_N_ELEMENTS (data_spheroid), 1e-2);
  g_object_unref (projection);

  g_message ("EPSG:3395: WGS84 pseudo-Mercator projection (sphere) [https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#Pseudo-code]");
  projection = HYSCAN_GEO_PROJECTION (hyscan_pseudo_mercator_new ());
  /* В HyScanPseudoMercator границы от 0 до 1, поэтому немного меняем сферические данные. */
  {
    guint i;
    gdouble equator_l;

    equator_l = (2 * M_PI * 6378137.0);
    for (i = 0; i < G_N_ELEMENTS (data_sphere); ++i)
      {
        data_sphere[i].x = data_sphere[i].x / equator_l + 0.5;
        data_sphere[i].y = data_sphere[i].y / equator_l + 0.5;
      }
  }
  test_projection (projection, data_sphere, G_N_ELEMENTS (data_sphere), 1e-2 / 6378137.0);
  g_object_unref (projection);

  g_message ("Tests done!");

  return 0;
}

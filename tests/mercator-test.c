#include <hyscan-mercator.h>
#include <hyscan-pseudo-mercator.h>
#include <math.h>

#define RADIUS_EARTH 6378137.0

typedef struct {
  HyScanGeoGeodetic geo;
  gdouble           x;
  gdouble           y;
} TestData;

void
test_hash (void)
{
  HyScanGeoEllipsoidParam p;
  HyScanGeoProjection *proj_wgs84_1, *proj_wgs84_2, *proj_sphere_1, *proj_sphere_2;
  HyScanGeoProjection *proj_pseudo_1, *proj_pseudo_2;

  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  proj_wgs84_1 = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  proj_wgs84_2 = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));

  hyscan_geo_init_ellipsoid_user (&p, RADIUS_EARTH, 0.0);
  proj_sphere_1 = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  proj_sphere_2 = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));

  proj_pseudo_1 = hyscan_pseudo_mercator_new ();
  proj_pseudo_2 = hyscan_pseudo_mercator_new ();

  g_assert_cmpint (hyscan_geo_projection_hash (proj_wgs84_1), ==, hyscan_geo_projection_hash (proj_wgs84_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_sphere_1), ==, hyscan_geo_projection_hash (proj_sphere_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_pseudo_1), ==, hyscan_geo_projection_hash (proj_pseudo_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_wgs84_1), !=, hyscan_geo_projection_hash (proj_sphere_2));
  g_assert_cmpint (hyscan_geo_projection_hash (proj_pseudo_1), !=, hyscan_geo_projection_hash (proj_sphere_2));
}

void
test_projection (HyScanGeoProjection *projection,
                 TestData            *data,
                 guint                n_elements,
                 gdouble              eps)
{
  guint i;

  /* Проверяем перевод точек из гео-СК в декартову и обратно. */
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

  /* Проверяем границы проекции. */
  {
    gdouble min_x, max_x, min_y, max_y;

    hyscan_geo_projection_get_limits (projection, &min_x, &max_x, &min_y, &max_y);
    g_assert_cmpfloat (min_x, <, max_x);
    g_assert_cmpfloat (min_y, <, max_y);
    g_assert_cmpfloat (max_x - min_x, ==, max_y - min_y);
  }
}

void
test_pseudo_mercator_scale (HyScanGeoProjection *projection)
{
  HyScanGeoGeodetic coords;
  gdouble scale0, scale20, scale40, scale40_1;

  coords.lon = 80;
  coords.lat = 0;
  scale0 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lat = 20;
  scale20 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lat = 40;
  scale40 = hyscan_geo_projection_get_scale (projection, coords);
  coords.lon = 90;
  scale40_1 = hyscan_geo_projection_get_scale (projection, coords);

  /* Масштаб уменьшается при приближении к полюсам. */
  g_assert_cmpfloat (scale0, >, scale20);
  g_assert_cmpfloat (scale20, >, scale40);
  /* При этом на одной параллели масштаб постоянный. */
  g_assert_cmpfloat (scale40_1, ==, scale40);
}

void
test_mercator_scale (HyScanGeoProjection *projection)
{
  HyScanGeoGeodetic coords;
  gdouble scale0;

  coords.lon = 80;
  coords.lat = 0;
  scale0 = hyscan_geo_projection_get_scale (projection, coords);

  /* Масштаб равен 1 на экваторе... */
  g_assert_cmpfloat (1.0, ==, scale0);

  /* В остальном ведёт себя так же, как псевдомеркатор. */
  test_pseudo_mercator_scale (projection);
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
  hyscan_geo_init_ellipsoid_user (&p, RADIUS_EARTH, 0.0);
  projection = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  test_projection (projection, data_sphere, G_N_ELEMENTS (data_sphere), 1e-2);
  test_mercator_scale (projection);
  g_object_unref (projection);

  g_message ("EPSG:3395: WGS84 Mercator projection (spheroid) [https://epsg.io/3395]");
  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  projection = HYSCAN_GEO_PROJECTION (hyscan_mercator_new (p));
  test_projection (projection, data_spheroid, G_N_ELEMENTS (data_spheroid), 1e-2);
  test_mercator_scale (projection);
  g_object_unref (projection);

  g_message ("EPSG:3395: WGS84 pseudo-Mercator projection (sphere) [https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames#Pseudo-code]");
  projection = HYSCAN_GEO_PROJECTION (hyscan_pseudo_mercator_new ());
  /* В HyScanPseudoMercator границы от 0 до 1, поэтому немного меняем сферические данные. */
  {
    guint i;
    gdouble equator_l;

    equator_l = (2 * M_PI * RADIUS_EARTH);
    for (i = 0; i < G_N_ELEMENTS (data_sphere); ++i)
      {
        data_sphere[i].x = data_sphere[i].x / equator_l + 0.5;
        data_sphere[i].y = data_sphere[i].y / equator_l + 0.5;
      }
  }
  test_projection (projection, data_sphere, G_N_ELEMENTS (data_sphere), 1e-2 / RADIUS_EARTH);
  test_pseudo_mercator_scale (projection);
  g_object_unref (projection);

  test_hash ();

  g_message ("Tests done!");

  return 0;
}

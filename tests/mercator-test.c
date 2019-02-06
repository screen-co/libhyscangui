#include <hyscan-mercator.h>
#include <math.h>

int main (int argc,
          gchar **argv)
{
  HyScanMercator *mercator;
  HyScanGeoEllipsoidParam p;
  HyScanGeoGeodetic coords[] = {
          {.lat = 52.36, .lon = 4.9},
          {.lat = 55.75, .lon = 37.61}
  };
  gdouble x, y;
  guint i;

  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  mercator = hyscan_mercator_new (p);

  for (i = 0; i < G_N_ELEMENTS (coords); ++i)
    {
      HyScanGeoGeodetic translated;
      gdouble lat_err, lon_err;

      hyscan_mercator_geo_to_value (mercator, coords[i], &x, &y);
      g_message ("Projection coordinates: %f, %f", x, y);

      hyscan_mercator_value_to_geo (mercator, &translated, x, y);
      g_message ("Geo coordinates: %f, %f", translated.lat, translated.lon);

      lat_err = fabs (translated.lat - coords[i].lat);
      lon_err = fabs (translated.lon - coords[i].lon);

      g_message ("Error lat: %.2e, lon: %.2e", lat_err, lon_err);

      g_assert_true (lat_err < 1e-6);
      g_assert_true (lon_err < 1e-6);
    }

  g_message ("Tests done!");

  return 0;
}
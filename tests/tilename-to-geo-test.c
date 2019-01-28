#include <hyscan-gtk-map.h>
#include <math.h>

int main (int argc,
          gchar **argv)
{
  HyScanGeoGeodetic coords[] = {
          {.lat = 52.36, .lon = 4.9},
          {.lat = 55.75, .lon = 37.61}
  };
  gdouble x, y;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (coords); ++i)
    {
      HyScanGeoGeodetic translated;
      gdouble lat_err, lon_err;

      hyscan_gtk_map_geo_to_tile (7, coords[i], &x, &y);
      g_message ("Tile name: %f, %f", x, y);

      hyscan_gtk_map_tile_to_geo (7, &translated, x, y);
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
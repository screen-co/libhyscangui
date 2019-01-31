#include <gtk/gtk.h>
#include <hyscan-gtk-map.h>
#include <hyscan-gtk-map-tiles.h>
#include <math.h>
#include <hyscan-gtk-map-float.h>
#include <hyscan-gtk-map-tiles-osm.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-crts-map.h>

void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
}

/* Фабрика карт. */
GtkWidget *
create_map (HyScanGeoGeodetic center)
{
  GtkWidget *map;

  if (FALSE)
    {
      HyScanGeo *geo;
      geo = hyscan_geo_new (center, HYSCAN_GEO_ELLIPSOID_WGS84);
      map = hyscan_gtk_crts_map_new (geo);

      g_object_unref (geo);
    }
  else
    {
      map = hyscan_gtk_map_new ();
    }

  return map;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;
  GtkWidget *map;
  HyScanGtkMapTilesOsm *osm_source;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));

  HyScanGtkMapTiles *tiles;
  HyScanGtkMapFloat *float_layer;

  HyScanGeoGeodetic center = {.lat = 52.36, .lon = 4.9};
  guint zoom = 12;
//  HyScanGeoGeodetic center = {.lat = 81.64, .lon = 63.21};
//  guint zoom = 10;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 1024, 1024);

  /* Создаём область карты. */
  map = create_map (center);

  /* Добавляем слои. */
  osm_source = hyscan_gtk_map_tiles_osm_new ();
  tiles = hyscan_gtk_map_tiles_new (HYSCAN_GTK_MAP (map),
                                    cache,
                                    HYSCAN_GTK_MAP_TILE_SOURCE (osm_source));

  float_layer = hyscan_gtk_map_float_new (HYSCAN_GTK_MAP (map));

  gtk_container_add (GTK_CONTAINER (window), map);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

  gtk_widget_show_all (window);

  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP(map), center);
  hyscan_gtk_map_set_zoom (HYSCAN_GTK_MAP(map), zoom);

  gtk_main ();

  g_clear_object (&float_layer);
  g_clear_object (&tiles);
  g_clear_object (&cache);

  return 0;
}
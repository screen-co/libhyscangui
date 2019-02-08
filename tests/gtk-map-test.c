#include <gtk/gtk.h>
#include <hyscan-gtk-map.h>
#include <hyscan-gtk-map-tiles.h>
#include <math.h>
#include <hyscan-gtk-map-float.h>
#include <hyscan-network-map-tile-source.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-map-fs-tile-source.h>
#include <hyscan-pseudo-mercator.h>
#include <hyscan-mercator.h>

static gchar    *tiles_dir = "/tmp/tiles";   /* Путь к каталогу, где хранятся тайлы. */

void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
}

/* Создаёт яндекс карту и источник тайлов к ней. */
GtkWidget *
create_map_yandex (HyScanGtkMapTileSource **source)
{
  GtkWidget *map;
  HyScanGeoProjection *projection;
  HyScanGeoEllipsoidParam p;
  HyScanNetworkMapTileSource *nw_source;

  /* Проекция Яндекса - эллипсоид WGS84 с указанными границами по широте. */
  hyscan_geo_init_ellipsoid (&p, HYSCAN_GEO_ELLIPSOID_WGS84);
  projection = hyscan_mercator_new (p, -85.08405905010976, 85.08405905010976);
  map = hyscan_gtk_map_new (projection);
  g_object_unref (projection);

  nw_source = hyscan_network_map_tile_source_new ("http://vec02.maps.yandex.net/tiles?l=map&v=2.2.3&z=%d&x=%d&y=%d");
  *source = HYSCAN_GTK_MAP_TILE_SOURCE (nw_source);

  return map;
}

/* Создаёт OSM карту. */
GtkWidget *
create_map (HyScanGtkMapTileSource **source)
{
  GtkWidget *map;
  HyScanGeoProjection *projection;
  HyScanNetworkMapTileSource *nw_source;
  // const gchar *url_format = "https://tile.thunderforest.com/landscape/%d/%d/%d.png?apikey=03fb8295553d4a2eaacc64d7dd88e3b9";
  const gchar *url_format[] = {
    "https://tile.thunderforest.com/landscape/%d/%d/%d.png?apikey=03fb8295553d4a2eaacc64d7dd88e3b9",
    "http://a.tile.openstreetmap.org/%d/%d/%d.png",
    "https://maps.wikimedia.org/osm-intl/%d/%d/%d.png",
    "http://www.google.cn/maps/vt?lyrs=s@189&gl=cn&x=%2$d&y=%3$d&z=%1$d" /* todo: jpeg пока не реализован. */
  };

  projection = hyscan_pseudo_mercator_new ();
  
  map = hyscan_gtk_map_new (projection);
  g_object_unref (projection);

  nw_source = hyscan_network_map_tile_source_new (url_format[2]);

  *source = HYSCAN_GTK_MAP_TILE_SOURCE (nw_source);

  return map;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;
  GtkWidget *map;
  HyScanGtkMapTileSource *nw_source;
  HyScanGtkMapFsTileSource *fs_source;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (64));

  HyScanGtkMapTiles *tiles;
  HyScanGtkMapFloat *float_layer;

  HyScanGeoGeodetic center = {.lat = 52.36, .lon = 4.9};
  guint zoom = 12;
//  HyScanGeoGeodetic center = {.lat = 81.64, .lon = 63.21};
//  guint zoom = 10;

  gtk_init (&argc, &argv);

/* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "tile-dir", 'd', 0, G_OPTION_ARG_STRING, &tiles_dir, "Path to directory containing tiles", NULL },
        { NULL }
      };

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);

    if (!g_option_context_parse (context, &argc, &argv, &error))
      {
        g_message( error->message);
        return -1;
      }

    g_option_context_free (context);
  }

  /* Создаём область карты. */
  map = create_map(&nw_source);

  fs_source = hyscan_gtk_map_fs_tile_source_new (tiles_dir, HYSCAN_GTK_MAP_TILE_SOURCE(nw_source));

  /* Добавляем слои. */
  tiles = hyscan_gtk_map_tiles_new (HYSCAN_GTK_MAP (map),
                                    cache,
                                    HYSCAN_GTK_MAP_TILE_SOURCE (fs_source));

  float_layer = hyscan_gtk_map_float_new (HYSCAN_GTK_MAP (map));

  /* Добавляем виджет карты в окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  gtk_container_add (GTK_CONTAINER (window), map);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);
  gtk_widget_show_all (window);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), 0, 10, 0, 10);
  hyscan_gtk_map_tiles_set_zoom (tiles, zoom);
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (map), center);

  /* Main loop. */
  gtk_main ();

  /* Освобождаем память. */
  g_clear_object (&fs_source);
  g_clear_object (&nw_source);
  g_clear_object (&float_layer);
  g_clear_object (&tiles);
  g_clear_object (&cache);

  return 0;
}
#include <gtk/gtk.h>
#include <hyscan-gtk-map.h>
#include <hyscan-gtk-map-tiles.h>
#include <math.h>
#include <hyscan-gtk-map-float.h>
#include <hyscan-gtk-map-tiles-osm.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-crts-map.h>
#include <hyscan-gtk-map-fs-tile-source.h>

static gchar    *tiles_dir = "/tmp/tiles";   /* Путь к каталогу, где хранятся тайлы. */

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
  HyScanGtkMapFsTileSource *fs_source;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (512));

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
  map = create_map (center);

  /* Источники тайлов. */
  osm_source = hyscan_gtk_map_tiles_osm_new ();
  fs_source = hyscan_gtk_map_fs_tile_source_new (tiles_dir, HYSCAN_GTK_MAP_TILE_SOURCE(osm_source));

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
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (map), center);
  hyscan_gtk_map_set_zoom (HYSCAN_GTK_MAP (map), zoom);

  /* Main loop. */
  gtk_main ();

  /* Освобождаем память. */
  g_clear_object (&fs_source);
  g_clear_object (&osm_source);
  g_clear_object (&float_layer);
  g_clear_object (&tiles);
  g_clear_object (&cache);

  return 0;
}
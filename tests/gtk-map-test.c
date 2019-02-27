#include <gtk/gtk.h>
#include <hyscan-gtk-map.h>
#include <hyscan-gtk-map-tiles.h>
#include <math.h>
#include <hyscan-network-map-tile-source.h>
#include <hyscan-cached.h>
#include <hyscan-gtk-map-fs-tile-source.h>
#include <hyscan-pseudo-mercator.h>
#include <hyscan-mercator.h>
#include <hyscan-gtk-map-control.h>
#include <hyscan-gtk-map-ruler.h>
#include <hyscan-gtk-map-grid.h>
#include <hyscan-gtk-map-pin-layer.h>

static gchar *tiles_dir;                     /* Путь к каталогу, где хранятся тайлы. */
static gboolean yandex_projection = FALSE;   /* Использовать карту яндекса. */
static guint tile_url_preset = 0;
static gchar *tile_url_format;

static HyScanGtkMap *map;
static GtkContainer *layer_toolbox;
void   (*layer_toolbox_cb) (GtkContainer   *container,
                            HyScanGtkLayer *layer);

/* Пресеты URL серверов. */
const gchar *url_presets[] = {
  "https://tile.thunderforest.com/landscape/%d/%d/%d.png?apikey=03fb8295553d4a2eaacc64d7dd88e3b9",
  "http://a.tile.openstreetmap.org/{z}/{x}/{y}.png",                                       /* -p 1: OSM. */
  "https://maps.wikimedia.org/osm-intl/{z}/{x}/{y}.png",                                   /* -p 2: Wikimedia. */
  "http://c.tile.stamen.com/watercolor/{z}/{x}/{y}.jpg",                                   /* -p 3: Watercolor. */
  "http://www.google.cn/maps/vt?lyrs=s@189&gl=cn&x={x}&y={y}&z={z}",                       /* -p 4: Google спутник. */
  "http://ecn.t0.tiles.virtualearth.net/tiles/a{quadkey}.jpeg?g=6897"                      /* -p 5: Bing спутник. */
};

const gchar *url_presets_yandex[] = {
  "http://vec02.maps.yandex.net/tiles?l=map&v=2.2.3&z={z}&x={x}&y={y}",                    /* -p 0: Yandex вектор. */
  "https://sat02.maps.yandex.net/tiles?l=sat&v=3.455.0&x={x}&y={y}&z={z}&lang=ru_RU"       /* -p 1: Yandex спутник. */
};

static HyScanGeoGeodetic center = {.lat = 52.36, .lon = 4.9};

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

  nw_source = hyscan_network_map_tile_source_new (tile_url_format ? tile_url_format : url_presets_yandex[tile_url_preset]);
  *source = HYSCAN_GTK_MAP_TILE_SOURCE (nw_source);

  return map;
}

/* Создаёт OSM карту. */
GtkWidget *
create_map_user (HyScanGtkMapTileSource **source)
{
  GtkWidget *map;
  HyScanGeoProjection *projection;
  HyScanNetworkMapTileSource *nw_source;

  projection = hyscan_pseudo_mercator_new ();
  
  map = hyscan_gtk_map_new (projection);
  g_object_unref (projection);

  nw_source = hyscan_network_map_tile_source_new (tile_url_format ? tile_url_format : url_presets[tile_url_preset]);

  *source = HYSCAN_GTK_MAP_TILE_SOURCE (nw_source);

  return map;
}

GtkWidget *
create_map (HyScanGtkMapTileSource **source)
{
  if (yandex_projection)
    return create_map_yandex (source);
  else
    return create_map_user (source);
}

void
on_editable_switch (GtkSwitch               *widget,
                    GParamSpec              *pspec,
                    HyScanGtkLayerContainer *container)
{
  hyscan_gtk_layer_container_set_changes_allowed (container, gtk_switch_get_active (widget));
}

void
on_coordinate_change (GtkSwitch  *widget,
                      GParamSpec *pspec,
                      gdouble    *value)
{
  *value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
}

void
on_move_to_click (HyScanGtkMap *map)
{
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (map), center);
}

/* Переключение активного слоя. */
void
on_row_activate (GtkListBox    *box,
                 GtkListBoxRow *row,
                 gpointer       user_data)
{
  HyScanGtkLayer *layer;

  gtk_container_foreach (layer_toolbox, (GtkCallback) gtk_widget_destroy, NULL);

  layer = g_object_get_data (G_OBJECT (row), "layer");
  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (map), layer);

  layer_toolbox_cb = g_object_get_data (G_OBJECT (layer), "toolbox-cb");
  if (layer_toolbox_cb != NULL)
    {
      layer_toolbox_cb (layer_toolbox, layer);
      gtk_widget_show_all (GTK_WIDGET (layer_toolbox));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (layer_toolbox));
    }
}

void
on_change_layer_visibility (GtkToggleButton *widget,
                            GParamSpec      *pspec,
                            HyScanGtkLayer  *layer)
{
  gboolean visible;

  visible = gtk_toggle_button_get_active (widget);
  hyscan_gtk_layer_set_visible (layer, visible);
}

gboolean
on_motion_show_coords (HyScanGtkMap   *map,
                       GdkEventMotion *event,
                       GtkLabel       *label)
{
  HyScanGeoGeodetic geo;
  gchar text[255];

  hyscan_gtk_map_point_to_geo (map, &geo, event->x, event->y);
  g_snprintf (text, sizeof (text), "%.5f°, %.5f°", geo.lat, geo.lon);
  gtk_label_set_text (label, text);

  return FALSE;
}

void
add_layer_row (GtkListBox     *list_box,
               const gchar    *title,
               HyScanGtkLayer *layer)
{
  GtkWidget *box;
  GtkWidget *vsbl_chkbx;
  GtkWidget *row;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  vsbl_chkbx = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vsbl_chkbx), hyscan_gtk_layer_get_visible (layer));
  gtk_box_pack_start (GTK_BOX (box), vsbl_chkbx, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), gtk_label_new (title), TRUE, TRUE, 0);

  row = gtk_list_box_row_new ();
  g_object_set_data (G_OBJECT (row), "layer", layer);
  gtk_container_add (GTK_CONTAINER (row), box);

  gtk_list_box_insert (list_box, row, 0);

  g_signal_connect (vsbl_chkbx, "notify::active", G_CALLBACK (on_change_layer_visibility), layer);
}

/* Создаёт панель инструментов для слоя булавок и линейки. */
void
create_ruler_toolbox (GtkContainer   *container,
                      HyScanGtkLayer *layer)
{
  GtkWidget *ctrl_widget;

  ctrl_widget = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (ctrl_widget), "Очистить");
  g_signal_connect_swapped (ctrl_widget, "clicked", G_CALLBACK (hyscan_gtk_map_pin_layer_clear), layer);
  gtk_container_add (container, ctrl_widget);
}

/* Кнопки управления виджетом. */
GtkWidget *
create_control_box (HyScanGtkMap         *map,
                    HyScanGtkMapRuler    *ruler,
                    HyScanGtkMapPinLayer *pin_layer,
                    HyScanGtkMapGrid     *grid)
{
  GtkWidget *ctrl_box;
  GtkWidget *ctrl_widget;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Блокировка редактирования. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Редактирование"));

    ctrl_widget = gtk_switch_new ();
    gtk_switch_set_active (GTK_SWITCH (ctrl_widget),
                           hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (map)));
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
    g_signal_connect (ctrl_widget, "notify::active", G_CALLBACK (on_editable_switch), map);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Переключение слоёв. */
  {
    GtkWidget *list_box;

    list_box = gtk_list_box_new ();

    add_layer_row (GTK_LIST_BOX (list_box), "Коорд. сетка", HYSCAN_GTK_LAYER (grid));
    add_layer_row (GTK_LIST_BOX (list_box), "Линейка", HYSCAN_GTK_LAYER (ruler));
    add_layer_row (GTK_LIST_BOX (list_box), "Булавка", HYSCAN_GTK_LAYER (pin_layer));
    g_signal_connect (list_box, "row-activated", G_CALLBACK (on_row_activate), NULL);

    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Слои"));
    gtk_container_add (GTK_CONTAINER (ctrl_box), list_box);
  }

  /* Контейнер для панели инструментов каждого слоя. */
  {
    layer_toolbox = GTK_CONTAINER (gtk_box_new (GTK_ORIENTATION_VERTICAL, 10));
    gtk_container_add (GTK_CONTAINER (ctrl_box), GTK_WIDGET (layer_toolbox));

    /* Устаналиваем коллбэки для создания панели инструментов слоя. */
    g_object_set_data (G_OBJECT (ruler), "toolbox-cb", create_ruler_toolbox);
    g_object_set_data (G_OBJECT (pin_layer), "toolbox-cb", create_ruler_toolbox);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Текстовые поля для ввода координат. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Навигация (ш, д)"));

    ctrl_widget = gtk_spin_button_new_with_range (-90.0, 90.0, 0.001);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (ctrl_widget), center.lat);
    g_signal_connect (ctrl_widget, "notify::value", G_CALLBACK (on_coordinate_change), &center.lat);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);

    ctrl_widget = gtk_spin_button_new_with_range (-180.0, 180.0, 0.001);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (ctrl_widget), center.lon);
    g_signal_connect (ctrl_widget, "notify::value", G_CALLBACK (on_coordinate_change), &center.lon);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);

    ctrl_widget = gtk_button_new ();
    gtk_button_set_label (GTK_BUTTON (ctrl_widget), "Перейти");
    g_signal_connect_swapped (ctrl_widget, "clicked", G_CALLBACK (on_move_to_click), map);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Текущие координаты. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Координаты"));
    ctrl_widget = gtk_label_new ("-, -");
    g_object_set (ctrl_widget, "width-chars", 24, NULL);
    g_signal_connect (map, "motion-notify-event", G_CALLBACK (on_motion_show_coords), ctrl_widget);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
  }

  return ctrl_box;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;
  GtkWidget *grid;

  HyScanGtkMapControl *control;
  HyScanGtkMapRuler *ruler;
  HyScanGtkMapPinLayer *pin_layer;
  HyScanGtkMapGrid *map_grid;
  HyScanCache *cache = HYSCAN_CACHE (hyscan_cached_new (64));

  HyScanGtkMapTiles *tiles;

  guint zoom = 12;

  gtk_init (&argc, &argv);

  /* Разбор командной строки. */
  {
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "tile-dir",        'd', 0, G_OPTION_ARG_STRING, &tiles_dir,         "Path to dir containing tiles", NULL },
        { "yandex",          'y', 0, G_OPTION_ARG_NONE,   &yandex_projection, "Use yandex projection", NULL },
        { "tile-url-preset", 'p', 0, G_OPTION_ARG_INT,    &tile_url_preset,   "Use one of preset tile source", NULL },
        { "tile-url-format", 'u', 0, G_OPTION_ARG_NONE,   &tile_url_format,   "User defined tile source", NULL },
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

    if (tiles_dir == NULL)
      tiles_dir = g_dir_make_tmp ("tilesXXXXXX", NULL);
    else
      tiles_dir = g_strdup (tiles_dir);

    g_option_context_free (context);
  }

  /* Добавляем слои. */
  {
    HyScanGtkMapTileSource *nw_source;
    HyScanGtkMapFsTileSource *fs_source;
    GdkRGBA color;

    /* Создаём виджет карты и соответствующий network-источник тайлов. */
    map = HYSCAN_GTK_MAP (create_map (&nw_source));

    fs_source = hyscan_gtk_map_fs_tile_source_new (tiles_dir, HYSCAN_GTK_MAP_TILE_SOURCE(nw_source));
    tiles = hyscan_gtk_map_tiles_new (cache, HYSCAN_GTK_MAP_TILE_SOURCE (fs_source));
    g_clear_object (&nw_source);
    g_clear_object (&fs_source);

    control = hyscan_gtk_map_control_new ();

    map_grid = hyscan_gtk_map_grid_new ();
    gdk_rgba_parse (&color, "rgba(255, 255, 255, 0.5)");
    g_object_set (G_OBJECT (map_grid),
                  "line-color", &color,
                  "line-width", 0.5,
                  NULL);

    ruler = hyscan_gtk_map_ruler_new ();
    gdk_rgba_parse (&color, "#dd5555");
    g_object_set (G_OBJECT (ruler),
                  "shape", HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE,
                  "radius", 4,
                  "color", &color,
                  "line-width", 1.5,
                  NULL);

    pin_layer = hyscan_gtk_map_pin_layer_new ();
    gdk_rgba_parse (&color, "#dd55dd");
    g_object_set (G_OBJECT (pin_layer),
                  "shape", HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN,
                  "radius", 10,
                  "color", &color,
                  NULL);

    /* Слой управления первый, чтобы обрабатывать все взаимодействия. */
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (control));
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (tiles));
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (map_grid));
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (pin_layer));
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (ruler));
  }

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 20);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
  gtk_widget_set_margin_start (grid, 20);
  gtk_widget_set_margin_end (grid, 20);
  gtk_widget_set_margin_top (grid, 20);
  gtk_widget_set_margin_bottom (grid, 20);

  gtk_widget_set_hexpand (GTK_WIDGET(map), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET(map), TRUE);

  /* Добавляем виджет карты в окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);

  gtk_grid_attach (GTK_GRID (grid), GTK_WIDGET (map), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), create_control_box (map, ruler, pin_layer, map_grid), 1, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

  gtk_widget_show_all (window);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), 0, 10, 0, 10);
  hyscan_gtk_map_tiles_set_zoom (tiles, zoom);
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (map), center);

  /* Main loop. */
  gtk_main ();

  /* Освобождаем память. */
  g_clear_object (&map_grid);
  g_clear_object (&ruler);
  g_clear_object (&control);
  g_clear_object (&pin_layer);
  g_clear_object (&tiles);
  g_clear_object (&cache);
  g_free (tiles_dir);

  return 0;
}

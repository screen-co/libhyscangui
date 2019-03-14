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
#include <hyscan-map-profile.h>
#include <hyscan-gtk-map-track-layer.h>
#include <hyscan-nmea-file-device.h>

static gchar *tiles_dir;                     /* Путь к каталогу, где хранятся тайлы. */
static gboolean yandex_projection = FALSE;   /* Использовать карту яндекса. */
static guint tile_url_preset = 0;
static gchar *tile_url_format;

static HyScanGtkMap *map;
static HyScanMapProfile *profiles[8];
static GtkContainer *layer_toolbox;
void   (*layer_toolbox_cb) (GtkContainer   *container,
                            HyScanGtkLayer *layer);

static HyScanGeoGeodetic center = {.lat = 55.571, .lon = 38.103};

void
destroy_callback (GtkWidget *widget,
                  gpointer   user_data)
{
  gtk_main_quit ();
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
on_row_select (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  HyScanGtkLayer *layer;

  gtk_container_foreach (layer_toolbox, (GtkCallback) gtk_widget_destroy, NULL);

  if (row == NULL)
    {
      hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (map), NULL);
      return;
    }

  layer = g_object_get_data (G_OBJECT (row), "layer");
  if (!hyscan_gtk_layer_grab_input (layer))
    {
      gtk_list_box_unselect_row (box, row);
      return;
    }

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
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
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

void
on_profile_change (GtkComboBoxText *widget,
                   gpointer         user_data)
{
  gint index;

  index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  if (index < 0)
    return;

  hyscan_map_profile_apply (profiles[index], map);
}

/* Кнопки управления виджетом. */
GtkWidget *
create_control_box (HyScanGtkMap          *map,
                    HyScanGtkMapRuler     *ruler,
                    HyScanGtkMapPinLayer  *pin_layer,
                    HyScanGtkMapTrackLayer *path_layer,
                    HyScanGtkMapGrid      *grid)
{
  GtkWidget *ctrl_box;
  GtkWidget *ctrl_widget;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Выбор профиля. */
  {
    ctrl_widget = gtk_combo_box_text_new ();

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Yandex Vector");
    profiles[0] = hyscan_map_profile_new ("http://vec02.maps.yandex.net/tiles?l=map&v=2.2.3&z={z}&x={x}&y={y}",
                                         "/tmp/tiles/yandex",
                                         "merc", 0, 19);


    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "OSM");
    profiles[1] = hyscan_map_profile_new ("http://a.tile.openstreetmap.org/{z}/{x}/{y}.png",
                                          "/tmp/tiles/osm",
                                          "webmerc", 0, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Wikipedia");
    profiles[2] = hyscan_map_profile_new ("https://maps.wikimedia.org/osm-intl/{z}/{x}/{y}.png",
                                          "/tmp/tiles/wiki",
                                          "webmerc", 0, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Yandex Satellite");
    profiles[3] = hyscan_map_profile_new ("https://sat02.maps.yandex.net/tiles?l=sat&v=3.455.0&x={x}&y={y}&z={z}&lang=ru_RU",
                                          "/tmp/tiles/yandex_sat",
                                          "merc", 0, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Bing Satellite");
    profiles[4] = hyscan_map_profile_new ("http://ecn.t0.tiles.virtualearth.net/tiles/a{quadkey}.jpeg?g=6897",
                                          "/tmp/tiles/bing",
                                          "webmerc", 1, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Google Satellite");
    profiles[5] = hyscan_map_profile_new ("http://www.google.cn/maps/vt?lyrs=s@189&gl=cn&x={x}&y={y}&z={z}",
                                          "/tmp/tiles/google-sat",
                                          "webmerc", 1, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Thunderforest Landscape");
    profiles[6] = hyscan_map_profile_new ("https://tile.thunderforest.com/landscape/{z}/{x}/{y}.png?apikey=03fb8295553d4a2eaacc64d7dd88e3b9",
                                          "/tmp/tiles/thunder_landscape",
                                          "webmerc", 0, 19);

    gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (ctrl_widget), "Watercolor");
    profiles[7] = hyscan_map_profile_new ("http://c.tile.stamen.com/watercolor/{z}/{x}/{y}.jpg",
                                          "/tmp/tiles/watercolor",
                                          "webmerc", 0, 19);


    g_signal_connect (ctrl_widget, "changed", G_CALLBACK (on_profile_change), NULL);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);

    gtk_combo_box_set_active (GTK_COMBO_BOX (ctrl_widget), 0);
  }

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
    add_layer_row (GTK_LIST_BOX (list_box), "Трек", HYSCAN_GTK_LAYER (path_layer));
    g_signal_connect (list_box, "row-selected", G_CALLBACK (on_row_select), NULL);

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

/* Слой с треком движения. */
HyScanGtkMapTrackLayer *
create_path_layer ()
{
  HyScanNavigationModel *model;
  HyScanNmeaFileDevice *device;
  HyScanGtkMapTrackLayer *layer;
  HyScanCached *cache;

  device = hyscan_nmea_file_device_new ("my-device", "/home/alex/move.nmea");
  model = hyscan_navigation_model_new (HYSCAN_SENSOR (device));
  hyscan_sensor_set_enable (HYSCAN_SENSOR (device), "my-device", TRUE);

  cache = hyscan_cached_new (100);
  layer = hyscan_gtk_map_track_layer_new (model, HYSCAN_CACHE (cache));

  g_object_unref (device);
  g_object_unref (model);
  g_object_unref (cache);

  return layer;
}

int main (int     argc,
          gchar **argv)
{
  GtkWidget *window;
  GtkWidget *grid;

  HyScanGtkMapControl *control;
  HyScanGtkMapRuler *ruler;
  HyScanGtkMapPinLayer *pin_layer;
  HyScanGtkMapTrackLayer *path_layer;
  HyScanGtkMapGrid *map_grid;

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
    map = HYSCAN_GTK_MAP (hyscan_gtk_map_new (NULL));

    control = hyscan_gtk_map_control_new ();
    map_grid = hyscan_gtk_map_grid_new ();
    ruler = hyscan_gtk_map_ruler_new ();
    pin_layer = hyscan_gtk_map_pin_layer_new ();
    path_layer = create_path_layer ();

    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (control),    "control");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (pin_layer),  "pin");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (ruler),      "ruler");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (path_layer), "path");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (map_grid),   "grid");
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
  gtk_grid_attach (GTK_GRID (grid), create_control_box (map, ruler, pin_layer, path_layer, map_grid), 1, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_callback), NULL);

  gtk_widget_show_all (window);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (map), 0, 10, 0, 10);

  /* Устанавливаем допустимые масштабы. */
  {
    gdouble *scales;
    gint scales_len;

    scales = hyscan_gtk_map_create_scales2 (1.0 / 10, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
    hyscan_gtk_map_set_scales (map, scales, scales_len);
    g_free (scales);
  }

  hyscan_gtk_map_move_to (map, center);

  /* Main loop. */
  gtk_main ();

  /* Освобождаем память. */
  g_clear_object (&map_grid);
  g_clear_object (&ruler);
  g_clear_object (&control);
  g_clear_object (&pin_layer);
  g_clear_object (&path_layer);
  g_free (tiles_dir);

  return 0;
}

#include "hyscan-gtk-map-kit.h"
#include <hyscan-gtk-map-tiles.h>
#include <hyscan-gtk-map-control.h>
#include <hyscan-gtk-map-ruler.h>
#include <hyscan-gtk-map-grid.h>
#include <hyscan-gtk-map-pin-layer.h>
#include <hyscan-gtk-map-way-layer.h>
#include <hyscan-gtk-map-track-layer.h>
#include <hyscan-cached.h>
#include <hyscan-driver.h>
#include <hyscan-map-profile.h>

/* Слой с треком движения. */
static HyScanGtkLayer *
create_way_layer (HyScanSensor *sensor,
                  const gchar  *sensor_name)
{
  HyScanNavigationModel *model;
  HyScanGtkLayer *layer;
  HyScanCached *cache;

  if (sensor == NULL)
    return NULL;

  model = hyscan_navigation_model_new ();
  hyscan_navigation_model_set_sensor (model, sensor);
  hyscan_navigation_model_set_sensor_name (model, sensor_name);

  cache = hyscan_cached_new (100);
  // cache = NULL;
  layer = hyscan_gtk_map_way_layer_new (model, HYSCAN_CACHE (cache));

  g_object_unref (model);
  g_clear_object (&cache);

  return layer;
}


static GtkWidget *
create_map (HyScanDB          *db,
            HyScanGtkMapKit   *kit,
            const gchar       *project_name,
            HyScanSensor      *sensor,
            const gchar       *sensor_name)
{
  HyScanGtkMap *map;
  gdouble *scales;
  gint scales_len;

  map = HYSCAN_GTK_MAP (hyscan_gtk_map_new (&kit->center));

  /* Устанавливаем допустимые масштабы. */
  scales = hyscan_gtk_map_create_scales2 (1.0 / 10, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
  hyscan_gtk_map_set_scales_meter (map, scales, scales_len);
  g_free (scales);

  /* Добавляем слои. */
  {
    kit->map_grid = hyscan_gtk_map_grid_new ();
    kit->ruler = hyscan_gtk_map_ruler_new ();
    kit->pin_layer = hyscan_gtk_map_pin_layer_new ();
    kit->way_layer = create_way_layer (sensor, sensor_name);

    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), hyscan_gtk_map_control_new (), "control");

    if (kit->way_layer != NULL)
      hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), HYSCAN_GTK_LAYER (kit->way_layer), "way");

    /* Добавляем слой с галсами. */
    if (db != NULL && project_name != NULL)
      {
        HyScanCache *cached;

        cached = HYSCAN_CACHE (hyscan_cached_new (200));
        kit->track_layer = hyscan_gtk_map_track_layer_new (db, project_name, cached);
        g_object_unref (cached);

        hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->track_layer,   "track");
      }

    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->pin_layer,   "pin");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->ruler,       "ruler");
    hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->map_grid,    "grid");
  }

  /* Чтобы виджет карты занял всё доступное место. */
  gtk_widget_set_hexpand (GTK_WIDGET (map), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (map), TRUE);

  return GTK_WIDGET (map);
}

static void
free_profiles (GPtrArray *profiles_array)
{
  g_ptr_array_free (profiles_array, TRUE);
}

/* Получает NULL-терминированный массив названий профилей.
 * Скопировано почти без изменений из hyscan_profile_common_list_profiles. */
static gchar **
list_profiles (const gchar *profiles_path)
{
  guint nprofiles = 0;
  gchar **profiles = NULL;
  GError *error = NULL;
  GDir *dir;

  dir = g_dir_open (profiles_path, 0, &error);
  if (error == NULL)
    {
      const gchar *filename;

      while ((filename = g_dir_read_name (dir)) != NULL)
        {
          gchar *fullname;

          fullname = g_build_path (G_DIR_SEPARATOR_S, profiles_path, filename, NULL);
          if (g_file_test (fullname, G_FILE_TEST_IS_REGULAR))
            {
              profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
              profiles[nprofiles - 1] = g_strdup (fullname);
            }
          g_free (fullname);
        }
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_dir_close (dir);

  profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
  profiles[nprofiles - 1] = NULL;

  return profiles;
}

static void
combo_box_add_profile (GtkComboBoxText  *combo_box,
                       HyScanMapProfile *profile)
{
  gchar *profile_title;
  GPtrArray *profiles_array;

  profiles_array = g_object_get_data (G_OBJECT (combo_box), "profiles");

  profile_title = hyscan_map_profile_get_title (profile);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), profile_title);
  g_ptr_array_add (profiles_array, g_object_ref (profile));
  g_free (profile_title);
}

static void
on_profile_change (GtkComboBoxText *widget,
                   gpointer         user_data)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (user_data);
  GPtrArray *profiles_array;
  gint index;

  profiles_array = g_object_get_data (G_OBJECT (widget), "profiles");
  if (profiles_array == NULL)
    return;

  index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  if (index < 0 || index >= (gint) profiles_array->len)
    return;

  hyscan_map_profile_apply (profiles_array->pdata[index], map);
}

/* Создает выпадающий список с профилями. */
static GtkWidget *
create_profile_switch (HyScanGtkMap *map,
                       const gchar  *profile_dir)
{
  GtkWidget *combo_box;
  GPtrArray *profiles_array;

  gchar **config_files;
  guint conf_i;

  profiles_array = g_ptr_array_new_with_free_func (g_object_unref);
  combo_box = gtk_combo_box_text_new ();
  g_object_set_data_full (G_OBJECT (combo_box), "profiles", profiles_array, (GDestroyNotify) free_profiles);

  /* Считываем профили карты из всех файлов в папке. */
  if (profile_dir != NULL)
    {
      config_files = list_profiles (profile_dir);
      for (conf_i = 0; config_files[conf_i] != NULL; ++conf_i)
        {
          HyScanMapProfile *profile;
          profile = hyscan_map_profile_new ();

          if (hyscan_map_profile_read (profile, config_files[conf_i]))
            combo_box_add_profile (GTK_COMBO_BOX_TEXT (combo_box), profile);

          g_object_unref (profile);
        }
      g_strfreev (config_files);
    }

  /* Если не добавили не один профиль, то добавим хотя бы дефолтный. */
  if (profiles_array->len == 0)
    {
      HyScanMapProfile *profile;

      profile = hyscan_map_profile_new_default ();
      combo_box_add_profile (GTK_COMBO_BOX_TEXT (combo_box), profile);

      g_object_unref (profile);
    }

  g_signal_connect (combo_box, "changed", G_CALLBACK (on_profile_change), map);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), 0);

  return combo_box;
}

static void
on_editable_switch (GtkSwitch               *widget,
                    GParamSpec              *pspec,
                    HyScanGtkLayerContainer *container)
{
  hyscan_gtk_layer_container_set_changes_allowed (container, gtk_switch_get_active (widget));
}


/* Переключение активного слоя. */
static void
on_row_select (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (kit->map);
  HyScanGtkLayer *layer = NULL;
  GtkWidget *layer_tools = NULL;

  if (row != NULL)
    layer = g_object_get_data (G_OBJECT (row), "layer");

  if (layer != NULL)
    layer_tools = g_object_get_data (G_OBJECT (layer), "toolbox-cb");

  if (layer == NULL || !hyscan_gtk_layer_grab_input (layer))
    hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (map), NULL);

  if (layer_tools != NULL && GTK_IS_WIDGET (layer_tools))
    {
      gtk_stack_set_visible_child (GTK_STACK (kit->layer_tool_stack), layer_tools);
      gtk_widget_show_all (GTK_WIDGET (kit->layer_tool_stack));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (kit->layer_tool_stack));
    }
}

static void
on_change_layer_visibility (GtkToggleButton *widget,
                            GParamSpec      *pspec,
                            HyScanGtkLayer  *layer)
{
  gboolean visible;

  visible = gtk_toggle_button_get_active (widget);
  hyscan_gtk_layer_set_visible (layer, visible);
}

/* Добавляет @layer в виджет со списком слоев. */
static void
add_layer_row (GtkListBox     *list_box,
               const gchar    *title,
               HyScanGtkLayer *layer)
{
  GtkWidget *box;
  GtkWidget *vsbl_chkbx;
  GtkWidget *row;

  /* Строка состоит из галочки и текста. */

  /* По галочке устанавливаем видимость слоя. */
  vsbl_chkbx = gtk_check_button_new ();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (vsbl_chkbx), hyscan_gtk_layer_get_visible (layer));
  g_signal_connect (vsbl_chkbx, "notify::active", G_CALLBACK (on_change_layer_visibility), layer);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start (GTK_BOX (box), vsbl_chkbx, FALSE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (box), gtk_label_new (title), TRUE, TRUE, 0);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_container_add (GTK_CONTAINER (row), box);

  /* Каждая строка хранит в себе указатель на слой. При удалении строки делаем unref слоя. */
  g_object_set_data (G_OBJECT (row), "layer", g_object_ref (layer));
  g_signal_connect_swapped (row, "destroy", G_CALLBACK (g_object_unref), layer);

  gtk_list_box_insert (list_box, row, 0);
}

/* Обрабатывает изменение настроек галса в слое HyScanGtkMapTrackLayer. */
static void
track_settings_changed (GtkSpinButton *spin_button,
                        gpointer       user_data)
{
  HyScanGtkMapTrackLayer *layer = HYSCAN_GTK_MAP_TRACK_LAYER (user_data);
  gint channel_num;
  gchar *track_name;
  guint channel;

  channel_num = (gint) gtk_spin_button_get_value (spin_button);
  track_name = g_object_get_data (G_OBJECT (spin_button), "track-name");
  channel = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (spin_button), "channel"));

  hyscan_gtk_map_track_layer_track_set_channel (layer, track_name, channel, channel_num);
}

/* Виджет переключения номера канала. */
static GtkWidget *
channel_spin_new (HyScanGtkMapTrackLayer *track_layer,
                  const gchar            *track_name,
                  guint                   channel)
{
  GtkWidget *spin_button;
  guint max_channel;

  max_channel = hyscan_gtk_map_track_layer_track_max_channel (track_layer, track_name, channel);
  spin_button = gtk_spin_button_new_with_range (0, max_channel, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button),
                             hyscan_gtk_map_track_layer_track_get_channel (track_layer, track_name, channel));
  g_object_set_data_full (G_OBJECT (spin_button), "track-name", g_strdup (track_name), g_free);
  g_object_set_data (G_OBJECT (spin_button), "channel", GUINT_TO_POINTER (channel));
  g_signal_connect (spin_button, "value-changed", G_CALLBACK (track_settings_changed), track_layer);

  return spin_button;
}

/* Виджет с выбором каналов источников данных для отдельного трека. */
static GtkWidget *
track_settings_new (HyScanGtkMapTrackLayer *track_layer,
                    const gchar *track_name)
{
  GtkWidget *form;
  GtkWidget *rmc_channel, *dpt_channel, *port_channel, *starboard_channel;

  form = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

  rmc_channel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  dpt_channel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  port_channel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  starboard_channel = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  gtk_box_pack_start (GTK_BOX (rmc_channel), gtk_label_new ("nmea (rmc)"), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (rmc_channel),
                      channel_spin_new (track_layer, track_name, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (dpt_channel), gtk_label_new ("nmea (dpt)"), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (dpt_channel),
                      channel_spin_new (track_layer, track_name, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (port_channel), gtk_label_new ("ss-port"), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (port_channel),
                      channel_spin_new (track_layer, track_name, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (starboard_channel), gtk_label_new ("ss-starboard"), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (starboard_channel),
                      channel_spin_new (track_layer, track_name, HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD),
                      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (form), gtk_label_new ("Выбор каналов данных"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (form), rmc_channel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (form), dpt_channel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (form), port_channel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (form), starboard_channel, FALSE, FALSE, 0);

  return form;
}

static void
on_enable_track (GtkToggleButton        *widget,
                 GParamSpec             *pspec,
                 HyScanGtkMapTrackLayer *track_layer)
{
  gboolean enable;
  const gchar *track_name;

  enable = gtk_toggle_button_get_active (widget);
  track_name = gtk_button_get_label (GTK_BUTTON (widget));

  hyscan_gtk_map_track_layer_track_enable (track_layer, track_name, enable);
}

static void
on_locate_track_clicked (GtkButton *button,
                         gpointer   user_data)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (user_data);
  gchar *track_name;

  track_name = g_object_get_data (G_OBJECT (button), "track-name");
  hyscan_gtk_map_track_layer_track_view (track_layer, track_name);
}

/* Виджет отдельной строки в списке галсов. */
static GtkWidget *
track_row_new (HyScanGtkMapTrackLayer *track_layer,
               const gchar            *track_name)
{
  GtkWidget *box;
  GtkWidget *track_checkbox;
  GtkWidget *locate_button;
  GtkWidget *settings_button;
  GtkWidget *row;
  GtkWidget *settings;
  GtkWidget *settings_popover;

  /* Галочка установки видимости слоя. */
  track_checkbox = gtk_check_button_new_with_label (track_name);
  g_signal_connect (track_checkbox, "notify::active", G_CALLBACK (on_enable_track), track_layer);

  /* Кнопка поиска галса на карте. */
  locate_button = gtk_button_new_from_icon_name ("zoom-fit-best", GTK_ICON_SIZE_MENU);
  g_object_set_data_full (G_OBJECT (locate_button), "track-name", g_strdup (track_name), g_free);
  g_signal_connect (locate_button, "clicked", G_CALLBACK (on_locate_track_clicked), track_layer);

  /* Кнопка переход к настройкам галса. */
  settings_button = gtk_menu_button_new ();
  settings_popover = gtk_popover_new (settings_button);
  settings = track_settings_new (track_layer, track_name);
  gtk_widget_show_all (settings);
  gtk_container_add (GTK_CONTAINER (settings_popover), settings);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (settings_button), settings_popover);

  /* Строка галса. */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start (GTK_BOX (box), track_checkbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), locate_button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), settings_button, FALSE, FALSE, 0);

  row = gtk_list_box_row_new ();
  g_object_set (row, "margin-bottom", 6,   "margin-top", 6,
                     "activatable", FALSE, "selectable", FALSE, NULL);
  gtk_container_add (GTK_CONTAINER (row), box);

  return row;
}

/* Выбор галсов проекта. */
static GtkWidget *
create_track_box (HyScanGtkMapTrackLayer *track_layer,
                  HyScanDB               *db,
                  const gchar            *project_name)
{
  GtkWidget *box;
  GtkWidget *label;

  gint32 project_id;
  gchar **track_list = NULL;
  guint i;
  gchar *title;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Название проекта. */
  title = g_strdup_printf ("Галсы %s", project_name);
  label = gtk_label_new (title);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new (title), FALSE, FALSE, 0);
  g_free (title);

  project_id = hyscan_db_project_open (db, project_name);
  if (project_id > 0)
    track_list = hyscan_db_track_list (db, project_id);

  /* Список галсов. */
  if (track_list != NULL)
    {
      GtkWidget *list_box;
      GtkWidget *scrolled_window;

      list_box = gtk_list_box_new ();

      for (i = 0; track_list[i] != NULL; ++i)
        gtk_list_box_insert (GTK_LIST_BOX (list_box), track_row_new (track_layer, track_list[i]), 0);

      scrolled_window = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 200);
      gtk_container_add (GTK_CONTAINER (scrolled_window), list_box);

      gtk_box_pack_end (GTK_BOX (box), scrolled_window, FALSE, FALSE, 0);

      g_strfreev (track_list);
    }
  else
    {
      gtk_box_pack_end (GTK_BOX (box), gtk_label_new ("Галсы не найдены"), TRUE, FALSE, 0);
    }

  if (project_id > 0)
    hyscan_db_close (db, project_id);

  return box;
}

static void
on_coordinate_change (GtkSwitch  *widget,
                      GParamSpec *pspec,
                      gdouble    *value)
{
  *value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
}

static void
on_move_to_click (HyScanGtkMapKit *kit)
{
  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), kit->center);
}

static gboolean
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

/* Создаёт панель инструментов для слоя булавок и линейки. */
static GtkWidget *
create_ruler_toolbox (HyScanGtkLayer *layer,
                      const gchar    *label)
{
  GtkWidget *box;
  GtkWidget *ctrl_widget;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  ctrl_widget = gtk_button_new ();
  gtk_button_set_label (GTK_BUTTON (ctrl_widget), label);
  g_signal_connect_swapped (ctrl_widget, "clicked", G_CALLBACK (hyscan_gtk_map_pin_layer_clear), layer);

  gtk_box_pack_start (GTK_BOX (box), ctrl_widget, TRUE, FALSE, 10);

  return box;
}

/* Кнопки управления виджетом. */
static GtkWidget *
create_control_box (HyScanGtkMapKit *kit,
                    HyScanDB        *db,
                    const gchar     *project_name,
                    const gchar     *profile_dir)
{
  GtkWidget *ctrl_box;
  GtkWidget *ctrl_widget;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Выпадающий список с профилями. */
  gtk_container_add (GTK_CONTAINER (ctrl_box), create_profile_switch (HYSCAN_GTK_MAP (kit->map), profile_dir));

  /* Блокировка редактирования. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Редактирование"));

    ctrl_widget = gtk_switch_new ();
    gtk_switch_set_active (GTK_SWITCH (ctrl_widget),
                           hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (kit->map)));
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
    g_signal_connect (ctrl_widget, "notify::active", G_CALLBACK (on_editable_switch), kit->map);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Переключение слоёв. */
  {
    GtkWidget *list_box;

    list_box = gtk_list_box_new ();

    add_layer_row (GTK_LIST_BOX (list_box), "Коорд. сетка", kit->map_grid);
    add_layer_row (GTK_LIST_BOX (list_box), "Линейка", kit->ruler);
    add_layer_row (GTK_LIST_BOX (list_box), "Булавка", kit->pin_layer);
    if (kit->way_layer != NULL)
      add_layer_row (GTK_LIST_BOX (list_box), "Трек", kit->way_layer);
    if (kit->track_layer != NULL)
      add_layer_row (GTK_LIST_BOX (list_box), "Галсы", kit->track_layer);
    g_signal_connect (list_box, "row-selected", G_CALLBACK (on_row_select), kit);

    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Слои"));
    gtk_container_add (GTK_CONTAINER (ctrl_box), list_box);
  }

  /* Контейнер для панели инструментов каждого слоя. */
  {
    GtkWidget *layer_tools;

    kit->layer_tool_stack = gtk_stack_new ();
    gtk_stack_set_homogeneous (GTK_STACK (kit->layer_tool_stack), FALSE);
    gtk_container_add (GTK_CONTAINER (ctrl_box), GTK_WIDGET (kit->layer_tool_stack));

    /* Устаналиваем виджеты с инструментами для каждого слоя. */
    layer_tools = create_ruler_toolbox (kit->ruler, "Удалить линейку");
    g_object_set_data (G_OBJECT (kit->ruler), "toolbox-cb", layer_tools);
    gtk_stack_add_titled (GTK_STACK (kit->layer_tool_stack), layer_tools, "ruler", "Ruler");

    layer_tools = create_ruler_toolbox (kit->pin_layer, "Удалить все метки");
    g_object_set_data (G_OBJECT (kit->pin_layer), "toolbox-cb", layer_tools);
    gtk_stack_add_titled (GTK_STACK (kit->layer_tool_stack), layer_tools, "pin", "Pin");

    if (kit->track_layer != NULL)
      {
        layer_tools = create_track_box (HYSCAN_GTK_MAP_TRACK_LAYER (kit->track_layer), db, project_name);
        g_object_set_data (G_OBJECT (kit->track_layer), "toolbox-cb", layer_tools);
        gtk_stack_add_titled (GTK_STACK (kit->layer_tool_stack), layer_tools, "track", "Track");
      }
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Текстовые поля для ввода координат. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Навигация (ш, д)"));

    ctrl_widget = gtk_spin_button_new_with_range (-90.0, 90.0, 0.001);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (ctrl_widget), kit->center.lat);
    g_signal_connect (ctrl_widget, "notify::value", G_CALLBACK (on_coordinate_change), &kit->center.lat);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);

    ctrl_widget = gtk_spin_button_new_with_range (-180.0, 180.0, 0.001);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (ctrl_widget), kit->center.lon);
    g_signal_connect (ctrl_widget, "notify::value", G_CALLBACK (on_coordinate_change), &kit->center.lon);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);

    ctrl_widget = gtk_button_new ();
    gtk_button_set_label (GTK_BUTTON (ctrl_widget), "Перейти");
    g_signal_connect_swapped (ctrl_widget, "clicked", G_CALLBACK (on_move_to_click), kit);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Текущие координаты. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Координаты"));
    ctrl_widget = gtk_label_new ("-, -");
    g_object_set (ctrl_widget, "width-chars", 24, NULL);
    g_signal_connect (kit->map, "motion-notify-event", G_CALLBACK (on_motion_show_coords), ctrl_widget);
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
  }

  return ctrl_box;
}

/**
 * hyscan_gtk_map_kit_new_map:
 * @center: центр карты
 * @db: указатель на #HyScanDB
 * @project_name: имя проекта
 * @profile_dir: папка с профилями карты (примеры файлов-профилей в папке hyscangui/misc)
 * @sensor: датчик GPS-ресивера
 * @sensor_name: имя датчика
 *
 * Returns: указатель на структуру #HyScanGtkMapKit, для удаления g_free().
 */
HyScanGtkMapKit *
hyscan_gtk_map_kit_new (HyScanGeoGeodetic *center,
                        HyScanDB          *db,
                        const gchar       *project_name,
                        const gchar       *profile_dir,
                        HyScanSensor      *sensor,
                        const gchar       *sensor_name)
{
  HyScanGtkMapKit *kit;

  kit = g_new0 (HyScanGtkMapKit, 1);

  kit->center  = *center;
  kit->map     = create_map (db, kit, project_name, sensor, sensor_name);
  kit->control = create_control_box (kit, db, project_name, profile_dir);

  return kit;
}

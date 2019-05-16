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
#include <hyscan-map-tile-loader.h>
#include <hyscan-db-info.h>
#include <hyscan-gtk-param-tree.h>
#include <hyscan-gtk-map-wfmark-layer.h>

#define PRELOAD_STATE_DONE 1000         /* Статус кэширования тайлов 0 "Загрузка завершена". */

/* Столбцы в GtkTreeView списка галсов. */
enum
{
  VISIBLE_COLUMN,
  DATE_SORT_COLUMN,
  TRACK_COLUMN,
  DATE_COLUMN
};

struct _HyScanGtkMapKitPrivate
{
  HyScanDBInfo          *db_info;          /* Доступ к данным БД. */
  GtkButton             *preload_button;   /* Кнопка загрузки тайлов. */
  GtkProgressBar        *preload_progress; /* Индикатор загрузки тайлов. */
  HyScanMapTileLoader   *loader;
  guint                  preload_tag;      /* Timeout-функция обновления виджета preload_progress. */
  gint                   preload_state;    /* Статус загрузки тайлов.
                                            * < 0                      - загрузка не началась,
                                            * 0 - PRELOAD_STATE_DONE-1 - прогресс загрузки,
                                            * >= PRELOAD_STATE_DONE    - загрузка завершена, не удалось загрузить
                                            *                            PRELOAD_STATE_DONE - preload_state тайлов */

  GtkTreeView           *track_tree;       /* GtkTreeView со списком галсов. */
  GtkListStore          *track_store;      /* Модель данных для track_tree. */
  GtkMenu               *track_menu;       /* Контекстное меню галса (по правой кнопке). */
};

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
        kit->wfmark_layer = hyscan_gtk_map_wfmark_layer_new (db, project_name, cached);

        g_object_unref (cached);

        hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->track_layer,   "track");
        hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), kit->wfmark_layer,  "wfmark");
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
  const gchar *layer_tools = NULL;

  if (row != NULL)
    layer = g_object_get_data (G_OBJECT (row), "layer");

  if (layer != NULL)
    layer_tools = g_object_get_data (G_OBJECT (layer), "toolbox-cb");

  if (layer == NULL || !hyscan_gtk_layer_grab_input (layer))
    hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (map), NULL);

  if (layer_tools != NULL)
    {
      gtk_stack_set_visible_child_name(GTK_STACK (kit->layer_tool_stack), layer_tools);
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

static void
on_enable_track (GtkCellRendererToggle *cell_renderer,
                 gchar                 *path,
                 gpointer               user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  gboolean enable;
  gchar *track_name;
  GtkTreePath *tree_path;
  GtkTreeIter iter;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);

  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (tree_model, &iter, tree_path);
  gtk_tree_path_free (tree_path);
  gtk_tree_model_get (tree_model, &iter, TRACK_COLUMN, &track_name, VISIBLE_COLUMN, &enable, -1);

  enable = !enable;
  gtk_list_store_set (priv->track_store, &iter, VISIBLE_COLUMN, enable, -1);

  hyscan_gtk_map_track_layer_track_set_visible (HYSCAN_GTK_MAP_TRACK_LAYER (kit->track_layer), track_name, enable);
  g_free (track_name);
}

static void
on_locate_track_clicked (GtkButton *button,
                         gpointer   user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeSelection *selection;
  GList *list;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);

  selection = gtk_tree_view_get_selection (priv->track_tree);
  list = gtk_tree_selection_get_selected_rows (selection, &tree_model);
  if (list != NULL)
    {
      GtkTreePath *path = list->data;
      GtkTreeIter iter;
      gchar *track_name;

      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->track_store), &iter, path))
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
          hyscan_gtk_map_track_layer_track_view (HYSCAN_GTK_MAP_TRACK_LAYER (kit->track_layer), track_name);

          g_free (track_name);
        }
    }
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

}

static GtkWidget *
create_param_settings_window (HyScanGtkMapKit *kit,
                              const gchar     *title,
                              HyScanParam     *param)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *window;
  GtkWidget *frontend;
  GtkWidget *grid;
  GtkWidget *abar;
  GtkWidget *apply, *discard, *ok, *cancel;
  GtkSizeGroup *size;
  GtkWidget *toplevel;

  /* Настраиваем окошко. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 400);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->track_tree));
  if (GTK_IS_WINDOW (toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));

  g_signal_connect_swapped (window, "destroy", G_CALLBACK (gtk_widget_destroy), window);

  /* Виджет отображения параметров. */
  frontend = hyscan_gtk_param_tree_new (param, "/", FALSE);
  hyscan_gtk_param_set_watch_period (HYSCAN_GTK_PARAM (frontend), 500);

  /* Кнопки сохранения настроек. */
  ok = gtk_button_new_with_label ("OK");
  cancel = gtk_button_new_with_label ("Отмена");
  discard = gtk_button_new_with_label ("Откатить");
  apply = gtk_button_new_with_label ("Применить");

  g_signal_connect_swapped (apply,   "clicked", G_CALLBACK (hyscan_gtk_param_apply),   frontend);
  g_signal_connect_swapped (discard, "clicked", G_CALLBACK (hyscan_gtk_param_discard), frontend);
  g_signal_connect_swapped (ok,      "clicked", G_CALLBACK (hyscan_gtk_param_apply),   frontend);
  g_signal_connect_swapped (ok,      "clicked", G_CALLBACK (gtk_widget_destroy),       window);
  g_signal_connect_swapped (cancel,  "clicked", G_CALLBACK (gtk_widget_destroy),       window);

  abar = gtk_action_bar_new ();

  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), apply);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), discard);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), cancel);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (abar), ok);

  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  g_object_set_data_full (G_OBJECT (abar), "size-group", size, g_object_unref);

  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), apply);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), discard);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), ok);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), cancel);

  grid = gtk_grid_new ();
  g_object_set (grid, "margin", 10, NULL);

  /* Пакуем всё вместе. */
  gtk_grid_attach (GTK_GRID (grid), frontend, 0, 0, 4, 1);
  gtk_grid_attach (GTK_GRID (grid), abar,     0, 1, 4, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);

  return window;
}

static void
on_configure_track_clicked (GtkButton *button,
                            gpointer   user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeSelection *selection;
  GList *list;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);

  selection = gtk_tree_view_get_selection (priv->track_tree);
  list = gtk_tree_selection_get_selected_rows (selection, &tree_model);
  if (list != NULL)
    {
      GtkTreePath *path = list->data;
      GtkTreeIter iter;
      gchar *track_name;

      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->track_store), &iter, path))
        {
          GtkWidget *window;
          HyScanGtkMapTrack *track;

          gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
          track = hyscan_gtk_map_track_layer_lookup (HYSCAN_GTK_MAP_TRACK_LAYER (kit->track_layer), track_name);

          window = create_param_settings_window (kit, "Параметры галса", HYSCAN_PARAM (track));
          gtk_widget_show_all (window);

          g_free (track_name);
        }
    }
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

}

/* Функция вызывается при изменении списка галсов. */
void
tracks_changed (HyScanDBInfo    *db_info,
                HyScanGtkMapKit *kit)
{
  GtkTreePath *null_path;
  GtkTreeIter tree_iter;
  GHashTable *tracks;
  GHashTableIter hash_iter;
  gpointer key, value;

  HyScanGtkMapKitPrivate *priv = kit->priv;

  null_path = gtk_tree_path_new ();
  gtk_tree_view_set_cursor (priv->track_tree, null_path, NULL, FALSE);
  gtk_tree_path_free (null_path);

  gtk_list_store_clear (priv->track_store);

  tracks = hyscan_db_info_get_tracks (db_info);
  g_hash_table_iter_init (&hash_iter, tracks);

  while (g_hash_table_iter_next (&hash_iter, &key, &value))
    {
      GDateTime *local;
      gchar *time_str;
      HyScanTrackInfo *track_info = value;
      gboolean visible;

      if (track_info->id == NULL)
        {
          g_message ("Unable to open track <%s>", track_info->name);
          continue;
        }

      /* Добавляем в список галсов. */
      local = g_date_time_to_local (track_info->ctime);
      time_str = g_date_time_format (local, "%d.%m %H:%M");
      visible = hyscan_gtk_map_track_layer_track_get_visible (HYSCAN_GTK_MAP_TRACK_LAYER (kit->track_layer),
                                                              track_info->id);

      gtk_list_store_append (priv->track_store, &tree_iter);
      gtk_list_store_set (priv->track_store, &tree_iter,
                          VISIBLE_COLUMN, visible,
                          DATE_SORT_COLUMN, g_date_time_to_unix (track_info->ctime),
                          TRACK_COLUMN, track_info->name,
                          DATE_COLUMN, time_str,
                          -1);

      g_free (time_str);
      g_date_time_unref (local);
    }

  g_hash_table_unref (tracks);
}

gboolean
on_button_press_event (GtkTreeView     *tree_view,
                       GdkEventButton  *event_button,
                       HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if ((event_button->type == GDK_BUTTON_PRESS) && (event_button->button == 3))
    {
      gtk_menu_popup (priv->track_menu, NULL, NULL, NULL, NULL, event_button->button, event_button->time);
    }

  return FALSE;
}

static GtkTreeView *
create_track_tree_view (HyScanGtkMapKit *kit,
                        GtkTreeModel    *tree_model)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *visible_column, *track_column, *date_column;
  GtkTreeView *tree_view;

  /* Галочка с признаком видимости галса. */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (on_enable_track), kit);
  visible_column = gtk_tree_view_column_new_with_attributes ("On", renderer,
                                                             "active", VISIBLE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (visible_column, VISIBLE_COLUMN);

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  track_column = gtk_tree_view_column_new_with_attributes ("Track", renderer,
                                                           "text", TRACK_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (track_column, TRACK_COLUMN);

  /* Дата создания галса. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes ("Date", renderer,
                                                          "text", DATE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, DATE_SORT_COLUMN);

  tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (tree_model));
  gtk_tree_view_set_search_column (tree_view, TRACK_COLUMN);
  gtk_tree_view_append_column (tree_view, visible_column);
  gtk_tree_view_append_column (tree_view, track_column);
  gtk_tree_view_append_column (tree_view, date_column);

  return tree_view;
}

static GtkMenu *
create_track_menu (HyScanGtkMapKit *kit)
{
  GtkWidget *menu_item;
  GtkMenu *menu;

  menu = GTK_MENU (gtk_menu_new ());
  menu_item = gtk_menu_item_new_with_label ("Найти на карте");
  g_signal_connect (menu_item, "activate", G_CALLBACK (on_locate_track_clicked), kit);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label ("Выбрать каналы данных");
  g_signal_connect (menu_item, "activate", G_CALLBACK (on_configure_track_clicked), kit);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  return menu;
}

/* Выбор галсов проекта. */
static GtkWidget *
create_track_box (HyScanGtkMapKit *kit,
                  HyScanDB        *db,
                  const gchar     *project_name)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *scrolled_window;

  gchar *title;

  priv->track_store = gtk_list_store_new (4,
                                          G_TYPE_BOOLEAN, /* VISIBLE_COLUMN   */
                                          G_TYPE_INT64,   /* DATE_SORT_COLUMN */
                                          G_TYPE_STRING,  /* TRACK_COLUMN     */
                                          G_TYPE_STRING   /* DATE_COLUMN      */);
  priv->track_tree = create_track_tree_view (kit, GTK_TREE_MODEL (priv->track_store));

  priv->track_menu = create_track_menu (kit);
  gtk_menu_attach_to_widget (priv->track_menu, GTK_WIDGET (priv->track_tree), NULL);

  g_signal_connect_swapped (priv->track_tree, "destroy", G_CALLBACK (gtk_widget_destroy), priv->track_menu);
  g_signal_connect (priv->track_tree, "button-press-event", G_CALLBACK (on_button_press_event), kit);
  g_signal_connect (priv->db_info, "tracks-changed", G_CALLBACK (tracks_changed), kit);


  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Название проекта. */
  title = g_strdup_printf ("Галсы %s", project_name);
  label = gtk_label_new (title);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  g_free (title);

  /* Область прокрутки со списком галсов. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 200);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->track_tree));


  /* Пакуем всё вместе. */
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), scrolled_window, FALSE, FALSE, 0);

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

static gboolean
source_update_progress (gpointer data)
{
  HyScanGtkMapKit *kit = data;
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gint state;

  state = g_atomic_int_get (&priv->preload_state);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->preload_progress),
                                 CLAMP ((gdouble) state / PRELOAD_STATE_DONE, 0.0, 1.0));

  return G_SOURCE_CONTINUE;
}

static gboolean
source_progress_done (gpointer data)
{
  HyScanGtkMapKit *kit = data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  gint state;
  gchar *text = NULL;

  state = g_atomic_int_get (&priv->preload_state);

  if (state == PRELOAD_STATE_DONE)
    text = g_strdup ("Done!");
  else if (state > PRELOAD_STATE_DONE)
    text = g_strdup_printf ("Done! Failed to load %d tiles", state - PRELOAD_STATE_DONE);
  else
    g_warn_if_reached ();

  gtk_button_set_label (priv->preload_button, "Начать загрузку");
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->preload_progress), 1.0);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->preload_progress), text);
  g_free (text);

  g_signal_handlers_disconnect_by_data (priv->loader, kit);
  g_clear_object (&priv->loader);

  g_atomic_int_set (&priv->preload_state, -1);

  return G_SOURCE_REMOVE;
}

static void
on_preload_done (HyScanMapTileLoader *loader,
                 guint                failed,
                 HyScanGtkMapKit     *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  g_atomic_int_set (&priv->preload_state, PRELOAD_STATE_DONE + failed);

  g_source_remove (priv->preload_tag);
  g_idle_add (source_progress_done, kit);
}

static void
on_preload_progress (HyScanMapTileLoader *loader,
                     gdouble              fraction,
                     HyScanGtkMapKit     *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_atomic_int_set (&priv->preload_state, CLAMP ((gint) (PRELOAD_STATE_DONE * fraction), 0, PRELOAD_STATE_DONE - 1));
}

static void
preload_stop (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  hyscan_map_tile_loader_stop (priv->loader);
}

static void
preload_start (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkLayer *layer;
  HyScanGtkMapTileSource *source;
  gdouble from_x, to_x, from_y, to_y;
  GThread *thread;

  // todo: надо бы найти слой каким-то другим образом, а не по ключу "tiles-layer"
  layer = hyscan_gtk_layer_container_lookup (HYSCAN_GTK_LAYER_CONTAINER (kit->map), "tiles-layer");
  if (!HYSCAN_IS_GTK_MAP_TILES (layer))
    return;

  source = hyscan_gtk_map_tiles_get_source (HYSCAN_GTK_MAP_TILES (layer));
  priv->loader = hyscan_map_tile_loader_new ();
  g_object_unref (source);

  gtk_button_set_label (priv->preload_button, "Остановить загрузку");
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->preload_progress), NULL);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->preload_progress), 0.0);

  /* Регистрируем обработчики сигналов для loader'а и обновление индикатора загрузки. */
  g_signal_connect (priv->loader, "progress", G_CALLBACK (on_preload_progress), kit);
  g_signal_connect (priv->loader, "done",     G_CALLBACK (on_preload_done),     kit);
  priv->preload_tag = g_timeout_add (500, source_update_progress, kit);

  /* Запускаем загрузку тайлов. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (kit->map), &from_x, &to_x, &from_y, &to_y);
  thread = hyscan_map_tile_loader_start (priv->loader, source, from_x, to_x, from_y, to_y);
  g_thread_unref (thread);
}

static void
on_preload_click (GtkButton       *button,
                  HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if (g_atomic_int_compare_and_exchange (&priv->preload_state, -1, 0))
    preload_start (kit);
  else
    preload_stop (kit);
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
    if (kit->wfmark_layer != NULL)
      add_layer_row (GTK_LIST_BOX (list_box), "Метки водопада", kit->wfmark_layer);
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
    g_object_set_data (G_OBJECT (kit->ruler), "toolbox-cb", "ruler");
    gtk_stack_add_titled (GTK_STACK (kit->layer_tool_stack), layer_tools, "ruler", "Ruler");

    layer_tools = create_ruler_toolbox (kit->pin_layer, "Удалить все метки");
    g_object_set_data (G_OBJECT (kit->pin_layer), "toolbox-cb", "pin");
    gtk_stack_add_titled (GTK_STACK (kit->layer_tool_stack), layer_tools, "pin", "Pin");

    if (kit->track_layer != NULL)
      {
        layer_tools = create_track_box (kit, db, project_name);
        g_object_set_data (G_OBJECT (kit->track_layer), "toolbox-cb", "track");
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
    ctrl_widget = gtk_label_new ("-, -");
    g_object_set (ctrl_widget, "width-chars", 24, NULL);
    g_signal_connect (kit->map, "motion-notify-event", G_CALLBACK (on_motion_show_coords), ctrl_widget);

    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Координаты"));
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Загрузка тайлов. */
  {
    HyScanGtkMapKitPrivate *priv = kit->priv;

    priv->preload_state = -1;

    priv->preload_button = GTK_BUTTON (gtk_button_new_with_label ("Начать загрузку"));
    g_signal_connect (priv->preload_button, "clicked", G_CALLBACK (on_preload_click), kit);

    priv->preload_progress = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
    gtk_progress_bar_set_show_text (priv->preload_progress, TRUE);

    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new ("Кэширование тайлов"));
    gtk_container_add (GTK_CONTAINER (ctrl_box), GTK_WIDGET (priv->preload_progress));
    gtk_container_add (GTK_CONTAINER (ctrl_box), GTK_WIDGET (priv->preload_button));
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
 * Returns: указатель на структуру #HyScanGtkMapKit, для удаления
 *          hyscan_gtk_map_kit_free().
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
  kit->priv = g_new0 (HyScanGtkMapKitPrivate, 1);
  kit->priv->db_info = hyscan_db_info_new (db);

  kit->center  = *center;
  kit->map     = create_map (db, kit, project_name, sensor, sensor_name);
  kit->control = create_control_box (kit, db, project_name, profile_dir);

  hyscan_db_info_set_project (kit->priv->db_info, project_name);

  return kit;
}

/**
 * hyscan_gtk_map_kit_free:
 * @kit: указатель на структуру #HyScanGtkMapKit
 */
void
hyscan_gtk_map_kit_free (HyScanGtkMapKit *kit)
{
  g_free (kit->priv);
  g_free (kit);
}

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
#include <hyscan-list-model.h>
#include <hyscan-mark-model.h>
#include <hyscan-gtk-map-planner.h>
#include <glib/gi18n.h>
#include <hyscan-mark-data-waterfall.h>
#include <hyscan-mark-data-geo.h>
#include <hyscan-gtk-map-geomark-layer.h>

#define PRELOAD_STATE_DONE 1000         /* Статус кэширования тайлов 0 "Загрузка завершена". */

/* Столбцы в GtkTreeView списка слоёв. */
enum
{
  LAYER_VISIBLE_COLUMN,
  LAYER_KEY_COLUMN,
  LAYER_TITLE_COLUMN,
  LAYER_COLUMN,
};

/* Столбцы в GtkTreeView списка галсов. */
enum
{
  VISIBLE_COLUMN,
  DATE_SORT_COLUMN,
  TRACK_COLUMN,
  DATE_COLUMN
};

/* Столбцы в GtkTreeView списка меток. */
enum
{
  MARK_ID_COLUMN,
  MARK_NAME_COLUMN,
  MARK_MTIME_COLUMN,
  MARK_MTIME_SORT_COLUMN
};

struct _HyScanGtkMapKitPrivate
{
  /* Модели данных. */
  HyScanDBInfo          *db_info;          /* Доступ к данным БД. */
  HyScanListModel       *list_model;       /* Модель списка активных галсов. */
  HyScanMarkModel       *mark_model;       /* Модель меток водопада. */
  HyScanMarkModel       *mark_geo_model;   /* Модель геометок. */
  HyScanDB              *db;
  HyScanMarkLocModel    *ml_model;
  HyScanCache           *cache;
  HyScanNavigationModel *nav_model;
  HyScanPlanner         *planner;
  gchar                 *project_name;

  gchar                 *profile_dir;      /* Папка с профилями карты. */
  HyScanGeoGeodetic      center;           /* Географические координаты для виджета навигации. */

  /* Слои. */
  GtkListStore          *layer_store;      /* Модель параметров отображения слоёв. */
  HyScanGtkLayer        *planner_layer;    /* Слой планировщика. */
  HyScanGtkLayer        *track_layer;      /* Слой просмотра галсов. */
  HyScanGtkLayer        *wfmark_layer;     /* Слой с метками водопада. */
  HyScanGtkLayer        *geomark_layer;    /* Слой с метками водопада. */
  HyScanGtkLayer        *map_grid;         /* Слой координатной сетки. */
  HyScanGtkLayer        *ruler;            /* Слой линейки. */
  HyScanGtkLayer        *pin_layer;        /* Слой географических отметок. */
  HyScanGtkLayer        *way_layer;        /* Слой текущего положения по GPS-датчику. */

  /* Виджеты. */
  GtkWidget             *layer_tool_stack; /* GtkStack с виджетами настроек каждого слоя. */
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

  GtkTreeView           *mark_tree;       /* GtkTreeView со списком галсов. */
  GtkListStore          *mark_store;      /* Модель данных для track_tree. */

  GtkWidget             *lat_spin;        /* Поля для ввода широты. */
  GtkWidget             *lon_spin;        /* Поля для ввода долготы. */
};

static GtkWidget *
create_map (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkMap *map;
  gdouble *scales;
  gint scales_len;

  map = HYSCAN_GTK_MAP (hyscan_gtk_map_new (&priv->center));

  /* Устанавливаем допустимые масштабы. */
  scales = hyscan_gtk_map_create_scales2 (1.0 / 1000, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
  hyscan_gtk_map_set_scales_meter (map, scales, scales_len);
  g_free (scales);

  /* По умолчанию добавляем слой управления картой. */
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), hyscan_gtk_map_control_new (), "control");

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

/* Добавляет @layer на карту и в список слоёв. */
static void
add_layer_row (HyScanGtkMapKit *kit,
               HyScanGtkLayer  *layer,
               const gchar     *key,
               const gchar     *title)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkTreeIter tree_iter;

  if (layer == NULL)
    return;

  /* Регистрируем слой в карте. */
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (kit->map), layer, key);

  /* Регистрируем слой в layer_store. */
  gtk_list_store_append (priv->layer_store, &tree_iter);
  gtk_list_store_set (priv->layer_store, &tree_iter,
                      LAYER_VISIBLE_COLUMN, hyscan_gtk_layer_get_visible (layer),
                      LAYER_KEY_COLUMN, key,
                      LAYER_TITLE_COLUMN, title,
                      LAYER_COLUMN, layer,
                      -1);
}

static void
on_enable_track (GtkCellRendererToggle *cell_renderer,
                 gchar                 *path,
                 gpointer               user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  gboolean active;
  gchar *track_name;
  GtkTreePath *tree_path;
  GtkTreeIter iter;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);

  /* Узнаем, галочку какого галса изменил пользователь. */
  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (tree_model, &iter, tree_path);
  gtk_tree_path_free (tree_path);
  gtk_tree_model_get (tree_model, &iter, TRACK_COLUMN, &track_name, VISIBLE_COLUMN, &active, -1);

  /* Устанавливаем новое значение в модель данных. */
  if (active)
    hyscan_list_model_remove (priv->list_model, track_name);
  else
    hyscan_list_model_add (priv->list_model, track_name);

  g_free (track_name);
}

static void
on_enable_layer (GtkCellRendererToggle *cell_renderer,
                 gchar                 *path,
                 gpointer               user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeIter iter;
  GtkTreePath *tree_path;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->layer_store);

  gboolean visible;
  HyScanGtkLayer *layer;

  /* Узнаем, галочку какого слоя изменил пользователь. */
  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (tree_model, &iter, tree_path);
  gtk_tree_path_free (tree_path);
  gtk_tree_model_get (tree_model, &iter,
                      LAYER_COLUMN, &layer,
                      LAYER_VISIBLE_COLUMN, &visible, -1);

  /* Устанавливаем новое данных. */
  visible = !visible;
  hyscan_gtk_layer_set_visible (layer, visible);
  gtk_list_store_set (priv->layer_store, &iter, LAYER_VISIBLE_COLUMN, visible, -1);

  g_object_unref (layer);
}

/* Обработчик HyScanListModel::changed
 * Обновляет список активных галсов при измении модели. */
static void
on_active_track_changed (HyScanListModel *model,
                         HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkTreeIter iter;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->track_store), &iter);
  while (valid)
   {
     gchar *track_name;
     gboolean active;

     gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);

     active = hyscan_list_model_has (priv->list_model, track_name);
     gtk_list_store_set (priv->track_store, &iter, VISIBLE_COLUMN, active, -1);

     g_free (track_name);

     valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->track_store), &iter);
   }
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
          hyscan_gtk_map_track_layer_track_view (HYSCAN_GTK_MAP_TRACK_LAYER (priv->track_layer), track_name);

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
  ok = gtk_button_new_with_label (_("OK"));
  cancel = gtk_button_new_with_label (_("Cancel"));
  discard = gtk_button_new_with_label (_("Restore"));
  apply = gtk_button_new_with_label (_("Apply"));

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
          track = hyscan_gtk_map_track_layer_lookup (HYSCAN_GTK_MAP_TRACK_LAYER (priv->track_layer), track_name);

          window = create_param_settings_window (kit, _("Track settings"), HYSCAN_PARAM (track));
          gtk_widget_show_all (window);

          g_free (track_name);
        }
    }
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

}

/* Функция вызывается при изменении списка галсов. */
static void
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
      visible = hyscan_list_model_has (priv->list_model, track_info->name);

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

static gboolean
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
  visible_column = gtk_tree_view_column_new_with_attributes (_("Visible"), renderer,
                                                             "active", VISIBLE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (visible_column, VISIBLE_COLUMN);

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  track_column = gtk_tree_view_column_new_with_attributes (_("Track"), renderer,
                                                           "text", TRACK_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (track_column, TRACK_COLUMN);

  /* Дата создания галса. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer,
                                                          "text", DATE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, DATE_SORT_COLUMN);

  tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (tree_model));
  gtk_tree_view_set_search_column (tree_view, TRACK_COLUMN);
  gtk_tree_view_append_column (tree_view, visible_column);
  gtk_tree_view_append_column (tree_view, track_column);
  gtk_tree_view_append_column (tree_view, date_column);

  return tree_view;
}

static GtkTreeView *
create_mark_tree_view (HyScanGtkMapKit *kit,
                       GtkTreeModel    *tree_model)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *name_column, *date_column;
  GtkTreeView *tree_view;

  /* Название метки. */
  renderer = gtk_cell_renderer_text_new ();
  name_column = gtk_tree_view_column_new_with_attributes (_("Mark"), renderer,
                                                          "text", MARK_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (name_column, MARK_NAME_COLUMN);

  /* Время последнего изменения метки. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Last upd."), renderer,
                                                          "text", MARK_MTIME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, MARK_MTIME_SORT_COLUMN);

  tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (tree_model));
  gtk_tree_view_set_search_column (tree_view, MARK_NAME_COLUMN);
  gtk_tree_view_append_column (tree_view, name_column);
  gtk_tree_view_append_column (tree_view, date_column);

  return tree_view;
}

static GtkMenu *
create_track_menu (HyScanGtkMapKit *kit)
{
  GtkWidget *menu_item;
  GtkMenu *menu;

  menu = GTK_MENU (gtk_menu_new ());
  menu_item = gtk_menu_item_new_with_label (_("Find track on the map"));
  g_signal_connect (menu_item, "activate", G_CALLBACK (on_locate_track_clicked), kit);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  menu_item = gtk_menu_item_new_with_label (_("Track settings"));
  g_signal_connect (menu_item, "activate", G_CALLBACK (on_configure_track_clicked), kit);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  return menu;
}

static void
on_marks_activated (GtkTreeView        *treeview,
                    GtkTreePath        *path,
                    GtkTreeViewColumn  *col,
                    gpointer            userdata)
{
  HyScanGtkMapKit *kit = userdata;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (treeview);
  if (gtk_tree_model_get_iter (model, &iter, path))
  {
    gchar *mark_id;

    gtk_tree_model_get (model, &iter, MARK_ID_COLUMN, &mark_id, -1);
    hyscan_gtk_map_wfmark_layer_mark_view (HYSCAN_GTK_MAP_WFMARK_LAYER (priv->wfmark_layer), mark_id);

    g_free (mark_id);
  }
}

static void
on_track_activated (GtkTreeView        *treeview,
                    GtkTreePath        *path,
                    GtkTreeViewColumn  *col,
                    gpointer            userdata)
{
  HyScanGtkMapKit *kit = userdata;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (treeview);
  if (gtk_tree_model_get_iter(model, &iter, path))
  {
    gchar *track_name;

    gtk_tree_model_get (model, &iter, TRACK_COLUMN, &track_name, -1);
    hyscan_gtk_map_track_layer_track_view (HYSCAN_GTK_MAP_TRACK_LAYER (priv->track_layer), track_name);

    g_free (track_name);
  }
}

static void
on_marks_changed (HyScanMarkModel *model,
                  HyScanGtkMapKit *kit)
{
  GtkTreePath *null_path;
  GtkTreeIter tree_iter;
  GHashTable *marks;
  GHashTableIter hash_iter;

  HyScanMarkWaterfall *mark;
  gchar *mark_id;

  HyScanGtkMapKitPrivate *priv = kit->priv;

  null_path = gtk_tree_path_new ();
  gtk_tree_view_set_cursor (priv->track_tree, null_path, NULL, FALSE);
  gtk_tree_path_free (null_path);

  gtk_list_store_clear (priv->track_store);

  marks = hyscan_mark_model_get (model);
  g_hash_table_iter_init (&hash_iter, marks);

  while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &mark))
    {
      GDateTime *local;
      gchar *time_str;

      /* Добавляем в список меток. */
      local = g_date_time_new_from_unix_local (mark->modification_time / 1000000);
      time_str = g_date_time_format (local, "%d.%m %H:%M");

      gtk_list_store_append (priv->mark_store, &tree_iter);
      gtk_list_store_set (priv->mark_store, &tree_iter,
                          MARK_ID_COLUMN, mark_id,
                          MARK_NAME_COLUMN, mark->name,
                          MARK_MTIME_COLUMN, time_str,
                          MARK_MTIME_SORT_COLUMN, mark->modification_time,
                          -1);

      g_free (time_str);
      g_date_time_unref (local);
    }

  g_hash_table_unref (marks);
}

/* Навигация по меткам. */
static GtkWidget *
create_wfmark_toolbox (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *scrolled_window;

  if (priv->db == NULL)
    return NULL;

  priv->mark_store = gtk_list_store_new (4,
                                         G_TYPE_STRING,  /* MARK_ID_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_NAME_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_MTIME_COLUMN */
                                         G_TYPE_INT64    /* MARK_MTIME_SORT_COLUMN */);
  priv->mark_tree = create_mark_tree_view (kit, GTK_TREE_MODEL (priv->mark_store));

  g_signal_connect (priv->mark_model, "changed", G_CALLBACK (on_marks_changed), kit);
  g_signal_connect (priv->mark_tree, "row-activated", G_CALLBACK (on_marks_activated), kit);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Название проекта. */
  label = gtk_label_new (_("Marks"));
  gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

  /* Область прокрутки со списком галсов. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 200);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->mark_tree));


  /* Пакуем всё вместе. */
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), scrolled_window, FALSE, FALSE, 0);

  return box;
}

/* Выбор галсов проекта. */
static GtkWidget *
create_track_box (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *scrolled_window;

  if (priv->db == NULL)
    return NULL;

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
  g_signal_connect (priv->track_tree, "row-activated", G_CALLBACK (on_track_activated), kit);
  g_signal_connect (priv->db_info, "tracks-changed", G_CALLBACK (tracks_changed), kit);
  g_signal_connect (priv->list_model, "changed", G_CALLBACK (on_active_track_changed), kit);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Название проекта. */
  label = gtk_label_new (_("Tracks"));
  gtk_label_set_max_width_chars (GTK_LABEL (label), 1);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);

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
on_locate_click (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanNavigationModelData data;

  if (hyscan_navigation_model_get (priv->nav_model, &data, NULL))
    hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), data.coord);
}

static void
on_move_to_click (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), priv->center);
}

static gboolean
on_motion_show_coords (HyScanGtkMap   *map,
                       GdkEventMotion *event,
                       GtkStatusbar   *statusbar)
{
  HyScanGeoGeodetic geo;
  gchar text[255];

  hyscan_gtk_map_point_to_geo (map, &geo, event->x, event->y);
  g_snprintf (text, sizeof (text), "%.5f°, %.5f°", geo.lat, geo.lon);
  gtk_statusbar_push (statusbar, 0, text);

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
    text = g_strdup (_("Done!"));
  else if (state > PRELOAD_STATE_DONE)
    text = g_strdup_printf (_("Done! Failed to load %d tiles"), state - PRELOAD_STATE_DONE);
  else
    g_warn_if_reached ();

  gtk_button_set_label (priv->preload_button, _("Start download"));
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
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILES (layer));

  source = hyscan_gtk_map_tiles_get_source (HYSCAN_GTK_MAP_TILES (layer));
  priv->loader = hyscan_map_tile_loader_new ();
  g_object_unref (source);

  gtk_button_set_label (priv->preload_button, _("Stop download"));
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

/* Список галсов и меток. */
static GtkWidget *
create_navigation_box (HyScanGtkMapKit *kit)
{
  GtkWidget *ctrl_box, *item;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  item = create_track_box (kit);
  if (item != NULL)
    gtk_container_add (GTK_CONTAINER (ctrl_box), item);

  item = create_wfmark_toolbox (kit);
  if (item != NULL)
    gtk_container_add (GTK_CONTAINER (ctrl_box), item);

  gtk_widget_set_size_request (ctrl_box, 200, -1);

  return ctrl_box;
}

/* Текстовые поля для ввода координат. */
static void
create_nav_input (HyScanGtkMapKit *kit,
                  GtkContainer    *container)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *move_button, *locate_button;

  locate_button = gtk_button_new_from_icon_name ("network-wireless", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_margin_bottom (locate_button, 5);
  gtk_widget_set_sensitive (locate_button, priv->nav_model != NULL);
  gtk_button_set_label (GTK_BUTTON (locate_button), _("My location"));
  g_signal_connect_swapped (locate_button, "clicked", G_CALLBACK (on_locate_click), kit);

  priv->lat_spin = gtk_spin_button_new_with_range (-90.0, 90.0, 0.001);
  g_signal_connect (priv->lat_spin, "notify::value", G_CALLBACK (on_coordinate_change), &priv->center.lat);

  priv->lon_spin = gtk_spin_button_new_with_range (-180.0, 180.0, 0.001);
  g_signal_connect (priv->lon_spin, "notify::value", G_CALLBACK (on_coordinate_change), &priv->center.lon);

  move_button = gtk_button_new_with_label (_("Go"));
  g_signal_connect_swapped (move_button, "clicked", G_CALLBACK (on_move_to_click), kit);

  gtk_container_add (container, locate_button);
  gtk_container_add (container, gtk_label_new (_("Coordinates (lat, lon)")));
  gtk_container_add (container, priv->lat_spin);
  gtk_container_add (container, priv->lon_spin);
  gtk_container_add (container, move_button);
}

/* Загрузка тайлов. */
static void
create_preloader (HyScanGtkMapKit *kit,
                  GtkContainer    *container)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  priv->preload_state = -1;

  priv->preload_button = GTK_BUTTON (gtk_button_new_with_label (_("Start download")));
  g_signal_connect (priv->preload_button, "clicked", G_CALLBACK (on_preload_click), kit);

  priv->preload_progress = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
  gtk_progress_bar_set_show_text (priv->preload_progress, TRUE);

  gtk_container_add (container, GTK_WIDGET (priv->preload_progress));
  gtk_container_add (container, GTK_WIDGET (priv->preload_button));
}

/* Текущие координаты. */
static GtkWidget *
create_status_bar (HyScanGtkMapKit *kit)
{
  GtkWidget *statusbar;

  statusbar = gtk_statusbar_new ();
  g_object_set (statusbar, "margin", 0, NULL);
  g_signal_connect (kit->map, "motion-notify-event", G_CALLBACK (on_motion_show_coords), statusbar);

  return statusbar;
}

static void
layer_changed (GtkTreeSelection *selection,
               HyScanGtkMapKit  *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeModel *model;
  GtkTreeIter iter;
  HyScanGtkLayer *layer;
  gchar *layer_key;
  GtkWidget *layer_tools;

  /* Получаем данные выбранного слоя. */
  gtk_tree_selection_get_selected (selection, &model, &iter);
  gtk_tree_model_get (model, &iter,
                      LAYER_COLUMN, &layer,
                      LAYER_KEY_COLUMN, &layer_key,
                      -1);

  if (!hyscan_gtk_layer_grab_input (layer))
    hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (kit->map), NULL);

  layer_tools = gtk_stack_get_child_by_name (GTK_STACK (priv->layer_tool_stack), layer_key);

  if (layer_tools != NULL)
    {
      gtk_stack_set_visible_child (GTK_STACK (priv->layer_tool_stack), layer_tools);
      gtk_widget_show_all (GTK_WIDGET (priv->layer_tool_stack));
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (priv->layer_tool_stack));
    }

  g_free (layer_key);
  g_object_unref (layer);
}

static GtkWidget *
create_layer_tree_view (HyScanGtkMapKit *kit,
                        GtkTreeModel    *tree_model)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *visible_column, *track_column;
  GtkWidget *tree_view;
  GtkTreeSelection *selection;

  /* Галочка с признаком видимости галса. */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (on_enable_layer), kit);
  visible_column = gtk_tree_view_column_new_with_attributes (_("Visible"), renderer,
                                                             "active", LAYER_VISIBLE_COLUMN, NULL);

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  track_column = gtk_tree_view_column_new_with_attributes (_("Layer"), renderer,
                                                           "text", LAYER_TITLE_COLUMN, NULL);

  tree_view = gtk_tree_view_new_with_model (tree_model);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), visible_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), track_column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  g_signal_connect (selection, "changed", G_CALLBACK (layer_changed), kit);

  return tree_view;
}

/* Кнопки управления виджетом. */
static GtkWidget *
create_control_box (HyScanGtkMapKit *kit,
                    const gchar     *profile_dir)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *ctrl_box;
  GtkWidget *ctrl_widget;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  /* Выпадающий список с профилями. */
  gtk_container_add (GTK_CONTAINER (ctrl_box), create_profile_switch (HYSCAN_GTK_MAP (kit->map), profile_dir));

  /* Блокировка редактирования. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new (_("Enable editing")));

    ctrl_widget = gtk_switch_new ();
    gtk_switch_set_active (GTK_SWITCH (ctrl_widget),
                           hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (kit->map)));
    gtk_container_add (GTK_CONTAINER (ctrl_box), ctrl_widget);
    g_signal_connect (ctrl_widget, "notify::active", G_CALLBACK (on_editable_switch), kit->map);
  }

  gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  /* Переключение слоёв. */
  {
    gtk_container_add (GTK_CONTAINER (ctrl_box), gtk_label_new (_("Layers")));
    gtk_container_add (GTK_CONTAINER (ctrl_box), create_layer_tree_view (kit, GTK_TREE_MODEL (priv->layer_store)));
  }

  /* Контейнер для панели инструментов каждого слоя. */
  {
    GtkWidget *layer_tools;

    priv->layer_tool_stack = gtk_stack_new ();
    gtk_stack_set_homogeneous (GTK_STACK (priv->layer_tool_stack), FALSE);
    gtk_container_add (GTK_CONTAINER (ctrl_box), GTK_WIDGET (priv->layer_tool_stack));

    /* Устаналиваем виджеты с инструментами для каждого слоя. */
    layer_tools = create_ruler_toolbox (priv->ruler, _("Remove ruler"));
    g_object_set_data (G_OBJECT (priv->ruler), "toolbox-cb", "ruler");
    gtk_stack_add_titled (GTK_STACK (priv->layer_tool_stack), layer_tools, "ruler", "Ruler");

    layer_tools = create_ruler_toolbox (priv->pin_layer, _("Remove all pins"));
    g_object_set_data (G_OBJECT (priv->pin_layer), "toolbox-cb", "pin");
    gtk_stack_add_titled (GTK_STACK (priv->layer_tool_stack), layer_tools, "pin", "Pin");
  }

  /* Стек с инструментами. */
  {
    GtkWidget *stack_switcher, *stack, *stack_box;

    stack = gtk_stack_new ();

    stack_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    create_nav_input (kit, GTK_CONTAINER (stack_box));
    gtk_stack_add_titled (GTK_STACK (stack), stack_box, "navigate", _("Go to"));

    stack_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    create_preloader (kit, GTK_CONTAINER (stack_box));
    gtk_stack_add_titled (GTK_STACK (stack), stack_box, "cache", _("Offline"));

    stack_switcher = gtk_stack_switcher_new ();
    gtk_widget_set_margin_top (stack_switcher, 5);
    gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (stack_switcher), GTK_STACK (stack));

    gtk_container_add (GTK_CONTAINER (ctrl_box), stack_switcher);
    gtk_container_add (GTK_CONTAINER (ctrl_box), stack);
  }

  return ctrl_box;
}

/* Создаёт слои, если соответствующие им модели данных доступны. */
static void
create_layers (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  priv->map_grid = hyscan_gtk_map_grid_new ();
  priv->ruler = hyscan_gtk_map_ruler_new ();
  priv->pin_layer = hyscan_gtk_map_pin_layer_new ();

  /* Слой планировщика миссий. */
  if (priv->planner != NULL)
    priv->planner_layer = hyscan_gtk_map_planner_new (priv->planner);

  /* Слой с траекторией движения судна. */
  if (priv->nav_model != NULL)
    priv->way_layer = hyscan_gtk_map_way_layer_new (priv->nav_model);

  /* Слой с галсами. */
  if (priv->db != NULL && priv->list_model != NULL)
    priv->track_layer = hyscan_gtk_map_track_layer_new (priv->db, priv->project_name, priv->list_model, priv->cache);

  /* Слой с метками. */
  if (priv->ml_model != NULL)
    priv->wfmark_layer = hyscan_gtk_map_wfmark_layer_new (priv->ml_model);

  if (priv->mark_geo_model != NULL)
    priv->geomark_layer = hyscan_gtk_map_geomark_layer_new (priv->mark_geo_model);
}

/* Создает модели данных. */
static void
hyscan_gtk_map_kit_model_create (HyScanGtkMapKit *kit,
                                 HyScanDB        *db)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  priv->cache = HYSCAN_CACHE (hyscan_cached_new (300));
  if (db != NULL)
    {
      priv->db = g_object_ref (db);
      priv->db_info = hyscan_db_info_new (db);
      priv->mark_model = hyscan_mark_model_new (HYSCAN_MARK_WATERFALL);
      priv->mark_geo_model = hyscan_mark_model_new (HYSCAN_MARK_GEO);
      priv->list_model = hyscan_list_model_new ();
      priv->ml_model = hyscan_mark_loc_model_new (db, priv->cache);
    }

  priv->nav_model = hyscan_navigation_model_new ();
  priv->planner = hyscan_planner_new ();

  kit->map = create_map (kit);
  create_layers (kit);

  priv->layer_store = gtk_list_store_new (4,
                                          G_TYPE_BOOLEAN,        /* LAYER_VISIBLE_COLUMN */
                                          G_TYPE_STRING,         /* LAYER_KEY_COLUMN   */
                                          G_TYPE_STRING,         /* LAYER_TITLE_COLUMN   */
                                          HYSCAN_TYPE_GTK_LAYER  /* LAYER_COLUMN         */);

  add_layer_row (kit, priv->planner_layer, "planner", _("Planner"));
  add_layer_row (kit, priv->way_layer,     "way",     _("Navigation"));
  add_layer_row (kit, priv->track_layer,   "track",   _("Tracks"));
  add_layer_row (kit, priv->wfmark_layer,  "wfmark",  _("Waterfall Marks"));
  add_layer_row (kit, priv->geomark_layer, "wfmark",  _("Geo Marks"));
  add_layer_row (kit, priv->ruler,         "ruler",   _("Ruler"));
  add_layer_row (kit, priv->pin_layer,     "pin",     _("Pin"));
  add_layer_row (kit, priv->map_grid,      "grid",    _("Grid"));
}

static void
hyscan_gtk_map_kit_view_create (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  kit->navigation = create_navigation_box (kit);
  kit->control    = create_control_box (kit, priv->profile_dir);
  kit->status_bar = create_status_bar (kit);
}

static void
hyscan_gtk_map_kit_model_init (HyScanGtkMapKit   *kit,
                               HyScanGeoGeodetic *center,
                               HyScanSensor      *sensor,
                               const gchar       *sensor_name,
                               const gchar       *planner_ini,
                               gdouble            delay_time)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if (priv->db != NULL)
    {
      hyscan_db_info_set_project (priv->db_info, priv->project_name);
      hyscan_mark_model_set_project (priv->mark_model, priv->db, priv->project_name);
      hyscan_mark_model_set_project (priv->mark_geo_model, priv->db, priv->project_name);
      hyscan_mark_loc_model_set_project (priv->ml_model, priv->project_name);
    }

  if (priv->nav_model != NULL)
    {
      hyscan_navigation_model_set_sensor (priv->nav_model, sensor);
      hyscan_navigation_model_set_sensor_name (priv->nav_model, sensor_name);
      hyscan_navigation_model_set_delay (priv->nav_model, delay_time);
    }

  if (planner_ini != NULL)
    {
      hyscan_planner_load_ini (priv->planner, planner_ini);
      /* Автосохранение. */
      g_signal_connect (priv->planner, "changed", G_CALLBACK (hyscan_planner_save_ini), (gpointer) planner_ini);
    }

  {
    priv->center = *center;
    hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), priv->center);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->lat_spin), priv->center.lat);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->lon_spin), priv->center.lon);
  }
}

/**
 * hyscan_gtk_map_kit_new_map:
 * @center: центр карты
 * @db: указатель на #HyScanDB
 * @project_name: имя проекта
 * @profile_dir: папка с профилями карты (примеры файлов-профилей в папке hyscangui/misc)
 * @sensor: датчик GPS-ресивера
 * @sensor_name: имя датчика
 * @planner_ini: ini-файл с запланированной миссией
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
                        const gchar       *sensor_name,
                        const gchar       *planner_ini,
                        gdouble            delay_time)
{
  HyScanGtkMapKit *kit;

  kit = g_new0 (HyScanGtkMapKit, 1);
  kit->priv = g_new0 (HyScanGtkMapKitPrivate, 1);

  kit->priv->project_name = g_strdup (project_name);
  kit->priv->profile_dir = g_strdup (profile_dir);

  hyscan_gtk_map_kit_model_create (kit, db);
  hyscan_gtk_map_kit_view_create (kit);
  hyscan_gtk_map_kit_model_init (kit, center, sensor, sensor_name, planner_ini, delay_time);

  return kit;
}

/**
 * hyscan_gtk_map_kit_free:
 * @kit: указатель на структуру #HyScanGtkMapKit
 */
void
hyscan_gtk_map_kit_free (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_free (priv->profile_dir);
  g_free (priv->project_name);
  g_clear_object (&priv->planner);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->ml_model);
  g_clear_object (&priv->db);
  g_clear_object (&priv->db_info);
  g_clear_object (&priv->mark_model);
  g_clear_object (&priv->mark_geo_model);
  g_clear_object (&priv->list_model);
  g_clear_object (&priv->layer_store);
  g_clear_object (&priv->track_store);
  g_clear_object (&priv->mark_store);
  g_free (priv);

  g_free (kit);
}

#include "hyscan-gtk-map-kit.h"
#include <hyscan-gtk-map-base.h>
#include <hyscan-gtk-map-control.h>
#include <hyscan-gtk-map-ruler.h>
#include <hyscan-gtk-map-grid.h>
#include <hyscan-gtk-map-pin.h>
#include <hyscan-gtk-map-nav.h>
#include <hyscan-gtk-map-track.h>
#include <hyscan-cached.h>
#include <hyscan-profile-map.h>
#include <hyscan-map-tile-loader.h>
#include <hyscan-db-info.h>
#include <hyscan-gtk-map-scale.h>
#include <hyscan-gtk-param-list.h>
#include <hyscan-gtk-map-wfmark.h>
#include <hyscan-object-model.h>
#include <hyscan-planner-model.h>
#include <hyscan-gtk-map-geomark.h>

#include <glib/gi18n-lib.h>
#include <hyscan-gtk-map-planner.h>
#include <hyscan-gtk-planner-status.h>
#include <hyscan-gtk-planner-origin.h>
#include <hyscan-gtk-layer-list.h>
#include <hyscan-gtk-planner-zeditor.h>
#include <hyscan-planner-selection.h>
#include <hyscan-gtk-map-steer.h>
#include <hyscan-nav-model.h>

#define DEFAULT_PROFILE_NAME "default"    /* Имя профиля карты по умолчанию. */
#define PRELOAD_STATE_DONE   1000         /* Статус кэширования тайлов 0 "Загрузка завершена". */

#define PLANNER_TOOLS_MODE    "planner-mode"
#define PLANNER_TAB_ZEDITOR   "zeditor"
#define PLANNER_TAB_ORIGIN    "origin"
#define PLANNER_TAB_STATUS    "status"

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
  MARK_MTIME_SORT_COLUMN,
  MARK_TYPE_COLUMN,
  MARK_TYPE_NAME_COLUMN,
};

struct _HyScanGtkMapKitPrivate
{
  /* Модели данных. */
  HyScanDBInfo            *db_info;          /* Доступ к данным БД. */
  HyScanObjectModel       *mark_model;       /* Модель меток водопада. */
  HyScanPlannerModel      *planner_model;    /* Модель объектов планировщика. */
  HyScanPlannerSelection  *planner_selection;/* Модель выбранных объектов планировщика. */
  HyScanObjectModel       *mark_geo_model;   /* Модель геометок. */
  HyScanMarkLocModel      *ml_model;         /* Модель местоположения меток водопада. */
  HyScanUnits             *units;
  HyScanDB                *db;
  HyScanCache             *cache;
  HyScanNavModel          *nav_model;
  gchar                   *project_name;

  GHashTable              *profiles;         /* Хэш-таблица профилей карты. */
  gchar                   *profile_active;   /* Ключ активного профиля. */
  gboolean                 profile_offline;  /* Признак оффлайн-профиля карты. */
  gchar                   *tile_cache_dir;   /* Путь к директории, в которой хранятся тайлы. */

  HyScanGeoPoint           center;           /* Географические координаты для виджета навигации. */

  /* Слои. */
  HyScanGtkLayer          *base_layer;       /* Слой подложки. */
  HyScanGtkLayer          *track_layer;      /* Слой просмотра галсов. */
  HyScanGtkLayer          *wfmark_layer;     /* Слой с метками водопада. */
  HyScanGtkLayer          *geomark_layer;    /* Слой с метками водопада. */
  HyScanGtkLayer          *map_grid;         /* Слой координатной сетки. */
  HyScanGtkLayer          *ruler;            /* Слой линейки. */
  HyScanGtkLayer          *pin_layer;        /* Слой географических отметок. */
  HyScanGtkLayer          *way_layer;        /* Слой текущего положения по GPS-датчику. */
  HyScanGtkLayer          *planner_layer;    /* Слой планировщика. */

  /* Виджеты. */
  GtkWidget               *profiles_box;     /* Выпадающий список профилей карты. */
  GtkWidget               *planner_stack;    /* GtkStack с виджетами настроек планировщика. */
  GtkButton               *preload_button;   /* Кнопка загрузки тайлов. */
  GtkProgressBar          *preload_progress; /* Индикатор загрузки тайлов. */
  GtkWidget               *steer_box;        /* Контейнер для виджета навигации по отклонениям от плана. */
  HyScanSonarRecorder     *recorder;         /* Управление записью галса. */

  HyScanMapTileLoader     *loader;
  guint                    preload_tag;      /* Timeout-функция обновления виджета preload_progress. */
  gint                     preload_state;    /* Статус загрузки тайлов.
                                              * < 0                      - загрузка не началась,
                                              * 0 - PRELOAD_STATE_DONE-1 - прогресс загрузки,
                                              * >= PRELOAD_STATE_DONE    - загрузка завершена, не удалось загрузить
                                              *                            PRELOAD_STATE_DONE - preload_state тайлов */

  GtkTreeView             *track_tree;       /* GtkTreeView со списком галсов. */
  GtkListStore            *track_store;      /* Модель данных для track_tree. */
  GtkMenu                 *track_menu;       /* Контекстное меню галса (по правой кнопке). */
  GtkWidget               *track_menu_find;  /* Пункт меню поиска галса на карте. */

  GtkTreeView             *mark_tree;       /* GtkTreeView со списком галсов. */
  GtkListStore            *mark_store;      /* Модель данных для track_tree. */

  GtkWidget               *locate_button;   /* Кнопка для определения текущего местоположения. */
  GtkWidget               *lat_spin;        /* Поля для ввода широты. */
  GtkWidget               *lon_spin;        /* Поля для ввода долготы. */

  GtkWidget               *stbar_offline;   /* Статусбар оффлайн. */
  GtkWidget               *stbar_coord;     /* Статусбар координат. */
  GtkWidget               *layer_list;
};

static void     hyscan_gtk_map_kit_set_tracks   (HyScanGtkMapKit      *kit,
                                                 gchar               **tracks);
static gchar ** hyscan_gtk_map_kit_get_tracks   (HyScanGtkMapKit      *kit);
static void     hyscan_gtk_map_kit_track_enable (HyScanGtkMapKit      *kit,
                                                 const gchar          *track_name,
                                                 gboolean              enable);
#if !GLIB_CHECK_VERSION (2, 44, 0)
static gboolean g_strv_contains               (const gchar * const  *strv,
                                               const gchar          *str);
#endif

static GtkWidget *
create_map (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkMap *map;
  gdouble *scales;
  gint scales_len;
  HyScanGtkLayer *control, *scale;

  map = HYSCAN_GTK_MAP (hyscan_gtk_map_new (priv->center));

  /* Устанавливаем допустимые масштабы. */
  scales = hyscan_gtk_map_create_scales2 (1.0 / 1000, HYSCAN_GTK_MAP_EQUATOR_LENGTH / 1000, 4, &scales_len);
  hyscan_gtk_map_set_scales_meter (map, scales, scales_len);
  g_free (scales);

  /* По умолчанию добавляем слой управления картой и масштаб. */
  control = hyscan_gtk_map_control_new ();
  scale = hyscan_gtk_map_scale_new ();
  hyscan_gtk_layer_set_visible (scale, TRUE);
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), control, "control");
  hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (map), scale,   "scale");

  /* Чтобы виджет карты занял всё доступное место. */
  gtk_widget_set_hexpand (GTK_WIDGET (map), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (map), TRUE);

  return GTK_WIDGET (map);
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

  if (profiles_path == NULL)
    goto exit;

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

      g_dir_close (dir);
    }
  else
    {
      g_warning ("HyScanGtkMapKit: %s", error->message);
      g_error_free (error);
    }

exit:
  profiles = g_realloc (profiles, ++nprofiles * sizeof (gchar **));
  profiles[nprofiles - 1] = NULL;

  return profiles;
}

static void
combo_box_update (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (priv->profiles_box);

  GHashTableIter iter;
  const gchar *key;
  HyScanProfileMap *profile;

  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (combo));

  g_hash_table_iter_init (&iter, priv->profiles);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &profile))
    {
      const gchar *title;

      title = hyscan_profile_get_name (HYSCAN_PROFILE (profile));
      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), key, title);
    }

  if (priv->profile_active != NULL)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), priv->profile_active);
}

static void
add_profile (HyScanGtkMapKit  *kit,
             const gchar      *profile_name,
             HyScanProfileMap *profile)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_hash_table_insert (priv->profiles, g_strdup (profile_name), g_object_ref (profile));
  combo_box_update (kit);
}

/* Переключает активный профиль карты. */
static void
on_profile_change (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkComboBox *combo = GTK_COMBO_BOX (priv->profiles_box);
  const gchar *active_id;

  active_id = gtk_combo_box_get_active_id (combo);

  if (active_id != NULL)
    hyscan_gtk_map_kit_set_profile_name (kit, active_id);
}

static void
on_editable_switch (GtkSwitch               *widget,
                    GParamSpec              *pspec,
                    HyScanGtkLayerContainer *container)
{
  hyscan_gtk_layer_container_set_changes_allowed (container, !gtk_switch_get_active (widget));
}

/* Добавляет @layer на карту и в список слоёв. */
static void
add_layer_row (HyScanGtkMapKit *kit,
               HyScanGtkLayer  *layer,
               gboolean         visible,
               const gchar     *key,
               const gchar     *title)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  /* Регистрируем слой в карте. */
  if (layer != NULL)
    {
      hyscan_gtk_layer_set_visible (layer, visible);
      hyscan_gtk_layer_container_add (HYSCAN_GTK_LAYER_CONTAINER (kit->map), layer, key);
    }
  hyscan_gtk_layer_list_add (HYSCAN_GTK_LAYER_LIST (priv->layer_list), layer, key, title);
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

  /* Устанавливаем видимость галса. */
  hyscan_gtk_map_kit_track_enable (kit, track_name, !active);

  g_free (track_name);
}

/* Обновляет список активных галсов при измении модели. */
static void
track_tree_view_visible_changed (HyScanGtkMapKit     *kit,
                                 const gchar *const *visible_tracks)
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

     active = visible_tracks != NULL && g_strv_contains (visible_tracks, track_name);
     gtk_list_store_set (priv->track_store, &iter, VISIBLE_COLUMN, active, -1);

     g_free (track_name);

     valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->track_store), &iter);
   }
}

static gchar *
track_tree_view_get_selected (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeSelection *selection;
  GList *list;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);
  gchar *track_name = NULL;

  selection = gtk_tree_view_get_selection (priv->track_tree);
  list = gtk_tree_selection_get_selected_rows (selection, &tree_model);
  if (list != NULL)
    {
      GtkTreePath *path = list->data;
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->track_store), &iter, path))
          gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
    }
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

  return track_name;
}

static void
on_locate_track_clicked (GtkButton *button,
                         gpointer   user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gchar *track_name;

  track_name = track_tree_view_get_selected (kit);

  if (track_name != NULL)
    hyscan_gtk_map_track_view (HYSCAN_GTK_MAP_TRACK (priv->track_layer), track_name, FALSE, NULL);

  g_free (track_name);
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
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 300);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (priv->track_tree));
  if (GTK_IS_WINDOW (toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));

  g_signal_connect_swapped (window, "destroy", G_CALLBACK (gtk_widget_destroy), window);

  /* Виджет отображения параметров. */
  frontend = hyscan_gtk_param_list_new (param, "/", FALSE);
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
  g_object_set (grid, "margin", 12, NULL);

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
          HyScanGtkMapTrackItem *track;

          gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
          track = hyscan_gtk_map_track_lookup (HYSCAN_GTK_MAP_TRACK (priv->track_layer), track_name);

          window = create_param_settings_window (kit, _("Track settings"), HYSCAN_PARAM (track));
          gtk_widget_show_all (window);

          g_object_unref (track);
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
  gchar **selected_tracks;

  HyScanGtkMapKitPrivate *priv = kit->priv;

  null_path = gtk_tree_path_new ();
  gtk_tree_view_set_cursor (priv->track_tree, null_path, NULL, FALSE);
  gtk_tree_path_free (null_path);

  gtk_list_store_clear (priv->track_store);

  tracks = hyscan_db_info_get_tracks (db_info);
  selected_tracks = hyscan_gtk_map_kit_get_tracks (kit);
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
      visible = g_strv_contains ((const gchar *const *) selected_tracks, track_info->name);

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

  g_strfreev (selected_tracks);
  g_hash_table_unref (tracks);
}

static gboolean
on_button_press_event (GtkTreeView     *tree_view,
                       GdkEventButton  *event_button,
                       HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if ((event_button->type == GDK_BUTTON_PRESS) && (event_button->button == GDK_BUTTON_SECONDARY))
    gtk_menu_popup_at_pointer (priv->track_menu, (const GdkEvent *) event_button);

  return FALSE;
}

/* Отмечает все галочки в списке галсов. */
static void
track_view_select_all (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gboolean valid;
  GtkTreeIter iter;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->track_store), &iter);
  while (valid)
    {
      gchar *track_name;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
      hyscan_gtk_map_kit_track_enable (kit, track_name, TRUE);
      g_free (track_name);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->track_store), &iter);
    }
}

/* Клик по заголовку с галочкой в таблице галсов. */
static void
on_enable_all_tracks (HyScanGtkMapKit *kit)
{
  gchar **selected;

  selected = hyscan_gtk_map_kit_get_tracks (kit);
  if (g_strv_length (selected) == 0)
    track_view_select_all (kit);
  else
    hyscan_gtk_map_kit_set_tracks (kit, NULL);

  g_strfreev (selected);
}

static GtkTreeView *
create_track_tree_view (HyScanGtkMapKit *kit,
                        GtkTreeModel    *tree_model)
{
  GtkWidget *image;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *visible_column, *track_column, *date_column;
  GtkTreeView *tree_view;

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  track_column = gtk_tree_view_column_new_with_attributes (_("Track"), renderer,
                                                           "text", TRACK_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (track_column, TRACK_COLUMN);
  gtk_tree_view_column_set_expand (track_column, TRUE);
  gtk_tree_view_column_set_resizable (track_column, TRUE);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  /* Дата создания галса. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer,
                                                          "text", DATE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, DATE_SORT_COLUMN);

  /* Галочка с признаком видимости галса. */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (on_enable_track), kit);
  visible_column = gtk_tree_view_column_new_with_attributes (_("Show"), renderer,
                                                             "active", VISIBLE_COLUMN, NULL);
  image = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
  gtk_tree_view_column_set_clickable (visible_column, TRUE);
  g_signal_connect_swapped (visible_column, "clicked", G_CALLBACK (on_enable_all_tracks), kit);
  gtk_widget_show (image);
  gtk_tree_view_column_set_widget (visible_column, image);

  tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (tree_model));
  gtk_tree_view_set_search_column (tree_view, TRACK_COLUMN);
  gtk_tree_view_append_column (tree_view, track_column);
  gtk_tree_view_append_column (tree_view, date_column);
  gtk_tree_view_append_column (tree_view, visible_column);

  gtk_tree_view_set_grid_lines (tree_view, GTK_TREE_VIEW_GRID_LINES_BOTH);

  return tree_view;
}

static GtkTreeView *
create_mark_tree_view (HyScanGtkMapKit *kit,
                       GtkTreeModel    *tree_model)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *type_column, *name_column, *date_column;
  GtkTreeView *tree_view;

  /* Тип метки. */
  renderer = gtk_cell_renderer_text_new ();
  type_column = gtk_tree_view_column_new_with_attributes ("", renderer,
                                                          "text", MARK_TYPE_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (type_column, MARK_TYPE_COLUMN);

  /* Название метки. */
  renderer = gtk_cell_renderer_text_new ();
  name_column = gtk_tree_view_column_new_with_attributes (_("Name"), renderer,
                                                          "text", MARK_NAME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (name_column, MARK_NAME_COLUMN);
  gtk_tree_view_column_set_expand (name_column, TRUE);
  gtk_tree_view_column_set_expand (name_column, TRUE);
  gtk_tree_view_column_set_resizable (name_column, TRUE);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  /* Время последнего изменения метки. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer,
                                                          "text", MARK_MTIME_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, MARK_MTIME_SORT_COLUMN);

  tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (tree_model));
  gtk_tree_view_set_search_column (tree_view, MARK_NAME_COLUMN);
  gtk_tree_view_append_column (tree_view, name_column);
  gtk_tree_view_append_column (tree_view, date_column);
  gtk_tree_view_append_column (tree_view, type_column);

  gtk_tree_view_set_grid_lines (tree_view, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

  return tree_view;
}

static void
on_track_change (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkMapTrackItem *track_item = NULL;
  gchar *track_name;
  gboolean has_nmea;

  track_name = track_tree_view_get_selected (kit);

  if (track_name != NULL)
    track_item = hyscan_gtk_map_track_lookup (HYSCAN_GTK_MAP_TRACK (priv->track_layer), track_name);

  has_nmea = (track_item != NULL) && hyscan_gtk_map_track_item_has_nmea (track_item);
  gtk_widget_set_sensitive (priv->track_menu_find, has_nmea);

  g_clear_object (&track_item);
  g_free (track_name);
}

static void
create_track_menu (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *menu_item_settings;

  priv->track_menu = GTK_MENU (gtk_menu_new ());
  priv->track_menu_find = gtk_menu_item_new_with_label (_("Find track on the map"));
  g_signal_connect (priv->track_menu_find, "activate", G_CALLBACK (on_locate_track_clicked), kit);
  gtk_widget_show (priv->track_menu_find);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->track_menu), priv->track_menu_find);
  gtk_widget_set_sensitive (priv->track_menu_find, FALSE);

  menu_item_settings = gtk_menu_item_new_with_label (_("Track settings"));
  g_signal_connect (menu_item_settings, "activate", G_CALLBACK (on_configure_track_clicked), kit);
  gtk_widget_show (menu_item_settings);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->track_menu), menu_item_settings);

  /* Обработчик определяет доступные пункт при выборе галса. */
  g_signal_connect_swapped (gtk_tree_view_get_selection (priv->track_tree), "changed", G_CALLBACK (on_track_change), kit);
}

static void
on_marks_activated (GtkTreeView        *treeview,
                    GtkTreePath        *path,
                    GtkTreeViewColumn  *col,
                    gpointer            user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model (treeview);
  if (gtk_tree_model_get_iter (model, &iter, path))
  {
    gchar *mark_id;
    GType mark_type;

    gtk_tree_model_get (model, &iter, MARK_ID_COLUMN, &mark_id, MARK_TYPE_COLUMN, &mark_type, -1);
    if (mark_type == HYSCAN_TYPE_MARK_WATERFALL && priv->wfmark_layer != NULL)
      hyscan_gtk_map_wfmark_mark_view (HYSCAN_GTK_MAP_WFMARK (priv->wfmark_layer), mark_id, FALSE);
    else if (mark_type == HYSCAN_TYPE_MARK_GEO && priv->geomark_layer != NULL)
      hyscan_gtk_map_geomark_mark_view (HYSCAN_GTK_MAP_GEOMARK (priv->geomark_layer), mark_id, FALSE);

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
    hyscan_gtk_map_track_view (HYSCAN_GTK_MAP_TRACK (priv->track_layer), track_name, FALSE, NULL);

    g_free (track_name);
  }
}

static void
list_store_insert (HyScanGtkMapKit   *kit,
                   HyScanObjectModel *model,
                   gchar             *selected_id)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  GtkTreeIter tree_iter;
  GHashTable *marks;
  GHashTableIter hash_iter;

  HyScanMark *mark;
  gchar *mark_id;

  marks = hyscan_object_model_get (model);
  if (marks == NULL)
    return;

  g_hash_table_iter_init (&hash_iter, marks);

  while (g_hash_table_iter_next (&hash_iter, (gpointer *) &mark_id, (gpointer *) &mark))
    {
      GDateTime *local;
      gchar *time_str;
      gchar *type_name;

      if (mark->type == HYSCAN_TYPE_MARK_WATERFALL && ((HyScanMarkWaterfall *) mark)->track == NULL)
        continue;

      /* Добавляем в список меток. */
      local = g_date_time_new_from_unix_local (mark->mtime / 1000000);
      time_str = g_date_time_format (local, "%d.%m %H:%M");

      if (mark->type == HYSCAN_TYPE_MARK_WATERFALL)
        type_name = "W";
      else if (mark->type == HYSCAN_TYPE_MARK_GEO)
        type_name = "G";
      else
        type_name = "?";

      gtk_list_store_append (priv->mark_store, &tree_iter);
      gtk_list_store_set (priv->mark_store, &tree_iter,
                          MARK_ID_COLUMN, mark_id,
                          MARK_NAME_COLUMN, mark->name,
                          MARK_MTIME_COLUMN, time_str,
                          MARK_MTIME_SORT_COLUMN, mark->mtime,
                          MARK_TYPE_COLUMN, mark->type,
                          MARK_TYPE_NAME_COLUMN, type_name,
                          -1);

      if (g_strcmp0 (selected_id, mark_id) == 0)
        gtk_tree_selection_select_iter (gtk_tree_view_get_selection (priv->mark_tree), &tree_iter);

      g_free (time_str);
      g_date_time_unref (local);
    }

  g_hash_table_unref (marks);
}

static gchar *
mark_get_selected_id (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  gchar *mark_id;

  selection = gtk_tree_view_get_selection (priv->mark_tree);
  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->mark_store), &iter, MARK_ID_COLUMN, &mark_id, -1);

  return mark_id;
}

static void
on_marks_changed (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gchar *selected_id;


  selected_id = mark_get_selected_id (kit);

  /* Удаляем все строки из mark_store. */
  gtk_list_store_clear (priv->mark_store);

  /* Добавляем метки из всех моделей. */
  if (priv->mark_model != NULL)
    list_store_insert (kit, priv->mark_model, selected_id);
  if (priv->mark_geo_model != NULL)
    list_store_insert (kit, priv->mark_geo_model, selected_id);

  g_free (selected_id);
}

/* Добавляет в панель навигации виджет просмотра меток. */
static void
create_wfmark_toolbox (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *scrolled_window;

  /* Невозможно создать виджет, т.к. */
  if (priv->db == NULL)
    return;

  /* Виджет уже создан. Ничего не делаем. */
  if (priv->mark_tree != NULL)
    return;

  priv->mark_store = gtk_list_store_new (6,
                                         G_TYPE_STRING,  /* MARK_ID_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_NAME_COLUMN   */
                                         G_TYPE_STRING,  /* MARK_MTIME_COLUMN */
                                         G_TYPE_INT64,   /* MARK_MTIME_SORT_COLUMN */
                                         G_TYPE_GTYPE,   /* MARK_TYPE_COLUMN */
                                         G_TYPE_STRING,  /* MARK_TYPE_NAME_COLUMN */
                                         -1);
  priv->mark_tree = create_mark_tree_view (kit, GTK_TREE_MODEL (priv->mark_store));

  g_signal_connect (priv->mark_tree, "row-activated", G_CALLBACK (on_marks_activated), kit);

  /* Область прокрутки со списком меток. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scrolled_window, FALSE);
  gtk_widget_set_vexpand (scrolled_window, FALSE);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled_window), 150);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 120);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->mark_tree));

  /* Помещаем в панель навигации. */
  gtk_box_pack_start (GTK_BOX (kit->navigation), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (kit->navigation), scrolled_window, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (kit->navigation), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
}

/* Выбор галсов проекта. */
static void
create_track_box (HyScanGtkMapKit *kit,
                  GtkBox          *box)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *scrolled_window;

  if (priv->db == NULL)
    return;

  priv->track_store = gtk_list_store_new (4,
                                          G_TYPE_BOOLEAN, /* VISIBLE_COLUMN   */
                                          G_TYPE_INT64,   /* DATE_SORT_COLUMN */
                                          G_TYPE_STRING,  /* TRACK_COLUMN     */
                                          G_TYPE_STRING   /* DATE_COLUMN      */);
  priv->track_tree = create_track_tree_view (kit, GTK_TREE_MODEL (priv->track_store));

  create_track_menu (kit);
  gtk_menu_attach_to_widget (priv->track_menu, GTK_WIDGET (priv->track_tree), NULL);

  g_signal_connect_swapped (priv->track_tree, "destroy", G_CALLBACK (gtk_widget_destroy), priv->track_menu);
  g_signal_connect (priv->track_tree, "button-press-event", G_CALLBACK (on_button_press_event), kit);
  g_signal_connect (priv->track_tree, "row-activated", G_CALLBACK (on_track_activated), kit);
  g_signal_connect (priv->db_info, "tracks-changed", G_CALLBACK (tracks_changed), kit);

  /* Область прокрутки со списком галсов. */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (scrolled_window, FALSE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  // gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), 200);
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (priv->track_tree));


  /* Помещаем в панель навигации. */
  gtk_box_pack_start (box, scrolled_window, TRUE, TRUE, 0);
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
  HyScanNavStateData data;

  if (hyscan_nav_state_get (HYSCAN_NAV_STATE (priv->nav_model), &data, NULL))
    hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), data.coord);
}

static void
on_move_to_click (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  hyscan_gtk_map_move_to (HYSCAN_GTK_MAP (kit->map), priv->center);
}

static gboolean
on_motion_show_coords (HyScanGtkMap    *map,
                       GdkEventMotion  *event,
                       HyScanGtkMapKit *kit)
{
  HyScanGeoPoint geo;
  gchar text[255];

  hyscan_gtk_map_point_to_geo (map, &geo, event->x, event->y);
  g_snprintf (text, sizeof (text), "%.6f° %.6f°", geo.lat, geo.lon);
  gtk_statusbar_push (GTK_STATUSBAR (kit->priv->stbar_coord), 0, text);

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
  HyScanMapTileSource *source;
  gdouble from_x, to_x, from_y, to_y;
  GThread *thread;

  layer = hyscan_gtk_layer_container_lookup (HYSCAN_GTK_LAYER_CONTAINER (kit->map), HYSCAN_PROFILE_MAP_BASE_ID);
  g_return_if_fail (HYSCAN_IS_GTK_MAP_BASE (layer));

  source = hyscan_gtk_map_base_get_source (HYSCAN_GTK_MAP_BASE (layer));
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
  GtkWidget *ctrl_widget;

  ctrl_widget = gtk_button_new_from_icon_name ("user-trash-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label (GTK_BUTTON (ctrl_widget), label);
  g_signal_connect_swapped (ctrl_widget, "clicked", G_CALLBACK (hyscan_gtk_map_pin_clear), layer);

  return ctrl_widget;
}

static void
track_layer_draw_change (GtkToggleButton   *button,
                         GParamSpec        *pspec,
                         HyScanGtkMapTrack *track_layer)
{
  HyScanGtkMapTrackDrawType draw_type;

  if (!gtk_toggle_button_get_active (button))
    return;

  draw_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "draw-type"));
  hyscan_gtk_map_track_set_draw_type (track_layer, draw_type);
}

/* Создаёт панель инструментов для слоя булавок и линейки. */
static GtkWidget *
create_track_toolbox (HyScanGtkMapKit *kit)
{
  GtkWidget *bar, *beam, *box;

  bar = gtk_radio_button_new_with_label_from_widget (NULL, _("Distance bars"));
  g_object_set_data (G_OBJECT (bar), "draw-type", GINT_TO_POINTER (HYSCAN_GTK_MAP_TRACK_BAR));
  g_signal_connect (bar, "notify::active", G_CALLBACK (track_layer_draw_change), kit->priv->track_layer);

  beam = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (bar), _("Fill beams"));
  g_object_set_data (G_OBJECT (beam), "draw-type", GINT_TO_POINTER (HYSCAN_GTK_MAP_TRACK_BEAM));
  g_signal_connect (beam, "notify::active", G_CALLBACK (track_layer_draw_change), kit->priv->track_layer);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), bar, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), beam, FALSE, TRUE, 0);

  return box;
}

static void
hyscan_gtk_map_planner_mode_changed (GtkToggleButton *togglebutton,
                                     gpointer         user_data)
{
  HyScanGtkMapKit *kit = user_data;
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkMapPlanner *planner = HYSCAN_GTK_MAP_PLANNER (kit->priv->planner_layer);
  HyScanGtkMapPlannerMode mode;
  const gchar *child_name;

  if (!gtk_toggle_button_get_active (togglebutton))
    return;

  mode = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (togglebutton), PLANNER_TOOLS_MODE));

  if (mode == HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN)
    child_name = PLANNER_TAB_ORIGIN;
  else if (mode == HYSCAN_GTK_MAP_PLANNER_MODE_ZONE)
    child_name = PLANNER_TAB_ZEDITOR;
  else
    child_name = PLANNER_TAB_STATUS;

  hyscan_gtk_map_planner_set_mode (planner, mode);
  gtk_stack_set_visible_child_name (GTK_STACK (priv->planner_stack), child_name);
}

static GtkWidget*
create_planner_mode_btn (HyScanGtkMapKit         *kit,
                         GtkWidget               *from,
                         const gchar             *icon,
                         const gchar              *tooltip,
                         HyScanGtkMapPlannerMode  mode)
{
  GtkWidget *button;

  button = gtk_radio_button_new_from_widget (GTK_RADIO_BUTTON (from));
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_BUTTON));
  gtk_button_set_image_position (GTK_BUTTON (button), GTK_POS_TOP);
  gtk_button_set_label (GTK_BUTTON (button), tooltip);

  g_object_set_data (G_OBJECT (button), PLANNER_TOOLS_MODE, GUINT_TO_POINTER (mode));
  g_signal_connect (button, "toggled", G_CALLBACK (hyscan_gtk_map_planner_mode_changed), kit);

  return button;
}

static GtkWidget *
create_planner_zeditor (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box, *zeditor, *topo_checkbox;

  zeditor = hyscan_gtk_planner_zeditor_new (priv->planner_model, priv->planner_selection);
  gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (zeditor), GTK_TREE_VIEW_GRID_LINES_BOTH);

  topo_checkbox = gtk_check_button_new_with_label(_("Topo coordinates (x, y)"));
  g_object_bind_property (topo_checkbox, "active",
                          zeditor, "geodetic", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), topo_checkbox, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), zeditor, TRUE, TRUE, 0);

  return box;
}

/* Создаёт панель инструментов для слоя планировщика. */
static GtkWidget *
create_planner_toolbox (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box;
  GtkWidget *tab_switch;
  GtkWidget *origin_label;
  GtkWidget *tab_status, *tab_origin, *tab_zeditor;
  GtkWidget *zone_mode, *track_mode, *origin_mode;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  priv->planner_stack = gtk_stack_new ();
  gtk_stack_set_vhomogeneous (GTK_STACK (priv->planner_stack), FALSE);

  tab_switch = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  track_mode = create_planner_mode_btn (kit, NULL, "document-new-symbolic", _("Track"),
                                        HYSCAN_GTK_MAP_PLANNER_MODE_TRACK);
  zone_mode = create_planner_mode_btn (kit, track_mode, "folder-new-symbolic", _("Zone"),
                                       HYSCAN_GTK_MAP_PLANNER_MODE_ZONE);
  origin_mode = create_planner_mode_btn (kit, zone_mode, "go-home-symbolic", _("Origin"),
                                         HYSCAN_GTK_MAP_PLANNER_MODE_ORIGIN);

  gtk_style_context_add_class (gtk_widget_get_style_context (tab_switch), GTK_STYLE_CLASS_LINKED);
  gtk_box_pack_start (GTK_BOX (tab_switch), track_mode, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_switch), zone_mode, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_switch), origin_mode, TRUE, TRUE, 0);

  tab_zeditor = create_planner_zeditor (kit);
  tab_status = hyscan_gtk_planner_status_new (HYSCAN_GTK_MAP_PLANNER (priv->planner_layer));
  tab_origin = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  origin_label = gtk_label_new (_("Enter parameters of coordinate system or pick it on the map"));
  gtk_label_set_max_width_chars (GTK_LABEL (origin_label), 10);
  gtk_label_set_line_wrap (GTK_LABEL (origin_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (origin_label), 0.0);
  gtk_box_pack_start (GTK_BOX (tab_origin), origin_label, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (tab_origin), hyscan_gtk_planner_origin_new (priv->planner_model), FALSE, TRUE, 0);

  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_status, PLANNER_TAB_STATUS);
  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_zeditor, PLANNER_TAB_ZEDITOR);
  gtk_stack_add_named (GTK_STACK (priv->planner_stack), tab_origin, PLANNER_TAB_ORIGIN);

  gtk_box_pack_start (GTK_BOX (box), tab_switch, TRUE, FALSE, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->planner_stack, TRUE, FALSE, 6);

  return box;
}

/* Список галсов и меток. */
static GtkWidget *
create_navigation_box (HyScanGtkMapKit *kit)
{
  GtkWidget *ctrl_box;

  ctrl_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_end (ctrl_box, 6);

  create_track_box (kit, GTK_BOX (ctrl_box));

  return ctrl_box;
}

/* Текстовые поля для ввода координат. */
static void
create_nav_input (HyScanGtkMapKit *kit,
                  GtkGrid         *grid)
{
  gint t=0;
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *move_button;

  priv->lat_spin = gtk_spin_button_new_with_range (-90.0, 90.0, 0.001);
  g_signal_connect (priv->lat_spin, "notify::value", G_CALLBACK (on_coordinate_change), &priv->center.lat);

  priv->lon_spin = gtk_spin_button_new_with_range (-180.0, 180.0, 0.001);
  g_signal_connect (priv->lon_spin, "notify::value", G_CALLBACK (on_coordinate_change), &priv->center.lon);

  move_button = gtk_button_new_with_label (_("Go"));
  g_signal_connect_swapped (move_button, "clicked", G_CALLBACK (on_move_to_click), kit);

  gtk_grid_attach (grid, gtk_label_new (_("Lat")), 0, ++t, 1, 1);
  gtk_grid_attach (grid, priv->lat_spin, 1, t, 1, 1);
  gtk_grid_attach (grid, gtk_label_new (_("Lon")), 0, ++t, 1, 1);
  gtk_grid_attach (grid, priv->lon_spin, 1, t, 1, 1);
  gtk_grid_attach (grid, move_button, 0, ++t, 2, 1);
}

/* Загрузка тайлов. */
static void
create_preloader (HyScanGtkMapKit *kit,
                  GtkBox          *box)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *description;

  priv->preload_state = -1;

  priv->preload_button = GTK_BUTTON (gtk_button_new_with_label (_("Start download")));
  g_signal_connect (priv->preload_button, "clicked", G_CALLBACK (on_preload_click), kit);

  priv->preload_progress = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
  gtk_progress_bar_set_show_text (priv->preload_progress, TRUE);

  description = gtk_label_new (_("Download visible area of map\nto make it available when offline"));
  gtk_label_set_line_wrap (GTK_LABEL (description), TRUE);

  gtk_box_pack_start (box, description,                         TRUE, TRUE, 0);
  gtk_box_pack_start (box, GTK_WIDGET (priv->preload_progress), TRUE, TRUE, 0);
  gtk_box_pack_start (box, GTK_WIDGET (priv->preload_button),   TRUE, TRUE, 0);
}

/* Текущие координаты. */
static GtkWidget *
create_status_bar (HyScanGtkMapKit *kit)
{
  GtkWidget *statusbar_box;

  statusbar_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  kit->priv->stbar_offline = gtk_statusbar_new ();
  kit->priv->stbar_coord = gtk_statusbar_new ();
  g_object_set (kit->priv->stbar_offline, "margin", 0, NULL);
  g_object_set (kit->priv->stbar_coord, "margin", 0, NULL);
  g_signal_connect (kit->map, "motion-notify-event", G_CALLBACK (on_motion_show_coords), kit);

  gtk_box_pack_start (GTK_BOX (statusbar_box), kit->priv->stbar_offline, FALSE, TRUE, 10);
  gtk_box_pack_start (GTK_BOX (statusbar_box), kit->priv->stbar_coord,   FALSE, TRUE, 10);

  return statusbar_box;
}

static void
hyscan_gtk_map_kit_scale (HyScanGtkMapKit *kit,
                          gint             steps)
{
  HyScanGtkMap *map = HYSCAN_GTK_MAP (kit->map);
  gdouble from_x, to_x, from_y, to_y;
  gint scale_idx;

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (map), &from_x, &to_x, &from_y, &to_y);

  scale_idx = hyscan_gtk_map_get_scale_idx (map, NULL);
  hyscan_gtk_map_set_scale_idx (map, scale_idx + steps, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
}

static void
hyscan_gtk_map_kit_scale_down (HyScanGtkMapKit *kit)
{
  hyscan_gtk_map_kit_scale (kit, 1);
}

static void
hyscan_gtk_map_kit_scale_up (HyScanGtkMapKit *kit)
{
  hyscan_gtk_map_kit_scale (kit, -1);
}

/* Кнопки управления виджетом. */
static GtkWidget *
create_control_box (HyScanGtkMapKit *kit)
{
  gint t = 0;
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *ctrl_box;

  ctrl_box = gtk_grid_new ();
  gtk_grid_set_column_homogeneous (GTK_GRID (ctrl_box), TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (ctrl_box), 5);
  gtk_grid_set_row_spacing (GTK_GRID (ctrl_box), 3);

  /* Выпадающий список с профилями. */
  priv->profiles_box = gtk_combo_box_text_new ();
  g_signal_connect_swapped (priv->profiles_box , "changed", G_CALLBACK (on_profile_change), kit);

  gtk_grid_attach (GTK_GRID (ctrl_box), priv->profiles_box,                             0, ++t, 5, 1);
  gtk_grid_attach (GTK_GRID (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);

  {
    GtkWidget *scale_up, *scale_down;

    scale_down = gtk_button_new_from_icon_name ("list-remove-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign (scale_down, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (scale_down, GTK_ALIGN_CENTER);

    scale_up = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign (scale_up, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (scale_up, GTK_ALIGN_CENTER);

    g_signal_connect_swapped (scale_up,   "clicked", G_CALLBACK (hyscan_gtk_map_kit_scale_up),   kit);
    g_signal_connect_swapped (scale_down, "clicked", G_CALLBACK (hyscan_gtk_map_kit_scale_down), kit);

    gtk_grid_attach (GTK_GRID (ctrl_box), gtk_label_new (_("Scale")),                     0, ++t, 3, 1);
    gtk_grid_attach (GTK_GRID (ctrl_box), scale_down,                                     3, t,   1, 1);
    gtk_grid_attach (GTK_GRID (ctrl_box), scale_up,                                       4, t,   1, 1);
    gtk_grid_attach (GTK_GRID (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);
  }

  /* Слои. */
  {
    GtkWidget *lock_switch;

    lock_switch = gtk_switch_new ();
    gtk_switch_set_active (GTK_SWITCH (lock_switch),
                           !hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (kit->map)));

    g_signal_connect (lock_switch, "notify::active", G_CALLBACK (on_editable_switch), kit->map);

    gtk_grid_attach (GTK_GRID (ctrl_box), gtk_label_new (_("Lock layers")), 0, ++t, 3, 1);
    gtk_grid_attach (GTK_GRID (ctrl_box), lock_switch,                      3,   t, 2, 1);
    gtk_grid_attach (GTK_GRID (ctrl_box), priv->layer_list,                 0, ++t, 5, 1);
  }

  /* Контейнер для панели инструментов каждого слоя. */
  {
    /* Устаналиваем виджеты с инструментами для каждого слоя. */
    hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "ruler",
                                     create_ruler_toolbox (priv->ruler, _("Remove ruler")));
    hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "pin",
                                     create_ruler_toolbox (priv->pin_layer, _("Remove all pins")));
    hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "track",
                                     create_track_toolbox (kit));
  }

  /* Стек с инструментами. */
  {
    GtkWidget *nav_box;
    GtkWidget *stack_switcher, *stack, *stack_box;

    nav_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    stack = gtk_stack_new ();

    stack_box = gtk_grid_new ();
    g_object_set (stack_box,
                  "margin", 6,
                  "row-spacing", 6,
                  "column-spacing", 6,
                  "halign", GTK_ALIGN_CENTER,
                  NULL);
    gtk_grid_set_column_spacing (GTK_GRID (stack_box), 2);
    gtk_grid_set_row_spacing (GTK_GRID (stack_box), 2);
    create_nav_input (kit, GTK_GRID (stack_box));
    gtk_stack_add_titled (GTK_STACK (stack), stack_box, "navigate", _("Go to"));

    stack_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    g_object_set (stack_box,
                  "margin", 6,
                  "halign", GTK_ALIGN_CENTER,
                  NULL);
    create_preloader (kit, GTK_BOX (stack_box));
    gtk_stack_add_titled (GTK_STACK (stack), stack_box, "cache", _("Download"));

    stack_switcher = gtk_stack_switcher_new ();
    gtk_widget_set_halign (stack_switcher, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top (stack_switcher, 5);
    gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (stack_switcher), GTK_STACK (stack));

    gtk_box_pack_start (GTK_BOX (nav_box), stack_switcher, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (nav_box), stack, FALSE, TRUE, 0);
    hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "base", nav_box);

    gtk_grid_attach (GTK_GRID (ctrl_box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, ++t, 5, 1);
  }

  return ctrl_box;
}

/* Создаёт слои, если соответствующие им модели данных доступны. */
static void
create_layers (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  priv->base_layer = hyscan_gtk_map_base_new (priv->cache);
  priv->map_grid = hyscan_gtk_map_grid_new (priv->units);
  priv->ruler = hyscan_gtk_map_ruler_new ();
  priv->pin_layer = hyscan_gtk_map_pin_new ();

  /* Слой с галсами. */
  if (priv->db != NULL)
    priv->track_layer = hyscan_gtk_map_track_new (priv->db, priv->cache);

  priv->layer_list = hyscan_gtk_layer_list_new (HYSCAN_GTK_LAYER_CONTAINER (kit->map));

  add_layer_row (kit, priv->base_layer,  FALSE, "base",   _("Base Map"));
  add_layer_row (kit, priv->track_layer, FALSE, "track",  _("Tracks"));
  add_layer_row (kit, priv->ruler,       TRUE,  "ruler",  _("Ruler"));
  add_layer_row (kit, priv->pin_layer,   TRUE,  "pin",    _("Pin"));
  add_layer_row (kit, priv->map_grid,    TRUE,  "grid",   _("Grid"));
}

/* Создает модели данных. */
static void
hyscan_gtk_map_kit_model_create (HyScanGtkMapKit *kit,
                                 HyScanDB        *db)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  priv->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  priv->cache = HYSCAN_CACHE (hyscan_cached_new (300));
  if (db != NULL)
    {
      priv->db = g_object_ref (db);
      priv->db_info = hyscan_db_info_new (db);
    }

  kit->map = create_map (kit);
  create_layers (kit);
}

static void
hyscan_gtk_map_kit_view_create (HyScanGtkMapKit *kit)
{
  kit->navigation = create_navigation_box (kit);
  kit->control    = create_control_box (kit);
  kit->status_bar = create_status_bar (kit);
}

static void
hyscan_gtk_map_kit_model_init (HyScanGtkMapKit   *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if (priv->db_info != NULL && priv->project_name != NULL)
    hyscan_db_info_set_project (priv->db_info, priv->project_name);

  /* Добавляем профиль по умолчанию. */
  {
    HyScanProfileMap *profile;

    profile = hyscan_profile_map_new_default (priv->tile_cache_dir);
    priv->units = hyscan_units_new ();

    add_profile (kit, DEFAULT_PROFILE_NAME, profile);
    hyscan_gtk_map_kit_set_profile_name (kit, DEFAULT_PROFILE_NAME);

    g_object_unref (profile);
  }
}

/* Устанавливает видимость галса track_name. */
static void
hyscan_gtk_map_kit_track_enable (HyScanGtkMapKit *kit,
                                 const gchar     *track_name,
                                 gboolean         enable)
{
  gchar **tracks;
  gboolean track_enabled;
  const gchar **tracks_new;
  gint i, j;

  tracks = hyscan_gtk_map_kit_get_tracks (kit);
  track_enabled = g_strv_contains ((const gchar *const *) tracks, track_name);

  /* Галс уже и так в нужном состоянии. */
  if ((track_enabled != 0) == (enable != 0))
    {
      g_free (tracks);
      return;
    }

  /* Новый массив = старый массив ± track_name + NULL. */
  tracks_new = g_new (const gchar *, g_strv_length (tracks) + (enable ? 1 : -1) + 1);

  for (i = 0, j = 0; tracks[i] != NULL; ++i)
    {
      if (!enable && g_strcmp0 (tracks[i], track_name) == 0)
        continue;

      tracks_new[j++] = tracks[i];
    }

  if (enable)
    tracks_new[j++] = track_name;

  tracks_new[j] = NULL;

  hyscan_gtk_map_kit_set_tracks (kit, (gchar **) tracks_new);

  g_strfreev (tracks);
  g_free (tracks_new);
}

/* Устанавливает видимость галсов с названиями tracks. */
static void
hyscan_gtk_map_kit_set_tracks (HyScanGtkMapKit  *kit,
                               gchar           **tracks)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if (priv->track_layer != NULL)
    hyscan_gtk_map_track_set_tracks (HYSCAN_GTK_MAP_TRACK (priv->track_layer), tracks);

  track_tree_view_visible_changed (kit, (const gchar *const *) tracks);
}

/* Устанавливает видимость галсов с названиями tracks. */
static gchar **
hyscan_gtk_map_kit_get_tracks (HyScanGtkMapKit  *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  if (priv->track_layer != NULL)
    return hyscan_gtk_map_track_get_tracks (HYSCAN_GTK_MAP_TRACK (priv->track_layer));

  return g_new0 (gchar *, 1);
}

/**
 * hyscan_gtk_map_kit_new_map:
 * @center: центр карты
 * @db: указатель на #HyScanDB
 * @cache_dir: папка для кэширования тайлов
 *
 * Returns: указатель на структуру #HyScanGtkMapKit, для удаления
 *          hyscan_gtk_map_kit_free().
 */
HyScanGtkMapKit *
hyscan_gtk_map_kit_new (HyScanGeoPoint    *center,
                        HyScanDB          *db,
                        HyScanUnits       *units,
                        const gchar       *cache_dir)
{
  HyScanGtkMapKit *kit;

  kit = g_new0 (HyScanGtkMapKit, 1);
  kit->priv = g_new0 (HyScanGtkMapKitPrivate, 1);
  kit->priv->tile_cache_dir = g_strdup (cache_dir);
  kit->priv->center = *center;
  kit->priv->units = g_object_ref (units);

  hyscan_gtk_map_kit_model_create (kit, db);
  hyscan_gtk_map_kit_view_create (kit);
  hyscan_gtk_map_kit_model_init (kit);

  return kit;
}

void
hyscan_gtk_map_kit_set_project (HyScanGtkMapKit *kit,
                                const gchar     *project_name)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  /* Очищаем список активных галсов, только если уже был открыт другой проект. */
  if (priv->project_name != NULL)
    hyscan_gtk_map_kit_set_tracks (kit, (gchar **) { NULL });

  g_free (priv->project_name);
  priv->project_name = g_strdup (project_name);

  if (priv->db_info)
    hyscan_db_info_set_project (priv->db_info, priv->project_name);

  /* Устанавливаем проект во всех сущностях. */
  if (priv->mark_geo_model != NULL)
    hyscan_object_model_set_project (priv->mark_geo_model, priv->db, priv->project_name);

  if (priv->mark_model != NULL)
    hyscan_object_model_set_project (priv->mark_model, priv->db, priv->project_name);

  if (priv->planner_model != NULL)
    hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (priv->planner_model), priv->db, priv->project_name);

  if (priv->ml_model != NULL)
    hyscan_mark_loc_model_set_project (priv->ml_model, priv->project_name);

  if (priv->track_layer != NULL)
    hyscan_gtk_map_track_set_project (HYSCAN_GTK_MAP_TRACK (priv->track_layer), priv->project_name);

  if (priv->wfmark_layer != NULL)
    hyscan_gtk_map_wfmark_set_project (HYSCAN_GTK_MAP_WFMARK(priv->wfmark_layer), priv->project_name);
}


gboolean
hyscan_gtk_map_kit_set_profile_name (HyScanGtkMapKit *kit,
                                     const gchar     *profile_name)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (kit->map);
  HyScanProfileMap *profile;

  g_return_val_if_fail (profile_name != NULL, FALSE);

  profile = g_hash_table_lookup (priv->profiles, profile_name);
  if (profile == NULL)
    return FALSE;

  g_free (priv->profile_active);
  priv->profile_active = g_strdup (profile_name);
  hyscan_profile_map_set_offline (profile, priv->profile_offline);
  hyscan_profile_map_apply (profile, map, "base");

  g_signal_handlers_block_by_func (priv->profiles_box, on_profile_change, kit);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->profiles_box), priv->profile_active);
  g_signal_handlers_unblock_by_func (priv->profiles_box, on_profile_change, kit);

  gtk_statusbar_push (GTK_STATUSBAR (priv->stbar_offline), 0,
                      priv->profile_offline ? _("Offline") : _("Online"));

  return TRUE;
}

gchar *
hyscan_gtk_map_kit_get_profile_name (HyScanGtkMapKit   *kit)
{
  return g_strdup (kit->priv->profile_active);
}

/**
 * hyscan_gtk_map_kit_load_profiles:
 * @kit:
 * @profile_dir: директория с ini-профилями карты
 *
 * Добавляет дополнительные профили карты, загружая из папки profile_dir
 */
void
hyscan_gtk_map_kit_load_profiles (HyScanGtkMapKit *kit,
                                  const gchar     *profile_dir)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gchar **config_files;
  guint conf_i;

  config_files = list_profiles (profile_dir);
  for (conf_i = 0; config_files[conf_i] != NULL; ++conf_i)
    {
      HyScanProfileMap *profile;
      gchar *profile_name, *extension_ptr;

      profile = hyscan_profile_map_new (priv->tile_cache_dir, config_files[conf_i]);

      /* Обрезаем имя файла по последней точке - это будет название профиля. */
      profile_name = g_path_get_basename (config_files[conf_i]);
      extension_ptr = g_strrstr (profile_name, ".");
      if (extension_ptr != NULL)
        *extension_ptr = '\0';

      /* Читаем профиль и добавляем его в карту. */
      if (hyscan_profile_read (HYSCAN_PROFILE (profile)))
        add_profile (kit, profile_name, profile);

      g_free (profile_name);
      g_object_unref (profile);
    }

  g_strfreev (config_files);
}

void
add_steer (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *gtk_steer;
  HyScanSteer *steer;

  /* Отклонение от курса. */
  if (priv->nav_model == NULL || priv->planner_selection == NULL)
    return;

  steer = hyscan_steer_new (HYSCAN_NAV_STATE (priv->nav_model), priv->planner_selection, priv->recorder);
  gtk_steer = hyscan_gtk_map_steer_new (steer);
  gtk_box_pack_start (GTK_BOX (priv->steer_box), gtk_steer, TRUE, TRUE, 0);

  g_object_unref (priv->recorder);
}

/**
 * hyscan_gtk_map_kit_add_nav:
 * @kit:
 * @sensor: датчик GPS-ресивера
 * @sensor_name: имя датчика
 * @delay_time: время задержки навигационных данных
 */
void
hyscan_gtk_map_kit_add_nav (HyScanGtkMapKit           *kit,
                            HyScanSensor              *sensor,
                            const gchar               *sensor_name,
                            HyScanSonarRecorder       *recorder,
                            const HyScanAntennaOffset *offset,
                            gdouble                    delay_time)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  GtkWidget *box;

  g_return_if_fail (priv->nav_model == NULL);

  /* Навигационные данные. */
  priv->nav_model = hyscan_nav_model_new ();
  hyscan_nav_model_set_sensor (priv->nav_model, sensor);
  hyscan_nav_model_set_sensor_name (priv->nav_model, sensor_name);
  hyscan_nav_model_set_offset (priv->nav_model, offset);
  hyscan_nav_model_set_delay (priv->nav_model, delay_time);

  /* Определение местоположения. */
  priv->locate_button = gtk_button_new_from_icon_name ("network-wireless-signal-good-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label (GTK_BUTTON (priv->locate_button), _("My location"));
  g_signal_connect_swapped (priv->locate_button, "clicked", G_CALLBACK (on_locate_click), kit);

  /* Слой с траекторией движения судна. */
  priv->way_layer = hyscan_gtk_map_nav_new (HYSCAN_NAV_STATE (priv->nav_model));
  add_layer_row (kit, priv->way_layer, FALSE, "nav", _("Navigation"));

  priv->recorder = g_object_ref (recorder);

  /* Контейнер для виджета навигации по галсу (сам виджет может быть, а может не быть). */
  priv->steer_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->steer_box, FALSE, TRUE, 6);
  gtk_box_pack_start (GTK_BOX (box), priv->locate_button, FALSE, TRUE, 6);

  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "nav", box);

  add_steer (kit);
}

void
hyscan_gtk_map_kit_add_marks_wf (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_return_if_fail (priv->mark_model == NULL);
  g_return_if_fail (priv->db != NULL);

  /* Модели меток водопада и их местоположения. */
  priv->mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  priv->ml_model = hyscan_mark_loc_model_new (priv->db, priv->cache);

  g_signal_connect_swapped (priv->mark_model, "changed", G_CALLBACK (on_marks_changed), kit);

  /* Слой с метками. */

  priv->wfmark_layer = hyscan_gtk_map_wfmark_new (priv->ml_model, priv->db, priv->cache, priv->units);
  add_layer_row (kit, priv->wfmark_layer, FALSE, "wfmark", _("Waterfall Marks"));

  /* Виджет навигации по меткам. */
  create_wfmark_toolbox (kit);

  /* Устанавливаем проект и БД. */
  if (priv->project_name != NULL)
    {
      hyscan_object_model_set_project (priv->mark_model, priv->db, priv->project_name);
      hyscan_mark_loc_model_set_project (priv->ml_model, priv->project_name);
      hyscan_gtk_map_wfmark_set_project (HYSCAN_GTK_MAP_WFMARK (priv->wfmark_layer), priv->project_name);
    }
}

void
hyscan_gtk_map_kit_add_planner (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_return_if_fail (priv->planner_model == NULL);
  g_return_if_fail (priv->db != NULL);

  /* Модель данных планировщика. */
  priv->planner_model = hyscan_planner_model_new ();
  priv->planner_selection = hyscan_planner_selection_new (priv->planner_model);

  /* Слой планировщика. */
  priv->planner_layer = hyscan_gtk_map_planner_new (priv->planner_model, priv->planner_selection);
  add_layer_row (kit, priv->planner_layer, FALSE, "planner", _("Planner"));

  /* Виджет настроек слоя. */
  hyscan_gtk_layer_list_set_tools (HYSCAN_GTK_LAYER_LIST (priv->layer_list), "planner", create_planner_toolbox (kit));

  /* Устанавливаем проект и БД. */
  if (priv->project_name != NULL)
    hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (priv->planner_model), priv->db, priv->project_name);

  add_steer (kit);
}

void
hyscan_gtk_map_kit_add_marks_geo (HyScanGtkMapKit   *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_return_if_fail (priv->mark_geo_model == NULL);
  g_return_if_fail (priv->db != NULL);

  /* Модель геометок. */
  priv->mark_geo_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  g_signal_connect_swapped (priv->mark_geo_model, "changed", G_CALLBACK (on_marks_changed), kit);

  /* Слой с геометками. */
  priv->geomark_layer = hyscan_gtk_map_geomark_new (priv->mark_geo_model);
  add_layer_row (kit, priv->geomark_layer, FALSE, "geomark", _("Geo Marks"));

  /* Виджет навигации по меткам. */
  create_wfmark_toolbox (kit);

  if (priv->project_name != NULL)
    hyscan_object_model_set_project (priv->mark_geo_model, priv->db, priv->project_name);
}

/**
 * hyscan_gtk_map_kit_get_offline:
 * @kit: указатель на #HyScanGtkMapKit
 *
 * Returns: признак режима работы оффлайн
 */
gboolean
hyscan_gtk_map_kit_get_offline (HyScanGtkMapKit *kit)
{
  return kit->priv->profile_offline;
}

/**
 * hyscan_gtk_map_kit_set_offline:
 * @kit: указатель на #HyScanGtkMapKit
 * @offline: признак режима работы оффлайн
 *
 * Устанавливает режим работы @offline и применяет текущий профиль в этом режиме
 */
void
hyscan_gtk_map_kit_set_offline (HyScanGtkMapKit   *kit,
                                gboolean           offline)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;
  gchar *profile_name;

  profile_name = g_strdup (priv->profile_active);

  priv->profile_offline = offline;
  hyscan_gtk_map_kit_set_profile_name (kit, profile_name);

  g_free (profile_name);
}

/**
 * hyscan_gtk_map_kit_free:
 * @kit: указатель на структуру #HyScanGtkMapKit
 */
void
hyscan_gtk_map_kit_free (HyScanGtkMapKit *kit)
{
  HyScanGtkMapKitPrivate *priv = kit->priv;

  g_free (priv->profile_active);
  g_free (priv->tile_cache_dir);
  g_free (priv->project_name);
  g_hash_table_destroy (priv->profiles);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->ml_model);
  g_clear_object (&priv->db);
  g_clear_object (&priv->db_info);
  g_clear_object (&priv->mark_model);
  g_clear_object (&priv->mark_geo_model);
  g_clear_object (&priv->track_store);
  g_clear_object (&priv->mark_store);
  g_clear_object (&priv->planner_model);
  g_clear_object (&priv->planner_selection);
  g_clear_object (&priv->recorder);
  g_clear_object (&priv->units);
  g_free (priv);

  g_free (kit);
}

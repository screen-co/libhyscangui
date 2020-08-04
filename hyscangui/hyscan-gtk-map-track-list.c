/* hyscan-gtk-map-track-list.c
 *
 * Copyright 2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-gtk-map-track-list
 * @Short_description: виджет списка галсов на карте
 * @Title: HyScanGtkMapTrackList
 *
 * Виджет показывает список галсов в БД и управляети их отображением на карте.
 *
 * - галочка напротив галса управляет его видимостью на карте,
 * - активация галса (двойной клик или Enter) переводит видимую область карты на галс,
 * - клик правой кнопкой по галсу открывает контекстное меню.
 *
 * В контекстном меню галса доступно управление параметрами галса #HyScanMapTrackParam.
 *
 * При множественном выборе галсов открывается окно управление параметрами
 * всех выбранных галсов через #HyScanParamMerge.
 */

#include "hyscan-gtk-map-track-list.h"
#include <hyscan-param-merge.h>
#include <hyscan-gtk-param-merge.h>
#include <glib/gi18n-lib.h>

enum
{
  PROP_O,
  PROP_DB_INFO,
  PROP_TRACK_MODEL,
  PROP_TRACK_LAYER,
};

/* Столбцы в GtkTreeView списка галсов. */
enum
{
  VISIBLE_COLUMN,
  DATE_SORT_COLUMN,
  TRACK_COLUMN,
  DATE_COLUMN
};

struct _HyScanGtkMapTrackListPrivate
{
  GtkListStore                *track_store;         /* Модель данных списка GtkTreeView. */
  HyScanMapTrackModel         *track_model;         /* Модель выбранных галсов. */
  HyScanDBInfo                *db_info;             /* Модель галсов в БД. */
  HyScanGtkMapTrack           *track_layer;         /* Слой галсов на карте. */
  GtkMenu                     *track_menu;          /* Виджет контекстного меню галса. */
  GtkWidget                   *track_menu_find;     /* Виджет элемента меню "Find track on the map". */
};

static void        hyscan_gtk_map_track_list_set_property             (GObject               *object,
                                                                       guint                  prop_id,
                                                                       const GValue          *value,
                                                                       GParamSpec            *pspec);
static void        hyscan_gtk_map_track_list_object_constructed       (GObject               *object);
static void        hyscan_gtk_map_track_list_object_finalize          (GObject               *object);
static void        hyscan_gtk_map_track_list_toggle                   (HyScanGtkMapTrackList *track_list,
                                                                       gchar                 *path);
static void        hyscan_gtk_map_track_list_toggle_all               (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_set_visible              (HyScanGtkMapTrackList *track_list,
                                                                       const gchar            *track_name,
                                                                       gboolean                enable);
static void        hyscan_gtk_map_track_list_tracks_changed           (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_activated                (HyScanGtkMapTrackList  *track_list,
                                                                       GtkTreePath            *path,
                                                                       GtkTreeViewColumn      *col);
static void        hyscan_gtk_map_track_list_on_track_change          (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_visible_changed          (HyScanGtkMapTrackList  *track_list);
static GtkWidget * hyscan_gtk_map_track_list_param_window             (HyScanGtkMapTrackList  *track_list,
                                                                       const gchar            *title,
                                                                       HyScanParamMerge       *param);
static gboolean    hyscan_gtk_map_track_list_btn_press                (HyScanGtkMapTrackList  *track_list,
                                                                       GdkEventButton         *event_button);
static void        hyscan_gtk_map_track_list_create_menu              (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_settings                 (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_locate                   (HyScanGtkMapTrackList  *track_list);
static void        hyscan_gtk_map_track_list_select_all               (HyScanGtkMapTrackList  *track_list);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapTrackList, hyscan_gtk_map_track_list, GTK_TYPE_TREE_VIEW)

static void
hyscan_gtk_map_track_list_class_init (HyScanGtkMapTrackListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_list_set_property;

  object_class->constructed = hyscan_gtk_map_track_list_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_list_object_finalize;

  g_object_class_install_property (object_class, PROP_TRACK_MODEL,
    g_param_spec_object ("track-model", "HyScanMapTrackModel", "HyScanMapTrackModel", HYSCAN_TYPE_MAP_TRACK_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_DB_INFO,
    g_param_spec_object ("db-info", "HyScanDBInfo", "HyScanDBInfo", HYSCAN_TYPE_DB_INFO,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_TRACK_LAYER,
    g_param_spec_object ("track-layer", "HyScanGtkMapTrack", "HyScanGtkMapTrack", HYSCAN_TYPE_GTK_MAP_TRACK,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_list_init (HyScanGtkMapTrackList *gtk_map_track_list)
{
  gtk_map_track_list->priv = hyscan_gtk_map_track_list_get_instance_private (gtk_map_track_list);
}

static void
hyscan_gtk_map_track_list_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkMapTrackList *gtk_map_track_list = HYSCAN_GTK_MAP_TRACK_LIST (object);
  HyScanGtkMapTrackListPrivate *priv = gtk_map_track_list->priv;

  switch (prop_id)
    {
    case PROP_DB_INFO:
      priv->db_info = g_value_dup_object (value);
      break;

    case PROP_TRACK_MODEL:
      priv->track_model = g_value_dup_object (value);
      break;

    case PROP_TRACK_LAYER:
      priv->track_layer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_track_list_object_constructed (GObject *object)
{
  HyScanGtkMapTrackList *track_list = HYSCAN_GTK_MAP_TRACK_LIST (object);
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  GtkWidget *image;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *visible_column, *track_column, *date_column;

  G_OBJECT_CLASS (hyscan_gtk_map_track_list_parent_class)->constructed (object);


  priv->track_store = gtk_list_store_new (4,
                                          G_TYPE_BOOLEAN, /* VISIBLE_COLUMN   */
                                          G_TYPE_INT64,   /* DATE_SORT_COLUMN */
                                          G_TYPE_STRING,  /* TRACK_COLUMN     */
                                          G_TYPE_STRING   /* DATE_COLUMN      */);

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  track_column = gtk_tree_view_column_new_with_attributes (_("Track"), renderer, "text", TRACK_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (track_column, TRACK_COLUMN);
  gtk_tree_view_column_set_expand (track_column, TRUE);
  gtk_tree_view_column_set_resizable (track_column, TRUE);
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  /* Дата создания галса. */
  renderer = gtk_cell_renderer_text_new ();
  date_column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer, "text", DATE_COLUMN, NULL);
  gtk_tree_view_column_set_sort_column_id (date_column, DATE_SORT_COLUMN);

  /* Галочка с признаком видимости галса. */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect_swapped (renderer, "toggled", G_CALLBACK (hyscan_gtk_map_track_list_toggle), track_list);
  visible_column = gtk_tree_view_column_new_with_attributes (_("Show"), renderer, "active", VISIBLE_COLUMN, NULL);
  image = gtk_image_new_from_icon_name ("object-select-symbolic", GTK_ICON_SIZE_MENU);
  gtk_tree_view_column_set_clickable (visible_column, TRUE);
  g_signal_connect_swapped (visible_column, "clicked", G_CALLBACK (hyscan_gtk_map_track_list_toggle_all), track_list);
  gtk_widget_show (image);
  gtk_tree_view_column_set_widget (visible_column, image);

  gtk_tree_view_set_model (GTK_TREE_VIEW (track_list), GTK_TREE_MODEL (priv->track_store));
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (track_list), TRACK_COLUMN);
  gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (track_list), GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (track_list)), GTK_SELECTION_MULTIPLE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (track_list), track_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (track_list), date_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (track_list), visible_column);

  /* Контекстное меню галса. */
  hyscan_gtk_map_track_list_create_menu (track_list);

  hyscan_gtk_map_track_list_visible_changed (track_list);

  g_signal_connect_swapped (track_list, "destroy", G_CALLBACK (gtk_widget_destroy), priv->track_menu);
  g_signal_connect (track_list, "button-press-event", G_CALLBACK (hyscan_gtk_map_track_list_btn_press), NULL);
  g_signal_connect (track_list, "row-activated", G_CALLBACK (hyscan_gtk_map_track_list_activated), NULL);
  g_signal_connect_swapped (priv->db_info, "tracks-changed", G_CALLBACK (hyscan_gtk_map_track_list_tracks_changed), track_list);
  g_signal_connect_swapped (priv->track_model, "notify::active-tracks", G_CALLBACK (hyscan_gtk_map_track_list_visible_changed), track_list);
}

static void
hyscan_gtk_map_track_list_object_finalize (GObject *object)
{
  HyScanGtkMapTrackList *gtk_map_track_list = HYSCAN_GTK_MAP_TRACK_LIST (object);
  HyScanGtkMapTrackListPrivate *priv = gtk_map_track_list->priv;

  g_clear_object (&priv->track_layer);
  g_object_unref (priv->track_model);
  g_object_unref (priv->track_store);
  g_object_unref (priv->track_menu);

  G_OBJECT_CLASS (hyscan_gtk_map_track_list_parent_class)->finalize (object);
}

/* Открывает контекстное меню по правой кнопке мыши. */
static gboolean
hyscan_gtk_map_track_list_btn_press (HyScanGtkMapTrackList *track_list,
                                     GdkEventButton        *event_button)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

  if (event_button->button != GDK_BUTTON_SECONDARY)
    return GDK_EVENT_PROPAGATE;

  gtk_menu_popup_at_pointer (priv->track_menu, (const GdkEvent *) event_button);

  return GDK_EVENT_STOP;
}

/* Функция вызывается при изменении списка галсов. */
static void
hyscan_gtk_map_track_list_tracks_changed (HyScanGtkMapTrackList *track_list)
{
  GtkTreePath *null_path;
  GtkTreeIter tree_iter;
  GHashTable *tracks;
  GHashTableIter hash_iter;
  gpointer key, value;
  gchar **selected_tracks;

  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

  null_path = gtk_tree_path_new ();
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (track_list), null_path, NULL, FALSE);
  gtk_tree_path_free (null_path);

  gtk_list_store_clear (priv->track_store);

  tracks = hyscan_db_info_get_tracks (priv->db_info);
  selected_tracks = hyscan_map_track_model_get_tracks (priv->track_model);
  g_hash_table_iter_init (&hash_iter, tracks);
  while (g_hash_table_iter_next (&hash_iter, &key, &value))
    {
      GDateTime *local;
      gchar *time_str;
      HyScanTrackInfo *track_info = value;
      gboolean visible;

      if (track_info->id == NULL)
        {
          g_warning ("HyScanGtkMapTrackList: unable to open track <%s>", track_info->name);
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

/* Активация галса (двойной клик или Enter) - открывает галс на карте. */
static void
hyscan_gtk_map_track_list_activated (HyScanGtkMapTrackList *track_list,
                                     GtkTreePath        *path,
                                     GtkTreeViewColumn  *col)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

  GtkTreeModel *model = GTK_TREE_MODEL (priv->track_store);
  GtkTreeIter iter;
  gchar *track_name;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, TRACK_COLUMN, &track_name, -1);
  hyscan_gtk_map_track_view (HYSCAN_GTK_MAP_TRACK (priv->track_layer), track_name, FALSE, NULL);

  g_free (track_name);
}

/* Обработчик изменения выделения в списке (GtkTreeSelection::changed). */
static void
hyscan_gtk_map_track_list_on_track_change (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  HyScanMapTrackParam *param = NULL;
  gchar *track_name;
  gboolean has_nmea = FALSE;

  track_name = hyscan_gtk_map_track_list_get_selected (track_list);

  if (track_name != NULL && priv->track_model != NULL)
    {
      param = hyscan_map_track_model_param (priv->track_model, track_name);
      has_nmea = (param != NULL) && hyscan_map_track_param_has_rmc (param);
      g_object_unref (param);
    }

  gtk_widget_set_sensitive (priv->track_menu_find, has_nmea);
  g_free (track_name);
}

/* Функция создаёт окно настройки параметров галса. */
static GtkWidget *
hyscan_gtk_map_track_list_param_window (HyScanGtkMapTrackList  *track_list,
                                    const gchar            *title,
                                    HyScanParamMerge       *param)
{
  GtkWidget *window;
  GtkWidget *frontend;
  GtkWidget *grid;
  GtkWidget *abar;
  GtkWidget *apply, *discard, *ok, *cancel;
  GtkSizeGroup *size;
  GtkWidget *toplevel;

  /* Создаём окно. */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 300);
  gtk_window_set_title (GTK_WINDOW (window), title);
  gtk_window_set_modal (GTK_WINDOW (window), TRUE);
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (track_list));
  if (GTK_IS_WINDOW (toplevel))
    gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));

  /* Виджет отображения параметров. */
  frontend = hyscan_gtk_param_merge_new_full (param, "/", FALSE);
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

/* Обновляет список активных галсов при измении модели (HyScanMapTrackModel::notify::active-tracks). */
static void
hyscan_gtk_map_track_list_visible_changed (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  GtkTreeIter iter;
  gboolean valid;
  gchar **visible_tracks;

  visible_tracks = hyscan_map_track_model_get_tracks (priv->track_model);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->track_store), &iter);
  while (valid)
   {
     gchar *track_name;
     gboolean active;

     gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);

     active = visible_tracks != NULL && g_strv_contains ((const gchar *const *) visible_tracks, track_name);
     gtk_list_store_set (priv->track_store, &iter, VISIBLE_COLUMN, active, -1);

     g_free (track_name);

     valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->track_store), &iter);
   }

  g_strfreev (visible_tracks);
}

/* Открывает всплывающее окно для настройки параметров выбранных галсов (GtkMenuItem::activate). */
static void
hyscan_gtk_map_track_list_settings (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

  GtkTreeSelection *selection;
  GList *list, *link;
  HyScanParamMerge *merge;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (track_list));
  list = gtk_tree_selection_get_selected_rows (selection, NULL);

  merge = hyscan_param_merge_new ();
  for (link = list; link != NULL; link = link->next)
    {
      GtkTreePath *path = link->data;
      GtkTreeIter iter;
      gchar *track_name;

      if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->track_store), &iter, path))
        {
          HyScanMapTrackParam *param;

          gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
          param = hyscan_map_track_model_param (priv->track_model, track_name);
          hyscan_param_merge_add (merge, HYSCAN_PARAM (param));
          g_object_unref (param);

          g_free (track_name);
        }
    }

  if (hyscan_param_merge_bind (merge))
    {
      GtkWidget *window;

      window = hyscan_gtk_map_track_list_param_window (track_list, _("Track settings"), merge);
      gtk_widget_show_all (window);
    }

  g_object_unref (merge);
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
}

/* Открывает на карте выбранный галс (GtkMenuItem::activate). */
static void
hyscan_gtk_map_track_list_locate (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  gchar *track_name;

  track_name = hyscan_gtk_map_track_list_get_selected (track_list);

  if (track_name != NULL)
    hyscan_gtk_map_track_view (priv->track_layer, track_name, FALSE, NULL);

  g_free (track_name);
}

/* Функция создаёт виджет контекстного меню галса. */
static void
hyscan_gtk_map_track_list_create_menu (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  GtkWidget *menu_item_settings;

  priv->track_menu = GTK_MENU (g_object_ref (gtk_menu_new ()));
  priv->track_menu_find = gtk_menu_item_new_with_label (_("Find track on the map"));
  g_signal_connect_swapped (priv->track_menu_find, "activate", G_CALLBACK (hyscan_gtk_map_track_list_locate), track_list);
  gtk_widget_show (priv->track_menu_find);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->track_menu), priv->track_menu_find);
  gtk_widget_set_sensitive (priv->track_menu_find, FALSE);

  menu_item_settings = gtk_menu_item_new_with_label (_("Track settings"));
  g_signal_connect_swapped (menu_item_settings, "activate", G_CALLBACK (hyscan_gtk_map_track_list_settings), track_list);
  gtk_widget_show (menu_item_settings);
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->track_menu), menu_item_settings);

  gtk_menu_attach_to_widget (priv->track_menu, GTK_WIDGET (track_list), NULL);

  /* Обработчик определяет доступные пункт при выборе галса. */
  g_signal_connect_swapped (gtk_tree_view_get_selection (GTK_TREE_VIEW (track_list)), "changed",
                            G_CALLBACK (hyscan_gtk_map_track_list_on_track_change), track_list);
}

/* Отмечает все галочки в списке галсов. */
static void
hyscan_gtk_map_track_list_select_all (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  gboolean valid;
  GtkTreeIter iter;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->track_store), &iter);
  while (valid)
    {
      gchar *track_name;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
      hyscan_gtk_map_track_list_set_visible (track_list, track_name, TRUE);
      g_free (track_name);

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->track_store), &iter);
    }
}

/* Клик по заголовку с галочкой в таблице галсов.
 * Включает или отключает видимость всех галсов.*/
static void
hyscan_gtk_map_track_list_toggle_all (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  gchar **selected;

  selected = hyscan_map_track_model_get_tracks (priv->track_model);
  if (g_strv_length (selected) == 0)
    hyscan_gtk_map_track_list_select_all (track_list);
  else
    hyscan_map_track_model_set_tracks (priv->track_model, NULL);

  g_strfreev (selected);
}

/* Устанавливает видимость галса track_name. */
static void
hyscan_gtk_map_track_list_set_visible (HyScanGtkMapTrackList *track_list,
                                       const gchar           *track_name,
                                       gboolean               enable)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;
  gchar **tracks;
  gboolean track_enabled;
  const gchar **tracks_new;
  gint i, j;

  tracks = hyscan_map_track_model_get_tracks (priv->track_model);
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

  hyscan_map_track_model_set_tracks (priv->track_model, (gchar **) tracks_new);

  g_strfreev (tracks);
  g_free (tracks_new);
}

/* Обрабатывает нажатие на галочку галса (GtkCellRendererToggle::toggled). */
static void
hyscan_gtk_map_track_list_toggle (HyScanGtkMapTrackList *track_list,
                                  gchar                 *path)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

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
  hyscan_gtk_map_track_list_set_visible (track_list, track_name, !active);

  g_free (track_name);
}

/**
 * hyscan_gtk_map_track_list_new:
 * @map_track_model: модель выбранны галсов #HyScanMapTrackModel
 * @db_info: указатель на #HyScanDBInfo
 * @track_layer: слой галсов на карте
 *
 * Функция создаёт виджет списка галсов.
 *
 * Returns: новый виджет списка галсов.
 */
GtkWidget *
hyscan_gtk_map_track_list_new (HyScanMapTrackModel *map_track_model,
                               HyScanDBInfo        *db_info,
                               HyScanGtkMapTrack   *track_layer)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LIST,
                       "track-model", map_track_model,
                       "db-info", db_info,
                       "track-layer", track_layer,
                       NULL);
}

/* Возвращает имя выбранного галса. */
gchar *
hyscan_gtk_map_track_list_get_selected (HyScanGtkMapTrackList *track_list)
{
  HyScanGtkMapTrackListPrivate *priv = track_list->priv;

  GtkTreeSelection *selection;
  GList *list = NULL;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->track_store);
  gchar *track_name;

  GtkTreePath *path;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (track_list));
  list = gtk_tree_selection_get_selected_rows (selection, &tree_model);
  if (list == NULL)
    return NULL;

  path = list->data;
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->track_store), &iter, path))
    gtk_tree_model_get (GTK_TREE_MODEL (priv->track_store), &iter, TRACK_COLUMN, &track_name, -1);
  else
    track_name = NULL;

  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

  return track_name;
}

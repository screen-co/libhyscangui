/* hyscan-gtk-planner-list.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-planner-list
 * @Short_description: Виджет списка галсов
 * @Title: HyScanGtkPlannerList
 *
 * Данный виджет выводит схему плановых галсов в виде списка из зон полигона и
 * плановых галсов внутри них.
 *
 * Виджет имеет контекстное меню для манипуляции галсами:
 * - переход к галсу на карте,
 * - активация галса (для навигации),
 * - выделение нескольких,
 * - изменение размеров и направления,
 * - удаление.
 *
 */

#include <glib/gi18n-lib.h>
#include <hyscan-track-stats.h>
#include "hyscan-gtk-planner-list.h"

#define OTHER_ZONE "other-zone"    /* Искусственный идентификатор для группировки галсов, у которых нет зоны. */
#define RAD2DEG(x) (((x) < 0 ? (x) + 2 * G_PI : (x)) / G_PI * 180.0)

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
  PROP_STATS,
  PROP_VIEWER,
};

enum
{
  TYPE_PLANNER_TRACK,
  TYPE_PLANNER_ZONE,
  TYPE_DB_TRACK,
};

typedef struct
{
  HyScanTrackStatsInfo *info;
} HyScanGtkPlannerListItem;

enum
{
  NUMBER_COLUMN,
  ID_COLUMN,
  NAME_COLUMN,
  TIME_COLUMN,
  LENGTH_COLUMN,
  Y_VAR_COLUMN,
  ANGLE_COLUMN,
  ANGLE_VAR_COLUMN,
  SPEED_COLUMN,
  SPEED_VAR_COLUMN,
  TYPE_COLUMN,
  WEIGHT_COLUMN,
  N_COLUMNS,
};

#define HYSCAN_TYPE_GTK_PLANNER_TREE_STORE             (hyscan_gtk_planner_tree_store_get_type ())
#define HYSCAN_GTK_PLANNER_TREE_STORE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PLANNER_TREE_STORE, HyScanGtkPlannerTreeStore))
#define HYSCAN_IS_GTK_PLANNER_TREE_STORE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PLANNER_TREE_STORE))
#define HYSCAN_GTK_PLANNER_TREE_STORE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PLANNER_TREE_STORE, HyScanGtkPlannerTreeStoreClass))
#define HYSCAN_IS_GTK_PLANNER_TREE_STORE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PLANNER_TREE_STORE))
#define HYSCAN_GTK_PLANNER_TREE_STORE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PLANNER_TREE_STORE, HyScanGtkPlannerTreeStoreClass))

/* Класс модели данных нашего дерева. Наследуется от GtkTreeStore и обрабатывает drag'n'drop галсов. */
typedef struct
{
  GtkTreeStore          parent_instance;
  HyScanGtkPlannerList *list;
} HyScanGtkPlannerTreeStore;

typedef struct
{
  GtkTreeStoreClass parent_class;
} HyScanGtkPlannerTreeStoreClass;

static GType          hyscan_gtk_planner_tree_store_get_type            (void);
static GtkTreeStore * hyscan_gtk_planner_tree_store_new                 (HyScanGtkPlannerList   *list);
static void           hyscan_gtk_planner_tree_store_source_iface_init   (GtkTreeDragSourceIface *iface);
static void           hyscan_gtk_planner_tree_store_dest_iface_init     (GtkTreeDragDestIface   *iface);
static void           hyscan_gtk_planner_list_db_trk_changed            (HyScanGtkPlannerList   *list);
G_DEFINE_TYPE_WITH_CODE (HyScanGtkPlannerTreeStore, hyscan_gtk_planner_tree_store, GTK_TYPE_TREE_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                hyscan_gtk_planner_tree_store_source_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
                                                hyscan_gtk_planner_tree_store_dest_iface_init))

static GtkTreeDragSourceIface *hyscan_gtk_layer_parent_src_iface;
static GtkTreeDragDestIface *hyscan_gtk_layer_parent_dest_iface;

/* Информация о зоне полигона. */
typedef struct
{
  gdouble                       length;             /* Суммарная длина всех галсов в зоне. */
  gdouble                       time;               /* Суммарное плановое время всех галсов в зоне. */
} HyScanGtkPlannerListZone;

struct _HyScanGtkPlannerListPrivate
{
  HyScanPlannerModel           *model;              /* Модель объектов планировщика. */
  HyScanPlannerSelection       *selection;          /* Модель выбранных объектов планировщика. */
  HyScanGtkMapPlanner          *viewer;             /* Слой для просмотора галсов. */
  HyScanDBInfo                 *db_info;            /* Слой для просмотора галсов. */
  HyScanTrackStats             *stats;              /* Модель данных отклонений галсов от плановых значений. */
  GtkTreeStore                 *store;              /* Модель данных дерева. */
  GtkTreeSelection             *tree_selection;     /* Объект выбранных элементов дерева. */
  gchar                       **tracks;             /* Идентификаторы выбранных галсов. */
  gchar                        *active;             /* Идентификатор активного галса. */
  gulong                        tree_sel_hndl;      /* Обработчик сигнала об изменении выбора в дереве. */
  gulong                        planner_sel_hndl;   /* Обработчик сигнала об изменении выбора в планировщике. */
  GHashTable                   *objects;            /* Объекты планировщика. */

  struct
  {
    GtkMenu                    *menu;                 /* Виджет меню. */
    GtkWidget                  *nav;                  /* Виджет пункта меню "Навигация". */
    GtkWidget                  *find;                 /* Виджет пункта меню "Найти". */
    GtkWidget                  *inv;                  /* Виджет пункта меню "Сменить направление". */
    GtkWidget                  *del;                  /* Виджет пункта меню "Удалить". */
    GtkWidget                  *extend;               /* Виджет пункта меню "Расятнуть до границ". */
  }                             menu;                 /* Контекстное меню галса. */

  GtkTreeViewColumn            *title_col;            /* Столбец "Название". */
  GtkTreeViewColumn            *dist_col;             /* Столбец "Длина". */
  GtkTreeViewColumn            *time_col;             /* Столбец "Время". */
  GtkTreeViewColumn            *speed_col;            /* Столбец "Скорость". */
  GtkTreeViewColumn            *angle_col;            /* Столбец "Угол". */
};

static void                       hyscan_gtk_planner_list_set_property       (GObject                  *object,
                                                                              guint                     prop_id,
                                                                              const GValue             *value,
                                                                              GParamSpec               *pspec);
static void                       hyscan_gtk_planner_list_object_constructed (GObject                  *object);
static void                       hyscan_gtk_planner_list_object_finalize    (GObject                  *object);
static void                       hyscan_gtk_planner_list_restore_selection  (HyScanGtkPlannerList     *list);
static void                       hyscan_gtk_planner_list_changed            (HyScanGtkPlannerList     *list);
static void                       hyscan_gtk_planner_list_selection_changed  (HyScanGtkPlannerList     *list);
static HyScanGtkPlannerListZone * hyscan_gtk_planner_list_zone_new           (void);
static void                       hyscan_gtk_planner_list_zone_free          (HyScanGtkPlannerListZone *zone);
static void                       hyscan_gtk_planner_list_tracks_changed     (HyScanGtkPlannerList      *list,
                                                                              gchar                    **tracks);
static void                       hyscan_gtk_planner_list_set_active         (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_cell_data          (GtkTreeViewColumn         *tree_column,
                                                                              GtkCellRenderer           *cell,
                                                                              GtkTreeModel              *tree_model,
                                                                              GtkTreeIter               *iter,
                                                                              gpointer                   data);
static gboolean                   hyscan_gtk_planner_list_selection          (GtkTreeSelection          *selection,
                                                                              GtkTreeModel              *model,
                                                                              GtkTreePath               *path,
                                                                              gboolean                   selected,
                                                                              gpointer                   data);
static void                       hyscan_gtk_planner_list_row_expanded       (HyScanGtkPlannerList      *list,
                                                                              GtkTreeIter               *iter,
                                                                              GtkTreePath               *path);
static gboolean                   hyscan_gtk_planner_list_button_press       (HyScanGtkPlannerList      *list,
                                                                              GdkEventButton            *event_button);
static inline void                hyscan_gtk_planner_list_add_menu           (HyScanGtkPlannerList      *list,
                                                                              const gchar               *mnemonic,
                                                                              GtkWidget                **widget);
static void                       hyscan_gtk_planner_list_update_zones       (HyScanGtkPlannerList      *list,
                                                                              GHashTable                *tree_iter_ht,
                                                                              GHashTable                *zone_info_ht);
static GHashTable *               hyscan_gtk_planner_list_update_tracks      (HyScanGtkPlannerList      *list,
                                                                              GHashTable                *tree_iter_ht);
static GHashTable *               hyscan_gtk_planner_list_map_existing       (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_menu_activate      (GtkMenuItem               *button,
                                                                              HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_title_edited       (GtkCellRendererText       *renderer,
                                                                              gchar                     *path,
                                                                              gchar                     *new_text,
                                                                              gpointer                   user_data);
static void                       hyscan_gtk_planner_list_speed_edited       (GtkCellRendererText       *renderer,
                                                                              gchar                     *path,
                                                                              gchar                     *new_text,
                                                                              gpointer                   user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkPlannerList, hyscan_gtk_planner_list, GTK_TYPE_TREE_VIEW)

static void
hyscan_gtk_planner_tree_store_class_init (HyScanGtkPlannerTreeStoreClass *klass)
{
}

static void
hyscan_gtk_planner_tree_store_init (HyScanGtkPlannerTreeStore *gtk_planner_tree_store)
{
}

static void
hyscan_gtk_planner_list_class_init (HyScanGtkPlannerListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_list_set_property;

  object_class->constructed = hyscan_gtk_planner_list_object_constructed;
  object_class->finalize = hyscan_gtk_planner_list_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Planner Model", "Planner model", HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "Planner Selection", "Planner selection", HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_STATS,
                                   g_param_spec_object ("stats", "Track Stats", "HyScanTrackStats", HYSCAN_TYPE_TRACK_STATS,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_VIEWER,
    g_param_spec_object ("viewer", "Preview layer", "Planner preview layer", HYSCAN_TYPE_GTK_MAP_PLANNER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_planner_list_init (HyScanGtkPlannerList *gtk_planner_list)
{
  gtk_planner_list->priv = hyscan_gtk_planner_list_get_instance_private (gtk_planner_list);
}

static void
hyscan_gtk_planner_list_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  HyScanGtkPlannerList *gtk_planner_list = HYSCAN_GTK_PLANNER_LIST (object);
  HyScanGtkPlannerListPrivate *priv = gtk_planner_list->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    case PROP_VIEWER:
      priv->viewer = g_value_dup_object (value);
      break;

    case PROP_STATS:
      priv->stats = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_planner_list_object_constructed (GObject *object)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (object);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkTreeView *tree_view = GTK_TREE_VIEW (list);
  GtkCellRenderer *renderer;

  G_OBJECT_CLASS (hyscan_gtk_planner_list_parent_class)->constructed (object);

  gtk_tree_view_set_grid_lines (tree_view, GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_tree_view_set_reorderable (tree_view, TRUE);

  /* Модель данных дерева. */
  priv->store = hyscan_gtk_planner_tree_store_new (list);
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (priv->store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->store), NUMBER_COLUMN, GTK_SORT_ASCENDING);

  /* Настройка выбора. */
  priv->tree_selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (priv->tree_selection, GTK_SELECTION_MULTIPLE);
  gtk_tree_selection_set_select_function (priv->tree_selection, hyscan_gtk_planner_list_selection, list, NULL);

  /* Столбец - "Название". */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "width-chars", 16, "width-chars", 16, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  priv->title_col = gtk_tree_view_column_new_with_attributes (_("Title"), renderer, "text", NAME_COLUMN, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (hyscan_gtk_planner_list_title_edited), list);
  gtk_tree_view_column_set_cell_data_func (priv->title_col, renderer, hyscan_gtk_planner_list_cell_data, list, NULL);
  // gtk_tree_view_column_set_sort_column_id (priv->title_col, NAME_COLUMN);
  gtk_tree_view_column_set_sort_column_id (priv->title_col, NUMBER_COLUMN);

  /* Столбец - "Длина". */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "alignment", PANGO_ALIGN_RIGHT, NULL);
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  priv->dist_col = gtk_tree_view_column_new_with_attributes (_("Len, m"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (priv->dist_col, renderer, hyscan_gtk_planner_list_cell_data, list, NULL);
  gtk_tree_view_column_set_sort_column_id (priv->dist_col, LENGTH_COLUMN);

  /* Столбец - "Время". */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  priv->time_col = gtk_tree_view_column_new_with_attributes (_("Time"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (priv->time_col, renderer, hyscan_gtk_planner_list_cell_data, list, NULL);
  gtk_tree_view_column_set_sort_column_id (priv->time_col, TIME_COLUMN);

  /* Столбец - "Скорость". */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "alignment", PANGO_ALIGN_RIGHT, NULL);
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  priv->speed_col = gtk_tree_view_column_new_with_attributes (_("Spd, m/s"), renderer, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (hyscan_gtk_planner_list_speed_edited), list);
  gtk_tree_view_column_set_cell_data_func (priv->speed_col, renderer, hyscan_gtk_planner_list_cell_data, list, NULL);
  gtk_tree_view_column_set_sort_column_id (priv->speed_col, SPEED_COLUMN);

  /* Столбец - "Угол". */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "alignment", PANGO_ALIGN_RIGHT, NULL);
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  priv->angle_col = gtk_tree_view_column_new_with_attributes (_("Angle, °"), renderer, NULL);
  g_signal_connect (renderer, "edited", G_CALLBACK (hyscan_gtk_planner_list_speed_edited), list);
  gtk_tree_view_column_set_cell_data_func (priv->angle_col, renderer, hyscan_gtk_planner_list_cell_data, list, NULL);
  gtk_tree_view_column_set_sort_column_id (priv->angle_col, ANGLE_COLUMN);

  gtk_tree_view_append_column (tree_view, priv->title_col);
  gtk_tree_view_append_column (tree_view, priv->dist_col);
  gtk_tree_view_append_column (tree_view, priv->time_col);
  gtk_tree_view_append_column (tree_view, priv->speed_col);
  gtk_tree_view_append_column (tree_view, priv->angle_col);

  /* Контекстное меню. */
  priv->menu.menu = GTK_MENU (gtk_menu_new ());
  gtk_menu_attach_to_widget (priv->menu.menu, GTK_WIDGET (list), NULL);
  if (priv->viewer != NULL)
     hyscan_gtk_planner_list_add_menu (list, _("_Find track on the map"), &priv->menu.find);
  hyscan_gtk_planner_list_add_menu (list, _("Start _navigation"), &priv->menu.nav);

  hyscan_gtk_planner_list_add_menu (list, NULL, NULL);
  hyscan_gtk_planner_list_add_menu (list, _("_Invert"), &priv->menu.inv);
  hyscan_gtk_planner_list_add_menu (list, _("Adjust length to _zone boundary"), &priv->menu.extend);
  hyscan_gtk_planner_list_add_menu (list, _("_Delete"), &priv->menu.del);

  priv->tree_sel_hndl = g_signal_connect_swapped (priv->tree_selection, "changed",
                                                  G_CALLBACK (hyscan_gtk_planner_list_selection_changed), list);
  priv->planner_sel_hndl = g_signal_connect_swapped (priv->selection, "tracks-changed",
                                                     G_CALLBACK (hyscan_gtk_planner_list_tracks_changed), list);

  g_signal_connect_swapped (priv->stats, "changed", G_CALLBACK (hyscan_gtk_planner_list_db_trk_changed), list);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_planner_list_set_active), list);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_list_changed), list);
  g_signal_connect_swapped (tree_view, "row-expanded", G_CALLBACK (hyscan_gtk_planner_list_row_expanded), list);
  g_signal_connect_swapped (tree_view, "button-press-event", G_CALLBACK (hyscan_gtk_planner_list_button_press), list);
}

static void
hyscan_gtk_planner_list_object_finalize (GObject *object)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (object);
  HyScanGtkPlannerListPrivate *priv = list->priv;

  g_signal_handlers_disconnect_by_data (priv->selection, list);
  g_signal_handlers_disconnect_by_data (priv->model, list);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_clear_object (&priv->viewer);
  g_clear_object (&priv->stats);
  g_clear_pointer (&priv->objects, g_hash_table_unref);
  g_clear_pointer (&priv->tracks, g_strfreev);
  g_free (priv->active);

  G_OBJECT_CLASS (hyscan_gtk_planner_list_parent_class)->finalize (object);
}

static HyScanGtkPlannerListItem *
hyscan_gtk_planner_list_item_copy (const HyScanGtkPlannerListItem *item)
{
  HyScanGtkPlannerListItem *copy;

  if (item == NULL)
    return NULL;

  copy = g_slice_new0 (HyScanGtkPlannerListItem);
  copy->info = hyscan_track_stats_info_copy (item->info);

  return copy;
}

static void
hyscan_gtk_planner_list_item_free (HyScanGtkPlannerListItem *item)
{
  if (item == NULL)
    return;

  hyscan_track_stats_info_free (item->info);
  g_slice_free (HyScanGtkPlannerListItem, item);
}

G_DEFINE_BOXED_TYPE (HyScanGtkPlannerListItem, hyscan_gtk_planner_list_item,
                     hyscan_gtk_planner_list_item_copy, hyscan_gtk_planner_list_item_free)

/* Создаёт наследника GtkTreeStore с особенностями в реализации Drag'n'drop (DND). */
static GtkTreeStore *
hyscan_gtk_planner_tree_store_new (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerTreeStore *store;
  GType types[] = { G_TYPE_UINT,       /* NUMBER_COLUMN */
                    G_TYPE_STRING,     /* ID_COLUMN */
                    G_TYPE_STRING,     /* NAME_COLUMN */
                    G_TYPE_DOUBLE,     /* TIME_COLUMN */
                    G_TYPE_DOUBLE,     /* LENGTH_COLUMN */
                    G_TYPE_DOUBLE,     /* Y_VAR_COLUMN */
                    G_TYPE_DOUBLE,     /* ANGLE_COLUMN */
                    G_TYPE_DOUBLE,     /* ANGLE_VAR_COLUMN */
                    G_TYPE_DOUBLE,     /* SPEED_COLUMN */
                    G_TYPE_DOUBLE,     /* SPEED_VAR_COLUMN */
                    G_TYPE_INT,        /* TYPE_COLUMN */
                    G_TYPE_INT };      /* WEIGHT_COLUMN */

  store = g_object_new (HYSCAN_TYPE_GTK_PLANNER_TREE_STORE, NULL);
  gtk_tree_store_set_column_types (GTK_TREE_STORE (store), G_N_ELEMENTS (types), types);
  store->list = list;

  return GTK_TREE_STORE (store);
}

/* Создаёт структуру HyScanGtkPlannerListZone. Для удаления -
 * hyscan_gtk_planner_list_zone_new(). */
static HyScanGtkPlannerListZone *
hyscan_gtk_planner_list_zone_new (void)
{
  return g_slice_new0 (HyScanGtkPlannerListZone);
}

/* Осовобождает память, выделенную в hyscan_gtk_planner_list_zone_new(). */
static void
hyscan_gtk_planner_list_zone_free (HyScanGtkPlannerListZone *zone)
{
  g_slice_free (HyScanGtkPlannerListZone, zone);
}

/* Обработчик сигнала "activate" элементов контекстного меню. */
static void
hyscan_gtk_planner_list_menu_activate (GtkMenuItem          *button,
                                       HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  /* Удалени выбранных галсов. */
  if (button == GTK_MENU_ITEM (priv->menu.del))
    {
      gint i;
      for (i = 0; priv->tracks[i] != NULL; ++i)
        hyscan_object_model_remove_object (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i]);
    }

  else if (button == GTK_MENU_ITEM (priv->menu.nav))
    {
      hyscan_planner_selection_activate (priv->selection, priv->tracks[0]);
    }

  else if (button == GTK_MENU_ITEM (priv->menu.find))
    {
      hyscan_gtk_map_planner_track_view (priv->viewer, priv->tracks[0], FALSE);
    }

  /* Расширение. */
  else if (button == GTK_MENU_ITEM (priv->menu.extend))
    {
      gint i;
      for (i = 0; priv->tracks[i] != NULL; ++i)
        {
          HyScanPlannerTrack *track, *modified_track;
          HyScanPlannerZone *zone;

          track = g_hash_table_lookup (priv->objects, priv->tracks[i]);
          if (track == NULL || track->zone_id == NULL)
            continue;

          zone = g_hash_table_lookup (priv->objects, track->zone_id);
          if (zone == NULL)
            continue;

          modified_track = hyscan_planner_track_extend (track, zone);
          hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i],
                                             (const HyScanObject *) modified_track);
          hyscan_planner_track_free (modified_track);
        }
    }

  else if (button == GTK_MENU_ITEM (priv->menu.inv))
    {
      gint i;
      for (i = 0; priv->tracks[i] != NULL; ++i)
        {
          HyScanPlannerTrack *track, *modified_track;
          HyScanPlannerZone *zone;
          HyScanGeoGeodetic swap;

          track = g_hash_table_lookup (priv->objects, priv->tracks[i]);
          if (track == NULL || track->zone_id == NULL)
            continue;

          zone = g_hash_table_lookup (priv->objects, track->zone_id);
          if (zone == NULL)
            continue;

          modified_track = hyscan_planner_track_copy (track);
          swap = modified_track->plan.start;
          modified_track->plan.start = modified_track->plan.end;
          modified_track->plan.end = swap;
          hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i],
                                             (const HyScanObject *) modified_track);
          hyscan_planner_track_free (modified_track);
        }
    }
}

/* Добавляет пункт контекстного меню. */
static inline void
hyscan_gtk_planner_list_add_menu (HyScanGtkPlannerList  *list,
                                  const gchar           *mnemonic,
                                  GtkWidget            **widget)
{
  GtkWidget *item;

  if (mnemonic != NULL)
    {
      item = gtk_menu_item_new_with_mnemonic (mnemonic);
      g_signal_connect (item, "activate", G_CALLBACK (hyscan_gtk_planner_list_menu_activate), list);
    }
  else
    {
      item = gtk_separator_menu_item_new ();
    }

  gtk_widget_show (item);
  gtk_menu_shell_append (GTK_MENU_SHELL (list->priv->menu.menu), item);

  widget != NULL ? (*widget = item) : 0;
}

/* Открывет контекстное меню по правой кнопке. */
static gboolean
hyscan_gtk_planner_list_button_press (HyScanGtkPlannerList *list,
                                      GdkEventButton       *event_button)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  gint type;
  gboolean selected;
  gboolean result = GDK_EVENT_PROPAGATE;

  /* Нажатие правой кнопкой. */
  if (event_button->type != GDK_BUTTON_PRESS || event_button->button != GDK_BUTTON_SECONDARY)
    goto exit;

  /* Нажатие на нужное окно. */
  if (event_button->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (list)))
    goto exit;

  /* Ищем строку под курсором мыши. */
  if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (list), event_button->x, event_button->y, &path, NULL, NULL, NULL))
    goto exit;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, TYPE_COLUMN, &type, -1);
  if (type != TYPE_PLANNER_TRACK)
    goto exit;

  selected = gtk_tree_selection_path_is_selected (priv->tree_selection, path);
  if (!selected)
    {
      gtk_tree_selection_unselect_all (priv->tree_selection);
      gtk_tree_selection_select_path (priv->tree_selection, path);
    }

  if (priv->tracks != NULL && g_strv_length (priv->tracks) == 1)
    {
      gtk_widget_set_sensitive (priv->menu.find, TRUE);
      gtk_widget_set_sensitive (priv->menu.nav, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->menu.find, FALSE);
      gtk_widget_set_sensitive (priv->menu.nav, FALSE);
    }

  result = GDK_EVENT_STOP;
  gtk_menu_popup_at_pointer (priv->menu.menu, (GdkEvent *) event_button);

exit:
  g_clear_pointer (&path, gtk_tree_path_free);

  return result;
}

/* Обработчик сигнала "row-expanded".
 * Функция устанавливает выбор внутри виджета, если какая-то из веток дерева была раскрыта.
 * Такой хак необходим, т.к. GTK автоматически снимает выбор со свёрнутых строк. */
static void
hyscan_gtk_planner_list_row_expanded (HyScanGtkPlannerList *list,
                                      GtkTreeIter          *iter,
                                      GtkTreePath          *path)
{
  hyscan_gtk_planner_list_restore_selection (list);
}

/* Изменение названия зоны. */
static void
hyscan_gtk_planner_list_title_edited (GtkCellRendererText *renderer,
                                      gchar               *path,
                                      gchar               *new_text,
                                      gpointer             user_data)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (user_data);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gint type;
  GtkTreeIter iter;
  const gchar *zone_id;
  GValue value = G_VALUE_INIT;
  HyScanPlannerZone *zone;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (priv->store), &iter, path))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, TYPE_COLUMN, &type, -1);
  if (type != TYPE_PLANNER_ZONE)
    return;

  gtk_tree_model_get_value (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &value);
  zone_id = g_value_get_string (&value);

  zone = hyscan_planner_zone_copy (g_hash_table_lookup (priv->objects, zone_id));
  if (zone == NULL)
    goto exit;

  g_free (zone->name);
  zone->name = g_strdup (new_text);
  hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), zone_id, (HyScanObject *) zone);
  hyscan_planner_zone_free (zone);

exit:
  g_value_unset (&value);
}

/* Изменение скорости галса. */
static void
hyscan_gtk_planner_list_speed_edited (GtkCellRendererText *renderer,
                                      gchar               *path,
                                      gchar               *new_text,
                                      gpointer             user_data)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (user_data);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gint type;
  GtkTreeIter iter;
  const gchar *zone_id;
  GValue value = G_VALUE_INIT;
  HyScanPlannerTrack *track;
  gdouble speed;

  speed = g_strtod (new_text, NULL);
  if (speed <= 0)
    return;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (priv->store), &iter, path))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, TYPE_COLUMN, &type, -1);
  if (type != TYPE_PLANNER_TRACK)
    return;

  gtk_tree_model_get_value (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &value);
  zone_id = g_value_get_string (&value);

  track = hyscan_planner_track_copy (g_hash_table_lookup (priv->objects, zone_id));
  if (track == NULL)
    goto exit;

  track->plan.velocity = speed;
  hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), zone_id, (HyScanObject *) track);
  hyscan_planner_track_free (track);

exit:
  g_value_unset (&value);
}

/* Устанавливает свойства рендерера в зависимости от текущих данных. */
static void
hyscan_gtk_planner_list_cell_data (GtkTreeViewColumn *tree_column,
                                   GtkCellRenderer   *cell,
                                   GtkTreeModel      *tree_model,
                                   GtkTreeIter       *iter,
                                   gpointer           data)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (data);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gchar *text;
  gdouble value, err_value;
  gint type;
  gint weight;

  gtk_tree_model_get (tree_model, iter, TYPE_COLUMN, &type, WEIGHT_COLUMN, &weight, -1);
  g_object_set (cell, "weight", weight, NULL);

  /* Возможность редактирования ячейки. */
  if (tree_column == priv->title_col)
    {
      GValue value_id = G_VALUE_INIT;

      gtk_tree_model_get_value (GTK_TREE_MODEL (priv->store), iter, ID_COLUMN, &value_id);
      g_object_set (cell, "editable", type == TYPE_PLANNER_ZONE && g_value_get_string (&value_id) != NULL, NULL);
      g_value_unset (&value_id);
    }
  else if (tree_column == priv->speed_col)
    {
      g_object_set (cell, "editable", type == TYPE_PLANNER_TRACK, NULL);
    }

  /* Текст ячейки. */
  if (tree_column == priv->title_col && type == TYPE_PLANNER_TRACK)
    {
      guint number;

      gtk_tree_model_get (tree_model, iter, NUMBER_COLUMN, &number, -1);
      text = number > 0 ? g_strdup_printf (_("%s %d"), _("Plan"), number) : g_strdup (_("Plan"));
    }

  else if (tree_column == priv->speed_col)
    {
      gtk_tree_model_get (tree_model, iter, SPEED_COLUMN, &value, SPEED_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.2f\n<small>%.3f</small>", value, err_value);
      else
        text = g_strdup_printf ("%.2f", value);
    }

  else if (tree_column == priv->angle_col)
    {
      gtk_tree_model_get (tree_model, iter, ANGLE_COLUMN, &value, ANGLE_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.1f\n<small>%.2f</small>", value, err_value);
      else if (type == TYPE_PLANNER_TRACK)
        text = g_strdup_printf ("%.1f", value);
      else
        text = NULL;
    }

  else if (tree_column == priv->dist_col)
    {
      gtk_tree_model_get (tree_model, iter, LENGTH_COLUMN, &value, Y_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.0f\n<small>%.2f</small>", value, err_value);
      else
        text = g_strdup_printf ("%.0f", value);
    }

  else if (tree_column == priv->time_col)
    {
      gint min, sec;

      gtk_tree_model_get (tree_model, iter, TIME_COLUMN, &value, -1);

      min = value / 60;
      value -= min * 60;

      sec = value;

      text = g_strdup_printf ("%02d:%02d", min, sec);
    }

  else
    {
      return;
    }

  g_object_set (cell, "markup", text, NULL);
  g_free (text);
}

/* Обработчик выбора элементов в списке. */
static gboolean
hyscan_gtk_planner_list_selection (GtkTreeSelection *selection,
                                   GtkTreeModel     *model,
                                   GtkTreePath      *path,
                                   gboolean          path_currently_selected,
                                   gpointer          data)
{
  GtkTreeIter iter;
  gint item_type;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return FALSE;

  gtk_tree_model_get (model, &iter, TYPE_COLUMN, &item_type, -1);

  return item_type == TYPE_PLANNER_TRACK;
}

/* Восстанавливает выбор галсов в дереве. */
static void
hyscan_gtk_planner_list_restore_selection (HyScanGtkPlannerList  *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkTreeIter zone_iter, track_iter;
  gboolean zone_valid, track_valid;

  g_signal_handler_block (priv->tree_selection, priv->tree_sel_hndl);
  if (priv->tracks == NULL)
    {
      gtk_tree_selection_unselect_all (priv->tree_selection);
      goto exit;
    }

  zone_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &zone_iter);
  while (zone_valid)
    {
      track_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &track_iter, &zone_iter);
      while (track_valid)
        {
          gchar *track_id;

          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &track_iter, ID_COLUMN, &track_id, -1);
          if (g_strv_contains ((const gchar *const *) priv->tracks, track_id))
            gtk_tree_selection_select_iter (priv->tree_selection, &track_iter);
          else
            gtk_tree_selection_unselect_iter (priv->tree_selection, &track_iter);

          g_free (track_id);
          track_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &track_iter);
        }

      zone_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &zone_iter);
    }

exit:
  g_signal_handler_unblock (priv->tree_selection, priv->tree_sel_hndl);
}

/* Коллбэк функция, которая устанавливает активный галс жирным. */
static gboolean
hyscan_gtk_planner_list_activate (GtkTreeModel *model,
                                  GtkTreePath  *path,
                                  GtkTreeIter  *iter,
                                  gpointer      data)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (data);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gboolean active;

  if (priv->active == NULL)
    {
      active = FALSE;
    }
  else
    {
      gchar *id;

      gtk_tree_model_get (model, iter, ID_COLUMN, &id, -1);
      active = (g_strcmp0 (priv->active, id) == 0);

      g_free (id);
    }

  gtk_tree_store_set (GTK_TREE_STORE (model), iter, WEIGHT_COLUMN, active ? 800 : 400, -1);

  /* Продолжаем обход. */
  return FALSE;
}

/* Обработчик сигнала "activated"  модели выбранных объектов планировщика, т.е.
 * если пользователь активировал какой-то галс. */
static void
hyscan_gtk_planner_list_set_active (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  g_free (priv->active);
  priv->active = hyscan_planner_selection_get_active_track (priv->selection);
  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store), hyscan_gtk_planner_list_activate, list);
}

/* Обработчик сигнала "tracks-changed" модели выбранных объектов планировщика, т.е.
 * если пользователь изменил список выбранных галсов извне виджета. */
static void
hyscan_gtk_planner_list_tracks_changed (HyScanGtkPlannerList  *list,
                                        gchar                **tracks)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  g_clear_pointer (&priv->tracks, g_strfreev);
  priv->tracks = g_strdupv (tracks);

  hyscan_gtk_planner_list_restore_selection (list);
}

static void
hyscan_gtk_planner_list_db_trk_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GHashTableIter iter;
  GHashTable *tracks, *tracks_by_id;
  HyScanTrackStatsInfo *track_var;
  GHashTable *added_tracks;

  GtkTreeIter db_track_iter, zone_iter, track_iter;
  gboolean zone_valid;

  /* Формируем таблицу: ид галса - HyScanTrackInfo. */
  tracks = hyscan_track_stats_get (priv->stats);
  tracks_by_id = g_hash_table_new (g_str_hash, g_str_equal);
  if (tracks != NULL)
    {
      g_hash_table_iter_init (&iter, tracks);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_var))
        {
          if (track_var->info->id == NULL)
            continue;

          g_hash_table_insert (tracks_by_id, (gpointer) track_var->info->id, track_var);
        }
    }

  /* Удаляем из списка все, чего больше нет в хэш-таблице. */
  added_tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  zone_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &zone_iter);
  while (zone_valid)
    {
      gboolean track_valid;

      track_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &track_iter, &zone_iter);
      while (track_valid)
        {
          HyScanPlannerTrack *track;
          gchar *track_id, *db_track_id;
          gboolean db_track_valid;
          gint i;

          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &track_iter, ID_COLUMN, &track_id, -1);
          track = g_hash_table_lookup (priv->objects, track_id);
          if (track == NULL || track->type != HYSCAN_PLANNER_TRACK)
            goto next_track;

          /* Удаляем галсы, которых больше нет в track->records. */
          db_track_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &db_track_iter, &track_iter);
          while (db_track_valid)
            {
              gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &db_track_iter, ID_COLUMN, &db_track_id, -1);
              if (track->records != NULL && g_strv_contains ((const gchar *const *) track->records, db_track_id))
                {
                  db_track_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &db_track_iter);
                  g_hash_table_add (added_tracks, db_track_id);
                }
              else
                {
                  db_track_valid = gtk_tree_store_remove (priv->store, &db_track_iter);
                  g_free (db_track_id);
                }
            }

          if (track->records == NULL)
            goto next_track;

          /* Добавляем галсы, которые есть в track->records, но еще нет в списке. */
          for (i = 0; track->records[i] != NULL; i++)
            {
              HyScanTrackInfo *info;

              if (g_hash_table_contains (added_tracks, track->records[i]))
                continue;

              track_var = g_hash_table_lookup (tracks_by_id, track->records[i]);
              if (track_var == NULL)
                continue;

              info = track_var->info;

              gtk_tree_store_append (priv->store, &db_track_iter, &track_iter);
              gtk_tree_store_set (priv->store, &db_track_iter,
                                  TYPE_COLUMN, TYPE_DB_TRACK,
                                  ID_COLUMN, info->id,
                                  NAME_COLUMN, info->name,
                                  SPEED_COLUMN, track_var->velocity,
                                  SPEED_VAR_COLUMN, track_var->velocity_var,
                                  ANGLE_COLUMN, track_var->angle,
                                  ANGLE_VAR_COLUMN, track_var->angle_var,
                                  LENGTH_COLUMN, track_var->x_length,
                                  Y_VAR_COLUMN, track_var->y_var,
                                  TIME_COLUMN, 1e-6 * (gdouble) (track_var->end_time - track_var->start_time),
                                  WEIGHT_COLUMN, 400,
                                  -1);
            }

        next_track:
          g_free (track_id);
          track_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &track_iter);
        }

      zone_valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &zone_iter);
    }

  g_hash_table_destroy (added_tracks);
  g_hash_table_destroy (tracks_by_id);
  g_clear_pointer (&tracks, g_hash_table_destroy);
}

/* Удаляет из списка пропавшие элементы, а из оставшихся формирует хэш таблицу. */
static GHashTable *
hyscan_gtk_planner_list_map_existing (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GtkTreeIter tr_iter_zn, tr_iter_tk;
  gchar *zone_id, *track_id;
  HyScanPlannerTrack *track;
  gboolean valid;
  GHashTable *tree_iter_ht;

  tree_iter_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &tr_iter_zn);
  while (valid)
    {
      gboolean keep_zone;
      gboolean tk_valid;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &tr_iter_zn, ID_COLUMN, &zone_id, -1);
      if (zone_id == NULL)
        {
          keep_zone = TRUE;
          g_hash_table_insert (tree_iter_ht, g_strdup (OTHER_ZONE), gtk_tree_iter_copy (&tr_iter_zn));
        }
      else if (g_hash_table_contains (priv->objects, zone_id))
        {
          keep_zone = TRUE;
          g_hash_table_insert (tree_iter_ht, g_strdup (zone_id), gtk_tree_iter_copy (&tr_iter_zn));
        }
      else
        {
          keep_zone = FALSE;
        }

      tk_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &tr_iter_tk, &tr_iter_zn);
      while (tk_valid)
        {
          gboolean keep_track;

          if (keep_zone)
            {
              gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &tr_iter_tk, ID_COLUMN, &track_id, -1);

              /* Проверяем, что галс существует и находится в нужной группе. */
              if ((track = g_hash_table_lookup (priv->objects, track_id)) != NULL)
                {
                  const gchar *parent_id;

                  if (track->zone_id != NULL && g_hash_table_contains (priv->objects, track->zone_id))
                    parent_id = track->zone_id;
                  else
                    parent_id = NULL;

                  keep_track = (g_strcmp0 (zone_id, parent_id) == 0);
                }

              /* Объекта с таким id больше нет — удаляем его. */
              else
                {
                  keep_track = FALSE;
                }

              if (keep_track)
                g_hash_table_insert (tree_iter_ht, track_id, gtk_tree_iter_copy (&tr_iter_tk));
              else
                g_free (track_id);
            }
          else
            {
              keep_track = FALSE;
            }

          tk_valid = keep_track ? gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &tr_iter_tk) :
                                  gtk_tree_store_remove (priv->store, &tr_iter_tk);
        }

      g_free (zone_id);
      valid = keep_zone ? gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &tr_iter_zn) :
                          gtk_tree_store_remove (priv->store, &tr_iter_zn);
    }

  return tree_iter_ht;
}

/* Обновляет информацию о галсах в виджете. */
static GHashTable *
hyscan_gtk_planner_list_update_tracks (HyScanGtkPlannerList *list,
                                       GHashTable           *tree_iter_ht)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GHashTableIter objects_iter;
  GtkTreeIter tr_iter_zn, tr_iter_tk;
  HyScanGtkPlannerListZone *zone_info;
  gchar *track_id;
  HyScanPlannerTrack *track;
  GHashTable *zone_info_ht;

  zone_info_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_gtk_planner_list_zone_free);

  /* Добавляем в список все галсы, которые есть в модели данных планировщика. */
  g_hash_table_iter_init (&objects_iter, priv->objects);
  while (g_hash_table_iter_next (&objects_iter, (gpointer *) &track_id, (gpointer *) &track))
    {
      GtkTreeIter *iter;
      const gchar *parent_key;

      if (track->type != HYSCAN_PLANNER_TRACK)
        continue;

      /* ИД родительского элемента галса. */
      if (track->zone_id != NULL && g_hash_table_contains (priv->objects, track->zone_id))
        parent_key = track->zone_id;
      else
        parent_key = OTHER_ZONE;

      /* Получаем указатель на итератор галса в списке GtkTreeStore. */
      iter = g_hash_table_lookup (tree_iter_ht, track_id);
      if (iter == NULL)
        {
          GtkTreeIter *parent_iter;

          parent_iter = g_hash_table_lookup (tree_iter_ht, parent_key);
          if (parent_iter == NULL)
            {
              gtk_tree_store_append (priv->store, &tr_iter_zn, NULL);
              parent_iter = gtk_tree_iter_copy (&tr_iter_zn);
              g_hash_table_insert (tree_iter_ht, g_strdup (parent_key), parent_iter);
            }

          gtk_tree_store_append (priv->store, &tr_iter_tk, parent_iter);
          iter = &tr_iter_tk;
        }

        /* Устанавливаем свойства галса. */
        gdouble track_dist, track_time;

        if ((zone_info = g_hash_table_lookup (zone_info_ht, parent_key)) == NULL)
          {
            zone_info = hyscan_gtk_planner_list_zone_new ();
            g_hash_table_insert (zone_info_ht, g_strdup (parent_key), zone_info);
          }

        track_dist = hyscan_planner_track_length (&track->plan);
        track_time = track_dist / track->plan.velocity;
        zone_info->time += track_time;
        zone_info->length += track_dist;

        gtk_tree_store_set (priv->store, iter,
                            TYPE_COLUMN, TYPE_PLANNER_TRACK,
                            ID_COLUMN, track_id,
                            SPEED_COLUMN, track->plan.velocity,
                            ANGLE_COLUMN, RAD2DEG (hyscan_planner_track_angle (track)),
                            NUMBER_COLUMN, track->number,
                            LENGTH_COLUMN, track_dist,
                            TIME_COLUMN, track_time,
                            WEIGHT_COLUMN, g_strcmp0 (track_id, priv->active) == 0 ? 800 : 400,
                            -1);
    }

  return zone_info_ht;
}

/* Обновляет информацию о зонах в виджете. */
static void
hyscan_gtk_planner_list_update_zones (HyScanGtkPlannerList *list,
                                      GHashTable           *tree_iter_ht,
                                      GHashTable           *zone_info_ht)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GHashTableIter objects_iter;
  GtkTreeIter tr_iter_zn;
  HyScanGtkPlannerListZone *zone_info;
  gchar *zone_id;
  HyScanPlannerZone *zone;

  /* Добавляем в список все зоны, которые есть в модели данных планировщика. */
  g_hash_table_iter_init (&objects_iter, priv->objects);
  while (g_hash_table_iter_next (&objects_iter, (gpointer *) &zone_id, (gpointer *) &zone))
    {
      GtkTreeIter *iter;

      if (zone->type != HYSCAN_PLANNER_ZONE)
        continue;

      iter = g_hash_table_lookup (tree_iter_ht, zone_id);
      if (iter == NULL)
        {
          gtk_tree_store_append (priv->store, &tr_iter_zn, NULL);
          iter = &tr_iter_zn;
        }

      zone_info = g_hash_table_lookup (zone_info_ht, zone_id);
      gtk_tree_store_set (priv->store, iter,
                          NAME_COLUMN, zone->name,
                          ID_COLUMN, zone_id,
                          SPEED_COLUMN, (zone_info != NULL && zone_info->time > 0) ? zone_info->length / zone_info->time : 0,
                          LENGTH_COLUMN, zone_info != NULL ? zone_info->length : 0,
                          TIME_COLUMN, zone_info != NULL ? zone_info->time : 0,
                          TYPE_COLUMN, TYPE_PLANNER_ZONE,
                          WEIGHT_COLUMN, 400,
                          -1);
    }

  GtkTreeIter *iter;

  /* Обновляем информацию о группе свободных галсов. */
  if ((zone_info = g_hash_table_lookup (zone_info_ht, OTHER_ZONE)) != NULL)
    {
      iter = g_hash_table_lookup (tree_iter_ht, OTHER_ZONE);
      if (iter == NULL)
        {
          gtk_tree_store_append (priv->store, &tr_iter_zn, NULL);
          iter = &tr_iter_zn;
        }

      gtk_tree_store_set (priv->store, iter,
                          NAME_COLUMN, _("Free tracks"),
                          ID_COLUMN, NULL,
                          SPEED_COLUMN, zone_info->time > 0 ? zone_info->length / zone_info->time : 0,
                          LENGTH_COLUMN, zone_info->length,
                          TIME_COLUMN, zone_info->time,
                          TYPE_COLUMN, TYPE_PLANNER_ZONE,
                          WEIGHT_COLUMN, 400,
                          -1);
    }
  /* Удаляем группу свободных галсов, если она пустая. */
  else if ((iter = g_hash_table_lookup (tree_iter_ht, OTHER_ZONE)) != NULL)
    {
      gtk_tree_store_remove (priv->store, iter);
    }
}

/* Обработчик сигнала "changed" модели объектов. */
static void
hyscan_gtk_planner_list_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GHashTable *tree_iter_ht, *zone_info_ht;

  g_signal_handler_block (priv->tree_selection, priv->tree_sel_hndl);

  /* Устанавливаем новый список объектов в модель данных. */
  g_clear_pointer (&priv->objects, g_hash_table_unref);
  priv->objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));

  /* Хэш таблица элементов списка. */
  tree_iter_ht = hyscan_gtk_planner_list_map_existing (list);

  /* Хэш таблица с информацией о зонах. */
  zone_info_ht = hyscan_gtk_planner_list_update_tracks (list, tree_iter_ht);

  hyscan_gtk_planner_list_update_zones (list, tree_iter_ht, zone_info_ht);

  hyscan_gtk_planner_list_restore_selection (list);

  g_hash_table_destroy (zone_info_ht);
  g_hash_table_destroy (tree_iter_ht);

  g_signal_handler_unblock (priv->tree_selection, priv->tree_sel_hndl);
}

/* Обработчик сигнала "changed" выбора в виджете, т.е. если пользователь
 * изменил выбор внутри виджета. */
static void
hyscan_gtk_planner_list_selection_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GList *selected_list, *link;
  gint i = 0;

  g_signal_handler_block (priv->selection, priv->planner_sel_hndl);

  /* Формируем список идентификаторов выбранных галсов. */
  selected_list = gtk_tree_selection_get_selected_rows (priv->tree_selection, NULL);
  g_clear_pointer (&priv->tracks, g_strfreev);
  priv->tracks = g_new (gchar *, g_list_length (selected_list) + 1);
  for (link = selected_list; link != NULL; link = link->next)
    {
      GtkTreePath *path = link->data;
      GtkTreeIter iter;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &priv->tracks[i++], -1);
    }
  priv->tracks[i] = NULL;

  hyscan_planner_selection_set_tracks (priv->selection, priv->tracks);

  g_list_free_full (selected_list, (GDestroyNotify) gtk_tree_path_free);

  g_signal_handler_unblock (priv->selection, priv->planner_sel_hndl);
}


/* Разрешает перетаскивать только галсы и только при сортировке по номеру галса. */
static gboolean
hyscan_gtk_planner_tree_store_row_draggable (GtkTreeDragSource *drag_source,
                                             GtkTreePath       *path)
{
  GtkTreeModel *model = GTK_TREE_MODEL (drag_source);
  gint type;
  GtkTreeIter iter;

  gint sort_column_id;
  GtkSortType order;
  if (!gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (drag_source), &sort_column_id, &order))
    return FALSE;

  if (sort_column_id != NUMBER_COLUMN || order != GTK_SORT_ASCENDING)
    return FALSE;

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return FALSE;

  gtk_tree_model_get (model, &iter, TYPE_COLUMN, &type, -1);

  return type == TYPE_PLANNER_TRACK;
}

static gboolean
hyscan_gtk_planner_tree_store_row_drop_possible (GtkTreeDragDest  *drag_dest,
                                                 GtkTreePath      *dest_path,
                                                 GtkSelectionData *selection_data)
{
  gboolean result = FALSE;
  GtkTreePath *src_path;
  GtkTreeModel *src_model;

  if (!gtk_tree_get_row_drag_data (selection_data, &src_model, &src_path))
    goto exit;
  
  /* Может перетаскивать только внутри нашей модели */
  if (src_model != GTK_TREE_MODEL (drag_dest))
    goto exit;

  /* */
  gint sort_column_id;
  GtkSortType order;
  if (!gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (drag_dest), &sort_column_id, &order))
    goto exit;

  if (sort_column_id != NUMBER_COLUMN || order != GTK_SORT_ASCENDING)
    goto exit;

  /* Разрешаем перемещать галс только в рамках его родительской зоны. */
  if (!gtk_tree_path_up (src_path))
    goto exit;

  result = (gtk_tree_path_get_depth (dest_path) == gtk_tree_path_get_depth (src_path) + 1) &&
            gtk_tree_path_is_ancestor (src_path, dest_path);

exit:
  g_clear_pointer (&src_path, gtk_tree_path_free);

  return result;
}

/* Обработка строк после завершения DND. */
static gboolean
hyscan_gtk_planner_tree_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                                GtkTreePath       *path)
{
  GtkTreeModel *tree_model = GTK_TREE_MODEL (drag_source);
  HyScanGtkPlannerListPrivate *priv = HYSCAN_GTK_PLANNER_TREE_STORE (drag_source)->list->priv;
  gboolean result;
  GtkTreeIter iter, parent;
  gboolean valid;
  GtkTreeIter selected_iter;
  GList *selected;
  gboolean any_selected;

  /* Получаем итератор с родителем удалённого элемента */
  valid = gtk_tree_model_get_iter (tree_model, &iter, path) &&
          gtk_tree_model_iter_parent (tree_model, &parent, &iter);

  /* DRAG_HACKING-2: запоминаем выбранную ранее строку (см DRAG_HACKING-1). */
  selected = gtk_tree_selection_get_selected_rows (priv->tree_selection, NULL);
  any_selected = (selected != NULL);
  if (any_selected)
    gtk_tree_model_get_iter (tree_model, &selected_iter, selected->data);
  g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

  /* Даём родительскому классу удалить строку.
   * В этот момент GtkTreeStore устанавливает выбор на строку по-соседству с удалённой.
   * Нам такое поведение не нравится, мы хотим сохранить выбор на перетаскиваемой строке.
   * Поэтому делаем DRAG_HACKING. */
  result = hyscan_gtk_layer_parent_src_iface->drag_data_delete (drag_source, path);
  if (!result)
    return result;

  /* DRAG_HACKING-3: Восстанавливаем выбор. */
  gtk_tree_selection_unselect_all (priv->tree_selection);
  gtk_tree_selection_select_iter (priv->tree_selection, &selected_iter);

  /* Обновляем порядковые номера в рамках изменённой зоны. */
  guint number = 0;
  valid = valid && gtk_tree_model_iter_children (tree_model, &iter, &parent);
  while (valid)
    {
      gchar *track_id;
      HyScanPlannerTrack *track, *modified_track;

      gtk_tree_model_get (tree_model, &iter, ID_COLUMN, &track_id, -1);
      number++;

      track = g_hash_table_lookup (priv->objects, track_id);
      if (track == NULL || track->type != HYSCAN_PLANNER_TRACK)
        goto next;

      if (track->number == number)
        goto next;

      modified_track = hyscan_planner_track_copy (track);
      modified_track->number = number;
      hyscan_object_model_modify_object (HYSCAN_OBJECT_MODEL (priv->model), track_id, (HyScanObject *) modified_track);
      hyscan_planner_track_free (modified_track);

    next:
      g_free (track_id);
      valid = gtk_tree_model_iter_next (tree_model, &iter);
    }

  return result;
}

static gboolean
hyscan_gtk_planner_tree_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                                  GtkTreePath       *dest,
                                                  GtkSelectionData  *selection_data)
{
  HyScanGtkPlannerTreeStore *store = HYSCAN_GTK_PLANNER_TREE_STORE (drag_dest);
  HyScanGtkPlannerListPrivate *priv = store->list->priv;

  if (!hyscan_gtk_layer_parent_dest_iface->drag_data_received (drag_dest, dest, selection_data))
    return FALSE;

  /* DRAG_HACKING-1: Мы хотим, чтобы выбор оставался на той строке, которую перетаскивает пользователь.
   * Выбираем только её. Продолжение в DRAG_HACKING-2. */
  gtk_tree_selection_unselect_all (priv->tree_selection);
  gtk_tree_selection_select_path (priv->tree_selection, dest);

  return TRUE;
}

static void
hyscan_gtk_planner_tree_store_source_iface_init (GtkTreeDragSourceIface *iface)
{
  hyscan_gtk_layer_parent_src_iface = g_type_interface_peek_parent (iface);
  iface->row_draggable = hyscan_gtk_planner_tree_store_row_draggable;
  iface->drag_data_delete = hyscan_gtk_planner_tree_store_drag_data_delete;
}

static void
hyscan_gtk_planner_tree_store_dest_iface_init (GtkTreeDragDestIface *iface)
{
  hyscan_gtk_layer_parent_dest_iface = g_type_interface_peek_parent (iface);
  iface->row_drop_possible = hyscan_gtk_planner_tree_store_row_drop_possible;
  iface->drag_data_received = hyscan_gtk_planner_tree_store_drag_data_received;
}

/**
 * hyscan_gtk_planner_list_new:
 * @model: модель объектов планировщика #HyScanPlannerModel
 * @selection: модель выбранных объектов планировщика
 * @stats: модель статистики по галсам #HyScanTrackStats
 * @viewer: (nullable): слой планировщика на карте
 *
 * Создаёт виджет списка плановых галсов. Параметр @viewer является необязательным
 * и требуется только для возможности перехода к галсу на карте.
 *
 * Returns: виджет #HyScanGtkPlannerList
 */
GtkWidget *
hyscan_gtk_planner_list_new (HyScanPlannerModel     *model,
                             HyScanPlannerSelection *selection,
                             HyScanTrackStats       *stats,
                             HyScanGtkMapPlanner    *viewer)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_LIST,
                       "model", model,
                       "stats", stats,
                       "selection", selection,
                       "viewer", viewer,
                       NULL);
}

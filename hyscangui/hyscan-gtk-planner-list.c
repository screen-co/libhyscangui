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
 * плановых галсов внутри них, а также записей этих галсов.
 *
 * Виджет имеет контекстное меню для манипуляции галсами:
 * - переход к галсу на карте,
 * - активация галса (для навигации),
 * - выделение нескольких галсов,
 * - изменение размеров и направления,
 * - удаление.
 *
 * Для каждого из планов галса отображаются галсы, которые были по этому плану
 * записаны. Пользователь также может самостоятельно связать запись галса с планом.
 * Если связанный с галсом план не соотвествует исходному плану, то в строке этого
 * галса выводится предупреждение, а в его контекстном меню появляется
 * возможность восстановить исходный план.
 *
 */

#include "hyscan-gtk-planner-list.h"
#include <glib/gi18n-lib.h>
#include <hyscan-track-stats.h>

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
  TYPE_PLANNER_TRACK,   /* Запланированный галс. */
  TYPE_PLANNER_ZONE,    /* Зона полигона. */
  TYPE_DB_TRACK,        /* Галс записанный. */
};

enum
{
  NUMBER_COLUMN,             /* Номер. */
  KEY_COLUMN,                /* Ключ объекта в хэш-таблице. */
  ID_COLUMN,                 /* Идентификатор объекта. */
  NAME_COLUMN,               /* Название объекта. */
  TIME_COLUMN,               /* Время. */
  LENGTH_COLUMN,             /* Длина. */
  Y_VAR_COLUMN,              /* Вариация отклонения поперёк галса. */
  ANGLE_COLUMN,              /* Курс. */
  ANGLE_VAR_COLUMN,          /* Вариация курса. */
  SPEED_COLUMN,              /* Скорость. */
  SPEED_VAR_COLUMN,          /* Вариация скорости. */
  PROGRESS_COLUMN,           /* Прогресс. */
  QUALITY_COLUMN,            /* Качество. */
  TYPE_COLUMN,               /* Тип объекта. */
  WEIGHT_COLUMN,             /* Толщина шрифта. */
  INCONSISTENT_COLUMN,       /* Признак того, что текущий план галса не соответствует тому, по которому он записан. */
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
G_DEFINE_TYPE_WITH_CODE (HyScanGtkPlannerTreeStore, hyscan_gtk_planner_tree_store, GTK_TYPE_TREE_STORE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                hyscan_gtk_planner_tree_store_source_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
                                                hyscan_gtk_planner_tree_store_dest_iface_init))

static GtkTreeDragSourceIface *hyscan_gtk_layer_parent_src_iface;
static GtkTreeDragDestIface *hyscan_gtk_layer_parent_dest_iface;

struct _HyScanGtkPlannerListPrivate
{
  HyScanPlannerModel           *model;              /* Модель объектов планировщика. */
  HyScanPlannerSelection       *selection;          /* Модель выбранных объектов планировщика. */
  HyScanDBInfo                 *db_info;            /* Модель информации о галсах проекта. */
  HyScanGtkMapPlanner          *viewer;             /* Слой для просмотора галсов. */
  HyScanPlannerStats           *stats;              /* Модель данных схемы галсов и статистики ее прохождения. */
  GtkTreeStore                 *store;              /* Модель данных дерева. */
  GtkTreeSelection             *tree_selection;     /* Объект выбранных элементов дерева. */
  gchar                        *zone_id;            /* Идентификатор выбранной зоны. */
  gchar                       **tracks;             /* Идентификаторы выбранных плановых галсов. */
  gchar                       **records;            /* Идентификаторы выбранных записей галсов. */
  gchar                        *active;             /* Идентификатор активного галса. */
  gulong                        hndl_tree_chgd;     /* Обработчик сигнала "changed" объекта tree_selection. */
  gulong                        hndl_tk_chgd;       /* Обработчик сигнала "tracks-changed" объекта selection. */
  gulong                        hndl_zn_chgd;       /* Обработчик сигнала "zone-changed" объекта selection. */
  GHashTable                   *objects;            /* Объекты планировщика. */
  GHashTable                   *zone_stats;         /* Хэш таблица статистики схемы галсы. */

  struct
  {
    GtkMenu                    *menu;                 /* Виджет меню. */
    gint                        user_position;        /* Позиция для добавления пользовательских элементов. */
    GtkWidget                  *nav;                  /* Виджет пункта меню "Навигация". */
    GtkWidget                  *find;                 /* Виджет пункта меню "Найти". */
    GtkWidget                  *inv;                  /* Виджет пункта меню "Сменить направление". */
    GtkWidget                  *del;                  /* Виджет пункта меню "Удалить". */
    GtkWidget                  *extend;               /* Виджет пункта меню "Расятнуть до границ". */
    GtkWidget                  *bind;                 /* Виджет пункта меню "Галсы" */
    GtkWidget                  *bind_submenu;         /* Виджет подменю "Галсы" */
  }                             menu;                 /* Контекстное меню плана галса. */

  struct
  {
    GtkMenu                    *menu;                 /* Виджет меню. */
    GtkWidget                  *restore_plan;         /* Виджет пункта меню "Восстановить план". */
  }                             rec_menu;             /* Контекстное меню галса. */

  GtkTreeViewColumn            *title_col;            /* Столбец "Название". */
  GtkTreeViewColumn            *y_sd_col;             /* Столбец "Отклонение по Y". */
  GtkTreeViewColumn            *length_col;           /* Столбец "Длина". */
  GtkTreeViewColumn            *time_col;             /* Столбец "Время". */
  GtkTreeViewColumn            *velocity_col;         /* Столбец "Скорость". */
  GtkTreeViewColumn            *velocity_sd_col;      /* Столбец "Std. Dev. скорости". */
  GtkTreeViewColumn            *track_col;            /* Столбец "Угол". */
  GtkTreeViewColumn            *progress_col;         /* Столбец "Прогресс". */
  GtkTreeViewColumn            *track_sd_col;         /* Столбец "Std. Dev. курса". */
  GtkTreeViewColumn            *quality_col;          /* Столбец "Качество". */
};

static void                       hyscan_gtk_planner_list_set_property       (GObject                   *object,
                                                                              guint                      prop_id,
                                                                              const GValue              *value,
                                                                              GParamSpec                *pspec);
static void                       hyscan_gtk_planner_list_object_constructed (GObject                   *object);
static void                       hyscan_gtk_planner_list_object_finalize    (GObject                   *object);
static gboolean                   hyscan_gtk_planner_list_query_tooltip      (GtkWidget                 *widget,
                                                                              gint                       x,
                                                                              gint                       y,
                                                                              gboolean                   keyboard_tooltip,
                                                                              GtkTooltip                *tooltip);
static void                       hyscan_gtk_planner_list_restore_selection  (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_changed            (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_stats_changed      (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_selection_changed  (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_tracks_changed     (HyScanGtkPlannerList      *list,
                                                                              gchar                    **tracks);
static void                       hyscan_gtk_planner_list_zone_changed       (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_set_active         (HyScanGtkPlannerList      *list);
static void                       hyscan_gtk_planner_list_text_data          (GtkTreeViewColumn         *tree_column,
                                                                              GtkCellRenderer           *cell,
                                                                              GtkTreeModel              *tree_model,
                                                                              GtkTreeIter               *iter,
                                                                              gpointer                   data);
static void                       hyscan_gtk_planner_list_icon_data          (GtkTreeViewColumn         *tree_column,
                                                                              GtkCellRenderer           *cell,
                                                                              GtkTreeModel              *tree_model,
                                                                              GtkTreeIter               *iter,
                                                                              gpointer                   data);
static void                       hyscan_gtk_planner_list_row_expanded       (HyScanGtkPlannerList      *list,
                                                                              GtkTreeIter               *iter,
                                                                              GtkTreePath               *path);
static gboolean                   hyscan_gtk_planner_list_button_press       (HyScanGtkPlannerList      *list,
                                                                              GdkEventButton            *event_button);
static inline void                hyscan_gtk_planner_list_add_menu           (HyScanGtkPlannerList      *list,
                                                                              GtkMenu                   *menu,
                                                                              const gchar               *mnemonic,
                                                                              GtkWidget                **widget);
static void                       hyscan_gtk_planner_list_update_zones       (HyScanGtkPlannerList      *list,
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
static GtkTreeViewColumn *        hyscan_gtk_planner_list_add_title          (HyScanGtkPlannerList      *list);
static GtkTreeViewColumn *        hyscan_gtk_planner_list_add                (HyScanGtkPlannerList      *list,
                                                                              const gchar               *title,
                                                                              const gchar               *description,
                                                                              gint                       sort_column_id,
                                                                              GCallback                  edited);
static void                       hyscan_gtk_planner_list_bind_menu_update   (HyScanGtkPlannerList      *list,
                                                                              const gchar               *planner_track_id);

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_planner_list_set_property;

  object_class->constructed = hyscan_gtk_planner_list_object_constructed;
  object_class->finalize = hyscan_gtk_planner_list_object_finalize;

  widget_class->query_tooltip = hyscan_gtk_planner_list_query_tooltip;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Planner Model", "Planner model", HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "Planner Selection", "Planner selection", HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_STATS,
    g_param_spec_object ("stats", "Planner Stats", "HyScanPlannerStats", HYSCAN_TYPE_PLANNER_STATS,
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

  G_OBJECT_CLASS (hyscan_gtk_planner_list_parent_class)->constructed (object);

  gtk_tree_view_set_enable_tree_lines (tree_view, TRUE);
  gtk_tree_view_set_grid_lines (tree_view, GTK_TREE_VIEW_GRID_LINES_BOTH);
  gtk_tree_view_set_reorderable (tree_view, TRUE);
  gtk_tree_view_set_search_column (tree_view, -1);

  /* Модель данных дерева. */
  priv->store = hyscan_gtk_planner_tree_store_new (list);
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (priv->store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (priv->store), NUMBER_COLUMN, GTK_SORT_ASCENDING);

  /* Настройка выбора. */
  priv->tree_selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (priv->tree_selection, GTK_SELECTION_MULTIPLE);

  /* Столбцы. */
  priv->title_col = hyscan_gtk_planner_list_add_title (list);
  priv->progress_col = hyscan_gtk_planner_list_add (list, _("Done"), _("Progress Done, %"), PROGRESS_COLUMN, NULL);
  priv->quality_col = hyscan_gtk_planner_list_add (list, _("Quality"), _("Quality (from 0 to 100)"), QUALITY_COLUMN, NULL);
  priv->length_col = hyscan_gtk_planner_list_add (list, _("L"), _("Length Along Track, m"), LENGTH_COLUMN, NULL);
  priv->time_col = hyscan_gtk_planner_list_add (list, _("Time"), _("Time, hh:mm:ss"), TIME_COLUMN, NULL);
  priv->velocity_col = hyscan_gtk_planner_list_add (list, _("Vel"), _("Velocity, m/s"), SPEED_COLUMN,
                                                    G_CALLBACK (hyscan_gtk_planner_list_speed_edited));
  priv->track_col = hyscan_gtk_planner_list_add (list, _("Trk, °"), _("Track, °"), ANGLE_COLUMN, NULL);
  priv->y_sd_col = hyscan_gtk_planner_list_add (list, _("SD(y)"), _("Std Dev Across Track, m"), Y_VAR_COLUMN, NULL);
  priv->velocity_sd_col = hyscan_gtk_planner_list_add (list, _("SD(Vel)"), _("Std Dev of Velocity, m/s"), SPEED_VAR_COLUMN, NULL);
  priv->track_sd_col = hyscan_gtk_planner_list_add (list, _("SD(Trk)"), _("Std Dev of Track, °"), ANGLE_VAR_COLUMN, NULL);

  /* Контекстное меню плана. */
  priv->menu.menu = GTK_MENU (gtk_menu_new ());
  gtk_menu_attach_to_widget (priv->menu.menu, GTK_WIDGET (list), NULL);
  if (priv->viewer != NULL)
     hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, _("_Find track on the map"), &priv->menu.find);
  hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, _("Start _navigation"), &priv->menu.nav);

  priv->menu.user_position = 3;

  hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, NULL, NULL);
  hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, _("_Invert"), &priv->menu.inv);
  hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, _("Adjust length to _zone boundary"), &priv->menu.extend);
  hyscan_gtk_planner_list_add_menu (list, priv->menu.menu, _("_Delete"), &priv->menu.del);

  /* Контекстное меню галса. */
  priv->rec_menu.menu = GTK_MENU (gtk_menu_new ());
  gtk_menu_attach_to_widget (priv->rec_menu.menu, GTK_WIDGET (list), NULL);
  hyscan_gtk_planner_list_add_menu (list, priv->rec_menu.menu, _("_Restore original plan"), &priv->rec_menu.restore_plan);

  priv->hndl_tree_chgd = g_signal_connect_swapped (priv->tree_selection, "changed",
                                                   G_CALLBACK (hyscan_gtk_planner_list_selection_changed), list);
  priv->hndl_tk_chgd = g_signal_connect_swapped (priv->selection, "tracks-changed",
                                                 G_CALLBACK (hyscan_gtk_planner_list_tracks_changed), list);
  priv->hndl_zn_chgd = g_signal_connect_swapped (priv->selection, "zone-changed",
                                                 G_CALLBACK (hyscan_gtk_planner_list_zone_changed), list);

  /* Всплываюшая подсказка. */
  gtk_widget_set_has_tooltip (GTK_WIDGET (list), TRUE);

  g_signal_connect_swapped (priv->stats, "changed", G_CALLBACK (hyscan_gtk_planner_list_stats_changed), list);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_planner_list_set_active), list);
  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_planner_list_changed), list);
  g_signal_connect_swapped (tree_view, "row-expanded", G_CALLBACK (hyscan_gtk_planner_list_row_expanded), list);
  g_signal_connect_swapped (tree_view, "button-press-event", G_CALLBACK (hyscan_gtk_planner_list_button_press), list);

  /* Загружаем данные из моделей. */
  hyscan_gtk_planner_list_stats_changed (list);
  hyscan_gtk_planner_list_changed (list);
  hyscan_gtk_planner_list_set_active (list);
}

static void
hyscan_gtk_planner_list_object_finalize (GObject *object)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (object);
  HyScanGtkPlannerListPrivate *priv = list->priv;

  g_signal_handlers_disconnect_by_data (priv->selection, list);
  g_signal_handlers_disconnect_by_data (priv->model, list);
  g_clear_pointer (&priv->zone_stats, g_hash_table_unref);
  g_clear_object (&priv->model);
  g_clear_object (&priv->selection);
  g_clear_object (&priv->viewer);
  g_clear_object (&priv->stats);
  g_clear_object (&priv->db_info);
  g_clear_pointer (&priv->objects, g_hash_table_unref);
  g_clear_pointer (&priv->tracks, g_strfreev);
  g_clear_pointer (&priv->records, g_strfreev);
  g_free (priv->zone_id);
  g_free (priv->active);

  G_OBJECT_CLASS (hyscan_gtk_planner_list_parent_class)->finalize (object);
}

static gboolean
hyscan_gtk_planner_list_query_tooltip (GtkWidget  *widget,
                                       gint        x,
                                       gint        y,
                                       gboolean    keyboard_tooltip,
                                       GtkTooltip *tooltip)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint type;
  gboolean inconsistent;

  if (!gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget), &x, &y, keyboard_tooltip, &model, NULL, &iter))
    return FALSE;

  gtk_tree_model_get (model, &iter, TYPE_COLUMN, &type, INCONSISTENT_COLUMN, &inconsistent, -1);

  if (type != TYPE_DB_TRACK || !inconsistent)
    return FALSE;

  gtk_tooltip_set_text (tooltip, _("Track has been recorded with another plan. The original plan can be restored from the context menu."));

  return TRUE;
}

static GtkTreeViewColumn *
hyscan_gtk_planner_list_add_title (HyScanGtkPlannerList *list)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *text_cell, *icon_cell;

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Title"));

  /* Иконка. */
  icon_cell = gtk_cell_renderer_pixbuf_new ();
  g_object_set (icon_cell,
                "stock-size", GTK_ICON_SIZE_SMALL_TOOLBAR,
                "icon-name", "dialog-warning-symbolic",
                NULL);
  gtk_tree_view_column_pack_start (column, icon_cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, icon_cell, hyscan_gtk_planner_list_icon_data, list, NULL);

  /* Текст. */
  text_cell = gtk_cell_renderer_text_new ();
  g_object_set (text_cell,
                "width-chars", 16,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  gtk_tree_view_column_pack_start (column, text_cell, TRUE);
  gtk_tree_view_column_set_attributes (column, text_cell, "text", NAME_COLUMN, NULL);
  gtk_tree_view_column_set_cell_data_func (column, text_cell, hyscan_gtk_planner_list_text_data, list, NULL);

  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, NUMBER_COLUMN);

  g_signal_connect (text_cell, "edited", G_CALLBACK (hyscan_gtk_planner_list_title_edited), list);

  gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
  gtk_tree_view_column_set_reorderable (column, TRUE);
  gtk_tree_view_column_set_resizable (column, TRUE);

  return column;
}

static GtkTreeViewColumn *
hyscan_gtk_planner_list_add (HyScanGtkPlannerList *list,
                             const gchar          *title,
                             const gchar          *description,
                             gint                  sort_column_id,
                             GCallback             edited)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkWidget *header;

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "alignment", PANGO_ALIGN_RIGHT, NULL);
  gtk_cell_renderer_set_alignment (renderer, 1.0, 0.0);
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, title);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);

  gtk_tree_view_column_set_cell_data_func (column, renderer, hyscan_gtk_planner_list_text_data, list, NULL);
  gtk_tree_view_column_set_sort_column_id (column, sort_column_id);
  if (edited != NULL)
    g_signal_connect (renderer, "edited", edited, list);

  /* Всплывающая подсказка для названия столбца. */
  header = gtk_tree_view_column_get_button (column);
  gtk_widget_set_tooltip_text (header, description);

  gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);
  gtk_tree_view_column_set_reorderable (column, TRUE);
  gtk_tree_view_column_set_resizable (column, TRUE);

  return column;
}

/* Создаёт наследника GtkTreeStore с особенностями в реализации Drag'n'drop (DND). */
static GtkTreeStore *
hyscan_gtk_planner_tree_store_new (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerTreeStore *store;
  GType types[] = { G_TYPE_UINT,       /* NUMBER_COLUMN */
                    G_TYPE_STRING,     /* KEY_COLUMN */
                    G_TYPE_STRING,     /* ID_COLUMN */
                    G_TYPE_STRING,     /* NAME_COLUMN */
                    G_TYPE_DOUBLE,     /* TIME_COLUMN */
                    G_TYPE_DOUBLE,     /* LENGTH_COLUMN */
                    G_TYPE_DOUBLE,     /* Y_VAR_COLUMN */
                    G_TYPE_DOUBLE,     /* ANGLE_COLUMN */
                    G_TYPE_DOUBLE,     /* ANGLE_VAR_COLUMN */
                    G_TYPE_DOUBLE,     /* SPEED_COLUMN */
                    G_TYPE_DOUBLE,     /* SPEED_VAR_COLUMN */
                    G_TYPE_DOUBLE,     /* PROGRESS_COLUMN */
                    G_TYPE_DOUBLE,     /* QUALITY_COLUMN */
                    G_TYPE_INT,        /* TYPE_COLUMN */
                    G_TYPE_INT,        /* WEIGHT_COLUMN */
                    G_TYPE_BOOLEAN };  /* INCONSISTENT_COLUMN */

  store = g_object_new (HYSCAN_TYPE_GTK_PLANNER_TREE_STORE, NULL);
  gtk_tree_store_set_column_types (GTK_TREE_STORE (store), G_N_ELEMENTS (types), types);
  store->list = list;

  return GTK_TREE_STORE (store);
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
        hyscan_object_model_remove (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i]);
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
          hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i],
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
          HyScanGeoPoint swap;

          track = g_hash_table_lookup (priv->objects, priv->tracks[i]);
          if (track == NULL)
            continue;

          modified_track = hyscan_planner_track_copy (track);
          swap = modified_track->plan.start;
          modified_track->plan.start = modified_track->plan.end;
          modified_track->plan.end = swap;
          hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), priv->tracks[i],
                                      (const HyScanObject *) modified_track);
          hyscan_planner_track_free (modified_track);
        }
    }

  else if (button == GTK_MENU_ITEM (priv->rec_menu.restore_plan))
    {
      GHashTable *tracks;
      GHashTableIter zn_iter, tk_iter;
      HyScanPlannerStatsZone *zone;
      HyScanPlannerStatsTrack *track;
      HyScanTrackStatsInfo *record;

      tracks = hyscan_db_info_get_tracks (priv->db_info);
      if (tracks == NULL)
        return;

      g_hash_table_iter_init (&zn_iter, priv->zone_stats);
      while (g_hash_table_iter_next (&zn_iter, NULL, (gpointer *) &zone))
        {
          g_hash_table_iter_init (&tk_iter, zone->tracks);
          while (g_hash_table_iter_next (&tk_iter, NULL, (gpointer *) &track))
            {
              record = g_hash_table_lookup (track->records, priv->records[0]);
              if (record != NULL && record->info->plan != NULL)
                {
                  HyScanPlannerTrack *track_object;

                  track_object = hyscan_planner_track_copy (track->object);
                  track_object->plan = *record->info->plan;
                  hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model),
                                              track->id, (const HyScanObject *) track_object);
                  hyscan_planner_track_free (track_object);

                  goto exit;
                }
            }
        }

    exit:
      g_hash_table_unref (tracks);
    }
}

/* Добавляет пункт контекстного меню. */
static inline void
hyscan_gtk_planner_list_add_menu (HyScanGtkPlannerList  *list,
                                  GtkMenu               *menu,
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
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  widget != NULL ? (*widget = item) : 0;
}

/* Отключает отдельные пункты меню в зависимости от выбранных объектов. */
static GtkMenu *
hyscan_gtk_planner_list_menu_prepare (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gboolean single_selected;

  /* Некоторые пункты меню актуальны при выборе только одного галса. */
  single_selected = (priv->tracks != NULL && g_strv_length (priv->tracks) == 1);
  gtk_widget_set_sensitive (priv->menu.nav, single_selected);

  if (priv->menu.find != NULL)
    gtk_widget_set_sensitive (priv->menu.find, single_selected);

  if (priv->menu.bind != NULL)
    {
      hyscan_gtk_planner_list_bind_menu_update (list, priv->tracks[0]);
      gtk_widget_set_sensitive (priv->menu.bind, single_selected);
    }

  return priv->menu.menu;
}

/* Отключает отдельные пункты меню в зависимости от выбранных объектов. */
static GtkMenu *
hyscan_gtk_planner_list_rec_menu_prepare (HyScanGtkPlannerList *list,
                                          GtkTreeIter          *iter)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gboolean single_selected;
  gboolean inconsistent;

  if (priv->db_info == NULL)
    return NULL;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter, INCONSISTENT_COLUMN, &inconsistent, -1);
  if (!inconsistent)
    return NULL;

  /* Некоторые пункты меню актуальны при выборе только одного галса. */
  single_selected = (priv->records != NULL && g_strv_length (priv->records) == 1);

  if (!single_selected)
    return NULL;

  return priv->rec_menu.menu;
}

/* Открывет контекстное меню по правой кнопке. */
static gboolean
hyscan_gtk_planner_list_button_press (HyScanGtkPlannerList *list,
                                      GdkEventButton       *event_button)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkMenu *menu = NULL;
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  gint type;
  gboolean selected;

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

  /* Если строка под курсором не выбрана, то снимаем выделение с остальных и выбираем только ее. */
  selected = gtk_tree_selection_path_is_selected (priv->tree_selection, path);
  if (!selected)
    {
      gtk_tree_selection_unselect_all (priv->tree_selection);
      gtk_tree_selection_select_path (priv->tree_selection, path);
    }

  if (type == TYPE_PLANNER_TRACK)
    menu = hyscan_gtk_planner_list_menu_prepare (list);
  else if (type == TYPE_DB_TRACK)
    menu = hyscan_gtk_planner_list_rec_menu_prepare (list, &iter);

  if (menu != NULL)
    gtk_menu_popup_at_pointer (menu, (GdkEvent *) event_button);

exit:
  g_clear_pointer (&path, gtk_tree_path_free);

  return menu != NULL ? GDK_EVENT_STOP : GDK_EVENT_PROPAGATE;
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
  hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), zone_id, (HyScanObject *) zone);
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

  track->plan.speed = speed;
  hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), zone_id, (HyScanObject *) track);
  hyscan_planner_track_free (track);

exit:
  g_value_unset (&value);
}

/* Устанавливает свойства рендерера иконки в зависимости от текущих данных. */
static void
hyscan_gtk_planner_list_icon_data (GtkTreeViewColumn *tree_column,
                                   GtkCellRenderer   *cell,
                                   GtkTreeModel      *tree_model,
                                   GtkTreeIter       *iter,
                                   gpointer           data)
{
  HyScanGtkPlannerList *list = HYSCAN_GTK_PLANNER_LIST (data);
  HyScanGtkPlannerListPrivate *priv = list->priv;
  gboolean inconsistent;

  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), iter, INCONSISTENT_COLUMN, &inconsistent, -1);
  g_object_set (cell, "visible", inconsistent, NULL);
}


/* Устанавливает свойства рендерера текста в зависимости от текущих данных. */
static void
hyscan_gtk_planner_list_text_data (GtkTreeViewColumn *tree_column,
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

  else if (tree_column == priv->velocity_col)
    {
      g_object_set (cell, "editable", type == TYPE_PLANNER_TRACK, NULL);
    }

  /* Текст ячейки. */
  if (tree_column == priv->title_col && type == TYPE_PLANNER_TRACK)
    {
      guint number;

      gtk_tree_model_get (tree_model, iter, NUMBER_COLUMN, &number, -1);
      text = number > 0 ? g_strdup_printf ("%s %d", _("Plan"), number) : g_strdup (_("Plan"));
    }

  else if (tree_column == priv->velocity_col)
    {
      gtk_tree_model_get (tree_model, iter, SPEED_COLUMN, &value, SPEED_VAR_COLUMN, &err_value, -1);
      text = g_strdup_printf ("%.2f", value);
    }

  else if (tree_column == priv->quality_col)
    {
      gtk_tree_model_get (tree_model, iter, QUALITY_COLUMN, &value, -1);
      text = value >= 0 ? g_strdup_printf ("%.0f", 100 * value) : g_strdup ("");
    }

  else if (tree_column == priv->velocity_sd_col)
    {
      gtk_tree_model_get (tree_model, iter, SPEED_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.2f", err_value);
      else
        text = NULL;
    }

  else if (tree_column == priv->track_col)
    {
      gtk_tree_model_get (tree_model, iter, ANGLE_COLUMN, &value, ANGLE_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK || type == TYPE_PLANNER_TRACK)
        text = g_strdup_printf ("%.1f", value);
      else
        text = NULL;
    }

  else if (tree_column == priv->track_sd_col)
    {
      gtk_tree_model_get (tree_model, iter, ANGLE_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.2f", err_value);
      else
        text = NULL;
    }

  else if (tree_column == priv->length_col)
    {
      gtk_tree_model_get (tree_model, iter, LENGTH_COLUMN, &value, Y_VAR_COLUMN, &err_value, -1);
      text = g_strdup_printf ("%.0f", value);
    }

  else if (tree_column == priv->y_sd_col)
    {
      gtk_tree_model_get (tree_model, iter, Y_VAR_COLUMN, &err_value, -1);
      if (type == TYPE_DB_TRACK)
        text = g_strdup_printf ("%.2f", err_value);
      else
        text = NULL;
    }

  else if (tree_column == priv->time_col)
    {
      gint hour, min, sec;

      gtk_tree_model_get (tree_model, iter, TIME_COLUMN, &value, -1);

      hour = value / 3600;
      value -= hour * 3600;
      min = value / 60;
      value -= min * 60;

      sec = value;

      if (hour > 0)
        text = g_strdup_printf ("%02d:%02d:%02d", hour, min, sec);
      else
        text = g_strdup_printf ("%02d:%02d", min, sec);
    }

  else if (tree_column == priv->progress_col)
    {
      gtk_tree_model_get (tree_model, iter, PROGRESS_COLUMN, &value, -1);
      text = g_strdup_printf ("%.0f%%", value * 100.0);
    }

  else
    {
      return;
    }

  g_object_set (cell, "markup", text, NULL);
  g_free (text);
}

/* Восстанавливает выбор галсов в дереве. */
static void
hyscan_gtk_planner_list_restore_selection (HyScanGtkPlannerList  *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkTreeIter zone_iter, track_iter;
  gboolean zone_valid, track_valid;

  g_signal_handler_block (priv->tree_selection, priv->hndl_tree_chgd);
  if (priv->tracks == NULL)
    {
      gtk_tree_selection_unselect_all (priv->tree_selection);
      goto exit;
    }

  zone_valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &zone_iter);
  while (zone_valid)
    {
      GValue value = G_VALUE_INIT;
      const gchar *zone_id;

      gtk_tree_model_get_value (GTK_TREE_MODEL (priv->store), &zone_iter, ID_COLUMN, &value);
      zone_id = g_value_get_string (&value);
      if (priv->zone_id != NULL && g_strcmp0 (priv->zone_id, zone_id) == 0)
        gtk_tree_selection_select_iter (priv->tree_selection, &zone_iter);
      else
        gtk_tree_selection_unselect_iter (priv->tree_selection, &zone_iter);
      g_value_unset (&value);

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
  g_signal_handler_unblock (priv->tree_selection, priv->hndl_tree_chgd);
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

/* Обработчик сигнала "zone-changed" модели выбранных объектов планировщика, т.е.
 * если пользователь изменил выбранную зону извне виджета. */
static void
hyscan_gtk_planner_list_zone_changed (HyScanGtkPlannerList  *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  g_free (priv->zone_id);
  priv->zone_id = hyscan_planner_selection_get_zone (priv->selection, NULL);

  hyscan_gtk_planner_list_restore_selection (list);
}

/* Удаляет из списка пропавшие элементы, а из оставшихся формирует хэш таблицу. */
static GHashTable *
hyscan_gtk_planner_list_map_existing (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GtkTreeIter tr_iter_zn, tr_iter_tk, tr_iter_rc;
  gboolean valid, tk_valid, rc_valid;
  gchar *zone_key, *track_key, *record_key;
  GHashTable *tree_iter_ht;

  tree_iter_ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) gtk_tree_iter_free);

  /* Обходим плановые зоны. */
  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &tr_iter_zn);
  while (valid)
    {
      HyScanPlannerStatsZone *zone;

      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &tr_iter_zn, KEY_COLUMN, &zone_key, -1);
      zone = g_hash_table_lookup (priv->zone_stats, zone_key);
      if (zone != NULL)
        g_hash_table_insert (tree_iter_ht, zone_key, gtk_tree_iter_copy (&tr_iter_zn));
      else
        g_free (zone_key);

      /* Обходим плановые галсы этой зоны. */
      tk_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &tr_iter_tk, &tr_iter_zn);
      while (tk_valid)
        {
          HyScanPlannerStatsTrack *track = NULL;

          if (zone != NULL)
            {
              gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &tr_iter_tk, KEY_COLUMN, &track_key, -1);
              track = g_hash_table_lookup (zone->tracks, track_key);
              if (track != NULL)
                g_hash_table_insert (tree_iter_ht, track_key, gtk_tree_iter_copy (&tr_iter_tk));
              else
                g_free (track_key);
            }

          /* Обходим записи галса. */
          rc_valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (priv->store), &tr_iter_rc, &tr_iter_tk);
          while (rc_valid)
            {
              HyScanTrackStatsInfo *record = NULL;

              if (track != NULL)
                {
                  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &tr_iter_rc, KEY_COLUMN, &record_key, -1);
                  record = g_hash_table_lookup (track->records, record_key);
                  if (record != NULL)
                    g_hash_table_insert (tree_iter_ht, record_key, gtk_tree_iter_copy (&tr_iter_rc));
                  else
                    g_free (record_key);
                }

              rc_valid = record != NULL ? gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &tr_iter_rc) :
                                          gtk_tree_store_remove (priv->store, &tr_iter_rc);
            }

          tk_valid = track != NULL ? gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &tr_iter_tk) :
                                     gtk_tree_store_remove (priv->store, &tr_iter_tk);
        }

      valid = zone != NULL ? gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &tr_iter_zn) :
                             gtk_tree_store_remove (priv->store, &tr_iter_zn);
    }

  return tree_iter_ht;
}

/* Обновляет информацию о зонах в виджете. */
static void
hyscan_gtk_planner_list_update_zones (HyScanGtkPlannerList *list,
                                      GHashTable           *tree_iter_ht)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GtkTreeIter *iter;

  GHashTableIter zone_iter, track_iter, record_iter;
  gchar *zone_key, *track_key, *record_key;
  HyScanPlannerStatsZone *zone;
  HyScanPlannerStatsTrack *track;
  HyScanTrackStatsInfo *record;

  /* Добавляем в список все зоны, которые есть в модели данных планировщика. */
  g_hash_table_iter_init (&zone_iter, priv->zone_stats);
  while (g_hash_table_iter_next (&zone_iter, (gpointer *) &zone_key, (gpointer *) &zone))
    {
      GtkTreeIter zone_t_iter;

      iter = g_hash_table_lookup (tree_iter_ht, zone_key);
      if (iter != NULL)
        zone_t_iter = *iter;
      else
        gtk_tree_store_append (priv->store, &zone_t_iter, NULL);

      gtk_tree_store_set (priv->store, &zone_t_iter,
                          TYPE_COLUMN, TYPE_PLANNER_ZONE,
                          NAME_COLUMN, zone->object != NULL ? zone->object->name : _("Free tracks"),
                          KEY_COLUMN, zone_key,
                          ID_COLUMN, zone->id,
                          SPEED_COLUMN, zone->velocity,
                          LENGTH_COLUMN, zone->length,
                          TIME_COLUMN, zone->time,
                          PROGRESS_COLUMN, zone->progress,
                          QUALITY_COLUMN, zone->quality,
                          WEIGHT_COLUMN, 400,
                          -1);

      g_hash_table_iter_init (&track_iter, zone->tracks);
      while (g_hash_table_iter_next (&track_iter, (gpointer *) &track_key, (gpointer *) &track))
        {
          GtkTreeIter track_t_iter;

          iter = g_hash_table_lookup (tree_iter_ht, track_key);
          if (iter != NULL)
            track_t_iter = *iter;
          else
            gtk_tree_store_append (priv->store, &track_t_iter, &zone_t_iter);

          gtk_tree_store_set (priv->store, &track_t_iter,
                              TYPE_COLUMN, TYPE_PLANNER_TRACK,
                              KEY_COLUMN, track_key,
                              ID_COLUMN, track->id,
                              SPEED_COLUMN, track->object->plan.speed,
                              ANGLE_COLUMN, track->angle,
                              NUMBER_COLUMN, track->object->number,
                              LENGTH_COLUMN, track->length,
                              TIME_COLUMN, track->time,
                              PROGRESS_COLUMN, track->progress,
                              QUALITY_COLUMN, track->quality,
                              WEIGHT_COLUMN, g_strcmp0 (track->id, priv->active) == 0 ? 800 : 400,
                              -1);

          g_hash_table_iter_init (&record_iter, track->records);
          while (g_hash_table_iter_next (&record_iter, (gpointer *) &record_key, (gpointer *) &record))
            {
              GtkTreeIter rec_t_iter;
              gboolean inconsistent;

              iter = g_hash_table_lookup (tree_iter_ht, record_key);
              if (iter != NULL)
                rec_t_iter = *iter;
              else
                gtk_tree_store_append (priv->store, &rec_t_iter, &track_t_iter);

              /* Определяем, совпадает ли план, использованный при записи, с текущим планом. */
              inconsistent = record->info->plan != NULL &&
                             !hyscan_planner_plan_equal (record->info->plan, &track->object->plan);

              gtk_tree_store_set (priv->store, &rec_t_iter,
                                  TYPE_COLUMN, TYPE_DB_TRACK,
                                  KEY_COLUMN, record_key,
                                  ID_COLUMN, record->info->id,
                                  NAME_COLUMN, record->info->name,
                                  SPEED_COLUMN, record->speed,
                                  SPEED_VAR_COLUMN, record->speed_var,
                                  ANGLE_COLUMN, record->angle,
                                  ANGLE_VAR_COLUMN, record->angle_var,
                                  LENGTH_COLUMN, record->x_length,
                                  Y_VAR_COLUMN, record->y_var,
                                  PROGRESS_COLUMN, record->progress,
                                  QUALITY_COLUMN, record->quality,
                                  INCONSISTENT_COLUMN, inconsistent,
                                  TIME_COLUMN, 1e-6 * (gdouble) (record->end_time - record->start_time),
                                  WEIGHT_COLUMN, 400,
                                  -1);
            }
        }
    }
}

/* Обработчик сигнала "changed" модели HyScanPlannerStats. */
static void
hyscan_gtk_planner_list_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  /* Устанавливаем новый список объектов в модель данных. */
  g_clear_pointer (&priv->objects, g_hash_table_unref);
  priv->objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));
}

/* Обработчик сигнала "changed" модели HyScanPlannerStats. */
static void
hyscan_gtk_planner_list_stats_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  GHashTable *tree_iter_ht;

  g_signal_handler_block (priv->tree_selection, priv->hndl_tree_chgd);

  /* Устанавливаем новый список объектов в модель данных. */
  g_clear_pointer (&priv->zone_stats, g_hash_table_unref);
  priv->zone_stats = hyscan_planner_stats_get (priv->stats);

  /* Хэш таблица элементов списка. */
  tree_iter_ht = hyscan_gtk_planner_list_map_existing (list);

  /* Хэш таблица с информацией о зонах. */
  hyscan_gtk_planner_list_update_zones (list, tree_iter_ht);

  hyscan_gtk_planner_list_restore_selection (list);

  g_hash_table_destroy (tree_iter_ht);

  g_signal_handler_unblock (priv->tree_selection, priv->hndl_tree_chgd);
}

static void
hyscan_gtk_planner_list_bind_activate (GtkCheckMenuItem     *button,
                                       GParamSpec           *spec,
                                       HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;

  HyScanPlannerTrack *plan_track;
  const gchar *plan_track_id, *record_id;
  gboolean active;

  if (priv->tracks == NULL || g_strv_length (priv->tracks) != 1)
    return;

  plan_track_id = priv->tracks[0];
  active = gtk_check_menu_item_get_active (button);

  record_id = g_object_get_data (G_OBJECT (button), "track-id");
  if (record_id == NULL)
    return;

  plan_track = g_hash_table_lookup (priv->objects, plan_track_id);
  if (!HYSCAN_IS_PLANNER_TRACK (plan_track))
    return;

  if (active)
    hyscan_planner_track_record_append (plan_track, record_id);
  else
    hyscan_planner_track_record_delete (plan_track, record_id);

  hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), plan_track_id, (const HyScanObject *) plan_track);
}

static gint
hyscan_gtk_planner_list_track_info_cmp (HyScanTrackInfo *a,
                                        HyScanTrackInfo *b)
{
  gint64 a_time, b_time;

  a_time = a->ctime == NULL ? 0 : g_date_time_to_unix (a->ctime);
  b_time = b->ctime == NULL ? 0 : g_date_time_to_unix (b->ctime);

  return a_time - b_time;
}

static gboolean
hyscan_gtk_planner_list_record_bound (HyScanGtkPlannerList *list,
                                      const gchar          *record_id)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GHashTableIter iter;
  HyScanPlannerTrack *track;
  gchar *id;

  if (priv->objects == NULL)
    return FALSE;

  g_hash_table_iter_init (&iter, priv->objects);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &track))
    {
      if (!HYSCAN_IS_PLANNER_TRACK (track))
        continue;

      if (track->records != NULL && g_strv_contains ((const gchar *const *) track->records, record_id))
        return TRUE;
    }

  return FALSE;
}

/* Обновляет контекстное меню привязанных записей галсов. */
static void
hyscan_gtk_planner_list_bind_menu_update (HyScanGtkPlannerList *list,
                                          const gchar          *planner_track_id)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GHashTable *tracks;
  GtkWidget *submenu = priv->menu.bind_submenu;
  GList *link;
  GList *children;
  GList *track_list;
  HyScanPlannerTrack *planner_track;

  /* Удаляем старые пункты. */
  children = gtk_container_get_children (GTK_CONTAINER (submenu));
  for (link = children; link != NULL; link = link->next)
    gtk_container_remove (GTK_CONTAINER (submenu), link->data);
  g_list_free (children);

  /* Определяем выбранный плановый галс. */
  planner_track = g_hash_table_lookup (priv->objects, planner_track_id);
  if (!HYSCAN_IS_PLANNER_TRACK (planner_track))
    return;

  /* Добавляем новые пункты. */
  tracks = hyscan_db_info_get_tracks (priv->db_info);
  if (tracks == NULL)
    return;

  track_list = g_hash_table_get_values (tracks);
  track_list = g_list_sort (track_list, (GCompareFunc) hyscan_gtk_planner_list_track_info_cmp);

  for (link = track_list; link != NULL; link = link->next)
    {
      HyScanTrackInfo *info = link->data;
      GtkWidget *item;
      gboolean active;

      active = planner_track->records != NULL &&
               g_strv_contains ((const gchar *const *) planner_track->records, info->id);

      if (!active && hyscan_gtk_planner_list_record_bound (list, info->id))
        continue;

      item = gtk_check_menu_item_new_with_label (info->name);
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), active);
      g_object_set_data_full (G_OBJECT (item), "track-id", g_strdup (info->id), g_free);

      gtk_widget_show_all (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
      g_signal_connect (item, "notify::active", G_CALLBACK (hyscan_gtk_planner_list_bind_activate), list);
    }

  g_list_free (track_list);
  g_hash_table_destroy (tracks);
}

/* Обработчик сигнала "changed" выбора в виджете, т.е. если пользователь
 * изменил выбор внутри виджета. */
static void
hyscan_gtk_planner_list_selection_changed (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv = list->priv;
  GList *selected_list, *link;
  gint n_tracks = 0, n_recs = 0;

  g_signal_handler_block (priv->selection, priv->hndl_tk_chgd);
  g_signal_handler_block (priv->selection, priv->hndl_zn_chgd);

  /* Формируем список идентификаторов выбранных галсов. */
  selected_list = gtk_tree_selection_get_selected_rows (priv->tree_selection, NULL);
  g_clear_pointer (&priv->zone_id, g_free);
  g_clear_pointer (&priv->tracks, g_strfreev);
  g_clear_pointer (&priv->records, g_strfreev);
  priv->tracks = g_new (gchar *, g_list_length (selected_list) + 1);
  priv->records = g_new (gchar *, g_list_length (selected_list) + 1);
  for (link = selected_list; link != NULL; link = link->next)
    {
      GtkTreePath *path = link->data;
      GtkTreeIter iter;
      gint type;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->store), &iter, path);
      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, TYPE_COLUMN, &type, -1);
      if (type == TYPE_PLANNER_TRACK)
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &priv->tracks[n_tracks++], -1);
        }
      else if (type == TYPE_DB_TRACK)
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &priv->records[n_recs++], -1);
        }
      else if (type == TYPE_PLANNER_ZONE)
        {
          g_clear_pointer (&priv->zone_id, g_free);
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter, ID_COLUMN, &priv->zone_id, -1);
        }
    }
  priv->tracks[n_tracks] = NULL;
  priv->records[n_recs] = NULL;

  hyscan_planner_selection_set_tracks (priv->selection, priv->tracks);
  hyscan_planner_selection_set_zone (priv->selection, priv->zone_id, 0);

  g_list_free_full (selected_list, (GDestroyNotify) gtk_tree_path_free);

  g_signal_handler_unblock (priv->selection, priv->hndl_tk_chgd);
  g_signal_handler_unblock (priv->selection, priv->hndl_zn_chgd);
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
  gint sort_column_id;
  GtkSortType order;

  if (!gtk_tree_get_row_drag_data (selection_data, &src_model, &src_path))
    goto exit;

  /* Может перетаскивать только внутри нашей модели */
  if (src_model != GTK_TREE_MODEL (drag_dest))
    goto exit;

  /* */

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
      if (!HYSCAN_IS_PLANNER_TRACK (track))
        goto next;

      if (track->number == number)
        goto next;

      modified_track = hyscan_planner_track_copy (track);
      modified_track->number = number;
      hyscan_object_model_modify (HYSCAN_OBJECT_MODEL (priv->model), track_id, (HyScanObject *) modified_track);
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
 * @stats: модель объектов планировщика #HyScanPlannerModel
 * @selection: модель выбранных объектов планировщика
 * @stats: (nullable): модель статистики по галсам #HyScanTrackStats
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
                             HyScanPlannerStats     *stats,
                             HyScanGtkMapPlanner    *viewer)
{
  return g_object_new (HYSCAN_TYPE_GTK_PLANNER_LIST,
                       "model", model,
                       "stats", stats,
                       "selection", selection,
                       "viewer", viewer,
                       NULL);
}

/**
 * hyscan_gtk_planner_list_set_visible_cols:
 * @list: указатель на #HyScanGtkPlannerList
 * @cols: битовая маска видимых столбцов
 *
 * Устанавливает битовую маску видимых столбцов #HyScanGtkPlannerListCol.
 */
void
hyscan_gtk_planner_list_set_visible_cols (HyScanGtkPlannerList *list,
                                          gint                 cols)
{
  HyScanGtkPlannerListPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_LIST (list));
  priv = list->priv;

  gtk_tree_view_column_set_visible (priv->progress_col, cols & HYSCAN_GTK_PLANNER_LIST_PROGRESS);
  gtk_tree_view_column_set_visible (priv->quality_col, cols & HYSCAN_GTK_PLANNER_LIST_QUALITY);
  gtk_tree_view_column_set_visible (priv->length_col, cols & HYSCAN_GTK_PLANNER_LIST_LENGTH);
  gtk_tree_view_column_set_visible (priv->time_col, cols & HYSCAN_GTK_PLANNER_LIST_TIME);
  gtk_tree_view_column_set_visible (priv->velocity_col, cols & HYSCAN_GTK_PLANNER_LIST_VELOCITY);
  gtk_tree_view_column_set_visible (priv->track_col, cols & HYSCAN_GTK_PLANNER_LIST_TRACK);
  gtk_tree_view_column_set_visible (priv->track_sd_col, cols & HYSCAN_GTK_PLANNER_LIST_TRACK_SD);
  gtk_tree_view_column_set_visible (priv->velocity_sd_col, cols & HYSCAN_GTK_PLANNER_LIST_VELOCITY_SD);
  gtk_tree_view_column_set_visible (priv->y_sd_col, cols & HYSCAN_GTK_PLANNER_LIST_Y_SD);
}

/**
 * hyscan_gtk_planner_list_get_visible_cols:
 * @list: указатель на #HyScanGtkPlannerList
 *
 * Получает битовую маску видимых столбцов #HyScanGtkPlannerListCol или
 * %HYSCAN_GTK_PLANNER_LIST_INVALID в случае ошибки.
 *
 * Returns: битовая маска видимых столбцов
 */
gint
hyscan_gtk_planner_list_get_visible_cols (HyScanGtkPlannerList *list)
{
  HyScanGtkPlannerListPrivate *priv;
  gint cols = 0;

  g_return_val_if_fail (HYSCAN_IS_GTK_PLANNER_LIST (list), HYSCAN_GTK_PLANNER_LIST_INVALID);
  priv = list->priv;

  cols |= gtk_tree_view_column_get_visible (priv->progress_col) ? HYSCAN_GTK_PLANNER_LIST_PROGRESS : 0;
  cols |= gtk_tree_view_column_get_visible (priv->quality_col) ? HYSCAN_GTK_PLANNER_LIST_QUALITY : 0;
  cols |= gtk_tree_view_column_get_visible (priv->length_col) ? HYSCAN_GTK_PLANNER_LIST_LENGTH : 0;
  cols |= gtk_tree_view_column_get_visible (priv->time_col) ? HYSCAN_GTK_PLANNER_LIST_TIME : 0;
  cols |= gtk_tree_view_column_get_visible (priv->velocity_col) ? HYSCAN_GTK_PLANNER_LIST_VELOCITY : 0;
  cols |= gtk_tree_view_column_get_visible (priv->track_col) ? HYSCAN_GTK_PLANNER_LIST_TRACK : 0;
  cols |= gtk_tree_view_column_get_visible (priv->track_sd_col) ? HYSCAN_GTK_PLANNER_LIST_TRACK_SD : 0;
  cols |= gtk_tree_view_column_get_visible (priv->velocity_sd_col) ? HYSCAN_GTK_PLANNER_LIST_VELOCITY_SD : 0;
  cols |= gtk_tree_view_column_get_visible (priv->y_sd_col) ? HYSCAN_GTK_PLANNER_LIST_Y_SD : 0;

  return cols;
}

/**
 * hyscan_gtk_planner_list_menu_append:
 * @list: указатель на #HyScanGtkPlannerList
 * @item: виджет пункта меню
 *
 * Добавляет в контекстное меню выбранных галсов пользовательский пункт меню @item.
 */
void
hyscan_gtk_planner_list_menu_append (HyScanGtkPlannerList *list,
                                     GtkWidget            *item)
{
  HyScanGtkPlannerListPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_LIST (list));

  priv = list->priv;

  gtk_widget_show (item);
  gtk_menu_shell_insert (GTK_MENU_SHELL (list->priv->menu.menu), item, priv->menu.user_position);
  priv->menu.user_position++;
}

/**
 * hyscan_gtk_planner_list_enable_binding:
 * @list: указатель на #HyScanGtkPlannerList
 * @db_info: указатель на объект HyScanDbInfo этой БД
 *
 * Включает возможность связи плановых галсов с их записями в БД.
 * В контекстном меню выбранного галса будет показано подменю с возможностью
 * прикрепить или открепить запись галса.
 */
void
hyscan_gtk_planner_list_enable_binding (HyScanGtkPlannerList *list,
                                        HyScanDBInfo         *db_info)
{
  HyScanGtkPlannerListPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_PLANNER_LIST (list));
  priv = list->priv;

  g_return_if_fail (priv->menu.bind_submenu == NULL);

  priv->db_info = g_object_ref (db_info);

  priv->menu.bind = gtk_menu_item_new_with_mnemonic(_("_Records"));
  priv->menu.bind_submenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (priv->menu.bind), priv->menu.bind_submenu);

  hyscan_gtk_planner_list_menu_append (list, priv->menu.bind);
}

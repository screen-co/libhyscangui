/* hyscan-gtk-model-manager.h
 *
 * Copyright 2020 Screen LLC, Andrey Zakharov <zaharov@screen-co.ru>
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

#ifndef __HYSCAN_GTK_MODEL_MANAGER_H__
#define __HYSCAN_GTK_MODEL_MANAGER_H__

#include <gtk/gtk.h>
#include <hyscan-db-info.h>
#include <hyscan-object-model.h>
#include <hyscan-mark-loc-model.h>
#include <hyscan-object-data-label.h>
#include <hyscan-units.h>
#include <hyscan-planner-model.h>
#include <hyscan-gtk-mark-manager-icon.h>

/* Максимальное количество пользовательских групп. */
#define MAX_CUSTOM_LABELS 32
/* Макросы для cравнения двух строк. */
#define IS_EQUAL(a, b)     (0 == g_strcmp0 ( (a), (b))) /* Одинаковые строки. */
#define IS_NOT_EQUAL(a, b) (0 != g_strcmp0 ( (a), (b))) /* Разные строки. */
/* Макросы для проверки строки. */
#define IS_EMPTY(str)      ( (str) == NULL || IS_EQUAL ( (str), ""))     /* Пустая строка. */
#define IS_NOT_EMPTY(str)  ( (str) != NULL && IS_NOT_EQUAL ( (str), "")) /* Не пустая строка. */
/* Макрос возвращает название сигнала по идентификатору. */
#define GET_SIGNAL_TITLE(signal_id)\
hyscan_gtk_model_manager_get_signal_title (priv->model_manager, (signal_id))

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MODEL_MANAGER            (hyscan_gtk_model_manager_get_type ())
#define HYSCAN_GTK_MODEL_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MODEL_MANAGER, HyScanGtkModelManager))
#define HYSCAN_IS_GTK_MODEL_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MODEL_MANAGER))
#define HYSCAN_GTK_MODEL_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MODEL_MANAGER, HyScanGtkModelManagerClass))
#define HYSCAN_IS_GTK_MODEL_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MODEL_MANAGER))
#define HYSCAN_GTK_MODEL_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MODEL_MANAGER, HyScanGtkModelManagerClass))

typedef struct _HyScanGtkModelManager        HyScanGtkModelManager;
typedef struct _HyScanGtkModelManagerPrivate HyScanGtkModelManagerPrivate;
typedef struct _HyScanGtkModelManagerClass   HyScanGtkModelManagerClass;

/* Сигналы.
 * Должны идти в порядке соответвующем signals[].
 * */
typedef enum
{
  HYSCAN_MODEL_MANAGER_SIGNAL_ACOUSTIC_MARKS_CHANGED,      /* Изменение данных в модели "водопадных" меток. */
  HYSCAN_MODEL_MANAGER_SIGNAL_GEO_MARKS_CHANGED,           /* Изменение данных в модели гео-меток. */
  HYSCAN_MODEL_MANAGER_SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED,  /* Изменение данных в модели "водопадных" меток с координатами. */
  HYSCAN_MODEL_MANAGER_SIGNAL_LABELS_CHANGED,              /* Изменение данных в модели групп. */
  HYSCAN_MODEL_MANAGER_SIGNAL_TRACKS_CHANGED,              /* Изменение данных в модели галсов. */
  HYSCAN_MODEL_MANAGER_SIGNAL_GROUPING_CHANGED,            /* Изменение типа группировки. */
  HYSCAN_MODEL_MANAGER_SIGNAL_VIEW_MODEL_UPDATED,          /* Обновление модели представления данных. */
  HYSCAN_MODEL_MANAGER_SIGNAL_ITEM_SELECTED,               /* Выделена строка. */
  HYSCAN_MODEL_MANAGER_SIGNAL_ITEM_TOGGLED,                /* Изменено состояние чек-бокса. */
  HYSCAN_MODEL_MANAGER_SIGNAL_ITEM_EXPANDED,               /* Разворачивание узла древовидного представления. */
  HYSCAN_MODEL_MANAGER_SIGNAL_ITEM_COLLAPSED,              /* Сворачивание узла древовидного представления. */
  HYSCAN_MODEL_MANAGER_SIGNAL_VIEW_SCROLLED_HORIZONTAL,    /* Изменение положения горизонтальной прокрутки представления. */
  HYSCAN_MODEL_MANAGER_SIGNAL_VIEW_SCROLLED_VERTICAL,      /* Изменение положения вертикальной прокрутки представления. */
  HYSCAN_MODEL_MANAGER_SIGNAL_UNSELECT_ALL,                /* Снятие выделения. */
  HYSCAN_MODEL_MANAGER_SIGNAL_SHOW_OBJECT,                 /* Показать объект на карте. */
  HYSCAN_MODEL_MANAGER_SIGNAL_MODEL_MANAGER_LAST           /* Количество сигналов. */
}HyScanModelManagerSignal;

/* Тип представления. */
typedef enum
{
  HYSCAN_MODEL_MANAGER_GROUPING_UNGROUPED,      /* Табличное. */
  HYSCAN_MODEL_MANAGER_GROUPING_BY_TYPES,       /* Древовидный с группировкой по типам. */
  HYSCAN_MODEL_MANAGER_GROUPING_BY_LABELS,      /* Древовидный с группировкой по группам. */
  HYSCAN_MODEL_MANAGER_GROUPING_TYPES           /* Количество типов представления. */
}HyScanModelManagerGroupingType;

/* Типы объектов. */
typedef enum
{
  HYSCAN_MODEL_MANAGER_OBJECT_LABEL,            /* Группа. */
  HYSCAN_MODEL_MANAGER_OBJECT_GEO_MARK,         /* Гео-метка. */
  HYSCAN_MODEL_MANAGER_OBJECT_ACOUSTIC_MARK,    /* Акустическая метка. */
  HYSCAN_MODEL_MANAGER_OBJECT_TRACK,            /* Галс. */
  HYSCAN_MODEL_MANAGER_OBJECT_TYPES             /* Количество объектов. */
}HyScanModelManagerObjectType;

/* Столбцы для табличного представления. */
enum
{
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_ID,          /* Идентификатор объекта в базе данных. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_NAME,        /* Название объекта. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_DESCRIPTION, /* Описание объекта. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_OPERATOR,    /* Оператор, создавший объект. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_TYPE,        /* Тип объекта: группа, гео-метка, "водопадная" метка или галс. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_ICON,        /* Иконка. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_ACTIVE,      /* Статус чек-бокса. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_VISIBLE,     /* Видимость чек-бокса. У атрибутов чек-бокс скрыт.*/
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_LABEL,       /* Метки групп к которым принадлежит объект. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_CTIME,       /* Время создания объекта. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_MTIME,       /* Врем модификации объекта. */
  /* Атрибуты для гео-меток и акустических меток. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_LOCATION,    /* Координаты.     |E| */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_TRACK_NAME,  /* Название галса. |C| */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_BOARD,       /* Борт.           |H| */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_DEPTH,       /* Глубина.        |O| */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_WIDTH,       /* Ширина. */
  HYSCAN_MODEL_MANAGER_VIEW_COLUMN_SLANT_RANGE, /* Наклонная дальность. */
  HYSCAN_MODEL_MANAGER_VIEW_MAX_COLUMNS         /* Общее количество колонок для представления данных. */
};

struct _HyScanGtkModelManager
{
  GObject parent_instance;

  HyScanGtkModelManagerPrivate *priv;
};

struct _HyScanGtkModelManagerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_model_manager_get_type                    (void);

HYSCAN_API
HyScanGtkModelManager* hyscan_gtk_model_manager_new                         (const gchar                    *project_name,
                                                                             HyScanDB                       *db,
                                                                             HyScanCache                    *cache,
                                                                             gchar                          *export_folder);

HYSCAN_API
HyScanUnits *          hyscan_gtk_model_manager_get_units                   (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanDBInfo*          hyscan_gtk_model_manager_get_track_model             (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanObjectModel*     hyscan_gtk_model_manager_get_acoustic_mark_model     (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanObjectModel*     hyscan_gtk_model_manager_get_geo_mark_model          (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanPlannerModel*    hyscan_gtk_model_manager_get_planner_model           (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanObjectModel*     hyscan_gtk_model_manager_get_label_model             (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanMarkLocModel*    hyscan_gtk_model_manager_get_acoustic_mark_loc_model (HyScanGtkModelManager          *self);

HYSCAN_API
GtkTreeModel*          hyscan_gtk_model_manager_get_view_model              (HyScanGtkModelManager          *self);

HYSCAN_API
const gchar*           hyscan_gtk_model_manager_get_signal_title            (HyScanGtkModelManager          *self,
                                                                             HyScanModelManagerSignal        signal_title);

HYSCAN_API
void                   hyscan_gtk_model_manager_set_project_name            (HyScanGtkModelManager          *self,
                                                                             const gchar                    *project_name);

HYSCAN_API
const gchar*           hyscan_gtk_model_manager_get_project_name            (HyScanGtkModelManager          *self);

HYSCAN_API
const gchar*           hyscan_gtk_model_manager_get_export_folder           (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanDB*              hyscan_gtk_model_manager_get_db                      (HyScanGtkModelManager          *self);

HYSCAN_API
HyScanCache*           hyscan_gtk_model_manager_get_cache                   (HyScanGtkModelManager          *self);

HYSCAN_API
gchar**                hyscan_gtk_model_manager_get_all_tracks_id           (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_set_grouping                (HyScanGtkModelManager          *self,
                                                                             HyScanModelManagerGroupingType  grouping);

HYSCAN_API
HyScanModelManagerGroupingType   hyscan_gtk_model_manager_get_grouping      (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_set_selected_item           (HyScanGtkModelManager          *self,
                                                                             gchar                          *id);

HYSCAN_API
gchar*                 hyscan_gtk_model_manager_get_selected_item           (HyScanGtkModelManager          *self);

HYSCAN_API
gchar*                 hyscan_gtk_model_manager_get_selected_track          (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_unselect_all                (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_set_horizontal_adjustment   (HyScanGtkModelManager          *self,
                                                                             gdouble                         value);

HYSCAN_API
void                   hyscan_gtk_model_manager_set_vertical_adjustment     (HyScanGtkModelManager          *self,
                                                                             gdouble                         value);

HYSCAN_API
gdouble                hyscan_gtk_model_manager_get_horizontal_adjustment   (HyScanGtkModelManager          *self);

HYSCAN_API
gdouble                hyscan_gtk_model_manager_get_vertical_adjustment     (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_toggle_item                 (HyScanGtkModelManager          *self,
                                                                             gchar                          *id,
                                                                             gboolean                        active);

HYSCAN_API
gchar**                hyscan_gtk_model_manager_get_toggled_items           (HyScanGtkModelManager          *self,
                                                                             HyScanModelManagerObjectType    type);

HYSCAN_API
gboolean               hyscan_gtk_model_manager_has_toggled_items           (HyScanGtkModelManager          *self,
                                                                             HyScanModelManagerObjectType    type);

HYSCAN_API
void                   hyscan_gtk_model_manager_expand_item                 (HyScanGtkModelManager          *self,
                                                                             gchar                          *id,
                                                                             gboolean                        expanded);

HYSCAN_API
gchar**                hyscan_gtk_model_manager_get_expanded_items          (HyScanGtkModelManager          *self,
                                                                             HyScanModelManagerObjectType    type,
                                                                             gboolean                        expanded);

HYSCAN_API
gchar*                 hyscan_gtk_model_manager_get_current_id              (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_delete_toggled_items        (HyScanGtkModelManager          *self,
                                                                             gboolean                        flag);

HYSCAN_API
gboolean               hyscan_gtk_model_manager_has_toggled                 (HyScanGtkModelManager          *self);

HYSCAN_API
void                   hyscan_gtk_model_manager_toggled_items_set_labels    (HyScanGtkModelManager          *self,
                                                                             guint64                         labels,
                                                                             guint64                         inconsistents);

HYSCAN_API
void                   hyscan_gtk_model_manager_toggled_items_get_bit_masks (HyScanGtkModelManager          *self,
                                                                             guint64                        *labels,
                                                                             guint64                        *inconsistents);

HYSCAN_API
void                   hyscan_gtk_model_manager_show_object                 (HyScanGtkModelManager          *self,
                                                                             gchar                          *id,
                                                                             guint                           type);

HYSCAN_API
GHashTable*            hyscan_gtk_model_manager_get_all_labels              (HyScanGtkModelManager          *self);

G_END_DECLS

#endif /* __HYSCAN_GTK_MODEL_MANAGER_H__ */

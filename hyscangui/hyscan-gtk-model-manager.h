/*
 * hyscan-gtk-model-manager.h
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#ifndef __HYSCAN_MODEL_MANAGER_H__
#define __HYSCAN_MODEL_MANAGER_H__

#include <gtk/gtk.h>
#include <hyscan-db-info.h>
#include <hyscan-object-model.h>
#include <hyscan-mark-loc-model.h>
#include <hyscan-object-data-label.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MODEL_MANAGER             (hyscan_model_manager_get_type ())
#define HYSCAN_MODEL_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManager))
#define HYSCAN_IS_MODEL_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MODEL_MANAGER))
#define HYSCAN_MODEL_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManagerClass))
#define HYSCAN_IS_MODEL_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MODEL_MANAGER))
#define HYSCAN_MODEL_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MODEL_MANAGER, HyScanModelManagerClass))

typedef struct _HyScanModelManager HyScanModelManager;
typedef struct _HyScanModelManagerPrivate HyScanModelManagerPrivate;
typedef struct _HyScanModelManagerClass HyScanModelManagerClass;

/* Сигналы.
 * Должны идти в порядке соответвующем signals[].
 * */
typedef enum
{
  SIGNAL_ACOUSTIC_MARKS_CHANGED,      /* Изменение данных в модели "водопадных" меток. */
  SIGNAL_GEO_MARKS_CHANGED,           /* Изменение данных в модели гео-меток. */
  SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED,  /* Изменение данных в модели "водопадных" меток с координатами. */
  SIGNAL_LABELS_CHANGED,              /* Изменение данных в модели групп. */
  SIGNAL_TRACKS_CHANGED,              /* Изменение данных в модели галсов. */
  SIGNAL_GROUPING_CHANGED,            /* Изменение типа группировки. */
  SIGNAL_EXPAND_NODES_MODE_CHANGED,   /* Изменение режима ототбражения всех узлов. */
  SIGNAL_VIEW_MODEL_UPDATED,          /* Обновление модели представления данных. */
  SIGNAL_ITEM_SELECTED,               /* Выделена строка. */
  SIGNAL_VIEW_SCROLLED_HORIZONTAL,    /* Изменение положения горизонтальной прокрутки представления. */
  SIGNAL_VIEW_SCROLLED_VERTICAL,      /* Изменение положения вертикальной прокрутки представления. */
  SIGNAL_MODEL_MANAGER_LAST           /* Количество сигналов. */
}ModelManagerSignal;

/* Тип представления. */
typedef enum
{
  UNGROUPED,       /* Табличное. */
  BY_LABELS,       /* Древовидный с группировкой по группам. */
  BY_TYPES,        /* Древовидный с группировкой по типам. */
  N_VIEW_TYPES     /* Количество типов представления. */
}ModelManagerGrouping;

/* Типы объектов. */
typedef enum
{
  LABEL,          /* Группа. */
  GEO_MARK,       /* гео-метка. */
  ACOUSTIC_MARK,  /* Акустическая метка. */
  TRACK,          /* Галс. */
  TYPES           /* Количество объектов. */
}ModelManagerObjectType;

/* Столбцы для табличного представления. */
enum
{
  COLUMN_ID,          /* Идентификатор объекта в базе данных. */
  COLUMN_NAME,        /* Название объекта. */
  COLUMN_DESCRIPTION, /* Описание объекта. */
  COLUMN_OPERATOR,    /* Оператор, создавший объект. */
  COLUMN_TYPE,        /* Тип объекта: группа, гео-метка, "водопадная" метка или галс. */
  COLUMN_ICON,        /* Название картинки. */
  COLUMN_ACTIVE,      /* Статус чек-бокса. */
  COLUMN_LABEL,       /* Метки групп к которым принадлежит объект. */
  COLUMN_CTIME,       /* Время создания объекта. */
  COLUMN_MTIME,       /* Врем модификации объекта. */
  MAX_COLUMNS         /* Общее количество колонок для представления данных. */
};

struct _HyScanModelManager
{
  GObject parent_instance;

  HyScanModelManagerPrivate *priv;
};

struct _HyScanModelManagerClass
{
  GObjectClass parent_class;
};

GType                hyscan_model_manager_get_type                    (void);

HyScanModelManager*  hyscan_model_manager_new                         (const gchar            *project_name,
                                                                       HyScanDB               *db,
                                                                       HyScanCache            *cache);

HyScanDBInfo*        hyscan_model_manager_get_track_model             (HyScanModelManager     *self);

HyScanObjectModel*   hyscan_model_manager_get_acoustic_mark_model     (HyScanModelManager     *self);

HyScanObjectModel*   hyscan_model_manager_get_geo_mark_model          (HyScanModelManager     *self);

HyScanObjectModel*   hyscan_model_manager_get_label_model             (HyScanModelManager     *self);

HyScanMarkLocModel*  hyscan_model_manager_get_acoustic_mark_loc_model (HyScanModelManager     *self);

GtkTreeModel*        hyscan_model_manager_get_view_model              (HyScanModelManager     *self);

const gchar*         hyscan_model_manager_get_signal_title            (HyScanModelManager     *self,
                                                                       ModelManagerSignal      signal_title);

gchar*               hyscan_model_manager_get_project_name            (HyScanModelManager     *self);

HyScanDB*            hyscan_model_manager_get_db                      (HyScanModelManager     *self);

HyScanCache*         hyscan_model_manager_get_cache                   (HyScanModelManager     *self);

GHashTable*          hyscan_model_manager_get_items                   (HyScanModelManager     *self,
                                                                       ModelManagerObjectType  type);

gchar**              hyscan_model_manager_get_all_tracks_id           (HyScanModelManager     *self);

gchar**              hyscan_model_manager_get_selected_tracks_id      (HyScanModelManager     *self);

void                 hyscan_model_manager_set_grouping                (HyScanModelManager     *self,
                                                                       ModelManagerGrouping    grouping);

ModelManagerGrouping hyscan_model_manager_get_grouping                (HyScanModelManager     *self);

void                 hyscan_model_manager_set_expand_nodes_mode       (HyScanModelManager     *self,
                                                                       gboolean                expand_nodes_mode);

gboolean             hyscan_model_manager_get_expand_nodes_mode       (HyScanModelManager     *self);

void                 hyscan_model_manager_set_selection               (HyScanModelManager     *self,
                                                                       GtkTreeSelection       *selection);

GtkTreeSelection*    hyscan_model_manager_get_selected_items          (HyScanModelManager     *self);

void                 hyscan_model_manager_set_horizontal_adjustment   (HyScanModelManager     *self,
                                                                       GtkAdjustment          *adjustment);

void                 hyscan_model_manager_set_vertical_adjustment     (HyScanModelManager     *self,
                                                                       GtkAdjustment          *adjustment);

GtkAdjustment*       hyscan_model_manager_get_horizontal_adjustment   (HyScanModelManager     *self);

GtkAdjustment*       hyscan_model_manager_get_vertical_adjustment     (HyScanModelManager     *self);

G_END_DECLS

#endif /* __HYSCAN_MODEL_MANAGER_H__ */

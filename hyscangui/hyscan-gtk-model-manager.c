/* hyscan-gtk-model-manager.c
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

/**
 * SECTION: hyscan-gtk-model-manager
 * @Short_description: Класс реализует обмен данными между различными моделями
 * и Журналом Меток, и выполняет функции с различными объктами.
 * @Title: HyScanGtkModelManager
 * @See_also: #HyScanGtkMarkManager, #HyScanGtkMarkManagerView
 *
 * Класс #HyScanGtkModelManager содержит данные из нескольких моделей и реализует общую
 * модель для табличного или древовидного представления, и функции для работы с объектами
 * и обмена данными. Виджет #HyScanGtkMarkManager реализует представление данных общей
 * модели и управление.
 *
 * - hyscan_gtk_mark_manager_view_new () - создание нового представления;
 * - hyscan_gtk_model_manager_get_track_model () - модель данных с информацией о галсах;
 * - hyscan_gtk_model_manager_get_acoustic_mark_model () - модель данных с информацией об акустических метках;
 * - hyscan_gtk_model_manager_get_geo_mark_model () - модель данных с информацией о гео-метках;
 * - hyscan_gtk_model_manager_get_label_model () - модель данных с информацией о группах;
 * - hyscan_gtk_model_manager_get_acoustic_mark_loc_model - модель данных с информацией об акустических метках c координатами;
 * - hyscan_gtk_model_manager_get_view_model () - модель представления данных для #HyScanMarkManager;
 * - hyscan_gtk_model_manager_get_signal_title () - название сигнала Менеджера Моделей;
 * - hyscan_gtk_model_manager_set_project_name () - смена названия проекта;
 * - hyscan_gtk_model_manager_get_project_name () - название проекта;
 * - hyscan_gtk_model_manager_get_export_folder () - директория для экспорта;
 * - hyscan_gtk_model_manager_get_db () - база данных;
 * - hyscan_gtk_model_manager_get_cache () - кэш;
 * - hyscan_gtk_model_manager_get_all_tracks_id () - список всех галсов;
 * - hyscan_gtk_model_manager_set_grouping () - установка типа группировки древовидного представления: по типам, по группам;
 * - hyscan_gtk_model_manager_get_grouping () - тип группировки;
 * - hyscan_gtk_model_manager_set_selected_item () - выделение объекта;
 * - hyscan_gtk_model_manager_get_selected_item () - идентификатор выделенного объект;
 * - hyscan_gtk_model_manager_get_selected_track () - идентификатор выделенного галса;
 * - hyscan_gtk_model_manager_unselect_all () - снятие выделения;
 * - hyscan_gtk_model_manager_set_horizontal_adjustment () - установка положения горизонтальной полосы прокрутки;
 * - hyscan_gtk_model_manager_set_vertical_adjustment () -  установка положения вертикальной полосы прокрутки;
 * - hyscan_gtk_model_manager_get_horizontal_adjustment () - текущее положение горизонтальной полосы прокрутки;
 * - hyscan_gtk_model_manager_get_vertical_adjustment () - текущее положение вертикальной полосы прокрутки;
 * - hyscan_gtk_model_manager_toggle_item () - установка состояния чек-бокса для объекта по идентификатору;
 * - hyscan_gtk_model_manager_get_toggled_items () - список идентификаторов объектов с активированным чек-боксом;
 * - hyscan_gtk_model_manager_expand_item () - установка состояния узла объекта по идентификатору;
 * - hyscan_gtk_model_manager_get_current_id () - идентифифкатор объекта;
 * - hyscan_gtk_model_manager_delete_toggled_items () - удаление объектов с активированным чек-боксом из базы данных;
 * - hyscan_gtk_model_manager_has_toggled () - наличие объектов с активированным чек-боксом;
 * - hyscan_gtk_model_manager_toggled_items_set_labels () - назначение групп объектам с активированным чек-боксом;
 * - hyscan_gtk_model_manager_toggled_items_get_bit_masks () - наполнение битовых масок в соответствии с группами выбранных объектов;
 * - hyscan_gtk_model_manager_show_object () - отображение объекта на карте;
 * - hyscan_gtk_model_manager_get_all_labels () - указатель на хэш-таблицу с глобальными группами и группами из базы данных.
 */

#include <hyscan-gtk-model-manager.h>
#include <hyscan-gui-marshallers.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <limits.h>

/* Максимальная длина строки описания объекта отображаемой в подсказке. */
#define DESCRIPTION_MAX_LENGTH 32
/* Макрос для освобождения ресурсов модели. */
#define RELEASE_MODEL(model, object)\
if ((model) != NULL)\
  {\
    g_signal_handlers_disconnect_by_data ((model), (object));\
    g_object_unref ((model));\
    (model) = NULL;\
  }

/* Макрос для обновления указателя на модель.
 * model_in_global  - указатель на элемент массива model[TYPES],
 * model_in_private - указатель на модель в HyScanGtkModelManagerPrivate.
 * * */
#define REFRESH_MODEL_POINTER(model_in_global, model_in_private)\
if ( (model_in_global) != (gpointer)(model_in_private))\
  (model_in_global) = (gpointer)(model_in_private);

/* Макрос для отправки сигнала по идентификатору. */
#define EMIT_SIGNAL(signal_id)\
HyScanGtkModelManagerPrivate *priv = self->priv;\
priv->update_model_flag = TRUE;\
g_signal_emit (self, hyscan_model_manager_signals[(signal_id)], 0);
/* Макрос блокировки сигнала. */
#define BLOCK_SIGNAL(signal, model)\
if ((signal) != 0)\
  g_signal_handler_block ((model), (signal));

/* Макрос разблокировки сигнала */
#define UNBLOCK_SIGNAL(signal, model)\
if ((signal) != 0)\
  g_signal_handler_unblock ((model), (signal));

/* Макрос добавляет объект в модель данных. */
#define ADD_NODE_IN_STORE(current, parent, id, name, desc, user, type, toggled, label)\
gtk_tree_store_append (store, (current), (parent));\
gtk_tree_store_set (store,               (current),\
                    COLUMN_ID,           (id),\
                    COLUMN_NAME,         (name),\
                    COLUMN_DESCRIPTION,  (desc),\
                    COLUMN_OPERATOR,     (user),\
                    COLUMN_TYPE,         (type),\
                    COLUMN_ICON,         icon,\
                    COLUMN_ACTIVE,       (toggled),\
                    COLUMN_VISIBLE,      TRUE,\
                    COLUMN_LABEL,        (label),\
                    -1);

/* Макрос добавляет атрибут объекта в модель данных. */
#define ADD_ATTRIBUTE_IN_STORE(str)\
{\
  GtkTreeIter item_iter;\
  gtk_tree_store_append (store, &item_iter, &child_iter);\
  gtk_tree_store_set (store,          &item_iter,\
                      COLUMN_NAME,     (str),\
                      COLUMN_ICON,     icon,\
                      COLUMN_ACTIVE,   toggled,\
                      COLUMN_VISIBLE,  FALSE,\
                      -1);\
}

enum
{
  PROP_PROJECT_NAME = 1,    /* Название проекта. */
  PROP_DB,                  /* База данных. */
  PROP_CACHE,               /* Кэш.*/
  PROP_EXPORT_FOLDER,       /* Директория для экспорта. */
  N_PROPERTIES
};

/* Атрибуты объекта. */
enum
{
  DESCRIPTION,              /* Описание. */
  OPERATOR,                 /* Оператор. */
  CREATION_TIME,            /* Время сохдания. */
  MODIFICATION_TIME,        /* Время изменения. */
  LOCATION,                 /* Координаты. */
  BOARD,                    /* Борт по умолчанию. */
  PORT,                     /* Левый борт. */
  STARBOARD,                /* Правый борт. */
  BOTTOM,                   /* Под собой. */
  DEPTH,                    /* Глубина. */
  WIDTH,                    /* Ширина. */
  SLANT_RANGE,              /* Наклонная дальность. */
  N_ATTR                    /* Количество атрибутов. */
};

/* Типы записей в модели. */
typedef enum
{
  PARENT,                   /* Узел. */
  CHILD,                    /* Объект. */
  ITEM                      /* Атрибут объекта. */
} ExtensionType;

/* Глобальные группы.
 * GLOBAL_LABEL_FIRST и GLOBAL_LABEL_LAST для перебора по группам.
 * * */
typedef enum
{
  GLOBAL_LABEL_OBJECT,                        /* Объект. */
  GLOBAL_LABEL_FIRST = GLOBAL_LABEL_OBJECT,   /* Первая глобальная группа - Объект. */
  GLOBAL_LABEL_DANGER,                        /* Опасность. */
  GLOBAL_LABEL_UNIDENTIFIED,                  /* Неизвестный. */
  GLOBAL_LABEL_GROUP,                         /* Группа. */
  GLOBAL_LABEL_UNINSPECTED,                   /* Необследованный. */
  GLOBAL_LABEL_INTERESTING,                   /* Интересный. */
  GLOBAL_LABEL_SHALLOW,                       /* Мелководье. */
  GLOBAL_LABEL_DEPTH,                         /* Глубина. */
  GLOBAL_LABELS,                              /* Общее количество глобальных групп. */
  GLOBAL_LABEL_LAST = GLOBAL_LABELS           /* Последняя глобальная группа + 1. */
}GlobalLabel;

/* Структура содержащая расширеную информацию об объектах. */
typedef struct
{
  ExtensionType  type;      /* Тип записи. */
  gboolean       active,    /* Состояние чек-бокса. */
                 expanded;  /* Развёрнут ли объект (для древовидного представления). */
}Extension;

struct _HyScanGtkModelManagerPrivate
{
  HyScanObjectModel    *acoustic_mark_model,  /* Модель данных акустических меток. */
                       *geo_mark_model,       /* Модель данных гео-меток. */
                       *label_model;          /* Модель данных групп. */
  HyScanPlannerModel   *planner_model;        /* Модель данных планирования. */
  HyScanMarkLocModel   *acoustic_loc_model;   /* Модель данных акустических меток с координатами. */
  HyScanDBInfo         *track_model;          /* Модель данных галсов. */
  HyScanUnits          *units;                /* Модель единиц измерения. */
  GtkTreeModel         *view_model;           /* Модель представления данных (табличное или древовидное). */
  HyScanCache          *cache;                /* Кэш.*/
  HyScanDB             *db;                   /* База данных. */
  ModelManagerGrouping  grouping;             /* Тип группировки. */
  Extension            *node[TYPES];          /* Информация для описания узлов для древовидного
                                               * представления с группировкой по типам. */
  GHashTable           *extensions[TYPES];    /* Массив таблиц с дополнительной информацией для всех типов объектов. */
  GType                 store_format[MAX_COLUMNS];
  gdouble               horizontal,           /* Положение горизонтальной полосы прокрутки. */
                        vertical;             /* Положение вертикальной полосы прокрутки. */
  gulong                signal_tracks_changed,
                        signal_acoustic_marks_changed,
                        signal_acoustic_marks_loc_changed,
                        signal_geo_marks_changed,
                        signal_labels_changed;
  gchar                *project_name,         /* Название проекта. */
                       *export_folder,        /* Директория для экспорта. */
                       *selected_item_id,     /* Выделенные объекты. */
                       *current_id;           /* Идентифифкатор объекта, используется для разворачивания
                                               * и сворачивания узлов. */
  gboolean              clear_model_flag,     /* Флаг очистки модели. */
                        constructed_flag,     /* Флаг инициализации всех моделей. */
                        update_model_flag;    /* Флаг обновления модели представления данных. */
};

/* Названия сигналов.
 * Должны идти в порядке соответвующем ModelManagerSignal.
 * */
static const gchar *signals[] = {"wf-marks-changed",     /* Изменение данных в модели акустических меток. */
                                 "geo-marks-changed",    /* Изменение данных в модели гео-меток. */
                                 "wf-marks-loc-changed", /* Изменение данных в модели акустических меток с координатами. */
                                 "labels-changed",       /* Изменение данных в модели групп. */
                                 "tracks-changed",       /* Изменение данных в модели галсов. */
                                 "grouping-changed",     /* Изменение типа группировки. */
                                 "view-model-updated",   /* Обновление модели представления данных. */
                                 "item-selected",        /* Выделена строка. */
                                 "item-toggled",         /* Изменено состояние чек-бокса. */
                                 "item-expanded",        /* Разворачивание узла древовидного представления. */
                                 "item-collapsed",       /* Сворачивание узла древовидного представления. */
                                 "scrolled-horizontal",  /* Изменение положения горизонтальной прокрутки представления. */
                                 "scrolled-vertical",    /* Изменение положения вертикальной прокрутки представления. */
                                 "unselect",             /* Снятие выделения. */
                                 "show-object"};         /* Показать объект на карте. */
/* Форматированная строка для вывода времени и даты. */
static const gchar *date_time_stamp = "%d.%m.%Y %H:%M:%S";
/* Форматированная строка для вывода расстояния. */
static const gchar *distance_stamp = N_("%.2f m");
/* Оператор создавший типы объектов в древовидном списке. */
static const gchar *author = N_("Default");
/* Строка если нет данных по какому-либо атрибуту. */
static const gchar *unknown = N_("Unknown");
/* Стандартные картинки для типов объектов. */
static const gchar *type_icon[TYPES] =  {
    "/org/hyscan/icons/emblem-documents.png",            /* Группы. */
    "/org/hyscan/icons/mark-location.png",               /* Гео-метки. */
    "/org/hyscan/icons/emblem-photos.png",               /* Акустические метки. */
    "/org/hyscan/icons/preferences-system-sharing.png"   /* Галсы. *//* Графические изображения для атрибутов обьектов. */
};
/* Графические изображения для атрибутов обьектов. */
static const gchar *attr_icon[N_ATTR] = {
    "/org/hyscan/icons/accessories-text-editor.png",                     /* Описание. */
    "/org/hyscan/icons/avatar-default.png",                              /* Оператор. */
    "/org/hyscan/icons/appointment-new.png",                             /* Время создания. */
    "/org/hyscan/icons/document-open-recent.png",                        /* Время изменения. */
    "/org/hyscan/icons/preferences-desktop-locale.png",                  /* Координаты */
    "/org/hyscan/icons/network-wireless-no-route-symbolic.symbolic.png", /* Борт по умолчанию. */
    "/org/hyscan/icons/go-previous.png",                                 /* Левый борт. */
    "/org/hyscan/icons/go-next.png",                                     /* Правый борт. */
    "/org/hyscan/icons/go-down.png",                                     /* Под собой. */
    "/org/hyscan/icons/object-flip-vertical.png",                        /* Глубина. */
    "/org/hyscan/icons/object-flip-horizontal.png",                      /* Ширина. */
    "/org/hyscan/icons/content-loading-symbolic.symbolic.png"            /* Наклонная дальность. */
};
/* Названия типов. */
static const gchar *type_name[] =    {N_("Labels"),                      /* Группы. */
                                      N_("Geo-marks"),                   /* Гео-метки. */
                                      N_("Acoustic marks"),              /* Акустические метки. */
                                      N_("Tracks")};                     /* Галсы. */
/* Описания типов. */
static const gchar *type_desc[] =    {N_("All labels"),                  /* Группы. */
                                      N_("All geo-marks"),               /* Гео-метки. */
                                      N_("All acoustic marks"),          /* Акустические метки. */
                                      N_("All tracks")};                 /* Галсы. */
/* Идентификаторы для узлов для древовидного представления с группировкой по типам. */
static const gchar *type_id[TYPES] = {"ID_NODE_LABEL",                   /* Группы. */
                                      "ID_NODE_GEO_MARK",                /* Гео-метки.*/
                                      "ID_NODE_ACOUSTIC_MARK",           /* Акустические метки. */
                                      "ID_NODE_TRACK"};                  /* Галсы. */
/* Массив глобальных групп. */
static HyScanLabel* global[GLOBAL_LABELS] = { 0 };
/* Названия глобальных групп. */
static const gchar *global_label_name[GLOBAL_LABELS] = {
    N_("Object"),       /* Объект. */
    N_("Danger"),       /* Опасность. */
    N_("Unidentified"), /* Неизвестный. */
    N_("Group"),        /* Группа. */
    N_("Uninspected"),  /* Необследованный. */
    N_("Interesting"),  /* Интересный. */
    N_("Shallow"),      /* Мелководье. */
    N_("Depth")         /* Глубина. */
};

/* Описания глобальных групп. */
static const gchar *global_label_desc[GLOBAL_LABELS] = {
    N_("Different objects."),                                                          /* Объект. */
    N_("Dangerous objects or targets."),                                               /* Опасность. */
    N_("Unidentified objects or targets."),                                            /* Неизвестный. */
    N_("Objects or targets located close to each other (as a group), group objects."), /* Группа. */
    N_("Objects or targets identified, but not inspected."),                           /* Необследованный. */
    N_("Objects or targets interesting for inspection or exploration."),               /* Интересный. */
    N_("Objects or targets located at shallow or low depth."),                         /* Мелководье. */
    N_("Objects or targets located at deep water.")                                    /* Глубина. */
};
/* Описания глобальных групп. */
static const gchar *global_label_icon[GLOBAL_LABELS] = {
    "/org/hyscan/icons/object.png",               /* Объект. */
    "/org/hyscan/icons/dialog-warning.png",       /* Опасность. */
    "/org/hyscan/icons/dialog-question.png",      /* Неизвестный. */
    "/org/hyscan/icons/multy-object.png",         /* Группа. */
    "/org/hyscan/icons/emblem-new.png",           /* Необследованный. */
    "/org/hyscan/icons/dialog-information.png",   /* Интересный. */
    "/org/hyscan/icons/shallow.png",              /* Мелководье. */
    "/org/hyscan/icons/depth.png"                 /* Глубина. */
};
/* Массив указателей на модели групп, гео-меток, акустических меток и галсов для быстрого доступа к моделям. */
static gpointer model[TYPES] = {NULL, NULL, NULL, NULL};
/* Cкорость движения при которой генерируются тайлы в Echosounder-е, но метка
 * сохраняется в базе данных без учёта этого коэфициента масштабирования.
 * * */
static gdouble ship_speed = 10.0;
/* Идентификатор для отслеживания изменения названия проекта. */
static GParamSpec *notify = NULL;

static void          hyscan_gtk_model_manager_set_property                      (GObject                *object,
                                                                                 guint                   prop_id,
                                                                                 const GValue           *value,
                                                                                 GParamSpec             *pspec);

static void          hyscan_gtk_model_manager_get_property                      (GObject                *object,
                                                                                 guint                   prop_id,
                                                                                 GValue                 *value,
                                                                                 GParamSpec             *pspec);

static void          hyscan_gtk_model_manager_constructed                       (GObject                *object);

static void          hyscan_gtk_model_manager_finalize                          (GObject                *object);

static void          hyscan_gtk_model_manager_acoustic_mark_model_changed       (HyScanObjectModel      *model,
                                                                                 HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_track_model_changed               (HyScanDBInfo           *model,
                                                                                 HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_acoustic_marks_loc_model_changed  (HyScanMarkLocModel     *model,
                                                                                 HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_geo_mark_model_changed            (HyScanObjectModel      *model,
                                                                                 HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_label_model_changed               (HyScanObjectModel      *model,
                                                                                 HyScanGtkModelManager  *self);

static GtkTreeModel* hyscan_gtk_model_manager_update_view_model                 (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_set_view_model_ungrouped          (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_set_view_model_by_types           (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_set_view_model_by_labels          (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_refresh_geo_marks_ungrouped       (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *store_iter,
                                                                                 GHashTable             *labels);

static void          hyscan_gtk_model_manager_refresh_acoustic_marks_ungrouped  (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *store_iter,
                                                                                 GHashTable             *labels);

static void          hyscan_gtk_model_manager_refresh_tracks_ungrouped          (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *store_iter,
                                                                                 GHashTable             *labels);

static void          hyscan_gtk_model_manager_refresh_labels_by_types           (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_refresh_geo_marks_by_types        (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_refresh_geo_marks_by_labels       (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *iter,
                                                                                 HyScanLabel            *label);

static void          hyscan_gtk_model_manager_refresh_acoustic_marks_by_types   (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_refresh_acoustic_marks_by_labels  (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *iter,
                                                                                 HyScanLabel            *label);

static void          hyscan_gtk_model_manager_refresh_tracks_by_types           (HyScanGtkModelManager  *self);

static void          hyscan_gtk_model_manager_refresh_tracks_by_labels          (HyScanGtkModelManager  *self,
                                                                                 GtkTreeIter            *iter,
                                                                                 HyScanLabel            *label);

static void          hyscan_gtk_model_manager_clear_view_model                  (GtkTreeModel           *view_model,
                                                                                 gboolean               *flag);

static gboolean      hyscan_gtk_model_manager_init_extensions                   (HyScanGtkModelManager  *self);

static Extension*    hyscan_gtk_model_manager_extension_new                     (ExtensionType           type,
                                                                                 gboolean                active,
                                                                                 gboolean                selected);

static Extension*    hyscan_gtk_model_manager_extension_copy                    (Extension              *ext);

static void          hyscan_gtk_model_manager_extension_free                    (gpointer                data);

static GHashTable*   hyscan_gtk_model_manager_get_extensions                    (HyScanGtkModelManager  *self,
                                                                                 ModelManagerObjectType  type);

static gboolean      hyscan_gtk_model_manager_is_all_toggled                    (GHashTable             *table,
                                                                                const gchar            *node_id);

static void          hyscan_gtk_model_manager_delete_track_by_id                (HyScanGtkModelManager  *self,
                                                                                 gchar                  *id,
                                                                                 gboolean                convert);

static gboolean      hyscan_gtk_model_manager_view_model_loop                   (gpointer                user_data);

static GdkPixbuf*    hyscan_gtk_model_manager_get_icon_from_base64              (const gchar            *str);

static guint         hyscan_gtk_model_manager_has_object_with_label             (ModelManagerObjectType  type,
                                                                                 HyScanLabel            *label);

static guint8        hyscan_gtk_model_manager_count_labels                      (guint64                  mask);

static GHashTable*   hyscan_gtk_model_manager_get_labels                        (gpointer                 model);

static GHashTable*   hyscan_gtk_model_manager_get_geo_marks                     (gpointer                 model);

static GHashTable*   hyscan_gtk_model_manager_get_acoustic_makrs                (gpointer                 model);

static GHashTable*   hyscan_gtk_model_manager_get_tracks                        (gpointer                 model);

static void          hyscan_gtk_model_manager_toggled_labels_set_labels         (HyScanGtkModelManager   *self,
                                                                                 guint64                  labels,
                                                                                 guint64                  inconsistents,
                                                                                 gint64                   current_time);

static void          hyscan_gtk_model_manager_toggled_geo_marks_set_labels      (HyScanGtkModelManager   *self,
                                                                                 guint64                  labels,
                                                                                 guint64                  inconsistents,
                                                                                 gint64                   current_time);

static void          hyscan_gtk_model_manager_toggled_acoustic_marks_set_labels (HyScanGtkModelManager   *self,
                                                                                 guint64                  labels,
                                                                                 guint64                  inconsistents,
                                                                                 gint64                   current_time);

static void          hyscan_gtk_model_manager_toggled_tracks_set_labels         (HyScanGtkModelManager   *self,
                                                                                 guint64                  labels,
                                                                                 guint64                  inconsistents,
                                                                                 gint64                   current_time);

static guint         hyscan_model_manager_signals[SIGNAL_MODEL_MANAGER_LAST] = { 0 };

/* Массив указателей на функции обновления модели данных без группировки. */
static void          (*refresh_model_ungrouped[TYPES])                          (HyScanGtkModelManager   *self,
                                                                                 GtkTreeIter             *store_iter,
                                                                                 GHashTable              *labels) = {
0,                                                         /* Группы. */
hyscan_gtk_model_manager_refresh_geo_marks_ungrouped,      /* Гео-метки. */
hyscan_gtk_model_manager_refresh_acoustic_marks_ungrouped, /* Акустические метки. */
hyscan_gtk_model_manager_refresh_tracks_ungrouped};        /* Галсы. */

/* Массив указателей на функции обновления модели данных при группировке по типам. */
static void          (*refresh_model_by_types[TYPES])                           (HyScanGtkModelManager   *self) = {
hyscan_gtk_model_manager_refresh_labels_by_types,          /* Группы. */
hyscan_gtk_model_manager_refresh_geo_marks_by_types,       /* Гео-метки. */
hyscan_gtk_model_manager_refresh_acoustic_marks_by_types,  /* Акустические метки. */
hyscan_gtk_model_manager_refresh_tracks_by_types};         /* Галсы. */

/* Массив указателей на функции обновления модели данных при группировке по группам. */
static void          (*refresh_model_by_labels[TYPES])                          (HyScanGtkModelManager   *self,
                                                                                 GtkTreeIter             *iter,
                                                                                 HyScanLabel             *label) = {
0,                                                         /* Группы. */
hyscan_gtk_model_manager_refresh_geo_marks_by_labels,      /* Гео-метки. */
hyscan_gtk_model_manager_refresh_acoustic_marks_by_labels, /* Акустические метки. */
hyscan_gtk_model_manager_refresh_tracks_by_labels};        /* Галсы. */

/* Массив укзателей на функции получения таблиц с объектами. */
static GHashTable*   (*get_objects_from_model[TYPES])                           (gpointer                 model) = {
hyscan_gtk_model_manager_get_labels,                       /* Группы. */
hyscan_gtk_model_manager_get_geo_marks,                    /* Гео-метки. */
hyscan_gtk_model_manager_get_acoustic_makrs,               /* Акустические метки. */
hyscan_gtk_model_manager_get_tracks};                      /* Галсы. */

/* Массив указателей на функции присвоения выделенным объектам битовой
 * маски групп и обновления даты и времени сделанных изменений.
 * * */
static void          (*toggled_items_set_label[TYPES])                          (HyScanGtkModelManager   *self,
                                                                                 guint64                  labels,
                                                                                 guint64                  inconsistents,
                                                                                 gint64                   current_time) = {
hyscan_gtk_model_manager_toggled_labels_set_labels,         /* Группы. */
hyscan_gtk_model_manager_toggled_geo_marks_set_labels,      /* Гео-метки. */
hyscan_gtk_model_manager_toggled_acoustic_marks_set_labels, /* Акустические метки. */
hyscan_gtk_model_manager_toggled_tracks_set_labels};        /* Галсы. */

/* Массив указателей на функции создающие модели для различных типов представления данных. */
static void          (*set_view_model[VIEW_TYPES])                             (HyScanGtkModelManager   *self) = {
hyscan_gtk_model_manager_set_view_model_ungrouped,          /* Табличное без группировки. */
hyscan_gtk_model_manager_set_view_model_by_types,           /* Древовидное c группировкой по типам. */
hyscan_gtk_model_manager_set_view_model_by_labels           /* Древовидное с группировкой по группам. */
};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkModelManager, hyscan_gtk_model_manager, G_TYPE_OBJECT)

static void
hyscan_gtk_model_manager_class_init (HyScanGtkModelManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_model_manager_set_property;
  object_class->get_property = hyscan_gtk_model_manager_get_property;
  object_class->constructed  = hyscan_gtk_model_manager_constructed;
  object_class->finalize     = hyscan_gtk_model_manager_finalize;

  notify = g_param_spec_string ("project_name", "Project_name", "Project name", "",
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_PROJECT_NAME, notify);

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Data base", "The link to data base",
                         HYSCAN_TYPE_DB, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "The link to main cache with frequency used stafs",
                         HYSCAN_TYPE_CACHE, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_EXPORT_FOLDER,
    g_param_spec_string ("export_folder", "Export folder", "Directory for export", "",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Сигналы. */
  for (guint index = 0; index < SIGNAL_MODEL_MANAGER_LAST; index++)
    {
      /* Сигнал изменения состояния чек-бокса. */
      if (index == SIGNAL_ITEM_TOGGLED)
        {
          hyscan_model_manager_signals[index] =
                 g_signal_new (signals[index],
                               HYSCAN_TYPE_GTK_MODEL_MANAGER,
                               G_SIGNAL_RUN_LAST,
                               0, NULL, NULL,
                               hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                               G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
        }
      else if (index == SIGNAL_SHOW_OBJECT)
        {
          /* Сигнал показать объект на карте. */
          hyscan_model_manager_signals[index] =
                 g_signal_new (signals[index],
                               HYSCAN_TYPE_GTK_MODEL_MANAGER,
                               G_SIGNAL_RUN_LAST,
                               0, NULL, NULL,
                               hyscan_gui_marshal_VOID__STRING_UINT,
                               G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);
        }
      else
        {
          /* Остальные сигналы. */
          hyscan_model_manager_signals[index] =
                 g_signal_new (signals[index],
                               HYSCAN_TYPE_GTK_MODEL_MANAGER,
                               G_SIGNAL_RUN_LAST,
                               0, NULL, NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
        }
    }
}

static void
hyscan_gtk_model_manager_init (HyScanGtkModelManager *self)
{
  self->priv = hyscan_gtk_model_manager_get_instance_private (self);
  /* Столбцы с типами данных для модели. */
  self->priv->store_format[COLUMN_ID]          =
  self->priv->store_format[COLUMN_NAME]        =
  self->priv->store_format[COLUMN_DESCRIPTION] =
  self->priv->store_format[COLUMN_OPERATOR]    =
  self->priv->store_format[COLUMN_CTIME]       =
  self->priv->store_format[COLUMN_MTIME]       =
  self->priv->store_format[COLUMN_LOCATION]    =
  self->priv->store_format[COLUMN_TRACK_NAME]  =
  self->priv->store_format[COLUMN_BOARD]       =
  self->priv->store_format[COLUMN_DEPTH]       =
  self->priv->store_format[COLUMN_WIDTH]       =
  self->priv->store_format[COLUMN_SLANT_RANGE] = G_TYPE_STRING;
  self->priv->store_format[COLUMN_ACTIVE]      =
  self->priv->store_format[COLUMN_VISIBLE]     = G_TYPE_BOOLEAN;
  self->priv->store_format[COLUMN_TYPE]        = G_TYPE_UINT;
  self->priv->store_format[COLUMN_ICON]        = HYSCAN_TYPE_GTK_MARK_MANAGER_ICON;
  self->priv->store_format[COLUMN_LABEL]       = G_TYPE_UINT64;
}

static void
hyscan_gtk_model_manager_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkModelManager *self = HYSCAN_GTK_MODEL_MANAGER (object);
  HyScanGtkModelManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_PROJECT_NAME:
      {
        if (priv->constructed_flag)
          hyscan_gtk_model_manager_set_project_name (self, g_value_get_string (value));
        else
          priv->project_name = g_value_dup_string (value);
      }
      break;

    case PROP_DB:
      {
        priv->db  = g_value_dup_object (value);
      }
      break;

    case PROP_CACHE:
      {
        priv->cache  = g_value_dup_object (value);
      }
      break;

    case PROP_EXPORT_FOLDER:
      {
        priv->export_folder = g_value_dup_string (value);
      }
      break;

    default:
      {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
    }
}

static void
hyscan_gtk_model_manager_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  HyScanGtkModelManager *self = HYSCAN_GTK_MODEL_MANAGER (object);
  HyScanGtkModelManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_PROJECT_NAME:
      g_value_set_string (value, priv->project_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_model_manager_constructed (GObject *object)
{
  HyScanGtkModelManager *self = HYSCAN_GTK_MODEL_MANAGER (object);
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanProjectInfo *project_info;
  gint64 project_creation_time;

  priv->track_model = hyscan_db_info_new (priv->db);
  hyscan_db_info_set_project (priv->track_model, priv->project_name);
  priv->signal_tracks_changed = g_signal_connect (priv->track_model, "tracks-changed",
                    G_CALLBACK (hyscan_gtk_model_manager_track_model_changed), self);

  priv->acoustic_loc_model = hyscan_mark_loc_model_new (priv->db, priv->cache);
  hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);
  priv->signal_acoustic_marks_loc_changed = g_signal_connect (priv->acoustic_loc_model, "changed",
                    G_CALLBACK (hyscan_gtk_model_manager_acoustic_marks_loc_model_changed), self);

  priv->acoustic_mark_model = hyscan_object_model_new ();
  hyscan_object_model_set_types (priv->acoustic_mark_model, 1, HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  hyscan_object_model_set_project (priv->acoustic_mark_model, priv->db, priv->project_name);
  priv->signal_acoustic_marks_changed = g_signal_connect (priv->acoustic_mark_model, "changed",
                    G_CALLBACK (hyscan_gtk_model_manager_acoustic_mark_model_changed), self);

  priv->geo_mark_model = hyscan_object_model_new ();
  hyscan_object_model_set_types (priv->geo_mark_model, 1, HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);
  priv->signal_geo_marks_changed = g_signal_connect (priv->geo_mark_model, "changed",
                    G_CALLBACK (hyscan_gtk_model_manager_geo_mark_model_changed), self);

  priv->planner_model = hyscan_planner_model_new ();
  hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (priv->planner_model), priv->db, priv->project_name);

  priv->units = hyscan_units_new ();

  priv->label_model = hyscan_object_model_new ();
  hyscan_object_model_set_types (priv->label_model, 1, HYSCAN_TYPE_OBJECT_DATA_LABEL);
  hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
  priv->signal_labels_changed = g_signal_connect (priv->label_model, "changed",
                    G_CALLBACK (hyscan_gtk_model_manager_label_model_changed), self);

  /* Время создания проекта для глобальных групп. */
  project_info = hyscan_db_info_get_project_info (priv->db, priv->project_name);
  project_creation_time = g_date_time_to_unix (project_info->ctime);
  hyscan_db_info_project_info_free (project_info);

  /* Создаём глобальные группы. */
  for (GlobalLabel index = GLOBAL_LABEL_FIRST; index < GLOBAL_LABEL_LAST; index++)
    {
      GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource (global_label_icon[index], NULL);
      guint64  bit_mask = 0x1;

      global[index] = hyscan_label_new ();

      bit_mask = (bit_mask << (MAX_CUSTOM_LABELS + index));

      if (pixbuf != NULL)
        {
          GOutputStream *stream = g_memory_output_stream_new_resizable ();

          if (gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, NULL, NULL))
            {
              gpointer data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));
              gsize  length = g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (stream));
              gchar    *str = g_base64_encode ( (const guchar*)data, length);

              hyscan_label_set_icon_data (global[index], (const gchar*)str);

              g_object_unref (pixbuf);
              g_free (str);
            }

          g_object_unref (stream);
        }

      hyscan_label_set_text  (global[index],
                              _(global_label_name[index]),
                              _(global_label_desc[index]),
                              "HyScan");
      hyscan_label_set_label (global[index], bit_mask);
      hyscan_label_set_ctime (global[index], project_creation_time);
      hyscan_label_set_mtime (global[index], project_creation_time);
    }

  priv->view_model = hyscan_gtk_model_manager_update_view_model (self);
  /* Устанавливаем флаг инициализации всех моделей. */
  priv->constructed_flag = TRUE;
  /* Запускам циклическую проверку для обновдения модели данных представления. */
  g_timeout_add (100, hyscan_gtk_model_manager_view_model_loop, self);
}

static void
hyscan_gtk_model_manager_finalize (GObject *object)
{
  HyScanGtkModelManager *self = HYSCAN_GTK_MODEL_MANAGER (object);
  HyScanGtkModelManagerPrivate *priv = self->priv;

  RELEASE_MODEL (priv->label_model,         object);
  RELEASE_MODEL (priv->track_model,         object);
  RELEASE_MODEL (priv->planner_model,       object);
  RELEASE_MODEL (priv->geo_mark_model,      object);
  RELEASE_MODEL (priv->acoustic_loc_model,  object);
  RELEASE_MODEL (priv->acoustic_mark_model, object);

  g_clear_object (&priv->view_model);
  g_clear_object (&priv->units);

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      if (priv->extensions[type] != NULL)
        {
          g_hash_table_destroy (priv->extensions[type]);
          priv->extensions[type] = NULL;
        }

      if (priv->node[type] != NULL)
        {
          priv->node[type] = NULL;
        }
    }

  /* Удаляем глобальные группы. */
  for (GlobalLabel index = GLOBAL_LABEL_FIRST; index < GLOBAL_LABEL_LAST; index++)
    hyscan_label_free (global[index]);

  g_free (priv->current_id);
  g_free (priv->project_name);
  g_free (priv->export_folder);
  g_free (priv->selected_item_id);

  priv->current_id       =
  priv->project_name     =
  priv->export_folder    =
  priv->selected_item_id = NULL;

  g_object_unref (priv->cache);
  priv->cache = NULL;
  g_object_unref (priv->db);
  priv->db = NULL;

  G_OBJECT_CLASS (hyscan_gtk_model_manager_parent_class)->finalize (object);
}

/* Обработчик сигнала изменения данных в модели галсов. */
static void
hyscan_gtk_model_manager_track_model_changed (HyScanDBInfo          *model,
                                              HyScanGtkModelManager *self)
{
  EMIT_SIGNAL (SIGNAL_TRACKS_CHANGED);
}

/* Обработчик сигнала изменения данных в модели акустических меток. */
static void
hyscan_gtk_model_manager_acoustic_mark_model_changed (HyScanObjectModel     *model,
                                                      HyScanGtkModelManager *self)
{
  EMIT_SIGNAL (SIGNAL_ACOUSTIC_MARKS_CHANGED);
}

/* Обработчик сигнала изменения данных в модели акустических меток с координатами. */
static void
hyscan_gtk_model_manager_acoustic_marks_loc_model_changed (HyScanMarkLocModel    *model,
                                                           HyScanGtkModelManager *self)
{
  EMIT_SIGNAL (SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED);
}

/* Обработчик сигнала изменения данных в модели гео-меток. */
static void
hyscan_gtk_model_manager_geo_mark_model_changed (HyScanObjectModel     *model,
                                                 HyScanGtkModelManager *self)
{
  EMIT_SIGNAL (SIGNAL_GEO_MARKS_CHANGED);
}

/* Обработчик сигнала изменения данных в модели групп. */
static void
hyscan_gtk_model_manager_label_model_changed (HyScanObjectModel     *model,
                                              HyScanGtkModelManager *self)
{
  EMIT_SIGNAL (SIGNAL_LABELS_CHANGED);
}

/* Функция обновляет модель представления данных в соответствии с текущими параметрами. */
static GtkTreeModel*
hyscan_gtk_model_manager_update_view_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GtkTreeModel* (*create_view_model) (gint n_columns, ...);

  REFRESH_MODEL_POINTER (model[LABEL],         priv->label_model);
  REFRESH_MODEL_POINTER (model[GEO_MARK],      priv->geo_mark_model);
  REFRESH_MODEL_POINTER (model[ACOUSTIC_MARK], priv->acoustic_loc_model);
  REFRESH_MODEL_POINTER (model[TRACK],         priv->track_model);

  if (priv->view_model == NULL)
    {
      create_view_model = (priv->grouping == UNGROUPED) ? (gpointer)gtk_list_store_newv : (gpointer)gtk_tree_store_newv;

      priv->view_model = create_view_model (MAX_COLUMNS, priv->store_format);
    }

  if (hyscan_gtk_model_manager_init_extensions (self))
    set_view_model[priv->grouping] (self);

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_MODEL_UPDATED], 0);

  return priv->view_model;
}

/* Функция, если нужно пересоздаёт, настраивает и наполняет модель представления данных
 * для табличного представления без группировки.
 * * */
static void
hyscan_gtk_model_manager_set_view_model_ungrouped (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GtkTreeIter  store_iter;
  /*GHashTable  *labels = hyscan_object_model_get (priv->label_model);*/
  /*GHashTable *labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                                    HYSCAN_TYPE_LABEL);*/
  GHashTable  *labels = hyscan_gtk_model_manager_get_all_labels (self);

  /* Очищаем модель. */
  hyscan_gtk_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
  /* Проверяем, нужно ли пересоздавать модель.*/
  if (GTK_IS_TREE_STORE (priv->view_model))
    {
      g_object_unref (priv->view_model);
      /* Создаём новую модель. */
      priv->view_model = GTK_TREE_MODEL (gtk_list_store_newv (MAX_COLUMNS, priv->store_format));
    }
  /* Обновляем данные в модели. */
  for (ModelManagerObjectType type = GEO_MARK; type < TYPES; type++)
    {
      if (priv->extensions[type] == NULL)
        continue;

      refresh_model_ungrouped[type] (self, &store_iter, labels);
    }

  if (labels != NULL)
    g_hash_table_destroy (labels);
}

/* Функция, если нужно пересоздаёт, настраивает и наполняет модель представления данных
 * для древовидного представления с группировкой по типам.
 * * */
static void
hyscan_gtk_model_manager_set_view_model_by_types (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  /* Очищаем модель. */
  hyscan_gtk_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
  /* Проверяем, нужно ли пересоздавать модель.*/
  if (GTK_IS_LIST_STORE (priv->view_model))
    {
      g_object_unref (priv->view_model);
      /* Создаём новую модель. */
      priv->view_model = GTK_TREE_MODEL (gtk_tree_store_newv (MAX_COLUMNS, priv->store_format));
    }
  /* Обновляем данные в модели. */
  for (ModelManagerObjectType type = GEO_MARK; type < TYPES; type++)
    {
      if (priv->extensions[type] == NULL)
        continue;

      refresh_model_by_types[type] (self);
    }
}

/* Функция, если нужно пересоздаёт, настраивает и наполняет модель представления данных
 * для древовидного представления с группировкой по группам.
 * * */
static void
hyscan_gtk_model_manager_set_view_model_by_labels (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GHashTableIter  table_iter;
  /*GHashTable     *labels = hyscan_object_model_get (priv->label_model);*/
  /*GHashTable     *labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                                        HYSCAN_TYPE_LABEL);*/
  GHashTable     *labels = hyscan_gtk_model_manager_get_all_labels (self);
  HyScanLabel    *label;
  gchar          *id;
  /* Очищаем модель. */
  hyscan_gtk_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
  /* Проверяем, нужно ли пересоздавать модель.*/
  if (GTK_IS_LIST_STORE (priv->view_model))
    {
      g_object_unref (priv->view_model);
      /* Создаём новую модель. */
      priv->view_model = GTK_TREE_MODEL (gtk_tree_store_newv (MAX_COLUMNS, priv->store_format));
    }

  g_hash_table_iter_init (&table_iter, labels);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&label))
    {
      GtkTreeIter   iter, child_iter;
      GtkTreeStore *store;
      Extension    *ext;
      HyScanGtkMarkManagerIcon *icon;
      GDateTime    *time;
      gint          total, counter;
      gchar        *tooltip, *creation_time, *modification_time;
      gboolean      toggled, flag;

      if (label == NULL)
        continue;

      ext     = g_hash_table_lookup (priv->extensions[LABEL], id);
      store   = GTK_TREE_STORE (priv->view_model);
      toggled = (ext != NULL) ? ext->active : FALSE;

      time = g_date_time_new_from_unix_local (label->ctime);
      creation_time = g_date_time_format (time, date_time_stamp),
      g_date_time_unref (time);
      time = g_date_time_new_from_unix_local (label->mtime);
      modification_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      if (IS_NOT_EMPTY(label->description))
        {
          gchar *tmp = g_strdup (label->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("Note: %s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     tmp,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, label),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      label),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         label),
                                     creation_time,
                                     modification_time);
          g_free (tmp);
        }
      else
        {
          g_print ("Hasn't description.\n");
          tooltip = g_strdup_printf (_("Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, label),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      label),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         label),
                                     creation_time,
                                     modification_time);
        }

      g_free (creation_time);
      g_free (modification_time);

      icon = hyscan_gtk_mark_manager_icon_new (NULL,
                            hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                            tooltip);
      g_free (tooltip);
      /* Добавляем новый узел c названием группы в модель */
      ADD_NODE_IN_STORE (&iter, NULL, id, label->name, label->description,
                         label->operator_name, LABEL, toggled, label->label);

      hyscan_gtk_mark_manager_icon_free (icon);
      /* Обновляем данные в модели. */
      for (ModelManagerObjectType type = GEO_MARK; type < TYPES; type++)
        {
          if (priv->extensions[type] == NULL)
            continue;

          refresh_model_by_labels[type] (self, &iter, label);
        }

      if (!gtk_tree_model_iter_children (priv->view_model, &child_iter, &iter))
        continue;

      total   = gtk_tree_model_iter_n_children  (priv->view_model, &iter);
      counter = 0;

      do
        {
          gtk_tree_model_get (priv->view_model, &child_iter,
                              COLUMN_ACTIVE,    &flag,
                              -1);
          if (flag)
            counter++;
        }
      while (gtk_tree_model_iter_next (priv->view_model, &child_iter));

      flag = (counter == total) ? TRUE : FALSE;

      gtk_tree_store_set (store, &iter, COLUMN_ACTIVE, flag, -1);
    }
  g_hash_table_destroy (labels);
}

/* Функция заполняет модель данными о гео-метках без группировки. */
static void
hyscan_gtk_model_manager_refresh_geo_marks_ungrouped (HyScanGtkModelManager *self,
                                                      GtkTreeIter           *store_iter,
                                                      GHashTable            *labels)
{
  HyScanGtkModelManagerPrivate *priv = self-> priv;
  HyScanMarkGeo  *object;
  GtkListStore   *store;
  GHashTableIter  table_iter;
  GHashTable     *geo_marks;
  guint8          counter;
  gchar          *id;

  if (priv->extensions[GEO_MARK] == NULL)
    return;

  /*geo_marks = hyscan_object_model_get (priv->geo_mark_model);*/
  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                           HYSCAN_TYPE_MARK_GEO);

  if (geo_marks == NULL)
    return;

  store = GTK_LIST_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, geo_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      HyScanGtkMarkManagerIcon *icon;
      GdkPixbuf *pixbuf;
      GDateTime *ctime, *mtime;
      GHashTableIter iter;
      HyScanLabel *label;
      Extension *ext;
      gchar     *position, *creation_time, *modification_time, *tooltip, *tmp, *label_name, *key;

      if (object == NULL)
        continue;

      ext   = g_hash_table_lookup (priv->extensions[GEO_MARK], id);
      ctime = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      mtime = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      position = g_strdup_printf ("%.6f° %.6f°", object->center.lat, object->center.lon),
      creation_time     = g_date_time_format (ctime, date_time_stamp),
      modification_time = g_date_time_format (mtime, date_time_stamp);

      g_date_time_unref (ctime);
      g_date_time_unref (mtime);

      tmp = label_name = NULL;

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (object->labels != label->label)
            continue;

          tmp = label->icon_data;
          label_name = label->name;
          break;
        }
      /* Если нет иконки группы, используем иконку из ресурсов. */
      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[GEO_MARK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)tmp);

      if (IS_NOT_EMPTY(object->description))
        {
          tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n"
                                       "%s (WGS 84)"),
                                     _(type_name[GEO_MARK]),
                                     tmp,
                                     creation_time,
                                     modification_time,
                                     position);
          g_free (tmp);
        }
      else
        {
           tooltip = g_strdup_printf (_("%s\n"
                                        "Created: %s\n"
                                        "Modified: %s\n"
                                        "%s (WGS 84)"),
                                      _(type_name[GEO_MARK]),
                                      creation_time,
                                      modification_time,
                                      position);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL, pixbuf, tooltip);
      g_free (tooltip);

      gtk_list_store_append (store, store_iter);
      gtk_list_store_set (store,               store_iter,
                          COLUMN_ID,           id,
                          COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         GEO_MARK,
                          COLUMN_ICON,         icon,
                          COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        object->labels,
                          COLUMN_CTIME,        creation_time,
                          COLUMN_MTIME,        modification_time,
                          COLUMN_LOCATION,     position,
                          -1);
      hyscan_gtk_mark_manager_icon_free (icon);
      g_free (creation_time);
      g_free (modification_time);
      g_free (position);
    }

  g_hash_table_destroy (geo_marks);
}

/* Функция заполняет модель данными об акустических метках без группировки. */
static void
hyscan_gtk_model_manager_refresh_acoustic_marks_ungrouped (HyScanGtkModelManager *self,
                                                           GtkTreeIter           *store_iter,
                                                           GHashTable            *labels)
{
  HyScanGtkModelManagerPrivate *priv = self-> priv;
  HyScanMarkLocation *location;
  GtkListStore   *store;
  GHashTableIter  table_iter;
  GHashTable     *acoustic_marks;
  guint8          counter;
  gchar          *id;

  if (priv->extensions[ACOUSTIC_MARK] == NULL)
    return;

  acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model);

  if (acoustic_marks == NULL)
    return;

  store = GTK_LIST_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, acoustic_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
    {
      HyScanMarkWaterfall *object = location->mark;
      HyScanGtkMarkManagerIcon *icon;
      GdkPixbuf *pixbuf;
      GHashTableIter iter;
      GDateTime *ctime, *mtime;
      HyScanLabel *label;
      Extension *ext;
      gchar *position, *board, *depth, *width, *slant_range, *creation_time, *modification_time, *tooltip, *tmp, *label_name, *key;

      if (object == NULL)
            continue;

      ext   = g_hash_table_lookup (priv->extensions[ACOUSTIC_MARK], id);
      ctime = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      mtime = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      board = g_strdup (_(unknown));
      depth = g_strdup_printf (_(distance_stamp), location->depth);
      width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width);
      position = g_strdup_printf ("%.6f° %.6f°", location->mark_geo.lat, location->mark_geo.lon);
      slant_range = NULL;
      creation_time     = g_date_time_format (ctime, date_time_stamp);
      modification_time = g_date_time_format (mtime, date_time_stamp);

      g_date_time_unref (ctime);
      g_date_time_unref (mtime);

      switch (location->direction)
        {
        case HYSCAN_MARK_LOCATION_PORT:
          {
            gdouble across_start = -location->mark->width - location->across ;
            gdouble across_end   =  location->mark->width - location->across;

            if (across_start < 0 && across_end > 0)
              width = g_strdup_printf (_(distance_stamp), -across_start);

            board       = g_strdup (_("Port"));
            slant_range = g_strdup_printf (_(distance_stamp), location->across);
          }
          break;

        case HYSCAN_MARK_LOCATION_STARBOARD:
          {
            gdouble across_start = location->across - location->mark->width;
            gdouble across_end   = location->across + location->mark->width;

            if (across_start < 0 && across_end > 0)
              width = g_strdup_printf (_(distance_stamp), across_end);

            board       = g_strdup (_("Starboard"));
            slant_range = g_strdup_printf (_(distance_stamp), location->across);
          }
          break;

        case HYSCAN_MARK_LOCATION_BOTTOM:
          {
            width = g_strdup_printf (_(distance_stamp), ship_speed * 2.0 * location->mark->height);
            board = g_strdup (_("Bottom"));
          }
          break;

        default:
          break;
        }

      tmp = label_name = NULL;

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (object->labels != label->label)
            continue;

          tmp  = label->icon_data;
          label_name = label->name;
          break;
        }
      /* Если нет иконки группы, используем иконку из ресурсов. */
      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[ACOUSTIC_MARK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)tmp);

      if (IS_NOT_EMPTY(object->description))
        {
          tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n%s (WGS 84)\n"
                                       "Track: %s\n"
                                       "Board: %s\n"
                                       "Depth: %s\n"
                                       "Width: %s\n"
                                       "Slant range: %s"),
                                      _(type_name[ACOUSTIC_MARK]),
                                      tmp,
                                      creation_time,
                                      modification_time,
                                      position,
                                      location->track_name,
                                      board,
                                      depth,
                                      width,
                                      slant_range);
          g_free (tmp);
        }
      else
        {
           tooltip = g_strdup_printf (_("%s\n"
                                        "Created: %s\n"
                                        "Modified: %s\n%s (WGS 84)\n"
                                        "Track: %s\n"
                                        "Board: %s\n"
                                        "Depth: %s\n"
                                        "Width: %s\n"
                                        "Slant range: %s"),
                                      _(type_name[ACOUSTIC_MARK]),
                                      creation_time,
                                      modification_time,
                                      position,
                                      location->track_name,
                                      board,
                                      depth,
                                      width,
                                      slant_range);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL, pixbuf, tooltip);
      g_free (tooltip);

      gtk_list_store_append (store, store_iter);
      gtk_list_store_set (store,               store_iter,
                          COLUMN_ID,           id,
                          COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         ACOUSTIC_MARK,
                          COLUMN_ICON,         icon,
                          COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        object->labels,
                          COLUMN_CTIME,        creation_time,
                          COLUMN_MTIME,        modification_time,
                          COLUMN_LOCATION,     position,
                          COLUMN_TRACK_NAME,   location->track_name,
                          COLUMN_BOARD,        board,
                          COLUMN_DEPTH,        depth,
                          COLUMN_WIDTH,        width,
                          COLUMN_SLANT_RANGE,  slant_range,
                          -1);
      hyscan_gtk_mark_manager_icon_free (icon);
      g_free (creation_time);
      g_free (modification_time);
      g_free (position);
      g_free (board);
      g_free (depth);
      g_free (width);
      g_free (slant_range);
    }
  g_hash_table_destroy (acoustic_marks);
}

/* Функция заполняет модель данными о галсах без группировки. */
static void
hyscan_gtk_model_manager_refresh_tracks_ungrouped (HyScanGtkModelManager *self,
                                                   GtkTreeIter           *store_iter,
                                                   GHashTable            *labels)
{
  HyScanGtkModelManagerPrivate *priv = self-> priv;
  HyScanTrackInfo *object;
  GtkListStore    *store;
  GHashTable      *tracks;
  GHashTableIter   table_iter;
  guint8           counter;
  gchar           *id;

  if (priv->extensions[TRACK] == NULL)
    return;

  tracks = hyscan_db_info_get_tracks (priv->track_model);

  if (tracks == NULL)
    return;

  store = GTK_LIST_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, tracks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      HyScanGtkMarkManagerIcon *icon;
      HyScanLabel *label;
      GdkPixbuf   *pixbuf;
      GHashTableIter iter;
      Extension *ext;
      gchar     *key, *tmp, *label_name, *creation_time, *modification_time, *tooltip;

      if (object == NULL)
        continue;

      ext = g_hash_table_lookup (priv->extensions[TRACK], id);

      if (object->ctime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->ctime);
          creation_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          creation_time = g_strdup ("");
        }

      if (object->mtime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->mtime);
          modification_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          modification_time = g_strdup ("");
        }

      tmp = label_name = NULL;

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (object->labels != label->label)
            continue;

          tmp  = label->icon_data;
          label_name = label->name;
          break;
        }
      /* Если нет иконки группы, используем иконку из ресурсов. */
      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)tmp);

      if (IS_NOT_EMPTY(object->description))
        {
          tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     _(type_name[TRACK]),
                                     tmp,
                                     creation_time,
                                     modification_time);
          g_free (tmp);
        }
      else
        {
           tooltip = g_strdup_printf (_("%s\n"
                                        "Created: %s\n"
                                        "Modified: %s"),
                                      _(type_name[TRACK]),
                                      creation_time,
                                      modification_time);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL, pixbuf, tooltip);
      g_free (tooltip);

      gtk_list_store_append (store, store_iter);
      gtk_list_store_set (store,               store_iter,
                          COLUMN_ID,           id,
                          COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         TRACK,
                          COLUMN_ICON,         icon,
                          COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        object->labels,
                          COLUMN_CTIME,        creation_time,
                          COLUMN_MTIME,        modification_time,
                          -1);
      hyscan_gtk_mark_manager_icon_free (icon);
      g_free (creation_time);
      g_free (modification_time);
    }
  g_hash_table_destroy (tracks);
}

/* Функция создаёт и заполняет узел "Группы" в древовидной модели с группировкой по типам. */
static void
hyscan_gtk_model_manager_refresh_labels_by_types (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanGtkMarkManagerIcon *icon;
  GtkTreeStore   *store;
  GtkTreeIter     parent_iter;
  GHashTableIter  table_iter;
  GHashTable     *labels;
  HyScanLabel    *object;
  gchar          *id, *tooltip;
  gboolean        active;

  if (priv->extensions[LABEL] == NULL)
    return;

  /*labels = hyscan_object_model_get (priv->label_model);*/
  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    return;

  store = GTK_TREE_STORE (priv->view_model);

  active = hyscan_gtk_model_manager_is_all_toggled (priv->extensions[LABEL], type_id[LABEL]);

  tooltip = g_strdup_printf (_("%s\nQuantity: %u"),_(type_name[LABEL]), g_hash_table_size (labels));

  icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                           gdk_pixbuf_new_from_resource (type_icon[LABEL], NULL),
                                           tooltip);
  g_free (tooltip);

  /* Добавляем новый узел "Группы" в модель */
  ADD_NODE_IN_STORE (&parent_iter, NULL, NULL, _(type_name[LABEL]),
                     _(type_desc[LABEL]), _(author), LABEL, active, 0);

  g_hash_table_iter_init (&table_iter, labels);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      GtkTreeIter  child_iter;
      gboolean     toggled;
      GDateTime   *time;
      gchar       *creation_time, *modification_time;

      if (object == NULL)
        continue;

      toggled  = active;

      if (!active)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[LABEL], id);
          toggled = (ext != NULL) ? ext->active : FALSE;
        }

      time = g_date_time_new_from_unix_local (object->ctime);
      creation_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      time = g_date_time_new_from_unix_local (object->mtime);
      modification_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      if (IS_NOT_EMPTY(object->description))
        {
          gchar *tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     object->name,
                                     tmp,
                                     creation_time,
                                     modification_time);
          g_free (tmp);
        }
      else
        {
          tooltip = g_strdup_printf (_("%s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     object->name,
                                     creation_time,
                                     modification_time);
        }

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)object->icon_data),
                            tooltip);
      g_free (tooltip);
      /* Добавляем новый узел c названием группы в модель */
      ADD_NODE_IN_STORE (&child_iter, &parent_iter, id, object->name, object->description,
                         object->operator_name, LABEL, toggled, object->label);
      /* Атрибуты группы. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY(object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY(object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY(creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY(modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);
    }
  hyscan_gtk_mark_manager_icon_free (icon);

  g_hash_table_destroy (labels);
}

/* Функция создаёт и заполняет узел "Гео-метки" в древовидной модели с группировкой по типам. */
static void
hyscan_gtk_model_manager_refresh_geo_marks_by_types (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanGtkMarkManagerIcon *icon;
  GtkTreeIter     parent_iter;
  GtkTreeStore   *store;
  GHashTable     *geo_marks, *labels;
  GHashTableIter  table_iter;
  HyScanMarkGeo  *object;
  guint8          counter;
  gchar          *id, *tooltip;
  gboolean        active;

  if (priv->extensions[GEO_MARK] == NULL)
    return;

  /*geo_marks = hyscan_object_model_get (priv->geo_mark_model);*/
  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                           HYSCAN_TYPE_MARK_GEO);

  if (geo_marks == NULL)
    return;

  /*labels = hyscan_object_model_get (priv->label_model);*/
  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (geo_marks);
      return;
    }

  store   = GTK_TREE_STORE (priv->view_model);
  active  = hyscan_gtk_model_manager_is_all_toggled (priv->extensions[GEO_MARK], type_id[GEO_MARK]);
  counter = 0;

  g_hash_table_iter_init (&table_iter, geo_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      if (object == NULL)
        continue;

      counter = MAX (counter, hyscan_gtk_model_manager_count_labels (object->labels));
    }

  tooltip = g_strdup_printf (_("%s\n"
                               "Quantity: %u\n"
                               "Groups: %hu"),
                             _(type_name[GEO_MARK]),
                             g_hash_table_size (geo_marks),
                             counter);
  icon = hyscan_gtk_mark_manager_icon_new (NULL,
                        gdk_pixbuf_new_from_resource (type_icon[GEO_MARK], NULL),
                        tooltip);
  g_free (tooltip);

  /* Добавляем новый узел "Гео-метки" в модель */
  ADD_NODE_IN_STORE (&parent_iter, NULL, type_id[GEO_MARK], _(type_name[GEO_MARK]),
                     _(type_desc[GEO_MARK]), _(author), GEO_MARK, active, 0);

  g_hash_table_iter_init (&table_iter, geo_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      HyScanLabel    *label;
      GtkTreeIter     child_iter;
      GdkPixbuf       *pixbuf;
      GHashTableIter  iter;
      GDateTime      *time;
      gchar          *key, *tmp, *label_name, *creation_time, *modification_time, *position;
      gboolean        toggled;

      if (object == NULL)
        continue;

      tmp = label_name = NULL;
      toggled = active;

      if (!active)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[GEO_MARK], id);
          toggled = (ext != NULL) ? ext->active : FALSE;
        }

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (object->labels != label->label)
            continue;

          tmp = label->icon_data;
          label_name = label->name;
          break;
        }
      /* Если нет иконки группы, используем иконку из ресурсов. */
      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[GEO_MARK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)tmp);


      time = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      creation_time  = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      time = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      modification_time  = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      position = g_strdup_printf ("%.6f° %.6f° (WGS 84)", object->center.lat, object->center.lon);

      if (IS_NOT_EMPTY(object->description))
        {
          tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n"
                                       "%s"),
                                     _(type_name[GEO_MARK]),
                                     tmp,
                                     creation_time,
                                     modification_time,
                                     position);
          g_free (tmp);
        }
      else
        {
          tooltip = g_strdup_printf (_("%s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n"
                                       "%s"),
                                     _(type_name[GEO_MARK]),
                                     creation_time,
                                     modification_time,
                                     position);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (icon, pixbuf, tooltip);
      g_free (tooltip);

      /* Добавляем новый узел c названием гео-метки в модель */
      ADD_NODE_IN_STORE (&child_iter, &parent_iter, id, object->name, object->description,
                         object->operator_name, GEO_MARK, toggled, object->labels);
      /* Атрибуты гео-метки. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                                               gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                                               _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                                               gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                                               _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                                               gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                                               _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                                               gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                                               _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                                               gdk_pixbuf_new_from_resource (attr_icon[LOCATION], NULL),
                                               _("Location"));

      if (IS_NOT_EMPTY (position))
        ADD_ATTRIBUTE_IN_STORE (position);

      g_free (position);

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (!(object->labels & label->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     label->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, label),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      label),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         label));

          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }

      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);
    }
  hyscan_gtk_mark_manager_icon_free (icon);

  g_hash_table_destroy (labels);
  g_hash_table_destroy (geo_marks);
}

/* Функция создаёт и заполняет узел "Гео-метки" в древовидной модели с группировкой по группам. */
static void
hyscan_gtk_model_manager_refresh_geo_marks_by_labels (HyScanGtkModelManager *self,
                                                      GtkTreeIter           *iter,
                                                      HyScanLabel           *label)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GtkTreeStore   *store;
  GHashTableIter  table_iter;
  GHashTable     *geo_marks, *labels;
  HyScanMarkGeo  *object;
  gchar          *id;

  if (label == NULL)
    return;

  geo_marks = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                           HYSCAN_TYPE_MARK_GEO);

  if (geo_marks == NULL)
    return;

  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (geo_marks);
      return;
    }

  store = GTK_TREE_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, geo_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      HyScanGtkMarkManagerIcon *icon;
      GtkTreeIter     child_iter;
      GHashTableIter  iterator;
      GDateTime      *time;
      HyScanLabel    *obj;
      Extension      *ext;
      guint8          counter;
      gchar          *key, *position, *tooltip, *creation_time, *modification_time;
      gboolean        toggled;

      if (object == NULL)
        continue;

      if (!(object->labels & label->label))
        continue;

      ext     = g_hash_table_lookup (priv->extensions[GEO_MARK], id);
      toggled = (ext != NULL) ? ext->active : FALSE;

      time = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      creation_time = g_date_time_format (time, date_time_stamp),
      g_date_time_unref (time);

      time = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      modification_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      position = g_strdup_printf ("%.6f° %.6f° (WGS 84)", object->center.lat, object->center.lon);

      if (IS_NOT_EMPTY(object->description))
        {
          gchar *tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n"
                                       "%s"),
                                     _(type_name[GEO_MARK]),
                                     tmp,
                                     creation_time,
                                     modification_time,
                                     position);
          g_free (tmp);
        }
      else
        {
          tooltip = g_strdup_printf (_("%s\n"
                                       "Created: %s\n"
                                       "Modified: %s\n"
                                       "%s"),
                                     _(type_name[GEO_MARK]),
                                     creation_time,
                                     modification_time,
                                     position);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL,
                            gdk_pixbuf_new_from_resource (type_icon[GEO_MARK], NULL),
                            tooltip);
      g_free (tooltip);
      /* Добавляем гео-метку в модель. */
      ADD_NODE_IN_STORE (&child_iter, iter, id, object->name, object->description,
                         object->operator_name, GEO_MARK, toggled, object->labels);

      /* Атрибуты акустической метки. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[LOCATION], NULL),
                            _("Location"));

      if (IS_NOT_EMPTY (position))
        ADD_ATTRIBUTE_IN_STORE (position);

      g_free (position);

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iterator, labels);
      while (g_hash_table_iter_next (&iterator, (gpointer*)&key, (gpointer*)&obj))
        {
          if (obj == NULL)
            continue;

          if (!(object->labels & obj->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     obj->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, obj),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      obj),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         obj));
          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }

      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);
    }
  g_hash_table_destroy (labels);
  g_hash_table_destroy (geo_marks);
}

/* Функция создаёт и заполняет узел "Акустические метки" в древовидной модели с группировкой по типам. */
static void
hyscan_gtk_model_manager_refresh_acoustic_marks_by_types (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanGtkMarkManagerIcon *icon;
  HyScanMarkLocation *location;
  GtkTreeStore   *store;
  GtkTreeIter     parent_iter;
  GHashTableIter  table_iter;
  GHashTable     *acoustic_marks, *labels;
  guint8          counter;
  gchar          *id, *tooltip;
  gboolean        active;

  if (priv->extensions[ACOUSTIC_MARK] == NULL)
    return;

  acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model);

  if (acoustic_marks == NULL)
    return;

  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (acoustic_marks);
      return;
    }

  store   = GTK_TREE_STORE (priv->view_model);
  active  = hyscan_gtk_model_manager_is_all_toggled (priv->extensions[ACOUSTIC_MARK], type_id[ACOUSTIC_MARK]);
  counter = 0;

  g_hash_table_iter_init (&table_iter, acoustic_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
    {
      if (location == NULL)
        continue;

      counter = MAX (counter, hyscan_gtk_model_manager_count_labels (location->mark->labels));
    }

  tooltip = g_strdup_printf (_("%s\n"
                               "Quantity: %u\n"
                               "Groups: %hu"),
                             _(type_name[GEO_MARK]),
                             g_hash_table_size (acoustic_marks),
                             counter);

  icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                           gdk_pixbuf_new_from_resource (type_icon[ACOUSTIC_MARK], NULL),
                                           tooltip);
  g_free (tooltip);

  /* Добавляем новый узел "Акустические метки" в модель. */
  ADD_NODE_IN_STORE (&parent_iter, NULL, type_id[ACOUSTIC_MARK], _(type_name[ACOUSTIC_MARK]),
                     _(type_desc[ACOUSTIC_MARK]), _(author), ACOUSTIC_MARK, active, 0);

  g_hash_table_iter_init (&table_iter, acoustic_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
    {
      GtkTreeIter          child_iter;
      GdkPixbuf           *pixbuf;
      GHashTableIter       iter;
      GDateTime           *time;
      HyScanMarkWaterfall *object = location->mark;
      HyScanLabel         *label;
      gchar               *key,
                          *tmp,
                          *label_name,
                          *creation_time,
                          *modification_time,
                          *position,
                          *board,
                          *depth,
                          *width,
                          *slant_range;
      gboolean             toggled;

      if (object == NULL)
        continue;

      tmp = label_name = NULL;
      toggled = active;

      if (!active)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[ACOUSTIC_MARK], id);
          toggled = (ext != NULL) ? ext->active : FALSE;
        }

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (object->labels != label->label)
            continue;

          tmp = label->icon_data;
          label_name = label->name;
          break;
        }

      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[ACOUSTIC_MARK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)tmp);

      time = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      creation_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      time = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      modification_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      position = g_strdup_printf ("%.6f° %.6f° (WGS 84)", location->mark_geo.lat, location->mark_geo.lon);

      board = g_strdup (attr_icon[BOARD]);
      switch (location->direction)
        {
        case HYSCAN_MARK_LOCATION_PORT:
          {
            key = g_strdup (_("Port"));
            g_free (board);
            board = g_strdup (attr_icon[PORT]);
          }
          break;

        case HYSCAN_MARK_LOCATION_STARBOARD:
          {
            key = g_strdup (_("Starboard"));
            g_free (board);
            board = g_strdup (attr_icon[STARBOARD]);
          }
          break;

        case HYSCAN_MARK_LOCATION_BOTTOM:
          {
            key = g_strdup (_("Bottom"));
            g_free (board);
            board = g_strdup (attr_icon[BOTTOM]);
          }
          break;

         default:
           {
             key = g_strdup (_(unknown));
           }
           break;
         }

      depth = g_strdup_printf (_(distance_stamp), location->depth);

      if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
        {
          if (IS_NOT_EMPTY(object->description))
            {
              tmp = g_strdup (object->description);

              if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
                {
                  gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
                  g_free (tmp);
                  tmp = g_strconcat (str, "...", (gchar*)NULL);
                  g_free (str);
                }

              tooltip = g_strdup_printf (_("%s\n"
                                           "Note: %s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         tmp,
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth);
              g_free (tmp);
            }
          else
            {
              tooltip = g_strdup_printf (_("%s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth);
            }
        }
      else
        {
          width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width);
          /* Для левого борта. */
          if (location->direction == HYSCAN_MARK_LOCATION_PORT)
            {
              /* Поэтому значения отрицательные, и start и end меняются местами. */
              gdouble across_start = -location->mark->width - location->across;
              gdouble across_end   =  location->mark->width - location->across;

              if (across_start < 0 && across_end > 0)
                width = g_strdup_printf (_(distance_stamp), -across_start);
            }
          /* Для правого борта. */
          else
            {
              gdouble across_start = location->across - location->mark->width;
              gdouble across_end   = location->across + location->mark->width;

              if (across_start < 0 && across_end > 0)
                width = g_strdup_printf (_(distance_stamp), across_end);
            }

          slant_range = g_strdup_printf (_(distance_stamp), location->across);

          if (IS_NOT_EMPTY(object->description))
            {
              tmp = g_strdup (object->description);

              if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
                {
                  gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
                  g_free (tmp);
                  tmp = g_strconcat (str, "...", (gchar*)NULL);
                  g_free (str);
                }

              tooltip = g_strdup_printf (_("%s\n"
                                           "Note: %s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"
                                           "Width: %s\n"
                                           "Slant range: %s"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         tmp,
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth,
                                         width,
                                         slant_range);
              g_free (tmp);
            }
          else
            {
              tooltip = g_strdup_printf (_("%s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"
                                           "Width: %s\n"
                                           "Slant range: %s"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth,
                                         width,
                                         slant_range);
            }
        }

      counter = hyscan_gtk_model_manager_count_labels (location->mark->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (icon, pixbuf, tooltip);
      g_free (tooltip);
      /* Добавляем новый узел c названием акустической метки в модель */
      ADD_NODE_IN_STORE (&child_iter, &parent_iter, id, object->name, object->description,
                         object->operator_name, ACOUSTIC_MARK, toggled, object->labels);
      /* Атрибуты акустической метки. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[LOCATION], NULL),
                            _("Location"));

      if (IS_NOT_EMPTY (position))
        ADD_ATTRIBUTE_IN_STORE (position);
      g_free (position);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL),
                             _("Track name"));

      if (IS_NOT_EMPTY (location->track_name))
        ADD_ATTRIBUTE_IN_STORE (location->track_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (board, NULL),
                             _("Board"));
      g_free (board);

      if (IS_NOT_EMPTY (key))
        ADD_ATTRIBUTE_IN_STORE (key);

      g_free (key);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DEPTH], NULL),
                             _("Depth"));

      if (IS_NOT_EMPTY (depth))
        ADD_ATTRIBUTE_IN_STORE (depth);

      g_free (depth);

      if (location->direction != HYSCAN_MARK_LOCATION_BOTTOM)
        {
          icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[WIDTH], NULL),
                            _("Width"));

          if (IS_NOT_EMPTY (width))
            ADD_ATTRIBUTE_IN_STORE (width);

          g_free (width);

          icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[SLANT_RANGE], NULL),
                            _("Slant range"));

          if (IS_NOT_EMPTY (slant_range))
            ADD_ATTRIBUTE_IN_STORE (slant_range);

          g_free (slant_range);
        }

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (!(object->labels & label->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     label->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, label),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      label),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         label));

          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }

      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);
    }
  hyscan_gtk_mark_manager_icon_free (icon);

  g_hash_table_destroy (labels);
  g_hash_table_destroy (acoustic_marks);
}

/* Функция создаёт и заполняет узел "Акустические метки" в древовидной модели с группировкой по группам */
static void
hyscan_gtk_model_manager_refresh_acoustic_marks_by_labels (HyScanGtkModelManager *self,
                                                           GtkTreeIter           *iter,
                                                           HyScanLabel           *label)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanMarkLocation *location;
  GtkTreeStore   *store;
  GHashTableIter  table_iter;
  GHashTable     *acoustic_marks, *labels;
  gchar          *id;

  if (label == NULL)
    return;

  acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model);

  if (acoustic_marks == NULL)
    return;

  /*labels = hyscan_object_model_get (priv->label_model);*/
  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (acoustic_marks);
      return;
    }

  store = GTK_TREE_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, acoustic_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
    {
      HyScanGtkMarkManagerIcon *icon;
      HyScanMarkWaterfall *object;
      GtkTreeIter     child_iter;
      GDateTime      *time;
      GHashTableIter  iterator;
      Extension      *ext;
      HyScanLabel    *obj;
      guint8    counter;
      gchar    *key,
               *tooltip,
               *creation_time,
               *modification_time,
               *position,
               *board,
               *depth,
               *width,
               *slant_range;
      gboolean  toggled;

      if (location == NULL)
        continue;

      object = location->mark;

      if (!(object->labels & label->label))
        continue;

      time = g_date_time_new_from_unix_local (object->ctime / G_TIME_SPAN_SECOND);
      creation_time = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      time = g_date_time_new_from_unix_local (object->mtime / G_TIME_SPAN_SECOND);
      modification_time  = g_date_time_format (time, date_time_stamp);
      g_date_time_unref (time);

      position = g_strdup_printf ("%.6f° %.6f° (WGS 84)", location->mark_geo.lat, location->mark_geo.lon);

      board = g_strdup (attr_icon[BOARD]);
      switch (location->direction)
        {
        case HYSCAN_MARK_LOCATION_PORT:
          {
            key = g_strdup (_("Port"));
            g_free (board);
            board = g_strdup (attr_icon[PORT]);
          }
          break;

        case HYSCAN_MARK_LOCATION_STARBOARD:
          {
            key = g_strdup (_("Starboard"));
            g_free (board);
            board = g_strdup (attr_icon[STARBOARD]);
          }
          break;

        case HYSCAN_MARK_LOCATION_BOTTOM:
          {
            key = g_strdup (_("Bottom"));
            g_free (board);
            board = g_strdup (attr_icon[BOTTOM]);
          }
          break;

        default:
          {
            key = g_strdup (_(unknown));
          }
          break;
        }

      depth = g_strdup_printf (_(distance_stamp), location->depth);

      if (location->direction == HYSCAN_MARK_LOCATION_BOTTOM)
        {
          if (IS_NOT_EMPTY(object->description))
            {
              gchar *tmp = g_strdup (object->description);

              if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
                {
                  gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
                  g_free (tmp);
                  tmp = g_strconcat (str, "...", (gchar*)NULL);
                  g_free (str);
                }

              tooltip = g_strdup_printf (_("%s\n"
                                           "Note: %s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         tmp,
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth);
              g_free (tmp);
            }
          else
            {
              tooltip = g_strdup_printf (_("%s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth);
            }
        }
      else
        {
          width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width);
          /* Для левого борта. */
          if (location->direction == HYSCAN_MARK_LOCATION_PORT)
            {
              /* Поэтому значения отрицательные, и start и end меняются местами. */
              gdouble across_start = -location->mark->width - location->across;
              gdouble across_end   =  location->mark->width - location->across;

              if (across_start < 0 && across_end > 0)
                width = g_strdup_printf (_(distance_stamp), -across_start);
            }
          /* Для правого борта. */
          else
            {
              gdouble across_start = location->across - location->mark->width;
              gdouble across_end   = location->across + location->mark->width;

              if (across_start < 0 && across_end > 0)
                width = g_strdup_printf (_(distance_stamp), across_end);
            }

          slant_range = g_strdup_printf (_(distance_stamp), location->across);

          if (IS_NOT_EMPTY(object->description))
            {
              gchar *tmp = g_strdup (object->description);

              if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
                {
                  gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
                  g_free (tmp);
                  tmp = g_strconcat (str, "...", (gchar*)NULL);
                  g_free (str);
                }

              tooltip = g_strdup_printf (_("%s\n"
                                           "Note: %s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"
                                           "Width: %s\n"
                                           "Slant range: %s"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         tmp,
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth,
                                         width,
                                         slant_range);
              g_free (tmp);
            }
          else
            {
              tooltip = g_strdup_printf (_("%s\n"
                                           "Created: %s\n"
                                           "Modified: %s\n"
                                           "%s\n"
                                           "Track: %s\n"
                                           "Board: %s\n"
                                           "Depth: %s\n"
                                           "Width: %s\n"
                                           "Slant range: %s"),
                                         _(type_name[ACOUSTIC_MARK]),
                                         creation_time,
                                         modification_time,
                                         position,
                                         location->track_name,
                                         key,
                                         depth,
                                         width,
                                         slant_range);
            }
        }

      counter = hyscan_gtk_model_manager_count_labels (location->mark->labels);

      if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                               gdk_pixbuf_new_from_resource (type_icon[ACOUSTIC_MARK], NULL),
                                               tooltip);
      g_free (tooltip);

      ext = g_hash_table_lookup (priv->extensions[ACOUSTIC_MARK], id);
      toggled = (ext != NULL) ? ext->active : FALSE;

      /* Добавляем акустическую метку в модель. */
      ADD_NODE_IN_STORE (&child_iter, iter, id, object->name, object->description,
                         object->operator_name, ACOUSTIC_MARK, toggled, object->labels);
      /* Атрибуты акустической метки. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[LOCATION], NULL),
                            _("Location"));

      if (IS_NOT_EMPTY (position))
        ADD_ATTRIBUTE_IN_STORE (position);

      g_free (position);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL),
                            _("Track name"));

      if (IS_NOT_EMPTY (location->track_name))
        ADD_ATTRIBUTE_IN_STORE (location->track_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (board, NULL),
                            _("Board"));
      g_free (board);

      if (IS_NOT_EMPTY (key))
        ADD_ATTRIBUTE_IN_STORE (key);

      g_free (key);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DEPTH], NULL),
                            _("Depth"));
      ADD_ATTRIBUTE_IN_STORE (depth);
      g_free (depth);

      if (location->direction != HYSCAN_MARK_LOCATION_BOTTOM)
        {
          icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[WIDTH], NULL),
                            _("Width"));

          if (IS_NOT_EMPTY (width))
            ADD_ATTRIBUTE_IN_STORE (width);

          g_free (width);

          icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[SLANT_RANGE], NULL),
                            _("Slant range"));

          if (IS_NOT_EMPTY (slant_range))
            ADD_ATTRIBUTE_IN_STORE (slant_range);

          g_free (slant_range);
        }

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iterator, labels);
      while (g_hash_table_iter_next (&iterator, (gpointer*)&key, (gpointer*)&obj))
        {
          if (obj == NULL)
            continue;

          if (!(object->labels & obj->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     obj->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, obj),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      obj),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         obj));
          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }


      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);

      hyscan_gtk_mark_manager_icon_free (icon);
    }
  g_hash_table_destroy (labels);
  g_hash_table_destroy (acoustic_marks);
}

/* Функция создаёт и заполняет узел "Галсы" в древовидной модели с группировкой по типам. */
static void
hyscan_gtk_model_manager_refresh_tracks_by_types (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanGtkMarkManagerIcon *icon;
  GtkTreeStore    *store;
  GtkTreeIter      parent_iter;
  GHashTableIter   table_iter;
  GHashTable      *tracks, *labels;
  HyScanTrackInfo *object;
  guint8    counter;
  gchar    *id, *tooltip;
  gboolean  active;

  if (priv->extensions[TRACK] == NULL)
    return;

  tracks = hyscan_db_info_get_tracks (priv->track_model);

  if (tracks == NULL)
    return;

  /*labels = hyscan_object_model_get (priv->label_model);*/
  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (tracks);
      return;
    }

  store   = GTK_TREE_STORE (priv->view_model);
  active  = hyscan_gtk_model_manager_is_all_toggled (priv->extensions[TRACK], type_id[TRACK]);
  counter = 0;

  g_hash_table_iter_init (&table_iter, tracks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      if (object == NULL)
        continue;

      counter = MAX (counter, hyscan_gtk_model_manager_count_labels (object->labels));
    }

  tooltip = g_strdup_printf (_("%s\n"
                               "Quantity: %u\n"
                               "Groups: %hu"),
                             _(type_name[TRACK]),
                             g_hash_table_size (tracks),
                             counter);
  icon = hyscan_gtk_mark_manager_icon_new (NULL,
                        gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL),
                        tooltip);
  g_free (tooltip);

  /* Добавляем новый узел "Галсы" в модель. */
  ADD_NODE_IN_STORE (&parent_iter, NULL, type_id[TRACK], _(type_name[TRACK]),
                     _(type_desc[TRACK]), _(author), TRACK, active, 0);

  g_hash_table_iter_init (&table_iter, tracks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      GtkTreeIter     child_iter;
      GdkPixbuf      *pixbuf;
      GHashTableIter  iter;
      HyScanLabel    *label;
      gchar          *key, *tmp, *label_name, *creation_time, *modification_time;
      gboolean        toggled;

      if (object == NULL)
        continue;

      tmp = label_name = NULL;
      toggled = active;

      if (!active)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[TRACK], id);
          toggled = (ext != NULL) ? ext->active : FALSE;
        }

      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (object->labels != label->label)
            continue;

          tmp = label->icon_data;
          label_name = label->name;
          break;
        }

      pixbuf = (tmp == NULL) ? gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL) :
               hyscan_gtk_model_manager_get_icon_from_base64 ((const gchar*)tmp);

      if (object->ctime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->ctime);
          creation_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          creation_time = g_strdup ("");
        }

      if (object->mtime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->mtime);
          modification_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          modification_time = g_strdup ("");
        }

      if (IS_NOT_EMPTY(object->description))
        {
          tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     _(type_name[TRACK]),
                                     tmp,
                                     creation_time,
                                     modification_time);
          g_free (tmp);
        }
      else
        {
          tooltip = g_strdup_printf (_("%s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     _(type_name[TRACK]),
                                     creation_time,
                                     modification_time);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter == 1)
        tooltip = g_strdup_printf (_("%s\nGroup %s"), tooltip, label_name);
      else if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (icon, pixbuf, tooltip);
      g_free (tooltip);

      /* Добавляем новый узел c названием галса в модель */
      ADD_NODE_IN_STORE (&child_iter, &parent_iter, id, object->name, object->description,
                         object->operator_name, TRACK, toggled, object->labels);
      /* Атрибуты галса. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iter, labels);
      while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (!(object->labels & label->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     label->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, label),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      label),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         label));
          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)label->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }

      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);
    }
  hyscan_gtk_mark_manager_icon_free (icon);

  g_hash_table_destroy (labels);
  g_hash_table_destroy (tracks);
}

/* Функция создаёт и заполняет узел "Галсы" в древовидной модели с группировкой по группам. */
static void
hyscan_gtk_model_manager_refresh_tracks_by_labels (HyScanGtkModelManager *self,
                                                   GtkTreeIter  *iter,
                                                   HyScanLabel  *label)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GtkTreeStore    *store;
  GHashTable      *tracks, *labels;;
  GHashTableIter   table_iter;
  HyScanTrackInfo *object;
  gchar           *id;

  if (label == NULL)
    return;

  tracks = hyscan_db_info_get_tracks (priv->track_model);

  if (tracks == NULL)
    return;

  /*labels = hyscan_object_model_get (priv->label_model);*/
  /*labels = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                        HYSCAN_TYPE_LABEL);*/
  labels = hyscan_gtk_model_manager_get_all_labels (self);

  if (labels == NULL)
    {
      g_hash_table_destroy (tracks);
      return;
    }

  store = GTK_TREE_STORE (priv->view_model);

  g_hash_table_iter_init (&table_iter, tracks);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
    {
      HyScanGtkMarkManagerIcon *icon;
      GtkTreeIter  child_iter;
      GHashTableIter  iterator;
      Extension   *ext;
      HyScanLabel *obj;
      guint8       counter;
      gchar       *key, *tooltip, *creation_time, *modification_time;
      gboolean     toggled;

      if (object == NULL)
        continue;

      if (!(object->labels & label->label))
        continue;

      ext     = g_hash_table_lookup (priv->extensions[TRACK], id);
      toggled = (ext != NULL) ? ext->active : FALSE;

      if (object->ctime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->ctime);
          creation_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          creation_time = g_strdup ("");
        }

      if (object->mtime != NULL)
        {
          GDateTime *local = g_date_time_to_local (object->mtime);
          modification_time = g_date_time_format (local, date_time_stamp);
          g_date_time_unref (local);
        }
      else
        {
          modification_time = g_strdup ("");
        }

      if (IS_NOT_EMPTY(object->description))
        {
          gchar *tmp = g_strdup (object->description);

          if (DESCRIPTION_MAX_LENGTH < g_utf8_strlen (tmp, -1))
            {
              gchar *str = g_utf8_substring (tmp, 0, DESCRIPTION_MAX_LENGTH);
              g_free (tmp);
              tmp = g_strconcat (str, "...", (gchar*)NULL);
              g_free (str);
            }

          tooltip = g_strdup_printf (_("%s\n"
                                       "Note: %s\n"
                                       "Created: %s\n"
                                       "Modified: %s"),
                                     _(type_name[TRACK]),
                                     tmp,
                                     creation_time,
                                     modification_time);
          g_free (tmp);
        }
      else
        {
           tooltip = g_strdup_printf (_("%s\n"
                                        "Created: %s\n"
                                        "Modified: %s"),
                                      _(type_name[TRACK]),
                                      creation_time,
                                      modification_time);
        }

      counter = hyscan_gtk_model_manager_count_labels (object->labels);

      if (counter > 1)
        tooltip = g_strdup_printf (_("%s\nGroups: %hu"), tooltip, counter);

      icon = hyscan_gtk_mark_manager_icon_new (NULL,
                            gdk_pixbuf_new_from_resource (type_icon[TRACK], NULL),
                            tooltip);
      g_free (tooltip);
      /* Добавляем галс в модель. */
      ADD_NODE_IN_STORE (&child_iter, iter, id, object->name, object->description,
                         object->operator_name, TRACK, toggled, object->labels);
      /* Атрибуты галса. */
      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[DESCRIPTION], NULL),
                            _("Description"));

      if (IS_NOT_EMPTY (object->description))
        ADD_ATTRIBUTE_IN_STORE (object->description);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[OPERATOR], NULL),
                            _("Operator"));

      if (IS_NOT_EMPTY (object->operator_name))
        ADD_ATTRIBUTE_IN_STORE (object->operator_name);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[CREATION_TIME], NULL),
                            _("Creation time"));

      if (IS_NOT_EMPTY (creation_time))
        ADD_ATTRIBUTE_IN_STORE (creation_time);

      g_free (creation_time);

      icon = hyscan_gtk_mark_manager_icon_new (icon,
                            gdk_pixbuf_new_from_resource (attr_icon[MODIFICATION_TIME], NULL),
                            _("Modification time"));

      if (IS_NOT_EMPTY (modification_time))
        ADD_ATTRIBUTE_IN_STORE (modification_time);

      g_free (modification_time);

      hyscan_gtk_mark_manager_icon_free (icon);
      icon = NULL;
      /* Формируем список групп и иконку с иконками групп. */
      g_hash_table_iter_init (&iterator, labels);
      while (g_hash_table_iter_next (&iterator, (gpointer*)&key, (gpointer*)&obj))
        {
          if (obj == NULL)
            continue;

          if (!(object->labels & obj->label))
            continue;

          tooltip = g_strdup_printf (_("%s\n"
                                       "Acoustic marks: %u\n"
                                       "Geo marks: %u\n"
                                       "Tracks: %u"),
                                     obj->name,
                                     hyscan_gtk_model_manager_has_object_with_label (ACOUSTIC_MARK, obj),
                                     hyscan_gtk_model_manager_has_object_with_label (GEO_MARK,      obj),
                                     hyscan_gtk_model_manager_has_object_with_label (TRACK,         obj));
          if (icon == NULL)
            {
              icon = hyscan_gtk_mark_manager_icon_new (NULL,
                                    hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                                    tooltip);
            }
          else
            {
              hyscan_gtk_mark_manager_icon_add (icon,
                               hyscan_gtk_model_manager_get_icon_from_base64 ( (const gchar*)obj->icon_data),
                               tooltip);
            }
          g_free (tooltip);
        }

      if (icon != NULL && HAS_MANY_ICONS (icon))
        ADD_ATTRIBUTE_IN_STORE (NULL);

      hyscan_gtk_mark_manager_icon_free (icon);
    }
  g_hash_table_destroy (labels);
  g_hash_table_unref (tracks);
}

/* Очищает модель от содержимого. */
static void
hyscan_gtk_model_manager_clear_view_model (GtkTreeModel *view_model,
                                           gboolean     *flag)
{
  /* Защита от срабатывания срабатывания сигнала "changed"
   * у GtkTreeSelection при очистке GtkTreeModel.
   * */
  *flag = TRUE;

  if (GTK_IS_LIST_STORE (view_model))
    gtk_list_store_clear (GTK_LIST_STORE (view_model));
  else if (GTK_IS_TREE_STORE (view_model))
    gtk_tree_store_clear (GTK_TREE_STORE (view_model));
  else
    g_warning ("Unknown type of store in HyScanMarkManagerView.\n");
  /* Защита от срабатывания срабатывания сигнала "changed"
   * у GtkTreeSelection при очистке GtkTreeModel.
   * */
  *flag = FALSE;
}

/* Инициализирует массив с данными об объектах данными из моделей. */
static gboolean
hyscan_gtk_model_manager_init_extensions (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  gint counter = 0;

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      GHashTable *tmp;

      if (priv->extensions[type] == NULL)
          priv->extensions[type] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                          (GDestroyNotify)hyscan_gtk_model_manager_extension_free);

      tmp = priv->extensions[type];
      priv->extensions[type] = hyscan_gtk_model_manager_get_extensions (self, type);

      if (priv->extensions[type] != NULL)
        {
          if (priv->node[type] == NULL)
            {
              priv->node[type] = hyscan_gtk_model_manager_extension_new (PARENT, FALSE, FALSE);
            }
          else
            {
              Extension *ext = g_hash_table_lookup (tmp, type_id[type]);

              if (ext != NULL)
                priv->node[type] = hyscan_gtk_model_manager_extension_copy (ext);
            }

          g_hash_table_insert (priv->extensions[type], g_strdup (type_id[type]), priv->node[type]);
        }
      counter++;

      if (tmp != NULL)
        g_hash_table_destroy (tmp);
    }

  return (gboolean)counter;
}

/* Возвращает указатель на хэш-таблицу с данными в соответствии
 * с типом запрашиваемых объектов. Когда хэш-таблица больше не нужна,
 * необходимо использовать #g_hash_table_unref ().
 * */
static GHashTable*
hyscan_gtk_model_manager_get_extensions (HyScanGtkModelManager  *self,
                                         ModelManagerObjectType  type)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GHashTable     *table = NULL, *extensions;
  GHashTableIter  iter;
  gpointer        object;
  gchar          *id;

  table = get_objects_from_model[type] (model[type]);

  if (table == NULL)
    return NULL;

  extensions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                      (GDestroyNotify)hyscan_gtk_model_manager_extension_free);

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, &object))
    {
      Extension *ext = g_hash_table_lookup (priv->extensions[type], id);

      ext = (ext == NULL) ?
            hyscan_gtk_model_manager_extension_new (CHILD, FALSE, FALSE) :
            hyscan_gtk_model_manager_extension_copy (ext);
      g_hash_table_insert (extensions, g_strdup (id), ext);
    }

  g_hash_table_destroy (table);
  table = extensions;

  return table;
}

/* Возвращает TRUE, если ВСЕ ОБЪЕКТЫ Extention в хэш-таблице имеют поля active = TRUE.
 * В противном случае, возвращает FALSE.
 * */
static gboolean
hyscan_gtk_model_manager_is_all_toggled (GHashTable  *table,
                                         const gchar *node_id)
{
  GHashTableIter iter;
  Extension *ext;
  guint      total = g_hash_table_size (table), counter = 1;
  gchar     *id;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (IS_EQUAL (id, node_id))
        continue;

      if (ext->active)
        counter++;
    }

  ext = g_hash_table_lookup (table, node_id);

  if (ext != NULL)
    ext->active = counter >= total;

  return ext->active;
}

/* Удаляет галс по идентификатору в базе данных.
 * convert - флаг разрешает преобразование акустических
 * меток, связанных с галсом, в гео-метки.
 * * */
static void
hyscan_gtk_model_manager_delete_track_by_id (HyScanGtkModelManager  *self,
                                             gchar                  *id,
                                             gboolean                convert)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GHashTable         *acoustic_marks;
  GHashTableIter      iter;
  HyScanMarkLocation *location;
  gchar              *key;
  gint32 project_id = hyscan_db_project_open (priv->db, priv->project_name);

  if (project_id <= 0)
    return;
  /* Функция hyscan_db_info_get_tracks возвращает таблицу
   * с объектами HyScanDBTrackInfo ключами в которой являются
   * названия галсов. Поэтому в hyscan_db_track_remove передаём id
   * и для поиска акустических меток связанных с галсом тоже используем id.
   * */
  if (!hyscan_db_track_remove (priv->db, project_id, id))
    goto terminate;

  acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model);

  g_hash_table_iter_init (&iter, acoustic_marks);
  while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&location))
    {
      if (location == NULL)
        continue;

      if (IS_NOT_EQUAL (id, location->track_name))
        continue;

      if (convert)
        {
          /* Конвертируем в акустическую метку в гео-метку. */
          HyScanMarkGeo *geo_mark = hyscan_mark_geo_new ();
          GDateTime     *dt       = g_date_time_new_now_local ();
          gdouble radius = sqrt ( (location->mark->width * location->mark->height) / G_PI_2);

          geo_mark->name          = g_strdup (location->mark->name);
          geo_mark->description   = g_strdup (location->mark->description);
          geo_mark->operator_name = g_strdup (location->mark->operator_name);
          geo_mark->labels = location->mark->labels;
          geo_mark->ctime  = location->mark->ctime;
          geo_mark->mtime  = G_TIME_SPAN_SECOND * g_date_time_to_unix (dt);
          g_date_time_unref (dt);
          /* Размеры гео-метки пересчитываются сохраняя площадь акустической метки. */
          geo_mark->height = geo_mark->width = 2.0 * radius;
          geo_mark->center = location->mark_geo;

          /*hyscan_object_model_add (priv->geo_mark_model, (const HyScanObject*)geo_mark);*/
          hyscan_object_store_add (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                   (const HyScanObject*)geo_mark,
                                   NULL);

          hyscan_mark_geo_free (geo_mark);
        }
      /* Удаляем акустическую метку вместе с галсом с которым она связана. */
      /*hyscan_object_model_remove (priv->acoustic_mark_model, key);*/
      hyscan_object_store_remove (HYSCAN_OBJECT_STORE (priv->acoustic_mark_model),
                                  HYSCAN_TYPE_MARK_WATERFALL,
                                  key);
    }
  g_hash_table_destroy (acoustic_marks);

terminate:
  hyscan_db_close (priv->db, project_id);
}

/* Создаёт новый Extention. Для удаления необходимо
 * использовать hyscan_gtk_model_manager_extension_free ().*/
static Extension*
hyscan_gtk_model_manager_extension_new (ExtensionType type,
                                        gboolean      active,
                                        gboolean      expanded)
{
  Extension *ext = g_new (Extension, 1);
  ext->type      = type;
  ext->active    = active;
  ext->expanded  = expanded;
  return ext;
}

/* Создаёт копию Extention-а. Для удаления необходимо использовать hyscan_gtk_model_manager_extension_free ().*/
static Extension*
hyscan_gtk_model_manager_extension_copy (Extension *ext)
{
  Extension *copy = g_new (Extension, 1);
  copy->type      = ext->type;
  copy->active    = ext->active;
  copy->expanded  = ext->expanded;
  return copy;
}

/* Освобождает ресурсы Extention-а */
static void
hyscan_gtk_model_manager_extension_free (gpointer data)
{
  if (data != NULL)
    {
      Extension *ext = (Extension*)data;
      ext->type      = PARENT;
      ext->active    =
      ext->expanded  = FALSE;
      g_free (ext);
    }
}

/* Функция циклической проверки и обновления модели представления данных. */
static gboolean
hyscan_gtk_model_manager_view_model_loop (gpointer user_data)
{
  HyScanGtkModelManager *self;
  HyScanGtkModelManagerPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MODEL_MANAGER (user_data), TRUE);

  self = HYSCAN_GTK_MODEL_MANAGER (user_data);
  priv = self->priv;

  if (priv->update_model_flag)
    {
      hyscan_gtk_model_manager_update_view_model (self);
      priv->update_model_flag = FALSE;
    }

  return TRUE;
}

/* Преобразует строку в формате BASE64 в картинку. */
static GdkPixbuf*
hyscan_gtk_model_manager_get_icon_from_base64 (const gchar *str)
{
  GdkPixbuf    *pixbuf;
  GInputStream *stream;
  gsize         length;
  guchar       *data;

  data   = g_base64_decode (str, &length);
  stream = g_memory_input_stream_new_from_data ( (const void*)data, (gssize)length, g_free);
  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, NULL);

  g_input_stream_close (stream, NULL, NULL);
  g_free (data);

  return pixbuf;
}

/* Проверяет, есть ли объекты заданного типа принадлежащие группе.
 * Возвращает количество объектов.
 * * */
static guint
hyscan_gtk_model_manager_has_object_with_label (ModelManagerObjectType  type,
                                                HyScanLabel            *label)
{
  GHashTable *table = NULL;
  GHashTableIter iter;
  gpointer object;
  guint counter = 0;
  gchar *id;

  table = get_objects_from_model[type] (model[type]);

  if (table == NULL)
    return 0;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&object))
    {
      guint64 mask = 0;

      if (object == NULL)
        continue;

      switch (type)
        {
        case LABEL:
          mask = ( (HyScanLabel*)object)->label;
          break;

        case GEO_MARK:
          mask = ( (HyScanMarkGeo*)object)->labels;
          break;

        case ACOUSTIC_MARK:
          mask = ( (HyScanMarkLocation*)object)->mark->labels;
          break;

        case TRACK:
          mask = ( (HyScanTrackInfo*)object)->labels;
          break;

        default:
          break;
        }

      if (mask == 0)
        continue;

      if (type == LABEL)
        {
          if (mask != label->label)
            continue;

          counter = 1;
        }
      else
        {
          if (mask & label->label)
            counter++;
        }
    }

  g_hash_table_destroy (table);

  return counter;
}

/* Подсчёт единиц в битовой маске. */
static guint8
hyscan_gtk_model_manager_count_labels (guint64 mask)
{
  mask -= (mask >> 1) & 0x5555555555555555llu;
  mask = ( (mask >> 2) & 0x3333333333333333llu ) + (mask & 0x3333333333333333llu);
  mask = ( ( ( (mask >> 4) + mask) & 0x0F0F0F0F0F0F0F0Fllu) * 0x0101010101010101) >> 56;
  return (guint8)mask;
}

/* Возвращает хэш-таблицу глобальных и пользовательских групп. */
static GHashTable*
hyscan_gtk_model_manager_get_labels (gpointer model)
{
  GHashTable *labels;

  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), NULL);

  /*labels = hyscan_object_model_get ((HyScanObjectModel*)model);*/
  labels = hyscan_object_store_get_all ( (HyScanObjectStore*)model, HYSCAN_TYPE_LABEL);

  if (labels == NULL)
    labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_label_free);

  for (GlobalLabel index = GLOBAL_LABEL_FIRST; index < GLOBAL_LABEL_LAST; index++)
    g_hash_table_insert (labels, g_strdup (global_label_name[index]), hyscan_label_copy (global[index]));

  return labels;
}

/* Обёртка для укзателя на функцию получения таблицы гео-меток. */
static GHashTable*
hyscan_gtk_model_manager_get_geo_marks (gpointer model)
{
  g_return_val_if_fail (HYSCAN_IS_OBJECT_MODEL (model), NULL);
  /*return hyscan_object_model_get ( (HyScanObjectModel*)model);*/
  return hyscan_object_store_get_all ( (HyScanObjectStore*)model, HYSCAN_TYPE_MARK_GEO);
}

/* Обёртка для укзателя на функцию получения таблицы скустических меток. */
static GHashTable*
hyscan_gtk_model_manager_get_acoustic_makrs (gpointer model)
{
  g_return_val_if_fail (HYSCAN_IS_MARK_LOC_MODEL (model), NULL);
  return hyscan_mark_loc_model_get( (HyScanMarkLocModel*)model);
}

/* Обёртка для укзателя на функцию получения таблицы галсов. */
static GHashTable*
hyscan_gtk_model_manager_get_tracks (gpointer model)
{
  if (model == NULL)
    return NULL;

  return hyscan_db_info_get_tracks ( (HyScanDBInfo*)model);
}

/* Присваивает выделенным группам битовую маску групп и обновляет дату и время сделанных изменений. */
static void
hyscan_gtk_model_manager_toggled_labels_set_labels (HyScanGtkModelManager *self,
                                                    guint64                labels,
                                                    guint64                inconsistents,
                                                    gint64                 current_time)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  /*GHashTable     *table = hyscan_object_model_get (priv->label_model);*/
  /*GHashTable     *table = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                                       HYSCAN_TYPE_LABEL);*/
  GHashTable     *table = hyscan_gtk_model_manager_get_all_labels (self);
  GHashTableIter  iter;
  HyScanLabel    *label;
  gchar          *id;

  BLOCK_SIGNAL (priv->signal_labels_changed, priv->label_model);

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&label))
    {
      if (label == NULL)
        continue;

      if (inconsistents & label->label)
        continue;

      if (!(labels & label->label))
        continue;

      /* При группировке по группам разворачиваем родительский узел. */
      if (priv->grouping == BY_LABELS)
        hyscan_gtk_model_manager_expand_item (self, id, TRUE);
    }

  UNBLOCK_SIGNAL (priv->signal_labels_changed, priv->label_model);

  g_hash_table_destroy (table);
}

/* Присваивает выделенным гео-меткам битовую маску групп и обновляет дату и время сделанных изменений. */
static void
hyscan_gtk_model_manager_toggled_geo_marks_set_labels (HyScanGtkModelManager *self,
                                                       guint64                labels,
                                                       guint64                inconsistents,
                                                       gint64                 current_time)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  guint64 old, new;
  gchar** list = hyscan_gtk_model_manager_get_toggled_items (self, GEO_MARK);

  if (list == NULL)
    return;

  for (gint i = 0; list[i] != NULL; i++)
    {
      /*HyScanMarkGeo  *object = (HyScanMarkGeo*)hyscan_object_model_get_by_id (priv->geo_mark_model, list[i]);*/
      HyScanMarkGeo  *object = (HyScanMarkGeo*)hyscan_object_store_get (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                                                        HYSCAN_TYPE_MARK_GEO,
                                                                        list[i]);
      GHashTable     *table;
      GHashTableIter  iter;
      HyScanLabel    *label;
      guint64         tmp;
      gchar          *id;

      if (object == NULL)
        continue;

      /*table = hyscan_object_model_get (priv->label_model);*/
      /*table = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                           HYSCAN_TYPE_LABEL);*/
      table = hyscan_gtk_model_manager_get_all_labels (self);
      tmp   = object->labels;

      g_hash_table_iter_init (&iter, table);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (inconsistents & label->label)
            continue;

          if ( (labels & label->label) == (object->labels & label->label))
            continue;

          old = tmp    &  inconsistents;
          new = labels & ~inconsistents;
          /* Сохраняем промежуточный результат. */
          tmp = old | new;
        }

      if (object->labels != tmp)
        {
          /* Вносим изменения. */
          object->labels = tmp;
          object->mtime  = G_TIME_SPAN_SECOND * current_time;

          BLOCK_SIGNAL (priv->signal_geo_marks_changed, priv->geo_mark_model);
          /* Сохраняем в базе данных. */
          /*hyscan_object_model_modify (priv->geo_mark_model,
                                      list[i],
                                      (const HyScanObject*)object);*/
          hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                      list[i],
                                      (const HyScanObject*)object);

          UNBLOCK_SIGNAL (priv->signal_geo_marks_changed, priv->geo_mark_model);
        }
      hyscan_mark_geo_free (object);
    }
}

/* Присваивает выделенным акустическим меткам битовую маску групп и обновляет дату и время сделанных изменений. */
static void
hyscan_gtk_model_manager_toggled_acoustic_marks_set_labels (HyScanGtkModelManager *self,
                                                            guint64                labels,
                                                            guint64                inconsistents,
                                                            gint64                 current_time)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  guint64 old, new;
  gchar** list = hyscan_gtk_model_manager_get_toggled_items (self, ACOUSTIC_MARK);

  if (list == NULL)
    return;
  for (gint i = 0; list[i] != NULL; i++)
    {
      /*HyScanMarkWaterfall *object = (HyScanMarkWaterfall*)hyscan_object_model_get_by_id (
                                                                 priv->acoustic_mark_model,
                                                                 list[i]);*/
      HyScanMarkWaterfall *object = (HyScanMarkWaterfall*)hyscan_object_store_get (
                                                                 HYSCAN_OBJECT_STORE (priv->acoustic_mark_model),
                                                                 HYSCAN_TYPE_MARK_WATERFALL,
                                                                 list[i]);
      GHashTable     *table;
      GHashTableIter  iter;
      HyScanLabel    *label;
      guint64         tmp;
      gchar          *id;

      if (object == NULL)
        continue;

      /*table = hyscan_object_model_get (priv->label_model);*/
      /*table = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                           HYSCAN_TYPE_LABEL);*/
      table = hyscan_gtk_model_manager_get_all_labels (self);
      tmp   = object->labels;

      g_hash_table_iter_init (&iter, table);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (inconsistents & label->label)
            continue;

          if ( (labels & label->label) == (object->labels & label->label))
            continue;

          old = tmp    &  inconsistents;
          new = labels & ~inconsistents;
          /* Сохраняем промежуточный результат. */
          tmp = old | new;
        }
      if (object->labels != tmp)
        {
          /* Вносим изменения. */
          object->labels = tmp;
          object->mtime  = G_TIME_SPAN_SECOND * current_time;
          BLOCK_SIGNAL (priv->signal_acoustic_marks_changed, priv->acoustic_mark_model);

          /* Сохраняем в базе данных. */
          /*hyscan_object_model_modify (priv->acoustic_mark_model,
                                      list[i],
                                      (const HyScanObject*)object);*/
          hyscan_object_store_modify (HYSCAN_OBJECT_STORE (priv->acoustic_mark_model),
                                      list[i],
                                      (const HyScanObject*)object);

          UNBLOCK_SIGNAL (priv->signal_acoustic_marks_changed, priv->acoustic_mark_model);
        }
      hyscan_mark_waterfall_free (object);
    }
}

/* Присваивает выделенным галсам битовую маску групп и обновляет дату и время сделанных изменений. */
static void
hyscan_gtk_model_manager_toggled_tracks_set_labels (HyScanGtkModelManager *self,
                                                    guint64                labels,
                                                    guint64                inconsistents,
                                                    gint64                 current_time)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GDateTime *now_local;
  gint32    project_id = hyscan_db_project_open (priv->db, priv->project_name);
  guint64   old, new;
  gchar   **list;

  if (project_id <= 0)
    return;

  list = hyscan_gtk_model_manager_get_toggled_items (self, TRACK);

  if (list == NULL)
    {
      hyscan_db_close (priv->db, project_id);
      return;
    }

  now_local = g_date_time_new_from_unix_local (current_time);

  for (gint i = 0; list[i] != NULL; i++)
    {
      HyScanTrackInfo *object;
      GHashTable      *table;
      GHashTableIter   iter;
      HyScanLabel     *label;
      guint64          tmp;
      gchar           *id;

      object = hyscan_db_info_get_track_info (priv->db, project_id, list[i]);

      if (object == NULL)
        continue;

      /*table = hyscan_object_model_get (priv->label_model);*/
      /*table = hyscan_object_store_get_all (HYSCAN_OBJECT_STORE (priv->label_model),
                                           HYSCAN_TYPE_LABEL);*/
      table = hyscan_gtk_model_manager_get_all_labels (self);
      tmp   = object->labels;

      g_hash_table_iter_init (&iter, table);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&label))
        {
          if (label == NULL)
            continue;

          if (inconsistents & label->label)
            continue;

          if ( (labels & label->label) == (object->labels & label->label))
            continue;

          old = tmp    &  inconsistents;
          new = labels & ~inconsistents;
          /* Сохраняем промежуточный результат. */
          tmp = old | new;
        }

      if (object->labels != tmp)
        {
          /* Вносим изменения. */
          object->labels = tmp;
          object->mtime  = g_date_time_ref (now_local);

          BLOCK_SIGNAL (priv->signal_tracks_changed, priv->track_model);
          /* Сохраняем в базе данных. */
          hyscan_db_info_modify_track_info (priv->track_model, object);

          UNBLOCK_SIGNAL (priv->signal_tracks_changed, priv->track_model);
        }
      hyscan_db_info_track_info_free (object);
    }
  hyscan_db_close (priv->db, project_id);
  g_date_time_unref (now_local);
}

/**
 * hyscan_gtk_model_manager_new:
 * @project_name: название проекта
 * @db: указатель на базу данных
 * @cache: указатель на кэш
 * @export_folder: папка для экспорта
 *
 * Returns: cоздаёт новый объект #HyScanGtkModelManager.
 * Когда Менеджер Mоделей больше не нужен,
 * необходимо использовать #g_object_unref () или g_clear_object ().
 */
HyScanGtkModelManager*
hyscan_gtk_model_manager_new (const gchar *project_name,
                              HyScanDB    *db,
                              HyScanCache *cache,
                              gchar       *export_folder)
{
  return g_object_new (HYSCAN_TYPE_GTK_MODEL_MANAGER,
                       "project_name",  project_name,
                       "db",            db,
                       "cache",         cache,
                       "export_folder", export_folder,
                       NULL);
}

/**
 * hyscan_gtk_model_manager_get_units:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель единиц измерения. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanUnits *
hyscan_gtk_model_manager_get_units (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->units);
}

/**
 * hyscan_gtk_model_manager_get_track_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель галсов. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanDBInfo*
hyscan_gtk_model_manager_get_track_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->track_model);
}

/**
 * hyscan_gtk_model_manager_get_acoustic_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель акустических меток. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_gtk_model_manager_get_acoustic_mark_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_mark_model);
}

/**
 * hyscan_gtk_model_manager_get_geo_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель гео-меток. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_gtk_model_manager_get_geo_mark_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->geo_mark_model);
}

/**
 * hyscan_gtk_model_manager_get_planner_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель объектов планирования. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanPlannerModel*
hyscan_gtk_model_manager_get_planner_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->planner_model);
}

/**
 * hyscan_gtk_model_manager_get_label_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель групп. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_gtk_model_manager_get_label_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->label_model);
}

/**
 * hyscan_gtk_model_manager_get_acoustic_mark_loc_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель акустических меток с координатами. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanMarkLocModel*
hyscan_gtk_model_manager_get_acoustic_mark_loc_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_loc_model);
}

/**
 * hyscan_gtk_model_manager_get_signal_title:
 * @self: указатель на Менеджер Моделей
 * signal: сигнал Менеджера Моделей
 *
 * Returns: название сигнала Менеджера Моделей
 */
const gchar*
hyscan_gtk_model_manager_get_signal_title (HyScanGtkModelManager *self,
                                           ModelManagerSignal     signal)
{
  return signals[signal];
}

/**
 * hyscan_gtk_model_manager_get_db:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на Базу Данных. Когда БД больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanDB*
hyscan_gtk_model_manager_get_db (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->db);
}

/**
 * hyscan_gtk_model_manager_set_project_name:
 * @self: указатель на Менеджер Моделей
 * @project_name: название проекта
 *
 * Устанавливает название проекта и отправляет сигнал "notify:project-name".
 */
void
hyscan_gtk_model_manager_set_project_name (HyScanGtkModelManager *self,
                                           const gchar           *project_name)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  HyScanProjectInfo *project_info;
  gint64 project_creation_time;

  if (IS_EQUAL (priv->project_name, project_name))
    return;

  g_free (priv->project_name);

  priv->project_name = g_strdup (project_name);

  if (priv->track_model != NULL)
    hyscan_db_info_set_project (priv->track_model, priv->project_name);

  if (priv->acoustic_loc_model != NULL)
    hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);

  if (priv->acoustic_mark_model != NULL)
    hyscan_object_model_set_project (priv->acoustic_mark_model, priv->db, priv->project_name);

  if (priv->geo_mark_model)
    hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);

  if (priv->planner_model)
    hyscan_object_model_set_project (HYSCAN_OBJECT_MODEL (priv->planner_model), priv->db, priv->project_name);

  /* В начале устанавливаем время создания проекта для глобальных групп. */
  project_info = hyscan_db_info_get_project_info (priv->db, priv->project_name);
  project_creation_time = g_date_time_to_unix (project_info->ctime);
  hyscan_db_info_project_info_free (project_info);

  for (GlobalLabel index = GLOBAL_LABEL_FIRST; index < GLOBAL_LABEL_LAST; index++)
    {
      hyscan_label_set_ctime (global[index], project_creation_time);
      hyscan_label_set_mtime (global[index], project_creation_time);
    }

  /* Затем устанавливаем проект для пользовательских групп. */
  if (priv->label_model != NULL)
    hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);

  g_object_notify_by_pspec (G_OBJECT (self), notify);
}

/**
 * hyscan_gtk_model_manager_get_project_name:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: название проекта
 */
const gchar*
hyscan_gtk_model_manager_get_project_name (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->project_name;
}

/**
 * hyscan_gtk_model_manager_get_export_folder:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: директорию для экспорта
 */
const gchar*
hyscan_gtk_model_manager_get_export_folder (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->export_folder;
}

/**
 * hyscan_gtk_model_manager_get_cache:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на кэш. Когда кэш больше не нужен,
 * необходимо использовать #g_object_unref ().
 */
HyScanCache*
hyscan_gtk_model_manager_get_cache (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->cache);
}

/**
 * hyscan_gtk_model_manager_get_all_tracks_id:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: NULL-терминированный список идентификаторов всех галсов.
 * Когда список больше не нужен, необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_gtk_model_manager_get_all_tracks_id (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  GHashTable  *tracks;
  guint        count;
  gchar      **list = NULL;

  tracks = hyscan_db_info_get_tracks (priv->track_model);
  count  = g_hash_table_size (tracks);

  if (count > 0)
    {
      HyScanTrackInfo *object;
      GHashTableIter   table_iter;
      gchar           *id;
      gint             i = 0;

      list = g_new0 (gchar*, count + 1);
      g_hash_table_iter_init (&table_iter, tracks);
      while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
        list[i++] = g_strdup (id);
    }

  g_hash_table_destroy (tracks);

  return list;
}

/**
 * hyscan_gtk_model_manager_set_grouping:
 * @self: указатель на Менеджер Моделей
 * @grouping: тип группировки
 *
 * Устанавливает тип группировки и отправляет сигнал об изменении типа группировки
 */
void
hyscan_gtk_model_manager_set_grouping (HyScanGtkModelManager *self,
                                       ModelManagerGrouping   grouping)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  if (priv->grouping == grouping)
    return;

  priv->grouping = grouping;
  /* Устанавливаем флаг для обновления модели представления данных. */
  priv->update_model_flag = TRUE;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GROUPING_CHANGED], 0);
}

/**
 * hyscan_gtk_model_manager_get_grouping:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: действительный тип группировки
 */
ModelManagerGrouping
hyscan_gtk_model_manager_get_grouping (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->grouping;
}

/**
 * hyscan_gtk_model_manager_get_view_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: модель представления данных для #HyScanMarkManager.
 * Когда модель больше не нужна, необходимо использовать #g_object_unref ().
 */
GtkTreeModel*
hyscan_gtk_model_manager_get_view_model (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->view_model);
}

/**
 * hyscan_gtk_model_manager_set_selected_item:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор выделенного объекта
 *
 * Устанавливает выделенную строку.
 */
void
hyscan_gtk_model_manager_set_selected_item (HyScanGtkModelManager *self,
                                            gchar                 *id)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  if (priv->clear_model_flag)  /* Защита против срабатывания сигнала "changed" */
    return;                    /* у GtkTreeSelection при очистке GtkTreeModel.   */

  if (priv->selected_item_id != NULL)
    g_free (priv->selected_item_id);

  priv->selected_item_id = g_strdup (id);

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_SELECTED], 0);

}

/**
 * hyscan_gtk_model_manager_get_selected_item:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: идентификатор выделенного объекта.
 */
gchar*
hyscan_gtk_model_manager_get_selected_item (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->selected_item_id;
}

/**
 * hyscan_gtk_model_manager_get_selected_track:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: идентификатор галса в базе данных или NULL, если нет выделенного галса.
 * когда идентификатор больше не нужен, необходимо использовать #g_free ().
 */
gchar*
hyscan_gtk_model_manager_get_selected_track (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  GHashTable *tracks = hyscan_db_info_get_tracks (priv->track_model);
  HyScanTrackInfo *info = g_hash_table_lookup (tracks, priv->selected_item_id);

  if (info == NULL)
    return NULL;

  return g_strdup (info->name);
}

/**
 * hyscan_gtk_model_manager_unselect_all:
 * @self: указатель на Менеджер Моделей
 *
 * Отправляет сигнал о снятии выделния.
 */
void
hyscan_gtk_model_manager_unselect_all (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  if (priv->selected_item_id == NULL)
    return;

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      Extension *ext;

      if (priv->extensions[type] == NULL)
        continue;

      ext = g_hash_table_lookup (priv->extensions[type], priv->selected_item_id);

      if (ext == NULL)
        continue;

      ext->expanded = FALSE;
      break;
    }

  g_free (priv->selected_item_id);
  priv->selected_item_id = NULL;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_UNSELECT_ALL], 0);
}

/**
 * hyscan_gtk_model_manager_set_horizontal_adjustment:
 * @self: указатель на Менеджер Моделей
 * @value: положение горизонтальной полосы прокрутки
 *
 * Устанавливает положение горизонтальной полосы прокрутки представления.
 */
void
hyscan_gtk_model_manager_set_horizontal_adjustment (HyScanGtkModelManager *self,
                                                    gdouble                value)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  priv->horizontal = value;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_HORIZONTAL], 0);
}

/**
 * hyscan_gtk_model_manager_set_vertical:
 * @self: указатель на Менеджер Моделей
 * @value: положение вертикальной полосы прокрутки
 *
 * Устанавливает положение вертикальной полосы прокрутки представления.
 */
void
hyscan_gtk_model_manager_set_vertical_adjustment (HyScanGtkModelManager *self,
                                                  gdouble                value)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  priv->vertical = value;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_VERTICAL], 0);
}

/* hyscan_gtk_model_manager_get_horizontal_adjustment:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на параметры горизонтальной полосы прокрутки.
 */
gdouble
hyscan_gtk_model_manager_get_horizontal_adjustment (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->horizontal;
}

/* hyscan_gtk_model_manager_get_vertical_adjustment:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на параметры вертикальной полосы прокрутки.
 */
gdouble
hyscan_gtk_model_manager_get_vertical_adjustment (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->vertical;
}

/**
 * hyscan_gtk_model_manager_toggle_item:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор объекта в базе данных
 * active: состояние чек-бокса
 *
 * Переключает состояние чек-бокса.
 */
void
hyscan_gtk_model_manager_toggle_item (HyScanGtkModelManager *self,
                                      gchar                 *id,
                                      gboolean               active)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      Extension *ext;

      if (priv->extensions[type] == NULL)
        continue;

      if (id == NULL)
        continue;

      ext = g_hash_table_lookup (priv->extensions[type], id);

      if (ext == NULL)
        continue;

      ext->active = active;
      /* Устанавливаем статус родительского чек-бокса. */
      hyscan_gtk_model_manager_is_all_toggled (priv->extensions[type], type_id[type]);

      break;
    }

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_TOGGLED], 0, id, active);
}

/**
 * hyscan_gtk_model_manager_get_toggled_items:
 * @self: указатель на Менеджер Моделей
 * @type: тип запрашиваемых объектов
 *
 * Returns: возвращает список идентификаторов объектов
 * с активированным чек-боксом. Тип объекта определяется
 * #ModelManagerObjectType. Когда список больше не нужен,
 * необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_gtk_model_manager_get_toggled_items (HyScanGtkModelManager  *self,
                                            ModelManagerObjectType  type)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  Extension *ext;
  GHashTableIter iter;
  guint   i    = 0;
  gchar **list = NULL,
         *id   = NULL;

  list = g_new0 (gchar*, 1);

  g_hash_table_iter_init (&iter, priv->extensions[type]);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (!ext->active)
        continue;

      list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
      list[i++] = g_strdup (id);
    }
  list[i] = NULL;
  return list;
}

/**
 * hyscan_gtk_model_manager_has_toggled_items:
 * @self: указатель на Менеджер Моделей
 * @type: тип запрашиваемых объектов
 *
 * Returns: TRUE  - есть объекты с активированным чек-боксом,
 *          FALSE - нет объектов с активированным чек-боксом.
 */
gboolean
hyscan_gtk_model_manager_has_toggled_items (HyScanGtkModelManager  *self,
                                            ModelManagerObjectType  type)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  Extension *ext;
  GHashTableIter iter;
  gchar *id;

  g_hash_table_iter_init (&iter, priv->extensions[type]);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (!ext->active)
        continue;

       return TRUE;
    }

  return FALSE;
}

/**
 * hyscan_gtk_model_manager_expand_item:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор объекта в базе данных
 * @expanded: состояние узла TRUE - развёрнут, FALSE - свёрнут.
 *
 * Сохраняет состояние узла.
 */
void
hyscan_gtk_model_manager_expand_item (HyScanGtkModelManager *self,
                                      gchar                 *id,
                                      gboolean               expanded)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  ModelManagerSignal signal;

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      Extension *ext;

      if (priv->extensions[type] == NULL)
        continue;

      if (id == NULL)
        continue;

      ext = g_hash_table_lookup (priv->extensions[type], id);

      if (ext == NULL)
        continue;

      ext->expanded = expanded;
      break;
    }

  if (priv->current_id != NULL)
    g_free (priv->current_id);
  priv->current_id = g_strdup (id);

  signal = (expanded)? SIGNAL_ITEM_EXPANDED : SIGNAL_ITEM_COLLAPSED;
  g_signal_emit (self, hyscan_model_manager_signals[signal], 0);
}

/**
 * hyscan_gtk_model_manager_get_expanded_items:
 * @self: указатель на Менеджер Моделей
 * @type: тип запрашиваемых объектов
 * @expanded: TRUE  - развёрнутые,
 *            FALSE - свёрнутые
 *
 * Returns: возвращает список идентификаторов объектов
 * которые нужно развёрнуть или свернуть. Тип объекта
 * определяется #ModelManagerObjectType. Когда список
 * больше не нужен, необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_gtk_model_manager_get_expanded_items (HyScanGtkModelManager  *self,
                                             ModelManagerObjectType  type,
                                             gboolean                expanded)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  Extension      *ext;
  GHashTableIter  iter;
  guint           i = 0;
  gchar         **list = NULL, *id = NULL;;

  list = g_new0 (gchar*, 1);

  g_hash_table_iter_init (&iter, priv->extensions[type]);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (ext->expanded != expanded)
        continue;

      list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
      list[i++] = g_strdup (id);
    }
  list[i] = NULL;
  return list;
}
/**
 * hyscan_gtk_model_manager_get_current_id:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: возвращает идентификатор текущего объекта в виде строки.
 */
gchar*
hyscan_gtk_model_manager_get_current_id (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return priv->current_id;
}

/**
 * hyscan_gtk_model_manager_delete_toggled_items:
 * @self: указатель на Менеджер Моделей
 * @convert: TRUE  - конвертировать акустические метки в гео-метки,
 *           FALSE - удалить акустические метки вместе с галсом
 *
 * Удаляет выбранные объекты из базы данных.
 */
void
hyscan_gtk_model_manager_delete_toggled_items (HyScanGtkModelManager *self,
                                               gboolean               convert)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  BLOCK_SIGNAL (priv->signal_labels_changed,             priv->label_model);
  BLOCK_SIGNAL (priv->signal_geo_marks_changed,          priv->geo_mark_model);
  BLOCK_SIGNAL (priv->signal_acoustic_marks_loc_changed, priv->acoustic_loc_model);
  BLOCK_SIGNAL (priv->signal_acoustic_marks_changed,     priv->acoustic_mark_model);
  BLOCK_SIGNAL (priv->signal_tracks_changed,             priv->track_model);

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      gchar **list = hyscan_gtk_model_manager_get_toggled_items (self, type);

      if (list == NULL)
        continue;
      /* Удаляем выделенные объекты. */
      for (gint i = 0; list[i] != NULL; i++)
        {
          switch (type)
           {
            case LABEL:
                hyscan_object_store_remove (HYSCAN_OBJECT_STORE (priv->label_model),
                                            HYSCAN_TYPE_LABEL,
                                            list[i]);
              break;

            case GEO_MARK:
              hyscan_object_store_remove (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                          HYSCAN_TYPE_MARK_GEO,
                                          list[i]);
              break;

            case ACOUSTIC_MARK:
              hyscan_object_store_remove (HYSCAN_OBJECT_STORE (priv->acoustic_mark_model),
                                          HYSCAN_TYPE_MARK_WATERFALL,
                                          list[i]);
              break;

            case TRACK:
              hyscan_gtk_model_manager_delete_track_by_id (self, list[i], convert);
              break;

            default:
              break;
           }
        }

      g_strfreev (list);
    }

  priv->update_model_flag = TRUE;

  UNBLOCK_SIGNAL (priv->signal_labels_changed,             priv->label_model);
  UNBLOCK_SIGNAL (priv->signal_geo_marks_changed,          priv->geo_mark_model);
  UNBLOCK_SIGNAL (priv->signal_acoustic_marks_loc_changed, priv->acoustic_loc_model);
  UNBLOCK_SIGNAL (priv->signal_acoustic_marks_changed,     priv->acoustic_mark_model);
  UNBLOCK_SIGNAL (priv->signal_tracks_changed,             priv->track_model);
}

/**
 * hyscan_gtk_model_manager_has_toggled:
 * @self: указатель на Менеджер Моделей
 *
 * Проверка, есть ли выбранные объекты.
 *
 * Returns: TRUE  - есть выбранные объекты,
 *          FALSE - нет выбранных объектов.
 */
gboolean
hyscan_gtk_model_manager_has_toggled (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    {
      GHashTableIter  iter;
      Extension      *ext;
      gchar          *id;

      g_hash_table_iter_init (&iter, priv->extensions[type]);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
        {
          if (ext->active == TRUE)
            return TRUE;
        }
    }
  return FALSE;
}

/**
 * hyscan_gtk_model_manager_toggled_items_set_labels:
 * @self: указатель на Менеджер Моделей
 * @labels: битовая маска групп в которые нужно перенести выделенные объекты
 * @inconsistents: битовая маска групп, состояние которых нужно изменить
 *
 * Присваивает выделенным объектам битовую маску групп и обновляет дату и время сделанных изменений.
 */
void
hyscan_gtk_model_manager_toggled_items_set_labels (HyScanGtkModelManager *self,
                                                   guint64                labels,
                                                   guint64                inconsistents)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  /* Текущие дата и время. */
  GDateTime *now_local = g_date_time_new_now_local ();
  gint64 current_time = g_date_time_to_unix (now_local);

  for (ModelManagerObjectType type = LABEL; type < TYPES; type++)
    toggled_items_set_label[type] (self, labels, inconsistents, current_time);

  g_date_time_unref (now_local);
  /* Устанавливаем флаг для обновления модели представления данных. */
  priv->update_model_flag = TRUE;
}

/**
 * hyscan_gtk_model_manager_toggled_items_get_bite_masks:
 * @self: указатель на Менеджер Моделей
 * @labels: указатель на битовую маску общих групп
 * @inconsistents: указатель на битовую маску групп с неопределённым статусом
 *
 * Наполняет битовые маски в соответствии с группами выбранных объектов.
 *
 */
void
hyscan_gtk_model_manager_toggled_items_get_bit_masks (HyScanGtkModelManager *self,
                                                      guint64               *labels,
                                                      guint64               *inconsistents)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;

  *inconsistents = 0;
  *labels        = 0;

  for (ModelManagerObjectType type = GEO_MARK; type < TYPES; type++)
    {
       gchar** list = hyscan_gtk_model_manager_get_toggled_items (self, type);

       if (list == NULL)
         continue;

       for (guint i = 0; list[i] != NULL; i++)
         {
           gint64 mask = 0;

           switch (type)
             {
             case GEO_MARK:
               {
                 HyScanMarkGeo *object = (HyScanMarkGeo*)hyscan_object_store_get (HYSCAN_OBJECT_STORE (priv->geo_mark_model),
                                                                                  HYSCAN_TYPE_MARK_GEO,
                                                                                  list[i]);

                 if (object == NULL)
                   break;

                 mask = object->labels;

                 hyscan_mark_geo_free (object);
               }
               break;

             case ACOUSTIC_MARK:
               {
                 /* Получаем объект из базы данных по идентификатору. */
                 /*HyScanMarkWaterfall *object = (HyScanMarkWaterfall*)hyscan_object_model_get_by_id (priv->acoustic_mark_model, list[i]);*/
                 HyScanMarkWaterfall *object = (HyScanMarkWaterfall*)hyscan_object_store_get (
                                                                            HYSCAN_OBJECT_STORE (priv->acoustic_mark_model),
                                                                            HYSCAN_TYPE_MARK_WATERFALL,
                                                                            list[i]);

                 if (object == NULL)
                   break;

                 mask = object->labels;

                 hyscan_mark_waterfall_free (object);
               }
               break;

             case TRACK:
               {
                 HyScanTrackInfo *object;
                 gint32 project_id = hyscan_db_project_open (priv->db, priv->project_name);

                 if (project_id <= 0)
                   break;

                 object = hyscan_db_info_get_track_info (priv->db, project_id, list[i]);

                 hyscan_db_close (priv->db, project_id);

                 if (object == NULL)
                   break;

                 mask = object->labels;

                 hyscan_db_info_track_info_free (object);
               }
               break;

             default:
               break;
             }

           if (*labels != 0)
             {
               /* Следующие объекты. */
               guint64 data[2] = {*labels | mask,  /* 0 */
                                  *labels & mask}; /* 1 */
               /* Сохраняем какие биты изменились. */
               *inconsistents |= data[0] ^ data[1];
               /* Сохраняем общую маску для всех объектов. */
               *labels |= mask;
               continue;
             }
           /* Первый объект, просто сохраняем маску. */
           *labels = mask;
         }
       g_strfreev (list);
    }
}

/**
 * hyscan_gtk_model_manager_show_object:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор объекта в базе данных.
 *
 * Показывает объект на карте.
 */
void
hyscan_gtk_model_manager_show_object (HyScanGtkModelManager *self,
                                      gchar                 *id,
                                      guint                  type)
{
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_SHOW_OBJECT], 0, id, type);
}

/**
 * hyscan_gtk_model_manager_get_all_labels:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на хэш-таблицу с глобальными и пользовательскими группами.
 */
GHashTable*
hyscan_gtk_model_manager_get_all_labels (HyScanGtkModelManager *self)
{
  HyScanGtkModelManagerPrivate *priv = self->priv;
  return hyscan_gtk_model_manager_get_labels (priv->label_model);
}

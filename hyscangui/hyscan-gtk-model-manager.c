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
 * @Title: HyScanModelManager
 * @See_also: #HyScanMarkManager, #HyScanMarkManagerView
 *
 * Класс #HyScanModelManager содержит данные из нескольких моделей и реализует общую
 * модель для табличного или древовидного представления, и функции для работы с объектами
 * и обмена данными. Виджет #HyScanMarkManager реализует представление данных общей
 * модели и управление.
 *
 * - hyscan_mark_manager_view_new () - создание нового представления;
 * - hyscan_model_manager_get_track_model () - модель данных с информацией о галсах;
 * - hyscan_model_manager_get_acoustic_mark_model () - модель данных с информацией об акустических метках;
 * - hyscan_model_manager_get_geo_mark_model () - модель данных с информацией о гео-метках;
 * - hyscan_model_manager_get_label_model () - модель данных с информацией о группах;
 * - hyscan_model_manager_get_acoustic_mark_loc_model - модель данных с информацией об акустических метках c координатами;
 * - hyscan_model_manager_get_view_model () - модель представления данных для #HyScanMarkManager;
 * - hyscan_model_manager_get_signal_title () - название сигнала Менеджера Моделей;
 * - hyscan_model_manager_set_project_name () - смена названия проекта;
 * - hyscan_model_manager_get_project_name () - название проекта;
 * - hyscan_model_manager_get_export_folder () - директория для экспорта;
 * - hyscan_model_manager_get_db () - база данных;
 * - hyscan_model_manager_get_cache () - кэш;
 * - hyscan_model_manager_get_all_tracks_id () - список всех галсов;
 * - hyscan_model_manager_set_grouping () - установка типа группировки древовидного представления: по типам, по группам;
 * - hyscan_model_manager_get_grouping () - тип группировки;
 * - hyscan_model_manager_set_selected_item () - выделение объекта;
 * - hyscan_model_manager_get_selected_item () - идентификатор выделенного объект;
 * - hyscan_model_manager_get_selected_track () - идентификатор выделенного галса;
 * - hyscan_model_manager_unselect_all () - снятие выделения;
 * - hyscan_model_manager_set_horizontal_adjustment () - установка положения горизонтальной полосы прокрутки;
 * - hyscan_model_manager_set_vertical_adjustment () -  установка положения вертикальной полосы прокрутки;
 * - hyscan_model_manager_get_horizontal_adjustment () - текущее положение горизонтальной полосы прокрутки;
 * - hyscan_model_manager_get_vertical_adjustment () - текущее положение вертикальной полосы прокрутки;
 * - hyscan_model_manager_toggle_item () - установка состояния чек-бокса для объекта по идентификатору;
 * - hyscan_model_manager_get_toggled_items () - список идентификаторов объектов с активированным чек-боксом;
 * - hyscan_model_manager_expand_item () - установка состояния узла объекта по идентификатору;
 * - hyscan_model_manager_get_current_id () - идентифифкатор объекта;
 * - hyscan_model_manager_delete_toggled_items () - удаление объектов с активированным чек-боксом из базы данных;
 * - hyscan_model_manager_has_toggled () - наличие объектов с активированным чек-боксом;
 * - hyscan_model_manager_toggled_iteml_change_label () - перенос объектов с активированным чек-боксом в другую группу.
 */

#include <hyscan-gtk-model-manager.h>
#include <glib/gi18n.h>

enum
{
  PROP_PROJECT_NAME = 1,    /* Название проекта. */
  PROP_DB,                  /* База данных. */
  PROP_CACHE,               /* Кэш.*/
  PROP_EXPORT_FOLDER,       /* Директория для экспорта. */
  N_PROPERTIES
};

/* Типы записей в модели. */
typedef enum
{
  PARENT,                   /* Узел. */
  CHILD,                    /* Объект. */
  ITEM                      /* Атрибут объекта. */
} ExtensionType;

/* Структура содержащая расширеную информацию об объектах. */
typedef struct
{
  ExtensionType  type;      /* Тип записи. */
  gboolean       active,    /* Состояние чек-бокса. */
                 expanded;  /* Развёрнут ли объект (для древовидного представления). */
}Extension;

struct _HyScanModelManagerPrivate
{
  HyScanObjectModel    *acoustic_marks_model, /* Модель данных акустических меток. */
                       *geo_mark_model,       /* Модель данных гео-меток. */
                       *label_model;          /* Модель данных групп. */
  HyScanMarkLocModel   *acoustic_loc_model;   /* Модель данных акустических меток с координатами. */
  HyScanDBInfo         *track_model;          /* Модель данных галсов. */
  GtkTreeModel         *view_model;           /* Модель представления данных (табличное или древовидное). */
  HyScanCache          *cache;                /* Кэш.*/
  HyScanDB             *db;                   /* База данных. */
  ModelManagerGrouping  grouping;             /* Тип группировки. */
  Extension            *node[TYPES];          /* Информация для описания узлов для древовидного
                                               * представления с группировкой по типам. */
  GHashTable           *extensions[TYPES];    /* Массив таблиц с дополнительной информацией для всех типов объектов. */
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
                        constructed_flag;     /* Флаг инициализации всех моделей. */
};

/* Названия сигналов.
 * Должны идти в порядке соответвующем ModelManagerSignal
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
                                 "unselect"};            /* Снятие выделения. */

/* Форматированная строка для вывода времени и даты. */
static gchar *date_time_stamp = "%d.%m.%Y %H:%M:%S";
/* Форматированная строка для вывода расстояния. */
static gchar *distance_stamp = N_("%.2f m");
/* Оператор создавший типы объектов в древовидном списке. */
static gchar *author = N_("Default");
/* Строка если нет данных по какому-либо атрибуту. */
static gchar *unknown = N_("Unknown");

/* Стандартные картинки для типов объектов. */
static gchar *icon_name[] =    {"emblem-documents",            /* Группы. */
                                "mark-location",               /* Гео-метки. */
                                "emblem-photos",               /* Акустические метки. */
                                "preferences-system-sharing"}; /* Галсы. */
/* Названия типов. */
static gchar *type_name[] =    {N_("Labels"),                      /* Группы. */
                                N_("Geo-marks"),                   /* Гео-метки. */
                                N_("Acoustic marks"),              /* Акустические метки. */
                                N_("Tracks")};                     /* Галсы. */
/* Описания типов. */
static gchar *type_desc[] =    {N_("All labels"),                  /* Группы. */
                                N_("All geo-marks"),               /* Гео-метки. */
                                N_("All acoustic marks"),          /* Акустические метки. */
                                N_("All tracks")};                 /* Галсы. */
/* Идентификаторы для узлов для древовидного представления с группировкой по типам. */
static gchar *type_id[TYPES] = {"ID_NODE_LABEL",                   /* Группы. */
                                "ID_NODE_GEO_MARK",                /* Гео-метки.*/
                                "ID_NODE_ACOUSTIC_MARK",           /* Акустические метки. */
                                "ID_NODE_TRACK"};                  /* Галсы. */
/* Cкорость движения при которой генерируются тайлы в Echosounder-е, но метка
 * сохраняется в базе данных без учёта этого коэфициента масштабирования. */
static gdouble ship_speed = 10.0;
/* Идентификатор для отслеживания изменения названия проекта. */
static GParamSpec   *notify = NULL;

static void          hyscan_model_manager_set_property                     (GObject                 *object,
                                                                            guint                    prop_id,
                                                                            const GValue            *value,
                                                                            GParamSpec              *pspec);

static void          hyscan_model_manager_get_property                     (GObject                 *object,
                                                                            guint                    prop_id,
                                                                            GValue                  *value,
                                                                            GParamSpec              *pspec);

static void          hyscan_model_manager_constructed                      (GObject                 *object);

static void          hyscan_model_manager_finalize                         (GObject                 *object);

static void          hyscan_model_manager_acoustic_marks_model_changed     (HyScanObjectModel       *model,
                                                                            HyScanModelManager      *self);

static void          hyscan_model_manager_track_model_changed              (HyScanDBInfo            *model,
                                                                            HyScanModelManager      *self);

static void          hyscan_model_manager_acoustic_marks_loc_model_changed (HyScanMarkLocModel      *model,
                                                                            HyScanModelManager      *self);

static void          hyscan_model_manager_geo_mark_model_changed           (HyScanObjectModel       *model,
                                                                            HyScanModelManager      *self);

static void          hyscan_model_manager_label_model_changed              (HyScanObjectModel       *model,
                                                                            HyScanModelManager      *self);

static GtkTreeModel* hyscan_model_manager_update_view_model                (HyScanModelManager      *self);

static void          hyscan_model_manager_set_view_model                   (HyScanModelManager      *self);

static void          hyscan_model_manager_refresh_all_items_by_types       (HyScanModelManager      *self);

static void          hyscan_model_manager_refresh_all_items_by_labels      (HyScanModelManager      *self);

static void          hyscan_model_manager_refresh_items_by_types           (HyScanModelManager      *self,
                                                                            ModelManagerObjectType   type);

static void          hyscan_model_manager_refresh_labels_by_types          (GtkTreeStore            *store,
                                                                            GHashTable              *labels,
                                                                            GHashTable              *extensions);

static void          hyscan_model_manager_refresh_geo_marks_by_types       (GtkTreeStore            *store,
                                                                            GHashTable              *geo_marks,
                                                                            GHashTable              *extensions,
                                                                            GHashTable              *labels);

static void          hyscan_model_manager_refresh_geo_marks_by_labels      (GtkTreeStore            *store,
                                                                            GtkTreeIter             *iter,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *geo_marks,
                                                                            GHashTable              *extensions);

static void          hyscan_model_manager_refresh_acoustic_marks_by_types  (GtkTreeStore            *store,
                                                                            GHashTable              *acoustic_marks,
                                                                            GHashTable              *extensions,
                                                                            GHashTable              *labels);

static void          hyscan_model_manager_refresh_acoustic_marks_by_labels (GtkTreeStore            *store,
                                                                            GtkTreeIter             *iter,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *acoustic_marks,
                                                                            GHashTable              *extensions);

static void          hyscan_model_manager_refresh_tracks_by_types          (GtkTreeStore            *store,
                                                                            GHashTable              *tracks,
                                                                            GHashTable              *extensions,
                                                                            GHashTable              *labels);

static void          hyscan_model_manager_refresh_tracks_by_labels         (GtkTreeStore            *store,
                                                                            GtkTreeIter             *iter,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *tracks,
                                                                            GHashTable              *extensions);

static void          hyscan_model_manager_clear_view_model                 (GtkTreeModel            *view_model,
                                                                            gboolean                *flag);

static gboolean      hyscan_model_manager_init_extensions                  (HyScanModelManager      *self);

static Extension*    hyscan_model_manager_extension_new                    (ExtensionType            type,
                                                                            gboolean                 active,
                                                                            gboolean                 selected);

static Extension*    hyscan_model_manager_extension_copy                   (Extension               *ext);

static void          hyscan_model_manager_extension_free                   (gpointer                 data);

static GHashTable*   hyscan_model_manager_get_extensions                   (HyScanModelManager      *self,
                                                                            ModelManagerObjectType   type);

static gboolean      hyscan_model_manager_is_all_toggled                   (GHashTable              *table,
                                                                            const gchar             *node_id);

static void          hyscan_model_manager_delete_item                      (HyScanModelManager      *self,
                                                                            ModelManagerObjectType   type,
                                                                            gchar                   *id);

static void          hyscan_model_manager_geo_mark_change_label            (HyScanModelManager      *self,
                                                                            gchar                  **list,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *table,
                                                                            guint64                  current_time);

static void          hyscan_model_manager_acoustic_mark_change_label       (HyScanModelManager      *self,
                                                                            gchar                  **list,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *table,
                                                                            guint64                  current_time);

static void          hyscan_model_manager_track_change_label               (HyScanModelManager      *self,
                                                                            gchar                  **list,
                                                                            HyScanLabel             *label,
                                                                            GHashTable              *table,
                                                                            GDateTime               *now_local,
                                                                            guint64                  current_time);

static guint         hyscan_model_manager_signals[SIGNAL_MODEL_MANAGER_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanModelManager, hyscan_model_manager, G_TYPE_OBJECT)

void
hyscan_model_manager_class_init (HyScanModelManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  guint index;

  object_class->set_property = hyscan_model_manager_set_property;
  object_class->get_property = hyscan_model_manager_get_property;
  object_class->constructed  = hyscan_model_manager_constructed;
  object_class->finalize     = hyscan_model_manager_finalize;

  /* Название проекта. */
  notify = g_param_spec_string ("project_name", "Project_name", "Project name",
                                "",
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (object_class, PROP_PROJECT_NAME, notify);
  /* База данных. */
  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Data base",
                         "The link to data base",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Кэш.*/
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The link to main cache with frequency used stafs",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Директория для экспорта.*/
    g_object_class_install_property (object_class, PROP_EXPORT_FOLDER,
      g_param_spec_string ("export_folder", "Export folder", "Directory for export",
                           "",
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  /* Создание сигналов. */
  for (index = 0; index < SIGNAL_MODEL_MANAGER_LAST; index++)
    {
       hyscan_model_manager_signals[index] =
                    g_signal_new (signals[index],
                                  HYSCAN_TYPE_MODEL_MANAGER,
                                  G_SIGNAL_RUN_LAST,
                                  0, NULL, NULL,
                                  g_cclosure_marshal_VOID__VOID,
                                  G_TYPE_NONE, 0);
    }
}

void
hyscan_model_manager_init (HyScanModelManager *self)
{
  self->priv = hyscan_model_manager_get_instance_private (self);
}

void
hyscan_model_manager_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
    /* Название проекта */
    case PROP_PROJECT_NAME:
      {
        if (priv->constructed_flag)
          hyscan_model_manager_set_project_name (self, g_value_get_string (value));
        else
          priv->project_name = g_value_dup_string (value);
      }
      break;
    /* База данных. */
    case PROP_DB:
      {
        priv->db  = g_value_dup_object (value);
      }
      break;
    /* Кэш.*/
    case PROP_CACHE:
      {
        priv->cache  = g_value_dup_object (value);
      }
      break;
    /* Директория для экспорта.*/
    case PROP_EXPORT_FOLDER:
      {
        priv->export_folder = g_value_dup_string (value);
      }
      break;
    /* Что-то ещё... */
    default:
      {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      }
      break;
    }
}

void
hyscan_model_manager_get_property (GObject      *object,
                                   guint         prop_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;

  switch (prop_id)
    {
    /* Название проекта */
    case PROP_PROJECT_NAME:
      g_value_set_string (value, priv->project_name);
      break;
    /* Что-то ещё... */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Конструктор. */
void
hyscan_model_manager_constructed (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;
  /* Модель галсов. */
  priv->track_model = hyscan_db_info_new (priv->db);
  hyscan_db_info_set_project (priv->track_model, priv->project_name);
  priv->signal_tracks_changed = g_signal_connect (priv->track_model,
                    "tracks-changed",
                    G_CALLBACK (hyscan_model_manager_track_model_changed),
                    self);
  /* Модель данных акустических меток с координатами. */
  priv->acoustic_loc_model = hyscan_mark_loc_model_new (priv->db, priv->cache);
  hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);
  priv->signal_acoustic_marks_changed = g_signal_connect (priv->acoustic_loc_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_loc_model_changed),
                    self);
  /* Модель акустических меток. */
  priv->acoustic_marks_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  hyscan_object_model_set_project (priv->acoustic_marks_model, priv->db, priv->project_name);
  priv->signal_acoustic_marks_loc_changed = g_signal_connect (priv->acoustic_marks_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_model_changed),
                    self);
  /* Модель геометок. */
  priv->geo_mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);
  priv->signal_geo_marks_changed = g_signal_connect (priv->geo_mark_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_geo_mark_model_changed),
                    self);
  /* Модель данных групп. */
  priv->label_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_LABEL);
  hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
  priv->signal_labels_changed = g_signal_connect (priv->label_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_label_model_changed),
                    self);
  /* Создаём модель представления данных. */
  priv->view_model = hyscan_model_manager_update_view_model (self);
  /* Устанавливаем флаг инициализации всех моделей. */
  priv->constructed_flag = TRUE;
}
/* Деструктор. */
void
hyscan_model_manager_finalize (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */
  g_free (priv->project_name);
  priv->project_name = NULL;
  g_free (priv->export_folder);
  priv->export_folder = NULL;
  g_object_unref (priv->cache);
  priv->cache = NULL;
  g_object_unref (priv->db);
  priv->db = NULL;

  G_OBJECT_CLASS (hyscan_model_manager_parent_class)->finalize (object);
}

/* Обработчик сигнала изменения данных в модели галсов. */
void
hyscan_model_manager_track_model_changed (HyScanDBInfo       *model,
                                          HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  hyscan_model_manager_update_view_model (self);
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_TRACKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели акустических меток. */
void
hyscan_model_manager_acoustic_marks_model_changed (HyScanObjectModel  *model,
                                                   HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  hyscan_model_manager_update_view_model (self);
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ACOUSTIC_MARKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели акустических меток с координатами. */
void
hyscan_model_manager_acoustic_marks_loc_model_changed (HyScanMarkLocModel *model,
                                                       HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  hyscan_model_manager_update_view_model (self);
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ACOUSTIC_MARKS_LOC_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели гео-меток. */
void
hyscan_model_manager_geo_mark_model_changed (HyScanObjectModel  *model,
                                             HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  hyscan_model_manager_update_view_model (self);
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GEO_MARKS_CHANGED], 0);
}

/* Обработчик сигнала изменения данных в модели групп. */
void
hyscan_model_manager_label_model_changed (HyScanObjectModel  *model,
                                          HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  hyscan_model_manager_update_view_model (self);
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_LABELS_CHANGED], 0);
}

/* Функция обновляет модель представления данных в соответствии с текущими параметрами. */
GtkTreeModel*
hyscan_model_manager_update_view_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->view_model == NULL)
    {
      if (priv->grouping == UNGROUPED)
        {
          priv->view_model = GTK_TREE_MODEL (
                gtk_list_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                    G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                    G_TYPE_STRING,   /* Название объекта. */
                                    G_TYPE_STRING,   /* Описание объекта. */
                                    G_TYPE_STRING,   /* Оператор, создавший объект. */
                                    G_TYPE_STRING,   /* Текст подсказки. */
                                    G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                    G_TYPE_STRING,   /* Название картинки. */
                                    G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                    G_TYPE_BOOLEAN,  /* Видимость чек-бокса. */
                                    G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                    G_TYPE_STRING,   /* Время создания объекта. */
                                    G_TYPE_STRING,   /* Время модификации объекта. */
                                    G_TYPE_STRING,   /* Координаты. */
                                    G_TYPE_STRING,   /* Название галса. */
                                    G_TYPE_STRING,   /* Борт. */
                                    G_TYPE_STRING,   /* Глубина. */
                                    G_TYPE_STRING,   /* Ширина.*/
                                    G_TYPE_STRING)); /* Наклонная дальность. */
        }
      else
        {
          priv->view_model = GTK_TREE_MODEL (
                gtk_tree_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                    G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                    G_TYPE_STRING,   /* Название объекта. */
                                    G_TYPE_STRING,   /* Описание объекта. */
                                    G_TYPE_STRING,   /* Оператор, создавший объект. */
                                    G_TYPE_STRING,   /* Текст подсказки. */
                                    G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                    G_TYPE_STRING,   /* Название картинки. */
                                    G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                    G_TYPE_BOOLEAN,  /* Видимость чек-бокса. */
                                    G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                    G_TYPE_STRING,   /* Время создания объекта. */
                                    G_TYPE_STRING,   /* Время модификации объекта. */
                                    G_TYPE_STRING,   /* Координаты. */
                                    G_TYPE_STRING,   /* Название галса. */
                                    G_TYPE_STRING,   /* Борт. */
                                    G_TYPE_STRING,   /* Глубина. */
                                    G_TYPE_STRING,   /* Ширина.*/
                                    G_TYPE_STRING)); /* Наклонная дальность. */
        }
    }

  /* Обновляем данные в модели. */
  hyscan_model_manager_set_view_model (self);

  /* Отправляем сигнал об обновлении модели представления данных. */
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_MODEL_UPDATED], 0);
  /* Возвращаем модель. */
  return priv->view_model;
}

/* Функция (если нужно пересоздаёт и) настраивает модель представления данных. */
void
hyscan_model_manager_set_view_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (hyscan_model_manager_init_extensions (self))
    {
      switch (priv->grouping)
        {
        case BY_LABELS: /* По группам. */
        case BY_TYPES:  /* По типам. */
          {
            /* Проверяем, нужно ли пересоздавать модель.*/
            if (GTK_IS_LIST_STORE (priv->view_model))
              {
                /* Очищаем модель. */
                hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
                g_object_unref (priv->view_model);
                /* Создаём новую модель. */
                priv->view_model = GTK_TREE_MODEL (
                      gtk_tree_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                          G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                          G_TYPE_STRING,   /* Название объекта. */
                                          G_TYPE_STRING,   /* Описание объекта. */
                                          G_TYPE_STRING,   /* Оператор, создавший объект. */
                                          G_TYPE_STRING,   /* Текст подсказки. */
                                          G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                          G_TYPE_STRING,   /* Название картинки. */
                                          G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                          G_TYPE_BOOLEAN,  /* Видимость чек-бокса. */
                                          G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                          G_TYPE_STRING,   /* Время создания объекта. */
                                          G_TYPE_STRING,   /* Время модификации объекта. */
                                          G_TYPE_STRING,   /* Координаты. */
                                          G_TYPE_STRING,   /* Название галса. */
                                          G_TYPE_STRING,   /* Борт. */
                                          G_TYPE_STRING,   /* Глубина. */
                                          G_TYPE_STRING,   /* Ширина.*/
                                          G_TYPE_STRING)); /* Наклонная дальность. */
              }
            else
              {
                /* Очищаем модель. */
                hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
                priv->view_model = GTK_TREE_MODEL (priv->view_model);
              }

            if (priv->grouping == BY_TYPES)
              hyscan_model_manager_refresh_all_items_by_types (self);
            else if (priv->grouping == BY_LABELS)
              hyscan_model_manager_refresh_all_items_by_labels (self);
          }
          break;
        case UNGROUPED: /* Табличное представление. */
        default:        /* Оно же и по умолчанию. */
          {
            GtkListStore   *store;
            GtkTreeIter     store_iter;

            /* Проверяем, нужно ли пересоздавать модель.*/
            if (GTK_IS_TREE_STORE (priv->view_model))
              {
                /* Очищаем модель. */
                hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
                g_object_unref (priv->view_model);
                /* Создаём новую модель. */
                store = gtk_list_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                            G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                            G_TYPE_STRING,   /* Название объекта. */
                                            G_TYPE_STRING,   /* Описание объекта. */
                                            G_TYPE_STRING,   /* Оператор, создавший объект. */
                                            G_TYPE_STRING,   /* Текст подсказки. */
                                            G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                            G_TYPE_STRING,   /* Название картинки. */
                                            G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                            G_TYPE_BOOLEAN,  /* Видимость чек-бокса. */
                                            G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                            G_TYPE_STRING,   /* Время создания объекта. */
                                            G_TYPE_STRING,   /* Время модификации объекта. */
                                            G_TYPE_STRING,   /* Координаты. */
                                            G_TYPE_STRING,   /* Название галса. */
                                            G_TYPE_STRING,   /* Борт. */
                                            G_TYPE_STRING,   /* Глубина. */
                                            G_TYPE_STRING,   /* Ширина.*/
                                            G_TYPE_STRING);  /* Наклонная дальность. */
              }
            else
              {
                /* Очищаем модель. */
                hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
                store = GTK_LIST_STORE (priv->view_model);
              }
            /* Группы. */
/*            if (priv->extensions[LABEL] != NULL)
              {
                GHashTable *table = hyscan_object_model_get (priv->label_model);
                HyScanLabel *object;

                if (table != NULL)
                  {
                    GHashTableIter  table_iter;      *//* Итератор для обхода хэш-таблиц. */
/*                    gchar *id;                       *//* Идентификатор для обхода хэш-таблиц (ключ). */

/*                    g_hash_table_iter_init (&table_iter, table);
                    while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
                      {
                        if (object != NULL)
                          {
                             Extension *ext = g_hash_table_lookup (priv->extensions[LABEL], id);

                             gtk_list_store_append (store, &store_iter);
                             gtk_list_store_set (store,              &store_iter,
                                                 COLUMN_ID,           id,
                                                 COLUMN_NAME,         object->name,
                                                 COLUMN_DESCRIPTION,  object->description,
                                                 COLUMN_OPERATOR,     object->operator_name,
                                                 COLUMN_TYPE,         LABEL,
                                                 COLUMN_ICON,         object->icon_name,
                                                 COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                                                 COLUMN_LABEL,        object->label,
                                                 COLUMN_CTIME,        g_date_time_format (
                                                                        g_date_time_new_from_unix_local (object->ctime),
                                                                        date_time_stamp),
                                                 COLUMN_MTIME,        g_date_time_format (
                                                                        g_date_time_new_from_unix_local (object->mtime),
                                                                        date_time_stamp),
                                                 -1);
                          }
                      }

                    g_hash_table_destroy (table);
                  }
              }*/
            /* Гео-метки. */
            if (priv->extensions[GEO_MARK] != NULL)
              {
                GHashTable *table = hyscan_object_model_get (priv->geo_mark_model);
                HyScanMarkGeo *object;

                if (table != NULL)
                  {
                    GHashTableIter  table_iter;      /* Итератор для обхода хэш-таблиц. */
                    gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

                    g_hash_table_iter_init (&table_iter, table);
                    while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
                      {
                        if (object != NULL)
                          {
                            Extension *ext  = g_hash_table_lookup (priv->extensions[GEO_MARK], id);
                            gchar *position = g_strdup_printf ("%.6f° %.6f°",
                                                               object->center.lat,
                                                               object->center.lon);

                            gtk_list_store_append (store, &store_iter);
                            gtk_list_store_set (store,              &store_iter,
                                                COLUMN_ID,           id,
                                                COLUMN_NAME,         object->name,
                                                COLUMN_DESCRIPTION,  object->description,
                                                COLUMN_OPERATOR,     object->operator_name,
                                                COLUMN_TOOLTIP,      _(type_name[GEO_MARK]),
                                                COLUMN_TYPE,         GEO_MARK,
                                                COLUMN_ICON,         icon_name[GEO_MARK],
                                                COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                                                COLUMN_VISIBLE,      TRUE,
                                                COLUMN_LABEL,        object->labels,
                                                COLUMN_CTIME,        g_date_time_format (
                                                                       g_date_time_new_from_unix_local (
                                                                         object->ctime / G_TIME_SPAN_SECOND),
                                                                       date_time_stamp),
                                                COLUMN_MTIME,        g_date_time_format (
                                                                       g_date_time_new_from_unix_local (
                                                                         object->mtime / G_TIME_SPAN_SECOND),
                                                                       date_time_stamp),
                                                COLUMN_LOCATION,     position,
                                                -1);

                            g_free (position);
                          }
                      }

                    g_hash_table_destroy (table);
                  }
              }
            /* Акустические метки. */
            if (priv->extensions[ACOUSTIC_MARK] != NULL)
              {
                GHashTable *table = hyscan_mark_loc_model_get (priv->acoustic_loc_model);
                HyScanMarkLocation *location;

                if (table != NULL)
                  {
                    GHashTableIter  table_iter;      /* Итератор для обхода хэш-таблиц. */
                    gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

                    g_hash_table_iter_init (&table_iter, table);
                    while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
                      {
                        HyScanMarkWaterfall *object = location->mark;

                        if (object != NULL)
                          {
                            Extension *ext = g_hash_table_lookup (priv->extensions[ACOUSTIC_MARK], id);
                            gchar *position = g_strdup_printf ("%.6f° %.6f°",
                                                               location->mark_geo.lat,
                                                               location->mark_geo.lon),
                                  *board = g_strdup (_(unknown)),
                                  *depth = g_strdup_printf (_(distance_stamp), location->depth),
                                  *width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width),
                                  *slant_range = NULL;

                            switch (location->direction)
                              {
                              case HYSCAN_MARK_LOCATION_PORT:
                                {
                                  gdouble across_start = -location->mark->width - location->across ;
                                  gdouble across_end   =  location->mark->width - location->across;

                                  if (across_start < 0 && across_end > 0)
                                    {
                                      width = g_strdup_printf (_(distance_stamp), -across_start);
                                    }

                                  board = g_strdup (_("Port"));

                                  slant_range = g_strdup_printf (_(distance_stamp), location->across);
                                }
                                break;
                              case HYSCAN_MARK_LOCATION_STARBOARD:
                                {
                                  gdouble across_start = location->across - location->mark->width;
                                  gdouble across_end   = location->across + location->mark->width;

                                  if (across_start < 0 && across_end > 0)
                                    {
                                      width = g_strdup_printf (_(distance_stamp), across_end);
                                    }

                                  board = g_strdup (_("Starboard"));

                                  slant_range = g_strdup_printf (_(distance_stamp), location->across);
                                }
                                break;
                              case HYSCAN_MARK_LOCATION_BOTTOM:
                                {
                                  width =  g_strdup_printf (_(distance_stamp), ship_speed * 2.0 * location->mark->height);

                                  board = g_strdup (_("Bottom"));
                                }
                                break;
                              default: break;
                              }

                            gtk_list_store_append (store, &store_iter);
                            gtk_list_store_set (store,              &store_iter,
                                                COLUMN_ID,           id,
                                                COLUMN_NAME,         object->name,
                                                COLUMN_DESCRIPTION,  object->description,
                                                COLUMN_OPERATOR,     object->operator_name,
                                                COLUMN_TOOLTIP,      _(type_name[ACOUSTIC_MARK]),
                                                COLUMN_TYPE,         ACOUSTIC_MARK,
                                                COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                                                COLUMN_ACTIVE,       (ext != NULL) ? ext->active : FALSE,
                                                COLUMN_VISIBLE,      TRUE,
                                                COLUMN_LABEL,        object->labels,
                                                COLUMN_CTIME,        g_date_time_format (
                                                                       g_date_time_new_from_unix_local (
                                                                         object->ctime / G_TIME_SPAN_SECOND),
                                                                       date_time_stamp),
                                                COLUMN_MTIME,        g_date_time_format (
                                                                       g_date_time_new_from_unix_local (
                                                                         object->mtime / G_TIME_SPAN_SECOND),
                                                                       date_time_stamp),
                                                COLUMN_LOCATION,     position,
                                                COLUMN_TRACK_NAME,   location->track_name,
                                                COLUMN_BOARD,        board,
                                                COLUMN_DEPTH,        depth,
                                                COLUMN_WIDTH,        width,
                                                COLUMN_SLANT_RANGE,  slant_range,
                                                -1);
                            g_free (position);
                            g_free (board);
                            g_free (depth);
                            g_free (width);
                            g_free (slant_range);
                          }
                      }

                    g_hash_table_destroy (table);
                  }
              }
            /* Галсы. */
            if (priv->extensions[TRACK] != NULL)
              {
                GHashTable *table = hyscan_db_info_get_tracks (priv->track_model);
                HyScanTrackInfo *object;

                if (table != NULL)
                  {
                    GHashTableIter  table_iter;      /* Итератор для обхода хэш-таблиц. */
                    gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

                    g_hash_table_iter_init (&table_iter, table);
                    while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
                      {
                        if (object != NULL)
                          {
                            Extension *ext = g_hash_table_lookup (priv->extensions[TRACK], id);

                            gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp),
                                  *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);

                            gtk_list_store_append (store, &store_iter);
                            gtk_list_store_set (store, &store_iter,
                                                COLUMN_ID,          id,
                                                COLUMN_NAME,        object->name,
                                                COLUMN_DESCRIPTION, object->description,
                                                COLUMN_OPERATOR,    object->operator_name,
                                                COLUMN_TOOLTIP,     _(type_name[TRACK]),
                                                COLUMN_TYPE,        TRACK,
                                                COLUMN_ICON,        icon_name[TRACK],
                                                COLUMN_ACTIVE,      (ext != NULL) ? ext->active : FALSE,
                                                COLUMN_VISIBLE,     TRUE,
                                                COLUMN_LABEL,       object->labels,
                                                COLUMN_CTIME,       ctime,
                                                COLUMN_MTIME,       mtime,
                                                -1);
                          }
                      }

                    g_hash_table_destroy (table);
                  }
              }
            /* Обновляем указатель на модель данных. */
            if (store != NULL)
              {
                priv->view_model = GTK_TREE_MODEL (store);
              }
          }
          break;
        }
    }
}

/* Функция создаёт и заполняет узел "Группы" в древовидной модели с группировкой по типам. */
void
hyscan_model_manager_refresh_labels_by_types (GtkTreeStore *store,
                                              GHashTable   *labels,
                                              GHashTable   *extensions)
{
  if (extensions != NULL)
    {
      GtkTreeIter parent_iter;
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanLabel *object;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
      gboolean active = hyscan_model_manager_is_all_toggled (extensions, type_id[LABEL]);

      /* Добавляем новый узел "Группы" в модель */
      gtk_tree_store_append (store, &parent_iter, NULL);
      gtk_tree_store_set (store,              &parent_iter,
                          COLUMN_ID,           NULL,
                          COLUMN_NAME,         _(type_name[LABEL]),
                          COLUMN_DESCRIPTION,  _(type_desc[LABEL]),
                          COLUMN_OPERATOR,     _(author),
                          COLUMN_TOOLTIP,      _(type_name[LABEL]),
                          COLUMN_TYPE,         LABEL,
                          COLUMN_ICON,         icon_name[LABEL],
                          COLUMN_ACTIVE,       active,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        0,
                          -1);

      g_hash_table_iter_init (&table_iter, labels);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL)
            {
              GtkTreeIter child_iter,
                          item_iter;
              gboolean toggled = active;

              if (!active)
                {
                  Extension *ext = g_hash_table_lookup (extensions, id);
                  toggled = (ext != NULL) ? ext->active : FALSE;
                }
              /* Добавляем новый узел c названием группы в модель */
              gtk_tree_store_append (store, &child_iter, &parent_iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
              /* Отображается. */ COLUMN_NAME,         object->name,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      object->name,
                                  COLUMN_TYPE,         LABEL,
                                  COLUMN_ICON,         object->icon_name,
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->label,
                                  -1);
              /* Атрибуты группы. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (object->ctime),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (object->mtime),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Гео-метки" в древовидной модели с группировкой по типам. */
void
hyscan_model_manager_refresh_geo_marks_by_types (GtkTreeStore *store,
                                                 GHashTable   *geo_marks,
                                                 GHashTable   *extensions,
                                                 GHashTable   *labels)
{
  if (extensions != NULL)
    {
      GtkTreeIter parent_iter;
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanMarkGeo *object;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
      gboolean active = hyscan_model_manager_is_all_toggled (extensions, type_id[GEO_MARK]);

      /* Добавляем новый узел "Гео-метки" в модель */
      gtk_tree_store_append (store, &parent_iter, NULL);
      gtk_tree_store_set (store,              &parent_iter,
                          COLUMN_ID,           type_id[GEO_MARK],
                          COLUMN_NAME,         _(type_name[GEO_MARK]),
                          COLUMN_DESCRIPTION,  _(type_desc[GEO_MARK]),
                          COLUMN_OPERATOR,     _(author),
                          COLUMN_TOOLTIP,      _(type_name[GEO_MARK]),
                          COLUMN_TYPE,         GEO_MARK,
                          COLUMN_ICON,         icon_name[GEO_MARK],
                          COLUMN_ACTIVE,       active,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        0,
                          -1);

      g_hash_table_iter_init (&table_iter, geo_marks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL)
            {
              HyScanLabel *label;
              GtkTreeIter child_iter,
                          item_iter;
              GHashTableIter iter;
              gchar *key,
                    *icon = icon_name[GEO_MARK],
                    *position = g_strdup_printf ("%.6f° %.6f° (WGS 84)",
                                                 object->center.lat,
                                                 object->center.lon);
              gboolean toggled = active;

              if (!active)
                {
                  Extension *ext = g_hash_table_lookup (extensions, id);
                  toggled = (ext != NULL) ? ext->active : FALSE;
                }

              g_hash_table_iter_init (&iter, labels);
              while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
                {
                  if (object->labels == label->label)
                    {
                      icon = label->icon_name;
                      break;
                    }
                }

              /* Добавляем новый узел c названием гео-метки в модель */
              gtk_tree_store_append (store, &child_iter, &parent_iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
              /* Отображается. */ COLUMN_NAME,         object->name,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      object->name,
                                  COLUMN_TYPE,         GEO_MARK,
                                  COLUMN_ICON,         icon,
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);
              /* Атрибуты гео-метки. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->ctime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->mtime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Координаты. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    position,
                                  COLUMN_TOOLTIP, _("Location"),
                                  COLUMN_ICON,    "preferences-desktop-locale",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (position);
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Гео-метки" в древовидной модели с группировкой по группам. */
void
hyscan_model_manager_refresh_geo_marks_by_labels (GtkTreeStore *store,
                                                  GtkTreeIter  *iter,
                                                  HyScanLabel  *label,
                                                  GHashTable   *geo_marks,
                                                  GHashTable   *extensions)
{
  if (label != NULL)
    {
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanMarkGeo *object;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

      g_hash_table_iter_init (&table_iter, geo_marks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL && object->labels == label->label)
            {
              GtkTreeIter child_iter,
                          item_iter;
              Extension *ext = g_hash_table_lookup (extensions, id);
              gchar *str = NULL,
                    *tmp = g_strdup (_(type_name[GEO_MARK])),
                    *position = g_strdup_printf ("%.6f° %.6f° (WGS 84)",
                                                 object->center.lat,
                                                 object->center.lon);
              gboolean toggled = FALSE;

              /*str = g_strdup_printf ("%s (%s)", object->name, type_name[GEO_MARK]);*/
              tmp[g_utf8_strlen (tmp , -1) - 1] = '\0';
              str = g_strdup_printf ("%s (%s)", object->name, tmp);
              g_free (tmp);

              if (ext != NULL)
                toggled = (ext != NULL) ? ext->active : FALSE;

              /* Добавляем гео-метку в модель. */
              gtk_tree_store_append (store, &child_iter, iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
                                  COLUMN_NAME,         str,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      _(type_name[GEO_MARK]),
                                  COLUMN_TYPE,         GEO_MARK,
                                  COLUMN_ICON,         icon_name[GEO_MARK],
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);

              g_free (str);

              /* Атрибуты акустической метки. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->ctime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                   -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->mtime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Координаты . */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    position,
                                  COLUMN_TOOLTIP, _("Location"),
                                  COLUMN_ICON,    "preferences-desktop-locale",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (position);
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Акустические метки" в древовидной модели с группировкой по типам. */
void
hyscan_model_manager_refresh_acoustic_marks_by_types (GtkTreeStore *store,
                                                      GHashTable   *acoustic_marks,
                                                      GHashTable   *extensions,
                                                      GHashTable   *labels)
{
  if (extensions != NULL)
    {
      GtkTreeIter parent_iter;
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanMarkLocation *location;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
      gboolean active = hyscan_model_manager_is_all_toggled (extensions, type_id[ACOUSTIC_MARK]);

      /* Добавляем новый узел "Акустические метки" в модель. */
      gtk_tree_store_append (store, &parent_iter, NULL);
      gtk_tree_store_set (store,              &parent_iter,
                          COLUMN_ID,           type_id[ACOUSTIC_MARK],
                          COLUMN_NAME,         _(type_name[ACOUSTIC_MARK]),
                          COLUMN_DESCRIPTION,  _(type_desc[ACOUSTIC_MARK]),
                          COLUMN_OPERATOR,     _(author),
                          COLUMN_TOOLTIP,      _(type_name[ACOUSTIC_MARK]),
                          COLUMN_TYPE,         ACOUSTIC_MARK,
                          COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                          COLUMN_ACTIVE,       active,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        0,
                          -1);

      g_hash_table_iter_init (&table_iter, acoustic_marks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
        {
          HyScanMarkWaterfall *object = location->mark;

          if (object != NULL)
            {
              HyScanLabel *label;
              GtkTreeIter child_iter,
                          item_iter;
              GHashTableIter iter;
              gchar *key,
                    *icon = icon_name[ACOUSTIC_MARK],
                    *position = g_strdup_printf ("%.6f° %.6f° (WGS 84)",
                                                 location->mark_geo.lat,
                                                 location->mark_geo.lon),
                    *board,
                    *board_icon = g_strdup ("network-wireless-no-route-symbolic"),
                    *depth = g_strdup_printf (_(distance_stamp), location->depth);
              gboolean toggled = active;

              if (!active)
                {
                  Extension *ext = g_hash_table_lookup (extensions, id);
                  toggled = (ext != NULL) ? ext->active : FALSE;
                }

              g_hash_table_iter_init (&iter, labels);
              while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
                {
                  if (object->labels == label->label)
                    {
                      icon = label->icon_name;
                      break;
                    }
                }

              /* Добавляем новый узел c названием группы в модель */
              gtk_tree_store_append (store, &child_iter, &parent_iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
              /* Отображается. */ COLUMN_NAME,         object->name,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      object->name,
                                  COLUMN_TYPE,         ACOUSTIC_MARK,
                                  COLUMN_ICON,         icon,
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);
              /* Атрибуты акустической метки. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->ctime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                   -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->mtime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Координаты. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    position,
                                  COLUMN_TOOLTIP, _("Location"),
                                  COLUMN_ICON,    "preferences-desktop-locale",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (position);
              /* Название галса. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    location->track_name,
                                  COLUMN_TOOLTIP, _("Track name"),
                                  COLUMN_ICON,    icon_name[TRACK],
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Борт. */
              switch (location->direction)
                {
                case HYSCAN_MARK_LOCATION_PORT:
                  {
                    board = g_strdup (_("Port"));
                    board_icon = g_strdup ("go-previous");
                  }
                  break;
                case HYSCAN_MARK_LOCATION_STARBOARD:
                  {
                    board = g_strdup (_("Starboard"));
                    board_icon = g_strdup ("go-next");
                  }
                  break;
                case HYSCAN_MARK_LOCATION_BOTTOM:
                  {
                    board = g_strdup (_("Bottom"));
                    board_icon = g_strdup ("go-down");
                  }
                  break;
                default:
                  board = g_strdup (_(unknown));
                  break;
                }

              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    board,
                                  COLUMN_TOOLTIP, _("Board"),
                                  COLUMN_ICON,    board_icon,
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (board);
              g_free (board_icon);
              /* Глубина. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    depth,
                                  COLUMN_TOOLTIP, _("Depth"),
                                  COLUMN_ICON,    "object-flip-vertical",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (depth);
              /* Атрибуты акустических меток для левого и правого бортов. */
              if (location->direction != HYSCAN_MARK_LOCATION_BOTTOM)
                {
                  gchar *width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width),
                        *slant_range = g_strdup_printf (_(distance_stamp), location->across);
                  /* Левоый борт. */
                  if (location->direction == HYSCAN_MARK_LOCATION_PORT)
                    {
                      /* Поэтому значения отрицательные, и start и end меняются местами. */
                      gdouble across_start = -location->mark->width - location->across;
                      gdouble across_end   =  location->mark->width - location->across;
                      if (across_start < 0 && across_end > 0)
                        {
                          width = g_strdup_printf (_(distance_stamp), -across_start);
                        }
                    }
                  /* Правый борт. */
                  else
                    {
                      gdouble across_start = location->across - location->mark->width;
                      gdouble across_end   = location->across + location->mark->width;
                      if (across_start < 0 && across_end > 0)
                        {
                          width = g_strdup_printf (_(distance_stamp), across_end);
                        }
                    }
                  /* Ширина. */
                  gtk_tree_store_append (store, &item_iter, &child_iter);
                  gtk_tree_store_set (store,         &item_iter,
                  /* Отображается. */ COLUMN_NAME,    width,
                                      COLUMN_TOOLTIP, _("Width"),
                                      COLUMN_ICON,    "object-flip-horizontal",
                                      COLUMN_ACTIVE,  toggled,
                                      COLUMN_VISIBLE, FALSE,
                                      -1);
                  g_free (width);
                  /* Наклонная дальность. */
                  gtk_tree_store_append (store, &item_iter, &child_iter);
                  gtk_tree_store_set (store,         &item_iter,
                  /* Отображается. */ COLUMN_NAME,    slant_range,
                                      COLUMN_TOOLTIP, _("Slant range"),
                                      COLUMN_ICON,    "content-loading-symbolic",
                                      COLUMN_ACTIVE,  toggled,
                                      COLUMN_VISIBLE, FALSE,
                                      -1);
                  g_free (slant_range);
                }
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Акустические метки" в древовидной модели с группировкой по группам. */
void
hyscan_model_manager_refresh_acoustic_marks_by_labels (GtkTreeStore *store,
                                                       GtkTreeIter  *iter,
                                                       HyScanLabel  *label,
                                                       GHashTable   *acoustic_marks,
                                                       GHashTable   *extensions)
{
  if (label != NULL)
    {
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanMarkLocation *location;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

      g_hash_table_iter_init (&table_iter, acoustic_marks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&location))
        {
          HyScanMarkWaterfall *object = location->mark;

          if (object != NULL && object->labels == label->label)
            {
              GtkTreeIter child_iter,
                          item_iter;
              Extension *ext = g_hash_table_lookup (extensions, id);
              gchar *str = NULL,
                    *tmp = g_strdup (_(type_name[ACOUSTIC_MARK])),
                    *position = g_strdup_printf ("%.6f° %.6f° (WGS 84)",
                                                 location->mark_geo.lat,
                                                 location->mark_geo.lon),
                    *board,
                    *board_icon = g_strdup ("network-wireless-no-route-symbolic"),
                    *depth = g_strdup_printf (_(distance_stamp), location->depth);
              gboolean toggled = FALSE;

              /*str = g_strdup_printf ("%s (%s)", object->name, type_name[ACOUSTIC_MARK]);*/
              tmp[g_utf8_strlen (tmp , -1) - 1] = '\0';
              str = g_strdup_printf ("%s (%s)", object->name, tmp);
              g_free (tmp);

              if (ext != NULL)
                toggled = (ext != NULL) ? ext->active : FALSE;

              /* Добавляем акустическую метку в модель. */
              gtk_tree_store_append (store, &child_iter, iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
                                  COLUMN_NAME,         str,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      _(type_name[ACOUSTIC_MARK]),
                                  COLUMN_TYPE,         ACOUSTIC_MARK,
                                  COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);

              g_free (str);

              /* Атрибуты акустической метки. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->ctime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                   -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                                    g_date_time_new_from_unix_local (
                                                      object->mtime / G_TIME_SPAN_SECOND),
                                                    date_time_stamp),
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Координаты. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    position,
                                  COLUMN_TOOLTIP, _("Location"),
                                  COLUMN_ICON,    "preferences-desktop-locale",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (position);
              /* Название галса. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    location->track_name,
                                  COLUMN_TOOLTIP, _("Track name"),
                                  COLUMN_ICON,    icon_name[TRACK],
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Борт. */
              switch (location->direction)
                {
                case HYSCAN_MARK_LOCATION_PORT:
                  {
                    board = g_strdup (_("Port"));
                    board_icon = g_strdup ("go-previous");
                  }
                  break;
                case HYSCAN_MARK_LOCATION_STARBOARD:
                  {
                    board = g_strdup (_("Starboard"));
                    board_icon = g_strdup ("go-next");
                  }
                  break;
                case HYSCAN_MARK_LOCATION_BOTTOM:
                  {
                    board = g_strdup (_("Bottom"));
                    board_icon = g_strdup ("go-down");
                  }
                  break;
                default:
                  board = g_strdup (_(unknown));
                  break;
                }

              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    board,
                                  COLUMN_TOOLTIP, _("Board"),
                                  COLUMN_ICON,    board_icon,
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (board);
              g_free (board_icon);
              /* Глубина. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    depth,
                                  COLUMN_TOOLTIP, _("Depth"),
                                  COLUMN_ICON,    "object-flip-vertical",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              g_free (depth);
              /* Атрибуты акустических меток для левого и правого бортов. */
              if (location->direction != HYSCAN_MARK_LOCATION_BOTTOM)
                {
                  gchar *width = g_strdup_printf (_(distance_stamp), 2.0 * location->mark->width),
                        *slant_range = g_strdup_printf (_(distance_stamp), location->across);
                  /* Левоый борт. */
                  if (location->direction == HYSCAN_MARK_LOCATION_PORT)
                    {
                      /* Поэтому значения отрицательные, и start и end меняются местами. */
                      gdouble across_start = -location->mark->width - location->across;
                      gdouble across_end   =  location->mark->width - location->across;
                      if (across_start < 0 && across_end > 0)
                        {
                          width = g_strdup_printf (_(distance_stamp), -across_start);
                        }
                    }
                  /* Правый борт. */
                  else
                    {
                      gdouble across_start = location->across - location->mark->width;
                      gdouble across_end   = location->across + location->mark->width;
                      if (across_start < 0 && across_end > 0)
                        {
                          width = g_strdup_printf (_(distance_stamp), across_end);
                        }
                    }
                  /* Ширина. */
                  gtk_tree_store_append (store, &item_iter, &child_iter);
                  gtk_tree_store_set (store,         &item_iter,
                  /* Отображается. */ COLUMN_NAME,    width,
                                      COLUMN_TOOLTIP, _("Width"),
                                      COLUMN_ICON,    "object-flip-horizontal",
                                      COLUMN_ACTIVE,  toggled,
                                      COLUMN_VISIBLE, FALSE,
                                      -1);
                  g_free (width);
                  /* Наклонная дальность. */
                  gtk_tree_store_append (store, &item_iter, &child_iter);
                  gtk_tree_store_set (store,         &item_iter,
                  /* Отображается. */ COLUMN_NAME,    slant_range,
                                      COLUMN_TOOLTIP, _("Slant range"),
                                      COLUMN_ICON,    "content-loading-symbolic",
                                      COLUMN_ACTIVE,  toggled,
                                      COLUMN_VISIBLE, FALSE,
                                      -1);
                  g_free (slant_range);
                }
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Галсы" в древовидной модели с группировкой по типам. */
void
hyscan_model_manager_refresh_tracks_by_types (GtkTreeStore *store,
                                              GHashTable   *tracks,
                                              GHashTable   *extensions,
                                              GHashTable   *labels)
{
  if (extensions != NULL)
    {
      GtkTreeIter parent_iter;
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanTrackInfo *object;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
      gboolean active = hyscan_model_manager_is_all_toggled (extensions, type_id[TRACK]);

      /* Добавляем новый узел "Галсы" в модель. */
      gtk_tree_store_append (store, &parent_iter, NULL);
      gtk_tree_store_set (store,              &parent_iter,
                          COLUMN_ID,           type_id[TRACK],
                          COLUMN_NAME,         _(type_name[TRACK]),
                          COLUMN_DESCRIPTION,  _(type_desc[TRACK]),
                          COLUMN_OPERATOR,     _(author),
                          COLUMN_TOOLTIP,      _(type_name[TRACK]),
                          COLUMN_TYPE,         TRACK,
                          COLUMN_ICON,         icon_name[TRACK],
                          COLUMN_ACTIVE,       active,
                          COLUMN_VISIBLE,      TRUE,
                          COLUMN_LABEL,        0,
                          -1);

      g_hash_table_iter_init (&table_iter, tracks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL)
            {
              HyScanLabel *label;
              GtkTreeIter child_iter,
                          item_iter;
              GHashTableIter iter;
              gchar *key,
                    *icon = icon_name[TRACK];
              gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp),
                    *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);
              gboolean toggled = active;

              if (!active)
                {
                  Extension *ext = g_hash_table_lookup (extensions, id);
                  toggled = (ext != NULL) ? ext->active : FALSE;
                }

              g_hash_table_iter_init (&iter, labels);
              while (g_hash_table_iter_next (&iter, (gpointer*)&key, (gpointer*)&label))
                {
                  if (object->labels == label->label)
                    {
                      icon = label->icon_name;
                      break;
                    }
                }

              /* Добавляем новый узел c названием галса в модель */
              gtk_tree_store_append (store, &child_iter, &parent_iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
              /* Отображается. */ COLUMN_NAME,         object->name,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      object->name,
                                  COLUMN_TYPE,         TRACK,
                                  COLUMN_ICON,         icon,
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);
              /* Атрибуты группы. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    ctime,
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                   -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    mtime,
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Галсы" в древовидной модели с группировкой по группам. */
void
hyscan_model_manager_refresh_tracks_by_labels (GtkTreeStore *store,
                                               GtkTreeIter  *iter,
                                               HyScanLabel  *label,
                                               GHashTable   *tracks,
                                               GHashTable   *extensions)
{
  if (label != NULL)
    {
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      HyScanTrackInfo *object;
      gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

      g_hash_table_iter_init (&table_iter, tracks);
      while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&object))
        {
          if (object != NULL && object->labels == label->label)
            {
              GtkTreeIter child_iter,
                          item_iter;
              Extension *ext = g_hash_table_lookup (extensions, id);
              gchar *str = NULL,
                    *tmp = g_strdup (_(type_name[TRACK])),
                    *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp),
                    *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);
              gboolean toggled = FALSE;

              /*str = g_strdup_printf ("%s (%s)", object->name, type_name[TRACK]);*/
              tmp[g_utf8_strlen (tmp , -1) - 1] = '\0';
              str = g_strdup_printf ("%s (%s)", object->name, tmp);
              g_free (tmp);

              if (ext != NULL)
                toggled = (ext != NULL) ? ext->active : FALSE;

              /* Добавляем галс в модель. */
              gtk_tree_store_append (store, &child_iter, iter);
              gtk_tree_store_set (store,              &child_iter,
                                  COLUMN_ID,           id,
                                  COLUMN_NAME,         str,
                                  COLUMN_DESCRIPTION,  object->description,
                                  COLUMN_OPERATOR,     object->operator_name,
                                  COLUMN_TOOLTIP,      _(type_name[TRACK]),
                                  COLUMN_TYPE,         TRACK,
                                  COLUMN_ICON,         icon_name[TRACK],
                                  COLUMN_ACTIVE,       toggled,
                                  COLUMN_VISIBLE,      TRUE,
                                  COLUMN_LABEL,        object->labels,
                                  -1);

              g_free (str);

              /* Атрибуты галса. */
              /* Описание. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->description,
                                  COLUMN_TOOLTIP, _("Description"),
                                  COLUMN_ICON,    "accessories-text-editor",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Оператор. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    object->operator_name,
                                  COLUMN_TOOLTIP, _("Operator"),
                                  COLUMN_ICON,    "avatar-default",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
              /* Время создания. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    ctime,
                                  COLUMN_TOOLTIP, _("Creation time"),
                                  COLUMN_ICON,    "appointment-new",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                   -1);
              /* Время модификации. */
              gtk_tree_store_append (store, &item_iter, &child_iter);
              gtk_tree_store_set (store,         &item_iter,
              /* Отображается. */ COLUMN_NAME,    mtime,
                                  COLUMN_TOOLTIP, _("Modification time"),
                                  COLUMN_ICON,    "document-open-recent",
                                  COLUMN_ACTIVE,  toggled,
                                  COLUMN_VISIBLE, FALSE,
                                  -1);
            }
        }
    }
}

/* Очищает модель от содержимого. */
void
hyscan_model_manager_clear_view_model (GtkTreeModel *view_model,
                                       gboolean     *flag)
{
  /* Защита от срабатывания срабатывания сигнала "changed"
   * у GtkTreeSelection при очистке GtkTreeModel.
   * */
  *flag = TRUE;

  if (GTK_IS_LIST_STORE (view_model))
    {
      gtk_list_store_clear (GTK_LIST_STORE (view_model));
    }
  else if (GTK_IS_TREE_STORE (view_model))
    {
      gtk_tree_store_clear (GTK_TREE_STORE (view_model));
    }
  else
    {
      g_warning ("Unknown type of store in HyScanMarkManagerView.\n");
    }
  /* Защита от срабатывания срабатывания сигнала "changed"
   * у GtkTreeSelection при очистке GtkTreeModel.
   * */
  *flag = FALSE;
}

/* Инициализирует массив с данными об объектах данными из моделей. */
gboolean
hyscan_model_manager_init_extensions (HyScanModelManager  *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;
  gint counter = 0;

  for (type = LABEL; type < TYPES; type++)
    {
      GHashTable *tmp;
      /* Создаём пустую таблицу. */
      if (priv->extensions[type] == NULL)
        {
          priv->extensions[type] = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                          (GDestroyNotify)hyscan_model_manager_extension_free);
        }
      /* Сохраняем указатель на таблицу со старыми данными. */
      tmp = priv->extensions[type];
      /* Заполняем таблицу новыми данными. */
      priv->extensions[type] = hyscan_model_manager_get_extensions (self, type);

      if (priv->extensions[type] != NULL)
        {
          /* Начальная инициализация узлов для древовидного представления с группировкой по типам.*/
          if (priv->node[type] == NULL)
            {
              priv->node[type] = hyscan_model_manager_extension_new (PARENT, FALSE, FALSE);
            }
          else
            {
              Extension *ext = g_hash_table_lookup (tmp, type_id[type]);

              if (ext != NULL)
                {
                  priv->node[type] = hyscan_model_manager_extension_copy (ext);
                }
            }

          /* Добавляем в общую таблицу с соответствующим идентификатором. */
          g_hash_table_insert (priv->extensions[type], g_strdup (type_id[type]), priv->node[type]);
        }
      counter++;
      /* Удаляем таблицу со старыми данными. */
      if (tmp != NULL)
        g_hash_table_destroy (tmp);
    }

  return (gboolean)counter;
}

/* Обновляет ВСЕ ОБЪЕКТЫ для древовидного списка с группировкой по типам. */
void
hyscan_model_manager_refresh_all_items_by_types (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  /*for (type = LABEL; type < TYPES; type++)*/
  for (type = GEO_MARK; type < TYPES; type++)
    {
      if (priv->extensions[type] != NULL)
        {
          hyscan_model_manager_refresh_items_by_types (self, type);
        }
    }
}

/* Обновляет ВСЕ ОБЪЕКТЫ для древовидного списка с группировкой по группам. */
void
hyscan_model_manager_refresh_all_items_by_labels (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTableIter table_iter;
  GHashTable *labels = hyscan_object_model_get (priv->label_model);

  HyScanLabel *label;
  gchar *id;

  g_hash_table_iter_init (&table_iter, labels);
  while (g_hash_table_iter_next (&table_iter, (gpointer*)&id, (gpointer*)&label))
    {
      if (label != NULL)
        {
          GtkTreeIter iter,
                      child_iter;
          GtkTreeStore *store = GTK_TREE_STORE (priv->view_model);
          GHashTable *acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model),
                     *geo_marks      = hyscan_object_model_get (priv->geo_mark_model),
                     *tracks         = hyscan_db_info_get_tracks (priv->track_model);
          Extension *ext = g_hash_table_lookup (priv->extensions[LABEL], id);
          gboolean toggled = FALSE;

          if (ext != NULL)
            toggled = (ext != NULL) ? ext->active : FALSE;

          /* Добавляем новый узел c названием группы в модель */
          gtk_tree_store_append (store, &iter, NULL);
          gtk_tree_store_set (store,              &iter,
                              COLUMN_ID,           id,
          /* Отображается. */ COLUMN_NAME,         label->name,
                              COLUMN_DESCRIPTION,  label->description,
                              COLUMN_OPERATOR,     label->operator_name,
                              COLUMN_TOOLTIP,      label->name,
                              COLUMN_TYPE,         LABEL,
                              COLUMN_ICON,         label->icon_name,
                              COLUMN_ACTIVE,       toggled,
                              COLUMN_VISIBLE,      TRUE,
                              COLUMN_LABEL,        label->label,
                              -1);
          /* Гео-метки. */
          hyscan_model_manager_refresh_geo_marks_by_labels (store,
                                                            &iter,
                                                            label,
                                                            geo_marks,
                                                            priv->extensions[GEO_MARK]);
          g_hash_table_unref (geo_marks);
          /* Акустические метки. */
          hyscan_model_manager_refresh_acoustic_marks_by_labels (store,
                                                                 &iter,
                                                                 label,
                                                                 acoustic_marks,
                                                                 priv->extensions[ACOUSTIC_MARK]);
          g_hash_table_unref (acoustic_marks);
          /* Галсы. */
          hyscan_model_manager_refresh_tracks_by_labels (store,
                                                         &iter,
                                                         label,
                                                         tracks,
                                                         priv->extensions[TRACK]);
          g_hash_table_unref (tracks);
          /* Проверяем не отмечены ли все дочерние объекты. */
          if (gtk_tree_model_iter_children (priv->view_model, &child_iter, &iter))
            {
              gint total   = gtk_tree_model_iter_n_children  (priv->view_model, &iter),
                   counter = 0;
              gboolean flag;

              do
                {
                  gtk_tree_model_get (priv->view_model, &child_iter,
                                      COLUMN_ACTIVE,    &flag,
                                      -1);
                  if (flag)
                    counter++; /* Считаем отмеченые. */
                }
              while (gtk_tree_model_iter_next (priv->view_model, &child_iter));

              if (counter == total)
                flag = TRUE; /* Все дочерние отмечены. */
              else
                flag = FALSE;

              gtk_tree_store_set(store, &iter, COLUMN_ACTIVE, flag, -1);
            }
        }
    }
  g_hash_table_destroy (labels);
}

/* Обновляет объекты заданного типа данными из модели для древовидного списка с группировкой по типам. */
void
hyscan_model_manager_refresh_items_by_types (HyScanModelManager     *self,
                                             ModelManagerObjectType  type)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GtkTreeStore *store = GTK_TREE_STORE (priv->view_model);
  GHashTable   *extensions = priv->extensions[type];

  switch (type)
    {
    case LABEL:
      {
        GHashTable *labels = hyscan_object_model_get (priv->label_model);
        hyscan_model_manager_refresh_labels_by_types (store, labels, extensions);
        g_hash_table_destroy (labels);
      }
      break;
    case GEO_MARK:
      {
        GHashTable *geo_marks = hyscan_object_model_get (priv->geo_mark_model),
                   *labels = hyscan_object_model_get (priv->label_model);
        hyscan_model_manager_refresh_geo_marks_by_types (store, geo_marks, extensions, labels);
        g_hash_table_destroy (labels);
        g_hash_table_destroy (geo_marks);
      }
      break;
    case ACOUSTIC_MARK:
      {
        GHashTable *acoustic_marks = hyscan_mark_loc_model_get (priv->acoustic_loc_model),
                   *labels = hyscan_object_model_get (priv->label_model);
        hyscan_model_manager_refresh_acoustic_marks_by_types (store, acoustic_marks, extensions, labels);
        g_hash_table_destroy (labels);
        g_hash_table_destroy (acoustic_marks);
      }
      break;
    case TRACK:
      {
        GHashTable *tracks = hyscan_db_info_get_tracks (priv->track_model),
                   *labels = hyscan_object_model_get (priv->label_model);
        hyscan_model_manager_refresh_tracks_by_types (store, tracks, extensions, labels);
        g_hash_table_destroy (labels);
        g_hash_table_destroy (tracks);
      }
      break;
    default: break;
    }
}

/* Возвращает указатель на хэш-таблицу с данными в соответствии
 * с типом запрашиваемых объектов. Когда хэш-таблица больше не нужна,
 * необходимо использовать #g_hash_table_unref ().
 * */
GHashTable*
hyscan_model_manager_get_extensions (HyScanModelManager     *self,
                                     ModelManagerObjectType  type)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTable *table = NULL;

  switch (type)
    {
    case LABEL:
      {
        table = hyscan_object_model_get (priv->label_model);
      }
      break;
    case GEO_MARK:
      {
        table = hyscan_object_model_get (priv->geo_mark_model);
      }
      break;
    case ACOUSTIC_MARK:
      {
        table = hyscan_mark_loc_model_get (priv->acoustic_loc_model);
      }
      break;
    case TRACK:
      {
        table = hyscan_db_info_get_tracks (priv->track_model);
      }
      break;
    default: break;
    }

  if (table != NULL)
    {
      GHashTable *extensions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                 (GDestroyNotify)hyscan_model_manager_extension_free);
      GHashTableIter  iter;
      gpointer object;
      gchar *id;

      g_hash_table_iter_init (&iter, table);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, &object))
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[type], id);

          if (ext == NULL)
            {
              ext = hyscan_model_manager_extension_new (CHILD, FALSE, FALSE);
              g_hash_table_insert (extensions, g_strdup (id), ext);
            }
          else
            {
              ext = hyscan_model_manager_extension_copy (ext);
              g_hash_table_insert (extensions, g_strdup (id), ext);
            }
        }

      g_hash_table_destroy (table);
      table = extensions;
    }

  return table;
}

/* Возвращает TRUE, если ВСЕ ОБЪЕКТЫ Extention в хэш-таблице имеют поля active = TRUE.
 * В противном случае, возвращает FALSE.
 * */
gboolean
hyscan_model_manager_is_all_toggled (GHashTable  *table,
                                     const gchar *node_id)
{
  GHashTableIter iter;
  Extension *ext;
  gchar     *id;
  guint total   = g_hash_table_size (table),
        counter = 0;

  if (total > 1)
    {
      g_hash_table_iter_init (&iter, table);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
        {
          if (0 == g_strcmp0 (id, node_id))
            {
              counter++;
            }
          if (ext->active)
            counter++;
        }

      if (counter >= total)
        {
          ext = g_hash_table_lookup (table, node_id);
          if (ext != NULL)
            {
              ext->active = TRUE;
            }
          return TRUE;
        }
    }

  return FALSE;
}

/* Удаляет объект из базы данных. */
void
hyscan_model_manager_delete_item (HyScanModelManager     *self,
                                  ModelManagerObjectType  type,
                                  gchar                  *id)
{
  HyScanModelManagerPrivate *priv = self->priv;

  switch (type)
    {
    case LABEL:
      {
        /* Удаляем группу. */
        hyscan_object_model_remove_object (priv->label_model, id);
      }
      break;
    case GEO_MARK:
      {
        /* Удаляем гео-метку. */
        hyscan_object_model_remove_object (priv->geo_mark_model, id);
      }
      break;
    case ACOUSTIC_MARK:
      {
        /* Удаляем акустическую метку. */
        hyscan_object_model_remove_object (priv->acoustic_marks_model, id);
      }
      break;
    case TRACK:
      {
        /* Удаляем галс. */

        /* Получаем идентификатор проекта. */
        gint32 project_id = hyscan_db_project_open (priv->db, priv->project_name);

        if (project_id >= 0)
          {
            hyscan_db_track_remove (priv->db,
                                    project_id,
                                    id);
          }
      }
      break;
    default: break;
    }
}

/* Переносит гео-метку в другую группу. */
void
hyscan_model_manager_geo_mark_change_label (HyScanModelManager  *self,
                                            gchar              **list,
                                            HyScanLabel         *label,
                                            GHashTable          *table,
                                            guint64              current_time)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->signal_geo_marks_changed != 0)
    {
      gint i;
      /* Отключаем сигнал. */
      g_signal_handler_block (priv->geo_mark_model, priv->signal_geo_marks_changed);

      for (i = 0; list[i] != NULL; i++)
        {
          HyScanMarkGeo *object = NULL;
          /* Получаем объект из базы данных по идентификатору. */
          object = (HyScanMarkGeo*)hyscan_object_model_get_id (priv->geo_mark_model, list[i]);
          if (object != NULL)
            {
              GHashTableIter iter;
              HyScanLabel *lbl;
              gchar *tmp;

              g_hash_table_iter_init (&iter, table);
              while (g_hash_table_iter_next (&iter, (gpointer*)&tmp, (gpointer*)&lbl))
                {
                  if (object->labels == lbl->label)
                    {
                      /* Устанавливаем время изменения для группы. */
                      lbl->mtime = current_time;
                      /* Сохраняем измения в базе данных. */
                      hyscan_object_model_modify_object (priv->label_model, tmp, (const HyScanObject*)lbl);
                      break;
                    }
                }
              /* Заменяем группу полученому объекту. */
              object->labels = label->label;
              /* Устанавливаем время изменения. */
              object->mtime  = G_TIME_SPAN_SECOND * current_time;
              /* Сохраняем измения в базе данных. */
              hyscan_object_model_modify_object (priv->geo_mark_model,
                                                 list[i],
                                                 (const HyScanObject*)object);
              /* Освобождаем полученный из базы данных объект. */
              hyscan_mark_geo_free (object);
            }
        }
      /* Включаем сигнал. */
      g_signal_handler_unblock (priv->geo_mark_model, priv->signal_geo_marks_changed);
    }
}

/* Переносит акустическую метку в другую группу */
void
hyscan_model_manager_acoustic_mark_change_label (HyScanModelManager  *self,
                                                 gchar              **list,
                                                 HyScanLabel         *label,
                                                 GHashTable          *table,
                                                 guint64              current_time)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->signal_acoustic_marks_changed != 0)
    {
      gint i;
      /* Отключаем сигнал. */
      g_signal_handler_block (priv->acoustic_marks_model, priv->signal_acoustic_marks_changed);

      for (i = 0; list[i] != NULL; i++)
        {
          HyScanMarkWaterfall *object = NULL;
          /* Получаем объект из базы данных по идентификатору. */
          object = (HyScanMarkWaterfall*)hyscan_object_model_get_id (priv->acoustic_marks_model, list[i]);
          if (object != NULL)
            {
              GHashTableIter iter;
              HyScanLabel *lbl;
              gchar *tmp;

              g_hash_table_iter_init (&iter, table);
              while (g_hash_table_iter_next (&iter, (gpointer*)&tmp, (gpointer*)&lbl))
                {
                  if (object->labels == lbl->label)
                    {
                      /* Устанавливаем время изменения для группы. */
                      lbl->mtime = current_time;
                      /* Сохраняем измения в базе данных. */
                      hyscan_object_model_modify_object (priv->label_model, tmp, (const HyScanObject*)lbl);
                      break;
                    }
                }
              /* Заменяем группу полученому объекту. */
              object->labels = label->label;
              /* Устанавливаем время изменения. */
              object->mtime  = G_TIME_SPAN_SECOND * current_time;
              /* Сохраняем измения в базе данных. */
              hyscan_object_model_modify_object (priv->acoustic_marks_model,
                                                 list[i],
                                                 (const HyScanObject*)object);
              /* Освобождаем полученный из базы данных объект. */
              hyscan_mark_waterfall_free (object);
            }
        }
      /* Включаем сигнал. */
      g_signal_handler_unblock (priv->acoustic_marks_model, priv->signal_acoustic_marks_changed);
    }
}

/* Переносит галс в другую группу */
void
hyscan_model_manager_track_change_label (HyScanModelManager  *self,
                                         gchar              **list,
                                         HyScanLabel         *label,
                                         GHashTable          *table,
                                         GDateTime           *now_local,
                                         guint64              current_time)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->signal_tracks_changed != 0)
    {
      gint i;
      /* Отключаем сигнал. */
      g_signal_handler_block (priv->track_model, priv->signal_tracks_changed);

      for (i = 0; list[i] != NULL; i++)
        {
          gint32 project_id = hyscan_db_project_open (priv->db, priv->project_name);
          /* Получаем объект из базы данных по идентификатору. */
          HyScanTrackInfo *object = hyscan_db_info_get_track_info (priv->db, project_id, list[i]);

          if (object != NULL)
            {
              GHashTableIter iter;
              HyScanLabel *lbl;
              gchar *tmp;

              g_hash_table_iter_init (&iter, table);
              while (g_hash_table_iter_next (&iter, (gpointer*)&tmp, (gpointer*)&lbl))
                {
                  if (object->labels == lbl->label)
                    {
                      /* Устанавливаем время изменения для группы. */
                      lbl->mtime = current_time;
                      /* Сохраняем измения в базе данных. */
                      hyscan_object_model_modify_object (priv->label_model, tmp, (const HyScanObject*)lbl);
                      break;
                   }
                }
              /* Заменяем группу полученому объекту. */
              object->labels = label->label;
              /* Устанавливаем время изменения. */
              object->mtime  = now_local;
              /* Сохраняем измения в базе данных. */
              hyscan_db_info_modify_track_info (priv->track_model, object);
              /* Освобождаем полученный из базы данных объект. */
              hyscan_db_info_track_info_free (object);
            }
        }
      /* Включаем сигнал. */
      g_signal_handler_unblock (priv->track_model, priv->signal_tracks_changed);
    }
}
/* Создаёт новый Extention. Для удаления необходимо
 * использовать hyscan_model_manager_extension_free ().*/
Extension*
hyscan_model_manager_extension_new (ExtensionType  type,
                                    gboolean       active,
                                    gboolean       expanded)
{
  Extension *ext = g_new (Extension, 1);
  ext->type      = type;
  ext->active    = active;
  ext->expanded  = expanded;
  return ext;
}

/* Создаёт копию Extention-а. Для удаления необходимо использовать hyscan_model_manager_extension_free ().*/
Extension*
hyscan_model_manager_extension_copy (Extension *ext)
{
  Extension *copy = g_new (Extension, 1);
  copy->type      = ext->type;
  copy->active    = ext->active;
  copy->expanded  = ext->expanded;
  return copy;
}

/* Освобождает ресурсы Extention-а */
void
hyscan_model_manager_extension_free (gpointer data)
{
  if (data != NULL)
    {
      Extension *ext = (Extension*)data;
      ext->type      = PARENT;
      ext->active    =
      ext->expanded  = FALSE;
    }
}

/**
 * hyscan_model_manager_new:
 * @project_name: название проекта
 * @db: указатель на базу данных
 * @cache: указатель на кэш
 * @export_folder: папка для экспорта
 *
 * Returns: cоздаёт новый объект #HyScanModelManager
 */
HyScanModelManager*
hyscan_model_manager_new (const gchar *project_name,
                          HyScanDB    *db,
                          HyScanCache *cache,
                          gchar       *export_folder)
{
  return g_object_new (HYSCAN_TYPE_MODEL_MANAGER,
                       "project_name",  project_name,
                       "db",            db,
                       "cache",         cache,
                       "export_folder", export_folder,
                       NULL);
}

/**
 * hyscan_model_manager_get_track_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель галсов. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanDBInfo*
hyscan_model_manager_get_track_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->track_model);
}

/**
 * hyscan_model_manager_get_acoustic_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель акустических меток. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_acoustic_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_marks_model);
}

/**
 * hyscan_model_manager_get_geo_mark_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель гео-меток. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_geo_mark_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->geo_mark_model);
}

/**
 * hyscan_model_manager_get_label_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель групп. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanObjectModel*
hyscan_model_manager_get_label_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->label_model);
}

/**
 * hyscan_model_manager_get_acoustic_mark_loc_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на модель акустических меток с координатами. Когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanMarkLocModel*
hyscan_model_manager_get_acoustic_mark_loc_model (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->acoustic_loc_model);
}

/**
 * hyscan_model_manager_get_signal_title:
 * @self: указатель на Менеджер Моделей
 * signal: сигнал Менеджера Моделей
 *
 * Returns: название сигнала Менеджера Моделей
 */
const gchar*
hyscan_model_manager_get_signal_title (HyScanModelManager *self,
                                       ModelManagerSignal  signal)
{
  return signals[signal];
}

/**
 * hyscan-model-manager-get-db:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на Базу Данных. Когда БД больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
HyScanDB*
hyscan_model_manager_get_db (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->db);
}

/**
 * hyscan_model_manager_set_project_name:
 * @self: указатель на Менеджер Моделей
 * @project_name: название проекта
 *
 * Устанавливает название проекта и отправляет сигнал "notify:project-name".
 */
void
hyscan_model_manager_set_project_name (HyScanModelManager *self,
                                       const gchar        *project_name)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (0 != g_strcmp0 (priv->project_name, project_name))
    {
      g_free (priv->project_name);

      priv->project_name = g_strdup (project_name);
      /* Обновляем имя проекта для всех моделей. */
      if (priv->track_model != NULL)
        hyscan_db_info_set_project (priv->track_model, priv->project_name);
      if (priv->acoustic_loc_model != NULL)
        hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);
      if (priv->acoustic_marks_model != NULL)
        hyscan_object_model_set_project (priv->acoustic_marks_model, priv->db, priv->project_name);
      if (priv->geo_mark_model)
        hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);
      if (priv->label_model != NULL)
        hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
      /* Отправляем сигнал об изменении названия проекта. */
      g_object_notify_by_pspec (G_OBJECT (self), notify);
    }
}

/**
 * hyscan_model_manager_get_project_name:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: название проекта
 */
const gchar*
hyscan_model_manager_get_project_name (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->project_name;
}

/**
 * hyscan_model_manager_get_export_folder:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: директорию для экспорта
 */
const gchar*
hyscan_model_manager_get_export_folder (HyScanModelManager     *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->export_folder;
}

/**
 * hyscan-model-manager-get-cache:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на кэш. Когда кэш больше не нужен,
 * необходимо использовать #g_object_unref ().
 */
HyScanCache*
hyscan_model_manager_get_cache (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->cache);
}

/**
 * hyscan_model_manager_get_all_tracks_id:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: NULL-терминированный список идентификаторов всех галсов.
 * Когда список больше не нужен, необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_model_manager_get_all_tracks_id (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  GHashTable *tracks;
  guint count; /* Количество галсов. */
  gchar **list = NULL;

  tracks = hyscan_db_info_get_tracks (priv->track_model);
  count = g_hash_table_size (tracks);

  if (count > 0)
    {
      HyScanTrackInfo *object;
      GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
      gchar  *id;                      /* Идентификатор для обхода хэш-таблиц (ключ). */
      gint i = 0;

      list = g_new0 (gchar*, count + 1);
      g_hash_table_iter_init (&table_iter, tracks);
      while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
        {
          list[i++] = g_strdup (id);
        }
    }

  g_hash_table_destroy (tracks);

  return list;
}

/**
 * hyscan_model_manager_set_grouping:
 * @self: указатель на Менеджер Моделей
 * @grouping: тип группировки
 *
 * Устанавливает тип группировки и отправляет сигнал об изменении типа группировки
 */
void
hyscan_model_manager_set_grouping (HyScanModelManager   *self,
                                   ModelManagerGrouping  grouping)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->grouping != grouping)
    {
      priv->grouping = grouping;
      hyscan_model_manager_update_view_model (self);

      g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GROUPING_CHANGED ], 0);
    }
}

/**
 * hyscan_model_manager_get_grouping:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: действительный тип группировки
 */
ModelManagerGrouping
hyscan_model_manager_get_grouping (HyScanModelManager   *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->grouping;
}

/**
 * hyscan_model_manager_get_view_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: модель представления данных для #HyScanMarkManager.
 * Когда модель больше не нужна, необходимо использовать #g_object_unref ().
 */
GtkTreeModel*
hyscan_model_manager_get_view_model (HyScanModelManager   *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->view_model);
}

/**
 * hyscan_model_manager_set_selected_item:
 * @self: указатель на Менеджер Моделей
 * @selection: указатель на выделенные объекты
 *
 * Устанавливает выделенную строку.
 */
void
hyscan_model_manager_set_selected_item (HyScanModelManager *self,
                                        gchar              *id)
{
  HyScanModelManagerPrivate *priv = self->priv;

  if (priv->clear_model_flag)  /* Защита против срабатывания сигнала "changed" */
    return;                    /* у GtkTreeSelection при очистке GtkTreeModel.   */

  if (priv->selected_item_id != NULL)
    {
      g_free (priv->selected_item_id);
    }
  priv->selected_item_id = g_strdup (id);

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_SELECTED], 0);

}

/**
 * hyscan_model_manager_get_selected_item:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: идентификатор выделенного объекта.
 */
gchar*
hyscan_model_manager_get_selected_item (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->selected_item_id;
}

/**
 * hyscan_model_manager_get_selected_track:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: идентификатор галса в базе данных или NULL, если нет выделенного галса.
 * когда идентификатор больше не нужен, необходимо использовать #g_free ().
 */
gchar*
hyscan_model_manager_get_selected_track (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTable *tracks = hyscan_db_info_get_tracks (priv->track_model);
  HyScanTrackInfo *info = g_hash_table_lookup (tracks, priv->selected_item_id);
  gchar *result = NULL;
  if (info != NULL)
    {
       result = g_strdup (info->name);
    }
  return result;
}

/**
 * hyscan_model_manager_unselect:
 * @self: указатель на Менеджер Моделей
 *
 * Отправляет сигнал о снятии выделния.
 */
void
hyscan_model_manager_unselect_all (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  if (priv->selected_item_id != NULL)
    {
      /* Сворачиваем узел. */
      for (type = LABEL; type < TYPES; type++)
        {
          if (priv->extensions[type] != NULL)
            {
              Extension *ext = g_hash_table_lookup (priv->extensions[type], priv->selected_item_id);

              if (ext != NULL)
                {
                  ext->expanded = FALSE;
                  break;
                }
            }
        }
      /* Нет выделеного объекта. */
      g_free (priv->selected_item_id);
      priv->selected_item_id = NULL;
    }
  /* Отправляем сингнал о снятии выделения. */
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_UNSELECT_ALL], 0);
}

/**
 * hyscan_model_manager_set_horizontal:
 * @self: указатель на Менеджер Моделей
 * @value: положение горизонтальной полосы прокрутки
 *
 * Устанавливает положение горизонтальной полосы прокрутки представления.
 */
void
hyscan_model_manager_set_horizontal_adjustment (HyScanModelManager *self,
                                                gdouble             value)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->horizontal = value;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_HORIZONTAL], 0);
}

/**
 * hyscan_model_manager_set_vertical:
 * @self: указатель на Менеджер Моделей
 * @value: положение вертикальной полосы прокрутки
 *
 * Устанавливает положение вертикальной полосы прокрутки представления.
 */
void
hyscan_model_manager_set_vertical_adjustment (HyScanModelManager *self,
                                              gdouble             value)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->vertical = value;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_VERTICAL], 0);
}

/* hyscan_model_manager_get_horizontal_adjustment:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на параметры горизонтальной полосы прокрутки.
 */
gdouble
hyscan_model_manager_get_horizontal_adjustment (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->horizontal;
}

/* hyscan_model_manager_get_vertical_adjustment:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на параметры вертикальной полосы прокрутки.
 */
gdouble
hyscan_model_manager_get_vertical_adjustment (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->vertical;
}

/**
 * hyscan_model_manager_toggle_item:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор объекта в базе данных
 *
 * Переключает состояние чек-бокса.
 */
void
hyscan_model_manager_toggle_item (HyScanModelManager *self,
                                  gchar              *id,
                                  gboolean            active)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      if (priv->extensions[type] != NULL && id != NULL)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[type], id);

          if (ext != NULL)
            {
              ext->active = active;
              break;
            }
        }
    }

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_TOGGLED], 0);
}

/**
 * hyscan_model_manager_get_toggled_items:
 * @self: указатель на Менеджер Моделей
 * @type: тип запрашиваемых объектов
 *
 * Returns: возвращает список идентификаторов объектов
 * с активированным чек-боксом. Тип объекта определяется
 * #ModelManagerObjectType. Когда список больше не нужен,
 * необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_model_manager_get_toggled_items (HyScanModelManager     *self,
                                        ModelManagerObjectType  type)
{
  HyScanModelManagerPrivate *priv = self->priv;
  Extension *ext;
  GHashTableIter iter;
  guint i = 0;
  gchar **list = NULL,
         *id   = NULL;

  list = g_new0 (gchar*, 1);

  g_hash_table_iter_init (&iter, priv->extensions[type]);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (ext->active)
        {
          list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
          list[i++] = g_strdup (id);
        }
    }
  list[i] = NULL;
  return list;
}

/**
 * hyscan_model_manager_expand_item:
 * @self: указатель на Менеджер Моделей
 * @id: идентификатор объекта в базе данных
 * @expanded: состояние узла TRUE - развёрнут, FALSE - свёрнут.
 *
 * Сохраняет состояние узла.
 */
void
hyscan_model_manager_expand_item (HyScanModelManager *self,
                                  gchar              *id,
                                  gboolean            expanded)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      if (priv->extensions[type] != NULL && id != NULL)
        {
          Extension *ext = g_hash_table_lookup (priv->extensions[type], id);

          if (ext != NULL)
            {
              ext->expanded = expanded;
              break;
            }
        }
    }

  if (priv->current_id != NULL)
    g_free (priv->current_id);
  priv->current_id = g_strdup (id);

  if (expanded)
    g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_EXPANDED], 0);
  else
    g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_COLLAPSED], 0);
}

/**
 * hyscan_model_manager_get_expanded_items:
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
hyscan_model_manager_get_expanded_items (HyScanModelManager     *self,
                                         ModelManagerObjectType  type,
                                         gboolean                expanded)
{
  HyScanModelManagerPrivate *priv = self->priv;
  Extension *ext;
  GHashTableIter iter;
  guint i = 0;
  gchar **list = NULL,
         *id   = NULL;

  list = g_new0 (gchar*, 1);

  g_hash_table_iter_init (&iter, priv->extensions[type]);
  while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
    {
      if (ext->expanded == expanded)
        {
          list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
          list[i++] = g_strdup (id);
        }
    }
  list[i] = NULL;
  return list;
}
/**
 * hyscan_model_manager_get_current_id:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: возвращает идентификатор текущего объекта в виде строки.
 */
gchar*
hyscan_model_manager_get_current_id (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->current_id;
}

/**
 * hyscan_model_manager_delete_item:
 * @self: указатель на Менеджер Моделей
 *
 * Удаляет выбранные объекты из базы данных.
 */
void
hyscan_model_manager_delete_toggled_items (HyScanModelManager *self)
{
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      gchar **list = hyscan_model_manager_get_toggled_items (self, type);

      if (list != NULL)
        {
          gint i;
          for (i = 0; list[i] != NULL; i++)
            {
              /* Удаляем выделенные объекты. */
              hyscan_model_manager_delete_item (self, type, list[i]);
            }
          g_strfreev (list);
        }
    }
}

/**
 * hyscan_model_manager_has_toggled:
 * @self: указатель на Менеджер Моделей
 *
 * Проверка, есть ли выбранные объекты.
 *
 * Returns: TRUE  - есть выбранные объекты,
 *          FALSE - нет выбранных объектов.
 */
gboolean
hyscan_model_manager_has_toggled (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      GHashTableIter iter;
      Extension *ext;
      gchar *id;

      g_hash_table_iter_init (&iter, priv->extensions[type]);
      while (g_hash_table_iter_next (&iter, (gpointer*)&id, (gpointer*)&ext))
        {
          if (ext->active == TRUE)
            {
              return TRUE;
            }
        }
    }
  return FALSE;
}

/**
 * hyscan_model_manager_toggled_iteml_change_label:
 * @self: указатель на Менеджер Моделей
 * @id: группа в которую нужно перенести выделенные объекты.
 *
 * Присваивает выделенным объектам заданную группу и обновляет дату и время сделанных изменений.
 */
void
hyscan_model_manager_toggled_iteml_change_label (HyScanModelManager *self,
                                                 gchar              *id)
{
  HyScanModelManagerPrivate *priv = self->priv;
  ModelManagerObjectType  type;
  HyScanLabel *label = (HyScanLabel*)hyscan_object_model_get_id (priv->label_model, id);
  GHashTable *table = hyscan_object_model_get (priv->label_model);
  /* Текущие дата и время. */
  GDateTime *now_local = g_date_time_new_now_local ();
  guint64 current_time = g_date_time_to_unix (now_local);

  /* Отключаем сигнал. */
  if (priv->signal_labels_changed != 0)
    g_signal_handler_block (priv->label_model, priv->signal_labels_changed);

  for (type = GEO_MARK; type < TYPES; type++)
    {
      gchar** list = hyscan_model_manager_get_toggled_items (self, type);

      if (list != NULL)
        {
          /* Разворачиваем родительский узел. */
          if (priv->grouping == BY_LABELS)
            {
              hyscan_model_manager_expand_item (self, id, TRUE);
            }

          switch (type)
            {
            case GEO_MARK:
              hyscan_model_manager_geo_mark_change_label (self, list, label, table, current_time);
              break;
            case ACOUSTIC_MARK:
              hyscan_model_manager_acoustic_mark_change_label (self, list, label, table, current_time);
              break;
            case TRACK:
              hyscan_model_manager_track_change_label (self, list, label, table, now_local, current_time);
              break;
            default: break;
            }
          g_strfreev (list);
        }
    }
  /* Удаляем хэш-таблицу с группами. */
  g_hash_table_destroy (table);
  /* Устанавливаем время изменения для группы. */
  label->mtime = current_time;
  /* Сохраняем измения в базе данных. */
  hyscan_object_model_modify_object (priv->label_model, id, (const HyScanObject*)label);
  /* Освобождаем полученный из базы данных объект. */
  hyscan_label_free (label);
  /* Включаем сигнал. */
  if (priv->signal_labels_changed != 0)
    g_signal_handler_unblock (priv->label_model, priv->signal_labels_changed);

  hyscan_model_manager_update_view_model (self);
}

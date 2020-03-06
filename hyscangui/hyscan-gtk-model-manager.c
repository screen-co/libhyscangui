/*
 * hyscan-gtk-model-manager.c
 *
 *  Created on: 12 фев. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */

#include <hyscan-gtk-model-manager.h>

enum
{
  PROP_PROJECT_NAME = 1,    /* Название проекта. */
  PROP_DB,                  /* База данных. */
  PROP_CACHE,               /* Кэш.*/
  N_PROPERTIES
};

/* Типы записей в модели. */
enum
{
  PARENT, /* Узел. */
  CHILD,  /* Объект. */
  ITEM    /* Атрибут объекта. */
};

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
  gchar                *project_name;         /* Название проекта. */

  GtkTreeSelection     *selection;            /* Выделенные объекты. */
  GtkAdjustment        *horizontal,           /* Положение горизонтальной полосы прокрутки. */
                       *vertical;             /* Положение вертикальной полосы прокрутки. */

  GList                *selected[TYPES],      /* Выделенные объекты. */
                       *checked[TYPES];       /* Отмеченные чек-боксами. */

  ModelManagerGrouping  grouping;             /* Тип группировки. */
  gboolean              expand_nodes_mode,    /* Развернуть/свернуть все узлы. */
                        clear_model_flag;     /* Флаг очистки модели. */
};

/* Названия сигналов.
 * Должны идти в порядке соответвующем ModelManagerSignal
 * */
static const gchar *signals[] = {"wf-marks-changed",     /* Изменение данных в модели "водопадных" меток. */
                                 "geo-marks-changed",    /* Изменение данных в модели гео-меток. */
                                 "wf-marks-loc-changed", /* Изменение данных в модели "водопадных" меток
                                                          * с координатами. */
                                 "labels-changed",       /* Изменение данных в модели групп. */
                                 "tracks-changed",       /* Изменение данных в модели галсов. */
                                 "grouping-changed",     /* Изменение типа группировки. */
                                 "expand-mode-changed",  /* Изменение режима отображения узлов. */
                                 "view-model-updated",   /* Обновление модели представления данных. */
                                 "item-selected",        /* Выделена строка. */
                                 "scrolled-horizontal",  /* Изменение положения горизонтальной прокрутки
                                                          * представления. */
                                 "scrolled-vertical"};   /* Изменение положения вертикальной прокрутки
                                                          * представления. */

/* Форматированная строка для вывода времени и даты. */
static gchar *date_time_stamp = "%d.%m.%Y %H:%M:%S";

/* Оператор создавший типы объектов в древовидном списке. */
static gchar *author = "Default";

/* Стандартные картинки для типов объектов. */
static gchar *icon_name[] = {"emblem-documents",            /* Группы. */
                             "mark-location",               /* Гео-метки. */
                             "emblem-photos",               /* Акустические метки. */
                             "preferences-system-sharing"}; /* Галсы. */
/* Названия типов. */
static gchar *type_name[] = {"Labels",                       /* Группы. */
                             "Geo-marks",                    /* Гео-метки. */
                             "Acoustic marks",               /* Акустические метки. */
                             "Tracks"};                      /* Галсы. */
/* Описания типов. */
static gchar *type_desc[] = {"All labels",                   /* Группы. */
                             "All geo-marks",                /* Гео-метки. */
                             "All acoustic marks",           /* Акустические метки. */
                             "All tracks"};                  /* Галсы. */

static void          hyscan_model_manager_set_property                     (GObject                 *object,
                                                                            guint                    prop_id,
                                                                            const GValue            *value,
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

static void          hyscan_model_manager_set_view_model                   (HyScanModelManager      *self,
                                                                            gboolean                 type);

static void          hyscan_model_manager_refresh_items                    (GtkTreeStore            *store,
                                                                            GHashTable             **items,
                                                                            ModelManagerObjectType   type);

static void          hyscan_model_manager_refresh_labels                   (GtkTreeStore            *store,
                                                                            GHashTable              *labels);

static void          hyscan_model_manager_refresh_geo_marks                (GtkTreeStore            *store,
                                                                            GHashTable              *geo_marks);

static void          hyscan_model_manager_refresh_acoustic_marks           (GtkTreeStore            *store,
                                                                            GHashTable              *acoustic_marks);

static void          hyscan_model_manager_refresh_tracks                   (GtkTreeStore            *store,
                                                                            GHashTable              *tracks);

static void          hyscan_model_manager_clear_view_model                 (GtkTreeModel            *view_model,
                                                                            gboolean                *flag);

static gboolean      hyscan_model_manager_init_items                       (HyScanModelManager      *self,
                                                                            GHashTable             **items);

static void          hyscan_model_manager_refresh_all_items                (GtkTreeStore            *store,
                                                                            GHashTable             **items);

static guint         hyscan_model_manager_signals[SIGNAL_MODEL_MANAGER_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanModelManager, hyscan_model_manager, G_TYPE_OBJECT)

void
hyscan_model_manager_class_init (HyScanModelManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  guint index;

  object_class->set_property = hyscan_model_manager_set_property;
  object_class->constructed  = hyscan_model_manager_constructed;
  object_class->finalize     = hyscan_model_manager_finalize;

  /* Название проекта. */
  g_object_class_install_property (object_class, PROP_PROJECT_NAME,
    g_param_spec_string ("project_name", "Project_name", "Project name",
                         "",
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
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
          priv->project_name = g_value_dup_string (value);
        }
      break;
      /* База данных. */
      case PROP_DB:
        {
          priv->db  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на базу данных. */
          g_object_ref (priv->db);
        }
      break;
      /* Кэш.*/
      case PROP_CACHE:
        {
          priv->cache  = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на кэш. */
          g_object_ref (priv->cache);
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
/* Конструктор. */
void
hyscan_model_manager_constructed (GObject *object)
{
  HyScanModelManager *self = HYSCAN_MODEL_MANAGER (object);
  HyScanModelManagerPrivate *priv = self->priv;
  /*priv->grouping = BY_LABELS;*/
  /* Модель галсов. */
  priv->track_model = hyscan_db_info_new (priv->db);
  hyscan_db_info_set_project (priv->track_model, priv->project_name);
  g_signal_connect (priv->track_model,
                    "tracks-changed",
                    G_CALLBACK (hyscan_model_manager_track_model_changed),
                    self);
  /* Модель "водопадных" меток. */
  priv->acoustic_marks_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  hyscan_object_model_set_project (priv->acoustic_marks_model, priv->db, priv->project_name);
  g_signal_connect (priv->acoustic_marks_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_model_changed),
                    self);
  /* Модель данных "водопадных" меток с координатами. */
  priv->acoustic_loc_model = hyscan_mark_loc_model_new (priv->db, priv->cache);
  hyscan_mark_loc_model_set_project (priv->acoustic_loc_model, priv->project_name);
  g_signal_connect (priv->acoustic_loc_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_acoustic_marks_loc_model_changed),
                    self);
  /* Модель геометок. */
  priv->geo_mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_GEOMARK);
  hyscan_object_model_set_project (priv->geo_mark_model, priv->db, priv->project_name);
  g_signal_connect (priv->geo_mark_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_geo_mark_model_changed),
                    self);
  /* Модель данных групп. */
  priv->label_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_LABEL);
  hyscan_object_model_set_project (priv->label_model, priv->db, priv->project_name);
  g_signal_connect (priv->label_model,
                    "changed",
                    G_CALLBACK (hyscan_model_manager_label_model_changed),
                    self);
  /* Создаём модель представления данных. */
  priv->view_model = hyscan_model_manager_update_view_model (self);
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
                                    G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, "водопадная" метка или галс. */
                                    G_TYPE_STRING,   /* Название картинки. */
                                    G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                    G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                    G_TYPE_STRING,   /* Время создания объекта. */
                                    G_TYPE_STRING)); /* Время модификации объекта. */
        }
      else
        {
          priv->view_model = GTK_TREE_MODEL (
                gtk_tree_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                    G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                    G_TYPE_STRING,   /* Название объекта. */
                                    G_TYPE_STRING,   /* Описание объекта. */
                                    G_TYPE_STRING,   /* Оператор, создавший объект. */
                                    G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, "водопадная" метка или галс. */
                                    G_TYPE_STRING,   /* Название картинки. */
                                    G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                    G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                    G_TYPE_STRING,   /* Время создания объекта. */
                                    G_TYPE_STRING)); /* Время модификации объекта. */
        }
    }

  if (priv->grouping == UNGROUPED)
    {
      /* Обновляем данные в модели для табличного представления. */
      hyscan_model_manager_set_view_model (self, FALSE);
    }
  else
    {
      /* Обновляем данные в модели для древовидного представления. */
      hyscan_model_manager_set_view_model (self, TRUE);
    }

  /* Отправляем сигнал об обновлении модели представления данных. */
  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_MODEL_UPDATED], 0);
  /* Возвращаем модель. */
  return priv->view_model;
}

/* Функция (если нужно пересоздаёт и) настраивает модель представления данных.
 * Тип представления задаётся type: FALSE - табличное, TRUE - древовидное.*/
void
hyscan_model_manager_set_view_model (HyScanModelManager *self,
                                     gboolean            type)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTable *items[TYPES];

  if (hyscan_model_manager_init_items (self, items))
    {
      if (type)
        {
          /* Проверяем, нужно ли пересоздавать модель.*/
          if (GTK_IS_LIST_STORE (priv->view_model))
            {
              /* Сохраняем идентификаторы выделенных галсов. */

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
                                        G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                        G_TYPE_STRING,   /* Название картинки. */
                                        G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                        G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                        G_TYPE_STRING,   /* Время создания объекта. */
                                        G_TYPE_STRING)); /* Время модификации объекта. */
              /* Разрешаем множественный выбор. */
              g_print ("*Selection: %p\n", priv->selection);
              if (priv->selection)
                {
                  gtk_tree_selection_set_mode (priv->selection, GTK_SELECTION_MULTIPLE);
                  g_print ("*GTK_SELECTION_MULTIPLE");
                }
            }
          else
            {
              /* Очищаем модель. */
              hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
              priv->view_model = GTK_TREE_MODEL (priv->view_model);
            }

          hyscan_model_manager_refresh_all_items (GTK_TREE_STORE (priv->view_model), items);

        }
      else
        {
          GtkListStore   *store;
          GtkTreeIter     store_iter;
          GHashTableIter  table_iter;      /* Итератор для обхода хэш-таблиц. */
          gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

          /* Проверяем, нужно ли пересоздавать модель.*/
          if (GTK_IS_TREE_STORE (priv->view_model))
            {
              /* Сохраняем идентификаторы выделенных галсов. */

              /* Очищаем модель. */
              hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
              g_object_unref (priv->view_model);
              /* Создаём новую модель. */
              store = gtk_list_store_new (MAX_COLUMNS,     /* Количество колонок для представления данных. */
                                          G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                          G_TYPE_STRING,   /* Название объекта. */
                                          G_TYPE_STRING,   /* Описание объекта. */
                                          G_TYPE_STRING,   /* Оператор, создавший объект. */
                                          G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, акустическая метка или галс. */
                                          G_TYPE_STRING,   /* Название картинки. */
                                          G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                          G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                          G_TYPE_STRING,   /* Время создания объекта. */
                                          G_TYPE_STRING);  /* Время модификации объекта. */
              /* Разрешаем множественный выбор. */
              g_print ("*Selection: %p\n", priv->selection);
              if (priv->selection)
                {
                  gtk_tree_selection_set_mode (priv->selection, GTK_SELECTION_MULTIPLE);
                  g_print ("*GTK_SELECTION_MULTIPLE");
                }
            }
          else
            {
              /* Очищаем модель. */
              hyscan_model_manager_clear_view_model (priv->view_model, &priv->clear_model_flag);
              store = GTK_LIST_STORE (priv->view_model);
            }
          /* Группы. */
          if (items[LABEL] != NULL)
            {
              HyScanLabel *object;

              g_hash_table_iter_init (&table_iter, items[LABEL]);
              while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
                {
                  gtk_list_store_append (store, &store_iter);
                  gtk_list_store_set (store,              &store_iter,
                                      COLUMN_ID,           id,
                                      COLUMN_NAME,         object->name,
                                      COLUMN_DESCRIPTION,  object->description,
                                      COLUMN_OPERATOR,     object->operator_name,
                                      COLUMN_TYPE,         LABEL,
                                      COLUMN_ICON,         object->icon_name,
                                      COLUMN_ACTIVE,       TRUE,
                                      COLUMN_LABEL,        object->label,
                                      COLUMN_CTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (object->ctime),
                                                             date_time_stamp),
                                      COLUMN_MTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (object->mtime),
                                                             date_time_stamp),
                                      -1);
                }
              g_hash_table_unref (items[LABEL]);
            }
          /* Гео-метки. */
          if (items[GEO_MARK] != NULL)
            {
              HyScanMarkGeo *object;

              g_hash_table_iter_init (&table_iter, items[GEO_MARK]);
              while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
                {
                  gtk_list_store_append (store, &store_iter);
                  gtk_list_store_set (store,              &store_iter,
                                      COLUMN_ID,           id,
                                      COLUMN_NAME,         object->name,
                                      COLUMN_DESCRIPTION,  object->description,
                                      COLUMN_OPERATOR,     object->operator_name,
                                      COLUMN_TYPE,         GEO_MARK,
                                      COLUMN_ICON,         icon_name[GEO_MARK],
                                      COLUMN_ACTIVE,       TRUE,
                                      COLUMN_LABEL,        object->labels,
                                      COLUMN_CTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (
                                                               object->ctime / G_TIME_SPAN_SECOND),
                                                             date_time_stamp),
                                      COLUMN_MTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (
                                                               object->mtime / G_TIME_SPAN_SECOND),
                                                             date_time_stamp),
                                      -1);
                }
              g_hash_table_unref (items[GEO_MARK]);
            }
          /* Акустические метки. */
          if (items[ACOUSTIC_MARK])
            {
              HyScanMarkLocation *location;

              g_hash_table_iter_init (&table_iter, items[ACOUSTIC_MARK]);
              while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &location))
                {
                  HyScanMarkWaterfall *object = location->mark;

                  gtk_list_store_append (store, &store_iter);
                  gtk_list_store_set (store,              &store_iter,
                                      COLUMN_ID,           id,
                                      COLUMN_NAME,         object->name,
                                      COLUMN_DESCRIPTION,  object->description,
                                      COLUMN_OPERATOR,     object->operator_name,
                                      COLUMN_TYPE,         ACOUSTIC_MARK,
                                      COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                                      COLUMN_ACTIVE,       TRUE,
                                      COLUMN_LABEL,        object->labels,
                                      COLUMN_CTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (
                                                               object->ctime / G_TIME_SPAN_SECOND),
                                                             date_time_stamp),
                                      COLUMN_MTIME,        g_date_time_format (
                                                             g_date_time_new_from_unix_local (
                                                               object->mtime / G_TIME_SPAN_SECOND),
                                                             date_time_stamp),
                                      -1);
                }
              g_hash_table_unref (items[ACOUSTIC_MARK]);
            }
          /* Галсы. */
          if (items[TRACK] != NULL)
            {
              HyScanTrackInfo *object;

              g_hash_table_iter_init (&table_iter, items[TRACK]);
              while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
                {
                  gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp);
                  gchar *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);
                  /* В структуре HyScanTrackInfo нет поля labels. */
                  guint64 labels = 0;
                  GList *ptr = g_list_first (priv->selected[TRACK]);

                  gtk_list_store_append (store, &store_iter);
                  gtk_list_store_set (store, &store_iter,
                                      COLUMN_ID,          id,
                                      COLUMN_NAME,        object->name,
                                      COLUMN_DESCRIPTION, object->description,
                                      COLUMN_OPERATOR,    object->operator_name,
                                      COLUMN_TYPE,        TRACK,
                                      COLUMN_ICON,        icon_name[TRACK],
                                      COLUMN_ACTIVE,      TRUE,
                                      /* В структуре HyScanTrackInfo нет поля labels. */
                                      COLUMN_LABEL,       labels,
                                      COLUMN_CTIME,       ctime,
                                      COLUMN_MTIME,       mtime,
                                      -1);

                  if (priv->selection != NULL)
                    {
                      /* Разрешаем множественный выбор. */
                      if (gtk_tree_selection_get_mode (priv->selection) != GTK_SELECTION_MULTIPLE)
                        {
                          gtk_tree_selection_set_mode (priv->selection, GTK_SELECTION_MULTIPLE);
                          g_print ("-> GTK_SELECTION_MULTIPLE");
                        }
                      /**/
                      while (ptr != NULL)
                        {
                          g_print ("ID: %s\nData: %s\n", id, (gchar*)ptr->data);
                          if (0 == g_strcmp0 (id, (gchar*)ptr->data))
                            {
                              g_print ("MATCH!!!\n");
                              priv->clear_model_flag = TRUE;
                              gtk_tree_selection_select_iter (priv->selection, &store_iter);
                              priv->clear_model_flag = FALSE;
                              break;
                            }
                          ptr = g_list_next (ptr);
                          g_print ("ptr: %p\n", ptr);
                        }
                    }
                }
              g_hash_table_unref (items[TRACK]);
            }
          /* Обновляем указатель на модель данных. */
          if (store != NULL)
            {
              priv->view_model = GTK_TREE_MODEL (store);
            }
        }
    }
}

/* Функция создаёт и заполняет узел "Группы" в древовидной модели. */
void
hyscan_model_manager_refresh_labels (GtkTreeStore *store,
                                     GHashTable   *labels)
{
  GtkTreeIter parent_iter,
              child_iter;
  GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
  HyScanLabel *object;
  gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

  /* Добавляем новый узел "Группы" в модель */
  gtk_tree_store_append (store, &parent_iter, NULL);
  gtk_tree_store_set (store,              &parent_iter,
                      COLUMN_ID,           NULL,
                      COLUMN_NAME,         type_name[LABEL],
                      COLUMN_DESCRIPTION,  type_desc[LABEL],
                      COLUMN_OPERATOR,     author,
                      COLUMN_TYPE,         LABEL,
                      COLUMN_ICON,         icon_name[LABEL],
                      COLUMN_ACTIVE,       TRUE,
                      COLUMN_LABEL,        0,
                      COLUMN_CTIME,        NULL,
                      COLUMN_MTIME,        NULL,
                      -1);

  g_hash_table_iter_init (&table_iter, labels);
  while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
    {
      GtkTreeIter item_iter;
      /* Добавляем новый узел c названием группы в модель */
      gtk_tree_store_append (store, &child_iter, &parent_iter);
      gtk_tree_store_set (store,              &child_iter,
                          COLUMN_ID,           id,
      /* Отображается. */ COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,  LABEL,
                          COLUMN_ICON,         object->icon_name,
                          COLUMN_ACTIVE,       TRUE,
                          COLUMN_LABEL,        object->label,
                          COLUMN_CTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (object->ctime),
                                                 date_time_stamp),
                          COLUMN_MTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (object->mtime),
                                                 date_time_stamp),
                          -1);
      /* Атрибуты группы. */
      /* Описание. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->description,
                          COLUMN_ICON,    "accessories-text-editor",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Оператор. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->operator_name,
                          COLUMN_ICON,    "user-info",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Время создания. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (object->ctime),
                                            date_time_stamp),
                          COLUMN_ICON,    "appointment-new",
                          COLUMN_ACTIVE,  TRUE,
                           -1);
      /* Время модификации. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (object->mtime),
                                            date_time_stamp),
                          COLUMN_ICON,    "document-open-recent",
                          COLUMN_ACTIVE,  TRUE,
      -1);
    }
}

/* Функция создаёт и заполняет узел "Гео-метки" в древовидной модели. */
void
hyscan_model_manager_refresh_geo_marks (GtkTreeStore *store,
                                        GHashTable   *geo_marks)
{
  GtkTreeIter parent_iter,
              child_iter;
  GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
  HyScanMarkGeo *object;
  gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
  /* Добавляем новый узел "Гео-метки" в модель */
  gtk_tree_store_append (store, &parent_iter, NULL);
  gtk_tree_store_set (store,              &parent_iter,
                      COLUMN_ID,           NULL,
                      COLUMN_NAME,         type_name[GEO_MARK],
                      COLUMN_DESCRIPTION,  type_desc[GEO_MARK],
                      COLUMN_OPERATOR,     author,
                      COLUMN_TYPE,         GEO_MARK,
                      COLUMN_ICON,         icon_name[GEO_MARK],
                      COLUMN_ACTIVE,       TRUE,
                      COLUMN_LABEL,        0,
                      COLUMN_CTIME,        NULL,
                      COLUMN_MTIME,        NULL,
                      -1);

  g_hash_table_iter_init (&table_iter, geo_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
    {
      GtkTreeIter item_iter;
      /* Добавляем новый узел c названием гео-метки в модель */
      gtk_tree_store_append (store, &child_iter, &parent_iter);
      gtk_tree_store_set (store,              &child_iter,
                          COLUMN_ID,           id,
      /* Отображается. */ COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         GEO_MARK,
                          COLUMN_ICON,         icon_name[GEO_MARK],
                          COLUMN_ACTIVE,       TRUE,
                          COLUMN_LABEL,        object->labels,
                          COLUMN_CTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (
                                                   object->ctime / G_TIME_SPAN_SECOND),
                                                 date_time_stamp),
                          COLUMN_MTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (
                                                   object->mtime / G_TIME_SPAN_SECOND),
                                                 date_time_stamp),
                          -1);
      /* Атрибуты гео-метки. */
      /* Описание. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->description,
                          COLUMN_ICON,    "accessories-text-editor",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Оператор. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->operator_name,
                          COLUMN_ICON,    "user-info",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Время создания. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (
                                              object->ctime / G_TIME_SPAN_SECOND),
                                            date_time_stamp),
                          COLUMN_ICON,    "appointment-new",
                          COLUMN_ACTIVE,  TRUE,
                           -1);
      /* Время модификации. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (
                                              object->mtime / G_TIME_SPAN_SECOND),
                                            date_time_stamp),
                          COLUMN_ICON,    "document-open-recent",
                          COLUMN_ACTIVE,  TRUE,
      -1);
    }
}

/* Функция создаёт и заполняет узел "Акустические метки" в древовидной модели. */
void
hyscan_model_manager_refresh_acoustic_marks (GtkTreeStore *store,
                                             GHashTable   *acoustic_marks)
{
  GtkTreeIter parent_iter,
              child_iter;
  GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
  HyScanMarkLocation *location;
  gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
  /* Добавляем новый узел "Акустические метки" в модель. */
  gtk_tree_store_append (store, &parent_iter, NULL);
  gtk_tree_store_set (store,              &parent_iter,
                      COLUMN_ID,           NULL,
                      COLUMN_NAME,         type_name[ACOUSTIC_MARK],
                      COLUMN_DESCRIPTION,  type_desc[ACOUSTIC_MARK],
                      COLUMN_OPERATOR,     author,
                      COLUMN_TYPE,         ACOUSTIC_MARK,
                      COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                      COLUMN_ACTIVE,       TRUE,
                      COLUMN_LABEL,        0,
                      COLUMN_CTIME,        NULL,
                      COLUMN_MTIME,        NULL,
                      -1);

  g_hash_table_iter_init (&table_iter, acoustic_marks);
  while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &location))
    {
      HyScanMarkWaterfall *object = location->mark;
      GtkTreeIter item_iter;
      /* Добавляем новый узел c названием группы в модель */
      gtk_tree_store_append (store, &child_iter, &parent_iter);
      gtk_tree_store_set (store,              &child_iter,
                          COLUMN_ID,           id,
      /* Отображается. */ COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         ACOUSTIC_MARK,
                          COLUMN_ICON,         icon_name[ACOUSTIC_MARK],
                          COLUMN_ACTIVE,       TRUE,
                          COLUMN_LABEL,        object->labels,
                          COLUMN_CTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (
                                                   object->ctime / G_TIME_SPAN_SECOND),
                                                 date_time_stamp),
                          COLUMN_MTIME,        g_date_time_format (
                                                 g_date_time_new_from_unix_local (
                                                   object->mtime / G_TIME_SPAN_SECOND),
                                                 date_time_stamp),
                          -1);
      /* Атрибуты группы. */
      /* Описание. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->description,
                          COLUMN_ICON,    "accessories-text-editor",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Оператор. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->operator_name,
                          COLUMN_ICON,    "user-info",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Время создания. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (
                                              object->ctime / G_TIME_SPAN_SECOND),
                                            date_time_stamp),
                          COLUMN_ICON,    "appointment-new",
                          COLUMN_ACTIVE,  TRUE,
                           -1);
      /* Время модификации. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    g_date_time_format (
                                            g_date_time_new_from_unix_local (
                                              object->mtime / G_TIME_SPAN_SECOND),
                                            date_time_stamp),
                          COLUMN_ICON,    "document-open-recent",
                          COLUMN_ACTIVE,  TRUE,
      -1);
    }
}

/* Функция создаёт и заполняет узел "Галсы" в древовидной модели. */
void
hyscan_model_manager_refresh_tracks (GtkTreeStore *store,
                                     GHashTable   *tracks)
{
  GtkTreeIter parent_iter,
              child_iter;
  GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
  HyScanTrackInfo *object;
  gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */
  /* Добавляем новый узел "Акустические метки" в модель. */
  gtk_tree_store_append (store, &parent_iter, NULL);
  gtk_tree_store_set (store,              &parent_iter,
                      COLUMN_ID,           NULL,
                      COLUMN_NAME,         type_name[TRACK],
                      COLUMN_DESCRIPTION,  type_desc[TRACK],
                      COLUMN_OPERATOR,     author,
                      COLUMN_TYPE,         TRACK,
                      COLUMN_ICON,         icon_name[TRACK],
                      COLUMN_ACTIVE,       TRUE,
                      COLUMN_LABEL,        0,
                      COLUMN_CTIME,        NULL,
                      COLUMN_CTIME,        NULL,
                      -1);

  g_hash_table_iter_init (&table_iter, tracks);
  while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
    {
      GtkTreeIter item_iter;
      gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp);
      gchar *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);
      /* В структуре HyScanTrackInfo нет поля labels. */
      guint64 labels = 0;

      /* Добавляем новый узел c названием группы в модель */
      gtk_tree_store_append (store, &child_iter, &parent_iter);
      gtk_tree_store_set (store,              &child_iter,
                          COLUMN_ID,           id,
      /* Отображается. */ COLUMN_NAME,         object->name,
                          COLUMN_DESCRIPTION,  object->description,
                          COLUMN_OPERATOR,     object->operator_name,
                          COLUMN_TYPE,         TRACK,
                          COLUMN_ICON,         icon_name[TRACK],
                          COLUMN_ACTIVE,       TRUE,
                          /* В структуре HyScanTrackInfo нет поля labels. */
                          COLUMN_LABEL,        labels,
                          COLUMN_CTIME,        ctime,
                          COLUMN_MTIME,        mtime,
                          -1);
      /* Атрибуты группы. */
      /* Описание. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->description,
                          COLUMN_ICON,    "accessories-text-editor",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Оператор. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    object->operator_name,
                          COLUMN_ICON,    "user-info",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
      /* Время создания. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    ctime,
                          COLUMN_ICON,    "appointment-new",
                          COLUMN_ACTIVE,  TRUE,
                           -1);
      /* Время модификации. */
      gtk_tree_store_append (store, &item_iter, &child_iter);
      gtk_tree_store_set (store,         &item_iter,
      /* Отображается. */ COLUMN_NAME,    mtime,
                          COLUMN_ICON,    "document-open-recent",
                          COLUMN_ACTIVE,  TRUE,
                          -1);
    }
}

/* Очищает модель от содержимого. */
void
hyscan_model_manager_clear_view_model (GtkTreeModel *view_model,
                                       gboolean     *flag)
{
  /* Защита от срабатывания срабатывания сигнала "changed"
  /* у GtkTreeSelection при очистке GtkTreeModel.
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
hyscan_model_manager_init_items (HyScanModelManager  *self,
                                 GHashTable         **items)
{
  ModelManagerObjectType type;
  gint res = 0;

  for (type = LABEL; type < TYPES; type++)
    {
      items[type] = hyscan_model_manager_get_items (self, type);
      res        += GPOINTER_TO_INT (items[type]);
    }

  return (gboolean)res;
}

/* Обновляет все объекты данными из моделей для древовидного списка. */
void
hyscan_model_manager_refresh_all_items (GtkTreeStore  *store,
                                        GHashTable   **items)
{
  ModelManagerObjectType type;

  for (type = LABEL; type < TYPES; type++)
    {
      if (items[type] != NULL)
        {
          hyscan_model_manager_refresh_items (store, items, type);
          g_hash_table_unref (items[type]);
        }
    }
}

/* Обновляет объекты заданного типа данными из модели для древовидного списка.*/
void
hyscan_model_manager_refresh_items (GtkTreeStore            *store,
                                    GHashTable             **items,
                                    ModelManagerObjectType   type)
{
  GHashTable *table = items[type];

  switch (type)
    {
    case LABEL:
      {
        hyscan_model_manager_refresh_labels (store, table);
        break;
      }
    case GEO_MARK:
      {
        hyscan_model_manager_refresh_geo_marks (store, table);
        break;
      }
    case ACOUSTIC_MARK:
      {
        hyscan_model_manager_refresh_acoustic_marks (store, table);
        break;
      }
    case TRACK:
      {
        hyscan_model_manager_refresh_tracks (store, table);
        break;
      }
    default: break;
    }
}
/**
 * hyscan_model_manager_new:
 * @project_name: название проекта
 * @db: указатель на базу данных
 * @cache: указатель на кэш
 *
 * Returns: cоздаёт новый объект #HyScanModelManager
 */
HyScanModelManager*
hyscan_model_manager_new (const gchar *project_name,
                          HyScanDB    *db,
                          HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_MODEL_MANAGER,
                       "project_name", project_name,
                       "db",           db,
                       "cache",        cache,
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
 * hyscan_model_manager_get_project_name:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: название проекта
 */
gchar*
hyscan_model_manager_get_project_name (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->project_name;
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
 * hyscan_model_manager_get_items:
 * @self: указатель на Менеджер Моделей
 * @type: тип запрашиваемых объектов (#ModelManagerObjectType)
 *
 * Returns: указатель на хэш-таблицу с данными в соответствии
 * с типом запрашиваемых объектов. Когда хэш-таблица больше не нужна,
 * необходимо использовать #g_hash_table_unref ().
 */
GHashTable*
hyscan_model_manager_get_items (HyScanModelManager     *self,
                                ModelManagerObjectType  type)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTable *table = NULL;

  switch (type)
  {
    case LABEL:
      {
        table = hyscan_object_model_get (priv->label_model);
        break;
      }
    case GEO_MARK:
      {
        table = hyscan_object_model_get (priv->geo_mark_model);
        break;
      }
    case ACOUSTIC_MARK:
      {
        table = hyscan_mark_loc_model_get (priv->acoustic_loc_model);
        break;
      }
    case TRACK:
      {
        table = hyscan_db_info_get_tracks (priv->track_model);
        break;
      }
    default: break;
  }
  return (table != NULL)? g_hash_table_ref (table) : NULL;
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
 * hyscan_model_manager_get_selected_tracks_id:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: NULL-терминированный список идентификаторов выбранных галсов.
 * Когда список больше не нужен, необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_model_manager_get_selected_tracks_id (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;

  guint count = g_list_length (priv->selected[TRACK]);
  gchar **list = NULL;
  if (count > 0)
    {
      GList *ptr = g_list_first (priv->selected[TRACK]);
      gint i = 0;
      list = g_new0 (gchar*, count + 1);
      while (ptr != NULL)
        {
          list[i++] = (gchar*)ptr->data;
          ptr = g_list_next (ptr);
        }
    }
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

  priv->grouping = grouping;
  hyscan_model_manager_update_view_model (self);

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_GROUPING_CHANGED], 0);
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
 * hyscan_model_manager_set_expand_nodes_mode:
 * @self: указатель на Менеджер Моделей
 * @expand_nodes_mode: отображения всех узлов.
 *                     TRUE  - развернуть все узлы,
 *                     FALSE - свернуть все узлы
 *
 * Устанавливает режим отображения всех узлов.
 */
void
hyscan_model_manager_set_expand_nodes_mode (HyScanModelManager *self,
                                            gboolean            expand_nodes_mode)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->expand_nodes_mode = expand_nodes_mode;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_EXPAND_NODES_MODE_CHANGED], 0);
}

/**
 * hyscan_model_manager_get_expand_nodes_mode:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: действительный режим отображения всех узлов.
 *          TRUE  - все узлы развернуты,
 *          FALSE - все узлы свернуты
 */
gboolean
hyscan_model_manager_get_expand_nodes_mode   (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->expand_nodes_mode;
}

/**
 * hyscan_model_manager_get_view_model:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: модель представления данных для #GtkTreeView. когда модель больше не нужна,
 * необходимо использовать #g_object_unref ().
 */
GtkTreeModel*
hyscan_model_manager_get_view_model (HyScanModelManager   *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return g_object_ref (priv->view_model);
}

/**
 * hyscan_model_manager_set_selection:
 * @self: указатель на Менеджер Моделей
 * @selected_items: указатель на список выделенных объектов
 *
 * Устанавливает выделенную строку.
 */
void
hyscan_model_manager_set_selection (HyScanModelManager *self,
                                    GtkTreeSelection   *selection)
{
  HyScanModelManagerPrivate *priv = self->priv;
  GHashTable *tracks          = hyscan_model_manager_get_items (self, TRACK);
  GList      *list            = NULL;

  if (priv->clear_model_flag)  /* Защита против срабатывания сигнала "changed" */
    return;                    /* у GtkTreeSelection при очистке GtkTreeModel.   */

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  priv->selection = selection;

  if (priv->selected[TRACK] != NULL)
    {
      g_list_free_full (priv->selected[TRACK], (GDestroyNotify)g_free);
      priv->selected[TRACK] = NULL;
    }

  /* Сохраняем идентификаторы */
  list = gtk_tree_selection_get_selected_rows (priv->selection, &priv->view_model);

  if (tracks != NULL && list != NULL)
    {
      GList *ptr = g_list_first (list);
      g_print ("*** Selected tracks ***\n");
      while (ptr != NULL)
        {
          GtkTreeIter  iter;
          GtkTreePath *path = (GtkTreePath*)ptr->data;

          if (gtk_tree_model_get_iter (priv->view_model, &iter, path))
            {
              GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
              HyScanTrackInfo *object;
              guint  type;
              gchar *id,                       /* Идентификатор для обхода хэш-таблиц (ключ). */
                    *str = NULL;               /* Идентификатор объекта из модели. */
              gtk_tree_model_get (priv->view_model, &iter,
                                  COLUMN_ID,        &str,
                                  -1);

              g_hash_table_iter_init (&table_iter, tracks);
              while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
                {
                  if (0 == g_strcmp0 (str, id))
                    {
                      g_print ("%s\n", str);
                      priv->selected[TRACK] = g_list_append (priv->selected[TRACK], str);
                      break;
                    }
                }
            }
          ptr = g_list_next (ptr);
        }
      g_print ("*** Selected tracks ***\n");
    }

  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_ITEM_SELECTED], 0);
}

/**
 * hyscan_model_manager_get_selected_items:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на список выделенных объектов.
 */
GtkTreeSelection*
hyscan_model_manager_get_selected_items (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->selection;
}

/**
 * hyscan_model_manager_set_horizontal:
 * @self: указатель на Менеджер Моделей
 *
 * Устанавливает положение горизонтальной полосы прокрутки представления.
 */
void
hyscan_model_manager_set_horizontal_adjustment (HyScanModelManager *self,
                                                GtkAdjustment      *adjustment)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->horizontal = adjustment;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_HORIZONTAL], 0);
}

/**
 * hyscan_model_manager_set_vertical:
 * @self: указатель на Менеджер Моделей
 *
 * Устанавливает положение вертикальной полосы прокрутки представления.
 */
void
hyscan_model_manager_set_vertical_adjustment (HyScanModelManager *self,
                                              GtkAdjustment      *adjustment)
{
  HyScanModelManagerPrivate *priv = self->priv;

  priv->vertical = adjustment;

  g_signal_emit (self, hyscan_model_manager_signals[SIGNAL_VIEW_SCROLLED_VERTICAL], 0);
}

/* hyscan_model_manager_get_horizontal_adjustment:
 * @self: указатель на Менеджер Моделей
 *
 * Returns: указатель на параметры горизонтальной полосы прокрутки.
 */
GtkAdjustment*
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
GtkAdjustment*
hyscan_model_manager_get_vertical_adjustment (HyScanModelManager *self)
{
  HyScanModelManagerPrivate *priv = self->priv;
  return priv->vertical;
}

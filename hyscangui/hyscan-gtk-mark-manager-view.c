/*
 * hyscan-gtk-mark-manager-view.c
 *
 *  Created on: 16 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include <hyscan-gtk-mark-manager-view.h>
#include <hyscan-mark.h>
#include <hyscan-mark-location.h>
#include <hyscan-db-info.h>

#define GETTEXT_PACKAGE "hyscanfnn-evoui"
#include <glib/gi18n-lib.h>

enum
{
  PROP_LABELS = 1,     /* Группы. */
  N_PROPERTIES
};

/* Сигналы. */
enum
{
  SIGNAL_SELECTED,
  SIGNAL_LAST
};
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

struct _HyScanMarkManagerViewPrivate
{
  GtkTreeView               *tree_view;       /* Представление. */
  ModelManagerGrouping       grouping;        /* Тип представления. */
  GtkTreeModel              *store;           /* Модель представления данных. */
  GHashTable                *labels,          /* Группы. */
                            *acoustic_marks,  /* Акустические метки. */
                            *geo_marks,       /* Гео-метки. */
                            *tracks;          /* Галсы. */
  gulong                     signal_selected; /* Идентификатор сигнала об изменении выбранных объектов.*/
};

static void       hyscan_mark_manager_view_set_property           (GObject               *object,
                                                                   guint                  prop_id,
                                                                   const GValue          *value,
                                                                   GParamSpec            *pspec);

static void       hyscan_mark_manager_view_constructed            (GObject               *object);

static void       hyscan_mark_manager_view_finalize               (GObject               *object);

static void       hyscan_mark_manager_view_update                 (HyScanMarkManagerView *self);

static gboolean   hyscan_mark_manager_view_emit_selected          (GtkTreeSelection      *selection,
                                                                   HyScanMarkManagerView *self);

static void       hyscan_mark_manager_view_refresh_labels         (HyScanMarkManagerView *self);

static void       hyscan_mark_manager_view_refresh_geo_marks      (HyScanMarkManagerView *self);

static void       hyscan_mark_manager_view_refresh_acoustic_marks (HyScanMarkManagerView *self);

static void       hyscan_mark_manager_view_refresh_tracks         (HyScanMarkManagerView *self);

static void       on_toggle_renderer_clicked                      (GtkCellRendererToggle *cell_renderer,
                                                                   gchar                 *path,
                                                                   gpointer               user_data);

static void       update_list_store                               (HyScanMarkManagerView *self);

static void       update_tree_store                               (HyScanMarkManagerView *self);

static void       set_list_model                                  (GtkTreeView           *list_view);

static void       set_tree_model                                  (GtkTreeView           *tree_view);

static void       remove_column                                   (gpointer               data,
                                                                   gpointer               user_data);

static void       macro_set_func_icon                             (GtkTreeViewColumn     *tree_column,
                                                                   GtkCellRenderer       *cell,
                                                                   GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   gpointer               data);

static void       macro_set_func_toggle                           (GtkTreeViewColumn     *tree_column,
                                                                   GtkCellRenderer       *cell,
                                                                   GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   gpointer               data);

static void       macro_set_func_text                             (GtkTreeViewColumn     *tree_column,
                                                                   GtkCellRenderer       *cell,
                                                                   GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   gpointer               data);

static guint      hyscan_mark_manager_view_signals[SIGNAL_LAST] = { 0 };

/* Форматированная строка для вывода времени и даты. */
static gchar *date_time_stamp = "%d.%m.%Y %H:%M:%S";

/* Оператор создавший типы объектов в древовидном списке. */
static gchar *author = "Author";

/* Стандартные картинки для типов объектов. */
static gchar *icon_name[] = {"emblem-documents",            /* Группы. */
                             "mark-location",               /* Гео-метки. */
                             "emblem-photos",               /* Акустические метки. */
                             "preferences-system-sharing"}; /* Галсы. */
/* Названия типов. */
static gchar *type_name[] = {"Labels",                       /* Группы. */
                             "Geo-marks",                    /* Гео-метки. */
                             "Acoustic marks",              /* Акустические метки. */
                             "Tracks"};                      /* Галсы. */
/* Описания типов. */
static gchar *type_desc[] = {"All labels",                   /* Группы. */
                             "All geo-marks",                /* Гео-метки. */
                             "All acoustic marks",          /* Акустические метки. */
                             "All tracks"};                  /* Галсы. */

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManagerView, hyscan_mark_manager_view, GTK_TYPE_SCROLLED_WINDOW)

void
hyscan_mark_manager_view_class_init (HyScanMarkManagerViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_view_set_property;
  object_class->constructed  = hyscan_mark_manager_view_constructed;
  object_class->finalize     = hyscan_mark_manager_view_finalize;

  /**
   * HyScanMarkManagerView::selected:
   * @self: указатель на #HyScanMarkManagerView
   *
   * Сигнал посылается при изменении списка меток.
   */
  hyscan_mark_manager_view_signals[SIGNAL_SELECTED] =
    g_signal_new ("selected", HYSCAN_TYPE_MARK_MANAGER_VIEW, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_TREE_SELECTION);
}

void
hyscan_mark_manager_view_init (HyScanMarkManagerView *self)
{
  self->priv = hyscan_mark_manager_view_get_instance_private (self);
}
void
hyscan_mark_manager_view_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanMarkManagerView *self        = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv = self->priv;

  switch (prop_id)
    {
      /* Группы. */
      /*case PROP_GROUPS:
        {
          priv->groups = (GHashTable*)g_value_get_pointer (value);
          if (priv->groups != NULL)
            g_hash_table_ref (priv->groups);
        }
      break;*/
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
hyscan_mark_manager_view_constructed (GObject *object)
{
  HyScanMarkManagerView        *self      = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv      = self->priv;
  GtkScrolledWindow            *widget    = GTK_SCROLLED_WINDOW (object);

  /* Рамка со скошенными внутрь границами. */
  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

  g_print ("Grouping: %i\n", priv->grouping);

  hyscan_mark_manager_view_update (self);

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->tree_view));
}
/* Деструктор. */
void
hyscan_mark_manager_view_finalize (GObject *object)
{
  HyScanMarkManagerView *self = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */

  /* Группы. */
  if (priv->labels != NULL)
    g_hash_table_unref (priv->labels);
  /* Акустические метки. */
  if (priv->acoustic_marks != NULL)
    g_hash_table_unref (priv->acoustic_marks);
  /* Гео-метки. */
  if (priv->geo_marks != NULL)
    g_hash_table_unref (priv->geo_marks);
  /* Галсы. */
  if (priv->tracks != NULL)
    g_hash_table_unref (priv->tracks);

  G_OBJECT_CLASS (hyscan_mark_manager_view_parent_class)->finalize (object);
}

void
hyscan_mark_manager_view_update (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;

  g_print (">>> UPDATE STORE <<<\n");
  g_print ("Mark Manager View: %p\n", self);
  g_print ("Grouping: %i\n", priv->grouping);
  g_print ("Tree view : %p\n", priv->tree_view);
  g_print ("Store: %p\n", priv->store);

  if (priv->store == NULL)
    {
      g_print ("Create a new store\n");
      if (priv->grouping == UNGROUPED)
        {
          priv->store = GTK_TREE_MODEL (
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
          g_print ("New list store created. Store: %p\n", priv->store);
        }
      else
        {
          priv->store = GTK_TREE_MODEL (
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
          g_print ("New tree store created. Store: %p\n", priv->store);
        }
    }
  else
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);
      g_print ("The store created already. Store: %p\n", priv->store);
      /* Отключаем сигнал об изменении выбранных объектов. */
      if (priv->signal_selected != 0 && selection != NULL)
        g_signal_handler_block (G_OBJECT (selection), priv->signal_selected);
      /* Очищаем модель. */
      if (GTK_IS_LIST_STORE (priv->store))
        {
          gtk_list_store_clear (GTK_LIST_STORE (priv->store));
          g_print ("List store is clear. Store: %p\n", priv->store);
        }
      else if (GTK_IS_TREE_STORE (priv->store))
        {
          gtk_tree_store_clear (GTK_TREE_STORE (priv->store));
          g_print ("Tree store is clear. Store: %p\n", priv->store);
        }
      else
        {
          g_warning ("Unknown type of store in HyScanMarkManagerView.\n");
        }
      /* Включаем сигнал об изменении выбранных объектов. */
      if (priv->signal_selected != 0 && selection != NULL)
        g_signal_handler_unblock (G_OBJECT (selection), priv->signal_selected);
    }

  if (priv->grouping == UNGROUPED)
    {
      /* Обновляем данные в модели для табличного представления. */
      update_list_store (self);

      /* Передаём модель в представление. */
      if (priv->tree_view == NULL)
        {
          GtkTreeSelection  *selection;

          priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

          g_print ("New tree view created. New tree view: %p\n", priv->tree_view);

          set_list_model (priv->tree_view);

          selection = gtk_tree_view_get_selection (priv->tree_view);
          /* Разрешаем множественный выбор. */
          gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
          /* Соединяем сигнал изменения выбранных элементов с функцией-обработчиком. */
          priv->signal_selected = g_signal_connect (G_OBJECT (selection), "changed",
                                                    G_CALLBACK (hyscan_mark_manager_view_emit_selected), self);
        }
      else
        {
          gtk_tree_view_set_model (priv->tree_view, GTK_TREE_MODEL (priv->store));
          g_print ("Use old tree view. Tree view: %p\n", priv->tree_view);
        }
    }
  else
    {
      /* Обновляем данные в модели для древовидного представления. */
      update_tree_store (self);

      /* Передаём модель в представление. */
      if (priv->tree_view == NULL)
        {
          GtkTreeSelection  *selection;

          priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

          g_print ("New tree view created. New tree view: %p\n", priv->tree_view);

          set_tree_model (priv->tree_view);

          selection = gtk_tree_view_get_selection (priv->tree_view);
          /* Разрешаем множественный выбор. */
          gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
          /* Соединяем сигнал изменения выбранных элементов с функцией-обработчиком. */
          priv->signal_selected = g_signal_connect (G_OBJECT (selection), "changed",
                                                    G_CALLBACK (hyscan_mark_manager_view_emit_selected), self);
        }
      else
        {
          gtk_tree_view_set_model (priv->tree_view, GTK_TREE_MODEL (priv->store));
          g_print ("Use old tree view. Tree view: %p\n", priv->tree_view);
        }
    }

  g_print ("<<< UPDATE STORE >>>\n");
  g_print ("Mark Manager View: %p\n", self);
  g_print ("Grouping: %i\n", priv->grouping);
  g_print ("Tree view : %p\n", priv->tree_view);
  g_print ("Store: %p\n", priv->store);
}

/* Обработчик выделеня строки в списке. */
gboolean
hyscan_mark_manager_view_emit_selected (GtkTreeSelection          *selection,
                                        HyScanMarkManagerView     *self)
{
  if (gtk_tree_selection_count_selected_rows (selection))
    {
      g_print ("<<< SIGNAL EMIT\n");
      g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_SELECTED], 0, selection);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

/* Обработчик клика по чек-боксу. */
void
on_toggle_renderer_clicked (GtkCellRendererToggle *cell_renderer,
                            gchar                 *path,
                            gpointer               user_data)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gboolean active;

  g_print ("TOGGLED\n");

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (user_data));

  if (gtk_tree_model_get_iter_from_string (model, &iter, path) == TRUE)
    {
      gtk_tree_model_get (model, &iter, COLUMN_ACTIVE, &active, -1);

      if (GTK_IS_TREE_STORE (model))
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COLUMN_ACTIVE, !active, -1);
      else
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACTIVE, !active, -1);
    }
}

/* Функция обновляет модель для табличного представления. */
void
update_list_store (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkListStore *store;
  GtkTreeIter store_iter;
  GHashTableIter table_iter;       /* Итератор для обхода хэш-таблиц. */
  gchar *id;                       /* Идентификатор для обхода хэш-таблиц (ключ). */

  g_print (">>> update LIST store <<<\n");
  g_print ("Labels: %p\n", priv->labels);
  g_print ("Geo-marks: %p\n", priv->geo_marks);
  g_print ("Waterfall marks: %p\n", priv->acoustic_marks);
  g_print ("Tracks: %p\n", priv->tracks);

  if (priv->labels         != NULL ||
      priv->geo_marks      != NULL ||
      priv->acoustic_marks != NULL ||
      priv->tracks         != NULL)
    {
      /* Проверяем, нужно ли пересоздавать модель.*/
      if (GTK_IS_TREE_STORE (priv->store))
        {
          g_print ("Used model is not a list store. This model will be destroyed.\n");
          g_object_unref (priv->store);
          store = gtk_list_store_new (MAX_COLUMNS,   /* Количество колонок для представления данных. */
                                      G_TYPE_STRING,   /* Идентификатор объекта в базе данных. */
                                      G_TYPE_STRING,   /* Название объекта. */
                                      G_TYPE_STRING,   /* Описание объекта. */
                                      G_TYPE_STRING,   /* Оператор, создавший объект. */
                                      G_TYPE_UINT,     /* Тип объекта: группа, гео-метка, "водопадная" метка или галс. */
                                      G_TYPE_STRING,   /* Название картинки. */
                                      G_TYPE_BOOLEAN,  /* Статус чек-бокса. */
                                      G_TYPE_UINT64,   /* Метки групп к которым принадлежит объект. */
                                      G_TYPE_STRING,   /* Время создания объекта. */
                                      G_TYPE_STRING);  /* Время модификации объекта. */
          g_print ("New list store created. Store: %p\n", store);
          /* Обновляем модель для табличного представления. */
          set_list_model (priv->tree_view);
        }
      else
        {
          store = GTK_LIST_STORE (priv->store);
          g_print ("Use old store. Store: %p\n", store);
        }
      /* Группы. */
      if (priv->labels != NULL)
        {
          HyScanLabel *object;

          g_hash_table_iter_init (&table_iter, priv->labels);
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
        }
      /* Гео-метки. */
      if (priv->geo_marks != NULL)
        {
          HyScanMarkGeo *object;

          g_hash_table_iter_init (&table_iter, priv->geo_marks);
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
        }
      /* Акустические метки. */
      if (priv->acoustic_marks)
        {
          HyScanMarkLocation *location;

          g_hash_table_iter_init (&table_iter, priv->acoustic_marks);
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
        }
      /* Галсы. */
      if (priv->tracks != NULL)
        {
          HyScanTrackInfo *object;

          g_hash_table_iter_init (&table_iter, priv->tracks);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
            {
              gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, date_time_stamp);
              gchar *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, date_time_stamp);
              /* В структуре HyScanTrackInfo нет поля labels. */
              guint64 labels = 0;

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
            }
        }
      /* Обновляем указатель на модель данных. */
      if (store != NULL)
        {
          priv->store = GTK_TREE_MODEL (store);
        }
    }
  g_print ("<<< update LIST store >>>\n");
}

/* Функция создаёт модель для древовидного представления. */
void
update_tree_store (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;

  g_print (">>> update TREE store <<<\n");
  g_print ("Labels: %p\n", priv->labels);
  g_print ("Geo-marks: %p\n", priv->geo_marks);
  g_print ("Waterfall marks: %p\n", priv->acoustic_marks);
  g_print ("Tracks: %p\n", priv->tracks);

  if (priv->labels     != NULL ||
      priv->geo_marks  != NULL ||
      priv->acoustic_marks   != NULL ||
      priv->tracks     != NULL)
    {
      /* Проверяем, нужно ли пересоздавать модель.*/
      if (GTK_IS_LIST_STORE (priv->store))
        {
          g_print ("Used model is not a tree store. This model will be destroyed.\n");
          g_object_unref (priv->store);
          priv->store = GTK_TREE_MODEL (
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
          g_print ("New tree store created. Store: %p\n", priv->store);
          /* Обновляем модель для древовидного представления. */
          set_tree_model (priv->tree_view);
        }
      else
        {
          priv->store = GTK_TREE_MODEL (priv->store);
          g_print ("Use old store. Store: %p\n", priv->store);
        }
      /* Группы. */
      if (priv->labels != NULL)
        {
          hyscan_mark_manager_view_refresh_labels (self);
        }
      /* Гео-метки. */
      if (priv->geo_marks != NULL)
        {
          hyscan_mark_manager_view_refresh_geo_marks (self);
        }
      /* Акустические метки. */
      if (priv->acoustic_marks)
        {
          hyscan_mark_manager_view_refresh_acoustic_marks (self);
        }
      /* Галсы. */
      if (priv->tracks != NULL)
        {
          hyscan_mark_manager_view_refresh_tracks (self);
        }
    }
  g_print ("<<< update TREE store >>>\n");
}

/* Функция устанавливает модель для табличного представления. */
void
set_list_model (GtkTreeView *list_view)
{
  GtkTreeViewColumn *column          = gtk_tree_view_column_new();            /* Колонка для ViewItem-а. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),         /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new(),        /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new();        /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (list_view); /* Список колонок. */

  g_print (">>> set_LIST_model <<<\n");

  /* Удаляем все колонки. */
  g_list_foreach (list, remove_column, list_view);
  /* Освобождаем список колонок. */
  g_list_free (list);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (on_toggle_renderer_clicked),
                    list_view);

  /* Заголовок колонки. */
  gtk_tree_view_column_set_title(column, "Name");
  /* Подключаем функцию для отрисовки чек-бокса.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           toggle_renderer,
                                           macro_set_func_toggle,
                                           NULL,
                                           NULL);
  /* Добавляем чек-бокс. */
  gtk_tree_view_column_pack_start (column, toggle_renderer, FALSE);
  /* Подключаем функцию для отрисовки картинки.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           macro_set_func_icon,
                                           NULL,
                                           NULL);
  /* Добавляем картинку.*/
  gtk_tree_view_column_pack_start (column, icon_renderer, FALSE);
  /* Подключаем функцию для отрисовки текста.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           macro_set_func_text,
                                           NULL,
                                           NULL);
  /* Добавляем текст. */
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  /* Добавляем колонку в список. */
  gtk_tree_view_append_column (list_view, column);

  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_DESCRIPTION,
                                               "Description", renderer,
                                               "text", COLUMN_DESCRIPTION,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_OPERATOR,
                                               "Operator name", renderer,
                                               "text", COLUMN_OPERATOR,
                                               NULL);
/*
  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_LABEL,
                                               "Label", renderer,
                                               "text", COLUMN_LABEL,
                                               NULL);
*/
  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_CTIME,
                                               "Created", renderer,
                                               "text", COLUMN_CTIME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_MTIME,
                                               "Modified", renderer,
                                               "text", COLUMN_MTIME,
                                               NULL);
  /* Показать заголовки. */
  gtk_tree_view_set_headers_visible (list_view, TRUE);
  /* Скрыть ветви линиями. */
  gtk_tree_view_set_enable_tree_lines (list_view, FALSE);
  g_print ("<<< set_LIST_model >>>\n");
}

/* Функция устанавливает модель для древовидного представления. */
void
set_tree_model (GtkTreeView *tree_view)
{
  GtkTreeViewColumn *column          = gtk_tree_view_column_new();            /* Колонка для ViewItem-а. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),         /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new(),        /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new();        /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (tree_view); /* Список колонок. */

  g_print (">>> set_TREE_model <<<\n");

  /* Удаляем все колонки. */
  g_list_foreach (list, remove_column, tree_view);
  /* Освобождаем список колонок. */
  g_list_free (list);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (on_toggle_renderer_clicked),
                    tree_view);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect(toggle_renderer,
                   "toggled",
                   G_CALLBACK (on_toggle_renderer_clicked),
                   tree_view);
  /* Заголовок колонки. */
  gtk_tree_view_column_set_title(column, "Objects");
  /* Подключаем функцию для отрисовки чек-бокса.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           toggle_renderer,
                                           macro_set_func_toggle,
                                           NULL,
                                           NULL);
  /* Добавляем чек-бокс. */
  gtk_tree_view_column_pack_start (column, toggle_renderer, FALSE);
  /* Подключаем функцию для отрисовки картинки.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           macro_set_func_icon,
                                           NULL,
                                           NULL);
  /* Добавляем картинку.*/
  gtk_tree_view_column_pack_start (column, icon_renderer, FALSE);
  /* Подключаем функцию для отрисовки текста.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           macro_set_func_text,
                                           NULL,
                                           NULL);
  /* Добавляем текст. */
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  /* Добавляем колонку в список. */
  gtk_tree_view_append_column (tree_view, column);
/*
  gtk_tree_view_insert_column_with_attributes (tree_view,
                                               COLUMN_TYPE,
                                               "Objects", renderer,
                                               "text", COLUMN_NAME,
                                               NULL);
*/
  /* Скрыть заголовки. */
  gtk_tree_view_set_headers_visible (tree_view, FALSE);
  /* Показывать ветви линиями. */
  gtk_tree_view_set_enable_tree_lines (tree_view, TRUE);
  /* Назначаем расширители для первого столбца. */
  /*column = gtk_tree_view_get_column (tree_view, COLUMN_TYPE);
  gtk_tree_view_set_expander_column (tree_view, column);*/
  g_print ("<<< set_TREE_model >>>\n");
}

/* Функция обновляет информацию о группах. */
void
hyscan_mark_manager_view_refresh_labels (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeStore *store = GTK_TREE_STORE (priv->store);
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

  g_hash_table_iter_init (&table_iter, priv->labels);
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

/* Функция обновляет информацию о гео-метках. */
void
hyscan_mark_manager_view_refresh_geo_marks (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeStore *store = GTK_TREE_STORE (priv->store);
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

  g_hash_table_iter_init (&table_iter, priv->geo_marks);
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

/* Функция обновляет информацию об акустических метках. */
void
hyscan_mark_manager_view_refresh_acoustic_marks (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeStore *store = GTK_TREE_STORE (priv->store);
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

  g_hash_table_iter_init (&table_iter, priv->acoustic_marks);
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
/* Функция обновляет информацию о галсах. */
void
hyscan_mark_manager_view_refresh_tracks (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeStore *store = GTK_TREE_STORE (priv->store);
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

  g_hash_table_iter_init (&table_iter, priv->tracks);
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

/* Функция удаляет колонку из GtkTreeView. */
void
remove_column (gpointer data,
               gpointer user_data)
{
  GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (data);
  GtkTreeView *tree_view    = GTK_TREE_VIEW (user_data);
  gtk_tree_view_remove_column (tree_view, column);
}

/* Отображение картинки. */
void
macro_set_func_icon (GtkTreeViewColumn *tree_column,
                     GtkCellRenderer   *cell,
                     GtkTreeModel      *model,
                     GtkTreeIter       *iter,
                     gpointer           data)
{
  gchar *icon_name = NULL;
  gtk_tree_model_get (model, iter, COLUMN_ICON, &icon_name, -1);
  g_object_set (GTK_CELL_RENDERER (cell), "icon-name", icon_name, NULL);
  if (icon_name != NULL)
    g_free (icon_name);
}

/* Отображение чек-бокса. */
void
macro_set_func_toggle (GtkTreeViewColumn *tree_column,
                       GtkCellRenderer   *cell,
                       GtkTreeModel      *model,
                       GtkTreeIter       *iter,
                       gpointer           data)
{
  gboolean active;
  gtk_tree_model_get (model, iter, COLUMN_ACTIVE, &active, -1);
  g_object_set (GTK_CELL_RENDERER (cell), "active", active, NULL);
}
/* Отображение названия. */
void
macro_set_func_text (GtkTreeViewColumn *tree_column,
                      GtkCellRenderer   *cell,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      gpointer           data)
{
  gchar *str = NULL;
  /* Отображаются текстовые данные из поля COLUMN_NAME. */
  gtk_tree_model_get (model, iter, COLUMN_NAME, &str, -1);
  g_object_set (GTK_CELL_RENDERER (cell), "text", str, NULL);
  if (str != NULL)
    g_free (str);
}


/**
 * hyscan_mark_manager_view_set_grouping:
 * @self: указатель на структуру HyScanMarkManagerView
 * @grouping: тип представления
 *
 * Устанавливает тип представления.
 */
void
hyscan_mark_manager_view_set_grouping  (HyScanMarkManagerView     *self,
                                        ModelManagerGrouping       grouping)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->grouping = grouping;
  g_print ("Set grouping\n");
  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_get_grouping:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Returns: текущиий тип предствления
 *
 * Возвращает актуальный тип представления.
 */
ModelManagerGrouping hyscan_mark_manager_view_get_grouping (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return priv->grouping;
}

/**
 * hyscan_mark_manager_view_update_labels:
 * @self: указатель на структуру HyScanMarkManagerView
 * @labels: указатель на таблицу с данными о группах
 *
 * Обновляет данные о группах.
 */
void
hyscan_mark_manager_view_update_labels (HyScanMarkManagerView *self,
                                        GHashTable            *labels)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;

  priv->labels = labels;

  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_update_geo_marks:
 * @self: указатель на структуру HyScanMarkManagerView
 * @geo_marks: указатель на таблицу с данными о гео-метках
 *
 * Обновляет данные о гео-метках.
 */
void
hyscan_mark_manager_view_update_geo_marks (HyScanMarkManagerView *self,
                                           GHashTable            *geo_marks)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->geo_marks = geo_marks;
  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_update_acoustic_marks:
 * @self: указатель на структуру HyScanMarkManagerView
 * @acoustic_marks: указатель на таблицу с данными о "водопадных" метках
 *
 * Обновляет данные о "водопадных" метках.
 */
void
hyscan_mark_manager_view_update_acoustic_marks (HyScanMarkManagerView *self,
                                                GHashTable            *acoustic_marks)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->acoustic_marks = acoustic_marks;
  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_update_tracks:
 * @self: указатель на структуру HyScanMarkManagerView
 * @tracks: указатель на таблицу с данными о галсах
 *
 * Обновляет данные о галсах.
 */
void
hyscan_mark_manager_view_update_tracks (HyScanMarkManagerView *self,
                                        GHashTable            *tracks)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->tracks = tracks;
  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_get_selection:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Returns: указатель на объект выбора.
 */
GtkTreeSelection*
hyscan_mark_manager_view_get_selection (HyScanMarkManagerView     *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return gtk_tree_view_get_selection (priv->tree_view);
}

/**
 * hyscan_mark_manager_view_has_selected:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Returns: TRUE - если есть выделенные строки,
 *          FALSE - если нет выделенных строк.
 */
gboolean
hyscan_mark_manager_view_has_selected (HyScanMarkManagerView     *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return gtk_tree_selection_count_selected_rows (
             gtk_tree_view_get_selection (priv->tree_view));
}

/**
 * hyscan_mark_manager_view_expand_all:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Рекурсивно разворачивает все узлы древовидного списка.
 */
void
hyscan_mark_manager_view_expand_all (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  if (priv->grouping != UNGROUPED && GTK_IS_TREE_STORE (priv->store))
    {
      gtk_tree_view_expand_all (priv->tree_view);
    }
}

/**
 * hyscan_mark_manager_view_collapse_all:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Рекурсивно cворачивает все узлы древовидного списка.
 */
void
hyscan_mark_manager_view_collapse_all (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  if (priv->grouping != UNGROUPED && GTK_IS_TREE_STORE (priv->store))
    {
      gtk_tree_view_collapse_all (priv->tree_view);
    }
}

/**
 * hyscan_mark_manager_view_select_all:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Выделяет все объекты.
 */
void
hyscan_mark_manager_view_select_all (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  gtk_tree_selection_select_all (gtk_tree_view_get_selection (priv->tree_view));
}

/**
 * hyscan_mark_manager_view_unselect_all:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Отменяет выделение всех объектов.
 */
void
hyscan_mark_manager_view_unselect_all (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (priv->tree_view));
}

/**
 * hyscan_mark_manager_view_expand_to_path:
 *
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Разворачивает узел по заданному пути.
 */
void
hyscan_mark_manager_view_expand_to_path (HyScanMarkManagerView *self,
                                         GtkTreePath           *path)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  if (priv->grouping != UNGROUPED  && GTK_IS_TREE_STORE (priv->store))
    {
      g_print ("Path: %s\n", gtk_tree_path_to_string (path));
      if (gtk_tree_path_up (path))
        {
          /*gtk_tree_path_down (path);*/
          g_print ("Path: %s\n", gtk_tree_path_to_string (path));
          /*gtk_tree_view_expand_to_path (priv->tree_view, path);*/
          if (gtk_tree_view_expand_row (priv->tree_view, path, TRUE))
            g_print ("Has children (TRUE)\n");
          else
            g_print ("Hasn't children (FALSE)\n");
        }
    }
}

/**
 * hyscan_mark_manager_view_new:
 *
 * Returns: cоздаёт новый экземпляр #HyScanMarkManageView
 */
GtkWidget*
hyscan_mark_manager_view_new (void)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_MARK_MANAGER_VIEW,
                                   NULL));
}


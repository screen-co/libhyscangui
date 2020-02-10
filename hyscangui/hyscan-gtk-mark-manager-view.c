/*
 * hyscan-gtk-mark-manager-view.c
 *
 *  Created on: 16 янв. 2020 г.
 *      Author: Andrey Zakharov <zaharov@screen-co.ru>
 */
#include "hyscan-gtk-mark-manager-view.h"
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

enum
{
  SIGNAL_SELECTED,
  SIGNAL_LAST
};
/* Столбцы представления. */
enum
{
  COLUMN_ID,          /* Идентификатор объекта в базе данных. */
  COLUMN_NAME,        /* Название объекта. */
  COLUMN_DESCRIPTION, /* Описание объекта. */
  COLUMN_OPERATOR,    /* Оператор, создавший объект. */
  COLUMN_LABEL,       /* Метки групп к которым принадлежит объект. */
  COLUMN_CTIME,       /* Время создания объекта. */
  COLUMN_MTIME,       /* Врем модификации объекта. */
  N_COLUMNS           /* Общее количество колонок для представления данных. */
};

struct _HyScanMarkManagerViewPrivate
{
  GtkTreeView               *tree_view;  /* Представление. */
  HyScanMarkManagerGrouping  grouping;   /* Тип представления. */
  GtkTreeModel              *store;      /* Модель представления данных. */
  GHashTable                *labels,     /* Группы. */
                            *wf_marks,   /* "Водопадные" метки. */
                            *geo_marks,  /* Гео-метки. */
                            *tracks;     /* Галсы. */
  gulong                     signal;     /* Идентификатор сигнала об изменении выбранных объектов.*/
};

static void       hyscan_mark_manager_view_set_property         (GObject               *object,
                                                                 guint                  prop_id,
                                                                 const GValue          *value,
                                                                 GParamSpec            *pspec);

static void       hyscan_mark_manager_view_constructed          (GObject               *object);

static void       hyscan_mark_manager_view_finalize             (GObject               *object);

static void       hyscan_mark_manager_view_update_store         (HyScanMarkManagerView *self);

static gboolean   hyscan_mark_manager_view_emit_selected        (GtkTreeSelection      *selection,
                                                                 HyScanMarkManagerView *self);

static guint      hyscan_mark_manager_view_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManagerView, hyscan_mark_manager_view, GTK_TYPE_SCROLLED_WINDOW)

static void
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

static void
hyscan_mark_manager_view_init (HyScanMarkManagerView *self)
{
  self->priv = hyscan_mark_manager_view_get_instance_private (self);
  self->priv->grouping = UNGROUPED;
}
static void
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
static void
hyscan_mark_manager_view_constructed (GObject *object)
{
  HyScanMarkManagerView        *self      = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv      = self->priv;
  GtkScrolledWindow            *widget    = GTK_SCROLLED_WINDOW (object);
  /* Рамка со скошенными внутрь границами. */
  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

  g_print ("Grouping: %i\n", priv->grouping);

  hyscan_mark_manager_view_update_store (self);

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->tree_view));
}
/* Деструктор. */
static void
hyscan_mark_manager_view_finalize (GObject *object)
{
  HyScanMarkManagerView *self = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */

  /* Уменьшаем счётчик ссылок на родительское окно. */
  /*if (priv->groups != NULL)
    g_hash_table_unref (priv->groups);
*/
  G_OBJECT_CLASS (hyscan_mark_manager_view_parent_class)->finalize (object);
}

inline void
hyscan_mark_manager_view_update_store (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkCellRenderer *renderer;
  GHashTableIter table_iter; /* Итератор для обхода хэш-таблиц. */
  gchar *id;                 /* Идентификатор для обхода хэш-таблиц (ключ). */

  g_print ("Update store\n");

  if (priv->store == NULL)
    {
      g_print ("Create a new store\n");
      if (priv->grouping == UNGROUPED)
        {
          priv->store = GTK_TREE_MODEL (gtk_list_store_new (N_COLUMNS,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_UINT64,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING));
        }
      else
        {
          priv->store = GTK_TREE_MODEL (gtk_tree_store_new (N_COLUMNS,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING,
                                                            G_TYPE_UINT64,
                                                            G_TYPE_STRING,
                                                            G_TYPE_STRING));
        }
    }
  else
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);
      g_print ("The store created already.\n");
      /* Отключаем сигнал об изменении выбранных объектов. */
      if (priv->signal != 0 && selection != NULL)
        g_signal_handler_block (G_OBJECT (selection), priv->signal);
      /* Очищаем модель. */
      if (GTK_IS_LIST_STORE (priv->store))
        {
          gtk_list_store_clear (GTK_LIST_STORE (priv->store));
          g_print ("List store is clear.\n");
        }
      else if (GTK_IS_TREE_STORE (priv->store))
        {
          gtk_tree_store_clear (GTK_TREE_STORE (priv->store));
          g_print ("Tree store is clear.\n");
        }
      else
        {
          g_warning ("Unknown type of store in HyScanMarkManagerView.\n");
        }
      /* Включаем сигнал об изменении выбранных объектов. */
      if (priv->signal != 0 && selection != NULL)
        g_signal_handler_unblock (G_OBJECT (selection), priv->signal);
    }

  if (priv->grouping == UNGROUPED)
    {
      GtkListStore *store = NULL;
      GtkTreeIter store_iter;

      if (priv->labels    != NULL ||
          priv->geo_marks != NULL ||
          priv->wf_marks  != NULL ||
          priv->tracks    != NULL)
        {
          if (GTK_IS_TREE_STORE (priv->store))
            {
               g_object_unref (priv->store);
               store = gtk_list_store_new (N_COLUMNS,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_UINT64,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);
            }
          else
            {
              store = GTK_LIST_STORE (priv->store);
            }
        }
      /* Группы. */
      if (priv->labels != NULL)
        {
          HyScanLabel *object;

          g_hash_table_iter_init (&table_iter, priv->labels);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
            {
              gtk_list_store_append (store, &store_iter);
              gtk_list_store_set (store, &store_iter,
                                  COLUMN_ID,          id,
                                  COLUMN_NAME,        object->name,
                                  COLUMN_DESCRIPTION, object->description,
                                  COLUMN_OPERATOR,    object->operator_name,
                                  COLUMN_LABEL,       object->label,
                                  COLUMN_CTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (object->ctime),
                                                        "%d.%m.%Y %H:%M:%S."),
                                  COLUMN_MTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (object->mtime),
                                                        "%d.%m.%Y %H:%M:%S."),
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
              gtk_list_store_set (store, &store_iter,
                                  COLUMN_ID,          id,
                                  COLUMN_NAME,        object->name,
                                  COLUMN_DESCRIPTION, object->description,
                                  COLUMN_OPERATOR,    object->operator_name,
                                  COLUMN_LABEL,       object->labels,
                                  COLUMN_CTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (
                                                          object->ctime / G_TIME_SPAN_SECOND),
                                                        "%d.%m.%Y %H:%M:%S."),
                                  COLUMN_MTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (
                                                          object->mtime / G_TIME_SPAN_SECOND),
                                                        "%d.%m.%Y %H:%M:%S."),
                                  -1);
            }
        }
      /* "Водопадные" метки. */
      if (priv->wf_marks)
        {
          HyScanMarkLocation *location;

          g_hash_table_iter_init (&table_iter, priv->wf_marks);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &location))
            {
              HyScanMarkWaterfall *object = location->mark;

              gtk_list_store_append (store, &store_iter);
              gtk_list_store_set (store, &store_iter,
                                  COLUMN_ID,          id,
                                  COLUMN_NAME,        object->name,
                                  COLUMN_DESCRIPTION, object->description,
                                  COLUMN_OPERATOR,    object->operator_name,
                                  COLUMN_LABEL,       object->labels,
                                  COLUMN_CTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (
                                                          object->ctime / G_TIME_SPAN_SECOND),
                                                        "%d.%m.%Y %H:%M:%S."),
                                  COLUMN_MTIME,       g_date_time_format (
                                                        g_date_time_new_from_unix_local (
                                                          object->mtime / G_TIME_SPAN_SECOND),
                                                        "%d.%m.%Y %H:%M:%S."),
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
               gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, "%d.%m.%Y %H:%M:%S.");
               gchar *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, "%d.%m.%Y %H:%M:%S.");
               /* В структуре HyScanTrackInfo нет поля labels. */
               guint64 labels = 0;
               gtk_list_store_append (store, &store_iter);
               gtk_list_store_set (store, &store_iter,
                                   COLUMN_ID,          id,
                                   COLUMN_NAME,        object->name,
                                   COLUMN_DESCRIPTION, object->description,
                                   COLUMN_OPERATOR,    object->operator_name,
                                   /* В структуре HyScanTrackInfo нет поля labels. */
                                   COLUMN_LABEL,       labels,
                                   COLUMN_CTIME,       ctime,
                                   COLUMN_MTIME,       mtime,
                                   -1);
            }
        }

      if (store != NULL)
        priv->store = GTK_TREE_MODEL (store);
    }
  else
    {
      GtkTreeIter parent_iter, child_iter;
      GtkTreeStore *store;

      if (priv->labels    != NULL ||
          priv->geo_marks != NULL ||
          priv->wf_marks  != NULL ||
          priv->tracks    != NULL)
        {
          if (GTK_IS_LIST_STORE (priv->store))
            {
              g_object_unref (priv->store);
              store = gtk_tree_store_new (N_COLUMNS,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_UINT64,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING);
            }
          else
            {
              store = GTK_TREE_STORE (priv->store);
            }
        }
      /* Группы. */
      if (priv->labels != NULL)
        {
          HyScanLabel *object;
          /* Добавляем новый узел в модель */
          gtk_tree_store_append (store, &parent_iter, NULL);
          gtk_tree_store_set (store, &parent_iter,
                              COLUMN_ID, "Labels",
                              -1);

          g_hash_table_iter_init (&table_iter, priv->labels);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
            {
               /* Добавляем новый узел в модель */
               gtk_tree_store_append (store, &child_iter, &parent_iter);
               gtk_tree_store_set (store, &child_iter,
                                   COLUMN_ID,          id,
                                   COLUMN_NAME,        object->name,
                                   COLUMN_DESCRIPTION, object->description,
                                   COLUMN_OPERATOR,    object->operator_name,
                                   COLUMN_LABEL,       object->label,
                                   COLUMN_CTIME,       g_date_time_format (
                                                         g_date_time_new_from_unix_local (object->ctime),
                                                         "%d.%m.%Y %H:%M:%S."),
                                   COLUMN_MTIME,       g_date_time_format (
                                                         g_date_time_new_from_unix_local (object->mtime),
                                                         "%d.%m.%Y %H:%M:%S."),
                                   -1);
            }
        }
      /* Гео-метки. */
      if (priv->geo_marks != NULL)
        {
          HyScanMarkGeo *object;

          gtk_tree_store_append (store, &parent_iter, NULL);
          gtk_tree_store_set (store, &parent_iter,
                              COLUMN_ID, "Geo marks",
                              -1);

          g_hash_table_iter_init (&table_iter, priv->geo_marks);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
            {
               /* Добавляем новый узел в модель */
               gtk_tree_store_append (store, &child_iter, &parent_iter);
               gtk_tree_store_set (store, &child_iter,
                                   COLUMN_ID,          id,
                                   COLUMN_NAME,        object->name,
                                   COLUMN_DESCRIPTION, object->description,
                                   COLUMN_OPERATOR,    object->operator_name,
                                   COLUMN_LABEL,       object->labels,
                                   COLUMN_CTIME,       g_date_time_format (
                                                         g_date_time_new_from_unix_local (
                                                           object->ctime / G_TIME_SPAN_SECOND),
                                                         "%d.%m.%Y %H:%M:%S."),
                                   COLUMN_MTIME,       g_date_time_format (
                                                         g_date_time_new_from_unix_local (
                                                           object->mtime / G_TIME_SPAN_SECOND),
                                                         "%d.%m.%Y %H:%M:%S."),
                                   -1);
            }
        }
      /* "Водопадные" метки. */
      if (priv->wf_marks)
        {
          HyScanMarkLocation *location;

          gtk_tree_store_append (store, &parent_iter, NULL);
          gtk_tree_store_set (store, &parent_iter,
                              COLUMN_ID, "Waterfall marks",
                              -1);

          g_hash_table_iter_init (&table_iter, priv->wf_marks);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &location))
            {
               HyScanMarkWaterfall *object = location->mark;
               /* Добавляем новый узел в модель */
               gtk_tree_store_append (store, &child_iter, &parent_iter);
               gtk_tree_store_set (store, &child_iter,
                                   COLUMN_ID,          id,
                                   COLUMN_NAME,        object->name,
                                   COLUMN_DESCRIPTION, object->description,
                                   COLUMN_OPERATOR,    object->operator_name,
                                   COLUMN_LABEL,       object->labels,
                                   COLUMN_CTIME,       g_date_time_format (
                                                         g_date_time_new_from_unix_local (
                                                           object->ctime / G_TIME_SPAN_SECOND),
                                                         "%d.%m.%Y %H:%M:%S."),
                                   COLUMN_MTIME,       g_date_time_format (
                                                          g_date_time_new_from_unix_local (
                                                            object->mtime / G_TIME_SPAN_SECOND),
                                                          "%d.%m.%Y %H:%M:%S."),
                                   -1);
            }

        }
      /* Галсы. */
      if (priv->tracks != NULL)
        {
          HyScanTrackInfo *object;

          gtk_tree_store_append (store, &parent_iter, NULL);
          gtk_tree_store_set (store, &parent_iter,
                              COLUMN_ID, "Tracks",
                              -1);

          g_hash_table_iter_init (&table_iter, priv->tracks);
          while (g_hash_table_iter_next (&table_iter, (gpointer *) &id, (gpointer *) &object))
            {
               gchar *ctime = (object->ctime == NULL)? "" : g_date_time_format (object->ctime, "%d.%m.%Y %H:%M:%S.");
               gchar *mtime = (object->mtime == NULL)? "" : g_date_time_format (object->mtime, "%d.%m.%Y %H:%M:%S.");
               guint64 labels = 0;
               /* Добавляем новый узел в модель */
               gtk_tree_store_append (store, &child_iter, &parent_iter);
               gtk_tree_store_set (store, &child_iter,
                                   COLUMN_ID,          id,
                                   COLUMN_NAME,        object->name,
                                   COLUMN_DESCRIPTION, object->description,
                                   COLUMN_OPERATOR,    object->operator_name,
                                   COLUMN_LABEL,       labels,
                                   COLUMN_CTIME,       ctime,
                                   COLUMN_MTIME,       mtime,
                                   -1);
            }
        }

      if (store != NULL)
        priv->store = GTK_TREE_MODEL (store);
    }
  /* Передаём модель в представление. */
  if (priv->tree_view == NULL)
    {
      GtkTreeSelection *selection;
      priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (priv->store));
      renderer = gtk_cell_renderer_text_new ();
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_ID,
                                                   "ID", renderer,
                                                   "text", COLUMN_ID,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_NAME,
                                                   "Name", renderer,
                                                   "text", COLUMN_NAME,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_DESCRIPTION,
                                                   "Description", renderer,
                                                   "text", COLUMN_DESCRIPTION,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_OPERATOR,
                                                   "Operator name", renderer,
                                                   "text", COLUMN_OPERATOR,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_LABEL,
                                                   "Label", renderer,
                                                   "text", COLUMN_LABEL,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_CTIME,
                                                   "CTime", renderer,
                                                   "text", COLUMN_CTIME,
                                                   NULL);
      gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                                   COLUMN_MTIME,
                                                   "MTime", renderer,
                                                   "text", COLUMN_MTIME,
                                                   NULL);

      selection = gtk_tree_view_get_selection (priv->tree_view);
      /* Разрешаем множественный выбор. */
      gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
      /* Соединяем сигнал изменения выбранных элементов с функцией-обработчиком. */
      priv->signal = g_signal_connect (G_OBJECT (selection), "changed",
                                       G_CALLBACK (hyscan_mark_manager_view_emit_selected), self);
    }
  else
    {
      gtk_tree_view_set_model (priv->tree_view, priv->store);
    }
}

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


/**
 * hyscan_mark_manager_view_set_grouping:
 * @self: указатель на структуру HyScanMarkManagerView
 * @grouping: тип представления
 *
 * Устанавливает тип представления.
 */
void
hyscan_mark_manager_view_set_grouping  (HyScanMarkManagerView     *self,
                                        HyScanMarkManagerGrouping  grouping)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->grouping = grouping;
  g_print ("Set grouping\n");
  hyscan_mark_manager_view_update_store (self);
}

/**
 * hyscan_mark_manager_view_get_grouping:
 * @self: указатель на структуру HyScanMarkManagerView
 *
 * Returns: текущиий тип предствления
 *
 * Возвращает актуальный тип представления.
 */
HyScanMarkManagerGrouping hyscan_mark_manager_view_get_grouping (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return priv->grouping;
}

/**
 * hyscan_mark_manager_view_update_groups:
 * @self: указатель на структуру HyScanMarkManagerView
 * @groups: указатель на таблицу с данными о группах
 *
 * Обновляет данные о группах.
 */
void
hyscan_mark_manager_view_update_labels (HyScanMarkManagerView *self,
                                        GHashTable            *labels)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->labels = labels;
  hyscan_mark_manager_view_update_store (self);
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
  hyscan_mark_manager_view_update_store (self);
}

/**
 * hyscan_mark_manager_view_update_wf_marks:
 * @self: указатель на структуру HyScanMarkManagerView
 * @wf_marks: указатель на таблицу с данными о "водопадных" метках
 *
 * Обновляет данные о "водопадных" метках.
 */
void
hyscan_mark_manager_view_update_wf_marks (HyScanMarkManagerView *self,
                                          GHashTable            *wf_marks)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  priv->wf_marks = wf_marks;
  hyscan_mark_manager_view_update_store (self);
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
  hyscan_mark_manager_view_update_store (self);
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
  if (GTK_IS_TREE_STORE (priv->store))
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
  if (GTK_IS_TREE_STORE (priv->store))
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
  if (GTK_IS_TREE_STORE (priv->store))
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


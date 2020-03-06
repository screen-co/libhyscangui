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
  PROP_STORE = 1,     /* Модель представления данных. */
  N_PROPERTIES
};

/* Сигналы. */
enum
{
  SIGNAL_SELECTED,   /* Выделение строки. */
  SIGNAL_TOGGLED,    /* Изменение состояня чек-бокса. */
  SIGNAL_LAST
};

struct _HyScanMarkManagerViewPrivate
{
  GtkTreeView               *tree_view;       /* Представление. */
  GtkTreeModel              *store;           /* Модель представления данных (табличное или древовидное). */
  gulong                     signal_selected; /* Идентификатор сигнала об изменении выбранных объектов. */
  gboolean                   toggle_flag;     /* Флаг для запрета выделения строки при переключении состояния чек-бокса. */
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

static void       hyscan_gtk_mark_manager_view_select_track       (GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   GtkTreeSelection      *selection);

static void       on_toggle_renderer_clicked                      (GtkCellRendererToggle *cell_renderer,
                                                                   gchar                 *path,
                                                                   gpointer               user_data);

static void       hyscan_gtk_mark_manager_view_toggle_children    (GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   gboolean               active);

static void       set_list_model                                  (HyScanMarkManagerView *self);

static void       set_tree_model                                  (HyScanMarkManagerView *self);

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

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkManagerView, hyscan_mark_manager_view, GTK_TYPE_SCROLLED_WINDOW)

void
hyscan_mark_manager_view_class_init (HyScanMarkManagerViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_manager_view_set_property;
  object_class->constructed  = hyscan_mark_manager_view_constructed;
  object_class->finalize     = hyscan_mark_manager_view_finalize;

  /* Модель представления данных. */
  g_object_class_install_property (object_class, PROP_STORE,
    g_param_spec_object ("store", "Store", "View model",
                         GTK_TYPE_TREE_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanMarkManagerView::selected:
   * @self: указатель на #HyScanMarkManagerView
   *
   * Сигнал посылается при выделении строки.
   */
  hyscan_mark_manager_view_signals[SIGNAL_SELECTED] =
    g_signal_new ("selected", HYSCAN_TYPE_MARK_MANAGER_VIEW, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_TREE_SELECTION);

  /**
   * HyScanMarkManagerView::toggled:
   * @self: указатель на #HyScanMarkManagerView
   *
   * Сигнал посылается при изменении состояния чек-бокса.
   */
  hyscan_mark_manager_view_signals[SIGNAL_TOGGLED] =
    g_signal_new ("toggled", HYSCAN_TYPE_MARK_MANAGER_VIEW, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
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
      /* Модель представления данных. */
      case PROP_STORE:
        {
        	priv->store = g_value_dup_object (value);
          /* Увеличиваем счётчик ссылок на базу данных. */
          g_object_ref (priv->store);
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
hyscan_mark_manager_view_constructed (GObject *object)
{
  HyScanMarkManagerView        *self      = HYSCAN_MARK_MANAGER_VIEW (object);
  HyScanMarkManagerViewPrivate *priv      = self->priv;
  GtkScrolledWindow            *widget    = GTK_SCROLLED_WINDOW (object);

  /* Рамка со скошенными внутрь границами. */
  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

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

  G_OBJECT_CLASS (hyscan_mark_manager_view_parent_class)->finalize (object);
}

/* Функция обновляет представление. */
void
hyscan_mark_manager_view_update (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;

  if (GTK_IS_LIST_STORE (priv->store))
    {
      /* Передаём модель в представление. */
      if (priv->tree_view == NULL)
        {
          GtkTreeSelection  *selection;

          priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

          set_list_model (self);

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
        }
    }
  else if (GTK_IS_TREE_STORE (priv->store))
    {
      /* Передаём модель в представление. */
      if (priv->tree_view == NULL)
        {
          GtkTreeSelection  *selection;

          priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

          set_tree_model (self);

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
        }
    }
}

/* Обработчик выделеня строки в списке. */
gboolean
hyscan_mark_manager_view_emit_selected (GtkTreeSelection      *selection,
                                        HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), G_SOURCE_REMOVE);

  priv = self->priv;

  if (priv->toggle_flag)
    {
      if (gtk_tree_selection_count_selected_rows (selection))
        {
          gtk_tree_selection_unselect_all (selection);
        }
      priv->toggle_flag = FALSE;
      return G_SOURCE_REMOVE;
    }

  if (gtk_tree_selection_count_selected_rows (selection))
    {
      g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_SELECTED], 0, selection);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

/* Функция рекурсивного выделения галсов. */
void
hyscan_gtk_mark_manager_view_select_track (GtkTreeModel     *model,
                                           GtkTreeIter      *iter,
                                           GtkTreeSelection *selection)
{
  GtkTreeIter child_iter;

  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      do
        {
          g_print ("Child_iter: %p\n", &child_iter);
          gtk_tree_selection_select_iter (selection, &child_iter);
          hyscan_gtk_mark_manager_view_select_track (model, &child_iter, selection);
        }
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
}

/* Обработчик клика по чек-боксу. */
void
on_toggle_renderer_clicked (GtkCellRendererToggle *cell_renderer,
                            gchar                 *path,
                            gpointer               user_data)
{
  HyScanMarkManagerView *self;
  HyScanMarkManagerViewPrivate *priv;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  g_return_if_fail (HYSCAN_IS_MARK_MANAGER_VIEW (user_data));

  self = HYSCAN_MARK_MANAGER_VIEW (user_data);
  priv = self->priv;

  priv->toggle_flag = TRUE; /* Защита от выделения строки по сигналу "selected". */

  model = gtk_tree_view_get_model (priv->tree_view);

  if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);
      GtkTreeIter       child_iter;
      gboolean          active;

      gtk_tree_model_get (model, &iter, COLUMN_ACTIVE, &active, -1);

      /* Отключаем сигнал об изменении выбранных объектов. */
/*      if (priv->signal_selected != 0 && selection != NULL)
        {
          g_signal_handler_block (G_OBJECT (selection), priv->signal_selected);
        }*/

      if (GTK_IS_TREE_STORE (model))
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COLUMN_ACTIVE, !active, -1);
      else if (GTK_IS_LIST_STORE (model))
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACTIVE, !active, -1);
      else
        return;

      /* Если есть дочерние объекты. */
      if (gtk_tree_model_iter_children (model, &child_iter, &iter))
        {
          do
            {
              hyscan_gtk_mark_manager_view_toggle_children (model, &child_iter, !active);
            }
          while (gtk_tree_model_iter_next (model, &child_iter));
        }

      /* Отправляем сигнал об изменении состояния чек-бокса. */
      g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_TOGGLED], 0, selection);

      /* Включаем сигнал об изменении выбранных объектов. */
/*      if (priv->signal_selected != 0 && selection != NULL)
        {
          g_signal_handler_unblock (G_OBJECT (selection), priv->signal_selected);
        }*/
    }
}

void
hyscan_gtk_mark_manager_view_toggle_children (GtkTreeModel *model,
                                              GtkTreeIter  *iter,
                                              gboolean      active)
{
  GtkTreeIter child_iter;
  if (GTK_IS_TREE_STORE (model))
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, COLUMN_ACTIVE, active, -1);
  else if (GTK_IS_LIST_STORE (model))
    gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_ACTIVE, active, -1);
  else
    return;
  /* Если есть дочерние объекты. */
  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      do
        {
          hyscan_gtk_mark_manager_view_toggle_children (model, &child_iter, active);
        }
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
}
/* Функция устанавливает модель для табличного представления. */
void
set_list_model (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeViewColumn *column          = gtk_tree_view_column_new();                  /* Колонка для ViewItem-а. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new(),              /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new();              /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */

  /* Удаляем все колонки. */
  g_list_foreach (list, remove_column, priv->tree_view);
  /* Освобождаем список колонок. */
  g_list_free (list);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (on_toggle_renderer_clicked),
                    self);

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
  gtk_tree_view_append_column (priv->tree_view, column);

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
/*
  gtk_tree_view_insert_column_with_attributes (list_view,
                                               COLUMN_LABEL,
                                               "Label", renderer,
                                               "text", COLUMN_LABEL,
                                               NULL);
*/
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_CTIME,
                                               "Created", renderer,
                                               "text", COLUMN_CTIME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_MTIME,
                                               "Modified", renderer,
                                               "text", COLUMN_MTIME,
                                               NULL);
  /* Показать заголовки. */
  gtk_tree_view_set_headers_visible (priv->tree_view, TRUE);
  /* Скрыть ветви линиями. */
  gtk_tree_view_set_enable_tree_lines (priv->tree_view, FALSE);
}

/* Функция устанавливает модель для древовидного представления. */
void
set_tree_model (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeViewColumn *column          = gtk_tree_view_column_new();                  /* Колонка для ViewItem-а. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new(),              /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new();              /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */

  /* Удаляем все колонки. */
  g_list_foreach (list, remove_column, priv->tree_view);
  /* Освобождаем список колонок. */
  g_list_free (list);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (on_toggle_renderer_clicked),
                    self);

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
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           macro_set_func_text,
                                           NULL,
                                           NULL);
  /* Добавляем текст. */
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  /* Добавляем колонку в список. */
  gtk_tree_view_append_column (priv->tree_view, column);
/*
  gtk_tree_view_insert_column_with_attributes (tree_view,
                                               COLUMN_TYPE,
                                               "Objects", renderer,
                                               "text", COLUMN_NAME,
                                               NULL);
*/
  /* Скрыть заголовки. */
  gtk_tree_view_set_headers_visible (priv->tree_view, FALSE);
  /* Показывать ветви линиями. */
  gtk_tree_view_set_enable_tree_lines (priv->tree_view, TRUE);
  /* Назначаем расширители для первого столбца. */
  /*column = gtk_tree_view_get_column (tree_view, COLUMN_TYPE);
  gtk_tree_view_set_expander_column (tree_view, column);*/
}

/* Функция удаляет колонку из представления. */
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
  gtk_tree_model_get (model, iter, COLUMN_ICON,  &icon_name, -1);
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
 * hyscan_mark_manager_view_get_selection:
 * @self: указатель на структуру #HyScanMarkManagerView
 *
 * Returns: указатель на объект выбора.
 */
GtkTreeSelection*
hyscan_mark_manager_view_get_selection (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return gtk_tree_view_get_selection (priv->tree_view);
}

/**
 * hyscan_mark_manager_view_has_selected:
 * @self: указатель на структуру #HyScanMarkManagerView
 *
 * Returns: TRUE - если есть выделенные строки,
 *          FALSE - если нет выделенных строк.
 */
gboolean
hyscan_mark_manager_view_has_selected (HyScanMarkManagerView *self)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  return (gboolean)gtk_tree_selection_count_selected_rows (gtk_tree_view_get_selection (priv->tree_view));
}

/**
 * hyscan_mark_manager_view_expand_all:
 * @self: указатель на структуру #HyScanMarkManagerView
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
 * @self: указатель на структуру #HyScanMarkManagerView
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
 * @self: указатель на структуру #HyScanMarkManagerView
 * @flag: TRUE - выделяет все объекты,
 *        FALSE - снимает выделение со всех объектов
 *
 * Выделяет все объекты или снимает выделение со всех объектов.
 */
void
hyscan_mark_manager_view_select_all (HyScanMarkManagerView *self,
                                     gboolean               flag)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);

  if (flag)
    gtk_tree_selection_select_all (selection);
  else
    gtk_tree_selection_unselect_all (selection);

  g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_SELECTED], 0, selection);
}

/**
 * hyscan_mark_manager_view_toggle_all:
 * @self: указатель на структуру #HyScanMarkManagerView
 * @active: состояние чек-бокса
 *
 * Устанавливает состояние чек-бокса для всех объектов.
 */
void
hyscan_mark_manager_view_toggle_all (HyScanMarkManagerView *self,
                                     gboolean               active)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);

      do
        {
          GtkTreeIter child_iter;

          if (GTK_IS_TREE_STORE (model))
            gtk_tree_store_set(GTK_TREE_STORE(model), &iter, COLUMN_ACTIVE, active, -1);
          else if (GTK_IS_LIST_STORE (model))
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACTIVE, active, -1);
          else
            return;

          /* Если есть дочерние объекты. */
          if (gtk_tree_model_iter_children (model, &child_iter, &iter))
            {
              do
                {
                  hyscan_gtk_mark_manager_view_toggle_children (model, &child_iter, active);
                }
              while (gtk_tree_model_iter_next (model, &child_iter));
            }
        }
      while (gtk_tree_model_iter_next (model, &iter));

      /* Отправляем сигнал об изменении состояния чек-бокса. */
      g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_TOGGLED], 0, selection);
    }
}

/**
 * hyscan_mark_manager_view_expand_to_path:
 * @self: указатель на структуру #HyScanMarkManagerView
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
      if (gtk_tree_path_up (path))
        {
          /*gtk_tree_path_down (path);*/
          /*gtk_tree_view_expand_to_path (priv->tree_view, path);*/
          if (gtk_tree_view_expand_row (priv->tree_view, path, TRUE))
            g_print ("Has children (TRUE)\n");
          else
            g_print ("Hasn't children (FALSE)\n");
        }
    }
}

/**
 * hyscan_mark_manager_view_get_toggled:
 * @self: указатель на структуру #HyScanMarkManagerView
 *
 * Returns: возвращает список идентификаторов объектов
 * с активированным чек-боксом. Тип объекта определяется
 * #ModelManagerObjectType. Когда список больше не нужен,
 * необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_mark_manager_view_get_toggled (HyScanMarkManagerView *self,
                                      ModelManagerObjectType type)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeIter iter;
  GtkTreePath *path;
  gchar **list = NULL;

  g_return_val_if_fail (type != TYPES, NULL);

  if (GTK_IS_LIST_STORE (priv->store))
    {
      path = gtk_tree_path_new_from_indices (0, -1);

      if (gtk_tree_model_get_iter (priv->store, &iter, path))
        {
          do
            {
              gchar    *id;              /* Идентификатор галса в базе данных. */
              gboolean  active;
              ModelManagerObjectType current_type;

              gtk_tree_model_get (priv->store,   &iter,
                                  COLUMN_ID,     &id,
                                  COLUMN_ACTIVE, &active,
                                  COLUMN_TYPE,   &current_type,
                                  -1);
              if (current_type == type && active)
                {
                  guint i = (list != NULL)? g_strv_length (list) : 0;
                  list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
                  list[i++] = g_strdup (id);
                  list[i++] = NULL;
                }
            }
          while (gtk_tree_model_iter_next (priv->store, &iter));
        }
    }
  else if (GTK_IS_TREE_STORE (priv->store))
    {
      path = gtk_tree_path_new_from_indices (type, 0, -1);

      if (gtk_tree_model_get_iter (priv->store, &iter, path))
        {
          do
            {
              gchar    *id;              /* Идентификатор галса в базе данных. */
              gboolean  active;

              gtk_tree_model_get (priv->store,   &iter,
                                  COLUMN_ID,     &id,
                                  COLUMN_ACTIVE, &active,
                                  -1);
              if (active)
                {
                  guint i = (list != NULL)? g_strv_length (list) : 0;
                  list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
                  list[i++] = g_strdup (id);
                  list[i++] = NULL;
                }
            }
          while (gtk_tree_model_iter_next (priv->store, &iter));
        }
    }

  return list;
}

/**
 * hyscan_mark_manager_view_new:
 * @model: модель представления данных
 *
 * Returns: cоздаёт новый экземпляр #HyScanMarkManageView
 */
GtkWidget*
hyscan_mark_manager_view_new (GtkTreeModel *store)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_MARK_MANAGER_VIEW,
                                   "store", store,
                                   NULL));
}

/**
 * hyscan_mark_manager_view_set_store:
 * @self: указатель на структуру #HyScanMarkManagerView
 * @model: модель представления данных
 *
 * Устанавливает модель представления данных. */
void
hyscan_mark_manager_view_set_store (HyScanMarkManagerView *self,
                                    GtkTreeModel          *store)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;

  /*g_mutex_lock (&priv->mutex);*/

  priv->store = store;
  if (GTK_IS_TREE_STORE (priv->store))
    set_tree_model (self);
  else if (GTK_IS_LIST_STORE (priv->store))
    set_list_model (self);
  hyscan_mark_manager_view_update (self);
}

/**
 * hyscan_mark_manager_view_set_selection:
 * @self: указатель на структуру #HyScanMarkManagerView
 * @selection: указатель на список выделенных объектов.
 *
 * Устанавливает выделенные объекты.
 */
void
hyscan_mark_manager_view_set_selection (HyScanMarkManagerView *self,
                                        GtkTreeSelection      *selection)
{
  HyScanMarkManagerViewPrivate *priv = self->priv;
  GtkTreeSelection *current_selection;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  current_selection = gtk_tree_view_get_selection (priv->tree_view);

  /* Выделяем объекты. */
  /*if (selection != current_selection)
    {*/
      GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
      GList *list = gtk_tree_selection_get_selected_rows (selection, &model);

      /* Отключаем сигнал об изменении выбранных объектов. */
      if (priv->signal_selected != 0 && current_selection != NULL)
        {
          GList *ptr = g_list_first (list);
          g_signal_handler_block (G_OBJECT (current_selection), priv->signal_selected);

          gtk_tree_selection_unselect_all (current_selection);

          while (ptr != NULL)
            {
              GtkTreeIter iter;
              GtkTreePath *path = (GtkTreePath*)ptr->data;

              gtk_tree_selection_select_path (current_selection, path);

              if (gtk_tree_model_get_iter (model, &iter, path))
                {
                  hyscan_gtk_mark_manager_view_select_track (model, &iter, selection);
                }
              ptr = g_list_next (ptr);
            }

          /* Включаем сигнал об изменении выбранных объектов. */
          g_signal_handler_unblock (G_OBJECT (current_selection), priv->signal_selected);
        }

      g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
    /*}*/
}

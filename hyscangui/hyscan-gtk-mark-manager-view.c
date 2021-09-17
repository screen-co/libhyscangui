/* hyscan-gtk-mark-manager-view.c
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
 * SECTION: hyscan-gtk-mark-manager-view
 * @Short_description: Класс реализует представление Журнала Меток
 * @Title: HyScanGtkMarkManagerView
 * @See_also: #HyScanGtkMarkManager, #HyScanGtkModelManager
 *
 * Класс #HyScanGtkMarkManagerView реализует отображение данных в табличном
 * и древовидном виде с группировкой по типам и группам.
 *
 * - hyscan_gtk_mark_manager_view_new () - создание нового представления;
 * - hyscan_gtk_mark_manager_view_set_store () - установка модели с данными для отображения;
 * - hyscan_gtk_mark_manager_view_expand_all () - развернуть все узлы древовидного представления;
 * - hyscan_gtk_mark_manager_view_collapse_all () - cвернуть все узлы древовидного представления;
 * - hyscan_gtk_mark_manager_view_unselect_all () - снимает выделение с объектов;
 * - hyscan_gtk_mark_manager_view_toggle_all () - устанавливает состояние чек-бокса для всех объектов;
 * - hyscan_gtk_mark_manager_view_expand_path () - разворачивает или сворачивает узел по заданному пути;
 * - hyscan_gtk_mark_manager_view_get_toggled () - возвращает список идентификаторов объектов
 * с активированным чек-боксом;
 * - hyscan_gtk_mark_manager_view_select_item () - выделяет объект;
 * - hyscan_gtk_mark_manager_view_find_item_by_id () - поиск объекта по идентификтору.
 */
#include <hyscan-gtk-mark-manager-view.h>
#include <hyscan-mark.h>
#include <hyscan-mark-location.h>
#include <hyscan-db-info.h>
#include <hyscan-gui-marshallers.h>
#include <glib/gi18n-lib.h>

enum
{
  PROP_STORE = 1,  /* Модель представления данных. */
  N_PROPERTIES
};

/* Сигналы. */
enum
{
  SIGNAL_SELECTED, /* Выделение строки. */
  SIGNAL_UNSELECT, /* Снять выделение. */
  SIGNAL_TOGGLED,  /* Изменение состояня чек-бокса. */
  SIGNAL_EXPANDED, /* Разворачивание узла древовидного представления. */
  SIGNAL_LAST
};

struct _HyScanGtkMarkManagerViewPrivate
{
  GtkTreeView      *tree_view;        /* Представление. */
  GtkTreeModel     *store;            /* Модель представления данных (табличное или древовидное). */
  GtkTreeIter       selected_iter;    /* Список выделенных объектов. Используется для временного хранения
                                       * чтобы правильно выделять объекты при установке фокуса. */
  gulong            signal_selected,  /* Идентификатор сигнала об изменении выбранных объектов. */
                    signal_expanded,  /* Идентификатор сигнала разворачивания узла древовидного представления.*/
                    signal_collapsed; /* Идентификатор сигнала cворачивания узла древовидного представления.*/
  gboolean          has_selected,     /* Флаг наличия выделенного объекта. */
                    toggle_flag,      /* Флаг для отмены выделения при клике по чек-боксу. */
                    focus_start,      /* Флаг получения первого фокуса. */
                    is_selected;      /* Текущее состояние: TRUE - выделено, FALSE - не выделено. */
};

static void       hyscan_gtk_mark_manager_view_set_property       (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   const GValue             *value,
                                                                   GParamSpec               *pspec);

static void       hyscan_gtk_mark_manager_view_constructed        (GObject                  *object);

static void       hyscan_gtk_mark_manager_view_finalize           (GObject                  *object);

static void       hyscan_gtk_mark_manager_view_update             (HyScanGtkMarkManagerView *self);

static void       hyscan_gtk_mark_manager_view_emit_selected      (GtkTreeSelection         *selection,
                                                                   HyScanGtkMarkManagerView *self);

static void       hyscan_gtk_mark_manager_view_on_toggle          (GtkCellRendererToggle    *cell_renderer,
                                                                   gchar                    *path,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_toggle             (HyScanGtkMarkManagerView *self,
                                                                   GtkTreeIter              *iter,
                                                                   gboolean                  active);

static void       hyscan_gtk_mark_manager_view_toggle_parent      (HyScanGtkMarkManagerView *self,
                                                                   GtkTreeIter              *iter);

static gboolean   hyscan_gtk_mark_manager_view_show_tooltip       (GtkWidget                *widget,
                                                                   gint                      x,
                                                                   gint                      y,
                                                                   gboolean                  keyboard_mode,
                                                                   GtkTooltip               *tooltip,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_item_expanded      (GtkTreeView              *tree_view,
                                                                   GtkTreeIter              *iter,
                                                                   GtkTreePath              *path,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_item_collapsed     (GtkTreeView              *tree_view,
                                                                   GtkTreeIter              *iter,
                                                                   GtkTreePath              *path,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_set_list_model     (HyScanGtkMarkManagerView *self);

static void       hyscan_gtk_mark_manager_view_set_tree_model     (HyScanGtkMarkManagerView *self);

static void       hyscan_gtk_mark_manager_view_remove_column      (gpointer                  data,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_set_func_icon      (GtkTreeViewColumn        *tree_column,
                                                                   GtkCellRenderer          *cell,
                                                                   GtkTreeModel             *model,
                                                                   GtkTreeIter              *iter,
                                                                   gpointer                  data);

static void       hyscan_gtk_mark_manager_view_set_func_toggle    (GtkTreeViewColumn        *tree_column,
                                                                   GtkCellRenderer          *cell,
                                                                   GtkTreeModel             *model,
                                                                   GtkTreeIter              *iter,
                                                                   gpointer                  data);

static void       hyscan_gtk_mark_manager_view_set_func_text      (GtkTreeViewColumn        *tree_column,
                                                                   GtkCellRenderer          *cell,
                                                                   GtkTreeModel             *model,
                                                                   GtkTreeIter              *iter,
                                                                   gpointer                  data);

static void       hyscan_gtk_mark_manager_view_grab_focus         (GtkWidget                *widget,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_grab_focus_after   (GtkWidget                *widget,
                                                                   gpointer                  user_data);

static gboolean   hyscan_gtk_mark_manager_view_select_func        (GtkTreeSelection         *selection,
                                                                   GtkTreeModel             *model,
                                                                   GtkTreePath              *path,
                                                                   gboolean                  path_currently_selected,
                                                                   gpointer                  data);

static void       hyscan_gtk_mark_manager_view_on_show            (GtkWidget                *widget,
                                                                   gpointer                  user_data);

static guint      hyscan_gtk_mark_manager_view_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMarkManagerView, hyscan_gtk_mark_manager_view, GTK_TYPE_SCROLLED_WINDOW)

void
hyscan_gtk_mark_manager_view_class_init (HyScanGtkMarkManagerViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_mark_manager_view_set_property;
  object_class->constructed  = hyscan_gtk_mark_manager_view_constructed;
  object_class->finalize     = hyscan_gtk_mark_manager_view_finalize;

  /* Модель представления данных. */
  g_object_class_install_property (object_class, PROP_STORE,
    g_param_spec_object ("store", "Store", "View model",
                         GTK_TYPE_TREE_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanGtkMarkManagerView::selected:
   * @self: указатель на #HyScanGtkMarkManagerView
   *
   * Сигнал посылается при выделении строки.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_SELECTED] =
    g_signal_new ("selected", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * HyScanGtkMarkManagerView::unselect:
   * @self: указатель на #HyScanGtkMarkManagerView
   *
   * Сигнал посылается при снятии выделения.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_UNSELECT] =
    g_signal_new ("unselect", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * HyScanGtkMarkManagerView::toggled:
   * @self: указатель на #HyScanGtkMarkManagerView
   *
   * Сигнал посылается при изменении состояния чек-бокса.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED] =
    g_signal_new ("toggled", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /**
     * HyScanGtkMarkManagerView::expanded:
     * @self: указатель на #HyScanGtkMarkManagerView
     *
     * Сигнал посылается при разворачивании узла древовидного представления.
     */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED] =
    g_signal_new ("expanded", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

void
hyscan_gtk_mark_manager_view_init (HyScanGtkMarkManagerView *self)
{
  self->priv = hyscan_gtk_mark_manager_view_get_instance_private (self);
  /* Костыль для снятия выделния по умолчанию. */
  self->priv->focus_start = TRUE;
}

void
hyscan_gtk_mark_manager_view_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  HyScanGtkMarkManagerView *self        = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  switch (prop_id)
    {
      /* Модель представления данных. */
      case PROP_STORE:
        priv->store = g_value_dup_object (value);
        break;
      /* Что-то ещё... */
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}
/* Конструктор. */
void
hyscan_gtk_mark_manager_view_constructed (GObject *object)
{
  HyScanGtkMarkManagerView        *self   = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv   = self->priv;
  GtkScrolledWindow               *widget = GTK_SCROLLED_WINDOW (object);
  /* Дефолтный конструктор родительского класса. */
  G_OBJECT_CLASS (hyscan_gtk_mark_manager_view_parent_class)->constructed (object);
  /* Рамка со скошенными внутрь границами. */
  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

  hyscan_gtk_mark_manager_view_update (self);

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->tree_view));

  /* Включаем отображение подсказок. */
  gtk_widget_set_has_tooltip (GTK_WIDGET (priv->tree_view), TRUE);
  /* Соединяем сигнал для показа подсказки с функцией-обработчиком */
  g_signal_connect (G_OBJECT (priv->tree_view), "query-tooltip",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_show_tooltip), NULL);
  /* Соединяем сигнал разворачивания узла с функцией-обработчиком. */
  priv->signal_expanded  = g_signal_connect (priv->tree_view,
                                             "row-expanded",
                                             G_CALLBACK (hyscan_gtk_mark_manager_view_item_expanded),
                                             self);
  /* Соединяем сигнал cворачивания узла с функцией-обработчиком. */
  priv->signal_collapsed = g_signal_connect (priv->tree_view,
                                             "row-collapsed",
                                             G_CALLBACK (hyscan_gtk_mark_manager_view_item_collapsed),
                                             self);
  /* * * * * * * * * * * * * * * * * * * * * * * */
  /* Костыль для правильного выделения объектов. */
  /* * * * * * * * * * * * * * * * * * * * * * * */
  /* Обработчик показа виджета.
   * Устанваливает фокус первый раз для правильной работы при последующих вызовах. */
  g_signal_connect_after (G_OBJECT (priv->tree_view), "show",
                          G_CALLBACK (hyscan_gtk_mark_manager_view_on_show), self);
  /* В обработчике получения фокуса сохраняется итератор выделенного объекта. */
  g_signal_connect (G_OBJECT (priv->tree_view), "grab-focus",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_grab_focus), self);
  /* В пост-обработчике получения фокуса выделяются объект по ранее сохранённому итератору. */
  g_signal_connect_after (G_OBJECT (priv->tree_view), "grab-focus",
                          G_CALLBACK (hyscan_gtk_mark_manager_view_grab_focus_after), self);
  /* * * * * * * * * * * * * * * * * * * * * * * */
  /* Костыль для правильного выделения объектов. */
  /* * * * * * * * * * * * * * * * * * * * * * * */
}

/* Деструктор. */
void
hyscan_gtk_mark_manager_view_finalize (GObject *object)
{
  HyScanGtkMarkManagerView *self = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  /* Освобождаем ресурсы. */
  g_object_unref (priv->store);
  priv->store = NULL;

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_view_parent_class)->finalize (object);
}

/* Функция обновляет представление. */
void
hyscan_gtk_mark_manager_view_update (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (GTK_IS_LIST_STORE (priv->store))
    {
      /* Передаём модель в представление. */
      if (priv->tree_view == NULL)
        {
          GtkTreeSelection  *selection;

          priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

          hyscan_gtk_mark_manager_view_set_list_model (self);

          selection = gtk_tree_view_get_selection (priv->tree_view);

          gtk_tree_selection_set_select_function (selection,
                                                  hyscan_gtk_mark_manager_view_select_func,
                                                  self,
                                                  NULL);
          /* Соединяем сигнал изменения выбранных элементов с функцией-обработчиком. */
          priv->signal_selected = g_signal_connect (selection, "changed",
                                                    G_CALLBACK (hyscan_gtk_mark_manager_view_emit_selected), self);
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

          hyscan_gtk_mark_manager_view_set_tree_model (self);

          selection = gtk_tree_view_get_selection (priv->tree_view);

          gtk_tree_selection_set_select_function (selection,
                                                  hyscan_gtk_mark_manager_view_select_func,
                                                  self,
                                                  NULL);
          /* Соединяем сигнал изменения выбранных элементов с функцией-обработчиком. */
          priv->signal_selected = g_signal_connect (G_OBJECT (selection), "changed",
                                                    G_CALLBACK (hyscan_gtk_mark_manager_view_emit_selected), self);
        }
      else
        {
          gtk_tree_view_set_model (priv->tree_view, GTK_TREE_MODEL (priv->store));
        }
    }
}

/* Обработчик выделения строки в списке. */
void
hyscan_gtk_mark_manager_view_emit_selected (GtkTreeSelection         *selection,
                                            HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  priv = self->priv;

  if (priv->is_selected)
    {
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_UNSELECT], 0);
    }
  else
    {
      model = gtk_tree_view_get_model (priv->tree_view);

      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          gchar *id = NULL;

          gtk_tree_model_get (model,     &iter,
                              COLUMN_ID, &id,
                              -1);

          if (id != NULL && 0 != g_strcmp0 (id, ""))
            {
              g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_SELECTED], 0, id);
              g_free (id);
            }
        }
    }
}

/* Обработчик клика по чек-боксу. */
void
hyscan_gtk_mark_manager_view_on_toggle (GtkCellRendererToggle *cell_renderer,
                                        gchar                 *path,
                                        gpointer               user_data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter   iter;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (user_data));

  self  = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  priv  = self->priv;
  model = gtk_tree_view_get_model (priv->tree_view);

  if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
      GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
      gboolean active;

      gtk_tree_model_get (model,         &iter,
                          COLUMN_ACTIVE, &active,
                          -1);

      hyscan_gtk_mark_manager_view_toggle (self, &iter, !active);
      /* Обновляем указаетль на модель и итератор, т.к. модель могла обновиться. */
      model = gtk_tree_view_get_model (priv->tree_view);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);

      if (GTK_IS_TREE_STORE (model))
        hyscan_gtk_mark_manager_view_toggle_parent (self, &iter);

      priv->toggle_flag = TRUE;
    }
}

/* Устанавливает состояние чек-бокса, в том числе и дочерним объектам. */
void
hyscan_gtk_mark_manager_view_toggle (HyScanGtkMarkManagerView *self,
                                     GtkTreeIter              *iter,
                                     gboolean                  active)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter child_iter;
  gchar *id;

  gtk_tree_model_get (model,      iter,
                      COLUMN_ID, &id,
                      -1);

  if (id != NULL)
    {
      GtkTreePath *path = gtk_tree_model_get_path (model, iter);
      /* Отправляем сигнал об изменении состояния чек-бокса. */
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, active);
      g_free (id);
      /* Обновляем указатель на модель и итератор, т.к. по сигналу модель пересоздаётся. */
      model = gtk_tree_view_get_model (priv->tree_view);
      gtk_tree_model_get_iter (model, iter, path);
      gtk_tree_path_free (path);
    }
  /* Если есть дочерние объекты. */
  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      do
        hyscan_gtk_mark_manager_view_toggle (self, &child_iter, active);
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
}

/* Устанавливает состояние чек-бокса родительских элементов
 * рекурсивно до самого верхнего. Если отмечены все дочерние,
 * то и родительский отмечается.
 * */
void
hyscan_gtk_mark_manager_view_toggle_parent (HyScanGtkMarkManagerView *self,
                                            GtkTreeIter              *iter)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter parent_iter;

  if (gtk_tree_model_iter_parent (model, &parent_iter, iter))
    {
      GtkTreeIter child_iter;

      if (gtk_tree_model_iter_children (model, &child_iter, &parent_iter))
        {
          gint total   = gtk_tree_model_iter_n_children  (model, &parent_iter),
               counter = 0;
          gchar *id;
          gboolean flag;

          do
            {
              gtk_tree_model_get (model,         &child_iter,
                                  COLUMN_ACTIVE, &flag,
                                  -1);
              if (flag)
                counter++; /* Считаем отмеченые. */
            }
          while (gtk_tree_model_iter_next (model, &child_iter));

          if (counter == total)
            flag = TRUE; /* Все дочерние отмечены. */
          else
            flag = FALSE;

          gtk_tree_model_get (model,         &parent_iter,
                              COLUMN_ID,     &id,
                              -1);
          gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter, COLUMN_ACTIVE, flag, -1);

          if (id != NULL)
            {
              GtkTreePath *path = gtk_tree_model_get_path (model, iter),
                          *parent_path = gtk_tree_model_get_path (model, &parent_iter);
              /* Отправляем сигнал об изменении состояния чек-бокса. */
              g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, flag);
              g_free (id);
              /* Обновляем указатель на модель и итераторы, т.к. по сигналу модель пересоздаётся. */
              model = gtk_tree_view_get_model (priv->tree_view);
              gtk_tree_model_get_iter (model, iter, path);
              gtk_tree_model_get_iter (model, &parent_iter, parent_path);
              gtk_tree_path_free (path);
              gtk_tree_path_free (parent_path);
            }

          hyscan_gtk_mark_manager_view_toggle_parent (self, &parent_iter);
        }
    }
}

/* Функция-обработчик для вывода подсказки. */
gboolean
hyscan_gtk_mark_manager_view_show_tooltip (GtkWidget  *widget,
                                           gint        x,
                                           gint        y,
                                           gboolean    keyboard_mode,
                                           GtkTooltip *tooltip,
                                           gpointer    user_data)
{
  GtkTreeView       *view   = GTK_TREE_VIEW (widget);
  GtkTreePath       *path   = NULL;
  GtkTreeViewColumn *column = NULL;
  /* Получаем модель. */
  GtkTreeModel      *model = gtk_tree_view_get_model (view);
  GtkTreeIter iter;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

  if (keyboard_mode)
    {
      gtk_tree_view_get_cursor (view, &path, &column);
      if (path == NULL)
        return FALSE;
    }
  else
    {
      gint bin_x, bin_y;
      gboolean is_path;
      /* Координаты курсора в пространстве виджета. */
      gtk_tree_view_convert_widget_to_bin_window_coords (view, x, y, &bin_x, &bin_y);
      /* Определяем положение курсора.*/
      is_path = gtk_tree_view_get_path_at_pos (view, bin_x, bin_y, &path, &column, NULL, NULL);
      /* Курсор на GtkTreeView. */
      if (!is_path)
        return FALSE;
    }
  /* Получаем итератор. */
  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      /* Строка с подсказкой. */
      gchar *str = "";
      /* Определяем положение ветви, чтобы рядом показать подсказку. */
      gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);
      /* Получаем текст подсказки из модели. */
      gtk_tree_model_get (model, &iter, COLUMN_TOOLTIP, &str, -1);
      /* Выводим подсказку. */
      gtk_tooltip_set_text (tooltip, str);
      g_free (str);
    }
  /* Освобождаем путь. */
  gtk_tree_path_free (path);

  return TRUE;
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления. */
void
hyscan_gtk_mark_manager_view_item_expanded (GtkTreeView *tree_view,
                                            GtkTreeIter *iter,
                                            GtkTreePath *path,
                                            gpointer     user_data)
{
  HyScanGtkMarkManagerView *self = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  gchar *id = NULL;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (self));

  gtk_tree_model_get (model,      iter,
                      COLUMN_ID, &id,
                      -1);

  if (id != NULL)
    {
      /* Отправляем сигнал о разворачивании узла древовидного представления. */
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED], 0, id, TRUE);
      g_free (id);
    }
}

/* Функция-обработчик сигнала о сворачивании узла древовидного представления. */
void
hyscan_gtk_mark_manager_view_item_collapsed (GtkTreeView *tree_view,
                                             GtkTreeIter *iter,
                                             GtkTreePath *path,
                                             gpointer     user_data)
{
  HyScanGtkMarkManagerView *self = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  gchar *id = NULL;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (self));

  gtk_tree_model_get (model,      iter,
                      COLUMN_ID, &id,
                      -1);

  if (id != NULL)
    {
      /* Отправляем сигнал о разворачивании узла древовидного представления. */
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED], 0, id, FALSE);
      g_free (id);
    }
}

/* Функция устанавливает модель для табличного представления. */
void
hyscan_gtk_mark_manager_view_set_list_model (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeViewColumn *column          = gtk_tree_view_column_new ();                 /* Колонка для картинки, чек-бокса и текста. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new (),             /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new ();             /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */

  /* Удаляем все колонки. */
  g_list_foreach (list, hyscan_gtk_mark_manager_view_remove_column, priv->tree_view);
  /* Освобождаем список колонок. */
  g_list_free (list);

  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_on_toggle),
                    self);

  /* Заголовок колонки. */
  gtk_tree_view_column_set_title (column, _("Name"));
  /* Подключаем функцию для отрисовки чек-бокса.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           toggle_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_toggle,
                                           NULL,
                                           NULL);
  /* Добавляем чек-бокс. */
  gtk_tree_view_column_pack_start (column, toggle_renderer, FALSE);
  /* Подключаем функцию для отрисовки картинки.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_icon,
                                           NULL,
                                           NULL);
  /* Добавляем картинку.*/
  gtk_tree_view_column_pack_start (column, icon_renderer, FALSE);
  /* Подключаем функцию для отрисовки текста.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           hyscan_gtk_mark_manager_view_set_func_text,
                                           NULL,
                                           NULL);
  /* Добавляем текст. */
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  /* Добавляем колонку в список. */
  gtk_tree_view_append_column (priv->tree_view, column);

  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_DESCRIPTION,
                                               _("Description"), renderer,
                                               "text", COLUMN_DESCRIPTION,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_OPERATOR,
                                               _("Operator name"), renderer,
                                               "text", COLUMN_OPERATOR,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_CTIME,
                                               _("Created"), renderer,
                                               "text", COLUMN_CTIME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_MTIME,
                                               _("Modified"), renderer,
                                               "text", COLUMN_MTIME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_LOCATION,
                                               _("Location (WGS 84)"), renderer,
                                               "text", COLUMN_LOCATION,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_TRACK_NAME,
                                               _("Track name"), renderer,
                                               "text", COLUMN_TRACK_NAME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_BOARD,
                                               _("Board"), renderer,
                                               "text", COLUMN_BOARD,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_DEPTH,
                                               _("Depth"), renderer,
                                               "text", COLUMN_DEPTH,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_WIDTH,
                                               _("Width"), renderer,
                                               "text", COLUMN_WIDTH,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_SLANT_RANGE,
                                               _("Slant range"), renderer,
                                               "text", COLUMN_SLANT_RANGE,
                                               NULL);
  /* Показать заголовки. */
  gtk_tree_view_set_headers_visible (priv->tree_view, TRUE);
  /* Скрыть ветви линиями. */
  gtk_tree_view_set_enable_tree_lines (priv->tree_view, FALSE);
}

/* Функция устанавливает модель для древовидного представления. */
void
hyscan_gtk_mark_manager_view_set_tree_model (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeViewColumn *column          = gtk_tree_view_column_new ();                 /* Колонка для ViewItem-а. */
  GtkCellRenderer   *renderer        = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer   = gtk_cell_renderer_pixbuf_new (),             /* Картинка. */
                    *toggle_renderer = gtk_cell_renderer_toggle_new ();             /* Чек-бокс. */
  GList             *list            = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */

  /* Удаляем все колонки. */
  g_list_foreach (list, hyscan_gtk_mark_manager_view_remove_column, priv->tree_view);
  /* Освобождаем список колонок. */
  g_list_free (list);
  /* Полключаем сигнал для обработки клика по чек-боксам. */
  g_signal_connect (toggle_renderer,
                    "toggled",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_on_toggle),
                    self);

  /* Заголовок колонки. */
  gtk_tree_view_column_set_title (column, _("Objects"));
  /* Подключаем функцию для отрисовки чек-бокса.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           toggle_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_toggle,
                                           NULL,
                                           NULL);
  /* Добавляем чек-бокс. */
  gtk_tree_view_column_pack_start (column, toggle_renderer, FALSE);
  /* Подключаем функцию для отрисовки картинки.*/
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_icon,
                                           NULL,
                                           NULL);
  /* Добавляем картинку.*/
  gtk_tree_view_column_pack_start (column, icon_renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           hyscan_gtk_mark_manager_view_set_func_text,
                                           NULL,
                                           NULL);
  /* Добавляем текст. */
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  /* Добавляем колонку в список. */
  gtk_tree_view_append_column (priv->tree_view, column);
  /* Связываем с колонкой, определяющей видимость. */
  gtk_tree_view_column_add_attribute (column, toggle_renderer, "visible", COLUMN_VISIBLE);
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
hyscan_gtk_mark_manager_view_remove_column (gpointer data,
                                        gpointer user_data)
{
  GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (data);
  GtkTreeView *tree_view    = GTK_TREE_VIEW (user_data);
  gtk_tree_view_remove_column (tree_view, column);
}

/* Отображение картинки. */
void
hyscan_gtk_mark_manager_view_set_func_icon (GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer   *cell,
                                            GtkTreeModel      *model,
                                            GtkTreeIter       *iter,
                                            gpointer           data)
{
  GdkPixbuf *pixbuf = NULL;
  gtk_tree_model_get (model, iter, COLUMN_ICON,  &pixbuf, -1);
  g_object_set (GTK_CELL_RENDERER (cell), "pixbuf", pixbuf, NULL);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);
}

/* Отображение чек-бокса. */
void
hyscan_gtk_mark_manager_view_set_func_toggle (GtkTreeViewColumn *tree_column,
                                              GtkCellRenderer   *cell,
                                              GtkTreeModel      *model,
                                              GtkTreeIter       *iter,
                                              gpointer           data)
{
  gboolean active;
  gtk_tree_model_get (model, iter, COLUMN_ACTIVE, &active, -1);
  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell), active);
}
/* Отображение названия. */
void
hyscan_gtk_mark_manager_view_set_func_text (GtkTreeViewColumn *tree_column,
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

/* Обработчик установки фокуса на виджет.
 * Сохраняет список выделенных объектов.
 * */
void
hyscan_gtk_mark_manager_view_grab_focus (GtkWidget *widget,
                                         gpointer   user_data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeSelection *selection;
  GtkTreeModel *model;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  priv = self->priv;
  selection = gtk_tree_view_get_selection (priv->tree_view);
  /* Если фокус получаем в первый раз, то нужно снять выделение по умолчанию. */
  if (priv->focus_start)
    {
      gtk_tree_selection_unselect_all (selection);
      return;
    }
  if (gtk_tree_selection_get_selected (selection, &model, &priv->selected_iter))
    priv->has_selected = TRUE;
}

/* Обработчик, вызываюшися после #hyscan_gtk_mark_manager_view_grab_focus ().
 * Выделяет объекты в соответствии с ранее сохранённым списком.
 * */
void
hyscan_gtk_mark_manager_view_grab_focus_after (GtkWidget *widget,
                                               gpointer   user_data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeSelection *selection;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  priv = self->priv;
  selection = gtk_tree_view_get_selection (priv->tree_view);

  /* Если фокус получаем в первый раз, то выходим. */
  if (priv->focus_start)
    {
      priv->focus_start = FALSE;
      return;
    }

  if (priv->has_selected)
    {
      gtk_tree_selection_select_iter (selection, &priv->selected_iter);
      priv->has_selected = FALSE;
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
    }
}

/* Обработчик выделения объекта. */
gboolean
hyscan_gtk_mark_manager_view_select_func (GtkTreeSelection *selection,
                                          GtkTreeModel     *model,
                                          GtkTreePath      *path,
                                          gboolean          path_currently_selected,
                                          gpointer          data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (data), FALSE);

  self = HYSCAN_GTK_MARK_MANAGER_VIEW (data);
  priv = self->priv;

  priv->is_selected = path_currently_selected;

  if (priv->toggle_flag)
    {
      priv->toggle_flag = FALSE;
      return FALSE;
    }
  else
    {
      return !priv->focus_start;
    }
}

/* Установка фокуса для снятия выделения по умолчанию. */
void
hyscan_gtk_mark_manager_view_on_show (GtkWidget *widget,
                                      gpointer   user_data)
{
  /* Возможно, чтобы не обрабатывать сигналы о фокусе,
   * нужно проверить получение фокуса первый раз и здесь. */
  gtk_widget_grab_focus (widget);
}

/**
 * hyscan_gtk_mark_manager_view_expand_all:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 *
 * Рекурсивно разворачивает все узлы древовидного списка.
 */
void
hyscan_gtk_mark_manager_view_expand_all (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (GTK_IS_TREE_STORE (priv->store))
    {
      gtk_tree_view_expand_all (priv->tree_view);
    }
}

/**
 * hyscan_gtk_mark_manager_view_collapse_all:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 *
 * Рекурсивно cворачивает все узлы древовидного списка.
 */
void
hyscan_gtk_mark_manager_view_collapse_all (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (GTK_IS_TREE_STORE (priv->store))
    {
      gtk_tree_view_collapse_all (priv->tree_view);
    }
}

/**
 * hyscan_gtk_mark_manager_view_unselect_all:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 *
 * Cнимает выделение со всех объектов.
 */
void
hyscan_gtk_mark_manager_view_unselect_all (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);

  gtk_tree_selection_unselect_all (selection);
}

/**
 * hyscan_gtk_mark_manager_view_toggle_all:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @active: состояние чек-бокса
 *
 * Устанавливает состояние чек-бокса для всех объектов.
 */
void
hyscan_gtk_mark_manager_view_toggle_all (HyScanGtkMarkManagerView *self,
                                         gboolean                  active)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {
          GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
          hyscan_gtk_mark_manager_view_toggle (self, &iter, active);
          /* Обновляем указаетль на модель и итератор, т.к. модель могла обновиться. */
          model = gtk_tree_view_get_model (priv->tree_view);
          gtk_tree_model_get_iter (model, &iter, path);
          gtk_tree_path_free (path);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }
}

/**
 * hyscan_gtk_mark_manager_view_expand_path:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @path: указатель на путь к объекту в модели.
 * @expand: TRUE  - развёрнуть узел,
 *          FALSE - свёрнуть узел
 *
 * Разворачивает или сворачивает узел по заданному пути.
 */
void
hyscan_gtk_mark_manager_view_expand_path (HyScanGtkMarkManagerView *self,
                                          GtkTreePath              *path,
                                          gboolean                  expanded)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (GTK_IS_TREE_STORE (priv->store))
    {
      if (expanded)
        {
          if (priv->signal_expanded != 0)
            {
              /* Отключаем сигнал разворачивания узла древовидного представления. */
              g_signal_handler_block (G_OBJECT (priv->tree_view), priv->signal_expanded);
              /* Разворачиваем, если узел свёрнут. */
              if (!gtk_tree_view_row_expanded (priv->tree_view, path))
                {
                  gtk_tree_view_expand_to_path (priv->tree_view, path);
                }
              /* Включаем сигнал разворачивания узла древовидного представления. */
              g_signal_handler_unblock (G_OBJECT (priv->tree_view), priv->signal_expanded);
            }
        }
      else
        {
          if (priv->signal_collapsed != 0)
            {
              /* Отключаем сигнал сворачивания узла древовидного представления. */
              g_signal_handler_block (G_OBJECT (priv->tree_view), priv->signal_collapsed);
              /* Сворачиваем, если узел развёрнут. */
              if (gtk_tree_view_row_expanded (priv->tree_view, path))
                {
                  gtk_tree_view_collapse_row (priv->tree_view, path);
                }
              /* Включаем сигнал сворачивания узла древовидного представления. */
              g_signal_handler_unblock (G_OBJECT (priv->tree_view), priv->signal_collapsed);
            }
        }
    }
}

/**
 * hyscan_gtk_mark_manager_view_get_toggled:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @type: тип объекта
 *
 * Returns: возвращает список идентификаторов объектов
 * с активированным чек-боксом. Тип объекта определяется
 * #ModelManagerObjectType. Когда список больше не нужен,
 * необходимо использовать #g_strfreev ().
 */
gchar**
hyscan_gtk_mark_manager_view_get_toggled (HyScanGtkMarkManagerView *self,
                                          ModelManagerObjectType    type)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
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
              if (id != NULL)
                g_free (id);
            }
          while (gtk_tree_model_iter_next (priv->store, &iter));
        }
      gtk_tree_path_free (path);
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
              if (id != NULL)
                g_free (id);
            }
          while (gtk_tree_model_iter_next (priv->store, &iter));
        }
      gtk_tree_path_free (path);
    }

  return list;
}

/**
 * hyscan_gtk_mark_manager_view_new:
 * @model: модель представления данных
 *
 * Returns: cоздаёт новый экземпляр #HyScanGtkMarkManageView
 */
GtkWidget*
hyscan_gtk_mark_manager_view_new (GtkTreeModel *store)
{
  return GTK_WIDGET (g_object_new (HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                                   "store", store,
                                   NULL));
}

/**
 * hyscan_gtk_mark_manager_view_set_store:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @model: указатель на модель с данными
 *
 * Устанавливает модель представления данных. */
void
hyscan_gtk_mark_manager_view_set_store (HyScanGtkMarkManagerView *self,
                                        GtkTreeModel             *store)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (priv->store != store)
    {
      priv->store = g_object_ref (store);

      if (GTK_IS_TREE_STORE (priv->store))
        hyscan_gtk_mark_manager_view_set_tree_model (self);
      else if (GTK_IS_LIST_STORE (priv->store))
        hyscan_gtk_mark_manager_view_set_list_model (self);

      hyscan_gtk_mark_manager_view_update (self);
    }
}

/**
 * hyscan_gtk_mark_manager_view_select_item:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @id: идентификатор выделенного объекта.
 *
 * Выделяет объект.
 */
void
hyscan_gtk_mark_manager_view_select_item (HyScanGtkMarkManagerView *self,
                                          gchar                    *id)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (priv->tree_view);
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);

  /* Отключаем сигнал об изменении выбранных объектов. */
  if (priv->signal_selected != 0 && selection != NULL)
    {
      GtkTreeIter iter;

      if (gtk_tree_model_get_iter_first (model, &iter))
        {
          GtkTreePath *path;

          do
            {
              if (hyscan_gtk_mark_manager_view_find_item_by_id (model, &iter, id))
                {
                   break;
                }
            }
          while (gtk_tree_model_iter_next (model, &iter));

          if (GTK_IS_TREE_STORE (model))
            g_return_if_fail (gtk_tree_store_iter_is_valid (GTK_TREE_STORE (model), &iter));
          else
            g_return_if_fail (gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &iter));

          /* Если нужно, разворачиваем узел. */
          path = gtk_tree_model_get_path (model, &iter);
          gtk_tree_path_up (path);
          gtk_tree_view_expand_to_path (priv->tree_view, path);
          gtk_tree_path_free (path);
          /* Отключаем сигнал о выделении. */
          g_signal_handler_block (G_OBJECT (selection), priv->signal_selected);
          /* Снимаем выделение со всего списка. */
          gtk_tree_selection_unselect_all (selection);
          /* Выделяем выбранное. */
          gtk_tree_selection_select_iter (selection, &iter);
          /* Включаем сигнал об изменении выбранных объектов. */
          g_signal_handler_unblock (G_OBJECT (selection), priv->signal_selected);
        }
    }
}

/**
 * hyscan_gtk_mark_manager_view_find_item_by_id:
 * @model: указатель на модель с данными
 * @iter: указатель на итератор
 * @id: идентификатор
 *
 * Функция для рекурсивного обхода модели и поиска записи по заданному идентификтору.
 * Returns: TRUE - если объект найден, то копирует итератор в iter;
 *          FALSE - объект не найден.
 */
gboolean
hyscan_gtk_mark_manager_view_find_item_by_id (GtkTreeModel *model,
                                              GtkTreeIter  *iter,
                                              const gchar  *id)
{
  GtkTreeIter child_iter;
  gchar *str;

  gtk_tree_model_get (model,     iter,
                      COLUMN_ID, &str,
                      -1);
  if (str == NULL)
    return FALSE;
  if (0 == g_strcmp0 (id, str))
    {
      g_free (str);
      return TRUE;
    }
  g_free (str);

  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      do
        {
          if (hyscan_gtk_mark_manager_view_find_item_by_id (model, &child_iter, id))
            {
              *iter = child_iter;
              return TRUE;
            }
        }
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
  return FALSE;
}

/**
 * hyscan_gtk_mark_manager_view_toggle_item:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @id: идентификатор объекта
 * @active: состояние чек-бокса
 *
 * Устанавливает состояние чек-бокса объектам по заданному идентификатору.
 */
void
hyscan_gtk_mark_manager_view_toggle_item  (HyScanGtkMarkManagerView  *self,
                                           gchar                     *id,
                                           gboolean                   active)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter iter;

  g_print ("Ok\n");
  g_print ("ID: %s\n", id);
  g_print ("Active: %s\n", active ? "TRUE" : "FALSE");

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      do
        {

          gchar *str;
          gtk_tree_model_get (model,     &iter,
                              COLUMN_ID, &str,
                              -1);
          g_print ("ID: %s\n", str);
/*
          if (0 == g_strcmp0 (id, str))
            {
              g_print (">>MATCH\n");
              if  (priv->signal_toggled != 0)
                {
                  g_signal_handler_block (priv->toggle_renderer, priv->signal_toggled);

                  if (GTK_IS_TREE_STORE (model))
                    gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_ACTIVE, active, -1);
                  else if (GTK_IS_LIST_STORE (model))
                    gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_ACTIVE, active, -1);

                  g_signal_handler_unblock (priv->toggle_renderer, priv->signal_toggled);
                }
            }
            */
/*          if (hyscan_gtk_mark_manager_view_find_item_by_id (model, &iter, id))
           {
             g_print ("MATCH\n");
           }*/
       }
     while (gtk_tree_model_iter_next (model, &iter));
  }
}

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
 * - hyscan_gtk_mark_manager_view_find_item_by_id () - поиск объекта по идентификтору;
 * - hyscan_gtk_mark_manager_view_find_items_by_id () - множественный поиск объекта по идентификатору;
 * - hyscan_gtk_mark_manager_view_toggle_item () - устанавливает состояние чек-бокса объектам по заданному
 * идентификатору;
 * - hyscan_gtk_mark_manager_view_block_signal_selected () - блокирует или разблокирует сигнал выбора строки;
 * - hyscan_gtk_mark_manager_view_show_object () - отправляет сигнал о двойном клике по объекту.
 */
#include <hyscan-gtk-mark-manager-view.h>
#include <hyscan-mark.h>
#include <hyscan-mark-location.h>
#include <hyscan-db-info.h>
#include <hyscan-gui-marshallers.h>
#include <glib/gi18n-lib.h>

enum
{
  PROP_STORE = 1,      /* Модель представления данных. */
  N_PROPERTIES
};

/* Сигналы. */
enum
{
  SIGNAL_SELECTED,     /* Выделение строки. */
  SIGNAL_UNSELECT,     /* Снять выделение. */
  SIGNAL_TOGGLED,      /* Изменение состояня чек-бокса. */
  SIGNAL_EXPANDED,     /* Разворачивание узла древовидного представления. */
  SIGNAL_DOUBLE_CLICK, /* Двойной клик по объекту. */
  SIGNAL_LAST
};

struct _HyScanGtkMarkManagerViewPrivate
{
  GtkTreeView      *tree_view;        /* Представление. */
  GtkTreeModel     *store;            /* Модель представления данных (табличное или древовидное). */
  GtkTreeIter       selected_iter;    /* Используется для временного хранения
                                       * чтобы правильно выделять объекты при установке фокуса. */
  GtkCellRenderer  *toggle_renderer;  /* Чек-бокс. */
  GList            *selected;         /* Список выделенных объектов. */
  gulong            signal_selected,  /* Идентификатор сигнала об изменении выбранных объектов. */
                    signal_expanded,  /* Идентификатор сигнала разворачивания узла древовидного представления.*/
                    signal_collapsed, /* Идентификатор сигнала cворачивания узла древовидного представления.*/
                    signal_toggled;   /* Идентификатор сигнала изменения состояния чек-бокса. */
  gboolean          has_selected,     /* Флаг наличия выделенного объекта. */
                    toggle_flag,      /* Флаг для отмены выделения при клике по чек-боксу. */
                    focus_start;      /* Флаг получения первого фокуса. */
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

static gboolean   hyscan_gtk_mark_manager_view_on_mouse_move      (GtkWidget                *widget,
                                                                   GdkEventMotion           *event,
                                                                   gpointer                  user_data);

static void       hyscan_gtk_mark_manager_view_on_double_click    (GtkTreeView              *tree_view,
                                                                   GtkTreePath              *path,
                                                                   GtkTreeViewColumn        *column,
                                                                   gpointer                  user_data);

static guint      hyscan_gtk_mark_manager_view_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMarkManagerView, hyscan_gtk_mark_manager_view, GTK_TYPE_SCROLLED_WINDOW)

static void
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
   * @id: идентификатор объекта в базе данных
   * @active: статус чек-бокса (%TRUE - отмечен, %FALSE - не отмечен)
   *
   * Сигнал посылается при изменении состояния чек-бокса.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED] =
    g_signal_new ("toggled", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /** HyScanGtkMarkManagerView::expanded:
   * @self: указатель на #HyScanGtkMarkManagerView
   * @id: идентификатор объекта в базе данных
   * @expanded: состояние узла (%TRUE - развёрнут, %FALSE - свёрнут)
   *
   * Сигнал посылается при разворачивании узла древовидного представления.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED] =
    g_signal_new ("expanded", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /**
   * HyScanGtkMarkManagerView::double-click:
   * @self: указатель на #HyScanGtkMarkManagerView
   *
   * Сигнал посылается при двойном клике по объекту.
   */
  hyscan_gtk_mark_manager_view_signals[SIGNAL_DOUBLE_CLICK] =
    g_signal_new ("double-click", HYSCAN_TYPE_GTK_MARK_MANAGER_VIEW,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  hyscan_gui_marshal_VOID__STRING_UINT,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_UINT);
}

static void
hyscan_gtk_mark_manager_view_init (HyScanGtkMarkManagerView *self)
{
  self->priv = hyscan_gtk_mark_manager_view_get_instance_private (self);
  /* Костыль для снятия выделния по умолчанию. */
  self->priv->focus_start = TRUE;
}

static void
hyscan_gtk_mark_manager_view_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  HyScanGtkMarkManagerView *self        = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_STORE:
      priv->store = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_mark_manager_view_constructed (GObject *object)
{
  HyScanGtkMarkManagerView        *self   = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv   = self->priv;
  GtkScrolledWindow               *widget = GTK_SCROLLED_WINDOW (object);

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_view_parent_class)->constructed (object);

  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

  hyscan_gtk_mark_manager_view_update (self);

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->tree_view));

  gtk_widget_set_has_tooltip (GTK_WIDGET (priv->tree_view), TRUE);

  g_signal_connect (G_OBJECT (priv->tree_view), "query-tooltip",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_show_tooltip), NULL);

  priv->signal_expanded  = g_signal_connect (priv->tree_view,
                                             "row-expanded",
                                             G_CALLBACK (hyscan_gtk_mark_manager_view_item_expanded),
                                             self);

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

  /* Сигнал о перемещении мыши. */
  /*g_signal_connect (priv->tree_view, "motion-notify-event",
                      G_CALLBACK (hyscan_gtk_mark_manager_view_on_mouse_move), NULL);*/
  /* Сигнал двойного клика мыши по объекту. */
  g_signal_connect (priv->tree_view, "row-activated",
                    G_CALLBACK (hyscan_gtk_mark_manager_view_on_double_click), self);
}

static void
hyscan_gtk_mark_manager_view_finalize (GObject *object)
{
  HyScanGtkMarkManagerView *self = HYSCAN_GTK_MARK_MANAGER_VIEW (object);
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  g_object_unref (priv->store);
  priv->store = NULL;

  g_list_free_full (priv->selected, (GDestroyNotify) gtk_tree_path_free);
  priv->selected = NULL;

  G_OBJECT_CLASS (hyscan_gtk_mark_manager_view_parent_class)->finalize (object);
}

/* Функция обновляет представление. */
static void
hyscan_gtk_mark_manager_view_update (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  void (*set_model) (HyScanGtkMarkManagerView *self);

  if (GTK_IS_LIST_STORE (priv->store))
    set_model = hyscan_gtk_mark_manager_view_set_list_model;
  else if (GTK_IS_TREE_STORE (priv->store))
    set_model = hyscan_gtk_mark_manager_view_set_tree_model;
  else
    return;

  if (priv->tree_view == NULL)
    {
      GtkTreeSelection  *selection;

      priv->tree_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->store)));

      set_model (self);

      selection = gtk_tree_view_get_selection (priv->tree_view);

      gtk_tree_selection_set_select_function (selection,
                                              hyscan_gtk_mark_manager_view_select_func,
                                              self,
                                              NULL);
      priv->signal_selected = g_signal_connect (selection, "changed",
                                                G_CALLBACK (hyscan_gtk_mark_manager_view_emit_selected), self);
    }
  else
    {
      gtk_tree_view_set_model (priv->tree_view, GTK_TREE_MODEL (priv->store));
    }
  if (GTK_IS_LIST_STORE (priv->store))
    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (priv->tree_view), GTK_SELECTION_SINGLE);
  else /* Здесь без "else if", т.к. в начале уже была проверка с "return"-ом. */
    gtk_tree_selection_set_mode (gtk_tree_view_get_selection (priv->tree_view), GTK_SELECTION_MULTIPLE);
}

/* Обработчик выделения строки в списке. */
static void
hyscan_gtk_mark_manager_view_emit_selected (GtkTreeSelection         *selection,
                                            HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeModel *model = NULL;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  priv = self->priv;

  if (priv->has_selected)
    {
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_UNSELECT], 0);
      return;
    }

  model = gtk_tree_view_get_model (priv->tree_view);

  if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE)
    {
      GList *list = gtk_tree_selection_get_selected_rows (selection, &model);

      for (GList *ptr = g_list_first (list); ptr != NULL; ptr = g_list_next (ptr))
        {
          GtkTreeIter iter;

          if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*)ptr->data))
            {
              gchar *id = NULL;

              gtk_tree_model_get (model,     &iter,
                                  COLUMN_ID, &id,
                                  -1);

              if (id != NULL)
                {
                  if (IS_NOT_EMPTY (id))
                    g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_SELECTED], 0, id);
                  g_free (id);
                }
            }
        }
      g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
    }
   else if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE)
    {
      gchar *id = NULL;

      if (!gtk_tree_selection_get_selected (selection, &model, &iter))
        return;

      gtk_tree_model_get (model,     &iter,
                          COLUMN_ID, &id,
                          -1);
      if (id == NULL)
        return;

      if (IS_EMPTY (id))
        return;

      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_SELECTED], 0, id);
      g_free (id);
    }
}

/* Обработчик клика по чек-боксу. */
static void
hyscan_gtk_mark_manager_view_on_toggle (GtkCellRendererToggle *cell_renderer,
                                        gchar                 *path,
                                        gpointer               user_data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gboolean      active;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (user_data));

  self  = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  priv  = self->priv;
  model = gtk_tree_view_get_model (priv->tree_view);

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_tree_model_get (model,         &iter,
                      COLUMN_ACTIVE, &active,
                      -1);

  hyscan_gtk_mark_manager_view_toggle (self, &iter, !active);

  if (GTK_IS_TREE_STORE (model))
    hyscan_gtk_mark_manager_view_toggle_parent (self, &iter);

  priv->toggle_flag = TRUE;
}

/* Устанавливает состояние чек-бокса, в том числе и дочерним объектам. */
static void
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
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, active);
      g_free (id);
    }

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
static void
hyscan_gtk_mark_manager_view_toggle_parent (HyScanGtkMarkManagerView *self,
                                            GtkTreeIter              *iter)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter parent_iter, child_iter;
  gint total, counter = 0;
  gchar *id;
  gboolean flag;

  if (!gtk_tree_model_iter_parent (model, &parent_iter, iter))
    return;

  if (!gtk_tree_model_iter_children (model, &child_iter, &parent_iter))
    return;

  total = gtk_tree_model_iter_n_children  (model, &parent_iter);

  do
    {
      gtk_tree_model_get (model,         &child_iter,
                          COLUMN_ACTIVE, &flag,
                          -1);
      if (flag)
        counter++;
    }
  while (gtk_tree_model_iter_next (model, &child_iter));

  /* TRUE  - все дочерние узлы отмечены,
   * FALSE - есть неотмеченные дочерние узлы. */
  flag = (counter == total) ? TRUE : FALSE;

  gtk_tree_model_get (model,     &parent_iter,
                      COLUMN_ID, &id,
                      -1);
  gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter, COLUMN_ACTIVE, flag, -1);

  if (id != NULL)
    {
      g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, flag);
      g_free (id);
    }

  hyscan_gtk_mark_manager_view_toggle_parent (self, &parent_iter);
}

/* Функция-обработчик для вывода подсказки. */
static gboolean
hyscan_gtk_mark_manager_view_show_tooltip (GtkWidget  *widget,
                                           gint        x,
                                           gint        y,
                                           gboolean    keyboard_mode,
                                           GtkTooltip *tooltip,
                                           gpointer    user_data)
{
  GtkTreeIter        iter;
  GtkTreeView       *view   = GTK_TREE_VIEW (widget);
  GtkTreePath       *path   = NULL;
  GtkTreeViewColumn *column = NULL;
  GtkTreeModel      *model  = gtk_tree_view_get_model (view);

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
      gtk_tree_view_convert_widget_to_bin_window_coords (view, x, y, &bin_x, &bin_y);

      if (!gtk_tree_view_get_path_at_pos (view, bin_x, bin_y, &path, &column, NULL, NULL))
        return FALSE;

      x = bin_x;
      y = bin_y;
    }

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      GdkPixbuf *pixbuf = NULL;
      GdkRectangle  rect;
      GStrv tooltips = NULL;
      HyScanGtkMarkManagerIcon *icon;
      gchar *ptr, *str;

      gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);
      gtk_tree_model_get (model, &iter, COLUMN_ICON, &icon, -1);

      tooltips = hyscan_gtk_mark_manager_icon_get_tooltips (icon);

      if (tooltips == NULL)
        {
          hyscan_gtk_mark_manager_icon_free (icon);
          gtk_tree_path_free (path);

          return FALSE;
        }

      ptr = tooltips[0];
      str = HAS_MANY_ICONS (icon) ? _("Labels") : tooltips[0];

      gtk_tree_view_get_cell_area (view, path, column, &rect);
      pixbuf = hyscan_gtk_mark_manager_icon_get_icon (icon);

      if (HAS_MANY_ICONS (icon) && pixbuf != NULL)
        {
          gint width  = gdk_pixbuf_get_width (pixbuf),
               cell_x = x - rect.x;

          if (cell_x >= 0 && cell_x < width)
            {
              gint index = (cell_x / gdk_pixbuf_get_height (pixbuf));

              if (tooltips[index] != NULL)
                ptr = tooltips[index];
            }
          else
            {
              ptr = str;
            }
         }

       gtk_tooltip_set_text (tooltip, ptr);

       g_strfreev (tooltips);
       gtk_tree_path_free (path);
       g_clear_object (&pixbuf);
       hyscan_gtk_mark_manager_icon_free (icon);

       return TRUE;
    }

  gtk_tree_path_free (path);

  return FALSE;
}

/* Функция-обработчик сигнала о разворачивании узла древовидного представления. */
static void
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

  if (id == NULL)
    return;

  g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED], 0, id, TRUE);
  g_free (id);
}

/* Функция-обработчик сигнала о сворачивании узла древовидного представления. */
static void
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

  if (id == NULL)
    return;

  g_signal_emit (self, hyscan_gtk_mark_manager_view_signals[SIGNAL_EXPANDED], 0, id, FALSE);
  g_free (id);
}

/* Функция устанавливает модель для табличного представления. */
static void
hyscan_gtk_mark_manager_view_set_list_model (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeViewColumn *column        = gtk_tree_view_column_new ();                 /* Колонка для картинки, чек-бокса и текста. */
  GtkCellRenderer   *renderer      = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer = gtk_cell_renderer_pixbuf_new ();             /* Картинка. */
  GList             *list          = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */

  g_list_foreach (list, hyscan_gtk_mark_manager_view_remove_column, priv->tree_view);
  g_list_free (list);

  priv->toggle_renderer = gtk_cell_renderer_toggle_new ();
  priv->signal_toggled = g_signal_connect (priv->toggle_renderer,
                                           "toggled",
                                           G_CALLBACK (hyscan_gtk_mark_manager_view_on_toggle),
                                           self);

  gtk_tree_view_column_set_title (column, _("Name"));
  gtk_tree_view_column_set_cell_data_func (column,
                                           priv->toggle_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_toggle,
                                           NULL,
                                           NULL);
  gtk_tree_view_column_pack_start (column, priv->toggle_renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_icon,
                                           NULL,
                                           NULL);
  gtk_tree_view_column_pack_start (column, icon_renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           hyscan_gtk_mark_manager_view_set_func_text,
                                           NULL,
                                           NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
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

  gtk_tree_view_set_headers_visible (priv->tree_view, TRUE);
  gtk_tree_view_set_enable_tree_lines (priv->tree_view, FALSE);
}

/* Функция устанавливает модель для древовидного представления. */
void
hyscan_gtk_mark_manager_view_set_tree_model (HyScanGtkMarkManagerView *self)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkCellArea       *area          = gtk_cell_area_box_new ();                    /* Контейнер. */
  GtkCellRenderer   *renderer      = gtk_cell_renderer_text_new (),               /* Текст. */
                    *icon_renderer = gtk_cell_renderer_pixbuf_new ();             /* Картинка. */
  GList             *list          = gtk_tree_view_get_columns (priv->tree_view); /* Список колонок. */
  GtkTreeViewColumn *column; /* Колонка для картинки, чек-бокса и текста. */

  g_list_foreach (list, hyscan_gtk_mark_manager_view_remove_column, priv->tree_view);
  g_list_free (list);

  priv->toggle_renderer = gtk_cell_renderer_toggle_new ();
  priv->signal_toggled = g_signal_connect (priv->toggle_renderer,
                                           "toggled",
                                           G_CALLBACK (hyscan_gtk_mark_manager_view_on_toggle),
                                           self);

  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), priv->toggle_renderer, FALSE, FALSE, FALSE);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), icon_renderer, FALSE, FALSE, FALSE);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE, FALSE);

  column = gtk_tree_view_column_new_with_area (area);
  gtk_tree_view_column_set_title (column, _("Objects"));
  gtk_tree_view_column_set_cell_data_func (column,
                                           priv->toggle_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_toggle,
                                           NULL,
                                           NULL);
  gtk_tree_view_column_set_cell_data_func (column,
                                           icon_renderer,
                                           hyscan_gtk_mark_manager_view_set_func_icon,
                                           NULL,
                                           NULL);
  gtk_tree_view_column_set_cell_data_func (column,
                                           renderer,
                                           hyscan_gtk_mark_manager_view_set_func_text,
                                           NULL,
                                           NULL);
  gtk_tree_view_append_column (priv->tree_view, column);

  gtk_tree_view_column_add_attribute (column, priv->toggle_renderer, "visible", COLUMN_VISIBLE);
  gtk_tree_view_set_headers_visible (priv->tree_view, FALSE);
  gtk_tree_view_set_enable_tree_lines (priv->tree_view, TRUE);
}

/* Функция удаляет колонку из представления. */
static void
hyscan_gtk_mark_manager_view_remove_column (gpointer data,
                                            gpointer user_data)
{
  GtkTreeViewColumn *column = GTK_TREE_VIEW_COLUMN (data);
  GtkTreeView *tree_view    = GTK_TREE_VIEW (user_data);

  gtk_tree_view_remove_column (tree_view, column);
}

/* Отображение картинки. */
static void
hyscan_gtk_mark_manager_view_set_func_icon (GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer   *cell,
                                            GtkTreeModel      *model,
                                            GtkTreeIter       *iter,
                                            gpointer           data)
{
  HyScanGtkMarkManagerIcon *icon;
  GdkPixbuf *pixbuf = NULL;

  gtk_tree_model_get (model, iter, COLUMN_ICON,  &icon, -1);
  pixbuf = hyscan_gtk_mark_manager_icon_get_icon (icon);
  hyscan_gtk_mark_manager_icon_free (icon);

  g_object_set (GTK_CELL_RENDERER (cell), "pixbuf", pixbuf, NULL);

  g_clear_object (&pixbuf);
}

/* Отображение чек-бокса. */
static void
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
static void
hyscan_gtk_mark_manager_view_set_func_text (GtkTreeViewColumn *tree_column,
                                            GtkCellRenderer   *cell,
                                            GtkTreeModel      *model,
                                            GtkTreeIter       *iter,
                                            gpointer           data)
{
  gchar *str = NULL;

  gtk_tree_model_get (model, iter, COLUMN_NAME, &str, -1);
  g_object_set (GTK_CELL_RENDERER (cell), "text", str, NULL);

  g_free (str);
}

/* Обработчик установки фокуса на виджет.
 * Сохраняет список выделенных объектов.
 * */
static void
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

  if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE)
    {
      if (priv->selected != NULL)
        g_list_free_full (priv->selected, (GDestroyNotify) gtk_tree_path_free);

      priv->selected = gtk_tree_selection_get_selected_rows (selection, &model);

      if (priv->selected != NULL)
        priv->has_selected = TRUE;
    }
  else
    {
      if (gtk_tree_selection_get_selected (selection, &model, &priv->selected_iter))
        priv->has_selected = TRUE;
    }
}

/* Обработчик, вызываюшися после #hyscan_gtk_mark_manager_view_grab_focus ().
 * Выделяет объекты в соответствии с ранее сохранённым списком.
 * */
static void
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
      if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE)
        {
          for (GList *ptr = g_list_first (priv->selected); ptr != NULL; ptr = g_list_next (ptr))
            gtk_tree_selection_select_path (selection, (GtkTreePath*)ptr->data);
        }
      else
        {
          gtk_tree_selection_select_iter (selection, &priv->selected_iter);
        }
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
    }
}

/* Обработчик выделения объекта. */
static gboolean
hyscan_gtk_mark_manager_view_select_func (GtkTreeSelection *selection,
                                          GtkTreeModel     *model,
                                          GtkTreePath      *path,
                                          gboolean          path_currently_selected,
                                          gpointer          data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  /* Флаг повторного вызова функции. */
  static gboolean flag = FALSE;

  g_return_val_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (data), FALSE);

  self = HYSCAN_GTK_MARK_MANAGER_VIEW (data);
  priv = self->priv;

  /* Прерываем выполнение при повторном вызове. */
  if (flag)
    {
      priv->toggle_flag = FALSE;
      flag = FALSE;
      return FALSE;
    }

  /* Прерываем при обработке изменения статуса чек-бокса. */
  if (priv->toggle_flag)
    {
      flag = TRUE;
      return FALSE;
    }
  /* Выполняем обработчик выделения объекта. */
  return TRUE;
}

/* Установка фокуса для снятия выделения по умолчанию. */
static void
hyscan_gtk_mark_manager_view_on_show (GtkWidget *widget,
                                      gpointer   user_data)
{
  /* Возможно, чтобы не обрабатывать сигналы о фокусе,
   * нужно проверить получение фокуса первый раз и здесь. */
  gtk_widget_grab_focus (widget);
}

/* Обработчик перемещения мыши.
 * Возвращает : %TRUE  для предотвращения вызова других обработчиков события.
 *              %FALSE для распространения события дальше.
 * */
static gboolean
hyscan_gtk_mark_manager_view_on_mouse_move (GtkWidget      *widget,
                                            GdkEventMotion *event,
                                            gpointer        user_data)
{
  GtkTreeView       *view   = GTK_TREE_VIEW (widget);
  GtkTreePath       *path   = NULL;
  GtkTreeViewColumn *column = NULL;
  GdkRectangle       rect;
  gint               bin_x,
                     bin_y;

  g_print ("Coordinates\nx: %f, y: %f\n", event->x, event->y);

  gtk_tree_view_convert_widget_to_bin_window_coords (view, event->x, event->y, &bin_x, &bin_y);

  g_print ("x: %i, y: %i\n", bin_x, bin_y);

  if (!gtk_tree_view_get_path_at_pos (view, bin_x, bin_y, &path, &column, NULL, NULL))
    return TRUE;

  gtk_tree_view_get_cell_area (view, path, column, &rect);
  g_print ("Rect\nx: %i, y: %i\nwidth: %i, height: %i\n", rect.x, rect.y, rect.width, rect.height);
  g_print ("Position\nx : %i, y: %i\n", bin_x - rect.x, bin_y - rect.y);

  return FALSE; /* Чтобы TreeView реагировал на клик нужно отправить сигнал дальше. */
}

/* Отправляет сигнал о двойном клике по объекту*/
static void
hyscan_gtk_mark_manager_view_on_double_click (GtkTreeView       *tree_view,
                                              GtkTreePath       *path,
                                              GtkTreeViewColumn *column,
                                              gpointer           user_data)
{
  HyScanGtkMarkManagerView *self;
  HyScanGtkMarkManagerViewPrivate *priv;
  ModelManagerObjectType type;
  GtkTreeIter iter;
  gchar *id;

  g_return_if_fail (HYSCAN_IS_GTK_MARK_MANAGER_VIEW (user_data));

  self = HYSCAN_GTK_MARK_MANAGER_VIEW (user_data);
  priv = self->priv;

  if (!gtk_tree_model_get_iter (priv->store, &iter, path))
    return;

  gtk_tree_model_get (priv->store, &iter,
                      COLUMN_ID,   &id,
                      COLUMN_TYPE, &type,
                      -1);
  if (id == NULL)
    return;

  g_signal_emit (HYSCAN_GTK_MARK_MANAGER_VIEW (user_data),
                 hyscan_gtk_mark_manager_view_signals[SIGNAL_DOUBLE_CLICK],
                 0,
                 id,
                 type);
  g_free (id);
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
    gtk_tree_view_expand_all (priv->tree_view);
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
    gtk_tree_view_collapse_all (priv->tree_view);
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
  priv->has_selected = FALSE;
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

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  do
    hyscan_gtk_mark_manager_view_toggle (self, &iter, active);
  while (gtk_tree_model_iter_next (model, &iter));
}

/**
 * hyscan_gtk_mark_manager_view_expand_path:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @path: указатель на путь к объекту в модели.
 * @expand: %TRUE  - развёрнуть узел,
 *          %FALSE - свёрнуть узел
 *
 * Разворачивает или сворачивает узел по заданному пути.
 */
void
hyscan_gtk_mark_manager_view_expand_path (HyScanGtkMarkManagerView *self,
                                          GtkTreePath              *path,
                                          gboolean                  expanded)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (!GTK_IS_TREE_STORE (priv->store))
    return;

  if (expanded)
    {
      if (priv->signal_expanded == 0)
        return;

      g_signal_handler_block (G_OBJECT (priv->tree_view), priv->signal_expanded);

      if (!gtk_tree_view_row_expanded (priv->tree_view, path))
        gtk_tree_view_expand_to_path (priv->tree_view, path);

      g_signal_handler_unblock (G_OBJECT (priv->tree_view), priv->signal_expanded);
    }
  else
    {
      if (priv->signal_collapsed == 0)
        return;

      g_signal_handler_block (G_OBJECT (priv->tree_view), priv->signal_collapsed);

      if (gtk_tree_view_row_expanded (priv->tree_view, path))
        gtk_tree_view_collapse_row (priv->tree_view, path);

      g_signal_handler_unblock (G_OBJECT (priv->tree_view), priv->signal_collapsed);
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
              gchar    *id;
              gboolean  active;
              ModelManagerObjectType current_type;

              gtk_tree_model_get (priv->store,   &iter,
                                  COLUMN_ID,     &id,
                                  COLUMN_ACTIVE, &active,
                                  COLUMN_TYPE,   &current_type,
                                  -1);
              if (current_type == type && active)
                {
                  guint i = (list != NULL)? g_strv_length (list) : 0,
                        index = 0;
                  gboolean result = FALSE;

                  while (index < i)
                    {
                      if (IS_NOT_EQUAL (list[index++], id))
                        continue;

                      result = TRUE;
                      break;
                    }

                  if (!result)
                    {
                      list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
                      list[i++] = g_strdup (id);
                      list[i++] = NULL;
                    }
                }
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
              gchar    *id;
              gboolean  active;

              gtk_tree_model_get (priv->store,   &iter,
                                  COLUMN_ID,     &id,
                                  COLUMN_ACTIVE, &active,
                                  -1);
              if (active)
                {
                  guint i = (list != NULL)? g_strv_length (list) : 0,
                        index = 0;
                  gboolean result = FALSE;

                  while (index < i)
                    {
                      if (IS_NOT_EQUAL (list[index++], id))
                        continue;

                      result = TRUE;
                      break;
                    }

                  if (!result)
                    {
                      list = (gchar**)g_realloc ( (gpointer)list, (i + 2) * sizeof (gchar*));
                      list[i++] = g_strdup (id);
                      list[i++] = NULL;
                    }
                }
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
 * @store: модель представления данных
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
 * @store: указатель на модель с данными
 *
 * Устанавливает модель представления данных. */
void
hyscan_gtk_mark_manager_view_set_store (HyScanGtkMarkManagerView *self,
                                        GtkTreeModel             *store)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (priv->store == store)
    return;

  priv->store = g_object_ref (store);

  if (GTK_IS_TREE_STORE (priv->store))
    hyscan_gtk_mark_manager_view_set_tree_model (self);
  else if (GTK_IS_LIST_STORE (priv->store))
    hyscan_gtk_mark_manager_view_set_list_model (self);

  hyscan_gtk_mark_manager_view_update (self);
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
  GtkTreeModel *model;
  GtkTreeIter  *array = NULL, iter;
  guint         index;

  if (selection == NULL)
    return;

  model = gtk_tree_view_get_model (priv->tree_view);

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  array = (GtkTreeIter*)g_realloc (NULL, sizeof (GtkTreeIter));
  index = 0;

  do
    array = hyscan_gtk_mark_manager_view_find_items_by_id (model, &iter, array, &index, id);
  while (gtk_tree_model_iter_next (model, &iter));

  if (index == 0)
    {
      g_free (array);
      return;
    }

  if (priv->signal_selected != 0)
    g_signal_handler_block (G_OBJECT (selection), priv->signal_selected);

  gtk_tree_selection_unselect_all (selection);

  while (index != 0)
    {
      GtkTreePath *path;
      gboolean valid;

      index--;

      valid = GTK_IS_TREE_STORE (model) ?
              gtk_tree_store_iter_is_valid (GTK_TREE_STORE (model), &array[index]) :
              gtk_list_store_iter_is_valid (GTK_LIST_STORE (model), &array[index]);

      if (!valid)
        continue;

      path = gtk_tree_model_get_path (model, &array[index]);
      gtk_tree_path_up (path);
      gtk_tree_view_expand_to_path (priv->tree_view, path);
      gtk_tree_path_free (path);

      if (!gtk_tree_selection_iter_is_selected (selection, &array[index]))
        gtk_tree_selection_select_iter (selection, &array[index]);
    }

  if (priv->signal_selected != 0)
    g_signal_handler_unblock (G_OBJECT (selection), priv->signal_selected);

  g_free (array);
}

/**
 * hyscan_gtk_mark_manager_view_find_item_by_id:
 * @model: указатель на модель с данными
 * @iter: указатель на итератор
 * @id: идентификатор
 *
 * Функция для рекурсивного обхода модели и поиска записи по заданному идентификтору.
 * Returns: %TRUE - если объект найден, то копирует итератор в iter;
 *          %FALSE - объект не найден.
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

  if (IS_EQUAL (id, str))
    {
      g_free (str);
      return TRUE;
    }
  g_free (str);

  if (!gtk_tree_model_iter_children (model, &child_iter, iter))
    return FALSE;

  do
    {
      if (!hyscan_gtk_mark_manager_view_find_item_by_id (model, &child_iter, id))
        continue;

      *iter = child_iter;
      return TRUE;
    }
  while (gtk_tree_model_iter_next (model, &child_iter));

  return FALSE;
}

/**
 * hyscan_gtk_mark_manager_view_find_items_by_id:
 * @model: указатель на модель с данными
 * @iter: начальный итератор
 * @array: указатель на массив итераторов для хранения результатов поиска
 * @index: указатель на количество элементов (итераторов) в массиве
 * @id: идентификатор
 *
 * Функция для рекурсивного обхода модели и поиска записей по заданному идентификтору.
 * Returns: указатель на массив итераторов с результатами поиска
 */
GtkTreeIter*
hyscan_gtk_mark_manager_view_find_items_by_id  (GtkTreeModel *model,
                                                GtkTreeIter  *iter,
                                                GtkTreeIter  *array,
                                                guint        *index,
                                                const gchar  *id)
{
  GtkTreeIter child_iter;
  gchar *str;

  gtk_tree_model_get (model,     iter,
                      COLUMN_ID, &str,
                      -1);

  if (str == NULL)
    return array;

  if (IS_EQUAL (id, str))
    {
      GtkTreeIter *tmp = (GtkTreeIter*)g_realloc (array, ( (++(*index)) * sizeof (GtkTreeIter)));

      if (tmp != NULL && tmp != array)
        array = tmp;

      array[(*index) - 1] = *iter;

    }

  g_free (str);

  if (!gtk_tree_model_iter_children (model, &child_iter, iter))
    return array;

  do
    array = hyscan_gtk_mark_manager_view_find_items_by_id (model, &child_iter, array, index, id);
  while (gtk_tree_model_iter_next (model, &child_iter));

  return array;
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
hyscan_gtk_mark_manager_view_toggle_item (HyScanGtkMarkManagerView *self,
                                          gchar                    *id,
                                          gboolean                  active)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;
  GtkTreeModel *model = gtk_tree_view_get_model (priv->tree_view);
  GtkTreeIter  *array = (GtkTreeIter*)g_realloc (NULL, sizeof (GtkTreeIter)),
                iter;
  guint index = 0;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  do
    array = hyscan_gtk_mark_manager_view_find_items_by_id (model, &iter, array, &index, id);
  while (gtk_tree_model_iter_next (model, &iter));

  while (index != 0)
    {
      index--;

      if (priv->signal_toggled != 0)
        g_signal_handler_block (priv->toggle_renderer, priv->signal_toggled);

      if (GTK_IS_TREE_STORE (model))
        gtk_tree_store_set (GTK_TREE_STORE (model), &array[index], COLUMN_ACTIVE, active, -1);
      else if (GTK_IS_LIST_STORE (model))
        gtk_list_store_set (GTK_LIST_STORE (model), &array[index], COLUMN_ACTIVE, active, -1);

      hyscan_gtk_mark_manager_view_toggle_parent (self, &array[index]);

      if (priv->signal_toggled != 0)
        g_signal_handler_unblock (priv->toggle_renderer, priv->signal_toggled);
    }
  g_free (array);
}

/**
 * hyscan_gtk_mark_manager_view_block_signal_selected:
 * @self: указатель на структуру #HyScanGtkMarkManagerView
 * @block: %TRUE - блокировать, %FALSE - разблокировать
 *
 * Функция блокирует или разблокирует сигнал выбора строки.
 */
void
hyscan_gtk_mark_manager_view_block_signal_selected (HyScanGtkMarkManagerView *self,
                                                   gboolean                   block)
{
  HyScanGtkMarkManagerViewPrivate *priv = self->priv;

  if (priv->signal_selected == 0)
    return;

  if (block)
    g_signal_handler_block (gtk_tree_view_get_selection (priv->tree_view), priv->signal_selected);
  else
    g_signal_handler_unblock (gtk_tree_view_get_selection (priv->tree_view), priv->signal_selected);

  priv->has_selected = FALSE;
}

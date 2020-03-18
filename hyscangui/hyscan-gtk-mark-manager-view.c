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
#include <hyscan-gui-marshallers.h>

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
  GtkTreeView  *tree_view;       /* Представление. */
  GtkTreeModel *store;           /* Модель представления данных (табличное или древовидное). */
  gulong        signal_selected; /* Идентификатор сигнала об изменении выбранных объектов. */
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

static gboolean   hyscan_gtk_mark_manager_view_toggle_activate    (GtkCellRenderer       *cell,
                                                                   GdkEvent              *event,
                                                                   GtkWidget             *widget,
                                                                   const gchar           *path,
                                                                   const GdkRectangle    *background_area,
                                                                   const GdkRectangle    *cell_area,
                                                                   GtkCellRendererState   flags);

static void       hyscan_gtk_mark_manager_view_toggle_crutch      (void);

static void       on_toggle_renderer_clicked                      (GtkCellRendererToggle *cell_renderer,
                                                                   gchar                 *path,
                                                                   gpointer               user_data);

static void       hyscan_gtk_mark_manager_view_toggle             (HyScanMarkManagerView *self,
                                                                   GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter,
                                                                   gboolean               active);

static void       hyscan_gtk_mark_manager_view_toggle_parent      (HyScanMarkManagerView *self,
                                                                   GtkTreeModel          *model,
                                                                   GtkTreeIter           *iter);

static gboolean   hyscan_mark_manager_view_show_tooltip           (GtkWidget             *widget,
                                                                   gint                   x,
                                                                   gint                   y,
                                                                   gboolean               keyboard_mode,
                                                                   GtkTooltip            *tooltip,
                                                                   gpointer               user_data);

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
                  /*g_cclosure_marshal_VOID__OBJECT,*/
                  hyscan_gui_marshal_VOID__STRING_BOOLEAN,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_BOOLEAN);
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

  /* Инициализация GtkCellRendererToggle для корректной работы в составной ячейке. */
  hyscan_gtk_mark_manager_view_toggle_crutch ();
  /* Рамка со скошенными внутрь границами. */
  gtk_scrolled_window_set_shadow_type (widget, GTK_SHADOW_IN);

  hyscan_mark_manager_view_update (self);

  gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (priv->tree_view));

  /* Включаем отображение подсказок. */
  gtk_widget_set_has_tooltip (GTK_WIDGET (priv->tree_view), TRUE);
  /* Соединяем сигнал для показа подсказки с функцией-обработчиком */
  g_signal_connect (G_OBJECT (priv->tree_view), "query-tooltip",
                    G_CALLBACK (hyscan_mark_manager_view_show_tooltip), NULL);
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
          /*gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);*/
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
          /*gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);*/
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
          gtk_tree_selection_select_iter (selection, &child_iter);
          hyscan_gtk_mark_manager_view_select_track (model, &child_iter, selection);
        }
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
}

/* Обработчик сигнала на активацию чек-бокса, чтобы
 * он активизировался только кликом именно по нему.
 * */
gboolean
hyscan_gtk_mark_manager_view_toggle_activate (GtkCellRenderer* cell,
                                              GdkEvent* event,
                                              GtkWidget* widget,
                                              const gchar* path,
                                              const GdkRectangle* background_area,
                                              const GdkRectangle* cell_area,
                                              GtkCellRendererState flags)
{
  gboolean activatable;

  g_object_get (G_OBJECT (cell), "activatable", &activatable, NULL);

  if (activatable)
    {
      gint cell_width    = cell_area->x + cell_area->width,
           cell_height   = cell_area->y + cell_area->height;
      gboolean is_inside = (event->button.x >= cell_area->x &&
                            event->button.x <  cell_width   &&
                            event->button.y >= cell_area->y &&
                            event->button.y <  cell_height) ? TRUE : FALSE;
      if (event       != NULL             ||
          event->type != GDK_BUTTON_PRESS ||
          is_inside)
        {
            g_signal_emit_by_name(cell, "toggled", path);
            return TRUE;
        }
    }

  return FALSE;
}

/* Инициализирует чек-бокс, чтобы при упаковке в одну колонку с другими
 * GtkCellRenderer-ами чек-бокс активизировался только кликом именно
 * по нему, а не по любому GtkCellRenderer-у данной колонке.
 * */
void
hyscan_gtk_mark_manager_view_toggle_crutch (void)
{
  GtkCellRendererClass *cell_class;
  GtkCellRendererToggle *toggle_renderer;

  toggle_renderer = GTK_CELL_RENDERER_TOGGLE (gtk_cell_renderer_toggle_new());
  cell_class = GTK_CELL_RENDERER_CLASS (GTK_WIDGET_GET_CLASS (toggle_renderer));
  cell_class->activate = hyscan_gtk_mark_manager_view_toggle_activate;
  g_object_unref (G_OBJECT (toggle_renderer));
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

  model = gtk_tree_view_get_model (priv->tree_view);

  if (gtk_tree_model_get_iter_from_string (model, &iter, path))
    {
      gboolean    active;

      gtk_tree_model_get (model,         &iter,
                          COLUMN_ACTIVE, &active,
                          -1);

      hyscan_gtk_mark_manager_view_toggle (self, model, &iter, !active);

      if (GTK_IS_TREE_STORE (model))
        {
          hyscan_gtk_mark_manager_view_toggle_parent (self, model, &iter);
        }
    }
}

/* Устанавливает состояние чек-бокса, в том числе и дочерним объектам. */
void
hyscan_gtk_mark_manager_view_toggle (HyScanMarkManagerView *self,
                                     GtkTreeModel          *model,
                                     GtkTreeIter           *iter,
                                     gboolean               active)
{
  GtkTreeIter child_iter;
  gchar *id;

  gtk_tree_model_get (model,      iter,
                      COLUMN_ID, &id,
                      -1);

  g_print ("->id: %s\n", id);

  if (GTK_IS_TREE_STORE (model))
    gtk_tree_store_set(GTK_TREE_STORE(model), iter, COLUMN_ACTIVE, active, -1);
  else if (GTK_IS_LIST_STORE (model))
    gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_ACTIVE, active, -1);
  else
    return;

  if (id != NULL)
    {
      /* Отправляем сигнал об изменении состояния чек-бокса. */
      g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, active);

      g_free (id);
    }
  /* Если есть дочерние объекты. */
  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      do
        {
          hyscan_gtk_mark_manager_view_toggle (self, model, &child_iter, active);
        }
      while (gtk_tree_model_iter_next (model, &child_iter));
    }
}

/* Устанавливает состояние чек-бокса родительских элементов
 * рекурсивно до самого верхнего. Если отмечены все дочерние,
 * то и родительский отмечается.
 * */
void
hyscan_gtk_mark_manager_view_toggle_parent (HyScanMarkManagerView *self,
                                            GtkTreeModel          *model,
                                            GtkTreeIter           *iter)
{
  GtkTreeIter parent_iter;

  if (gtk_tree_model_iter_parent (model, &parent_iter, iter))
    {
      GtkTreeIter child_iter;

      if (gtk_tree_model_iter_children (model, &child_iter, &parent_iter))
        {
          gint total    = gtk_tree_model_iter_n_children  (model, &parent_iter),
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
          gtk_tree_store_set(GTK_TREE_STORE(model), &parent_iter, COLUMN_ACTIVE, flag, -1);

          if (id != NULL)
            {
              /* Отправляем сигнал об изменении состояния чек-бокса. */
              g_signal_emit (self, hyscan_mark_manager_view_signals[SIGNAL_TOGGLED], 0, id, flag);
              g_free (id);
            }

          hyscan_gtk_mark_manager_view_toggle_parent (self, model, &parent_iter);
        }
    }
}

/* Функция-обработчик для вывода подсказки. */
gboolean
hyscan_mark_manager_view_show_tooltip (GtkWidget  *widget,
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

  if (GTK_IS_LIST_STORE (model))
    {
      /* Определяем положение ветви, чтобы рядом показать подсказку. */
      gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);
      /* Выводим подсказку. */
      gtk_tooltip_set_text (tooltip, "TESTING");
    }
  else if (GTK_IS_TREE_STORE (model))
    {
      /* Получаем ветвь в виде индексов. */
      gint depth;
      gint *indeces = gtk_tree_path_get_indices_with_depth (path, &depth);
      if (indeces)
        {
          /* Строка с подсказкой. */
          gchar *str = "";
          /* Определяем положение ветви, чтобы рядом показать подсказку. */
          gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);
          /* Получаем итератор ветви. */
          GtkTreeIter iter;
          gtk_tree_model_get_iter (model, &iter, path);
          /* Освобождаем путь. (Вызывает ошибку)*/
          /*gtk_tree_path_free (path);*/
          switch (depth - 1)
          {
          /* Курсор на категории. */
          case 0:
            {
              str = g_strdup ("First");
            }
            break;
          /* Курсор на метке. */
          case 1:
            {
              /* Удостоверьтесь что вы закончили вызов gtk_tree_model_get() значением '-1' */
              /*gtk_tree_model_get (model, &iter, COLUMN_MARK, &str, -1);*/
              str = g_strdup ("Second");
            }
            break;
          /* Курсор на атрибуте метки. */
          case 2:
            {
              /* Удостоверьтесь что вы закончили вызов gtk_tree_model_get() значением '-1' */
              /*gtk_tree_model_get (model, &iter, COLUMN_VALUE, &str, -1);*/
              str = g_strdup ("Third");
            }
            break;
          /* Курсор где-то ещё, выводить подсказку не нужно. */
          default:
            {
              /* Освобождаем путь и выходим. */
              gtk_tree_path_free (path);
              return FALSE;
            }
          }
          /* Выводим подсказку. */
          gtk_tooltip_set_text (tooltip, str);
          g_free (str);
       }
      /* Освобождаем путь. */
      gtk_tree_path_free (path);
    }

  return TRUE;
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
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_LOCATION,
                                               "Location (WGS 84)", renderer,
                                               "text", COLUMN_LOCATION,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_TRACK_NAME,
                                               "Track name", renderer,
                                               "text", COLUMN_TRACK_NAME,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_BOARD,
                                               "Board", renderer,
                                               "text", COLUMN_BOARD,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_DEPTH,
                                               "Depth", renderer,
                                               "text", COLUMN_DEPTH,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_WIDTH,
                                               "Width", renderer,
                                               "text", COLUMN_WIDTH,
                                               NULL);
  gtk_tree_view_insert_column_with_attributes (priv->tree_view,
                                               COLUMN_SLANT_RANGE,
                                               "Slant range", renderer,
                                               "text", COLUMN_SLANT_RANGE,
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
  /* Связываем с колонкой, определяющей видимость. */
  gtk_tree_view_column_add_attribute (column, toggle_renderer, "visible", COLUMN_VISIBLE);
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
  /*g_object_set (GTK_CELL_RENDERER (cell), "active", active, NULL);*/
  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell), active);
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
      do
        {
          hyscan_gtk_mark_manager_view_toggle (self, model, &iter, active);
        }
      while (gtk_tree_model_iter_next (model, &iter));

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
                  GtkSelectionMode mode = gtk_tree_selection_get_mode (selection);
                  if (mode == GTK_SELECTION_SINGLE)        /* Единичное выделение. */
                    gtk_tree_selection_select_iter (selection, &iter);
                  else if (mode == GTK_SELECTION_MULTIPLE) /* Множественное выделение. */
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

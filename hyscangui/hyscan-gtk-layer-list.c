/* hyscan-gtk-layer-list.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-gtk-layer-list
 * @Short_description: Виджет списка слоёв
 * @Title: HyScanGtkLayerList
 *
 * Виджет показывает список слоёв #HyScanGtkLayer и позволяет управлять ими:
 *
 * - включает видимость слоя,
 * - переключает ввод мыши на указанный слой,
 * - показывает виджеты настройки, специфичные для каждого слоя (задаются пользователем).
 *
 *
 */

#include <glib/gi18n-lib.h>
#include "hyscan-gtk-layer-list.h"

#define EMPTY_STACK_CHILD "empty-child"

enum
{
  PROP_O,
  PROP_CONTAINER,
};

enum
{
  LAYER_VISIBLE_COLUMN,   /* Признак видимости слоя. */
  LAYER_KEY_COLUMN,       /* Уникальный ключ. */
  LAYER_TITLE_COLUMN,     /* Название слоя. */
  LAYER_COLUMN,           /* Указатель на объект слоя #HyScanGtkLayer. */
  LAYER_SENSITIVE_COLUMN, /* Доступна ли галочка видимости слоя. */
  N_COLUMNS
};

struct _HyScanGtkLayerListPrivate
{
  GtkWidget              *tree_view;        /* Виджет списка слоёв. */
  GtkWidget              *tools_stack;      /* Вмджет стека с пользовательскими виджетами настроек. */
  GtkListStore           *layer_store;      /* Модель параметров отображения слоёв. */
  HyScanGtkLayerContainer *container;       /* Контейнер слоёв. */
};

static void        hyscan_gtk_layer_list_object_constructed       (GObject               *object);
static void        hyscan_gtk_layer_list_object_finalize          (GObject               *object);
static void        hyscan_gtk_layer_list_set_property             (GObject               *object,
                                                                   guint                  prop_id,
                                                                   const GValue          *value,
                                                                   GParamSpec            *pspec);
static void        hyscan_gtk_layer_list_changed                  (GtkTreeSelection      *selection,
                                                                   HyScanGtkLayerList    *layer_list);
static GtkWidget * hyscan_gtk_layer_list_create_tree_view         (HyScanGtkLayerList    *layer_list,
                                                                   GtkTreeModel          *tree_model);
static void        hyscan_gtk_layer_list_on_enable                (GtkCellRendererToggle *cell_renderer,
                                                                   gchar                 *path,
                                                                   gpointer               user_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkLayerList, hyscan_gtk_layer_list, GTK_TYPE_BOX)

static void
hyscan_gtk_layer_list_class_init (HyScanGtkLayerListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_layer_list_object_constructed;
  object_class->finalize = hyscan_gtk_layer_list_object_finalize;
  object_class->set_property = hyscan_gtk_layer_list_set_property;

  g_object_class_install_property (object_class, PROP_CONTAINER,
    g_param_spec_object ("container", "HyScanGtkLayerContainer", "Container for layers",
                         HYSCAN_TYPE_GTK_LAYER_CONTAINER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_layer_list_init (HyScanGtkLayerList *gtk_layer_list)
{
  gtk_layer_list->priv = hyscan_gtk_layer_list_get_instance_private (gtk_layer_list);
}

static void
hyscan_gtk_layer_list_object_constructed (GObject *object)
{
  HyScanGtkLayerList *layer_list = HYSCAN_GTK_LAYER_LIST (object);
  GtkBox *box = GTK_BOX (object);
  HyScanGtkLayerListPrivate *priv = layer_list->priv;

  G_OBJECT_CLASS (hyscan_gtk_layer_list_parent_class)->constructed (object);

  priv->layer_store = gtk_list_store_new (N_COLUMNS,
                                          G_TYPE_BOOLEAN,        /* LAYER_VISIBLE_COLUMN */
                                          G_TYPE_STRING,         /* LAYER_KEY_COLUMN     */
                                          G_TYPE_STRING,         /* LAYER_TITLE_COLUMN   */
                                          HYSCAN_TYPE_GTK_LAYER, /* LAYER_COLUMN           */
                                          G_TYPE_BOOLEAN         /* LAYER_SENSITIVE_COLUMN */);

  priv->tree_view = hyscan_gtk_layer_list_create_tree_view (layer_list, GTK_TREE_MODEL (priv->layer_store));

  priv->tools_stack = gtk_stack_new ();
  gtk_stack_set_homogeneous (GTK_STACK (priv->tools_stack), FALSE);
  gtk_stack_set_hhomogeneous (GTK_STACK (priv->tools_stack), TRUE);
  gtk_stack_add_named (GTK_STACK (priv->tools_stack), gtk_box_new (GTK_ORIENTATION_VERTICAL, 0), EMPTY_STACK_CHILD);

  gtk_box_pack_start (box, priv->tree_view, FALSE, TRUE, 0);
  gtk_box_pack_start (box, priv->tools_stack, FALSE, TRUE, 0);
}

static void
hyscan_gtk_layer_list_object_finalize (GObject *object)
{
  HyScanGtkLayerList *gtk_layer_list = HYSCAN_GTK_LAYER_LIST (object);
  HyScanGtkLayerListPrivate *priv = gtk_layer_list->priv;

  g_clear_object (&priv->layer_store);
  g_clear_object (&priv->container);

  G_OBJECT_CLASS (hyscan_gtk_layer_list_parent_class)->finalize (object);
}

static void
hyscan_gtk_layer_list_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanGtkLayerList *layer_list = HYSCAN_GTK_LAYER_LIST (object);
  HyScanGtkLayerListPrivate *priv = layer_list->priv;

  switch (prop_id)
    {
    case PROP_CONTAINER:
      priv->container = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Обработчик включения видимости слоя. */
static void
hyscan_gtk_layer_list_on_enable (GtkCellRendererToggle *cell_renderer,
                                 gchar                 *path,
                                 gpointer               user_data)
{
  HyScanGtkLayerList *layer_list = HYSCAN_GTK_LAYER_LIST (user_data);
  HyScanGtkLayerListPrivate *priv = layer_list->priv;

  GtkTreeIter iter;
  GtkTreePath *tree_path;
  GtkTreeModel *tree_model = GTK_TREE_MODEL (priv->layer_store);

  gboolean visible;
  HyScanGtkLayer *layer;

  /* Узнаем, галочку какого слоя изменил пользователь. */
  tree_path = gtk_tree_path_new_from_string (path);
  gtk_tree_model_get_iter (tree_model, &iter, tree_path);
  gtk_tree_path_free (tree_path);
  gtk_tree_model_get (tree_model, &iter,
                      LAYER_COLUMN, &layer,
                      LAYER_VISIBLE_COLUMN, &visible, -1);

  /* Устанавливаем новое данных. */
  if (layer != NULL)
    {
      hyscan_gtk_layer_set_visible (layer, !visible);
      visible = hyscan_gtk_layer_get_visible (layer);

      g_object_unref (layer);
    }

  gtk_list_store_set (priv->layer_store, &iter, LAYER_VISIBLE_COLUMN, visible, -1);
}

static GtkWidget *
hyscan_gtk_layer_list_create_tree_view (HyScanGtkLayerList *layer_list,
                                        GtkTreeModel       *tree_model)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *visible_column, *layer_column;
  GtkWidget *tree_view;
  GtkTreeSelection *selection;

  /* Галочка с признаком видимости галса. */
  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (hyscan_gtk_layer_list_on_enable), layer_list);
  visible_column = gtk_tree_view_column_new_with_attributes (_("Show"), renderer,
                                                             "active", LAYER_VISIBLE_COLUMN,
                                                             "visible", LAYER_SENSITIVE_COLUMN,
                                                             NULL);

  /* Название галса. */
  renderer = gtk_cell_renderer_text_new ();
  layer_column = gtk_tree_view_column_new_with_attributes (_("Layer"), renderer,
                                                           "text", LAYER_TITLE_COLUMN, NULL);
  gtk_tree_view_column_set_expand (layer_column, TRUE);

  tree_view = gtk_tree_view_new_with_model (tree_model);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), layer_column);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), visible_column);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  g_signal_connect (selection, "changed", G_CALLBACK (hyscan_gtk_layer_list_changed), layer_list);

  return tree_view;
}

/* Обработчик переключения слоев. */
static void
hyscan_gtk_layer_list_changed (GtkTreeSelection   *selection,
                               HyScanGtkLayerList *layer_list)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;


  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *layer_tools = NULL;
  HyScanGtkLayer *layer = NULL;

  /* Получаем данные выбранного слоя. */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gchar *layer_key;

      gtk_tree_model_get (model, &iter,
                          LAYER_COLUMN, &layer,
                          LAYER_KEY_COLUMN, &layer_key,
                          -1);

      layer_tools = gtk_stack_get_child_by_name (GTK_STACK (priv->tools_stack), layer_key);

      g_free (layer_key);
    }

  /* Устанавливаем активный слой. */
  if (layer == NULL || !hyscan_gtk_layer_grab_input (layer))
    hyscan_gtk_layer_container_set_input_owner (priv->container, NULL);

  /* Показываем виджет настройки слоя. */
  if (layer_tools != NULL)
    gtk_stack_set_visible_child (GTK_STACK (priv->tools_stack), layer_tools);
  else
    gtk_stack_set_visible_child_name (GTK_STACK (priv->tools_stack), EMPTY_STACK_CHILD);

  g_clear_object (&layer);
}

/**
 * hyscan_gtk_layer_list_new:
 * @container: указатель на контейнер слоёв #HyScanGtkLayerContainer
 *
 * Создаёт новый виджет списка слоёв #HyScanGtkLayerList
 *
 * Returns: указатель на новый #HyScanGtkLayerList
 */
GtkWidget *
hyscan_gtk_layer_list_new (HyScanGtkLayerContainer *container)
{
  return g_object_new (HYSCAN_TYPE_GTK_LAYER_LIST,
                       "container", container,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       "spacing", 6,
                       NULL);
}

/**
 * hyscan_gtk_layer_list_add:
 * @layer_list: указатель на #HyScanGtkLayerList
 * @layer: (nullable): указатель на слой или %NULL
 * @name: уникальное имя слоя
 * @title: название слоя, которое выводится для пользователя
 *
 * Добавляет строку со слоем @layer в список слоёв виджета.
 */
void
hyscan_gtk_layer_list_add (HyScanGtkLayerList *layer_list,
                           HyScanGtkLayer     *layer,
                           const gchar        *name,
                           const gchar        *title)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;
  GtkTreeIter tree_iter;

  /* Регистрируем слой в layer_store. */
  gtk_list_store_append (priv->layer_store, &tree_iter);
  gtk_list_store_set (priv->layer_store, &tree_iter,
                      LAYER_VISIBLE_COLUMN, layer == NULL ? TRUE : hyscan_gtk_layer_get_visible (layer),
                      LAYER_KEY_COLUMN, name,
                      LAYER_TITLE_COLUMN, title,
                      LAYER_COLUMN, layer,
                      LAYER_SENSITIVE_COLUMN, layer != NULL,
                      -1);
}

/**
 * hyscan_gtk_layer_list_set_tools:
 * @layer_list: указатель на #HyScanGtkLayerList
 * @name: имя слоя
 * @tools: виджет настроек слоя
 *
 * Устанавливает виджет настроек слоя с именем @name. Виджет настроек будет
 * отображаться при выборе указанного слоя.
 */
void
hyscan_gtk_layer_list_set_tools (HyScanGtkLayerList *layer_list,
                                 const gchar        *name,
                                 GtkWidget          *tools)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;

  gtk_stack_add_named (GTK_STACK (priv->tools_stack), tools, name);
}

/**
 * hyscan_gtk_layer_list_get_visible_ids:
 * @list:
 *
 * Returns: (transfer full): нуль-терминированный массив идентификаторов видимых слоёв.
 */
gchar **
hyscan_gtk_layer_list_get_visible_ids (HyScanGtkLayerList *list)
{
  HyScanGtkLayerListPrivate *priv;
  GtkTreeIter iter;
  gboolean valid;
  GArray *array;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_LIST (list), NULL);
  priv = list->priv;

  array = g_array_new (TRUE, FALSE, sizeof (gchar *));

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->layer_store), &iter);
  while (valid)
   {
     HyScanGtkLayer *layer;
     gchar *layer_key;

     gtk_tree_model_get (GTK_TREE_MODEL (priv->layer_store), &iter,
                         LAYER_KEY_COLUMN, &layer_key,
                         LAYER_COLUMN, &layer,
                         -1);

     if (layer != NULL && hyscan_gtk_layer_get_visible (layer))
       g_array_append_val (array, layer_key);
     else
       g_free (layer_key);

     g_clear_object (&layer);

     valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->layer_store), &iter);
   }

  return (gchar **) g_array_free (array, FALSE);
}

/**
 * hyscan_gtk_layer_list_set_visible_ids:
 * @list
 * @ids
 *
 * Делает слои с ключами @ids видимыми, а остальные - невидимыми.
 *
 */
void
hyscan_gtk_layer_list_set_visible_ids (HyScanGtkLayerList  *list,
                                       gchar              **ids)
{
  HyScanGtkLayerListPrivate *priv;
  GtkTreeIter iter;
  gboolean valid;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_LIST (list));
  priv = list->priv;

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->layer_store), &iter);
  while (valid)
   {
     gchar *layer_key;
     HyScanGtkLayer *layer;
     gboolean visible;

     gtk_tree_model_get (GTK_TREE_MODEL (priv->layer_store), &iter,
                         LAYER_KEY_COLUMN, &layer_key,
                         LAYER_COLUMN, &layer,
                         -1);

     /* Записи, у которых нет слоя, всегда считаем видимыми. */
     visible = layer == NULL || g_strv_contains ((const gchar *const *) ids, layer_key);
     gtk_list_store_set (priv->layer_store, &iter, LAYER_VISIBLE_COLUMN, visible, -1);
     if (layer != NULL)
       hyscan_gtk_layer_set_visible (layer, visible);

     g_free (layer_key);
     g_clear_object (&layer);

     valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->layer_store), &iter);
   }
}

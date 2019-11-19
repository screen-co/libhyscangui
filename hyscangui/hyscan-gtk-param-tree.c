/* hyscan-gtk-param-tree.c
 *
 * Copyright 2018 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui.
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
 * SECTION: hyscan-gtk-param-tree
 * @Short_description: виджет, отображающий HyScanParam одним списком
 * @Title: HyScanGtkParamTree
 *
 * Виджет, отображающий дерево параметров. Слева располагается иерархия схемы,
 * а справа - ключи текущей ветви.
 */

#include "hyscan-gtk-param-tree.h"
#include <glib/gi18n-lib.h>

enum
{
  ID_COL,
  NAME_COL,
  DESCRIPTION_COL,
  N_COLUMNS
};

struct _HyScanGtkParamTreePrivate
{
  GtkStack    *stack;       /* Стек со страницами (правая половина). */
  GHashTable  *param_lists; /* HyScanParamList'ы для страниц. */

  GtkTreeModelFilter *model;
  gchar              *filter;
};

static void        hyscan_gtk_param_tree_object_constructed   (GObject                    *object);
static void        hyscan_gtk_param_tree_object_finalize      (GObject                    *object);
static void        hyscan_gtk_param_tree_plist_adder          (gpointer                    data,
                                                               gpointer                    udata);
static GtkWidget * hyscan_gtk_param_tree_make_tree            (HyScanGtkParamTree         *self,
                                                               GtkTreeStore              **_store);
static gboolean    hyscan_gtk_param_tree_populate             (const HyScanDataSchemaNode *node,
                                                               GtkTreeView                *view,
                                                               GtkTreeIter                *parent,
                                                               GtkStack                   *stack,
                                                               GHashTable                 *widgets,
                                                               GHashTable                 *plists,
                                                               gboolean                    show_hidden);
static GtkWidget * hyscan_gtk_param_tree_make_page            (const HyScanDataSchemaNode *node,
                                                               GHashTable                 *widgets,
                                                               GHashTable                 *plists,
                                                               gboolean                    show_hidden);
static GtkWidget * hyscan_gtk_param_tree_make_links           (const HyScanDataSchemaNode *node,
                                                               GtkTreeView                *view,
                                                               GtkTreeModel               *model,
                                                               GtkTreeIter                *parent);
gboolean           hyscan_gtk_param_tree_activate_link        (GtkLabel                   *label,
                                                               gchar                      *uri,
                                                               gpointer                    user_data);
const HyScanDataSchemaNode *
                   hyscan_gtk_param_tree_find_node            (const HyScanDataSchemaNode *node,
                                                               const gchar                *path);

static gboolean    hyscan_gtk_param_tree_tree_visible_func    (GtkTreeModel               *model,
                                                               GtkTreeIter                *iter,
                                                               gpointer                    data);
static void        hyscan_gtk_param_search_changed            (GtkEntry                   *entry,
                                                               gpointer                    udata);
static void        hyscan_gtk_param_tree_row_activated        (GtkTreeView                *tree_view,
                                                               GtkTreePath                *path,
                                                               GtkTreeViewColumn          *column,
                                                               gpointer                    user_data);
static void        hyscan_gtk_param_tree_set_page             (HyScanGtkParamTree         *self,
                                                               const gchar                *id);
static void        hyscan_gtk_param_tree_selection_changed    (GtkTreeSelection           *select,
                                                               HyScanGtkParamTree         *tree);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkParamTree, hyscan_gtk_param_tree, HYSCAN_TYPE_GTK_PARAM);

static void
hyscan_gtk_param_tree_class_init (HyScanGtkParamTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_param_tree_object_constructed;
  object_class->finalize = hyscan_gtk_param_tree_object_finalize;
}

static void
hyscan_gtk_param_tree_init (HyScanGtkParamTree *self)
{
  self->priv = hyscan_gtk_param_tree_get_instance_private (self);
}

static void
hyscan_gtk_param_tree_object_constructed (GObject *object)
{
  const HyScanDataSchemaNode *nodes;
  GHashTable *widgets;
  GtkTreeStore *tstore;
  GtkTreeModel *filtered_model;
  GtkWidget *filter_entry;
  GtkWidget *tview, *tree_scroll;
  gboolean show_hidden;

  HyScanGtkParamTree *self = HYSCAN_GTK_PARAM_TREE (object);
  HyScanGtkParamTreePrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_param_tree_parent_class)->constructed (object);

  priv->param_lists = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             NULL, g_object_unref);
  /* Правая половина с виджетами. */
  priv->stack = GTK_STACK (gtk_stack_new ());
  g_object_set (priv->stack,
                "hexpand", TRUE, "vexpand", TRUE,
                "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN,
                NULL);

  /* Левая половина с древесиной. */
  nodes = hyscan_gtk_param_get_nodes (HYSCAN_GTK_PARAM (self));
  widgets = hyscan_gtk_param_get_widgets (HYSCAN_GTK_PARAM (self));
  show_hidden = hyscan_gtk_param_get_show_hidden (HYSCAN_GTK_PARAM (self));

  tview = hyscan_gtk_param_tree_make_tree (self, &tstore);
  hyscan_gtk_param_tree_populate (nodes, GTK_TREE_VIEW (tview), NULL,
                                  priv->stack, widgets,priv->param_lists,
                                  show_hidden);
  hyscan_gtk_param_tree_set_page (self, "/");

  /* Создаем фильтрацию полей.
   * TODO: сейчас нельзя создать вью без модели, т.к. функции наполнения
   * выше берут модель из вьюхи и потом подключают сигналы.
   * Это надо устранить, возможно, закинув вью и оригинальную модель в priv. */
  filtered_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (tstore), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filtered_model),
                                          hyscan_gtk_param_tree_tree_visible_func,
                                          priv, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (tview), filtered_model);
  priv->model = GTK_TREE_MODEL_FILTER (filtered_model);

  /* Текстовое поле для поиска. */
  filter_entry = gtk_search_entry_new ();
  g_signal_connect (filter_entry, "search-changed",
                    G_CALLBACK (hyscan_gtk_param_search_changed), priv);

  tree_scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                              "vadjustment", NULL,
                              "hadjustment", NULL,
                              "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                              "hscrollbar-policy", GTK_POLICY_NEVER,
                              "child", tview,
                              NULL);

  /* Упаковываем всё в сетку. */
  gtk_grid_attach (GTK_GRID (self), filter_entry,             0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (tree_scroll), 0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (priv->stack), 1, 0, 1, 2);
}

static void
hyscan_gtk_param_tree_object_finalize (GObject *object)
{
  HyScanGtkParamTree *self = HYSCAN_GTK_PARAM_TREE (object);

  g_hash_table_unref (self->priv->param_lists);

  G_OBJECT_CLASS (hyscan_gtk_param_tree_parent_class)->finalize (object);
}

/* Функция, добавляет ключ в список параметров. */
static void
hyscan_gtk_param_tree_plist_adder (gpointer data,
                                   gpointer udata)
{
  HyScanDataSchemaKey *pkey = data;
  HyScanParamList *plist = udata;

  hyscan_param_list_add (plist, pkey->id);
}

/* Функция создает и настраивает виджет дерева. */
static GtkWidget *
hyscan_gtk_param_tree_make_tree (HyScanGtkParamTree  *self,
                                 GtkTreeStore       **_store)
{
  GtkTreeView *tree = NULL;
  GtkTreeStore *store = NULL;
  GtkCellRenderer *renderer = NULL;
  GtkTreeViewColumn *column = NULL;
  GtkTreeSelection *select = NULL;

  store = gtk_tree_store_new (N_COLUMNS,
                              G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  tree = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (store)));

  gtk_widget_set_vexpand (GTK_WIDGET (tree), TRUE);
  gtk_tree_view_set_enable_tree_lines (tree, TRUE);
  gtk_tree_view_set_enable_search (tree, FALSE);
  g_signal_connect (tree, "row-activated",
                    G_CALLBACK (hyscan_gtk_param_tree_row_activated), NULL);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("ID", renderer,
                                                     "text", ID_COL,
                                                     NULL);
  gtk_tree_view_column_set_visible (column, FALSE);
  gtk_tree_view_append_column (tree, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Name"), renderer,
                                                     "text", NAME_COL,
                                                     NULL);
  gtk_tree_view_append_column (tree, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Description"), renderer,
                                                     "markup", DESCRIPTION_COL,
                                                     NULL);
  gtk_tree_view_append_column (tree, column);

  select = gtk_tree_view_get_selection (tree);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (select, "changed",
                    G_CALLBACK (hyscan_gtk_param_tree_selection_changed), self);

  *_store = store;

  return GTK_WIDGET (tree);
}

/* Функция добавляет узел в дерево. */
static void
hyscan_gtk_param_tree_append_node (GtkTreeStore               *store,
                                   GtkTreeIter                *iter,
                                   GtkTreeIter                *parent,
                                   const HyScanDataSchemaNode *node)
{
  gchar *descr;
  gchar *name;
  const gchar *text;

  /* Определяем, что будет в полях "название" и "описание". */
  name = hyscan_gtk_param_get_node_name (node);

  text = (node->description != NULL) ? node->description : "";
  descr = g_markup_printf_escaped ("<span size=\"smaller\">\%s</span>", text);

  /* Добавляем в дерево. */
  gtk_tree_store_append (store, iter, parent);
  gtk_tree_store_set (store, iter,
                      ID_COL, node->path,
                      NAME_COL, name,
                      DESCRIPTION_COL, descr,
                      -1);

  g_free (name);
  g_free (descr);
}

static gboolean
hyscan_gtk_param_tree_populate (const HyScanDataSchemaNode *node,
                                GtkTreeView                *view,
                                GtkTreeIter                *parent,
                                GtkStack                   *stack,
                                GHashTable                 *widgets,
                                GHashTable                 *plists,
                                gboolean                    show_hidden)
{
  GList *link;
  GtkTreeIter iter;
  GtkWidget *page;
  GtkTreeStore *store;
  gboolean have_subnodes = FALSE;
  gboolean have_keys = FALSE;

  if (node == NULL)
    {
      g_warning ("Node is NULL. Maybe you set wrong root.");
      return FALSE;
    }

  store = GTK_TREE_STORE (gtk_tree_view_get_model (view));

  /* Для всех узлов... */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      gboolean populated;
      HyScanDataSchemaNode *child = link->data;

      /* Добавляем узел в дерево и углубляемся. */
      hyscan_gtk_param_tree_append_node (store, &iter, parent, child);
      populated = hyscan_gtk_param_tree_populate (child, view, &iter,
                                                  stack, widgets, plists,
                                                  show_hidden);

      /* Если ничего добавлено не было, то удаляем узел. Это, кстати, автоматически
       * инвалидирует итер, но последующий вызов hyscan_gtk_param_tree_append_node
       * снова сделает его валидным.
       * Иначе запоминаем, что на этом уровне или ниже что-то да есть. */
      if (!populated)
        gtk_tree_store_remove (store, &iter);
      else
        have_subnodes = TRUE;
    }

  /* Оказались на уровне текущих ключей. Если добавлять нечего, то досрочно
   * выходим, не забыв оповестить вызывающего, добавили мы что-то или нет.
   * А ещё делаем страничку-заглушку, если ниже были какие-то ключи. */
  have_keys = hyscan_gtk_param_node_has_visible_keys (node, show_hidden);
  if (!have_keys)
    {
      if (have_subnodes)
        {
          page = hyscan_gtk_param_tree_make_links (node, view,
                                                   GTK_TREE_MODEL (store),
                                                   parent);

          gtk_stack_add_named (stack, page, node->path);
        }

      return have_subnodes;
    }

  /* Добавляем страничку в стек и узел в дерево. */
  page = hyscan_gtk_param_tree_make_page (node, widgets, plists, show_hidden);
  gtk_stack_add_named (stack, page, node->path);

  if (parent == NULL)
    hyscan_gtk_param_tree_append_node (store, &iter, parent, node);

  /* Возвращаем тру, так как элемент был добавлен. */
  return TRUE;
}

/* Функция создания страницы (ключей текущего узла). */
static GtkWidget *
hyscan_gtk_param_tree_make_page (const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets,
                                 GHashTable                 *plists,
                                 gboolean                    show_hidden)
{
  GtkWidget *box, *scroll;
  GtkSizeGroup *size;
  HyScanParamList *plist;
  GList *link;

  if (node->keys == NULL)
    return NULL;

  plist = hyscan_param_list_new ();
  g_list_foreach (node->keys, hyscan_gtk_param_tree_plist_adder, plist);
  g_hash_table_insert (plists, (gchar*)node->path, plist);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* Добавляем только ключи. */
  for (link = node->keys; link != NULL; link = link->next)
    {
      HyScanDataSchemaKey *key = link->data;
      GtkWidget *paramkey = g_hash_table_lookup (widgets, key->id);

      if ((key->access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN) && !show_hidden)
        continue;

      gtk_box_pack_start (GTK_BOX (box), paramkey, FALSE, TRUE, 0);
      hyscan_gtk_param_key_add_to_size_group (HYSCAN_GTK_PARAM_KEY (paramkey),
                                              size);
    }

  g_object_unref (size);

  gtk_widget_set_hexpand (GTK_WIDGET (box), TRUE);

  g_object_set (box, "margin", 18, NULL);

  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "vadjustment", NULL,
                         "hadjustment", NULL,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "hscrollbar-policy", GTK_POLICY_NEVER,
                         "child", box,
                         NULL);

  gtk_widget_set_name (scroll, node->path);
  gtk_widget_show_all (scroll);

  return scroll;
}

static GtkWidget *
hyscan_gtk_param_tree_make_links (const HyScanDataSchemaNode *node,
                                  GtkTreeView                *view,
                                  GtkTreeModel               *model,
                                  GtkTreeIter                *parent)
{
  gboolean valid;
  GtkTreeIter iter;
  const HyScanDataSchemaNode *node_link;
  gchar *node_path;
  gchar *uri, *markup, *text;
  GtkWidget *scroll;
  GtkWidget *label;
  GtkWidget *box;

  /* Инициализируем итератор и проходим по всем дочерним элементам. */
  valid = gtk_tree_model_iter_children (model, &iter, parent);

  if (!valid)
    return NULL;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  while (valid)
    {
      /* Строка TreePath будет использоваться, как uri. */
      uri = gtk_tree_model_get_string_from_iter (model, &iter);
      gtk_tree_model_get (model, &iter, ID_COL, &node_path, -1);

      /* Ищем соответствующий узел и определяем текст ссылки. */
      node_link = hyscan_gtk_param_tree_find_node (node, node_path);
      text = hyscan_gtk_param_get_node_name (node_link);

      /* Составляем markup ссылки. */
      markup = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", uri, text);

      /* И наконец составляем виджет, подключаем сигнал и упаковываем. */
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), markup);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      g_signal_connect (label, "activate-link", G_CALLBACK (hyscan_gtk_param_tree_activate_link), view);

      gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

      g_free (node_path);
      g_free (uri);
      g_free (text);
      g_free (markup);

      /* Следующий элемент. */
      valid = gtk_tree_model_iter_next (model, &iter);
    }

  g_object_set (box, "margin", 18, NULL);

  scroll = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                         "vadjustment", NULL,
                         "hadjustment", NULL,
                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                         "hscrollbar-policy", GTK_POLICY_NEVER,
                         "child", box,
                         NULL);
  gtk_widget_set_name (scroll, node->path);
  gtk_widget_show_all (scroll);

  return scroll;
}

gboolean
hyscan_gtk_param_tree_activate_link (GtkLabel *label,
                                     gchar    *uri,
                                     gpointer  user_data)
{
  GtkTreeView *view = user_data;
  GtkTreeSelection *sel;
  GtkTreePath *path;

  path = gtk_tree_path_new_from_string (uri);
  sel = gtk_tree_view_get_selection (view);

  gtk_tree_view_expand_to_path (view, path);
  gtk_tree_selection_select_path (sel, path);

  g_clear_pointer (&path, gtk_tree_path_free);

  return TRUE;
}

const HyScanDataSchemaNode *
hyscan_gtk_param_tree_find_node (const HyScanDataSchemaNode *node,
                                 const gchar                *path)
{
  const HyScanDataSchemaNode * found;
  GList *link = node->nodes;

  if (0 == (g_strcmp0 (node->path, path)))
    return node;

  for (link = node->nodes; link != NULL; link = link->next)
    {
      found = hyscan_gtk_param_tree_find_node (link->data, path);

      if (found != NULL)
        return found;
    }

  return NULL;
}

/* Функция определяет видимость узлов TreeModel. */
static gboolean
hyscan_gtk_param_tree_tree_visible_func (GtkTreeModel *model,
                                         GtkTreeIter  *iter,
                                         gpointer      data)
{
  HyScanGtkParamTreePrivate *priv = data;
  gchar *id, *name;
  gboolean visible = FALSE;
  GtkTreeIter child;

  if (priv->filter == NULL || g_str_equal (priv->filter, ""))
    return TRUE;

  /* Рекурсивно проходим по всем дочерним узлам. */
  if (gtk_tree_model_iter_children (model, &child, iter))
    {
      do
        {
          if (hyscan_gtk_param_tree_tree_visible_func (model, &child, data))
            return TRUE;
        }
      while (gtk_tree_model_iter_next (model, &child));
    }

  /* Если подузлов нет, сравниваем имя и путь текущего узла с фильтром. */
  gtk_tree_model_get (model, iter, ID_COL, &id, NAME_COL, &name, -1);

  if ((id != NULL && g_strstr_len (id, -1, priv->filter) != NULL) ||
      (name != NULL && g_strstr_len (name, -1, priv->filter) != NULL))
    {
      visible = TRUE;
    }

  g_free (id);
  g_free (name);

  return visible;
}

/* Функция вызывается при смене значения фильтра. */
static void
hyscan_gtk_param_search_changed (GtkEntry   *entry,
                                 gpointer    udata)
{
  HyScanGtkParamTreePrivate *priv = udata;
  const gchar * string = NULL;

  string = gtk_entry_get_text (entry);
  g_clear_pointer (&priv->filter, g_free);
  priv->filter = g_strdup (string);

  /* Инициируем перефильтрацию модели. */
  gtk_tree_model_filter_refilter (priv->model);
}

/* Функция раскрывания дерева по двойному клику. */
static void
hyscan_gtk_param_tree_row_activated (GtkTreeView       *tree_view,
                                     GtkTreePath       *path,
                                     GtkTreeViewColumn *column,
                                     gpointer           user_data)
{
  gtk_tree_view_expand_row (tree_view, path, FALSE);
}

/* Функция задания страницы. */
static void
hyscan_gtk_param_tree_set_page (HyScanGtkParamTree *self,
                                const gchar        *id)
{
  HyScanGtkParamTreePrivate *priv = self->priv;
  HyScanParamList *plist;

  gtk_stack_set_visible_child_name (priv->stack, id);
  plist = g_hash_table_lookup (priv->param_lists, id);

  if (plist == NULL)
    {
      g_info ("HyScanGtkParamTree: ParamList not found. This may be an error "
              "or HyScanDataSchemaNode with no keys. ");
    }
  else
    {
      hyscan_gtk_param_set_watch_list (HYSCAN_GTK_PARAM (self), plist);
    }
}

/* Функция обрабатывает выбор страницы. */
static void
hyscan_gtk_param_tree_selection_changed (GtkTreeSelection   *select,
                                         HyScanGtkParamTree *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *id;

  if (!gtk_tree_selection_get_selected (select, &model, &iter))
    return;

  gtk_tree_model_get (model, &iter, ID_COL, &id, -1);

  if (id == NULL)
    return;

  hyscan_gtk_param_tree_set_page (self, id);
  g_free (id);
}

/**
 * hyscan_gtk_param_tree_new:
 * @param param указатель на интерфейс HyScanParam
 * @root: корневой узел схемы
 * @show_hidden: показывать ли скрытые ключи
 *
 * Функция создает виджет #HyScanGtkParamTree.
 *
 * Returns: #HyScanGtkParamTree.
 */
GtkWidget *
hyscan_gtk_param_tree_new (HyScanParam *param,
                           const gchar *root,
                           gboolean     show_hidden)
{
  g_return_val_if_fail (HYSCAN_IS_PARAM (param), NULL);

  return g_object_new (HYSCAN_TYPE_GTK_PARAM_TREE,
                       "param", param,
                       "root", root,
                       "hidden", show_hidden,
                       NULL);
}

/**
 * hyscan_gtk_param_tree_new_default:
 * @param param указатель на интерфейс HyScanParam
 *
 * Функция создает виджет #HyScanGtkParamTree c настройками по умолчанию.
 *
 * Returns: #HyScanGtkParamTree.
 */
GtkWidget*
hyscan_gtk_param_tree_new_default (HyScanParam *param)
{
  return g_object_new (HYSCAN_TYPE_GTK_PARAM_TREE,
                       "param", param,
                       NULL);
}

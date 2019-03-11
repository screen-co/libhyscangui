/* hyscan-gtk-param-cc.c
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
 * SECTION: hyscan-gtk-param-cc
 * @Short_description: виджета отображения параметров в стиле gnome-control-center
 * @Title: HyScanGtkParamCC
 *
 * Виджет отображает параметры в виде списка категорий слева и списка параметров
 * справа. Подходит только для схем с уровнем вложенности не больше 2.
 *
 * корень         <- Страница 1
 * ├ уровень 1    ┐
 * │ ├ уровень 2  │ Страница 2
 * │ └ уровень 2  ┘
 * └ уровень 1    ┐
 *   ├ уровень 2  │ Страница 3
 *   └ уровень 2  ┘
 */

#include "hyscan-gtk-param-cc.h"

#define SCROLLABLE(widget) g_object_new (GTK_TYPE_SCROLLED_WINDOW,  \
                                         "vadjustment", NULL, \
                                         "hadjustment", NULL, \
                                         "vscrollbar-policy", GTK_POLICY_AUTOMATIC, \
                                         "hscrollbar-policy", GTK_POLICY_NEVER, \
                                         "child", (widget), NULL)

struct _HyScanGtkParamCCPrivate
{
  GtkStack   *stack;  /* Стек со всеми страницами. */
  GtkListBox *lbox;   /* Виджет списка страниц. */
  GPtrArray  *paths;  /* Названия страниц. */
  GPtrArray  *plists; /* ParamList'ы для страниц. */
};

static void        hyscan_gtk_param_cc_object_constructed   (GObject                     *object);
static void        hyscan_gtk_param_cc_object_finalize      (GObject                     *object);
static void        hyscan_gtk_param_cc_plist_adder          (gpointer                     data,
                                                             gpointer                     udata);
static void        hyscan_gtk_param_cc_row_activated        (GtkListBox                  *box,
                                                             GtkListBoxRow               *row,
                                                             gpointer                     udata);
static void        hyscan_gtk_param_cc_make_level0          (HyScanGtkParamCC            *self,
                                                             GHashTable                  *widgets);
static GtkWidget * hyscan_gtk_param_cc_make_level1          (const HyScanDataSchemaNode  *node,
                                                             GHashTable                  *widgets,
                                                             HyScanParamList             *plist,
                                                             gboolean                     show_hidden);
static GtkWidget * hyscan_gtk_param_cc_make_level2          (const HyScanDataSchemaNode  *node,
                                                             GHashTable                  *widgets,
                                                             HyScanParamList             *plist,
                                                             gboolean                     show_label,
                                                             gboolean                     show_hidden);
static GtkWidget * hyscan_gtk_param_cc_add_row              (HyScanGtkParamCC            *self,
                                                             const HyScanDataSchemaNode  *node,
                                                             HyScanParamList             *plist);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkParamCC, hyscan_gtk_param_cc, HYSCAN_TYPE_GTK_PARAM);

static void
hyscan_gtk_param_cc_class_init (HyScanGtkParamCCClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_param_cc_object_constructed;
  object_class->finalize = hyscan_gtk_param_cc_object_finalize;
}

static void
hyscan_gtk_param_cc_init (HyScanGtkParamCC *self)
{
  self->priv = hyscan_gtk_param_cc_get_instance_private (self);
}

static void
hyscan_gtk_param_cc_object_constructed (GObject *object)
{
  GHashTable *widgets;
  GtkWidget *scroll;
  HyScanGtkParamCC *self = HYSCAN_GTK_PARAM_CC (object);
  HyScanGtkParamCCPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_param_cc_parent_class)->constructed (object);

  /* */
  priv->paths = g_ptr_array_new_full (1, g_free);
  priv->plists = g_ptr_array_new_full (1, g_object_unref);

  /* Переключатели. */
  priv->lbox = GTK_LIST_BOX (gtk_list_box_new ());
  g_signal_connect (priv->lbox, "row-activated",
                    G_CALLBACK (hyscan_gtk_param_cc_row_activated), self);

  /* Правая половина с виджетами. */
  priv->stack = GTK_STACK (gtk_stack_new ());
  g_object_set (priv->stack,
                "hexpand", TRUE, "vexpand", TRUE,
                "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN,
                NULL);


  widgets = hyscan_gtk_param_get_widgets (HYSCAN_GTK_PARAM (self));

  hyscan_gtk_param_cc_make_level0 (self, widgets);

  {
    GtkListBoxRow *row;
    row = gtk_list_box_get_row_at_index (priv->lbox, 0);
    gtk_list_box_select_row (priv->lbox, row);
    hyscan_gtk_param_cc_row_activated (priv->lbox, row, self);
  }

  scroll = SCROLLABLE (priv->lbox);

  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (scroll), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (priv->stack), 1, 0, 1, 1);
}

static void
hyscan_gtk_param_cc_object_finalize (GObject *object)
{
  HyScanGtkParamCC *self = HYSCAN_GTK_PARAM_CC (object);
  HyScanGtkParamCCPrivate *priv = self->priv;
  g_ptr_array_unref (priv->paths);
  g_ptr_array_unref (priv->plists);

  G_OBJECT_CLASS (hyscan_gtk_param_cc_parent_class)->finalize (object);
}

/* Функция добавляет идентификаторы ключей в HyScanParamList. */
static void
hyscan_gtk_param_cc_plist_adder (gpointer data,
                                 gpointer udata)
{
  HyScanDataSchemaKey *pkey = data;
  HyScanParamList *plist = udata;

  hyscan_param_list_add (plist, pkey->id);
}

/* Обработчик смены выбранного раздела. */
static void
hyscan_gtk_param_cc_row_activated (GtkListBox    *box,
                                   GtkListBoxRow *row,
                                   gpointer       udata)
{
  gint row_index;
  gchar *path;
  HyScanParamList *plist;
  HyScanGtkParamCC *self = udata;
  HyScanGtkParamCCPrivate *priv = self->priv;

  g_return_if_fail (row != NULL);

  row_index = gtk_list_box_row_get_index (row);
  path = g_ptr_array_index (priv->paths, row_index);
  plist = g_ptr_array_index (priv->plists, row_index);

  gtk_stack_set_visible_child_name (priv->stack, path);

  hyscan_gtk_param_set_watch_list (HYSCAN_GTK_PARAM (self), plist);
}

/* Функция создает виджет нулевого уровня (ключи + все виджеты 1 уровня). */
static void
hyscan_gtk_param_cc_make_level0 (HyScanGtkParamCC           *self,
                                 GHashTable                 *widgets)
{
  HyScanGtkParamCCPrivate *priv = self->priv;
  const HyScanDataSchemaNode *node;
  gboolean show_hidden;
  HyScanParamList *plist;
  GList *link;
  GtkWidget *box;

  node = hyscan_gtk_param_get_nodes (HYSCAN_GTK_PARAM (self));
  show_hidden = hyscan_gtk_param_get_show_hidden (HYSCAN_GTK_PARAM (self));

  /* Ключи этого уровня. */
  plist = hyscan_param_list_new ();
  box = hyscan_gtk_param_cc_make_level2 (node, widgets, plist, FALSE, show_hidden);
  if (box != NULL)
    {
      GtkWidget *scroll;

      g_object_set (box, "margin", 18, NULL);
      scroll = SCROLLABLE (box);

      gtk_stack_add_titled (priv->stack, scroll, node->path, node->path);
      hyscan_gtk_param_cc_add_row (self, node, plist);
    }
  g_object_unref (plist);

  /* Подузлы этого уровня. */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      HyScanDataSchemaNode *subnode = link->data;

      plist = hyscan_param_list_new ();
      box = hyscan_gtk_param_cc_make_level1 (subnode, widgets, plist, show_hidden);
      if (box != NULL)
        {
          gtk_stack_add_titled (priv->stack, box, subnode->path, subnode->path);
          hyscan_gtk_param_cc_add_row (self, subnode, plist);
        }

      g_clear_object (&plist);
    }

}

/* Функция создает виджет 1 уровня (текущие ключи + ключи 2 уровня). */
static GtkWidget *
hyscan_gtk_param_cc_make_level1 (const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets,
                                 HyScanParamList            *plist,
                                 gboolean                    show_hidden)
{
  GtkWidget *box, *widget;
  HyScanDataSchemaNode *subnode;
  GList *link;
  gboolean empty = TRUE;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  g_object_set (box, "margin", 18, NULL);

  widget = hyscan_gtk_param_cc_make_level2 (node, widgets, plist, FALSE, show_hidden);
  if (widget != NULL)
    {
      gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
      empty = FALSE;
    }

  for (link = node->nodes; link != NULL; link = link->next)
    {
      subnode = link->data;

      widget = hyscan_gtk_param_cc_make_level2 (subnode, widgets, plist, TRUE, show_hidden);

      if (!widget)
        continue;

      gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
      empty = FALSE;
    }

  if (empty)
    {
      g_clear_object (&box);
      return NULL;
    }
  else
    {
      GtkWidget *scroll = SCROLLABLE (box);
      return scroll;
    }
}

/* Функция создает виджет 2 уровня (текущие ключи). */
static GtkWidget *
hyscan_gtk_param_cc_make_level2 (const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets,
                                 HyScanParamList            *plist,
                                 gboolean                    show_label,
                                 gboolean                    show_hidden)
{
  GList *link;
  GtkWidget *box, *frame, *pkey;
  GtkSizeGroup *size;

  if (!hyscan_gtk_param_node_has_visible_keys (node, show_hidden))
    return NULL;

  g_list_foreach (node->keys, hyscan_gtk_param_cc_plist_adder, plist);

  box = g_object_new (GTK_TYPE_LIST_BOX,
                      "selection-mode", GTK_SELECTION_NONE,
                      NULL);

  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_set_vexpand (GTK_WIDGET (box), FALSE);
  gtk_widget_set_valign (GTK_WIDGET (box), GTK_ALIGN_START);

  for (link = node->keys; link != NULL; link = link->next)
    {
      HyScanDataSchemaKey *key = link->data;

      /* А скрытые ключи я тебе, лысый, не покажу. */
      if ((key->access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN) && !show_hidden)
        continue;

      pkey = g_hash_table_lookup (widgets, key->id);
      if (pkey == NULL)
        continue;

      hyscan_gtk_param_key_add_to_size_group (HYSCAN_GTK_PARAM_KEY (pkey), size);

      gtk_list_box_insert (GTK_LIST_BOX (box), pkey, -1);
    }

  g_object_unref (size);

  if (!show_label)
    return box;

  frame = gtk_frame_new (node->name);
  g_object_set (frame, "shadow-type", GTK_SHADOW_NONE, NULL);

  gtk_container_add (GTK_CONTAINER (frame), box);
  return frame;
}

/* Функция добавляет ряд (страницу) в левую часть. */
static GtkWidget *
hyscan_gtk_param_cc_add_row (HyScanGtkParamCC           *self,
                             const HyScanDataSchemaNode *node,
                             HyScanParamList            *plist)
{
  HyScanGtkParamCCPrivate *priv = self->priv;
  GtkWidget *box, *title, *subtitle;
  gchar *markup, *name;
  const gchar *desc;

  /* Виджет заголовка. */
  name = hyscan_gtk_param_get_node_name (node);
  title = g_object_new (GTK_TYPE_LABEL,
                        "wrap", FALSE,
                        "single-line-mode", TRUE,
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "width-chars", 20,
                        "label", name,
                        NULL);

  /* Виджет описания. */
  desc = (node->description != NULL) ? node->description : "";
  markup = g_markup_printf_escaped ("<span size=\"smaller\">\%s</span>", desc);

  subtitle = g_object_new (GTK_TYPE_LABEL,
                           "wrap", FALSE,
                           "single-line-mode", TRUE,
                           "ellipsize", PANGO_ELLIPSIZE_END,
                           "label", markup,
                           "use-markup", TRUE,
                           NULL);

  /* Контейнер для заголовка и описания. */
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "margin", 6, NULL);

  /* Упаковка. */
  gtk_box_pack_start (GTK_BOX (box), title, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (box), subtitle, FALSE, FALSE, 0);

  gtk_list_box_insert (GTK_LIST_BOX (priv->lbox), box, -1);

  g_ptr_array_add (priv->paths, (gpointer)g_strdup (node->path));
  g_ptr_array_add (priv->plists, g_object_ref (plist));

  g_free (markup);
  g_free (name);

  return NULL;
}

/**
 * hyscan_gtk_param_cc_new:
 * @param: интерфейс #HyScanParam
 * @root: корневой узел схемы
 * @show_hidden: показывать ли скрытые ключи
 *
 * Функция создаёт виджет #HyScanGtkParamCC.
 *
 * Returns: #HyScanGtkParamCC.
 */
GtkWidget *
hyscan_gtk_param_cc_new (HyScanParam *param,
                         const gchar *root,
                         gboolean     show_hidden)
{
  return g_object_new (HYSCAN_TYPE_GTK_PARAM_CC,
                       "param", param,
                       "root", root,
                       "hidden", show_hidden,
                       NULL);
}

/**
 * hyscan_gtk_param_cc_new_default:
 * @param: интерфейс #HyScanParam
 *
 * Функция создаёт виджет #HyScanGtkParamCC с настройками по умолчанию.
 *
 * Returns: #HyScanGtkParamCC.
 */
GtkWidget *
hyscan_gtk_param_cc_new_default (HyScanParam *param)
{
  return g_object_new (HYSCAN_TYPE_GTK_PARAM_CC,
                       "param", param,
                       NULL);
}


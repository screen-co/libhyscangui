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
                                                             const HyScanDataSchemaNode  *node,
                                                             GHashTable                  *widgets);
static GtkWidget * hyscan_gtk_param_cc_make_level1          (const HyScanDataSchemaNode  *node,
                                                             GHashTable                  *widgets,
                                                             HyScanParamList             *plist);
static GtkWidget * hyscan_gtk_param_cc_make_level2          (const HyScanDataSchemaNode  *node,
                                                             GHashTable                  *widgets,
                                                             HyScanParamList             *plist,
                                                             gboolean                     label);
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
  HyScanDataSchema *schema;
  const HyScanDataSchemaNode *node;
  GHashTable *widgets;
  GtkWidget *abar, *scroll;
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

  schema = hyscan_gtk_param_get_schema (HYSCAN_GTK_PARAM (self));
  node = hyscan_data_schema_list_nodes (schema);

  widgets = hyscan_gtk_param_get_widgets (HYSCAN_GTK_PARAM (self));

  hyscan_gtk_param_cc_make_level0 (self, node, widgets);

  {
    GtkListBoxRow *row;
    row = gtk_list_box_get_row_at_index (priv->lbox, 0);
    gtk_list_box_select_row (priv->lbox, row);
    hyscan_gtk_param_cc_row_activated (priv->lbox, row, self);
  }

  scroll = SCROLLABLE (priv->lbox);

  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (scroll), 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (self), GTK_WIDGET (priv->stack), 1, 0, 1, 1);

  abar = hyscan_gtk_param_make_action_bar (HYSCAN_GTK_PARAM (self));

  gtk_grid_attach (GTK_GRID (self), abar, 0, 1, 2, 1);
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
  HyScanGtkParamCC *self = udata;
  HyScanGtkParamCCPrivate *priv = self->priv;

  gint row_index = gtk_list_box_row_get_index (row);
  gchar *path = g_ptr_array_index (priv->paths, row_index);
  HyScanParamList *plist = g_ptr_array_index (priv->plists, row_index);

  gtk_stack_set_visible_child_name (priv->stack, path);

  hyscan_gtk_param_set_watch_list (HYSCAN_GTK_PARAM (self), plist);
}

/* Функция создает виджет нулевого уровня (ключи + все виджеты 1 уровня). */
static void
hyscan_gtk_param_cc_make_level0 (HyScanGtkParamCC           *self,
                                 const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets)
{
  HyScanGtkParamCCPrivate *priv = self->priv;
  HyScanParamList *plist;
  GList *link;
  GtkWidget *box;

  plist = hyscan_param_list_new ();

  /* Ключи этого уровня. */
  box = hyscan_gtk_param_cc_make_level2 (node, widgets, plist, FALSE);
  if (box != NULL)
    {
      GtkWidget *scroll;
      g_object_set (box, "margin", 18, NULL);

      scroll = SCROLLABLE (box);

      gtk_stack_add_titled (priv->stack, scroll, node->path, node->path);
      hyscan_gtk_param_cc_add_row (self, node, plist);
    }

  /* Подузлы этого уровня. */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      HyScanDataSchemaNode *subnode = link->data;

      plist = hyscan_param_list_new ();
      box = hyscan_gtk_param_cc_make_level1 (subnode, widgets, plist);
      gtk_stack_add_titled (priv->stack, box, subnode->path, subnode->path);
      hyscan_gtk_param_cc_add_row (self, subnode, plist);
    }
}

/* Функция создает виджет 1 уровня (текущие ключи + ключи 2 уровня). */
static GtkWidget *
hyscan_gtk_param_cc_make_level1 (const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets,
                                 HyScanParamList            *plist)
{
  GtkWidget *box, *widget, *scroll;
  HyScanDataSchemaNode *subnode;
  GList *link;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  g_object_set (box, "margin", 18, NULL);

  widget = hyscan_gtk_param_cc_make_level2 (node, widgets, plist, FALSE);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  for (link = node->nodes; link != NULL; link = link->next)
    {
      subnode = link->data;

      widget = hyscan_gtk_param_cc_make_level2 (subnode, widgets, plist, TRUE);

      gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
    }

  scroll = SCROLLABLE (box);
  return scroll;
}

/* Функция создает виджет 2 уровня (текущие ключи). */
static GtkWidget *
hyscan_gtk_param_cc_make_level2 (const HyScanDataSchemaNode *node,
                                 GHashTable                 *widgets,
                                 HyScanParamList            *plist,
                                 gboolean                    label)
{
  GList *link;
  GtkWidget *box, *frame;
  GtkSizeGroup *size;

  if (node->keys == NULL)
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
      GtkWidget *pkey = g_hash_table_lookup (widgets, key->id);

      if (pkey == NULL)
        continue;

      hyscan_gtk_param_key_add_to_size_group (HYSCAN_GTK_PARAM_KEY (pkey), size);

      gtk_list_box_insert (GTK_LIST_BOX (box), pkey, -1);
    }

  g_object_unref (size);

  if (!label)
    return box;

  frame = gtk_frame_new (node->name);
  g_object_set (frame, "shadow-type", GTK_SHADOW_NONE,
                       "margin-top", 12, NULL);

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
  GtkWidget *box, *name, *descr;
  gchar *markup;
  const gchar * text;

  /* Виджет заголовка. */
  name = g_object_new (GTK_TYPE_LABEL,
                       "wrap", FALSE,
                       "single-line-mode", TRUE,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "width-chars", 20,
                       "label", node->name,
                       NULL);

  /* Виджет описания. */
  text = (node->description != NULL) ? node->description : "";
  markup = g_markup_printf_escaped ("<span size=\"smaller\">\%s</span>", text);

  descr = g_object_new (GTK_TYPE_LABEL,
                       "wrap", FALSE,
                       "single-line-mode", TRUE,
                       "ellipsize", PANGO_ELLIPSIZE_END,
                       "label", markup,
                       "use-markup", TRUE,
                       NULL);
  g_free (markup);

  /* Контейнер для заголовка и описания. */
  box = g_object_new (GTK_TYPE_BOX,
                      "orientation", GTK_ORIENTATION_VERTICAL,
                      "margin", 6, NULL);

  /* Упаковка. */
  gtk_box_pack_start (GTK_BOX (box), name, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (box), descr, FALSE, FALSE, 0);

  gtk_list_box_insert (GTK_LIST_BOX (priv->lbox), box, -1);

  g_ptr_array_add (priv->paths, (gpointer)g_strdup (node->path));
  g_ptr_array_add (priv->plists, plist);

  return NULL;
}

/**
 * hyscan_gtk_param_cc_new:
 * @param: интерфейс #HyScanParam
 *
 * Функция создаёт новый виджет #HyScanGtkParamCC
 *
 * Returns: Свежесозданный #HyScanGtkParamCC.
 */
GtkWidget *
hyscan_gtk_param_cc_new (HyScanParam *param)
{
  return g_object_new (HYSCAN_TYPE_GTK_PARAM_CC,
                       "param", param,
                       NULL);
}

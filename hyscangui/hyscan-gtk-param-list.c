/* hyscan-gtk-param-list.c
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
 * SECTION: hyscan-gtk-param-list
 * @Short_description: виджет, отображающий HyScanParam одним списком
 * @Title: HyScanGtkParamList
 *
 * Простейший виджет, отображающий все ключи единым списком. Маловероятно, что
 * кто-то захочет его использовать, зато он хорош образовательных целях.
 */

#include "hyscan-gtk-param-list.h"

static void       hyscan_gtk_param_list_object_constructed (GObject *object);
static void       hyscan_gtk_param_list_object_finalize    (GObject *object);

G_DEFINE_TYPE (HyScanGtkParamList, hyscan_gtk_param_list, HYSCAN_TYPE_GTK_PARAM);

static void
hyscan_gtk_param_list_class_init (HyScanGtkParamListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_param_list_object_constructed;
  object_class->finalize = hyscan_gtk_param_list_object_finalize;
}

static void
hyscan_gtk_param_list_init (HyScanGtkParamList *self)
{
}

static void
hyscan_gtk_param_list_add_widgets (HyScanGtkParamList   *self,
                                   const HyScanDataSchemaNode *node,
                                   GtkWidget            *container,
                                   HyScanParamList      *plist,
                                   GtkSizeGroup         *size)
{
  GtkWidget *widget;
  GList *link;
  gboolean show_hidden;
  GHashTable *widgets;

  /* Текущий узел. */
  if (node == NULL)
    {
      node = hyscan_gtk_param_get_nodes (HYSCAN_GTK_PARAM (self));
      if (node == NULL)
        {
          g_warning ("Node is NULL. Maybe you set wrong root.");
          return;
        }
    }

  show_hidden = hyscan_gtk_param_get_show_hidden (HYSCAN_GTK_PARAM (self));
  widgets = hyscan_gtk_param_get_widgets (HYSCAN_GTK_PARAM (self));

  /* Для одинакового размера виджетов. */
  if (size == NULL)
    size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* Рекурсивно идем по всем узлам. */
  for (link = node->nodes; link != NULL; link = link->next)
    {
      HyScanDataSchemaNode *subnode = link->data;
      hyscan_gtk_param_list_add_widgets (self, subnode, container, plist, size);
    }

  /* А теперь по всем ключам. */
  for (link = node->keys; link != NULL; link = link->next)
    {
      HyScanDataSchemaKey *key = link->data;

      if ((key->access & HYSCAN_DATA_SCHEMA_ACCESS_HIDDEN) && !show_hidden)
        continue;

      /* Ищем виджет. */
      widget = g_hash_table_lookup (widgets, key->id);

      /* Путь - в список отслеживаемых. */
      hyscan_param_list_add (plist, key->id);

      /* Виджет (с твиками) в контейнер. */
      g_object_set (widget, "margin-start", 12, "margin-end", 12, NULL);
      hyscan_gtk_param_key_add_to_size_group (HYSCAN_GTK_PARAM_KEY (widget), size);
      gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
    }
}

static void
hyscan_gtk_param_list_object_constructed (GObject *object)
{
  GtkWidget *scrolled;
  GtkWidget *subbox;
  HyScanParamList *plist;

  HyScanGtkParamList *self = HYSCAN_GTK_PARAM_LIST (object);

  G_OBJECT_CLASS (hyscan_gtk_param_list_parent_class)->constructed (object);

  /* Контейнер для виджетов. */
  subbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  /* Список параметров для слежения. */
  plist = hyscan_param_list_new ();

  hyscan_gtk_param_list_add_widgets (self, NULL, subbox, plist, NULL);

  /* Устанавливаем список отслеживаемых (то есть все пути). */
  hyscan_gtk_param_set_watch_list (HYSCAN_GTK_PARAM (self), plist);
  g_object_unref (plist);

  /* Прокрутка области параметров. */
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (scrolled), subbox);
  g_object_set (scrolled, "hexpand", TRUE, "vexpand", TRUE, NULL);
  gtk_grid_attach (GTK_GRID (self), scrolled, 0, 0, 1, 1);
}

static void
hyscan_gtk_param_list_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (hyscan_gtk_param_list_parent_class)->finalize (object);
}

/**
 * hyscan_gtk_param_list_new:
 * @param param указатель на интерфейс HyScanParam
 * @root: корневой узел схемы
 * @show_hidden: показывать ли скрытые ключи
 *
 * Функция создает виджет #HyScanGtkParamList.
 *
 * Returns: #HyScanGtkParamList.
 */
GtkWidget *
hyscan_gtk_param_list_new (HyScanParam *param,
                           const gchar *root,
                           gboolean     show_hidden)
{
  g_return_val_if_fail (HYSCAN_IS_PARAM (param), NULL);

  return g_object_new (HYSCAN_TYPE_GTK_PARAM_LIST,
                       "param", param,
                       "root", root,
                       "hidden", show_hidden,
                       NULL);
}

/**
 * hyscan_gtk_param_list_new_default:
 * @param param указатель на интерфейс HyScanParam
 *
 * Функция создает виджет #HyScanGtkParamList c настройками по умолчанию.
 *
 * Returns: #HyScanGtkParamList.
 */
GtkWidget *
hyscan_gtk_param_list_new_default (HyScanParam *param)
{
  g_return_val_if_fail (HYSCAN_IS_PARAM (param), NULL);

  return g_object_new (HYSCAN_TYPE_GTK_PARAM_LIST,
                       "param", param,
                       NULL);
}

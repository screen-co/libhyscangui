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

static void hyscan_gtk_param_list_object_constructed (GObject *object);
static void hyscan_gtk_param_list_object_finalize    (GObject *object);

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
hyscan_gtk_param_list_object_constructed (GObject *object)
{
  GtkWidget *abar;
  GtkWidget *scrolled;
  GtkWidget *subbox;
  GtkSizeGroup *size;
  HyScanParamList *plist;
  gpointer htkey, htval;

  GHashTable *ht;
  GHashTableIter iter;

  HyScanGtkParamList *self = HYSCAN_GTK_PARAM_LIST (object);

  G_OBJECT_CLASS (hyscan_gtk_param_list_parent_class)->constructed (object);

  /* Контейнер для виджетов. */
  subbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  /* Для одинакового размера виджетов. */
  size = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* Список параметров для слежения. */
  plist = hyscan_param_list_new ();

  /* Каждый путь и каждый виджет нужно положить куда следует. */
  ht = hyscan_gtk_param_get_widgets (HYSCAN_GTK_PARAM (self));
  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &htkey, &htval))
    {
      gchar *kpath = htkey;
      GtkWidget *kwidget = htval;

      /* Путь - в список отслеживаемых. */
      hyscan_param_list_add (plist, kpath);

      /* Виджет (с твиками) в контейнер. */
      g_object_set (kwidget, "margin-start", 12, "margin-end", 12, NULL);
      hyscan_gtk_param_key_add_to_size_group (HYSCAN_GTK_PARAM_KEY (kwidget), size);
      gtk_box_pack_start (GTK_BOX (subbox), kwidget, FALSE, FALSE, 0);
    }

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

  /* Кнопки внизу виджета, отмена и применить. */
  abar = hyscan_gtk_param_make_action_bar (HYSCAN_GTK_PARAM (self));
  gtk_grid_attach (GTK_GRID (self), abar, 0, 1, 1, 1);
}

static void
hyscan_gtk_param_list_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (hyscan_gtk_param_list_parent_class)->finalize (object);
}

/**
 * hyscan_gtk_param_list_new:
 * @param param указатель на интерфейс HyScanParam
 *
 * Функция создает новый виджет.
 *
 * Returns: виджет, отображающий список ключей.
 */
GtkWidget*
hyscan_gtk_param_list_new (HyScanParam *param)
{
  g_return_val_if_fail (HYSCAN_IS_PARAM (param), NULL);

  return g_object_new (HYSCAN_TYPE_GTK_PARAM_LIST,
                       "param", param,
                       NULL);
}

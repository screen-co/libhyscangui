/* hyscan-gtk-layer-list.c
 *
 * Copyright 2019-2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * @Short_description: Виджет управления слоями и настройками
 * @Title: HyScanGtkLayerList
 *
 * Виджет #HyScanGtkLayerList представляет из себя меню доступных слоёв #HyScanGtkLayer
 * и виджетов настроек, связанных с каждым из слоёв.
 *
 * - hyscan_gtk_layer_list_add() - добавляет пункт меню,
 * - hyscan_gtk_layer_list_set_tools() - устанавливает виджет настроек для пункта меню.
 *
 * Если для пункта меню установлен виджет настройки, то этот виджет будет показан
 * при выборе указанного пункта, при этом инструменты других пунктов будут скрыты.
 *
 * Если пункт меню связан со слоем #HyScanGtkLayer, то в меню выводятся кнопки
 * включения и отключения видимости слоя, а при выборе этого пункта ввод мыши в контейнере
 * слоёв передаётся в выбранный слой.
 *
 * Пользователь может установить собственную функцию на кнопку включения видимости
 * с помощью функции hyscan_gtk_layer_list_set_visible_fn().
 *
 */

#include "hyscan-gtk-layer-list.h"
#include <hyscan-gui-style.h>
#include <glib/gi18n-lib.h>

enum
{
  PROP_O,
  PROP_CONTAINER,
};

struct _HyScanGtkLayerListPrivate
{
  HyScanGtkLayerContainer *container;       /* Контейнер слоёв. */
  GHashTable              *list_items;      /* Хэш-таблица пунктов меню. */
  gchar                   *active;          /* Идентификатор выбранного пункта. */
};

typedef struct {
  HyScanGtkLayerList               *list;            /* Указатель на HyScanGtkLayerList. */
  HyScanGtkLayer                   *layer;           /* Слой HyScanGtkLayer. */
  gchar                            *name;            /* Идентификатор пункта меню. */
  hyscan_gtk_layer_list_visible_fn  visible_fn;      /* Реакция на изменения видимости слоя. */
  GtkWidget                        *activate_btn;    /* Кнопка активации пункта меню. */
  GtkWidget                        *vsbl_btn;        /* Кнопка управления видимостью слоя. */
  GtkWidget                        *vsbl_img;        /* Иконка в кнопке управления видимостью слоя. */
  GtkWidget                        *revealer;        /* Контейнер для виджета настройки. */
} HyScanGtkLayerListItem;

static void        hyscan_gtk_layer_list_object_constructed       (GObject                *object);
static void        hyscan_gtk_layer_list_object_finalize          (GObject                *object);
static void        hyscan_gtk_layer_list_set_property             (GObject                *object,
                                                                   guint                   prop_id,
                                                                   const GValue           *value,
                                                                   GParamSpec             *pspec);
static void        hyscan_gtk_layer_list_item_free                (HyScanGtkLayerListItem *item);
static void        hyscan_gtk_layer_list_toggle_active            (HyScanGtkLayerListItem *item);
static void        hyscan_gtk_layer_list_toggle_visible           (HyScanGtkLayerListItem *item);

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

  hyscan_gui_style_init ();
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
  HyScanGtkLayerListPrivate *priv = layer_list->priv;

  G_OBJECT_CLASS (hyscan_gtk_layer_list_parent_class)->constructed (object);

  priv->list_items = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
                                            (GDestroyNotify) hyscan_gtk_layer_list_item_free);
}

static void
hyscan_gtk_layer_list_object_finalize (GObject *object)
{
  HyScanGtkLayerList *gtk_layer_list = HYSCAN_GTK_LAYER_LIST (object);
  HyScanGtkLayerListPrivate *priv = gtk_layer_list->priv;

  g_hash_table_destroy (priv->list_items);
  g_clear_object (&priv->container);
  g_free (priv->active);

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

/* Удаляет структуру HyScanGtkLayerListItem. */
static void
hyscan_gtk_layer_list_item_free (HyScanGtkLayerListItem *item)
{
  g_clear_object (&item->layer);
  g_free (item->name);
  g_free (item);
}

/* Обработчик сигнала "notify::active" для кнопки видимости слоя. */
static void
hyscan_gtk_layer_list_toggle_visible (HyScanGtkLayerListItem *item)
{
  gboolean visible;
  const gchar *icon_name;

  if (item->layer == NULL)
    return;

  visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->vsbl_btn));
  item->visible_fn (item->layer, visible);

  icon_name = visible ? "eye" : "hidden";
  item->vsbl_img = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);

  gtk_widget_show_all (item->vsbl_img);
  gtk_button_set_image (GTK_BUTTON (item->vsbl_btn), item->vsbl_img);
}

/* Обработчик сигнала "notify::active" для кнопки активации слоя. */
static void
hyscan_gtk_layer_list_toggle_active (HyScanGtkLayerListItem *item)
{
  HyScanGtkLayerListPrivate *priv = item->list->priv;
  HyScanGtkLayerListItem *prev_item;
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item->activate_btn));

  /* Скрываем виджет настройки и деактивизируем текущий слой. */
  prev_item = priv->active != NULL ? g_hash_table_lookup (priv->list_items, priv->active) : NULL;
  if (prev_item != NULL)
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (prev_item->revealer), FALSE);

      g_signal_handlers_block_by_func (prev_item->activate_btn, hyscan_gtk_layer_list_toggle_active, prev_item);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prev_item->activate_btn), FALSE);
      g_signal_handlers_unblock_by_func (prev_item->activate_btn, hyscan_gtk_layer_list_toggle_active, prev_item);
    }
  hyscan_gtk_layer_container_set_input_owner (priv->container, NULL);

  g_free (priv->active);
  priv->active = active ? g_strdup (item->name) : NULL;

  /* Показываем виджет настройки и активизируем новый слой. */
  if (active && gtk_bin_get_child (GTK_BIN (item->revealer)) != NULL)
    gtk_revealer_set_reveal_child (GTK_REVEALER (item->revealer), TRUE);

  if (!active || item->layer == NULL)
    return;

  hyscan_gtk_layer_grab_input (item->layer);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->vsbl_btn), TRUE);
}

/**
 * hyscan_gtk_layer_list_new:
 * @container: указатель на контейнер слоёв #HyScanGtkLayerContainer
 *
 * Создаёт новый виджет списка слоёв #HyScanGtkLayerList.
 *
 * Returns: указатель на новый #HyScanGtkLayerList
 */
GtkWidget *
hyscan_gtk_layer_list_new (HyScanGtkLayerContainer *container)
{
  return g_object_new (HYSCAN_TYPE_GTK_LAYER_LIST,
                       "container", container,
                       "orientation", GTK_ORIENTATION_VERTICAL,
                       NULL);
}

/**
 * hyscan_gtk_layer_list_add:
 * @layer_list: указатель на #HyScanGtkLayerList
 * @layer: (nullable): указатель на слой или %NULL
 * @key: уникальный ключ слоя
 * @title: название слоя, которое выводится для пользователя
 *
 * Добавляет пункт меню с ключом @key. При выборе этого пункта отображается виджет
 * настройки, соответствующий этому ключу, а также происходит захват ввода слоем @layer.
 *
 * Для добавления виджета настроек см. функцию hyscan_gtk_layer_list_set_tools().
 */
void
hyscan_gtk_layer_list_add (HyScanGtkLayerList *layer_list,
                           HyScanGtkLayer     *layer,
                           const gchar        *key,
                           const gchar        *title)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;
  HyScanGtkLayerListItem *item;

  GtkWidget *btn_box, *box;
  GtkWidget *activate_btn_arrow;

  item = g_new0 (HyScanGtkLayerListItem, 1);
  item->name = g_strdup (key);
  item->list = layer_list;

  /* Кнопка включения видимости слоя. */
  if (layer != NULL)
    {
      item->layer = g_object_ref (layer);
      item->vsbl_btn = gtk_toggle_button_new_with_label (NULL);
      item->visible_fn = hyscan_gtk_layer_set_visible;
      gtk_widget_set_tooltip_text (item->vsbl_btn, _("Layer visibility"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->vsbl_btn), hyscan_gtk_layer_get_visible (item->layer));
      hyscan_gtk_layer_list_toggle_visible (item);
      g_signal_connect_swapped (item->vsbl_btn, "notify::active", G_CALLBACK (hyscan_gtk_layer_list_toggle_visible), item);
    }

  /* Кнопка активации пункта меню. */
  item->activate_btn = gtk_toggle_button_new_with_label (title);
  activate_btn_arrow = gtk_image_new_from_icon_name ("pan-start-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_image (GTK_BUTTON (item->activate_btn), activate_btn_arrow);
  gtk_button_set_always_show_image (GTK_BUTTON (item->activate_btn), TRUE);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (item->activate_btn), FALSE);
  gtk_button_set_image_position (GTK_BUTTON (item->activate_btn), GTK_POS_RIGHT);
  gtk_style_context_add_class (gtk_widget_get_style_context (item->activate_btn), "layer-list__activate");
  gtk_widget_set_halign (gtk_bin_get_child (GTK_BIN (item->activate_btn)), GTK_ALIGN_END);
  gtk_widget_set_opacity (gtk_button_get_image (GTK_BUTTON (item->activate_btn)), 0.0);
  g_signal_connect_swapped (item->activate_btn, "notify::active", G_CALLBACK (hyscan_gtk_layer_list_toggle_active), item);

  /* Компановка кнопок. */
  btn_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_style_context_add_class (gtk_widget_get_style_context (btn_box), "layer-list__bbox");
  gtk_button_box_set_layout (GTK_BUTTON_BOX (btn_box), GTK_BUTTONBOX_EXPAND);
  gtk_box_set_homogeneous (GTK_BOX (btn_box), FALSE);
  if (item->vsbl_btn != NULL)
    gtk_box_pack_start (GTK_BOX (btn_box), item->vsbl_btn, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (btn_box), item->activate_btn, TRUE, TRUE, 0);

  /* Контейнер для инструментов слоя. */
  item->revealer = gtk_revealer_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (item->revealer), "layer-list__revealer");
  gtk_widget_set_margin_top (item->revealer, 1);
  gtk_widget_set_margin_bottom (item->revealer, 1);

  /* Собираем всё вместе. */
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (box), btn_box, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), item->revealer, FALSE, TRUE, 0);

  g_hash_table_replace (priv->list_items, item->name, item);
  gtk_box_pack_start (GTK_BOX (layer_list), box, FALSE, TRUE, 0);
}

/**
 * hyscan_gtk_layer_list_set_tools:
 * @layer_list: указатель на #HyScanGtkLayerList
 * @key: ключ пункта меню
 * @tools: виджет настроек слоя
 *
 * Устанавливает виджет настроек для пункта меню @key. Виджет настроек будет
 * отображаться при выборе указанного пункта.
 */
void
hyscan_gtk_layer_list_set_tools (HyScanGtkLayerList *layer_list,
                                 const gchar        *key,
                                 GtkWidget          *tools)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;
  HyScanGtkLayerListItem *item;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_LIST (layer_list));

  item = g_hash_table_lookup (priv->list_items, key);
  if (item == NULL)
    return;

  gtk_container_add (GTK_CONTAINER (item->revealer), tools);
  gtk_widget_set_opacity (gtk_button_get_image (GTK_BUTTON (item->activate_btn)), 1.0);
}

/**
 * hyscan_gtk_layer_list_set_tools:
 * @layer_list: указатель на #HyScanGtkLayerList
 * @key: ключ пункта меню
 * @visible_fn: функция для установки видимости слоя
 *
 * Устанавливает функцию включения видимости слоя для пункта меню @key. По умолчанию используется функция
 * hyscan_gtk_layer_set_visible(), которая управляет видимостью всего слоя.
 */
void
hyscan_gtk_layer_list_set_visible_fn (HyScanGtkLayerList               *layer_list,
                                      const gchar                      *key,
                                      hyscan_gtk_layer_list_visible_fn  visible_fn)
{
  HyScanGtkLayerListPrivate *priv = layer_list->priv;
  HyScanGtkLayerListItem *item;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_LIST (layer_list));
  g_return_if_fail (visible_fn != NULL);

  item = g_hash_table_lookup (priv->list_items, key);
  if (item == NULL)
    return;

  item->visible_fn = visible_fn;
}

/**
 * hyscan_gtk_layer_list_get_visible_ids:
 * @list: указатель на #HyScanGtkLayerList
 *
 * Returns: (transfer full): нуль-терминированный массив идентификаторов видимых слоёв.
 */
gchar **
hyscan_gtk_layer_list_get_visible_ids (HyScanGtkLayerList *list)
{
  HyScanGtkLayerListPrivate *priv;
  GHashTableIter iter;
  gchar *layer_key;
  HyScanGtkLayerListItem *item;
  GArray *array;

  g_return_val_if_fail (HYSCAN_IS_GTK_LAYER_LIST (list), NULL);
  priv = list->priv;

  array = g_array_new (TRUE, FALSE, sizeof (gchar *));

  g_hash_table_iter_init (&iter, priv->list_items);
  while (g_hash_table_iter_next (&iter, (gpointer *) &layer_key, (gpointer *) &item))
   {
     gchar *layer_key_dup;

     if (item->layer == NULL || !hyscan_gtk_layer_get_visible (item->layer))
       continue;

     layer_key_dup = g_strdup (layer_key);
     g_array_append_val (array, layer_key_dup);
   }

  return (gchar **) g_array_free (array, FALSE);
}

/**
 * hyscan_gtk_layer_list_set_visible_ids:
 * @list: указатель на #HyScanGtkLayerList
 * @ids: NULL-терминированный массив идентификаторов слоёв
 *
 * Включает видимость слоёв с ключами @ids и отключает остальные.
 */
void
hyscan_gtk_layer_list_set_visible_ids (HyScanGtkLayerList  *list,
                                       gchar              **ids)
{
  HyScanGtkLayerListPrivate *priv;
  GHashTableIter iter;
  gchar *layer_key;
  HyScanGtkLayerListItem *item;

  g_return_if_fail (HYSCAN_IS_GTK_LAYER_LIST (list));
  priv = list->priv;

  g_hash_table_iter_init (&iter, priv->list_items);
  while (g_hash_table_iter_next (&iter, (gpointer *) &layer_key, (gpointer *) &item))
   {
     gboolean visible;

     if (item->layer == NULL)
       continue;

     visible = g_strv_contains ((const gchar *const *) ids, layer_key);
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->vsbl_btn), visible);
   }
}

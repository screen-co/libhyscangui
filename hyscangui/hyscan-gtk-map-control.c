/* hyscan-gtk-map-control.c
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
 * SECTION: hyscan-gtk-map-control
 * @Short_description: Слой управления видимой областью карты
 * @Title: HyScanGtkMapControl
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Данный слой позволяет управлять видимой областью карты:
 * - перемещение по карте с помощью клавиатуры,
 * - изменение масштаба колесом мыши.
 *
 * Также слой географические координаты курсора мыши в буфер обмена по нажатию
 * Ctrl + Insert.
 *
 * Слой не выводит на карте никакого изображения.
 *
 */

#include "hyscan-gtk-map-control.h"
#include <math.h>
#include <glib/gi18n-lib.h>

#define MOVE_THRESHOLD        5   /* Сдвиг мыши, при котором активируется режим перемещения карты. */
#define KEYBOARD_MOVE_STEP    10  /* Число пикселей, на которое сдвигается карта при однократном нажатии стрелки. */

struct _HyScanGtkMapControlPrivate
{
  HyScanGtkMap                   *map;           /* Gtk-виджет карты. */

  /* Обработка перемещения карты мышью. */
  gint                            mode;          /* Состояние слоя. */
  gdouble                         cursor_x;      /* Текущие координаты мыши. */
  gdouble                         cursor_y;      /* Текущие координаты мыши. */
};

static void     hyscan_gtk_map_control_interface_init       (HyScanGtkLayerInterface    *iface);
static gboolean hyscan_gtk_map_control_key_press            (HyScanGtkMapControl        *control,
                                                             GdkEventKey                *event);
static gboolean hyscan_gtk_map_control_scroll               (HyScanGtkMapControl        *control,
                                                             GdkEventScroll             *event);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapControl, hyscan_gtk_map_control, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_control_interface_init))

static void
hyscan_gtk_map_control_class_init (HyScanGtkMapControlClass *klass)
{
}

static void
hyscan_gtk_map_control_init (HyScanGtkMapControl *gtk_map_control)
{
  gtk_map_control->priv = hyscan_gtk_map_control_get_instance_private (gtk_map_control);
}

/* Отключается от контейнера #HyScanGtkLayerContainer. */
void
hyscan_gtk_map_control_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapControl *gtk_map_control = HYSCAN_GTK_MAP_CONTROL (gtk_layer);
  HyScanGtkMapControlPrivate *priv = gtk_map_control->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gtk_layer);
  g_clear_object (&priv->map);
}

/* Регистрирует слой при добавлении в контейнер #HyScanGtkLayerContainer. */
void
hyscan_gtk_map_control_added (HyScanGtkLayer          *gtk_layer,
                              HyScanGtkLayerContainer *container)
{
  HyScanGtkMapControl *gtk_map_control = HYSCAN_GTK_MAP_CONTROL (gtk_layer);
  HyScanGtkMapControlPrivate *priv = gtk_map_control->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));

  g_signal_connect_swapped (priv->map, "scroll-event",
                            G_CALLBACK (hyscan_gtk_map_control_scroll), gtk_map_control);
  g_signal_connect_swapped (priv->map, "key-press-event",
                            G_CALLBACK (hyscan_gtk_map_control_key_press), gtk_map_control);
}

static void
hyscan_gtk_map_control_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_control_added;
  iface->removed = hyscan_gtk_map_control_removed;
}

/* Копирует координаты курсора в буфер обмена. */
static gboolean 
hyscan_gtk_map_control_press_copy (HyScanGtkMapControl *control,
                                   GdkEventKey         *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  HyScanGeoCartesian2D val;
  HyScanGeoGeodetic coords;

  GtkClipboard *clipboard;
  GtkWidget *dialog;

  gchar lat_text[16], lon_text[16], text[64];

  if (!(event->keyval == GDK_KEY_Insert && (event->state & GDK_CONTROL_MASK)))
    return GDK_EVENT_PROPAGATE;

  /* Копируем координаты точки. */
  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), priv->cursor_x, priv->cursor_y, &val.x, &val.y);
  hyscan_gtk_map_value_to_geo (priv->map, &coords, val);

  g_snprintf (lat_text, sizeof (lat_text), "%.8f", coords.lat);
  g_snprintf (lon_text, sizeof (lat_text), "%.8f", coords.lon);
  /* Меняем запятые на точки. */
  g_strcanon (lat_text, "-0123456789.", '.');
  g_strcanon (lon_text, "-0123456789.", '.');

  g_snprintf (text, sizeof (text), "%s, %s", lat_text, lon_text);

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, -1);

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (priv->map))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_CLOSE,
                                   _("Mouse coordinates copied to clipboard:\n\n%s"),
                                   text);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return GDK_EVENT_STOP;
}

/* Меняет масштаб при нажатии клавиш плюс и минус. */
static gboolean
hyscan_gtk_map_control_press_zoom (HyScanGtkMapControl *control,
                                   GdkEventKey         *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  gdouble from_x, to_x, from_y, to_y;
  GtkCifroAreaZoomType direction;
  
  switch (event->keyval)
  {
    case GDK_KEY_minus:
    case GDK_KEY_underscore:
    case GDK_KEY_KP_Subtract:
      direction = GTK_CIFRO_AREA_ZOOM_OUT;
      break;

    case GDK_KEY_plus:  
    case GDK_KEY_equal:  
    case GDK_KEY_KP_Add:
      direction = GTK_CIFRO_AREA_ZOOM_IN;
      break;

    default:
      return GDK_EVENT_PROPAGATE;
  }

  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_zoom (GTK_CIFRO_AREA (priv->map), direction, direction, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  return GDK_EVENT_STOP;
}

/* Смещает видимую областб стрелками. */
static gboolean
hyscan_gtk_map_control_press_move (HyScanGtkMapControl *control,
                                   GdkEventKey         *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  gint dx = 0, dy = 0;

  if (event->keyval == GDK_KEY_Up)
    dy = 1;
  else if (event->keyval == GDK_KEY_Down)
    dy = -1;
  else if (event->keyval == GDK_KEY_Right)
    dx = 1;
  else if (event->keyval == GDK_KEY_Left)
    dx = -1;
  else
    return GDK_EVENT_PROPAGATE;

  gtk_cifro_area_move (GTK_CIFRO_AREA (priv->map), KEYBOARD_MOVE_STEP * dx, KEYBOARD_MOVE_STEP * dy);

  return GDK_EVENT_STOP;
}

/* Обработчик сигнала "key-press-event". */
static gboolean
hyscan_gtk_map_control_key_press (HyScanGtkMapControl *control,
                                  GdkEventKey         *event)
{
  return hyscan_gtk_map_control_press_copy (control, event) ||
         hyscan_gtk_map_control_press_zoom (control, event) ||
         hyscan_gtk_map_control_press_move (control, event);
}

/* Обработчик событий прокрутки колёсика мышки "scroll-event". */
static gboolean
hyscan_gtk_map_control_scroll (HyScanGtkMapControl *control,
                               GdkEventScroll      *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  GtkCifroAreaZoomType direction;
  gdouble x, y;

  /* Поворот. */
  if (event->state & GDK_SHIFT_MASK)
    {
      if (event->direction == GDK_SCROLL_UP)
        gtk_cifro_area_rotate (GTK_CIFRO_AREA (priv->map), G_PI / 180);
      else if (event->direction == GDK_SCROLL_DOWN)
        gtk_cifro_area_rotate (GTK_CIFRO_AREA (priv->map), -G_PI / 180);

      return GDK_EVENT_STOP;
    }

  if (event->direction == GDK_SCROLL_UP)
    direction = GTK_CIFRO_AREA_ZOOM_IN;
  else if (event->direction == GDK_SCROLL_DOWN)
    direction = GTK_CIFRO_AREA_ZOOM_OUT;
  else
    return GDK_EVENT_PROPAGATE;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &x, &y);
  gtk_cifro_area_zoom (GTK_CIFRO_AREA (priv->map), direction, direction, x, y);

  return GDK_EVENT_STOP;
}

/**
 * hyscan_gtk_map_control_new:
 *
 * Создаёт новый слой управления картой #HyScanGtkMap. Для добавления слоя на
 * карту используйте hyscan_gtk_layer_container_add().
 *
 * Returns: новый объект #HyScanGtkMapControl. Для удаления g_object_unref()
 */
HyScanGtkLayer *
hyscan_gtk_map_control_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_CONTROL, NULL);
}

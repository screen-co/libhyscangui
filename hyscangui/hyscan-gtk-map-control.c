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

#include "hyscan-gtk-map-control.h"
#include <math.h>

#define MOVE_THRESHOLD 5 /* Сдвиг мыши, при котором активируется режим перемещения карты. */

enum
{
  PROP_O,
};

enum
{
  MODE_NONE,     /* Холостой режим. */
  MODE_AWAIT,    /* Режим ожидания движения, когда кнопка мыши уже зажата, но курсор ещё не сдвинут. */
  MODE_MOVE,     /* Режим перемещения, двигаем карту вслед за движением мыши. */
};

struct _HyScanGtkMapControlPrivate
{
  HyScanGtkMap                   *map;           /* Gtk-виджет карты. */

  /* Обработка перемещения карты мышью. */
  gint                            mode;
  gdouble                         start_from_x;  /* Начальная граница отображения по оси X. */
  gdouble                         start_to_x;    /* Начальная граница отображения по оси X. */
  gdouble                         start_from_y;  /* Начальная граница отображения по оси Y. */
  gdouble                         start_to_y;    /* Начальная граница отображения по оси Y. */
  gint                            move_from_x;   /* Начальная координата перемещения. */
  gint                            move_from_y;   /* Начальная координата перемещения. */
};

static void     hyscan_gtk_map_control_interface_init       (HyScanGtkLayerInterface    *iface);
static gboolean hyscan_gtk_map_control_button_press_release (HyScanGtkMapControl        *control,
                                                             GdkEventButton             *event);
static gboolean hyscan_gtk_map_control_motion_notify        (HyScanGtkMapControl        *control,
                                                             GdkEventMotion             *event);
static gboolean hyscan_gtk_map_control_scroll               (HyScanGtkMapControl        *control,
                                                             GdkEventScroll             *event);
static void     hyscan_gtk_map_control_set_mode             (HyScanGtkMapControl        *control,
                                                             gint                        mode);

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

  priv->map = g_object_ref (container);

  /* Обработчики взаимодействия мышки пользователя с картой. */
  g_signal_connect_swapped (priv->map, "button-press-event",    G_CALLBACK (hyscan_gtk_map_control_button_press_release), gtk_map_control);
  g_signal_connect_swapped (priv->map, "button-release-event",  G_CALLBACK (hyscan_gtk_map_control_button_press_release), gtk_map_control);
  g_signal_connect_swapped (priv->map, "motion-notify-event",   G_CALLBACK (hyscan_gtk_map_control_motion_notify), gtk_map_control);
  g_signal_connect_swapped (priv->map, "scroll-event",          G_CALLBACK (hyscan_gtk_map_control_scroll), gtk_map_control);
}

static void
hyscan_gtk_map_control_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_control_added;
  iface->removed = hyscan_gtk_map_control_removed;
}

/* Обработчик событий прокрутки колёсика мышки "scroll-event". */
static gboolean
hyscan_gtk_map_control_scroll (HyScanGtkMapControl *control,
                               GdkEventScroll      *event)
{
  HyScanGtkMap *map = control->priv->map;

  gint dscale;
  gint scale_idx;

  gdouble center_x, center_y;

  if (event->direction == GDK_SCROLL_UP)
    dscale = -1;
  else if (event->direction == GDK_SCROLL_DOWN)
    dscale = 1;
  else
    return FALSE;

  scale_idx = hyscan_gtk_map_get_scale_idx (map, NULL);
  scale_idx += dscale;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (map), event->x, event->y, &center_x, &center_y);
  hyscan_gtk_map_set_scale_idx (map, scale_idx < 0 ? 0 : scale_idx, center_x, center_y);

  return FALSE;
}

/* Установка нового режима работы слоя управления. */
static void
hyscan_gtk_map_control_set_mode (HyScanGtkMapControl *control,
                                 gint                 mode)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  GdkWindow *window;
  GdkCursor *cursor = NULL;

  priv->mode = mode;

  window = gtk_widget_get_window (GTK_WIDGET (priv->map));
  if (priv->mode == MODE_MOVE)
    {
      GdkDisplay *display;
      gconstpointer howner;

      display = gdk_window_get_display (window);
      cursor = gdk_cursor_new_from_name (display, "grabbing");

      /* Если никакой хэндл не взят, то сами его берём, а вернём на отжатие мыши. */
      howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
      if (howner == NULL)
        hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), control);
    }

  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}

/* Обработчик перемещений курсора мыши. */
static gboolean
hyscan_gtk_map_control_motion_notify (HyScanGtkMapControl *control,
                                      GdkEventMotion      *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (priv->map);
  GtkCifroArea *carea = GTK_CIFRO_AREA (map);

  /* Если после нажатия кнопки сдвинулись больше 3 пикселей - начинаем сдвиг. */
  if (priv->mode == MODE_AWAIT)
    {
      if (fabs (priv->move_from_x - event->x) + fabs (priv->move_from_y - event->y) > MOVE_THRESHOLD)
        {
          hyscan_gtk_map_control_set_mode (control, MODE_MOVE);

          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  /* Режим перемещения - сдвигаем область. */
  if (priv->mode == MODE_MOVE)
    {
      gdouble x0, y0;
      gdouble xd, yd;
      gdouble dx, dy;

      gtk_cifro_area_point_to_value (carea, priv->move_from_x, priv->move_from_y, &x0, &y0);
      gtk_cifro_area_point_to_value (carea, event->x, event->y, &xd, &yd);
      dx = x0 - xd;
      dy = y0 - yd;

      gtk_cifro_area_set_view (carea, priv->start_from_x + dx, priv->start_to_x + dx,
                                      priv->start_from_y + dy, priv->start_to_y + dy);

      gdk_event_request_motions (event);
    }

  return FALSE;
}

/* Обработчик нажатия кнопок мышки: "button-release-event", "button-press-event". */
static gboolean
hyscan_gtk_map_control_button_press_release (HyScanGtkMapControl *control,
                                             GdkEventButton      *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (priv->map);
  GtkCifroArea *carea = GTK_CIFRO_AREA (map);

  /* Обрабатываем только нажатия левой клавишей мыши. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  /* Нажата левая клавиша мышки в видимой области - переходим в режим ожидания. */
  if (event->type == GDK_BUTTON_PRESS)
    {
      guint widget_width, widget_height;
      guint border_top, border_bottom;
      guint border_left, border_right;
      gint clip_width, clip_height;

      gtk_cifro_area_get_size (carea, &widget_width, &widget_height);
      gtk_cifro_area_get_border (carea, &border_top, &border_bottom, &border_left, &border_right);

      clip_width = widget_width - border_left - border_right;
      clip_height = widget_height - border_top - border_bottom;
      if ((clip_width <= 0) || (clip_height <= 0))
        return GDK_EVENT_PROPAGATE;

      if ((event->x > border_left) && (event->x < (border_left + clip_width)) &&
          (event->y > border_top) && (event->y < (border_top + clip_height)))
        {
          hyscan_gtk_map_control_set_mode (control, MODE_AWAIT);
          priv->move_from_x = (gint) event->x;
          priv->move_from_y = (gint) event->y;
        }
    }

  /* Клавиша мышки отжата - завершаем перемещение. */
  if (event->type == GDK_BUTTON_RELEASE)
    {
      gboolean stop_propagation = GDK_EVENT_PROPAGATE;

      if (priv->mode == MODE_MOVE) /* priv->mode != MODE_AWAIT */
        {
          gconstpointer howner;

          /* Отпускаем хэндл, если он был захвачен нами. */
          howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
          if (howner == control)
            hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);

          stop_propagation = GDK_EVENT_STOP;
        }

      hyscan_gtk_map_control_set_mode (control, MODE_NONE);

      return stop_propagation;
    }

  return GDK_EVENT_PROPAGATE;
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

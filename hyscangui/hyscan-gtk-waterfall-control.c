/* hyscan-gtk-waterfall-control.c
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-gtk-waterfall-control
 * @Title HyScanGtkWaterfallControl
 * @Short_description Управление видимой областью водопада
 *
 * Слой HyScanGtkWaterfallControl предназначен для управления видимой областью.
 * Это включает в себя зуммирование и перемещение.
 *
 * Класс умеет обрабатывать движения мыши, прокрутку колеса и нажатия кнопок
 * клавиатуры: стрелки, +/-, PgUp, PgDown, Home, End.
 * Если пользователь переместит указатель мыши на 10% от размеров окна к началу
 * галса с зажатой левой кнопкой, будет отключена автосдвижка.
 */

#include "hyscan-gtk-waterfall-control.h"

#define GTK_WATERFALL_CONTROL_INPUT_ID ((gpointer)NULL)

#define IS_ARROW(key) (key == GDK_KEY_Left || key == GDK_KEY_Right ||\
                       key == GDK_KEY_Up || key == GDK_KEY_Down)
#define IS_PGHE_KEYS(key) (key == GDK_KEY_Page_Down || key == GDK_KEY_Page_Up ||\
                           key == GDK_KEY_End || key == GDK_KEY_Home)
#define IS_PLUS_MINUS(key) (key == GDK_KEY_minus || key == GDK_KEY_equal ||\
                            key == GDK_KEY_KP_Add || key == GDK_KEY_KP_Subtract ||\
                            key == GDK_KEY_underscore || key == GDK_KEY_plus)
#define IS_TO_START_KEY(key,display_type) ((display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN &&\
                                            (key == GDK_KEY_Down || key == GDK_KEY_Page_Down || key == GDK_KEY_Home))\
                                               ||\
                                           (display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER &&\
                                            (key == GDK_KEY_Left || key == GDK_KEY_Page_Up || key == GDK_KEY_Home)))
#define IS_TO_START_SCROLL(dir,display_type) ((display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN &&\
                                               dir == GDK_SCROLL_DOWN)\
                                               ||\
                                              (display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER &&\
                                               dir == GDK_SCROLL_UP))

enum
{
  MOVE_NONE,
  MOVE_START,
  MOVE_DONE
};

struct _HyScanGtkWaterfallControlPrivate
{
  HyScanGtkWaterfall        *wfall;          /* Водопад. */
  HyScanWaterfallDisplayType display_type;   /* Тип отображения. */

  guint     width;               /* Ширина виджета. */
  guint     height;              /* Высота виджета. */
  gboolean  move_area;           /* Признак перемещения при нажатой клавише мыши. */
  gdouble   move_from_y;         /* Y-координата мыши в момент нажатия кнопки.    */
  gdouble   move_from_x;         /* X-координата мыши в момент нажатия кнопки.    */
  gdouble   start_from_x;        /* Начальная граница отображения по оси X.       */
  gdouble   start_to_x;          /* Начальная граница отображения по оси X.       */
  gdouble   start_from_y;        /* Начальная граница отображения по оси Y.       */
  gdouble   start_to_y;          /* Начальная граница отображения по оси Y.       */
  gboolean  scroll_without_ctrl; /* Режим прокрутки. */
};

static void         hyscan_gtk_waterfall_control_interface_init  (HyScanGtkLayerInterface   *iface);
static void         hyscan_gtk_waterfall_control_object_finalize (GObject                   *object);

static void         hyscan_gtk_waterfall_control_added           (HyScanGtkLayer            *layer,
                                                                  HyScanGtkLayerContainer   *container);
static void         hyscan_gtk_waterfall_control_removed         (HyScanGtkLayer            *layer);
static gboolean     hyscan_gtk_waterfall_control_grab_input      (HyScanGtkLayer            *layer);
static const gchar* hyscan_gtk_waterfall_control_get_icon_name    (HyScanGtkLayer            *layer);

static gboolean     hyscan_gtk_waterfall_control_configure       (GtkWidget                 *widget,
                                                                  GdkEventConfigure         *event,
                                                                  HyScanGtkWaterfallControl *control);
static gboolean     hyscan_gtk_waterfall_control_mouse_button    (GtkWidget                 *widget,
                                                                  GdkEventButton            *event,
                                                                  HyScanGtkWaterfallControl *data);
static gboolean     hyscan_gtk_waterfall_control_mouse_motion    (GtkWidget                 *widget,
                                                                  GdkEventMotion            *event,
                                                                  HyScanGtkWaterfallControl *data);
static gboolean     hyscan_gtk_waterfall_control_keyboard        (GtkWidget                 *widget,
                                                                  GdkEventKey               *event,
                                                                  HyScanGtkWaterfallControl *data);
static gboolean     hyscan_gtk_waterfall_control_mouse_wheel     (GtkWidget                 *widget,
                                                                  GdkEventScroll            *event,
                                                                  HyScanGtkWaterfallControl *data);
static void         hyscan_gtk_waterfall_control_sources_changed (HyScanGtkWaterfallState   *model,
                                                                  HyScanGtkWaterfallControl *control);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallControl, hyscan_gtk_waterfall_control, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkWaterfallControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_waterfall_control_interface_init));

static void
hyscan_gtk_waterfall_control_class_init (HyScanGtkWaterfallControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_gtk_waterfall_control_object_finalize;
}

static void
hyscan_gtk_waterfall_control_init (HyScanGtkWaterfallControl *self)
{
  self->priv = hyscan_gtk_waterfall_control_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_control_object_finalize (GObject *object)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (object);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  if (priv->wfall != NULL)
    g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_clear_object (&priv->wfall);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_control_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_control_added (HyScanGtkLayer          *layer,
                                    HyScanGtkLayerContainer *container)
{
  HyScanGtkWaterfall *wfall;
  HyScanGtkWaterfallControl *self;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (container));

  self = HYSCAN_GTK_WATERFALL_CONTROL (layer);
  wfall = HYSCAN_GTK_WATERFALL (container);
  self->priv->wfall = g_object_ref (wfall);

  /* Сигналы Gtk. */
  g_signal_connect_after (wfall, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_control_configure), self);
  g_signal_connect (wfall, "button-press-event",    G_CALLBACK (hyscan_gtk_waterfall_control_mouse_button), self);
  g_signal_connect (wfall, "button-release-event",  G_CALLBACK (hyscan_gtk_waterfall_control_mouse_button), self);
  g_signal_connect (wfall, "motion-notify-event",   G_CALLBACK (hyscan_gtk_waterfall_control_mouse_motion), self);
  g_signal_connect (wfall, "scroll-event",          G_CALLBACK (hyscan_gtk_waterfall_control_mouse_wheel), self);
  g_signal_connect (wfall, "key-press-event",       G_CALLBACK (hyscan_gtk_waterfall_control_keyboard), self);

  /* Сигналы модели. */
  g_signal_connect (wfall, "changed::sources", G_CALLBACK (hyscan_gtk_waterfall_control_sources_changed), self);

  /* Инициализация. */
  hyscan_gtk_waterfall_control_sources_changed (HYSCAN_GTK_WATERFALL_STATE (wfall), self);
}

static void
hyscan_gtk_waterfall_control_removed (HyScanGtkLayer *layer)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (layer);

  g_signal_handlers_disconnect_by_data (self->priv->wfall, self);
  g_clear_object (&self->priv->wfall);
}

/* Функция захыватывает ввод. */
static gboolean
hyscan_gtk_waterfall_control_grab_input (HyScanGtkLayer *iface)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (iface);

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (self->priv->wfall), GTK_WATERFALL_CONTROL_INPUT_ID);

  return TRUE;
}

/* Функция возвращает название иконки. */
static const gchar*
hyscan_gtk_waterfall_control_get_icon_name (HyScanGtkLayer *iface)
{
  return "find-location-symbolic";
}

static gboolean
hyscan_gtk_waterfall_control_configure (GtkWidget                 *widget,
                                        GdkEventConfigure         *event,
                                        HyScanGtkWaterfallControl *self)
{
  gtk_cifro_area_get_size (GTK_CIFRO_AREA (widget),
                           &self->priv->width,
                           &self->priv->height);

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик клавиш клавиатуры. */
static gboolean
hyscan_gtk_waterfall_control_keyboard (GtkWidget   *widget,
                                       GdkEventKey *event,
                                       HyScanGtkWaterfallControl *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;
  guint key = event->keyval;

  /* Проверяем, есть ли у нас право обработки ввода. */
  // if (GTK_WATERFALL_CONTROL_INPUT_ID != hyscan_gtk_waterfall_get_input_owner (HYSCAN_GTK_WATERFALL (widget)))
    // return FALSE;

  gboolean arrow_keys = FALSE;
  gboolean plus_minus_keys = FALSE;
  gboolean pg_home_end_keys = FALSE;

  /* Стрелки (движение). */
  arrow_keys = IS_ARROW (key);
  plus_minus_keys = IS_PLUS_MINUS (key);
  pg_home_end_keys = IS_PGHE_KEYS (key);

  if (arrow_keys)
    {
      gint step_x = -1 * (key == GDK_KEY_Left) + 1 * (key == GDK_KEY_Right);
      gint step_y = -1 * (key == GDK_KEY_Down) + 1 * (key == GDK_KEY_Up);

      if (event->state & GDK_CONTROL_MASK)
        {
          step_x *= 10.0;
          step_y *= 10.0;
        }

      gtk_cifro_area_move (carea, step_x, step_y);
    }
  else if (pg_home_end_keys)
    {
      gint step_echo = 0;
      gint step_ss = 0;
      guint width, height = 0;

      /* В режимах ГБО и эхолот эти кнопки по-разному реагируют. */
      gtk_cifro_area_get_size (carea, &width, &height);

      switch (key)
        {
        case GDK_KEY_Page_Up:
          step_echo = -0.75 * width;
          step_ss = 0.75 * height;
          break;
        case GDK_KEY_Page_Down:
          step_echo = 0.75 * width;
          step_ss = -0.75 * height;
          break;
        case GDK_KEY_End:
          step_echo = step_ss = G_MAXINT;
          break;
        case GDK_KEY_Home:
          step_echo = step_ss = G_MININT;
          break;
        }

      if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)
        gtk_cifro_area_move (carea, step_echo, 0);
      else /* if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN) */
        gtk_cifro_area_move (carea, 0, step_ss);
    }
  else if (plus_minus_keys)
    {
      GtkCifroAreaZoomType direction;
      gdouble from_x, to_x, from_y, to_y;

      if (key == GDK_KEY_KP_Add || key == GDK_KEY_equal || key == GDK_KEY_plus)
        direction = GTK_CIFRO_AREA_ZOOM_IN;
      else
        direction = GTK_CIFRO_AREA_ZOOM_OUT;

      gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
      gtk_cifro_area_zoom (carea, direction, direction, (to_x + from_x) / 2.0, (to_y + from_y) / 2.0);
    }

  if (IS_TO_START_KEY (key, priv->display_type))
    hyscan_gtk_waterfall_automove (priv->wfall, FALSE);

  return GDK_EVENT_STOP;
}

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_waterfall_control_mouse_button (GtkWidget                 *widget,
                                           GdkEventButton            *event,
                                           HyScanGtkWaterfallControl *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;

  gtk_widget_grab_focus (widget);

  if (event->button != 1)
    return GDK_EVENT_PROPAGATE;

  if (event->type == GDK_BUTTON_PRESS)
    {
      guint width, height;
      guint b_top, b_bottom, b_left, b_right;

      gtk_cifro_area_get_size (carea, &width, &height);
      gtk_cifro_area_get_border (carea, &b_top, &b_bottom, &b_left, &b_right);

      if (event->x > b_left && event->x < width - b_right &&
          event->y > b_top && event->y < height - b_bottom)
        {
          priv->move_from_x = event->x;
          priv->move_from_y = event->y;
          priv->move_area = MOVE_START;
          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  else if (event->type == GDK_BUTTON_RELEASE)
    {
      if (priv->move_area == MOVE_START)
        {
          priv->move_area = MOVE_NONE;
          return GDK_EVENT_PROPAGATE;
        }
      if (priv->move_area == MOVE_DONE)
        {
          priv->move_area = MOVE_NONE;
          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_control_mouse_motion (GtkWidget                 *widget,
                                           GdkEventMotion            *event,
                                           HyScanGtkWaterfallControl *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;
  gdouble x0, y0, x1, y1, dx, dy;
  gdouble stored, received;
  guint widget_size;

  /* Режим перемещения - сдвигаем область. */
  if (priv->move_area == MOVE_NONE)
    return GDK_EVENT_PROPAGATE;

  priv->move_area = MOVE_DONE;

  gtk_cifro_area_point_to_value (carea, priv->move_from_x, priv->move_from_y, &x0, &y0);
  gtk_cifro_area_point_to_value (carea, event->x, event->y, &x1, &y1);
  dx = x0 - x1;
  dy = y0 - y1;

  gtk_cifro_area_set_view (carea, priv->start_from_x + dx, priv->start_to_x + dx,
                                  priv->start_from_y + dy, priv->start_to_y + dy);

  gdk_event_request_motions (event);

  if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
    {
      stored = priv->move_from_y;
      received = event->y;
      widget_size = priv->height;
    }
  else /* if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER) */
    {
      stored = event->x;
      received = priv->move_from_x;
      widget_size = priv->width;
    }

  if (received < stored - widget_size / 10.0)
    hyscan_gtk_waterfall_automove (priv->wfall, FALSE);

  return GDK_EVENT_PROPAGATE;
}

/* Обработчик колесика мыши. */
static gboolean
hyscan_gtk_waterfall_control_mouse_wheel (GtkWidget                 *widget,
                                          GdkEventScroll            *event,
                                          HyScanGtkWaterfallControl *self)
{
  HyScanGtkWaterfallControlPrivate *priv = self->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  guint width, height;
  gboolean do_scroll;

  /* Не проверяем, есть ли у нас право обработки ввода.
   * У control оно есть всегда. */

  gtk_cifro_area_get_size (carea, &width, &height);
  do_scroll = priv->scroll_without_ctrl == !(event->state & GDK_CONTROL_MASK);

  /* Зуммирование. */
  if (!do_scroll)
    {
      gdouble from_x, to_x, from_y, to_y;
      gdouble zoom_x, zoom_y;
      GtkCifroAreaZoomType direction;

      gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

      direction = (event->direction == GDK_SCROLL_UP) ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

      zoom_x = from_x + (to_x - from_x) * (event->x / (gdouble)width);
      zoom_y = from_y + (to_y - from_y) * (1.0 - event->y / height);

      gtk_cifro_area_zoom (carea, direction, direction, zoom_x, zoom_y);
    }
  /* Прокрутка. */
  else
    {
      gint step = height / 10;
      step *= (event->direction == GDK_SCROLL_UP) ? 1 : -1;

      if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN ||
          (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER &&
          (event->state & GDK_SHIFT_MASK)))
        {
          gtk_cifro_area_move (carea, 0, step);
        }
      else if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER ||
               (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN &&
               (event->state & GDK_SHIFT_MASK)))
        {
          gtk_cifro_area_move (carea, -step, 0);
        }

      if (IS_TO_START_SCROLL (event->direction, priv->display_type))
        hyscan_gtk_waterfall_automove (priv->wfall, FALSE);
    }

  return GDK_EVENT_PROPAGATE;
}

/* Функция обрабатывает сигнал смены типа отображения. */
static void
hyscan_gtk_waterfall_control_sources_changed (HyScanGtkWaterfallState   *model,
                                              HyScanGtkWaterfallControl *self)
{
  self->priv->display_type = hyscan_gtk_waterfall_state_get_sources (model, NULL, NULL);
}

/**
 * hyscan_gtk_waterfall_control_new:
 *
 * Функция создает новый объект.
 *
 * Returns: (transfer full): #HyScanGtkWaterfallControl.
 */
HyScanGtkWaterfallControl*
hyscan_gtk_waterfall_control_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_CONTROL, NULL);
}

/**
 * hyscan_gtk_waterfall_control_set_wheel_behaviour:
 * @control: указатель на объект #HyScanGtkWaterfallControl;
 * @scroll_without_ctrl: %TRUE, чтобы колесо мыши отвечало за прокрутку, %FALSE,
 *  чтобы колесо мыши отвечало за зуммирование.
 *
 * Функция задает поведение колесика мыши.
 * Колесико мыши может быть настроено на прокрутку или зуммирование.
 * Альтернативное действие будет выполняться с нажатой клавишей Ctrl.
 */
void
hyscan_gtk_waterfall_control_set_wheel_behaviour (HyScanGtkWaterfallControl *control,
                                                  gboolean                   scroll_without_ctrl)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_CONTROL (control));
  control->priv->scroll_without_ctrl = scroll_without_ctrl;
}

/**
 * hyscan_gtk_waterfall_control_zoom:
 * @wfall: указатель на объект #HyScanGtkWaterfall;
 * @zoom_in: направление масштабирования (%TRUE - увеличение, %FALSE - уменьшение).
 *
 * Функция масштабирует изображение относительно центра.
 */
void
hyscan_gtk_waterfall_control_zoom (HyScanGtkWaterfallControl *control,
                                   gboolean                   zoom_in)
{
  GtkCifroArea *carea;
  GtkCifroAreaZoomType dir;
  gdouble x0, x1, y0, y1;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_CONTROL (control));
  carea = GTK_CIFRO_AREA (control->priv->wfall);

  dir = zoom_in ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

  gtk_cifro_area_get_view (carea, &x0, &x1, &y0, &y1);
  gtk_cifro_area_zoom (carea, dir, dir, (x0 + x1) / 2.0, (y0 + y1) / 2.0);
}

static void
hyscan_gtk_waterfall_control_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_control_grab_input;
  iface->get_icon_name = hyscan_gtk_waterfall_control_get_icon_name;
  iface->added = hyscan_gtk_waterfall_control_added;
  iface->removed = hyscan_gtk_waterfall_control_removed;
}

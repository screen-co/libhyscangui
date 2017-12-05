/*
 * \file hyscan-gtk-waterfall-control.c
 *
 * \brief Исходный файл класса управления виджетом водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
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
  PROP_WATERFALL = 1
};

struct _HyScanGtkWaterfallControlPrivate
{
  HyScanGtkWaterfallState   *wf_state;
  HyScanGtkWaterfall        *wfall;

  HyScanWaterfallDisplayType display_type;

  guint                  width;
  guint                  height;

  gboolean               move_area;          /* Признак перемещения при нажатой клавише мыши. */
  gdouble                move_from_y;        /* Y-координата мыши в момент нажатия кнопки.    */
  gdouble                move_from_x;        /* X-координата мыши в момент нажатия кнопки.    */
  gdouble                start_from_x;       /* Начальная граница отображения по оси X.       */
  gdouble                start_to_x;         /* Начальная граница отображения по оси X.       */
  gdouble                start_from_y;       /* Начальная граница отображения по оси Y.       */
  gdouble                start_to_y;         /* Начальная граница отображения по оси Y.       */
  gboolean               scroll_without_ctrl;

};

static void     hyscan_gtk_waterfall_control_interface_init          (HyScanGtkWaterfallLayerInterface *iface);
static void     hyscan_gtk_waterfall_control_set_property            (GObject                  *object,
                                                                      guint                     prop_id,
                                                                      const GValue             *value,
                                                                      GParamSpec               *pspec);
static void     hyscan_gtk_waterfall_control_object_constructed      (GObject                  *object);
static void     hyscan_gtk_waterfall_control_object_finalize         (GObject                  *object);

static void     hyscan_gtk_waterfall_control_grab_input              (HyScanGtkWaterfallLayer  *layer);
static const gchar* hyscan_gtk_waterfall_control_get_mnemonic        (HyScanGtkWaterfallLayer  *layer);

static gboolean hyscan_gtk_waterfall_control_configure               (GtkWidget                 *widget,
                                                                      GdkEventConfigure         *event,
                                                                      HyScanGtkWaterfallControl *control);
static gboolean hyscan_gtk_waterfall_control_mouse_button            (GtkWidget                 *widget,
                                                                      GdkEventButton            *event,
                                                                      HyScanGtkWaterfallControl *data);
static gboolean hyscan_gtk_waterfall_control_mouse_motion            (GtkWidget                 *widget,
                                                                      GdkEventMotion            *event,
                                                                      HyScanGtkWaterfallControl *data);
static gboolean hyscan_gtk_waterfall_control_keyboard                (GtkWidget                 *widget,
                                                                      GdkEventKey               *event,
                                                                      HyScanGtkWaterfallControl *data);
static gboolean hyscan_gtk_waterfall_control_mouse_wheel             (GtkWidget                 *widget,
                                                                      GdkEventScroll            *event,
                                                                      HyScanGtkWaterfallControl *data);
static void     hyscan_gtk_waterfall_control_sources_changed         (HyScanGtkWaterfallState      *model,
                                                                      HyScanGtkWaterfallControl *control);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkWaterfallControl, hyscan_gtk_waterfall_control, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkWaterfallControl)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_WATERFALL_LAYER,
                                                hyscan_gtk_waterfall_control_interface_init));

static void
hyscan_gtk_waterfall_control_class_init (HyScanGtkWaterfallControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_waterfall_control_set_property;

  object_class->constructed = hyscan_gtk_waterfall_control_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_control_object_finalize;

  g_object_class_install_property (object_class, PROP_WATERFALL,
    g_param_spec_object ("waterfall", "Waterfall", "GtkWaterfall object",
                         HYSCAN_TYPE_GTK_WATERFALL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_control_init (HyScanGtkWaterfallControl *self)
{
  self->priv = hyscan_gtk_waterfall_control_get_instance_private (self);
}


static void
hyscan_gtk_waterfall_control_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (object);

  if (prop_id == PROP_WATERFALL)
    {
      self->priv->wfall = g_value_dup_object (value);
      self->priv->wf_state = HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall);
    }
  else
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
hyscan_gtk_waterfall_control_object_constructed (GObject *object)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (object);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_control_parent_class)->constructed (object);

  /* Сигналы Gtk. */
  g_signal_connect_after (priv->wfall, "configure-event",
                          G_CALLBACK (hyscan_gtk_waterfall_control_configure), self);
  g_signal_connect (priv->wfall, "button-press-event", G_CALLBACK (hyscan_gtk_waterfall_control_mouse_button), self);
  g_signal_connect (priv->wfall, "button-release-event", G_CALLBACK (hyscan_gtk_waterfall_control_mouse_button), self);
  g_signal_connect (priv->wfall, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_control_mouse_motion), self);
  g_signal_connect (priv->wfall, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_control_mouse_wheel), self);
  g_signal_connect (priv->wfall, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_control_keyboard), self);

  /* Сигналы модели. */
  g_signal_connect (priv->wfall, "changed::sources", G_CALLBACK (hyscan_gtk_waterfall_control_sources_changed), self);

  hyscan_gtk_waterfall_control_sources_changed (priv->wf_state, self);
}


static void
hyscan_gtk_waterfall_control_object_finalize (GObject *object)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (object);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->wfall, self);
  g_signal_handlers_disconnect_by_data (priv->wfall, self);

  g_clear_object (&priv->wfall);
  g_clear_object (&priv->wfall);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_control_parent_class)->finalize (object);
}

static void
hyscan_gtk_waterfall_control_grab_input (HyScanGtkWaterfallLayer *iface)
{
  HyScanGtkWaterfallControl *self = HYSCAN_GTK_WATERFALL_CONTROL (iface);

  hyscan_gtk_waterfall_state_set_input_owner (HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall), GTK_WATERFALL_CONTROL_INPUT_ID);
  hyscan_gtk_waterfall_state_set_changes_allowed (HYSCAN_GTK_WATERFALL_STATE (self->priv->wfall), FALSE);
}

static const gchar*
hyscan_gtk_waterfall_control_get_mnemonic (HyScanGtkWaterfallLayer *iface)
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

  return FALSE;
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

  return FALSE;
}

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_waterfall_control_mouse_button (GtkWidget      *widget,
                                            GdkEventButton *event,
                                            HyScanGtkWaterfallControl *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;

  /* Проверяем, есть ли у нас право обработки ввода. */
  // if (GTK_WATERFALL_CONTROL_INPUT_ID != hyscan_gtk_waterfall_get_input_owner (HYSCAN_GTK_WATERFALL (widget)))
    // return FALSE;

  if (event->button != 1)
    return FALSE;

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
          priv->move_area = TRUE;
          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  else if (event->type == GDK_BUTTON_RELEASE)
    {
      priv->move_area = FALSE;
      gtk_widget_grab_focus (widget);
    }

  return FALSE;
}

/* Обработчик движения мыши. */
static gboolean
hyscan_gtk_waterfall_control_mouse_motion (GtkWidget      *widget,
                             GdkEventMotion *event,
                             HyScanGtkWaterfallControl *self)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfallControlPrivate *priv = self->priv;
  gdouble x0, y0, x1, y1, dx, dy;
  gdouble stored, received;
  guint widget_size;

  /* Проверяем, есть ли у нас право обработки ввода. */
  // if (GTK_WATERFALL_CONTROL_INPUT_ID != hyscan_gtk_waterfall_get_input_owner (HYSCAN_GTK_WATERFALL (widget)))
    // return FALSE;

  /* Режим перемещения - сдвигаем область. */
  if (!priv->move_area)
    return FALSE;

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

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_control_mouse_wheel (GtkWidget                 *widget,
                                          GdkEventScroll            *event,
                                          HyScanGtkWaterfallControl *self)
{
  HyScanGtkWaterfallControlPrivate *priv = self->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  guint width, height;
  gboolean do_scroll;

  /* Проверяем, есть ли у нас право обработки ввода. */
  // if (GTK_WATERFALL_CONTROL_INPUT_ID != hyscan_gtk_waterfall_get_input_owner (HYSCAN_GTK_WATERFALL (widget)))
    // return FALSE;

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

      if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_SIDESCAN)
        gtk_cifro_area_move (carea, 0, step);
      else /*if (priv->display_type == HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER)*/
        gtk_cifro_area_move (carea, -step, 0);

      if (IS_TO_START_SCROLL (event->direction, priv->display_type))
        hyscan_gtk_waterfall_automove (priv->wfall, FALSE);
    }

  return FALSE;
}

static void
hyscan_gtk_waterfall_control_sources_changed (HyScanGtkWaterfallState   *model,
                                              HyScanGtkWaterfallControl *self)
{
  hyscan_gtk_waterfall_state_get_sources (model, &self->priv->display_type, NULL, NULL);
}

HyScanGtkWaterfallControl*
hyscan_gtk_waterfall_control_new (HyScanGtkWaterfall *waterfall)
{
  return g_object_new (HYSCAN_TYPE_GTK_WATERFALL_CONTROL,
                       "waterfall", waterfall,
                       NULL);
}

void
hyscan_gtk_waterfall_control_set_wheel_behaviour (HyScanGtkWaterfallControl *self,
                                                  gboolean                   scroll_without_ctrl)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_CONTROL (self));
  self->priv->scroll_without_ctrl = scroll_without_ctrl;
}

void
hyscan_gtk_waterfall_control_zoom (HyScanGtkWaterfallControl *self,
                                   gboolean            zoom_in)
{
  GtkCifroArea *carea;
  GtkCifroAreaZoomType dir;
  gdouble x0, x1, y0, y1;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_CONTROL (self));
  carea = GTK_CIFRO_AREA (self->priv->wfall);

  dir = zoom_in ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

  gtk_cifro_area_get_view (carea, &x0, &x1, &y0, &y1);
  gtk_cifro_area_zoom (carea, dir, dir, (x0 + x1) / 2.0, (y0 + y1) / 2.0);
}

static void
hyscan_gtk_waterfall_control_interface_init (HyScanGtkWaterfallLayerInterface *iface)
{
  iface->grab_input = hyscan_gtk_waterfall_control_grab_input;
  iface->get_mnemonic = hyscan_gtk_waterfall_control_get_mnemonic;
}

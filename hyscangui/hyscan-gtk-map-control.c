#include "hyscan-gtk-map-control.h"
#include <gdk/gdk.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP
};

enum
{
  STATE_IDLE,
  STATE_AWAIT,
  STATE_MOVE,
};

struct _HyScanGtkMapControlPrivate
{
  HyScanGtkMap *                  map;           /* Gtk-виджет карты. */

  /* Обработка перемещения карты мышью. */
  gint                     move_state;
  gdouble                  start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                  start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                  start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                  start_to_y;           /* Начальная граница отображения по оси Y. */
  gint                     move_from_x;          /* Начальная координата перемещения. */
  gint                     move_from_y;          /* Начальная координата перемещения. */
};

static void     hyscan_gtk_map_control_set_property         (GObject             *object,
                                                             guint                prop_id,
                                                             const GValue        *value,
                                                             GParamSpec          *pspec);
static void     hyscan_gtk_map_control_object_constructed   (GObject             *object);
static void     hyscan_gtk_map_control_object_finalize      (GObject             *object);
static gboolean hyscan_gtk_map_control_button_press_release (HyScanGtkMapControl *control,
                                                             GdkEventButton      *event);
static gboolean hyscan_gtk_map_control_motion               (HyScanGtkMapControl *control,
                                                             GdkEventMotion      *event);
static gboolean hyscan_gtk_map_control_scroll               (HyScanGtkMapControl *control,
                                                             GdkEventScroll      *event);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapControl, hyscan_gtk_map_control, G_TYPE_OBJECT)

static void
hyscan_gtk_map_control_class_init (HyScanGtkMapControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_control_set_property;
  object_class->constructed = hyscan_gtk_map_control_object_constructed;
  object_class->finalize = hyscan_gtk_map_control_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "Map", "HyScanGtkMap widget", HYSCAN_TYPE_GTK_MAP,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_control_init (HyScanGtkMapControl *gtk_map_control)
{
  gtk_map_control->priv = hyscan_gtk_map_control_get_instance_private (gtk_map_control);
}

static void
hyscan_gtk_map_control_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanGtkMapControl *gtk_map_control = HYSCAN_GTK_MAP_CONTROL (object);
  HyScanGtkMapControlPrivate *priv = gtk_map_control->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_control_object_constructed (GObject *object)
{
  HyScanGtkMapControl *gtk_map_control = HYSCAN_GTK_MAP_CONTROL (object);
  HyScanGtkMapControlPrivate *priv = gtk_map_control->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_control_parent_class)->constructed (object);

  /* Обработчики взаимодействия мышки пользователя с картой. */
  g_signal_connect_swapped (priv->map, "button-press-event",    G_CALLBACK (hyscan_gtk_map_control_button_press_release), gtk_map_control);
  g_signal_connect_swapped (priv->map, "button-release-event",  G_CALLBACK (hyscan_gtk_map_control_button_press_release), gtk_map_control);
  g_signal_connect_swapped (priv->map, "motion-notify-event",   G_CALLBACK (hyscan_gtk_map_control_motion), gtk_map_control);
  g_signal_connect_swapped (priv->map, "scroll-event",          G_CALLBACK (hyscan_gtk_map_control_scroll), gtk_map_control);
}

static void
hyscan_gtk_map_control_object_finalize (GObject *object)
{
  HyScanGtkMapControl *gtk_map_control = HYSCAN_GTK_MAP_CONTROL (object);
  HyScanGtkMapControlPrivate *priv = gtk_map_control->priv;

  g_clear_object (&priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_control_parent_class)->finalize (object);
}

/* Обработчик событий прокрутки колёсика мышки. */
static gboolean
hyscan_gtk_map_control_scroll (HyScanGtkMapControl *control,
                               GdkEventScroll      *event)
{
  HyScanGtkMap *map = control->priv->map;

  gdouble dscale;
  gdouble scale;

  gdouble center_x, center_y;

  if (event->direction == GDK_SCROLL_UP)
    dscale = -1;
  else if (event->direction == GDK_SCROLL_DOWN)
    dscale = 1;
  else
    return FALSE;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (map), &scale, NULL);
  /* За 4 колесика изменяем масштаб в 2 раза. */
  scale *= pow (2, dscale * .25);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (map), event->x, event->y, &center_x, &center_y);
  gtk_cifro_area_set_scale (GTK_CIFRO_AREA (map), scale, scale, center_x, center_y);

  return FALSE;
}

static void
hyscan_gtk_map_set_state (HyScanGtkMapControlPrivate *priv,
                          gint state)
{
  GdkWindow *window;
  GdkCursor *cursor = NULL;

  priv->move_state = state;

  window = gtk_widget_get_window (GTK_WIDGET (priv->map));
  if (priv->move_state == STATE_MOVE)
    {
      GdkDisplay *display;

      display = gdk_window_get_display (window);
      cursor = gdk_cursor_new_from_name (display, "grabbing");
      // cursor = gdk_cursor_new (GDK_DIAMOND_CROSS);
    }

  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);
}

/* Обработчик перемещений курсора мыши. */
static gboolean
hyscan_gtk_map_control_motion (HyScanGtkMapControl *control,
                               GdkEventMotion      *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (priv->map);
  GtkCifroArea *carea = GTK_CIFRO_AREA (map);

  /* Если после нажатия кнопки сдвинулись больше 3 пикселей - начинаем сдвиг. */
  if (priv->move_state == STATE_AWAIT)
    {
      if (fabs (priv->move_from_x - event->x) + fabs (priv->move_from_y - event->y) > 3 && hyscan_gtk_map_grab_input (map, control))
        {
          hyscan_gtk_map_set_state (priv, STATE_MOVE);

          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  /* Режим перемещения - сдвигаем область. */
  if (priv->move_state == STATE_MOVE)
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

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_map_control_button_press_release (HyScanGtkMapControl *control,
                                             GdkEventButton      *event)
{
  HyScanGtkMapControlPrivate *priv = control->priv;
  HyScanGtkMap *map = HYSCAN_GTK_MAP (priv->map);
  GtkCifroArea *carea = GTK_CIFRO_AREA (map);

  /* Обрабатываем только нажатия левой клавишей мыши. */
  if (event->button != 1)
    return FALSE;

  /* Нажата левая клавиша мышки в видимой области - переходим в режим перемещения. */
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
        return FALSE;

      if ((event->x > border_left) && (event->x < (border_left + clip_width)) &&
          (event->y > border_top) && (event->y < (border_top + clip_height)))
        {
          hyscan_gtk_map_set_state (priv, STATE_AWAIT);
          priv->move_from_x = (gint) event->x;
          priv->move_from_y = (gint) event->y;
        }
    }

  /* Выключаем режим перемещения, если ввод был захвачен нами. */
  if (event->type == GDK_BUTTON_RELEASE)
    {
      gboolean stop_propagation;

      /* Если ввод был захвачен нами, то не передаём это событие дальше. */
      stop_propagation = (priv->move_state == STATE_MOVE);

      hyscan_gtk_map_set_state (priv, STATE_IDLE);
      hyscan_gtk_map_release_input (map, control);

      return stop_propagation;
    }

  return FALSE;
}

/**
 * hyscan_gtk_map_control_new:
 * @map: указатель на #HyScanGtkMap
 *
 * Returns:
 */
HyScanGtkMapControl *
hyscan_gtk_map_control_new (HyScanGtkMap *map)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_CONTROL, "map", map, NULL);
}

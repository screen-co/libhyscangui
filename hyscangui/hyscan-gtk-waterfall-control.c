#include "hyscan-gtk-wfcontrol-control.h"
#include <hyscan-trackrect.h>

struct _HyScanGtkWaterfallControlPrivate
{
  HyScanTrackRect      *rect;

  gboolean              automove;           /**< Включение и выключение режима автоматической сдвижки. */
  guint                 auto_tag;           /**< Идентификатор функции сдвижки. */
  guint                 automove_time;      /**< Период обновления экрана. */
  gdouble               mouse_y;            /**< У-координата мыши в момент нажатия кнопки. */
};

static void    hyscan_gtk_waterfall_control_object_constructed       (GObject               *object);
static void    hyscan_gtk_waterfall_control_object_finalize          (GObject               *object);
static gboolean hyscan_gtk_waterfall_control_scroll_pre              (GtkWidget             *widget,
                                                                      GdkEventScroll        *event,
                                                                      gpointer               data);
static gboolean hyscan_gtk_waterfall_control_scroll_post             (GtkWidget             *widget,
                                                                      GdkEventScroll        *event,
                                                                      gpointer               data);
static gboolean hyscan_gtk_waterfall_control_key_press_pre           (GtkWidget             *widget,
                                                                      GdkEventKey           *event,
                                                                      gpointer               data);
static gboolean hyscan_gtk_waterfall_control_key_press_post          (GtkWidget             *widget,
                                                                      GdkEventKey           *event,
                                                                      gpointer               data);
static gboolean hyscan_gtk_waterfall_control_button_press            (GtkWidget             *widget,
                                                                      GdkEventButton        *event,
                                                                      gpointer               data);
static gboolean hyscan_gtk_waterfall_control_motion                  (GtkWidget             *widget,
                                                                      GdkEventMotion        *event,
                                                                      gpointer               data);

static gboolean hyscan_gtk_waterfall_automover               (gpointer               data);
static void     hyscan_gtk_waterfall_set_view_once           (HyScanGtkWaterfall    *waterfall);
static gboolean hyscan_gtk_waterfall_track_params            (HyScanGtkWaterfall     *waterfall,
                                                              gdouble                *lwidth,
                                                              gdouble                *rwidth,
                                                              gdouble                *length);

static gboolean hyscan_gtk_waterfall_update_limits           (HyScanGtkWaterfall    *waterfall,
                                                              gdouble               *left,
                                                              gdouble               *right,
                                                              gdouble               *bottom,
                                                              gdouble               *top);



G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfallControl, hyscan_gtk_waterfall_control, HYSCAN_TYPE_GTK_WATERFALL);

static void
hyscan_gtk_waterfall_control_class_init (HyScanGtkWaterfallControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_control_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_control_object_finalize;
}

static void
hyscan_gtk_waterfall_control_init (HyScanGtkWaterfallControl *wfcontrol)
{
  wfcontrol->priv = hyscan_gtk_waterfall_control_get_instance_private (wfcontrol);
}

static void
hyscan_gtk_waterfall_control_object_constructed (GObject *object)
{
  HyScanGtkWaterfallControl *wfcontrol = HYSCAN_GTK_WATERFALL_CONTROL (object);
  HyScanGtkWaterfallControlPrivate *priv = wfcontrol->priv;

  priv->rect = hyscan_trackrect_new ();

  /* Подключаемся к сигналам. */
  g_signal_connect (gtk_waterfall, "configure-event", G_CALLBACK (hyscan_gtk_waterfall_configure), NULL);

  g_signal_connect (gtk_waterfall, "button-press-event", G_CALLBACK (hyscan_gtk_waterfall_control_button_press), gtk_waterfall);
  g_signal_connect (gtk_waterfall, "motion-notify-event", G_CALLBACK (hyscan_gtk_waterfall_control_motion), gtk_waterfall);
  g_signal_connect_after (gtk_waterfall, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_control_scroll_post), NULL);
  g_signal_connect (gtk_waterfall, "scroll-event", G_CALLBACK (hyscan_gtk_waterfall_control_scroll_pre), NULL);
  g_signal_connect_after (gtk_waterfall, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_control_key_press_post), NULL);
  g_signal_connect (gtk_waterfall, "key-press-event", G_CALLBACK (hyscan_gtk_waterfall_control_key_press_pre), NULL);

  priv->auto_tag = 0;
  priv->automove_time = 40;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_control_parent_class)->constructed (object);
}

static void
hyscan_gtk_waterfall_control_object_finalize (GObject *object)
{
  HyScanGtkWaterfallControl *wfcontrol = HYSCAN_GTK_WATERFALL_CONTROL (object);
  HyScanGtkWaterfallControlPrivate *priv = wfcontrol->priv;

  if (priv->auto_tag > 0)
    g_source_remove (priv->auto_tag);

  g_clear_object (&priv->rect);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_control_parent_class)->finalize (object);
}

/* Обработчик сигнала прокрутки мыши. Выполняется до обработчика cifroarea. */
static gboolean
hyscan_gtk_waterfall_control_scroll_pre (GtkWidget      *widget,
                                    GdkEventScroll *event,
                                    gpointer        data)
{
  if ((event->state & GDK_SHIFT_MASK) ||
      (event->state & GDK_MOD1_MASK) ||
      (event->state & GDK_CONTROL_MASK))
    {
      gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), -999999, 999999, -999999, 999999);
    }
  return FALSE;
}

/* Обработчик сигнала прокрутки мыши. Выполняется после обработчика cifroarea.*/
static gboolean
hyscan_gtk_waterfall_control_scroll_post (GtkWidget      *widget,
                                   GdkEventScroll *event,
                                   gpointer        data)
{
  gdouble left, right, bottom, top;
  hyscan_gtk_waterfall_control_update_limits (HYSCAN_GTK_WATERFALL (widget), &left, &right, &bottom, &top);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), left, right, bottom, top);

  if (HYSCAN_GTK_WATERFALL (widget)->priv->automove)
    gtk_cifro_area_move (GTK_CIFRO_AREA (widget), 0, G_MAXDOUBLE);
  return FALSE;
}

/* Обработчик сигнала нажатия кнопки. Выполняется до обработчика cifroarea. */
static gboolean
hyscan_gtk_waterfall_control_key_press_pre (GtkWidget   *widget,
                                       GdkEventKey *event,
                                       gpointer     data)
{
  if ((event->keyval == GDK_KEY_KP_Add) ||
      (event->keyval == GDK_KEY_KP_Subtract) ||
      (event->keyval == GDK_KEY_plus) ||
      (event->keyval == GDK_KEY_minus))
    {
      gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), -999999, 999999, -999999, 999999);
    }
  return FALSE;
}

/* Обработчик сигнала нажатия кнопки. Выполняется после обработчика cifroarea.*/
static gboolean
hyscan_gtk_waterfall_control_key_press_post (GtkWidget   *widget,
                                      GdkEventKey *event,
                                      gpointer     data)
{
  gdouble left, right, bottom, top;

  if ((event->keyval == GDK_KEY_KP_Add) ||
      (event->keyval == GDK_KEY_KP_Subtract) ||
      (event->keyval == GDK_KEY_plus) ||
      (event->keyval == GDK_KEY_minus))
    {
      hyscan_gtk_waterfall_control_update_limits (HYSCAN_GTK_WATERFALL (widget), &left, &right, &bottom, &top);
      gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (widget), left, right, bottom, top);


      if (HYSCAN_GTK_WATERFALL (widget)->priv->automove)
        gtk_cifro_area_move (GTK_CIFRO_AREA (widget), 0, G_MAXDOUBLE);
    }
  return FALSE;
}

/* Обработчик нажатия кнопок мышки. */
static gboolean
hyscan_gtk_waterfall_control_button_press (GtkWidget      *widget,
                                   GdkEventButton *event,
                                   gpointer        data)
{
  HyScanGtkWaterfallControl *wfcontrol = data;
  HyScanGtkWaterfallControlPrivate *priv = wfcontrol->priv;

  if (event->type == GDK_BUTTON_PRESS && (event->button == 1))
    priv->mouse_y = event->y;

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_control_motion (GtkWidget             *widget,
                             GdkEventMotion        *event,
                             gpointer               data)
{
  HyScanGtkWaterfallControl *wfcontrol = data;
  HyScanGtkWaterfallControlPrivate *priv = wfcontrol->priv;

  if (event->state & GDK_BUTTON1_MASK)
    {
      if (event->y < priv->mouse_y && priv->automove)
        {
          priv->automove = FALSE;
          g_signal_emit (wfcontrol, hyscan_gtk_waterfall_control_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
        }
    }
  return FALSE;
}

/* Функция устанавливает частоту кадров. */
void
hyscan_gtk_waterfall_control_set_fps (HyScanGtkWaterfallControl *wfcontrol,
                                       guint               fps)
{
  HyScanGtkWaterfallControlPrivate *priv;
  guint interval;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (wfcontrol));
  priv = wfcontrol->priv;

  interval = 1000 / fps;

  if (priv->automove_time != interval)
    {
      priv->automove_time = interval;
      if (priv->auto_tag != 0)
        g_source_remove (priv->auto_tag);
      priv->auto_tag = g_timeout_add (priv->automove_time, hyscan_gtk_waterfall_automover, wfcontrol);
    }
}

/* Функция сдвижки изображения. */
void
hyscan_gtk_waterfall_control_automove (HyScanGtkWaterfallControl *wfcontrol,
                                       gboolean            on)
{
  HyScanGtkWaterfallControlPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (wfcontrol));
  priv = wfcontrol->priv;

  priv->automove = on;

  if (on)
    {
      gtk_cifro_area_move (GTK_CIFRO_AREA (wfcontrol), 0, G_MAXDOUBLE);
      /* Если функции нет, создаем её. */
      if (priv->auto_tag == 0)
        priv->auto_tag = g_timeout_add (priv->automove_time, hyscan_gtk_waterfall_automover, wfcontrol);
    }
}

static gboolean
hyscan_gtk_waterfall_update_limits (HyScanGtkWaterfallControl *wfcontrol,
                                    gdouble            *left,
                                    gdouble            *right,
                                    gdouble            *bottom,
                                    gdouble            *top)
{
  HyScanGtkWaterfallControlPrivate *priv = wfcontrol->priv;

  gdouble lwidth = 0.0;
  gdouble rwidth = 0.0;
  gdouble length = 0.0;
  gboolean writeable;
  gint visible_w, visible_h,
       lborder, rborder, tborder, bborder,
       zoom_num;
  gdouble req_width = 0.0,
          req_length = 0.0,
          min_length = 0.0,
          max_length = 0.0;
  gdouble bottom_limit = 0.0,
          bottom_view = 0.0;

  /* К этому моменту времени я уверен, что у меня есть хоть какие-то
   * геометрические параметры галса. */
  if (priv->opened)
    {
      writeable = hyscan_gtk_waterfall_track_params (wfcontrol, &lwidth, &rwidth, &length);
    }
  else
    {
      writeable = TRUE;
      length = lwidth = rwidth = 0.0;
    }

  priv->length = length;
  priv->lwidth = lwidth;
  priv->rwidth = rwidth;

  /* Получаем параметры цифроарии. */
  gtk_cifro_area_get_size	(GTK_CIFRO_AREA (wfcontrol), &visible_w, &visible_h);
  gtk_cifro_area_get_border (GTK_CIFRO_AREA (wfcontrol), &lborder, &rborder, &tborder, &bborder);
  zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (wfcontrol), NULL, NULL);

  visible_w -= (lborder + rborder);
  visible_h -= (tborder + bborder);

  /* Для текущего масштаба рассчиываем необходимую длину и ширину по каждому борту. */
  req_length = visible_h * priv->zooms[zoom_num];
  req_width = visible_w * priv->zooms[zoom_num];

  /* Горизонтальные размеры области рассчитываются независимо от вертикальных и режима работы. */
  req_width = MAX (req_width, (lwidth + rwidth));

  if (lwidth <= 0.0 && rwidth <= 0.0)
    {
      lwidth = req_width / 2.0;
      rwidth = req_width / 2.0;
    }
  else
    {
      gdouble lratio = lwidth / (rwidth + lwidth);
      lwidth = req_width * lratio;
      rwidth = req_width - lwidth;
    }

  /* Вертикальные размеры области. */
  if (!writeable) /*< Режим чтения. */
    {
      priv->automove = FALSE;
      g_signal_emit (wfcontrol, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, wfcontrol);

      req_length = MAX (length, req_length);
      if (length >= req_length)
        {
          min_length = 0;
          max_length = length;
        }
      else
        {
          min_length = (req_length - length) / 2.0;
          max_length = req_length - min_length;
        }
    }
  else if (length < req_length) /*< Режим дозаписи, длина галса меньше виджета на текущем масштабе. */
    {
      max_length = length;
      min_length = (req_length - max_length);
    }
  else /*< Режим дозаписи, длина галса больше виджета на самом мелком масштабе. */
    {
      min_length = 0;
      max_length = length;
    }

  if (!priv->automove && writeable)
    {
      gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (wfcontrol), NULL, NULL, &bottom_limit, NULL);
      gtk_cifro_area_get_view (GTK_CIFRO_AREA (wfcontrol), NULL, NULL, &bottom_view, NULL);
      bottom_limit = MAX (bottom_view, bottom_limit);
      bottom_limit = MIN (-min_length, bottom_limit);
    }
  else
    {
      bottom_limit = -min_length;
    }

  if (left != NULL)
    *left = -lwidth;
  if (right != NULL)
    *right = rwidth;
  if (bottom != NULL)
    *bottom = bottom_limit;
  if (top != NULL)
    *top = max_length;

  return writeable;
}

static gboolean
hyscan_gtk_waterfall_automover (gpointer data)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (data);
  HyScanGtkWaterfallPrivate *priv;
  gdouble distance;
  gint64 time;
  gint zoom_num;
  priv = waterfall->priv;
  gdouble left, right;
  gdouble old_bottom, old_top;
  gdouble required_bottom, required_top;
  gdouble bottom, top;
  gboolean writeable;

  if (!priv->opened)
    return G_SOURCE_CONTINUE;

  /* Актуальные данные по пределам видимой области. */
  gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (waterfall), NULL, NULL, &old_bottom, NULL);
  writeable = hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (waterfall), &left, &right, &required_bottom, &required_top);

  /* Наращиваем пределы видимой области. */
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, old_bottom, required_top);

  /* Теперь при необходимости сдвигаемся и сжимаем видимую область. */
  if (priv->view_finalised && priv->automove)
    {
      time = g_get_monotonic_time ();
      distance = (time - priv->prev_time) / 1e6;
      distance *= priv->ship_speed;

      /* Сдвигаем видимую область. */
      gtk_cifro_area_get_view (GTK_CIFRO_AREA (waterfall), &left, &right, &bottom, &old_top);
      zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);
      if (old_top + distance < required_top - 1.0 * priv->ship_speed)
        {
          distance = required_top - 0.5 * priv->ship_speed - old_top;
          distance /= priv->zooms[zoom_num];
          gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, distance);
        }
      else if (old_top + distance < required_top - 0.5 * priv->ship_speed)
        {
          distance /= priv->zooms[zoom_num];
          gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, distance);
        }
      priv->prev_time = time;
    }

  /* Когда данных недостаточно, нужно сжимать видимую область. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (waterfall), NULL, NULL, &bottom, &top);
  gtk_cifro_area_get_view_limits (GTK_CIFRO_AREA (waterfall), NULL, NULL, &old_bottom, &old_top);
  if (required_bottom < 0.0)
    {
      gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, old_top);
    }

  if (writeable)
    return G_SOURCE_CONTINUE;

  hyscan_gtk_waterfall_update_limits (HYSCAN_GTK_WATERFALL (waterfall), &left, &right, &bottom, &top);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);

  priv->automove = FALSE;
  g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_AUTOMOVE_STATE], 0, priv->automove);
  priv->auto_tag = 0;
  return G_SOURCE_REMOVE;
}

/* Функция зуммирует изображение. */
gint
hyscan_gtk_waterfall_zoom (HyScanGtkWaterfall *waterfall,
                           gboolean            zoom_in)
{
  gint zoom_num;
  gdouble min_x, max_x, min_y, max_y;
  gdouble left, right, bottom, top;

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1);

  if (!waterfall->priv->opened)
    return -1;

  gtk_cifro_area_get_view	(GTK_CIFRO_AREA (waterfall), &min_x, &max_x, &min_y, &max_y);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), -999999, 999999, -999999, 999999);

  gtk_cifro_area_fixed_zoom	(GTK_CIFRO_AREA (waterfall), (max_x + min_x) / 2.0, (max_y + min_y) / 2.0, zoom_in);

  hyscan_gtk_waterfall_update_limits (waterfall, &left, &right, &bottom, &top);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);

  if (waterfall->priv->automove)
    gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, G_MAXDOUBLE);

  zoom_num = gtk_cifro_area_get_scale (GTK_CIFRO_AREA (waterfall), NULL, NULL);

  if (zoom_num >= 0 && zoom_num < ZOOM_LEVELS)
    g_signal_emit (waterfall, hyscan_gtk_waterfall_signals[SIGNAL_ZOOM], 0, zoom_num, zooms_gost[zoom_num]);

  return zoom_num;
}

gboolean
hyscan_gtk_waterfall_track_params (HyScanGtkWaterfall *waterfall,
                                   gdouble           *lwidth,
                                   gdouble           *rwidth,
                                   gdouble           *length)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), TRUE);

  return hyscan_trackrect_get (waterfall->priv->rect, lwidth, rwidth, length);
}

static void
hyscan_gtk_waterfall_set_view_once (HyScanGtkWaterfall    *waterfall)
{

  gdouble lwidth, rwidth, length;
  gdouble left, right, bottom, top;

  hyscan_gtk_waterfall_update_limits (waterfall, &left, &right, &bottom, &top);
  gtk_cifro_area_set_view_limits (GTK_CIFRO_AREA (waterfall), left, right, bottom, top);

  if (!hyscan_gtk_waterfall_track_params (waterfall, &lwidth, &rwidth, &length))
    {
      gtk_cifro_area_set_view_side_select (GTK_CIFRO_AREA (waterfall), -lwidth, rwidth, 0, length, TRUE);
    }
  else
    {
      gtk_cifro_area_move (GTK_CIFRO_AREA (waterfall), 0, G_MAXDOUBLE);
    }
}

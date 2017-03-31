#include "hyscan-gtk-waterfall.h"
#include "hyscan-gtk-waterfall-private.h"
#include <hyscan-depth-acoustic.h>
#include <string.h>

struct _HyScanGtkWaterfallPrivate
{
  /* Переменные, используемые наследниками. */
  HyScanCache         *cache;
  HyScanCache         *cache2;
  gchar               *cache_prefix;

  HyScanTileType       tile_type;

  gfloat               ship_speed;
  GArray              *sound_velocity;

  HyScanSourceType     depth_source;
  guint                depth_channel;
  guint                depth_filter_size;
  gulong               depth_usecs;

  HyScanDB            *db;
  gchar               *project;
  gchar               *track;
  gboolean             raw;

  HyScanGtkWaterfallType widget_type;
  HyScanSourceType       left_source;
  HyScanSourceType       right_source;

  /* Переменные, используемые объектом. */
  gboolean               move_area;            /* Признак перемещения при нажатой клавише мыши. */
  gint                   move_from_x;          /* Начальная координата x перемещения. */
  gint                   move_from_y;          /* Начальная координата y перемещения. */
  gdouble                start_from_x;         /* Начальная граница отображения по оси X. */
  gdouble                start_to_x;           /* Начальная граница отображения по оси X. */
  gdouble                start_from_y;         /* Начальная граница отображения по оси Y. */
  gdouble                start_to_y;           /* Начальная граница отображения по оси Y. */

  gint                   mouse_x;              /* Начальная координата x перемещения. */
  gint                   mouse_y;              /* Начальная координата y перемещения. */
};

static void      hyscan_gtk_waterfall_object_constructed     (GObject               *object);
static void      hyscan_gtk_waterfall_object_finalize        (GObject               *object);

static gboolean  hyscan_gtk_waterfall_keyboard               (GtkWidget             *widget,
                                                              GdkEventKey           *event);

static gboolean  hyscan_gtk_waterfall_mouse_buttons          (GtkWidget             *widget,
                                                              GdkEventButton        *event);

static gboolean  hyscan_gtk_waterfall_motion                 (GtkWidget             *widget,
                                                              GdkEventMotion        *event);
static gboolean  hyscan_gtk_waterfall_leave                  (GtkWidget             *widget,
                                                              GdkEventCrossing      *event);

static gboolean  hyscan_gtk_waterfall_scroll                 (GtkWidget             *widget,
                                                              GdkEventScroll        *event);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkWaterfall, hyscan_gtk_waterfall, GTK_TYPE_CIFRO_AREA);

static void
hyscan_gtk_waterfall_class_init (HyScanGtkWaterfallClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = hyscan_gtk_waterfall_object_constructed;
  object_class->finalize = hyscan_gtk_waterfall_object_finalize;

  /* Инициализируем виртуальные функции. */
  klass->open = NULL;
  klass->close = NULL;

  widget_class->key_press_event = hyscan_gtk_waterfall_keyboard;
  widget_class->button_press_event = hyscan_gtk_waterfall_mouse_buttons;
  widget_class->button_release_event = hyscan_gtk_waterfall_mouse_buttons;
  widget_class->motion_notify_event = hyscan_gtk_waterfall_motion;
  widget_class->leave_notify_event = hyscan_gtk_waterfall_leave;
  widget_class->scroll_event = hyscan_gtk_waterfall_scroll;
}

static void
hyscan_gtk_waterfall_init (HyScanGtkWaterfall *waterfall)
{
  waterfall->priv = hyscan_gtk_waterfall_get_instance_private (waterfall);

  gtk_widget_set_can_focus (GTK_WIDGET (waterfall), TRUE);
}

static void
hyscan_gtk_waterfall_object_constructed (GObject *object)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv = waterfall->priv;
  HyScanSoundVelocity *link;

  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->constructed (object);

  priv->sound_velocity = g_array_sized_new (FALSE, FALSE, sizeof (HyScanSoundVelocity), 1);
  link = &g_array_index (priv->sound_velocity, HyScanSoundVelocity, 0);
  link->depth = 0.0;
  link->velocity = 1500.0;

  priv->widget_type = HYSCAN_GTK_WATERFALL_SIDESCAN;
  priv->left_source = HYSCAN_SOURCE_SIDE_SCAN_PORT;
  priv->right_source = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;

  priv->tile_type = HYSCAN_TILE_SLANT;
  priv->ship_speed = 1.0;
  priv->depth_source = HYSCAN_SOURCE_ECHOSOUNDER;
  priv->depth_channel = 1;
}

static void
hyscan_gtk_waterfall_object_finalize (GObject *object)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (object);
  HyScanGtkWaterfallPrivate *priv = waterfall->priv;

  g_clear_object (&priv->cache);
  g_clear_object (&priv->cache2);
  g_free (priv->cache_prefix);

  g_array_unref (priv->sound_velocity);

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_parent_class)->finalize (object);
}

static gboolean
hyscan_gtk_waterfall_keyboard (GtkWidget   *widget,
                               GdkEventKey *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);

  gboolean arrow_keys = FALSE;
  gboolean plus_minus_keys = FALSE;
  gboolean pg_home_end_keys = FALSE;

  /* Стрелки (движение). */
  arrow_keys       = (event->keyval == GDK_KEY_Left        ||
                      event->keyval == GDK_KEY_Right       ||
                      event->keyval == GDK_KEY_Up          ||
                      event->keyval == GDK_KEY_Down);
  /* Плюс/минус (зум). */
  plus_minus_keys  = (event->keyval == GDK_KEY_minus       ||
                      event->keyval == GDK_KEY_equal       ||
                      event->keyval == GDK_KEY_KP_Add      ||
                      event->keyval == GDK_KEY_KP_Subtract ||
                      event->keyval == GDK_KEY_underscore  ||
                      event->keyval == GDK_KEY_plus);
  /* PgUp, PgDn, home, end (движение). */
  pg_home_end_keys = (event->keyval == GDK_KEY_Page_Down   ||
                      event->keyval == GDK_KEY_Page_Up     ||
                      event->keyval == GDK_KEY_End         ||
                      event->keyval == GDK_KEY_Home);
  if (arrow_keys)
    {
      gint step_x = -1 * (event->keyval == GDK_KEY_Left) + 1 * (event->keyval == GDK_KEY_Right);
      gint step_y = -1 * (event->keyval == GDK_KEY_Down) + 1 * (event->keyval == GDK_KEY_Up);

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
      HyScanGtkWaterfallPrivate *priv = HYSCAN_GTK_WATERFALL (widget)->priv;

      /* В режимах ГБО и эхолот эти кнопки по-разному реагируют. */
      gtk_cifro_area_get_size (carea, &width, &height);

      if (event->keyval == GDK_KEY_Page_Up)
        {
          step_echo = -0.75 * width;
          step_ss = 0.75 * height;
        }
      else if (event->keyval == GDK_KEY_Page_Down)
        {
          step_echo = 0.75 * width;
          step_ss = -0.75 * height;
        }
      else if (event->keyval == GDK_KEY_End)
        {
          step_echo = step_ss = G_MAXINT;
        }
      else if (event->keyval == GDK_KEY_Home)
        {
          step_echo = step_ss = G_MININT;
        }

      if (priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER)
        gtk_cifro_area_move (carea, step_echo, 0);
      else if (priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
        gtk_cifro_area_move (carea, 0, step_ss);
    }
  else if (plus_minus_keys)
    {
      GtkCifroAreaZoomType direction;
      gdouble from_x, to_x, from_y, to_y;

      if (event->keyval == GDK_KEY_KP_Add ||
          event->keyval == GDK_KEY_equal ||
          event->keyval == GDK_KEY_plus)
        {
          direction = GTK_CIFRO_AREA_ZOOM_IN;
        }
      else
        {
          direction = GTK_CIFRO_AREA_ZOOM_OUT;
        }

      gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
      gtk_cifro_area_zoom (carea, direction, direction, (to_x + from_x) / 2.0, (to_y+from_y)/2.0);
    }

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_mouse_buttons (GtkWidget      *widget,
                                    GdkEventButton *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = waterfall->priv;

  if ((event->type == GDK_BUTTON_PRESS) && (event->button == 1))
    {
      guint widget_width, widget_height;
      guint border_top, border_bottom;
      guint border_left, border_right;

      gtk_cifro_area_get_size (carea, &widget_width, &widget_height);
      gtk_cifro_area_get_border (carea, &border_top, &border_bottom, &border_left, &border_right);

      if (event->x > border_left && event->x < widget_width - border_right &&
          event->y > border_top && event->y < widget_height - border_bottom)
        {
          priv->move_from_x = event->x;
          priv->move_from_y = event->y;
          priv->move_area = TRUE;
          gtk_cifro_area_get_view (carea, &priv->start_from_x, &priv->start_to_x,
                                          &priv->start_from_y, &priv->start_to_y);
        }
    }

  if (event->type == GDK_BUTTON_RELEASE && event->button == 1)
    priv->move_area = FALSE;

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_motion (GtkWidget      *widget,
                             GdkEventMotion *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);
  HyScanGtkWaterfallPrivate *priv = waterfall->priv;

  /* Режим перемещения - сдвигаем область. */
  if (priv->move_area)
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

  priv->mouse_x = event->x;
  priv->mouse_y = event->y;

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_leave (GtkWidget        *widget,
                            GdkEventCrossing *event)
{
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);

  waterfall->priv->mouse_x = -1;
  waterfall->priv->mouse_y = -1;

  return FALSE;
}

static gboolean
hyscan_gtk_waterfall_scroll (GtkWidget      *widget,
                             GdkEventScroll *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkWaterfall *waterfall = HYSCAN_GTK_WATERFALL (widget);
  guint width, height;

  gtk_cifro_area_get_size (carea, &width, &height);

  /* Зуммирование. */
  if (event->state & GDK_CONTROL_MASK)
    {
      gdouble from_x, to_x, from_y, to_y;
      gdouble zoom_x, zoom_y;
      GtkCifroAreaZoomType direction;
      HyScanGtkWaterfallPrivate *priv = waterfall->priv;

      gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

      direction = (event->direction == GDK_SCROLL_UP) ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

      zoom_x = from_x + (to_x - from_x) * ((gdouble)priv->mouse_x / (gdouble)width);
      zoom_y = from_y + (to_y - from_y) * (1.0 - (gdouble)priv->mouse_y / height);

      gtk_cifro_area_zoom (carea, direction, direction, zoom_x, zoom_y);
    }
  /* Скролл. */
  else
    {
      gint step = height / 10;
      step *= (event->direction == GDK_SCROLL_UP) ? 1 : -1;

      if (waterfall->priv->widget_type == HYSCAN_GTK_WATERFALL_SIDESCAN)
        gtk_cifro_area_move (carea, 0, step);
      else if (waterfall->priv->widget_type == HYSCAN_GTK_WATERFALL_ECHOSOUNDER)
        gtk_cifro_area_move (carea, -step, 0);
    }

  return FALSE;
}

void
hyscan_gtk_waterfall_echosounder (HyScanGtkWaterfall *waterfall,
                                  HyScanSourceType    source)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  if (!hyscan_source_is_acoustic (source))
    return;

  waterfall->priv->widget_type = HYSCAN_GTK_WATERFALL_ECHOSOUNDER;

  waterfall->priv->left_source = source;
  waterfall->priv->right_source = source;
}

void
hyscan_gtk_waterfall_sidescan (HyScanGtkWaterfall *waterfall,
                               HyScanSourceType    left,
                               HyScanSourceType    right)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  if (!hyscan_source_is_acoustic (left) || !hyscan_source_is_acoustic (right))
    return;

  waterfall->priv->widget_type = HYSCAN_GTK_WATERFALL_SIDESCAN;

  waterfall->priv->left_source = left;
  waterfall->priv->right_source = right;
}

void
hyscan_gtk_waterfall_set_cache (HyScanGtkWaterfall *waterfall,
                                HyScanCache        *cache,
                                HyScanCache        *cache2,
                                const gchar        *prefix)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  if (cache == NULL)
    return;

  if (cache2 == NULL)
    cache2 = cache;

  g_clear_object (&priv->cache);
  g_clear_object (&priv->cache2);
  g_free (priv->cache_prefix);

  priv->cache  = g_object_ref (cache);
  priv->cache2 = g_object_ref (cache2);
  priv->cache_prefix = g_strdup (prefix);
}

const gchar*
hyscan_gtk_waterfall_get_cache_prefix (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), NULL);

  return waterfall->priv->cache_prefix;
}

void
hyscan_gtk_waterfall_set_tile_type (HyScanGtkWaterfall *waterfall,
                                    HyScanTileType      type)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  waterfall->priv->tile_type = type;
}

void
hyscan_gtk_waterfall_set_ship_speed (HyScanGtkWaterfall  *waterfall,
                                     gfloat               ship_speed)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  waterfall->priv->ship_speed = ship_speed;
}

void
hyscan_gtk_waterfall_set_sound_velocity (HyScanGtkWaterfall  *waterfall,
                                         GArray              *velocity)
{
  HyScanGtkWaterfallPrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  priv = waterfall->priv;

  if (priv->sound_velocity != NULL)
    g_clear_pointer (&priv->sound_velocity, g_array_unref);

  if (velocity != NULL)
    waterfall->priv->sound_velocity = g_array_ref (velocity);

}
void
hyscan_gtk_waterfall_set_depth_source (HyScanGtkWaterfall  *waterfall,
                                       HyScanSourceType     source,
                                       guint                channel)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  waterfall->priv->depth_source = source;
  waterfall->priv->depth_channel = channel;
}
void
hyscan_gtk_waterfall_set_depth_filter_size (HyScanGtkWaterfall *waterfall,
                                            guint               size)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  waterfall->priv->depth_filter_size = size;
}

void
hyscan_gtk_waterfall_set_depth_time (HyScanGtkWaterfall  *waterfall,
                                     gulong               usecs)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  waterfall->priv->depth_usecs = usecs;
}

void
hyscan_gtk_waterfall_open (HyScanGtkWaterfall *waterfall,
                           HyScanDB           *db,
                           const gchar        *project,
                           const gchar        *track,
                           gboolean            raw)
{
  HyScanGtkWaterfallClass *class;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  class = HYSCAN_GTK_WATERFALL_GET_CLASS (waterfall);


  class->close (waterfall); // TODO: подумать, нужно ли это.
  class->open (waterfall, db, project, track, raw);
}

void
hyscan_gtk_waterfall_close (HyScanGtkWaterfall *waterfall)
{
  HyScanGtkWaterfallClass *class;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));

  class = HYSCAN_GTK_WATERFALL_GET_CLASS (waterfall);

  class->close (waterfall);
}

void
hyscan_gtk_waterfall_zoom (HyScanGtkWaterfall *waterfall,
                           gboolean            zoom_in)
{
  GtkCifroArea *carea;
  GtkCifroAreaZoomType direction;
  gdouble from_x, to_x, from_y, to_y;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall));
  carea = GTK_CIFRO_AREA (waterfall);

  direction = zoom_in ? GTK_CIFRO_AREA_ZOOM_IN : GTK_CIFRO_AREA_ZOOM_OUT;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  gtk_cifro_area_zoom (carea, direction, direction, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);
}

HyScanGtkWaterfallType
hyscan_gtk_waterfall_get_sources (HyScanGtkWaterfall *waterfall,
                                  HyScanSourceType   *left,
                                  HyScanSourceType   *right)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), 0);

  if (left != NULL)
    *left = waterfall->priv->left_source;
  if (right != NULL)
    *right = waterfall->priv->right_source;

  return waterfall->priv->widget_type;
}

HyScanCache*
hyscan_gtk_waterfall_get_cache (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), NULL);

  return waterfall->priv->cache;
}

HyScanCache*
hyscan_gtk_waterfall_get_cache2 (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), NULL);

  return waterfall->priv->cache2;
}

HyScanTileType
hyscan_gtk_waterfall_get_tile_type (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1);

  return waterfall->priv->tile_type;
}

gfloat
hyscan_gtk_waterfall_get_ship_speed (HyScanGtkWaterfall  *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1.0);

  return waterfall->priv->ship_speed;
}

GArray*
hyscan_gtk_waterfall_get_sound_velocity (HyScanGtkWaterfall  *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), NULL);

  return waterfall->priv->sound_velocity;
}

HyScanSourceType
hyscan_gtk_waterfall_get_depth_source (HyScanGtkWaterfall  *waterfall,
                                       guint               *channel)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), -1);

  if (channel != NULL)
    *channel = waterfall->priv->depth_channel;

  return waterfall->priv->depth_source;
}

guint
hyscan_gtk_waterfall_get_depth_filter_size (HyScanGtkWaterfall *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), 0);

  return waterfall->priv->depth_filter_size;
}

gulong
hyscan_gtk_waterfall_get_depth_time (HyScanGtkWaterfall  *waterfall)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL (waterfall), 0);

  return waterfall->priv->depth_usecs;
}

/* hyscan-gtk-map-steer.c
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
 * SECTION: hyscan-gtk-map-steer
 * @Short_description: виджет навигации по плановому галсу
 * @Title: HyScanGtkMapSteer
 *
 * Виджет #HyScanGtkMapSteer служит для помощи судоводителю при навигации по плановому
 * галсу. В виджете отображается:
 * - плановые курс и скорость для текущего галса,
 * - отклонения по положению и скорости,
 * - мини-карта с положением антенны гидролокатора относительно планового галса.
 *
 * На мини-карте отображается стрелка, соответствующая положению антенны и курсу судна.
 *
 */

#include "hyscan-gtk-map-steer.h"
#include <hyscan-cartesian.h>
#include <glib/gi18n-lib.h>
#include <hyscan-steer.h>

#define PATH_LIFETIME 100   /* Время жизни точек траектории, секунды. */
#define ARROW_SIZE    30

#define HYSCAN_TYPE_GTK_MAP_STEER_CAREA             (hyscan_gtk_map_steer_carea_get_type ())
#define HYSCAN_GTK_MAP_STEER_CAREA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCarea))
#define HYSCAN_IS_GTK_MAP_STEER_CAREA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA))
#define HYSCAN_GTK_MAP_STEER_CAREA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCareaClass))
#define HYSCAN_IS_GTK_MAP_STEER_CAREA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_STEER_CAREA))
#define HYSCAN_GTK_MAP_STEER_CAREA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCareaClass))

enum
{
  PROP_O,
  PROP_STEER,
};

enum
{
  STATE_NONE,
  STATE_GRAB
};

typedef struct
{
  GtkCifroArea                parent_instance;    /* Родительский объект. */
  HyScanGtkMapSteer          *gtk_steer;          /* Указатель на основной виджет. */
  PangoLayout                *pango_layout;       /* Шрифт. */
  gint                        border_size;        /* Размер окантовки. */
} HyScanGtkMapSteerCarea;

typedef struct
{
  GtkCifroAreaClass           parent_instance;    /* Родительский класс. */
} HyScanGtkMapSteerCareaClass;

struct _HyScanGtkMapSteerPrivate
{
  HyScanSteer                *steer;              /* Модель управления судном. */
  guint                       state;              /* Статус перетаскивания линии допустимого отклонения. */
  GQueue                     *points;             /* Список точек HyScanSteerPoint. */

  HyScanTrackPlan            *plan;               /* Активный план галса. */
  gchar                      *track_id;           /* Ид активного галса. */

  GtkWidget                  *track_info;         /* Виджет с информацией о текущем галсе, GtkLabel. */
  GtkWidget                  *carea;              /* Виджет мини-карты с траекторией судна, HyScanGtkMapSteerCarea. */

  GdkRGBA                     color_bg;           /* Цвет фона области рисования. */
  GdkRGBA                     color_bg_good;      /* Цвет фона "в пределах нормы". */
  gdouble                     arrow_size;         /* Размер стрелки судна. */
};

static void           hyscan_gtk_map_steer_set_property                   (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           const GValue             *value,
                                                                           GParamSpec               *pspec);
static void           hyscan_gtk_map_steer_object_constructed             (GObject                  *object);
static void           hyscan_gtk_map_steer_object_finalize                (GObject                  *object);

static void           hyscan_gtk_map_steer_carea_object_finalize          (GObject                  *object);
static void           hyscan_gtk_map_steer_carea_get_border               (GtkCifroArea             *carea,
                                                                           guint                    *border_top,
                                                                           guint                    *border_bottom,
                                                                           guint                    *border_left,
                                                                           guint                    *border_right);
static void           hyscan_gtk_map_steer_carea_check_scale              (GtkCifroArea             *carea,
                                                                           gdouble                  *scale_x,
                                                                           gdouble                  *scale_y);
static void           hyscan_gtk_map_steer_carea_draw                     (GtkCifroArea             *carea,
                                                                           cairo_t                  *cairo,
                                                                           gpointer                  user_data);
static void           hyscan_gtk_map_steer_carea_screen_changed           (GtkWidget                *widget,
                                                                           GdkScreen                *previous_screen);
static gboolean       hyscan_gtk_map_steer_carea_configure                (GtkWidget                *widget,
                                                                           GdkEvent                 *event,
                                                                           gpointer                  user_data);
static gboolean       hyscan_gtk_map_steer_carea_scroll                   (GtkWidget                *widget,
                                                                           GdkEventScroll           *event);
static gboolean       hyscan_gtk_map_steer_carea_motion_notify            (GtkWidget                *widget,
                                                                           GdkEventMotion           *event);
static gboolean       hyscan_gtk_map_steer_carea_button                   (GtkWidget                *widget,
                                                                           GdkEventButton           *event);
static void           hyscan_gtk_map_steer_carea_draw_path                (HyScanGtkMapSteer        *gtk_steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_current             (HyScanGtkMapSteer        *gtk_steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_axis                (GtkCifroArea             *carea,
                                                                           cairo_t                  *cairo,
                                                                           gpointer                  user_data);
static void           hyscan_gtk_map_steer_carea_draw_center              (HyScanGtkMapSteer        *gtk_steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_err_line            (HyScanGtkMapSteer        *gtk_steer,
                                                                           cairo_t                  *cairo);
static gboolean       hyscan_gtk_map_steer_is_err_line                    (HyScanGtkMapSteer        *gtk_steer,
                                                                           gdouble                   x,
                                                                           gdouble                   y);

static GtkWidget *    hyscan_gtk_map_steer_create_carea                   (HyScanGtkMapSteer        *gtk_steer);

static void           hyscan_gtk_map_steer_nav_changed                    (HyScanGtkMapSteer        *gtk_steer,
                                                                           HyScanSteerPoint         *point);
static void           hyscan_gtk_map_steer_set_track                      (HyScanGtkMapSteer        *gtk_steer);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapSteer, hyscan_gtk_map_steer, GTK_TYPE_BOX)
G_DEFINE_TYPE (HyScanGtkMapSteerCarea, hyscan_gtk_map_steer_carea, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_map_steer_carea_class_init (HyScanGtkMapSteerCareaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkCifroAreaClass *carea_class = GTK_CIFRO_AREA_CLASS (klass);

  object_class->finalize = hyscan_gtk_map_steer_carea_object_finalize;

  widget_class->screen_changed = hyscan_gtk_map_steer_carea_screen_changed;
  widget_class->scroll_event = hyscan_gtk_map_steer_carea_scroll;
  widget_class->motion_notify_event = hyscan_gtk_map_steer_carea_motion_notify;
  widget_class->button_release_event = hyscan_gtk_map_steer_carea_button;
  widget_class->button_press_event = hyscan_gtk_map_steer_carea_button;

  carea_class->get_border = hyscan_gtk_map_steer_carea_get_border;
  carea_class->check_scale = hyscan_gtk_map_steer_carea_check_scale;
}

static void
hyscan_gtk_map_steer_carea_init (HyScanGtkMapSteerCarea *steer_carea)
{
  gtk_widget_set_size_request (GTK_WIDGET (steer_carea), -1, 200);
  gtk_widget_set_margin_top (GTK_WIDGET (steer_carea), 6);

  g_signal_connect_after (steer_carea, "configure-event", G_CALLBACK (hyscan_gtk_map_steer_carea_configure), NULL);
  g_signal_connect (steer_carea, "visible-draw", G_CALLBACK (hyscan_gtk_map_steer_carea_draw), NULL);
  g_signal_connect (steer_carea, "area-draw", G_CALLBACK (hyscan_gtk_map_steer_carea_draw_axis), NULL);
}

static void
hyscan_gtk_map_steer_carea_object_finalize (GObject *object)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (object);

  g_clear_object (&steer_carea->pango_layout);
}

static void
hyscan_gtk_map_steer_class_init (HyScanGtkMapSteerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_steer_set_property;
  object_class->constructed = hyscan_gtk_map_steer_object_constructed;
  object_class->finalize = hyscan_gtk_map_steer_object_finalize;

  g_object_class_install_property (object_class, PROP_STEER,
    g_param_spec_object ("steer", "HyScanSteer", "Steer model",
                         HYSCAN_TYPE_STEER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_steer_init (HyScanGtkMapSteer *gtk_steer)
{
  gtk_steer->priv = hyscan_gtk_map_steer_get_instance_private (gtk_steer);

  gtk_widget_add_events (GTK_WIDGET (gtk_steer), GDK_SCROLL_MASK);
}

static void
hyscan_gtk_map_steer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapSteer *gtk_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;

  switch (prop_id)
    {
    case PROP_STEER:
      priv->steer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_steer_object_constructed (GObject *object)
{
  HyScanGtkMapSteer *gtk_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkBox *box = GTK_BOX (object);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->constructed (object);

  priv->points = g_queue_new ();

  gdk_rgba_parse (&priv->color_bg, "#888888");
  gdk_rgba_parse (&priv->color_bg_good, "#FFFFFF");

  priv->carea = hyscan_gtk_map_steer_create_carea (gtk_steer);

  priv->track_info = gtk_label_new (NULL);
  gtk_widget_set_margin_bottom (priv->track_info, 6);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (gtk_steer), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (box, 6);
  gtk_box_pack_start (box, priv->carea, TRUE, TRUE, 0);
  gtk_box_pack_start (box, priv->track_info, FALSE, TRUE, 0);

  g_signal_connect_swapped (priv->steer, "point", G_CALLBACK (hyscan_gtk_map_steer_nav_changed), gtk_steer);
  g_signal_connect_swapped (priv->steer, "plan-changed", G_CALLBACK (hyscan_gtk_map_steer_set_track), gtk_steer);

  hyscan_gtk_map_steer_set_track (gtk_steer);
}

static void
hyscan_gtk_map_steer_object_finalize (GObject *object)
{
  HyScanGtkMapSteer *gtk_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;

  g_signal_handlers_disconnect_by_data (priv->steer, gtk_steer);
  g_free (priv->track_id);
  hyscan_track_plan_free (priv->plan);
  g_clear_object (&priv->steer);
  g_queue_free_full (priv->points, (GDestroyNotify) hyscan_steer_point_free);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->finalize (object);
}

static GtkWidget *
hyscan_gtk_map_steer_create_carea (HyScanGtkMapSteer *gtk_steer)
{
  HyScanGtkMapSteerCarea *steer_carea;

  steer_carea = g_object_new (HYSCAN_TYPE_GTK_MAP_STEER_CAREA, NULL);
  steer_carea->gtk_steer = gtk_steer;

  return GTK_WIDGET (steer_carea);
}

static void
hyscan_gtk_map_steer_nav_changed (HyScanGtkMapSteer *gtk_steer,
                                  HyScanSteerPoint  *point)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  HyScanSteerPoint *last_point;

  g_queue_push_head (priv->points, hyscan_steer_point_copy (point));

  /* Удаляем устаревшие точки. */
  while ((last_point = g_queue_peek_tail (priv->points)) != NULL)
    {
      if (point->nav_data.time - last_point->nav_data.time < PATH_LIFETIME)
        break;

      last_point = g_queue_pop_tail (priv->points);
      hyscan_steer_point_free (last_point);
    }

  gtk_cifro_area_set_view_center (GTK_CIFRO_AREA (priv->carea), -point->track.y, point->track.x);
  gtk_widget_queue_draw (GTK_WIDGET (gtk_steer));
}

/* Переводит время в минуты и секунды. */
static void
hyscan_gtk_map_steer_time_to_mmss (gint  time,
                                   gint *min,
                                   gint *sec)
{
  *min = time / 60;
  *sec = (time - *min * 60);
}

/* Рисует линию допустимых отклонений поперёк галса. */
static void
hyscan_gtk_map_steer_carea_draw_err_line (HyScanGtkMapSteer *gtk_steer,
                                          cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x0, y0, x1, y1, x2, y2, x3, y3;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale_x, scale_y;
  gdouble threshold;

  cairo_save (cairo);

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);

  /* Линии допустимых отклонений. */
  threshold = hyscan_steer_get_threshold (priv->steer);
  gtk_cifro_area_visible_value_to_point (carea, &x0, &y0, -threshold, from_y);
  gtk_cifro_area_visible_value_to_point (carea, &x1, &y1, -threshold, to_y);
  gtk_cifro_area_visible_value_to_point (carea, &x2, &y2, threshold, from_y);
  gtk_cifro_area_visible_value_to_point (carea, &x3, &y3, threshold, to_y);

  gdk_cairo_set_source_rgba (cairo, &priv->color_bg);
  cairo_paint (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->color_bg_good);
  cairo_rectangle (cairo, x0, y0, threshold / scale_x * 2.0, - (to_y - from_y) / scale_y);
  cairo_fill (cairo);

  cairo_restore (cairo);
}

/* Рисует план галса. */
static void
hyscan_gtk_map_steer_carea_draw_track (HyScanGtkMapSteer *gtk_steer,
                                       cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble start_x, start_y, end_x, end_y;
  HyScanGeoCartesian2D start, end;

  if (!hyscan_steer_get_track_topo (priv->steer, &start, &end))
    return;

  gtk_cifro_area_visible_value_to_point (carea, &start_x, &start_y, -start.y,start.x);
  gtk_cifro_area_visible_value_to_point (carea, &end_x, &end_y, -end.y, end.x);

  cairo_move_to (cairo, start_x, start_y);
  cairo_line_to (cairo, end_x, end_y);

  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.0, 1.0);
  cairo_stroke (cairo);

  cairo_arc (cairo, start_x, start_y, 4.0, 0, 2 * G_PI);
  cairo_fill (cairo);

  cairo_move_to (cairo, end_x - 5.0, end_y);
  cairo_line_to (cairo, end_x, end_y - 10.0);
  cairo_line_to (cairo, end_x + 5.0, end_y);
  cairo_close_path (cairo);
  cairo_fill (cairo);
}

/* Палитра jet.
 * Взято из https://stackoverflow.com/questions/7706339/grayscale-to-red-green-blue-matlab-jet-color-scale */
static inline GdkRGBA
hyscan_gtk_map_steer_value2color (gdouble value,
                                  gdouble value_min,
                                  gdouble value_max)
{
  gdouble dv;
  GdkRGBA color;
  gdouble opacity = 0.8;
  gdouble max_green = 0.8;

  color.red = 1.0;
  color.green = max_green;
  color.blue = 1.0;
  color.alpha = opacity;

  if (value < value_min)
     value = value_min;
  if (value > value_max)
     value = value_max;

  dv = value_max - value_min;
  if (value < value_min + 0.25 * dv)
    {
      color.red = 0;
      color.green = 4 * max_green * (value - value_min) / dv;
    }
  else if (value < (value_min + 0.5 * dv))
    {
      color.red = 0;
      color.blue = 1 + 4 * (value_min + 0.25 * dv - value) / dv;
    }
  else if (value < (value_min + 0.75 * dv))
    {
      color.red = 4 * (value - value_min - 0.5 * dv) / dv;
      color.blue = 0;
    }
  else
    {
      color.green = 1 + 4 * max_green * (value_min + 0.75 * dv - value) / dv;
      color.blue = 0;
    }

  return color;
}

/* Ось координат OX - отклонение поперёк галса. */
static void
hyscan_gtk_map_steer_carea_draw_xaxis (HyScanGtkMapSteerCarea *steer_carea,
                                       cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->gtk_steer->priv;
  HyScanSteerPoint *point;
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale_x, scale_y;
  gdouble step, axis_val, mini_step, start_val;
  const gchar *format;
  gint power;

  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);

  /* Координатные линии по времени и по отклонению. */
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  cairo_save (cairo);
  gtk_cifro_area_value_to_point (carea, &x, &y, 0, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_value_to_point (carea, &x, &y, 0, to_y);
  cairo_line_to (cairo, x, y);
  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.0, 0.5);
  cairo_stroke (cairo);
  cairo_restore (cairo);

  /* Горизонтальная линия сверху. Показывает отклонение последней точки поперёк галса. */
  point = g_queue_peek_head (priv->points);
  if (point != NULL)
    {
      GdkRGBA track_color;
      gdouble line_width, line_length;
      guint width;
      gdouble width2;
      gdouble max_y;

      cairo_save (cairo);

      gtk_cifro_area_get_visible_size (carea, &width, NULL);
      width2 = (gdouble) width / 2.0;
      line_width = steer_carea->border_size / 2.0;
      line_length = CLAMP (point->d_y / scale_x, -width2, width2);
      max_y = 2.0 * hyscan_steer_get_threshold (priv->steer);

      gtk_cifro_area_value_to_point (carea, &x, &y, 0, to_y);
      cairo_move_to (cairo, x, y - line_width / 2.0);
      cairo_line_to (cairo, x + line_length, y - line_width / 2.0);
      track_color = hyscan_gtk_map_steer_value2color (ABS (point->d_y), -max_y, max_y);
      gdk_cairo_set_source_rgba (cairo, &track_color);
      cairo_set_line_width (cairo, line_width);
      cairo_stroke (cairo);
      cairo_restore (cairo);
    }

  /* Шкала оси OX, показывает отклонение поперёк галса. */
  start_val = from_x;
  gtk_cifro_area_get_axis_step (scale_x, 50, &start_val, &step, NULL, &power);
  mini_step = step / 5.0;

  if (power < -1)
    format = "%.2f";
  else if (power == -1)
    format = "%.1f";
  else
    format = "%.0f";

  /* Основные деления с подписью. */
  axis_val = start_val;
  while (axis_val < to_x)
    {
      gchar *label;
      gint label_height, label_width;

      gtk_cifro_area_value_to_point (carea, &x, &y, axis_val, to_y);

      label = g_strdup_printf (format, axis_val);
      pango_layout_set_text (steer_carea->pango_layout, label, -1);
      pango_layout_get_pixel_size (steer_carea->pango_layout, &label_width, &label_height);
      cairo_move_to (cairo, x - label_width / 2.0, 0);
      pango_cairo_show_layout (cairo, steer_carea->pango_layout);
      g_free (label);

      cairo_move_to (cairo, x, steer_carea->border_size);
      cairo_line_to (cairo, x, steer_carea->border_size - 6.0);

      axis_val += step;
    }

  /* Промежуточные деления без подписи. */
  axis_val = start_val - step;
  while (axis_val < to_x)
    {
      if (axis_val > from_x)
        {
          gtk_cifro_area_value_to_point (carea, &x, &y, axis_val, to_y);
          cairo_move_to (cairo, x, steer_carea->border_size);
          cairo_line_to (cairo, x, steer_carea->border_size - 3.0);
        }

      axis_val += mini_step;
    }

  cairo_stroke (cairo);
}

/* Левая ось координат OY - положение вдоль галса. */
static void
hyscan_gtk_map_steer_carea_draw_yaxis (HyScanGtkMapSteerCarea *steer_carea,
                                       cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  gdouble step, axis_val, mini_step, start_val;
  gdouble scale_y;
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (carea, NULL, &scale_y);

  start_val = from_y;
  gtk_cifro_area_get_axis_step (scale_y, 40, &start_val, &step, NULL, NULL);

  /* Основые деления с подписью. */
  axis_val = start_val;
  while (axis_val < to_y)
    {
      gchar *label;
      gint label_height, label_width;

      gtk_cifro_area_value_to_point (carea, &x, &y, from_x, axis_val);

      label = g_strdup_printf ("%.0f", axis_val);
      pango_layout_set_text (steer_carea->pango_layout, label, -1);
      pango_layout_get_pixel_size (steer_carea->pango_layout, &label_width, &label_height);
      cairo_move_to (cairo, 0, y + label_width / 2.0);
      cairo_save (cairo);
      cairo_rotate (cairo, -G_PI_2);
      pango_cairo_show_layout (cairo, steer_carea->pango_layout);
      cairo_restore (cairo);
      g_free (label);

      cairo_move_to (cairo, steer_carea->border_size, y);
      cairo_line_to (cairo, steer_carea->border_size - 6.0, y);

      axis_val += step;
    }

  /* Промежуточные деления без подписи. */
  mini_step = step / 5.0;
  axis_val = start_val - step;
  while (axis_val < to_y)
    {
      if (axis_val > from_y)
        {
          gtk_cifro_area_value_to_point (carea, &x, &y, from_x, axis_val);
          cairo_move_to (cairo, steer_carea->border_size, y);
          cairo_line_to (cairo, steer_carea->border_size - 3.0, y);
        }

      axis_val += mini_step;
    }

  /* Отметка точки начала галса. */
  gtk_cifro_area_value_to_point (carea, &x, &y, from_x, (from_y + to_y) / 2.0);
  cairo_move_to (cairo, x, y);
  cairo_line_to (cairo, x - steer_carea->border_size, y);

  cairo_stroke (cairo);
}

/* Рисует время до конца галса на нижней части рамки. */
static void
hyscan_gtk_map_steer_carea_draw_ttg (HyScanGtkMapSteerCarea *steer_carea,
                                     cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->gtk_steer->priv;
  HyScanSteerPoint *point;
  gchar *text;
  gint ttg, min, sec;
  gint text_width, text_height;
  guint width, height;

  if (priv->plan == NULL)
    return;
  
  point = g_queue_peek_head (priv->points);
  if (point == NULL)
    return;

  gtk_cifro_area_get_size (carea, &width, &height);
  
  ttg = point->time_left;
  hyscan_gtk_map_steer_time_to_mmss (ttg, &min, &sec);
  text = g_strdup_printf ("TTG %02d:%02d", min, sec);
  pango_layout_set_text (steer_carea->pango_layout, text, -1);
  pango_layout_get_pixel_size (steer_carea->pango_layout, &text_width, &text_height);
  cairo_move_to (cairo, width / 2.0 - text_width / 2.0, height - text_height);
  pango_cairo_show_layout (cairo, steer_carea->pango_layout);
  g_free (text);
}

/* Правая ось координат OY - скорость. */
static void
hyscan_gtk_map_steer_carea_draw_speed (HyScanGtkMapSteerCarea *steer_carea,
                                       cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->gtk_steer->priv;
  gdouble plan_speed;
  gdouble from_speed, to_speed;
  gdouble scale;
  gdouble x, y;
  gdouble step, axis_val, mini_step, start_val;
  guint width, height;

  HyScanSteerPoint *point;

  if (priv->plan == NULL)
    return;

  plan_speed = priv->plan->speed;

  /* Границы координат скорости. */
  from_speed = 0.0;
  to_speed = 2.0 * plan_speed;

  /* Масштаб. */
  gtk_cifro_area_get_size (carea, &width, &height);
  height -= 2.0 * steer_carea->border_size;
  scale = (to_speed - from_speed) / height;

  /* Координата x для рисования оси OY. */
  x = width - steer_carea->border_size;

  /* Макрос для перевода скорости в координату Y. */
#define SPEED_TO_Y(speed) (steer_carea->border_size + height - speed / scale)

  /* Рисуем столбец, соответствующий текущей скорости.
   * Цвет столбца определяется отклонением скорости от плана. */
  point = g_queue_peek_head (priv->points);
  if (point != NULL)
    {
      gdouble current_speed;
      gdouble bar_height, bar_width;
      GdkRGBA active_color;

      current_speed = point->nav_data.speed;
      cairo_save (cairo);
      bar_width = steer_carea->border_size / 2.0;
      bar_height = MIN (current_speed / scale, height);
      cairo_rectangle (cairo, x, SPEED_TO_Y (0), bar_width, -bar_height);

      active_color = hyscan_gtk_map_steer_value2color (ABS (current_speed - plan_speed),
                                                       -0.5 * plan_speed,
                                                       0.5 * plan_speed);
      gdk_cairo_set_source_rgba (cairo, &active_color);
      cairo_fill (cairo);
      cairo_restore (cairo);
    }

  /* Основные деления. */
  start_val = from_speed;
  gtk_cifro_area_get_axis_step (scale, 40, &start_val, &step, NULL, NULL);
  axis_val = start_val;
  while (axis_val <= to_speed)
    {
      gchar *label;
      gint label_height, label_width;

      y = SPEED_TO_Y (axis_val);

      label = g_strdup_printf ("%.1f", axis_val);
      pango_layout_set_text (steer_carea->pango_layout, label, -1);
      pango_layout_get_pixel_size (steer_carea->pango_layout, &label_width, &label_height);
      cairo_move_to (cairo, width - label_height, y + label_width / 2.0);
      cairo_save (cairo);
      cairo_rotate (cairo, -G_PI_2);
      pango_cairo_show_layout (cairo, steer_carea->pango_layout);
      cairo_restore (cairo);
      g_free (label);

      cairo_move_to (cairo, x, y);
      cairo_line_to (cairo, x + 6.0, y);

      axis_val += step;
    }

  /* Промежуточные деления. */
  mini_step = step / 5.0;
  axis_val = start_val - step;
  while (axis_val <= to_speed)
    {
      if (axis_val > from_speed)
        {
          y = SPEED_TO_Y (axis_val);

          cairo_move_to (cairo, x, y);
          cairo_line_to (cairo, x + 3.0, y);
        }

      axis_val += mini_step;
    }

  /* Отметка плановой скорости. */
  y = SPEED_TO_Y (plan_speed);
  cairo_move_to (cairo, x, y);
  cairo_line_to (cairo, x + steer_carea->border_size, y);

  cairo_stroke (cairo);
}

/* Функция рисует на рамке мини-карты шкалы с отклонениями по скорости, положению,
 * и статус прохождения галса. */
static void
hyscan_gtk_map_steer_carea_draw_axis (GtkCifroArea *carea,
                                      cairo_t      *cairo,
                                      gpointer      user_data)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (carea);
  GdkRGBA color;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (carea));
  gtk_style_context_get_color (context,
                               gtk_style_context_get_state (context),
                               &color);
  gdk_cairo_set_source_rgba (cairo, &color);
  cairo_set_line_width (cairo, 0.5);

  hyscan_gtk_map_steer_carea_draw_xaxis (steer_carea, cairo);
  hyscan_gtk_map_steer_carea_draw_yaxis (steer_carea, cairo);
  hyscan_gtk_map_steer_carea_draw_speed (steer_carea, cairo);
  hyscan_gtk_map_steer_carea_draw_ttg (steer_carea, cairo);
}

/* Функция рисует центральную линию вдоль направления плана галса. */
static void
hyscan_gtk_map_steer_carea_draw_center (HyScanGtkMapSteer *gtk_steer,
                                        cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  gdouble from_y, to_y;

  gtk_cifro_area_get_view (carea, NULL, NULL, &from_y, &to_y);

  /* Центральная линия. */
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, to_y);
  cairo_line_to (cairo, x, y);

  cairo_set_source_rgba (cairo, 0.0, 0.0, 0.0, 0.5);
  cairo_set_line_width (cairo, 0.5);
  cairo_stroke (cairo);
}

/* Рисует на мини-карте текущее положение судна. */
static void
hyscan_gtk_map_steer_carea_draw_current (HyScanGtkMapSteer *gtk_steer,
                                         cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;
  HyScanSteerPoint *point = priv->points->head->data;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  /* Маленькая стрелка. */
  cairo_save (cairo);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -point->position.y, point->position.x);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, point->d_angle);

  cairo_move_to (cairo, 0, 0);
  cairo_line_to (cairo, -9.0, 5.0);
  cairo_line_to (cairo, 0, -ARROW_SIZE);
  cairo_line_to (cairo, 9.0, 5.0);
  cairo_close_path (cairo);
  cairo_set_source_rgba (cairo, 1.0, 1.0, 1.0, 0.5);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
  cairo_stroke (cairo);

  cairo_move_to (cairo, 0, -ARROW_SIZE);
  cairo_line_to (cairo, 0, -3.0 * ARROW_SIZE);
  cairo_stroke (cairo);

  cairo_restore (cairo);
}

/* Рисует на мини-карте траекторию судна. */
static void
hyscan_gtk_map_steer_carea_draw_path (HyScanGtkMapSteer *gtk_steer,
                                      cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  GList *link;

  cairo_set_line_width (cairo, 2.0);
  for (link = priv->points->head; link != NULL; link = link->next)
    {
      HyScanSteerPoint *point = link->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, -point->position.y, point->position.x);
      cairo_line_to (cairo, x, y);
    }

  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
  cairo_stroke (cairo);
}

/* Обработчик "visible-draw". */
static void
hyscan_gtk_map_steer_carea_draw (GtkCifroArea *carea,
                                 cairo_t      *cairo,
                                 gpointer      user_data)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (carea);
  HyScanGtkMapSteer *gtk_steer = steer_carea->gtk_steer;
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  guint width, height;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (carea));

  gtk_cifro_area_get_visible_size (carea, &width, &height);

  gtk_render_background (context, cairo, 0, 0, width, height);

  cairo_save (cairo);

  hyscan_gtk_map_steer_carea_draw_err_line (gtk_steer, cairo);
  hyscan_gtk_map_steer_carea_draw_center (gtk_steer, cairo);
  hyscan_gtk_map_steer_carea_draw_track (gtk_steer, cairo);

  if (g_queue_get_length (priv->points) == 0)
    goto exit;

  hyscan_gtk_map_steer_carea_draw_path (gtk_steer, cairo);
  hyscan_gtk_map_steer_carea_draw_current (gtk_steer, cairo);

exit:
  cairo_restore (cairo);
}

/* Реализация функции GtkWidgetClass.screen_changed. */
static void
hyscan_gtk_map_steer_carea_screen_changed (GtkWidget *widget,
                                           GdkScreen *previous_screen)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (widget);
  gint height;

  g_clear_object (&steer_carea->pango_layout);
  steer_carea->pango_layout = gtk_widget_create_pango_layout (widget, "0123456789.");
  pango_layout_get_pixel_size (steer_carea->pango_layout, NULL, &height);

  steer_carea->border_size = height * 1.5;
}

/* Обработчик "configure-event". */
static gboolean
hyscan_gtk_map_steer_carea_configure (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   user_data)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (widget);
  HyScanGtkMapSteerPrivate *priv = steer_carea->gtk_steer->priv;
  guint width, height;
  gdouble size;

  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (widget), &width, &height);
  if (width == 0 || height == 0)
    return FALSE;
  
  size = 2.0 * hyscan_steer_get_threshold (priv->steer);

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (widget), -size, size, -size, size);
  g_signal_handlers_disconnect_by_func (widget, hyscan_gtk_map_steer_carea_configure, user_data);

  return FALSE;
}

/* Реализация GtkWidgetClass.scroll_event. */
static gboolean
hyscan_gtk_map_steer_carea_scroll (GtkWidget      *widget,
                                   GdkEventScroll *event)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (widget);
  HyScanGtkMapSteer *gtk_steer = steer_carea->gtk_steer;
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  GtkCifroAreaZoomType zoom;
  gdouble from_x, to_x, from_y, to_y;

  if (event->direction == GDK_SCROLL_UP)
    zoom = GTK_CIFRO_AREA_ZOOM_IN;
  else if (event->direction == GDK_SCROLL_DOWN)
    zoom = GTK_CIFRO_AREA_ZOOM_OUT;
  else
    return GDK_EVENT_PROPAGATE;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_zoom (carea, zoom, zoom, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  return GDK_EVENT_STOP;
}

/* Определяет, находится ли точка (x, y) на линии допустимого отклонения, чтобы захватить её мышкой. */
static gboolean
hyscan_gtk_map_steer_is_err_line (HyScanGtkMapSteer *gtk_steer,
                                  gdouble            x,
                                  gdouble            y)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  gdouble val_x, val_y;
  gdouble scale_x;
  gdouble threshold;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->carea), x, y, &val_x, &val_y);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->carea), &scale_x, NULL);
  threshold = hyscan_steer_get_threshold (priv->steer);

  return (ABS (ABS (val_x) - threshold) < scale_x * 5.0);
}

/* Реализация GtkWidgetClass.button_press_event.
 * Обрабатывает захват и перемещение линии допустимых значений. */
static gboolean
hyscan_gtk_map_steer_carea_button (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  HyScanGtkMapSteer *gtk_steer = HYSCAN_GTK_MAP_STEER_CAREA (widget)->gtk_steer;
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (!hyscan_gtk_map_steer_is_err_line (gtk_steer, event->x, event->y))
        return GDK_EVENT_PROPAGATE;

      priv->state = STATE_GRAB;
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      if (priv->state != STATE_GRAB)
        return GDK_EVENT_PROPAGATE;

      priv->state = STATE_NONE;
    }

  return GDK_EVENT_STOP;
}

static gboolean
hyscan_gtk_map_steer_carea_motion_notify (GtkWidget      *widget,
                                          GdkEventMotion *event)
{
  HyScanGtkMapSteer *gtk_steer = HYSCAN_GTK_MAP_STEER_CAREA (widget)->gtk_steer;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GdkWindow *window;
  GdkCursor *cursor;

  /* Меняем значение threshold, если пользователь перетаскивает линию допустимого отклонения. */
  if (priv->state == STATE_GRAB)
    {
      gdouble x;

      gtk_cifro_area_point_to_value (carea, event->x, event->y, &x, NULL);
      hyscan_steer_set_threshold (priv->steer, ABS (x));

      gtk_widget_queue_draw (GTK_WIDGET (gtk_steer));
    }

  /* Устанавливаем курсор мыши. */
  cursor = hyscan_gtk_map_steer_is_err_line (gtk_steer, event->x, event->y) ?
           gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (gtk_steer)), "col-resize") :
           NULL;
  window = gtk_widget_get_window (GTK_WIDGET (gtk_steer));
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);

  return GDK_EVENT_PROPAGATE;
}

/* Функция возвращает размер рамки мини-карты. */
static void
hyscan_gtk_map_steer_carea_get_border (GtkCifroArea *carea,
                                       guint        *border_top,
                                       guint        *border_bottom,
                                       guint        *border_left,
                                       guint        *border_right)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (carea);

  border_top != NULL ? (*border_top = steer_carea->border_size) : 0;
  border_bottom != NULL ? (*border_bottom = steer_carea->border_size) : 0;
  border_left != NULL ? (*border_left = steer_carea->border_size) : 0;
  border_right != NULL ? (*border_right = steer_carea->border_size) : 0;
}

/* Функция проверяет масштаб мини-карты. */
static void
hyscan_gtk_map_steer_carea_check_scale (GtkCifroArea *carea,
                                        gdouble      *scale_x,
                                        gdouble      *scale_y)
{
  *scale_x = CLAMP (*scale_x, 0.01, 1.0);
  *scale_y = *scale_x;
}

/* Функция обновляет активный план галса. */
static void
hyscan_gtk_map_steer_set_track (HyScanGtkMapSteer *gtk_steer)
{
  HyScanGtkMapSteerPrivate *priv = gtk_steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  const HyScanPlannerTrack *track = NULL;
  gchar *active_id;

  track = hyscan_steer_get_track (priv->steer, &active_id);

  /* Если план галса поменялся, то очищаем траекторию и перемещаемся в начало плана. */
  if (g_strcmp0 (active_id, priv->track_id) != 0)
    {
      g_queue_clear (priv->points);
      gtk_cifro_area_set_view_center (carea, 0, 0);
    }

  g_free (priv->track_id);
  priv->track_id = active_id;
  hyscan_track_plan_free (priv->plan);
  priv->plan = track != NULL ? hyscan_track_plan_copy (&track->plan) : NULL;

  if (priv->plan == NULL)
    {
      gtk_label_set_text (GTK_LABEL (priv->track_info), _("No active track"));
      goto exit;
    }

  /* Т.к. план поменялся, то и координаты траектории относительно плана галса тоже поменялись. Пересчитываем их. */
  g_queue_foreach (priv->points, (GFunc) hyscan_steer_calc_point, priv->steer);

  /* Выводим информацию о плане галса. */
  {
    gchar *text;
    gdouble length, angle;
    gdouble estimated_time;
    gint min, sec;

    hyscan_steer_get_track_info (priv->steer, &angle, &length);
    estimated_time = length / priv->plan->speed;
    hyscan_gtk_map_steer_time_to_mmss (estimated_time, &min, &sec);
    text = g_strdup_printf (_("Plan: %.1f m/s %.0f° L%.0fm ~%02d:%02d"),
                            priv->plan->speed, angle, length, min, sec);
    gtk_label_set_text (GTK_LABEL (priv->track_info), text);
    g_free (text);
  }

exit:
  gtk_widget_queue_draw (GTK_WIDGET (priv->carea));
}

/**
 * hyscan_gtk_map_steer_new:
 * @steer: модель управления судном #HyScanSteer
 *
 * Создаёт виджет навигации по запланированному галсу
 *
 * Returns: новый виджет #HyScanGtkMapSteer
 */
GtkWidget *
hyscan_gtk_map_steer_new (HyScanSteer *steer)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_STEER,
                       "steer", steer,
                       NULL);
}

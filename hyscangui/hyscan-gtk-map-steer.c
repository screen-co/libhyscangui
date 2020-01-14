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
 * галсу. В виджете отображаются:
 * - плановые курс и скорсть для текущего галса,
 * - отклонения по положению и скорости,
 * - миникарта с положением антенны гидролокатора относительно планового галса.
 *
 * При записи галса важно именно положение гидролокатора, а не судна как такового,
 * поэтому в виджете есть возможность вносить поправку на смещение антенны с
 * помощью функции hyscan_gtk_map_steer_sensor_set_offset().
 *
 * На миникарте отображается стрелка, соответствующая положению антенны и курсу судна.
 *
 */

#include "hyscan-gtk-map-steer.h"
#include <math.h>
#include <hyscan-cartesian.h>
#include <glib/gi18n-lib.h>

#define DEG2RAD(deg) (deg / 180. * G_PI)
#define RAD2DEG(rad) (rad / G_PI * 180.)

#define HYSCAN_TYPE_GTK_MAP_STEER_CAREA             (hyscan_gtk_map_steer_carea_get_type ())
#define HYSCAN_GTK_MAP_STEER_CAREA(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCarea))
#define HYSCAN_IS_GTK_MAP_STEER_CAREA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA))
#define HYSCAN_GTK_MAP_STEER_CAREA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCareaClass))
#define HYSCAN_IS_GTK_MAP_STEER_CAREA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_STEER_CAREA))
#define HYSCAN_GTK_MAP_STEER_CAREA_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_STEER_CAREA, HyScanGtkMapSteerCareaClass))

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

enum
{
  SIGNAL_START,
  SIGNAL_STOP,
  SIGNAL_LAST,
};

enum
{
  STATE_NONE,
  STATE_GRAB
};

enum
{
  MODE_ORIGIN,
  MODE_VESSEL
};

typedef struct
{
  HyScanNavModelData   nav_data;     /* Исходные навигационные данные о положении и скорости судна. */
  HyScanGeoCartesian2D position;     /* Положение судна. */
  HyScanGeoCartesian2D track;        /* Проекция положения судна на галс. */
  gdouble              d_y;          /* Отклонение положения антенны по расстоянию, м. */
  gdouble              d_angle;      /* Отклонение курса судна по углу, рад. */
  gdouble              time_left;    /* Отклонение судна по скорости, м/с. */
} HyScanGtkMapSteerPoint;

typedef struct
{
  GtkCifroArea       parent_instance;

  HyScanGtkMapSteer *steer;          /* Указатель на основной виджет. */
  PangoLayout       *pango_layout;   /* Шрифт. */
  gint               border_size;    /* Размер окантовки. */
} HyScanGtkMapSteerCarea;

typedef struct
{
  GtkCifroAreaClass parent_instance;
} HyScanGtkMapSteerCareaClass;

struct _HyScanGtkMapSteerPrivate
{
  HyScanNavModel             *nav_model;         /* Модель навигационных данных. */
  HyScanPlannerSelection     *selection;         /* Модель выбранных объектов планировщика. */
  HyScanObjectModel          *planner_model;     /* Модель объектов планировщика. */
  HyScanGeo                  *geo;               /* Объект перевода географических координат. */
  HyScanAntennaOffset        *offset;            /* Смещение антенны гидролокатора. */

  guint                       state;             /* Статус перетаскивания границы допустимого отклонения. */
  guint                       mode;              /* Режим отображения: судно в центре или галс в центре. */

  gdouble                     threshold_x;       /* Допустимое отклонение. */

  GQueue                     *points;            /* Список точек HyScanGtkMapSteerPoint. */
  gdouble                     time_span;         /* Промежуток времени, в течение которого показываются точки, секунды. */
  gdouble                     end_time;          /* Время последней точки. */

  HyScanPlannerTrack         *track;             /* Активный галс. */
  gchar                      *track_id;          /* Ид активного галса. */
  HyScanGeoCartesian2D        start;             /* Точка начала. */
  HyScanGeoCartesian2D        end;               /* Точка окончания. */
  gdouble                     length;            /* Длина галса. */
  gdouble                     angle;             /* Целевой азимут. */

  GtkWidget                  *track_info;        /* Виджет с информацией о текущем галсе. */
  GtkWidget                  *carea;             /* Виджет миникарты с траекторией. */

  GdkRGBA                     color_bg;          /* Цвет фона области рисования. */
  GdkRGBA                     color_bg_good;     /* Цвет фона "в пределах нормы". */
  gdouble                     arrow_size;        /* Размер стрелки судна. */

  gdouble                     recording;         /* Признак того, что идёт запись. */
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
static void           hyscan_gtk_map_steer_carea_draw_path                (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_current             (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_axis                (GtkCifroArea             *carea,
                                                                           cairo_t                  *cairo,
                                                                           gpointer                  user_data);
static void           hyscan_gtk_map_steer_carea_draw_center              (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_err_line            (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static gboolean       hyscan_gtk_map_steer_is_err_line                    (HyScanGtkMapSteer        *steer,
                                                                           gdouble                   x,
                                                                           gdouble                   y);

static GtkWidget *    hyscan_gtk_map_steer_create_carea                   (HyScanGtkMapSteer        *steer);

static void           hyscan_gtk_map_steer_nav_changed                    (HyScanGtkMapSteer        *steer);
static void           hyscan_gtk_map_steer_set_track                      (HyScanGtkMapSteer        *steer);
static HyScanGtkMapSteerPoint *  hyscan_gtk_map_steer_point_new           (void);
static void           hyscan_gtk_map_steer_point_free                     (gpointer                  point);

static guint hyscan_gtk_map_steer_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapSteer, hyscan_gtk_map_steer, GTK_TYPE_GRID)
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


  /**
   * HyScanGtkMapSteer::start:
   * @planner: указатель на #HyScanGtkMapPlanner
   *
   * Сигнал посылается при необходимости начать запись галса.
   */
  hyscan_gtk_map_steer_signals[SIGNAL_START] =
    g_signal_new ("start", HYSCAN_TYPE_GTK_MAP_STEER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * HyScanGtkMapSteer::stop:
   * @planner: указатель на #HyScanGtkMapPlanner
   *
   * Сигнал посылается при необходимости остановить запись галса.
   */
  hyscan_gtk_map_steer_signals[SIGNAL_STOP] =
    g_signal_new ("stop", HYSCAN_TYPE_GTK_MAP_STEER, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_gtk_map_steer_carea_init (HyScanGtkMapSteerCarea *steer_carea)
{
  gtk_widget_set_size_request (GTK_WIDGET (steer_carea), -1, 200);
  gtk_widget_set_margin_top (GTK_WIDGET (steer_carea), 10);

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

static void
hyscan_gtk_map_steer_carea_check_scale (GtkCifroArea *carea,
                                        gdouble      *scale_x,
                                        gdouble      *scale_y)
{
  *scale_x = CLAMP (*scale_x, 0.01, 1.0);
  *scale_y = *scale_x;
}

static void
hyscan_gtk_map_steer_class_init (HyScanGtkMapSteerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_steer_set_property;
  object_class->constructed = hyscan_gtk_map_steer_object_constructed;
  object_class->finalize = hyscan_gtk_map_steer_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanNavModel", "Navigation model",
                         HYSCAN_TYPE_NAV_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SELECTION,
    g_param_spec_object ("selection", "HyScanPlannerSelection", "Model of selected planner objects",
                         HYSCAN_TYPE_PLANNER_SELECTION,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_steer_init (HyScanGtkMapSteer *steer)
{
  steer->priv = hyscan_gtk_map_steer_get_instance_private (steer);

  gtk_widget_add_events (GTK_WIDGET (steer), GDK_SCROLL_MASK);
}

static void
hyscan_gtk_map_steer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapSteer *gtk_map_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_map_steer->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->nav_model = g_value_dup_object (value);
      break;

    case PROP_SELECTION:
      priv->selection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_steer_object_constructed (GObject *object)
{
  HyScanGtkMapSteer *steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkGrid *grid = GTK_GRID (object);
  gint i = 0;

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->constructed (object);

  priv->planner_model = HYSCAN_OBJECT_MODEL (hyscan_planner_selection_get_model (priv->selection));

  priv->points = g_queue_new ();
  priv->time_span = 100.0;
  priv->threshold_x = 2.0;
  priv->arrow_size = 30.0;

  gdk_rgba_parse (&priv->color_bg, "#888888");
  gdk_rgba_parse (&priv->color_bg_good, "#FFFFFF");

  priv->carea = hyscan_gtk_map_steer_create_carea (steer);

  priv->track_info = gtk_label_new (NULL);
  gtk_widget_set_margin_bottom (priv->track_info, 6);

  i = -1;
  gtk_grid_attach (grid, priv->carea,         0, ++i, 1, 1);
  gtk_grid_attach (grid, priv->track_info,    0, ++i, 1, 1);

  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_steer_nav_changed), steer);
  g_signal_connect_swapped (priv->planner_model, "changed", G_CALLBACK (hyscan_gtk_map_steer_set_track), steer);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_steer_set_track), steer);

  hyscan_gtk_map_steer_set_track (steer);
  gtk_grid_set_column_homogeneous (grid, TRUE);
  gtk_grid_set_row_spacing (grid, 3);
}

static void
hyscan_gtk_map_steer_object_finalize (GObject *object)
{
  HyScanGtkMapSteer *gtk_map_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_map_steer->priv;

  g_free (priv->track_id);
  hyscan_planner_track_free (priv->track);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->selection);
  g_queue_free_full (priv->points, hyscan_gtk_map_steer_point_free);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->finalize (object);
}

static GtkWidget *
hyscan_gtk_map_steer_create_carea (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerCarea *steer_carea;

  steer_carea = g_object_new (HYSCAN_TYPE_GTK_MAP_STEER_CAREA, NULL);
  steer_carea->steer = steer;

  return GTK_WIDGET (steer_carea);
}

/* Определяет координаты судна в системе координат планового галса. */
static void
hyscan_gtk_map_steer_calc_point (HyScanGtkMapSteerPoint *point,
                                 HyScanGtkMapSteer      *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  gdouble distance;
  gint side;
  gdouble distance_to_end;
  HyScanGeoCartesian2D track_end;
  HyScanGeoCartesian2D ship_pos;

  point->d_angle = point->nav_data.heading - priv->angle / 180 * G_PI;

  if (!hyscan_geo_geo2topoXY (priv->geo, &ship_pos, point->nav_data.coord))
    return;

  /* Смещение антенны учитываем только для рассчёта её местоположения.
   * В качестве курса оставляем курс судна, т.к. иначе показания виджета становятся неочевидными. */
  if (priv->offset != NULL)
    {
      gdouble cosa, sina;

      cosa = cos (DEG2RAD (point->d_angle));
      sina = sin (DEG2RAD (point->d_angle));
      point->position.x = ship_pos.x - priv->offset->starboard * sina + priv->offset->forward * cosa;
      point->position.y = ship_pos.y - priv->offset->starboard * cosa - priv->offset->forward * sina;
    }
  else
    {
      point->position = ship_pos;
    }

  distance = hyscan_cartesian_distance_to_line (&priv->start, &priv->end, &point->position, &point->track);
  side = hyscan_cartesian_side (&priv->start, &priv->end, &point->position);
  point->d_y = side * distance;

  hyscan_geo_geo2topoXY (priv->geo, &track_end, priv->track->plan.end);
  distance_to_end = hyscan_cartesian_distance (&track_end, &point->track);
  point->time_left = distance_to_end / priv->track->plan.velocity;
}

static void
hyscan_gtk_map_steer_nav_changed (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  HyScanNavModelData data;
  HyScanGtkMapSteerPoint *point;
  HyScanGtkMapSteerPoint *last_point;

  if (!hyscan_nav_model_get (priv->nav_model, &data, NULL))
    return;

  if (priv->geo == NULL)
    return;

  point = hyscan_gtk_map_steer_point_new ();
  point->nav_data = data;
  hyscan_gtk_map_steer_calc_point (point, steer);

  g_queue_push_head (priv->points, point);
  priv->end_time = data.time;

  if (priv->mode == MODE_VESSEL)
    {
      gtk_cifro_area_set_view_center (GTK_CIFRO_AREA (priv->carea), point->d_y, 0);
      gtk_cifro_area_set_angle (GTK_CIFRO_AREA (priv->carea), -point->d_angle);
    }

  while ((last_point = g_queue_peek_tail (priv->points)) != NULL)
    {
      if (priv->end_time - last_point->nav_data.time < priv->time_span)
        break;

      last_point = g_queue_pop_tail (priv->points);
      hyscan_gtk_map_steer_point_free (last_point);
    }

  /* Отправляем сигналы "start" и "stop" для начала и завершения записи галса. */
  if (!priv->recording)
    {
      gdouble speed_along;

      /* Начинаем запись при выполнении условий:
       * (1) движение по направлению галса,
       * (2) положение вдоль галса в интервале (-speed_along * time, track_length),
       * (3) расстояние до линии галса меньше заданного значения. */
      speed_along = point->nav_data.speed * cos (point->d_angle);
      if (speed_along > 0 &&                                                        /* (1) */
          -speed_along < point->position.x && point->position.x < priv->length &&   /* (2) */
          ABS (point->d_y) < priv->threshold_x)                                     /* (3) */
        {
          g_signal_emit (steer, hyscan_gtk_map_steer_signals[SIGNAL_START], 0);
        }
    }
  else
    {
      if (point->position.x > priv->length)
        g_signal_emit (steer, hyscan_gtk_map_steer_signals[SIGNAL_STOP], 0);
    }

  gtk_cifro_area_set_view_center (GTK_CIFRO_AREA (priv->carea), -point->track.y, point->track.x);
  gtk_widget_queue_draw (GTK_WIDGET (steer));
}

static HyScanGtkMapSteerPoint *
hyscan_gtk_map_steer_point_new (void)
{
  return g_slice_new0 (HyScanGtkMapSteerPoint);
}

static void
hyscan_gtk_map_steer_point_free (gpointer point)
{
  g_slice_free (HyScanGtkMapSteerPoint, point);
}

static void
hyscan_gtk_map_steer_time_to_mmss (gint  time,
                                   gint *min,
                                   gint *sec)
{
  *min = time / 60;
  *sec = (time - *min * 60);
}

static void
hyscan_gtk_map_steer_carea_draw_err_line (HyScanGtkMapSteer *steer,
                                          cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x0, y0, x1, y1, x2, y2, x3, y3;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale_x, scale_y;

  cairo_save (cairo);

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);

  /* Линии допустимых отклонений. */
  gtk_cifro_area_visible_value_to_point (carea, &x0, &y0, -priv->threshold_x, from_y);
  gtk_cifro_area_visible_value_to_point (carea, &x1, &y1, -priv->threshold_x, to_y);
  gtk_cifro_area_visible_value_to_point (carea, &x2, &y2, priv->threshold_x, from_y);
  gtk_cifro_area_visible_value_to_point (carea, &x3, &y3, priv->threshold_x, to_y);

  gdk_cairo_set_source_rgba (cairo, &priv->color_bg);
  cairo_paint (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->color_bg_good);
  cairo_rectangle (cairo, x0, y0, priv->threshold_x / scale_x * 2.0, - (to_y - from_y) / scale_y);
  cairo_fill (cairo);

  cairo_restore (cairo);
}

static void
hyscan_gtk_map_steer_carea_draw_track (HyScanGtkMapSteer *steer,
                                       cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble start_x, start_y, end_x, end_y;

  if (priv->track == NULL)
    return;

  gtk_cifro_area_visible_value_to_point (carea, &start_x, &start_y, -priv->start.y, priv->start.x);
  gtk_cifro_area_visible_value_to_point (carea, &end_x, &end_y, -priv->end.y, priv->end.x);

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

/* Координата Y - скорость. */
static void
hyscan_gtk_map_steer_carea_draw_xaxis (HyScanGtkMapSteerCarea *steer_carea,
                                       cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->steer->priv;
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

  /* Линия отклонения. */
  HyScanGtkMapSteerPoint *point;

  point = g_queue_peek_head (priv->points);
  if (point != NULL)
    {
      GdkRGBA track_color;
      gdouble line_width, line_length;
      guint width;
      gdouble width2;

      cairo_save (cairo);

      gtk_cifro_area_get_visible_size (carea, &width, NULL);
      width2 = (gdouble) width / 2.0;
      line_width = steer_carea->border_size / 2.0;
      line_length = CLAMP (point->d_y / scale_x, -width2, width2);

      gtk_cifro_area_value_to_point (carea, &x, &y, 0, to_y);
      cairo_move_to (cairo, x, y - line_width / 2.0);
      cairo_line_to (cairo, x + line_length, y - line_width / 2.0);
      track_color = hyscan_gtk_map_steer_value2color (ABS (point->d_y),
                                                      -2.0 * priv->threshold_x,
                                                      2.0 * priv->threshold_x);
      gdk_cairo_set_source_rgba (cairo, &track_color);
      cairo_set_line_width (cairo, line_width);
      cairo_stroke (cairo);
      cairo_restore (cairo);
    }

  start_val = from_x;
  gtk_cifro_area_get_axis_step (scale_x, 50, &start_val, &step, NULL, &power);
  mini_step = step / 5.0;

  if (power < -1)
    format = "%.2f";
  else if (power == -1)
    format = "%.1f";
  else
    format = "%.0f";

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

/* Координата Y - скорость. */
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

  /* Отметка центральной точки. */
  gtk_cifro_area_value_to_point (carea, &x, &y, from_x, (from_y + to_y) / 2.0);
  cairo_move_to (cairo, x, y);
  cairo_line_to (cairo, x - steer_carea->border_size, y);

  cairo_stroke (cairo);
}

/* Координата Y - скорость. */
static void
hyscan_gtk_map_steer_carea_draw_ttg (HyScanGtkMapSteerCarea *steer_carea,
                                     cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->steer->priv;
  HyScanGtkMapSteerPoint *point;
  gchar *text;
  gint ttg, min, sec;
  gint text_width, text_height;
  guint width, height;

  
  if (priv->track == NULL)
    return;
  
  point = g_queue_peek_head (priv->points);
  if (point == NULL)
    return;

  gtk_cifro_area_get_size (carea, &width, &height);
  
  ttg = hyscan_cartesian_distance (&priv->end, &point->position) / priv->track->plan.velocity;
  hyscan_gtk_map_steer_time_to_mmss (ttg, &min, &sec);
  text = g_strdup_printf ("TTG %02d:%02d", min, sec);
  pango_layout_set_text (steer_carea->pango_layout, text, -1);
  pango_layout_get_pixel_size (steer_carea->pango_layout, &text_width, &text_height);
  cairo_move_to (cairo, width / 2.0 - text_width / 2.0, height - text_height);
  pango_cairo_show_layout (cairo, steer_carea->pango_layout);
  g_free (text);
}

/* Координата Y - скорость. */
static void
hyscan_gtk_map_steer_carea_draw_speed (HyScanGtkMapSteerCarea *steer_carea,
                                       cairo_t                *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (steer_carea);
  HyScanGtkMapSteerPrivate *priv = steer_carea->steer->priv;
  gdouble from_speed, to_speed;
  gdouble scale;
  gdouble x, y;
  gdouble step, axis_val, mini_step, start_val;
  guint width, height;

  HyScanGtkMapSteerPoint *point;

  if (priv->track == NULL)
    return;

  point = g_queue_peek_head (priv->points);

  gtk_cifro_area_get_size (carea, &width, &height);
  height -= 2.0 * steer_carea->border_size;

  from_speed = 0.0;
  to_speed = 2.0 * priv->track->plan.velocity;
  scale = (to_speed - from_speed) / height;

  x = width - steer_carea->border_size;
#define SPEED_TO_Y(speed) (steer_carea->border_size + height - speed / scale)

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

      active_color = hyscan_gtk_map_steer_value2color (ABS (current_speed - priv->track->plan.velocity),
                                                       -0.5 * priv->track->plan.velocity,
                                                       0.5 * priv->track->plan.velocity);
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
  y = SPEED_TO_Y (priv->track->plan.velocity);
  cairo_move_to (cairo, x, y);
  cairo_line_to (cairo, x + steer_carea->border_size, y);

  cairo_stroke (cairo);
}

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

static void
hyscan_gtk_map_steer_carea_draw_center (HyScanGtkMapSteer *steer,
                                        cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
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

static void
hyscan_gtk_map_steer_carea_draw_current (HyScanGtkMapSteer *steer,
                                         cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;
  HyScanGtkMapSteerPoint *point = priv->points->head->data;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  /* Маленькая стрелка. */
  cairo_save (cairo);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -point->position.y, point->position.x);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, point->d_angle);

  cairo_move_to (cairo, 0, 0);
  cairo_line_to (cairo, -9.0, 5.0);
  cairo_line_to (cairo, 0, -priv->arrow_size);
  cairo_line_to (cairo, 9.0, 5.0);
  cairo_close_path (cairo);
  cairo_set_source_rgba (cairo, 1.0, 1.0, 1.0, 0.5);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
  cairo_stroke (cairo);

  cairo_move_to (cairo, 0, -priv->arrow_size);
  cairo_line_to (cairo, 0, -3.0 * priv->arrow_size);
  cairo_stroke (cairo);

  cairo_restore (cairo);
}

static void
hyscan_gtk_map_steer_carea_draw_path (HyScanGtkMapSteer *steer,
                                      cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  GList *link;

  cairo_set_line_width (cairo, 2.0);
  for (link = priv->points->head; link != NULL; link = link->next)
    {
      HyScanGtkMapSteerPoint *point = link->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, -point->position.y, point->position.x);
      cairo_line_to (cairo, x, y);
    }

  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
  cairo_stroke (cairo);
}

static void
hyscan_gtk_map_steer_carea_draw (GtkCifroArea *carea,
                                 cairo_t      *cairo,
                                 gpointer      user_data)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (carea);
  HyScanGtkMapSteer *steer = steer_carea->steer;
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  guint width, height;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (carea));

  gtk_cifro_area_get_visible_size (carea, &width, &height);

  gtk_render_background (context, cairo, 0, 0, width, height);

  cairo_save (cairo);

  hyscan_gtk_map_steer_carea_draw_err_line (steer, cairo);
  hyscan_gtk_map_steer_carea_draw_center (steer, cairo);
  hyscan_gtk_map_steer_carea_draw_track (steer, cairo);

  if (g_queue_get_length (priv->points) == 0)
    goto exit;

  hyscan_gtk_map_steer_carea_draw_path (steer, cairo);
  hyscan_gtk_map_steer_carea_draw_current (steer, cairo);

exit:
  cairo_restore (cairo);
}

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

static gboolean
hyscan_gtk_map_steer_carea_configure (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   user_data)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (widget);
  HyScanGtkMapSteerPrivate *priv = steer_carea->steer->priv;
  guint width, height;

  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (widget), &width, &height);
  if (width == 0 || height == 0)
    return FALSE;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (widget),
                           -priv->threshold_x * 2.0, priv->threshold_x * 2.0,
                           -priv->threshold_x * 2.0, priv->threshold_x * 2.0);
  g_signal_handlers_disconnect_by_func (widget, hyscan_gtk_map_steer_carea_configure, user_data);

  return FALSE;
}

static gboolean
hyscan_gtk_map_steer_carea_scroll (GtkWidget      *widget,
                                   GdkEventScroll *event)
{
  HyScanGtkMapSteerCarea *steer_carea = HYSCAN_GTK_MAP_STEER_CAREA (widget);
  HyScanGtkMapSteer *steer = steer_carea->steer;
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  GtkCifroAreaZoomType zoom;
  gdouble from_x, to_x, from_y, to_y;

  if (event->direction == GDK_SCROLL_UP)
    zoom = GTK_CIFRO_AREA_ZOOM_IN;
  else if (event->direction == GDK_SCROLL_DOWN)
    zoom = GTK_CIFRO_AREA_ZOOM_OUT;
  else
    zoom = GTK_CIFRO_AREA_ZOOM_NONE;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_zoom (carea, zoom, zoom, (from_x + to_x) / 2.0, (from_y + to_y) / 2.0);

  return FALSE;
}

static gboolean
hyscan_gtk_map_steer_is_err_line (HyScanGtkMapSteer *steer,
                                  gdouble            x,
                                  gdouble            y)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  gdouble val_x, val_y;
  gdouble scale_x;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->carea), x, y, &val_x, &val_y);
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->carea), &scale_x, NULL);

  return (ABS (ABS (val_x) - priv->threshold_x) < scale_x * 5.0);
}

static gboolean
hyscan_gtk_map_steer_carea_button (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  HyScanGtkMapSteer *steer = HYSCAN_GTK_MAP_STEER_CAREA (widget)->steer;
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);

  if (event->type == GDK_2BUTTON_PRESS)
    {
      if (priv->mode == MODE_VESSEL)
        {
          gtk_cifro_area_set_view_center (carea, 0, 0);
          gtk_cifro_area_set_angle (carea, 0);
          priv->mode = MODE_ORIGIN;
        }
      else
        {
          priv->mode = MODE_VESSEL;
        }
      gtk_widget_queue_draw (GTK_WIDGET (steer));
    }

  if (event->type == GDK_BUTTON_PRESS)
    {
      if (!hyscan_gtk_map_steer_is_err_line (steer, event->x, event->y))
        return GDK_EVENT_PROPAGATE;

      priv->state = STATE_GRAB;
    }
  else if (event->type == GDK_BUTTON_RELEASE)
    {
      if (priv->state == STATE_GRAB)
        {
          priv->state = STATE_NONE;
        }
    }

  return GDK_EVENT_STOP;
}

static gboolean
hyscan_gtk_map_steer_carea_motion_notify (GtkWidget      *widget,
                                          GdkEventMotion *event)
{
  HyScanGtkMapSteer *steer = HYSCAN_GTK_MAP_STEER_CAREA (widget)->steer;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMapSteerPrivate *priv = steer->priv;

  const gchar *cursor_name = NULL;

  if (priv->state == STATE_NONE)
    {
      if (hyscan_gtk_map_steer_is_err_line (steer, event->x, event->y))
        {
          cursor_name = "col-resize";
        }
    }
  else if (priv->state == STATE_GRAB)
    {
      gdouble x, y;

      gtk_cifro_area_point_to_value (carea, event->x, event->y, &x, &y);
      priv->threshold_x = ABS (x);

      gtk_widget_queue_draw (GTK_WIDGET (steer));
    }


  GdkWindow *window;
  GdkCursor *cursor;
  window = gtk_widget_get_window (GTK_WIDGET (steer));
  if (cursor_name != NULL)
    cursor = gdk_cursor_new_from_name (gtk_widget_get_display (GTK_WIDGET (steer)), cursor_name);
  else
    cursor = NULL;
  gdk_window_set_cursor (window, cursor);
  g_clear_object (&cursor);

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_steer_set_track (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  HyScanPlannerTrack *track = NULL;
  HyScanTrackPlan *plan;
  gchar *active_id;

  active_id = hyscan_planner_selection_get_active_track (priv->selection);
  if (active_id != NULL)
    track = (HyScanPlannerTrack *) hyscan_object_model_get_id (priv->planner_model, active_id);

  if (g_strcmp0 (active_id, priv->track_id) != 0)
    {
      g_queue_clear (priv->points);
      gtk_cifro_area_set_view_center (carea, 0, 0);
    }

  g_free (priv->track_id);
  priv->track_id = active_id;
  hyscan_planner_track_free (priv->track);
  priv->track = track;
  g_clear_object (&priv->geo);

  if (priv->track == NULL)
    {
      gtk_label_set_text (GTK_LABEL (priv->track_info), _("No active track"));
      goto exit;
    }

  plan = &priv->track->plan;
  priv->geo = hyscan_planner_track_geo (plan, &priv->angle);
  hyscan_geo_geo2topoXY (priv->geo, &priv->start, plan->start);
  hyscan_geo_geo2topoXY (priv->geo, &priv->end, plan->end);
  priv->length = hyscan_cartesian_distance (&priv->start, &priv->end);
  g_queue_foreach (priv->points, (GFunc) hyscan_gtk_map_steer_calc_point, steer);

  /* Выводим информацию о галсе. */
  {
    gchar *text;
    gdouble estimated_time;
    gint min, sec;

    estimated_time = priv->length / plan->velocity;
    hyscan_gtk_map_steer_time_to_mmss (estimated_time, &min, &sec);
    text = g_strdup_printf (_("Plan: %.1f m/s %.0f° L%.0fm ~%02d:%02d"),
                            plan->velocity, priv->angle, priv->length, min, sec);
    gtk_label_set_text (GTK_LABEL (priv->track_info), text);
    g_free (text);
  }

exit:
  gtk_widget_queue_draw (GTK_WIDGET (priv->carea));
}

/**
 * hyscan_gtk_map_steer_new:
 * @model: модель навигационных данных
 * @selection: выбор пользователя в планировщике
 *
 * Создаёт виджет навигации по запланированному галсу
 *
 * Returns: новый виджет #HyScanGtkMapSteer
 */
GtkWidget *
hyscan_gtk_map_steer_new (HyScanNavModel         *model,
                          HyScanPlannerSelection *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_STEER,
                       "model", model,
                       "selection", selection,
                       NULL);
}

/**
 * hyscan_gtk_map_steer_sensor_set_offset:
 * @steer: указатель на #HyScanGtkMapSteer
 * @offset: смещение антенны гидролокатора
 *
 * Устанавливает смещение антенны гидролокотора. Смещение используется для
 * внесения поправки в местоположение антенны.
 */
void
hyscan_gtk_map_steer_sensor_set_offset (HyScanGtkMapSteer         *steer,
                                        const HyScanAntennaOffset *offset)
{
  HyScanGtkMapSteerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_STEER (steer));
  priv = steer->priv;

  g_clear_pointer (&priv->offset, hyscan_antenna_offset_free);
  priv->offset = hyscan_antenna_offset_copy (offset);
}

/**
 * hyscan_gtk_map_steer_set_recording:
 * @steer: указатель на #HyScanGtkMapSteer
 * @recording: %TRUE, если запись включена
 *
 * Устанавливает статус записи галса
 */
void
hyscan_gtk_map_steer_set_recording (HyScanGtkMapSteer *steer,
                                    gboolean           recording)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_STEER (steer));

  steer->priv->recording = recording;
}

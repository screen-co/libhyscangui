#include "hyscan-gtk-map-steer.h"
#include <math.h>
#include <hyscan-cartesian.h>
#include <glib/gi18n-lib.h>

#define HYSCAN_TYPE_GTK_MAP_STEER_NUM             (hyscan_gtk_map_steer_num_get_type ())
#define HYSCAN_GTK_MAP_STEER_NUM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_STEER_NUM, HyScanGtkMapSteerNum))
#define HYSCAN_IS_GTK_MAP_STEER_NUM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_STEER_NUM))
#define HYSCAN_GTK_MAP_STEER_NUM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_STEER_NUM, HyScanGtkMapSteerNumClass))
#define HYSCAN_IS_GTK_MAP_STEER_NUM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_STEER_NUM))
#define HYSCAN_GTK_MAP_STEER_NUM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_STEER_NUM, HyScanGtkMapSteerNumClass))

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
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
  gdouble time;       /* Время фиксации, с. */
  gdouble speed;      /* Скорость движения, м/с. */
  gdouble d_x;        /* Отклонение по расстоянию, м. */
  gdouble d_angle;    /* Отклонение по углу, рад. */
  gdouble d_speed;    /* Отклонение по скорости, м/с. */
  gdouble time_left;  /* Отклонение по скорости, м/с. */
} HyScanGtkMapSteerPoint;

typedef struct
{
  GtkDrawingArea parent_instance;

  HyScanGtkMapSteer *steer;          /* Указатель на основной виджет. */
  PangoLayout       *pango_label;    /* Шрифт текста. */
  PangoLayout       *pango_small;    /* Шрифт подписи. */
  gdouble            label_height;   /* Размер текста. */
  gdouble            margin;         /* Горизонтальный отступ. */
  gdouble            margin_bottom;  /* Отступ снизу. */
  gdouble            margin_top;     /* Отступ сверху. */
  gdouble            height;         /* Общая высота виджета. */
} HyScanGtkMapSteerNum;

typedef struct
{
  GtkDrawingAreaClass parent_instance;
} HyScanGtkMapSteerNumClass;

struct _HyScanGtkMapSteerPrivate
{
  HyScanNavModel             *nav_model;         /* Модель навигационных данных. */
  HyScanPlannerSelection     *selection;         /* Модель выбранных объектов планировщика. */
  HyScanGeo                  *geo;               /* Объект перевода географических координат. */

  guint                       state;
  guint                       mode;

  gdouble                     threshold_x;       /* Допустимое отклонение. */
  gdouble                     threshold_angle;   /* Допустимое отклонение. */
  gdouble                     threshold_speed;   /* Допустимое отклонение. */

  GQueue                     *points;            /* Список точек HyScanGtkMapSteerPoint. */
  gdouble                     time_span;         /* Промежуток времени, в течение которого показываются точки, секунды. */
  gdouble                     end_time;          /* Время последней точки. */

  HyScanPlannerTrack         *track;             /* Активный галс. */
  HyScanGeoCartesian2D        start;             /* Точка начала. */
  HyScanGeoCartesian2D        end;               /* Точка окончания. */
  gdouble                     angle;             /* Целевой азимут. */

  GtkWidget                  *display_eta;
  GtkWidget                  *display_speed;
  GtkWidget                  *display_dist;
  GtkWidget                  *display_rot;
  GtkWidget                  *carea;

  PangoLayout                *pango_axis;        /* Шрифт. */
  GdkRGBA                     color_neutral;
  GdkRGBA                     color_good;
  GdkRGBA                     color_bad;
  gdouble                     arrow_size;
};

static void           hyscan_gtk_map_steer_set_property                   (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           const GValue             *value,
                                                                           GParamSpec               *pspec);
static void           hyscan_gtk_map_steer_object_constructed             (GObject                  *object);
static void           hyscan_gtk_map_steer_object_finalize                (GObject                  *object);

static void           hyscan_gtk_map_steer_num_object_finalize            (GObject                  *object);
static gboolean       hyscan_gtk_map_steer_num_draw                       (GtkWidget                *display,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_num_draw_corr1                 (HyScanGtkMapSteerNum     *steer_num,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_num_draw_corr2                 (HyScanGtkMapSteerNum     *steer_num,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_num_screen_changed             (GtkWidget                *widget,
                                                                           GdkScreen                *previous_screen);
static void           hyscan_gtk_map_steer_num_get_preferred_width        (GtkWidget                *widget,
                                                                           gint                     *minimum_width,
                                                                           gint                     *natural_width);

static void           hyscan_gtk_map_steer_carea_draw                     (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_screen_changed           (HyScanGtkMapSteer        *steer,
                                                                           GdkScreen                *previous_screen);
static gboolean       hyscan_gtk_map_steer_carea_configure                (GtkWidget                *widget,
                                                                           GdkEvent                 *event,
                                                                           HyScanGtkMapSteer        *steer);
static gboolean       hyscan_gtk_map_steer_carea_scroll                   (HyScanGtkMapSteer        *steer,
                                                                           GdkEventScroll           *event);
static gboolean       hyscan_gtk_map_steer_carea_motion_notify            (HyScanGtkMapSteer        *steer,
                                                                           GdkEventMotion           *event);
static gboolean       hyscan_gtk_map_steer_carea_button                   (HyScanGtkMapSteer        *steer,
                                                                           GdkEventButton           *event);
static void           hyscan_gtk_map_steer_carea_draw_path                (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_current             (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_axis                (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static void           hyscan_gtk_map_steer_carea_draw_err_line            (HyScanGtkMapSteer        *steer,
                                                                           cairo_t                  *cairo);
static gboolean       hyscan_gtk_map_steer_is_err_line                    (HyScanGtkMapSteer        *steer,
                                                                           gdouble                   x,
                                                                           gdouble                   y);

static GtkWidget *    hyscan_gtk_map_steer_create_num                     (HyScanGtkMapSteer        *steer);
static GtkWidget *    hyscan_gtk_map_steer_create_carea                   (HyScanGtkMapSteer        *steer);

static void           hyscan_gtk_map_steer_nav_changed                    (HyScanGtkMapSteer        *steer);
static void           hyscan_gtk_map_steer_activated                      (HyScanGtkMapSteer        *steer);
static void           hyscan_gtk_map_steer_set_track                      (HyScanGtkMapSteer        *steer,
                                                                           const HyScanPlannerTrack *track);
static HyScanGtkMapSteerPoint *  hyscan_gtk_map_steer_point_new           (void);
static void           hyscan_gtk_map_steer_point_free                     (gpointer                  point);


G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapSteer, hyscan_gtk_map_steer, GTK_TYPE_GRID)
G_DEFINE_TYPE (HyScanGtkMapSteerNum, hyscan_gtk_map_steer_num, GTK_TYPE_DRAWING_AREA)

static void
hyscan_gtk_map_steer_num_class_init (HyScanGtkMapSteerNumClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = hyscan_gtk_map_steer_num_object_finalize;
  widget_class->screen_changed = hyscan_gtk_map_steer_num_screen_changed;
  widget_class->get_preferred_width = hyscan_gtk_map_steer_num_get_preferred_width;
  widget_class->draw = hyscan_gtk_map_steer_num_draw;
}

static void
hyscan_gtk_map_steer_num_init (HyScanGtkMapSteerNum *steer_num)
{
  steer_num->label_height = 25.0;
  steer_num->margin = 10.0;
  steer_num->margin_bottom = 10.0;
  steer_num->margin_top = 10.0;
  steer_num->height = steer_num->label_height + steer_num->margin_bottom + steer_num->margin_top;

  gtk_widget_set_size_request (GTK_WIDGET (steer_num), -1, steer_num->height);
}

static void
hyscan_gtk_map_steer_num_object_finalize (GObject *object)
{
  HyScanGtkMapSteerNum *steer_num = HYSCAN_GTK_MAP_STEER_NUM (object);

  g_clear_object (&steer_num->pango_label);
  g_clear_object (&steer_num->pango_small);
}

static void
hyscan_gtk_map_steer_class_init (HyScanGtkMapSteerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->constructed (object);

  priv->points = g_queue_new ();
  priv->time_span = 100.0;
  priv->threshold_x = 6.0;
  priv->threshold_angle = 4.0;
  priv->threshold_speed = 0.1;
  priv->arrow_size = 10.0;

  gdk_rgba_parse (&priv->color_neutral, "#888888");
  gdk_rgba_parse (&priv->color_good, "#008800");
  gdk_rgba_parse (&priv->color_bad, "#880000");

  priv->carea = hyscan_gtk_map_steer_create_carea (steer);

  priv->display_dist = hyscan_gtk_map_steer_create_num (steer);
  priv->display_eta = hyscan_gtk_map_steer_create_num (steer);
  priv->display_rot = hyscan_gtk_map_steer_create_num (steer);
  priv->display_speed = hyscan_gtk_map_steer_create_num (steer);

  gtk_grid_attach (grid, priv->display_speed, 0, 0, 1, 1);
  gtk_grid_attach (grid, priv->display_eta,   1, 0, 1, 1);
  gtk_grid_attach (grid, priv->display_dist,  0, 1, 1, 1);
  gtk_grid_attach (grid, priv->display_rot,   1, 1, 1, 1);
  gtk_grid_attach (grid, priv->carea,         0, 2, 2, 1);

  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_steer_nav_changed), steer);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_steer_activated), steer);
}

static void
hyscan_gtk_map_steer_object_finalize (GObject *object)
{
  HyScanGtkMapSteer *gtk_map_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_map_steer->priv;

  g_clear_object (&priv->pango_axis);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->selection);
  g_queue_free_full (priv->points, hyscan_gtk_map_steer_point_free);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->finalize (object);
}

static GtkWidget *
hyscan_gtk_map_steer_create_carea (HyScanGtkMapSteer *steer)
{
  GtkWidget *carea;

  carea = gtk_cifro_area_new ();
  gtk_widget_set_size_request (carea, -1, 150);

  g_signal_connect_after (carea, "configure-event", G_CALLBACK (hyscan_gtk_map_steer_carea_configure), steer);
  g_signal_connect_swapped (carea, "screen-changed", G_CALLBACK (hyscan_gtk_map_steer_carea_screen_changed), steer);
  g_signal_connect_swapped (carea, "visible-draw", G_CALLBACK (hyscan_gtk_map_steer_carea_draw), steer);
  g_signal_connect_swapped (carea, "scroll-event", G_CALLBACK (hyscan_gtk_map_steer_carea_scroll), steer);
  g_signal_connect_swapped (carea, "motion-notify-event", G_CALLBACK (hyscan_gtk_map_steer_carea_motion_notify), steer);
  g_signal_connect_swapped (carea, "button-press-event", G_CALLBACK (hyscan_gtk_map_steer_carea_button), steer);
  g_signal_connect_swapped (carea, "button-release-event", G_CALLBACK (hyscan_gtk_map_steer_carea_button), steer);

  return carea;
}

static GtkWidget *
hyscan_gtk_map_steer_create_num (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerNum *display;

  display = g_object_new (HYSCAN_TYPE_GTK_MAP_STEER_NUM, NULL);
  display->steer = steer;

  return GTK_WIDGET (display);
}

static void
hyscan_gtk_map_steer_nav_changed (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  HyScanNavModelData data;
  HyScanGeoCartesian2D position, nearest_point;
  gdouble distance;
  HyScanGtkMapSteerPoint *point;
  gdouble heading;
  gint side;

  if (!hyscan_nav_model_get (priv->nav_model, &data, NULL))
    return;

  if (priv->geo == NULL)
    return;

  if (!hyscan_geo_geo2topoXY (priv->geo, &position, data.coord))
    return;

  distance = hyscan_cartesian_distance_to_line (&priv->start, &priv->end, &position, &nearest_point);
  side = hyscan_cartesian_side (&priv->start, &priv->end, &position);
  point = hyscan_gtk_map_steer_point_new ();
  point->d_x = side * distance;
  point->time = data.time;
  heading = data.heading;
  point->d_angle = heading - priv->angle;
  if (point->d_angle > G_PI)
    point->d_angle -= 2.0 * G_PI;
  else if (point->d_angle < -G_PI)
    point->d_angle += 2.0 * G_PI;
  point->speed = data.speed;
  point->d_speed = data.speed - priv->track->speed;

  gdouble distance_to_end;
  HyScanGeoCartesian2D track_end;
  hyscan_geo_geo2topoXY (priv->geo, &track_end, priv->track->end);
  distance_to_end = hyscan_cartesian_distance (&track_end, &nearest_point);
  point->time_left = distance_to_end / priv->track->speed;

  g_queue_push_head (priv->points, point);
  priv->end_time = point->time;

  if (priv->mode == MODE_VESSEL)
    {
      gtk_cifro_area_set_view_center (GTK_CIFRO_AREA (steer), point->d_x, 0);
      gtk_cifro_area_set_angle (GTK_CIFRO_AREA (steer), -point->d_angle);
    }

  HyScanGtkMapSteerPoint *last_point;
  while ((last_point = g_queue_peek_tail (priv->points)) != NULL)
    {
      if (priv->end_time - last_point->time < priv->time_span)
        break;

      last_point = g_queue_pop_tail (priv->points);
      hyscan_gtk_map_steer_point_free (last_point);
    }

  gtk_widget_queue_draw (GTK_WIDGET (steer));
}

static void
hyscan_gtk_map_steer_activated (HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  gchar *active_id;
  HyScanPlannerObject *object;
  HyScanPlannerModel *model;

  active_id = hyscan_planner_selection_get_active_track (priv->selection);
  if (active_id != NULL)
    {
      model = hyscan_planner_selection_get_model (priv->selection);
      object = hyscan_object_model_get_id (HYSCAN_OBJECT_MODEL (model), active_id);
      g_free (active_id);
      g_object_unref (model);
    }
  else
    {
      object = NULL;
    }

  if (object != NULL && object->type == HYSCAN_PLANNER_TRACK)
    hyscan_gtk_map_steer_set_track (steer, &object->track);
  else
    hyscan_gtk_map_steer_set_track (steer, NULL);

  hyscan_planner_object_free (object);


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
hyscan_gtk_map_steer_num_draw_corr2 (HyScanGtkMapSteerNum *steer_num,
                                     cairo_t              *cairo)
{
  gdouble height = steer_num->label_height * 0.6;

  cairo_move_to (cairo, 5, 0);
  cairo_line_to (cairo, 15, -height / 2.0);
  cairo_line_to (cairo, 5, -height);

  cairo_set_line_width (cairo, 3.0);
  cairo_stroke (cairo);
}

static void
hyscan_gtk_map_steer_num_draw_corr1 (HyScanGtkMapSteerNum *steer_num,
                                     cairo_t              *cairo)
{
  gdouble center_y;
  gdouble height = steer_num->label_height * 0.8;

  center_y = -height / 2.0;

  cairo_move_to (cairo, 0, 0);
  cairo_curve_to (cairo, 0, -5, 0, center_y - 5.0, 20, center_y);

  cairo_move_to (cairo, 10, center_y + 10.0);
  cairo_line_to (cairo, 20, center_y);
  cairo_line_to (cairo, 10, center_y - 10.0);
  cairo_set_line_width (cairo, 3.0);
  cairo_stroke (cairo);
}

static gboolean
hyscan_gtk_map_steer_num_draw (GtkWidget *display,
                               cairo_t   *cairo)
{
  HyScanGtkMapSteerNum *steer_num = HYSCAN_GTK_MAP_STEER_NUM (display);
  HyScanGtkMapSteer *steer = steer_num->steer;
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  gint height, width;
  gint layout_height, layout_width;
  gchar text[16];
  const gchar *label;
  gdouble x_end, y_end;
  gdouble cell_width;
  gdouble cell_height;
  GdkRGBA *color;
  gdouble correction_width;
  gdouble value;
  HyScanGtkMapSteerPoint *point;
  void (*correction) (HyScanGtkMapSteerNum *steer_num,
                      cairo_t              *cairo);

  point = g_queue_peek_head (priv->points);
  value = 0.0;
  correction = NULL;
  color = &priv->color_neutral;
  g_snprintf (text, sizeof (text), "N/A");

  if (display == priv->display_rot)
    {
      label = _("ROT");
      if (point != NULL)
        {
          value = ABS (point->d_angle / G_PI * 180.0) < 1.0 ? 0.0 : point->d_angle / G_PI * 180.0;
          g_snprintf (text, sizeof (text), "%.0f°", ABS (value));
          color = ABS (value) < priv->threshold_angle ? &priv->color_good : &priv->color_bad;
          correction = hyscan_gtk_map_steer_num_draw_corr1;
        }
    }

  else if (display == priv->display_dist)
    {
      label = _("DIST m");
      if (point != NULL)
        {
          value = ABS (point->d_x) < 0.1 ? 0.0 : point->d_x;
          g_snprintf (text, sizeof (text), value > 1000 ? "%.0f" : "%.1f", ABS (value));
          color = ABS (value) < priv->threshold_x ? &priv->color_good : &priv->color_bad;
          correction = hyscan_gtk_map_steer_num_draw_corr2;
        }
    }

  else if (display == priv->display_speed)
    {
      label = _("SPD m/s");
      if (point != NULL)
        {
          gdouble max_error;

          g_snprintf (text, sizeof (text), "%.2f", point->speed);

          max_error = priv->track->speed * priv->threshold_speed;
          color = ABS (point->d_speed) < max_error ? &priv->color_good : &priv->color_bad;
        }
    }

  else if (display == priv->display_eta)
    {
      label = _("ETA");
      if (point != NULL)
      {
        gint min, sec;

        min = point->time_left / 60;
        sec = (point->time_left - min * 60);

        g_snprintf (text, sizeof (text), "%02d:%02d", min, sec);
        color = &priv->color_good;
      }
    }

  else
    {
      g_return_val_if_reached (FALSE);
    }

  correction_width = correction != NULL ? 20.0 : 0;

  height = gtk_widget_get_allocated_height (GTK_WIDGET (steer_num));
  width = gtk_widget_get_allocated_width (GTK_WIDGET (steer_num));

  cell_width = width;
  cell_height = height;
  x_end = cell_width;
  y_end = height;

  cairo_rectangle (cairo, x_end, y_end, -cell_width, -cell_height);
  gdk_cairo_set_source_rgba (cairo, color);
  cairo_fill_preserve (cairo);
  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);
  cairo_stroke (cairo);

  /* Текущие значения. */
  pango_layout_set_text (steer_num->pango_label, text, -1);
  cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);

  gint baseline;
  pango_layout_get_pixel_size (steer_num->pango_label, &layout_width, &layout_height);
  baseline = pango_layout_get_baseline (steer_num->pango_label);
  baseline /= PANGO_SCALE;

  cairo_move_to (cairo, x_end - layout_width - correction_width - 2 * steer_num->margin, y_end - steer_num->margin_bottom - baseline);
  pango_cairo_show_layout (cairo, steer_num->pango_label);

  cairo_save (cairo);
  pango_layout_set_text (steer_num->pango_small, label, -1);
  cairo_translate (cairo, x_end - cell_width + steer_num->margin_bottom / 2.0, y_end - steer_num->margin_bottom / 2.0);
  cairo_rotate (cairo, -G_PI_2);
  cairo_move_to (cairo, 0, 0);
  pango_cairo_show_layout (cairo, steer_num->pango_small);
  cairo_restore (cairo);

  if (value == 0.0 || correction == NULL)
    return FALSE;

  cairo_save (cairo);
  cairo_translate (cairo, x_end - correction_width - steer_num->margin, y_end - steer_num->margin_bottom);
  if (value > 0)
    {
      cairo_scale (cairo, -1, 1);
      cairo_translate (cairo, -correction_width, 0);
    }

  correction (steer_num, cairo);
  cairo_restore (cairo);

  return FALSE;
}

static void
hyscan_gtk_map_steer_num_screen_changed (GtkWidget *widget,
                                         GdkScreen *previous_screen)
{
  HyScanGtkMapSteerNum *steer_num = HYSCAN_GTK_MAP_STEER_NUM (widget);
  PangoFontDescription *font_desc;
  static gint i;

  g_clear_object (&steer_num->pango_label);
  steer_num->pango_label = gtk_widget_create_pango_layout (widget, NULL);

  g_clear_object (&steer_num->pango_small);
  steer_num->pango_small = gtk_widget_create_pango_layout (widget, NULL);

  font_desc = pango_font_description_new ();
  pango_font_description_set_absolute_size (font_desc, steer_num->label_height * PANGO_SCALE);
  pango_layout_set_font_description (steer_num->pango_label, font_desc);

  pango_font_description_set_absolute_size (font_desc, 0.3 * steer_num->label_height * PANGO_SCALE);
  pango_layout_set_font_description (steer_num->pango_small, font_desc);

  pango_font_description_free (font_desc);

  g_message ("Screen changed %d", ++i);
}

static void
hyscan_gtk_map_steer_num_get_preferred_width (GtkWidget *widget,
                                              gint      *minimum_width,
                                              gint      *natural_width)
{
  *minimum_width = 100;
  *natural_width = 132;
}

static void
hyscan_gtk_map_steer_carea_draw_err_line (HyScanGtkMapSteer *steer,
                                          cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;

  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);

  /* Линии допустимых отклонений. */
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -priv->threshold_x, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -priv->threshold_x, 0);
  cairo_line_to (cairo, x, y);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, priv->threshold_x, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, priv->threshold_x, 0);
  cairo_line_to (cairo, x, y);

  cairo_set_dash (cairo, (gdouble[]) { 5.0 }, 1, 0.0);
  cairo_set_line_width (cairo, 1.0);
  cairo_stroke (cairo);
}

static void
hyscan_gtk_map_steer_carea_draw_axis (HyScanGtkMapSteer *steer,
                                      cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale_x, scale_y;

  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);

  /* Координатные линии по времени и по отклонению. */
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, from_x, 0);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, to_x, 0);
  cairo_line_to (cairo, x, y);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, 0);
  cairo_line_to (cairo, x, y);

  cairo_set_line_width (cairo, 1.0);
  cairo_stroke (cairo);

  /* Координата X. */
  {
    gdouble step, axis_val;

    axis_val = from_x;
    gtk_cifro_area_get_axis_step (scale_x, 50, &axis_val, &step, NULL, NULL);
    while (axis_val < to_x)
      {
        gchar *label;
        gint label_height, label_width;

        if (ABS (axis_val) < step / 2.0)
          {
            axis_val += step;
            continue;
          }

        gtk_cifro_area_visible_value_to_point (carea, &x, &y, axis_val, 0);

        label = g_strdup_printf ("%.2f", axis_val);
        pango_layout_set_text (priv->pango_axis, label, -1);
        pango_layout_get_pixel_size (priv->pango_axis, &label_width, &label_height);
        cairo_move_to (cairo, x - label_width / 2.0, y + 3.0);
        pango_cairo_show_layout (cairo, priv->pango_axis);
        g_free (label);

        cairo_move_to (cairo, x, y + 3.0);
        cairo_line_to (cairo, x, y - 3.0);

        axis_val += step;
      }

    cairo_stroke (cairo);
  }

  /* Координата Y. */
  {
    gdouble step, axis_val;
    gint i = 0;

    axis_val = 0;
    gtk_cifro_area_get_axis_step (scale_y, 10, &axis_val, &step, NULL, NULL);
    axis_val -= step;
    while (axis_val > from_y)
      {
        gchar *label;
        gint label_height, label_width;

        gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, axis_val);

        if (i++ % 3 == 0)
          {
            label = g_strdup_printf ("%.1f", -axis_val);
            pango_layout_set_text (priv->pango_axis, label, -1);
            pango_layout_get_pixel_size (priv->pango_axis, &label_width, &label_height);
            cairo_move_to (cairo, x + 3.0, y - label_height / 2.0);
            pango_cairo_show_layout (cairo, priv->pango_axis);
            g_free (label);
          }

        cairo_move_to (cairo, x - 3.0, y);
        cairo_line_to (cairo, x + 3.0, y);

        axis_val -= step;
      }

    cairo_stroke (cairo);
  }
}

static void
hyscan_gtk_map_steer_carea_draw_current (HyScanGtkMapSteer *steer,
                                         cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  gdouble x, y;

  GdkRGBA *track_color;
  HyScanGtkMapSteerPoint *point = priv->points->head->data;

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, 0);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->d_x, priv->end_time - point->time);
  cairo_line_to (cairo, x, y);
  track_color = ABS (point->d_x) < priv->threshold_x ? &priv->color_good : &priv->color_bad;
  gdk_cairo_set_source_rgba (cairo, track_color);
  cairo_set_line_width (cairo, 5.0);
  cairo_stroke (cairo);

  /* Маленькая стрелка. */
  cairo_save (cairo);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, point->d_angle);

  cairo_move_to (cairo, 0, priv->arrow_size);
  cairo_line_to (cairo, -6.0, priv->arrow_size);
  cairo_line_to (cairo, 0, 0.0);
  cairo_line_to (cairo, 6.0, priv->arrow_size);
  cairo_close_path (cairo);
  cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
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

  /* Точки нашего положения. */
  GList *link;
  cairo_set_dash (cairo, NULL, 0, 0.0);
  cairo_set_line_width (cairo, 1.0);
  for (link = priv->points->head; link != NULL; link = link->next)
    {
      HyScanGtkMapSteerPoint *point = link->data;
      GdkRGBA *track_color;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->d_x, point->time - priv->end_time);
      y += priv->arrow_size;
      cairo_line_to (cairo, x, y);

      track_color = ABS (point->d_x) < priv->threshold_x ? &priv->color_good : &priv->color_bad;
      gdk_cairo_set_source_rgba (cairo, track_color);
      cairo_stroke (cairo);

      cairo_move_to (cairo, x, y);
    }
}

static void
hyscan_gtk_map_steer_carea_draw (HyScanGtkMapSteer *steer,
                                 cairo_t           *cairo)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  guint width, height;
  GdkRGBA color;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (carea));

  gtk_cifro_area_get_visible_size (carea, &width, &height);

  gtk_render_background (context, cairo, 0, 0, width, height);

  gtk_style_context_get_color (context,
                               gtk_style_context_get_state (context),
                               &color);

  gdk_cairo_set_source_rgba (cairo, &color);

  cairo_save (cairo);

  hyscan_gtk_map_steer_carea_draw_axis (steer, cairo);
  hyscan_gtk_map_steer_carea_draw_err_line (steer, cairo);

  if (g_queue_get_length (priv->points) == 0)
    goto exit;

  hyscan_gtk_map_steer_carea_draw_path (steer, cairo);
  hyscan_gtk_map_steer_carea_draw_current (steer, cairo);

exit:
  cairo_restore (cairo);
}

static void
hyscan_gtk_map_steer_carea_screen_changed (HyScanGtkMapSteer *steer,
                                           GdkScreen         *previous_screen)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;

  g_clear_object (&priv->pango_axis);
  priv->pango_axis = gtk_widget_create_pango_layout (priv->carea, NULL);
}

static gboolean
hyscan_gtk_map_steer_carea_configure (GtkWidget         *widget,
                                      GdkEvent          *event,
                                      HyScanGtkMapSteer *steer)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  guint width, height;

  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->carea), &width, &height);
  if (width == 0 || height == 0)
    return FALSE;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->carea), -priv->threshold_x * 2.0, priv->threshold_x * 2.0, -20, 0);
  g_signal_handlers_disconnect_by_func (widget, hyscan_gtk_map_steer_carea_configure, steer);

  return FALSE;
}

static gboolean
hyscan_gtk_map_steer_carea_scroll (HyScanGtkMapSteer *steer,
                                   GdkEventScroll    *event)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);
  GtkCifroAreaZoomType zoom_x = GTK_CIFRO_AREA_ZOOM_NONE;
  GtkCifroAreaZoomType zoom_y = GTK_CIFRO_AREA_ZOOM_NONE;

  if (event->state & GDK_SHIFT_MASK)
    {
      if (event->direction == GDK_SCROLL_UP)
        zoom_x = GTK_CIFRO_AREA_ZOOM_IN;
      else if (event->direction == GDK_SCROLL_DOWN)
        zoom_x = GTK_CIFRO_AREA_ZOOM_OUT;
    }
  else
    {
      if (event->direction == GDK_SCROLL_UP)
        zoom_y = GTK_CIFRO_AREA_ZOOM_IN;
      else if (event->direction == GDK_SCROLL_DOWN)
        zoom_y = GTK_CIFRO_AREA_ZOOM_OUT;
    }

  gtk_cifro_area_zoom (carea, zoom_x, zoom_y, 0, 0);

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

  if (val_y > 0)
    return FALSE;

  return (ABS (ABS (val_x) - priv->threshold_x) < scale_x * 5.0);
}

static gboolean
hyscan_gtk_map_steer_carea_button (HyScanGtkMapSteer *steer,
                                   GdkEventButton    *event)
{
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
hyscan_gtk_map_steer_carea_motion_notify (HyScanGtkMapSteer *steer,
                                          GdkEventMotion    *event)
{
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->carea);


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
hyscan_gtk_map_steer_set_track (HyScanGtkMapSteer        *steer,
                                const HyScanPlannerTrack *track)
{
  HyScanGtkMapSteerPrivate *priv;
  HyScanGeoGeodetic origin;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_STEER (steer));
  priv = steer->priv;

  hyscan_planner_object_free ((HyScanPlannerObject *) priv->track);
  priv->track = track != NULL ? hyscan_planner_track_copy (track) : NULL;
  g_clear_object (&priv->geo);

  g_queue_clear (priv->points);

  if (track == NULL)
    return;

  {
    gdouble lat1, lat2, lon1, lon2, dlon;

    lat1 = priv->track->start.lat / 180.0 * G_PI;
    lon1 = priv->track->start.lon / 180.0 * G_PI;
    lat2 = priv->track->end.lat / 180.0 * G_PI;
    lon2 = priv->track->end.lon / 180.0 * G_PI;
    dlon = lon2 - lon1;

    priv->angle = atan2 (sin(dlon) * cos (lat2), cos (lat1) * sin (lat2) - sin (lat1) * cos (lat2) * cos (dlon));
  }

  origin = priv->track->start;
  origin.h = priv->angle / G_PI * 180;
  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  hyscan_geo_geo2topoXY (priv->geo, &priv->start, priv->track->start);
  hyscan_geo_geo2topoXY (priv->geo, &priv->end, priv->track->end);
}

GtkWidget *
hyscan_gtk_map_steer_new (HyScanNavModel         *model,
                          HyScanPlannerSelection *selection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_STEER,
                       "model", model,
                       "selection", selection,
                       NULL);
}

#include "hyscan-gtk-map-steer.h"
#include <math.h>
#include <hyscan-cartesian.h>

enum
{
  PROP_O,
  PROP_MODEL,
  PROP_SELECTION,
};

typedef struct
{
  gdouble time;       /* Время фиксации, с. */
  gdouble speed;      /* Скорость движения, м/с. */
  gdouble d_x;        /* Отклонение по расстоянию, м. */
  gdouble d_angle;    /* Отклонение по углу, рад. */
  gdouble d_speed;    /* Отклонение по скорости, м/с. */
} HyScanGtkMapSteerPoint;

struct _HyScanGtkMapSteerPrivate
{
  HyScanNavModel             *nav_model;         /* Модель навигационных данных. */
  HyScanPlannerSelection     *selection;         /* Модель выбранных объектов планировщика. */
  HyScanGeo                  *geo;               /* Объект перевода географических координат. */

  gdouble                     err_line;          /* Допустимое отклонение. */
  gdouble                     scale_x;           /* Масштаб по отклонению. */
  gdouble                     scale_time;        /* Масштаб по времени. */

  GQueue                     *points;            /* Список точек HyScanGtkMapSteerPoint. */
  gdouble                     time_span;         /* Промежуток времени, в течение которого показываются точки, секунды. */
  gdouble                     end_time;          /* Время последней точки. */

  HyScanPlannerTrack         *track;             /* Активный галс. */
  HyScanGeoCartesian2D        start;             /* Точка начала. */
  HyScanGeoCartesian2D        end;               /* Точка окончания. */
  gdouble                     angle;             /* Целевой азимут. */

  PangoLayout                *pango_layout;      /* Шрифт. */
};

static void       hyscan_gtk_map_steer_set_property                   (GObject                  *object,
                                                                       guint                     prop_id,
                                                                       const GValue             *value,
                                                                       GParamSpec               *pspec);
static void       hyscan_gtk_map_steer_object_constructed             (GObject                  *object);
static void       hyscan_gtk_map_steer_object_finalize                (GObject                  *object);
static void       hyscan_gtk_map_steer_draw                           (GtkWidget                *widget,
                                                                       cairo_t                  *cairo);
static gboolean   hyscan_gtk_map_steer_configure                      (GtkWidget                *widget,
                                                                       GdkEventConfigure        *event);
static gboolean   hyscan_gtk_map_steer_scroll                         (GtkWidget                *widget,
                                                                       GdkEventScroll           *event);
static void       hyscan_gtk_map_steer_get_preferred_height_for_width (GtkWidget                *widget,
                                                                       gint                      width,
                                                                       gint                     *minimum_height,
                                                                       gint                     *natural_height);
static void       hyscan_gtk_map_steer_get_preferred_height           (GtkWidget                *widget,
                                                                       gint                     *minimum_height,
                                                                       gint                     *natural_height);
static void       hyscan_gtk_map_steer_nav_changed                    (HyScanGtkMapSteer        *steer);
static void       hyscan_gtk_map_steer_activated                      (HyScanGtkMapSteer        *steer);
static void       hyscan_gtk_map_steer_set_track                      (HyScanGtkMapSteer        *steer,
                                                                       const HyScanPlannerTrack *track);
static HyScanGtkMapSteerPoint *  hyscan_gtk_map_steer_point_new       (void);
static void       hyscan_gtk_map_steer_point_free                     (gpointer                  point);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapSteer, hyscan_gtk_map_steer, GTK_TYPE_CIFRO_AREA)

static void
hyscan_gtk_map_steer_class_init (HyScanGtkMapSteerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_steer_set_property;
  object_class->constructed = hyscan_gtk_map_steer_object_constructed;
  object_class->finalize = hyscan_gtk_map_steer_object_finalize;
  widget_class->configure_event = hyscan_gtk_map_steer_configure;
  widget_class->scroll_event = hyscan_gtk_map_steer_scroll;
  widget_class->get_preferred_height_for_width = hyscan_gtk_map_steer_get_preferred_height_for_width;
  widget_class->get_preferred_height = hyscan_gtk_map_steer_get_preferred_height;

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
  GtkCifroArea *carea = GTK_CIFRO_AREA (object);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->constructed (object);

  priv->points = g_queue_new ();
  priv->time_span = 100.0;
  priv->err_line = 6.0;
  
  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_steer_nav_changed), steer);
  g_signal_connect_swapped (priv->selection, "activated", G_CALLBACK (hyscan_gtk_map_steer_activated), steer);
  g_signal_connect_swapped (carea, "visible-draw", G_CALLBACK (hyscan_gtk_map_steer_draw), steer);
}

static void
hyscan_gtk_map_steer_object_finalize (GObject *object)
{
  HyScanGtkMapSteer *gtk_map_steer = HYSCAN_GTK_MAP_STEER (object);
  HyScanGtkMapSteerPrivate *priv = gtk_map_steer->priv;

  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->selection);
  g_queue_free_full (priv->points, hyscan_gtk_map_steer_point_free);

  G_OBJECT_CLASS (hyscan_gtk_map_steer_parent_class)->finalize (object);
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

  g_queue_push_head (priv->points, point);
  priv->end_time = point->time;

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
hyscan_gtk_map_steer_draw (GtkWidget *widget,
                           cairo_t   *cairo)
{
  HyScanGtkMapSteer *steer = HYSCAN_GTK_MAP_STEER (widget);
  HyScanGtkMapSteerPrivate *priv = steer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  guint width, height;
  GdkRGBA color;
  GtkStyleContext *context;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale_x, scale_y;
  gdouble x, y;

  context = gtk_widget_get_style_context (widget);

  gtk_cifro_area_get_visible_size (carea, &width, &height);
  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);

  gtk_render_background (context, cairo, 0, 0, width, height);

  gtk_style_context_get_color (context,
                               gtk_style_context_get_state (context),
                               &color);

  gdk_cairo_set_source_rgba (cairo, &color);

  cairo_save (cairo);

  /* Координатные линии по времени и по отклонению. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (widget), &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, from_x, 0);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, to_x, 0);
  cairo_line_to (cairo, x, y);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, from_y);
  cairo_move_to (cairo, x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, to_y);
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
        pango_layout_set_text (priv->pango_layout, label, -1);
        pango_layout_get_pixel_size (priv->pango_layout, &label_width, &label_height);
        cairo_move_to (cairo, x - label_width / 2.0, y - label_height - 3.0);
        pango_cairo_show_layout (cairo, priv->pango_layout);
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
            pango_layout_set_text (priv->pango_layout, label, -1);
            pango_layout_get_pixel_size (priv->pango_layout, &label_width, &label_height);
            cairo_move_to (cairo, x + 3.0, y - label_height / 2.0);
            pango_cairo_show_layout (cairo, priv->pango_layout);
            g_free (label);
          }

        cairo_move_to (cairo, x - 3.0, y);
        cairo_line_to (cairo, x + 3.0, y);

        axis_val -= step;
      }

    cairo_stroke (cairo);
  }

  /* Линии допустимых отклонений. */
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -priv->err_line, from_y);
  cairo_move_to (cairo,  x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, -priv->err_line, to_y);
  cairo_line_to (cairo,  x, y);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, priv->err_line, from_y);
  cairo_move_to (cairo,  x, y);
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, priv->err_line, to_y);
  cairo_line_to (cairo,  x, y);

  cairo_set_dash (cairo, (gdouble[]) { 5.0 }, 1, 0.0);
  cairo_set_line_width (cairo, 1.0);
  cairo_stroke (cairo);

  if (g_queue_get_length (priv->points) == 0)
    goto exit;

  /* Точки нашего положения. */
  GList *link;
  for (link = priv->points->head; link != NULL; link = link->next)
    {
      HyScanGtkMapSteerPoint *point = link->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->d_x, point->time - priv->end_time);
      cairo_line_to (cairo, x, y);
    }

  cairo_set_dash (cairo, NULL, 0, 0.0);
  cairo_set_source_rgb (cairo, 0.0, 0.6, 0.0);
  cairo_set_line_width (cairo, 1.0);
  cairo_stroke (cairo);

  /* По последней точки стрелка и линии отклонения. */
  {
    gdouble length = height * .4;
    HyScanGtkMapSteerPoint *point = priv->points->head->data;

    gtk_cifro_area_visible_value_to_point (carea, &x, &y, 0, 0);
    cairo_move_to (cairo, x, y);
    gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->d_x, priv->end_time - point->time);
    cairo_line_to (cairo, x, y);
    cairo_set_source_rgb (cairo, 0.0, 0.6, 0.0);
    cairo_set_line_width (cairo, 5.0);
    cairo_stroke (cairo);

    cairo_save (cairo);
    cairo_translate (cairo, x, y);
    cairo_rotate (cairo, point->d_angle);

    cairo_move_to (cairo, 0, 0);
    cairo_line_to (cairo, 0, -length);

    cairo_move_to (cairo, 3.0, -length + 6.0);
    cairo_line_to (cairo, 0, -length);
    cairo_line_to (cairo, -3.0, -length + 6.0);

    cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
    cairo_set_line_width (cairo, 1.0);
    cairo_stroke (cairo);

    cairo_move_to (cairo, 0, 0);
    cairo_line_to (cairo, -10.0, 5.0);
    cairo_line_to (cairo, 0, -length / 2.0);
    cairo_line_to (cairo, 10.0, 5.0);
    cairo_close_path (cairo);
    cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);
    cairo_fill_preserve (cairo);
    cairo_set_source_rgb (cairo, 0.0, 0.0, 0.6);
    cairo_stroke (cairo);

    cairo_restore (cairo);
  }

exit:
  cairo_restore (cairo);
}

static gboolean
hyscan_gtk_map_steer_configure (GtkWidget         *widget,
                                GdkEventConfigure *event)
{
  HyScanGtkMapSteer *steer = HYSCAN_GTK_MAP_STEER (widget);
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  HyScanGtkMapSteerPrivate *priv = steer->priv;

  GTK_WIDGET_CLASS (hyscan_gtk_map_steer_parent_class)->configure_event (widget, event);

  if (priv->scale_x == 0.0)
    {
      gint width;

      width = gtk_widget_get_allocated_width (widget);
      if (width > 1)
        {
          priv->scale_x = width / priv->err_line / 4.0;
          gtk_cifro_area_set_view (carea, -priv->err_line * 2.0, priv->err_line * 2.0, -20, 20);
        }
    }

  if (priv->scale_time == 0.0)
    {
      gint height;

      height = gtk_widget_get_allocated_height (widget);
      if (height > 1)
        {
          priv->scale_time = height / priv->time_span / 2.0;
          gtk_cifro_area_set_view (carea, -priv->err_line * 2.0, priv->err_line * 2.0, -20, 20);
        }
    }


  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (widget, NULL);

  return FALSE;
}

static gboolean
hyscan_gtk_map_steer_scroll (GtkWidget      *widget,
                             GdkEventScroll *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (widget);
  GtkCifroAreaZoomType zoom_x = GTK_CIFRO_AREA_ZOOM_NONE;
  GtkCifroAreaZoomType zoom_y = GTK_CIFRO_AREA_ZOOM_NONE;

  if (event->state & GDK_CONTROL_MASK)
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

static void
hyscan_gtk_map_steer_get_preferred_height_for_width (GtkWidget *widget,
                                                     gint       width,
                                                     gint      *minimum_height,
                                                     gint      *natural_height)
{
  minimum_height != NULL ? (*minimum_height = 50) : 0;
  natural_height != NULL ? (*natural_height = MAX (width, 50)) : 0;
}

static void
hyscan_gtk_map_steer_get_preferred_height (GtkWidget *widget,
                                           gint      *minimum_height,
                                           gint      *natural_height)
{
  minimum_height != NULL ? (*minimum_height = 100) : 0;
  natural_height != NULL ? (*natural_height = 200) : 0;
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

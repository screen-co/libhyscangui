#include "hyscan-gtk-map-ruler.h"
#include <math.h>

#define EARTH_RADIUS 6378137.0
#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)

enum
{
  PROP_O,
  PROP_MAP
};

/* Точка на линейке. */
typedef struct
{
  gdouble x;
  gdouble y;
} HyScanGtkMapRulerPoint;

struct _HyScanGtkMapRulerPrivate
{
  HyScanGtkMap              *map;              /* Виджет карты. */

  GList                     *points;           /* Список точек, между которыми необходимо измерить расстояние. */
  HyScanGtkMapRulerPoint    *hover_point;      /* Точка из списка points под указателем мыши. */
  HyScanGtkMapRulerPoint    *drag_point;       /* Точка из списка points, которая сейчас перетаскивается. */
  HyScanGtkMapRulerPoint    *remove_point;     /* Точка из списка points, которую надо удалить при отжатии кнопки. */

  gdouble                    radius;           /* Радиус точки - узла в измеряемом пути. */
};

static void     hyscan_gtk_map_ruler_set_property         (GObject               *object,
                                                           guint                  prop_id,
                                                           const GValue          *value,
                                                           GParamSpec            *pspec);
static void     hyscan_gtk_map_ruler_object_constructed   (GObject               *object);
static void     hyscan_gtk_map_ruler_object_finalize      (GObject               *object);
static void     hyscan_gtk_map_ruler_draw                 (HyScanGtkMapRuler     *ruler,
                                                           cairo_t               *cairo);
static gboolean hyscan_gtk_map_ruler_button_press_release (HyScanGtkMapRuler     *ruler,
                                                           GdkEventButton        *event);
static gboolean hyscan_gtk_map_ruler_motion               (HyScanGtkMapRuler     *ruler,
                                                           GdkEventMotion        *event);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapRuler, hyscan_gtk_map_ruler, G_TYPE_OBJECT)

static void
hyscan_gtk_map_ruler_class_init (HyScanGtkMapRulerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_ruler_set_property;
  object_class->constructed = hyscan_gtk_map_ruler_object_constructed;
  object_class->finalize = hyscan_gtk_map_ruler_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "Map", "HyScanGtkMap widget", HYSCAN_TYPE_GTK_MAP,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_ruler_init (HyScanGtkMapRuler *gtk_map_ruler)
{
  gtk_map_ruler->priv = hyscan_gtk_map_ruler_get_instance_private (gtk_map_ruler);
}

static void
hyscan_gtk_map_ruler_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

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
hyscan_gtk_map_ruler_object_constructed (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->constructed (object);

  priv->radius = 4.0;

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_ruler_draw), gtk_map_ruler);

  g_signal_connect_swapped (priv->map, "button-press-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_button_press_release), gtk_map_ruler);
  g_signal_connect_swapped (priv->map, "button-release-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_button_press_release), gtk_map_ruler);
  g_signal_connect_swapped (priv->map, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_motion), gtk_map_ruler);
}

static void
hyscan_gtk_map_ruler_object_finalize (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  g_list_free_full (priv->points, g_free);
  g_clear_object (&priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->finalize (object);
}

static gdouble
hyscan_gtk_map_ruler_measure (HyScanGeoGeodetic coord1,
                              HyScanGeoGeodetic coord2)
{
  double lat1r, lon1r, lat2r, lon2r, u, v;
  lat1r = DEG2RAD (coord1.lat);
  lon1r = DEG2RAD (coord1.lon);
  lat2r = DEG2RAD (coord2.lat);
  lon2r = DEG2RAD (coord2.lon);

  u = sin ((lat2r - lat1r) / 2);
  v = sin ((lon2r - lon1r) / 2);
  return 2.0 * EARTH_RADIUS * asin (sqrt (u * u + cos (lat1r) * cos (lat2r) * v * v));
}

static gdouble
hyscan_gtk_map_ruler_get_distance (HyScanGtkMapRulerPrivate *priv)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  GList *point_l;
  HyScanGtkMapRulerPoint *point;

  HyScanGeoGeodetic coord0, coord1;
  gdouble x, y;

  gdouble distance;

  /* Меньше двух точек, расстояние посчитать не получится. */
  if (priv->points == NULL || priv->points->next == NULL)
    return -1.0;

  point_l = priv->points;

  point = point_l->data;
  hyscan_gtk_map_value_to_geo (priv->map, &coord1, point->x, point->y);

  distance = 0.0;
  for (point_l = point_l->next; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;
      coord0 = coord1;

      hyscan_gtk_map_value_to_geo (priv->map, &coord1, point->x, point->y);

      distance += hyscan_gtk_map_ruler_measure (coord0, coord1);
    }

  return distance;
}

static void
hyscan_gtk_map_ruler_draw (HyScanGtkMapRuler *ruler,
                           cairo_t           *cairo)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  GList *point_l;
  HyScanGtkMapRulerPoint *point;

  gdouble x, y;

  cairo_set_line_width (cairo, 1.0);
  cairo_set_source_rgb (cairo, 0, 0, 0);

  /* Рисуем линию. */
  cairo_new_path (cairo);
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);
      cairo_line_to (cairo, x, y);
    }
  cairo_stroke (cairo);

  /* Рисуем узлы. */
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);

      cairo_move_to (cairo, x, y);
      if (point == priv->hover_point)
        {
          cairo_set_source_rgb (cairo, 0, 1.0, 0);
          cairo_arc (cairo, x, y, priv->radius * 2.0, 0, 2.0 * G_PI);
        }
      else
        {
          cairo_set_source_rgb (cairo, 1, 1, 1);
          cairo_arc (cairo, x, y, priv->radius, 0, 2.0 * G_PI);
        }

      cairo_set_source_rgb (cairo, 0.0, 0.0, 0.0);
      cairo_stroke_preserve (cairo);
      cairo_fill (cairo);
    }

  /* Подписываем расстояние над последней точкой. */
  {
    gdouble distance;

    distance = hyscan_gtk_map_ruler_get_distance (priv);
    if (distance > 0.0)
      {
        gchar label[255];

        cairo_move_to (cairo, x, y - 10);
        if (distance > 1000)
          g_snprintf (label, 255, "%.1f km", distance / 1000.0);
        else
          g_snprintf (label, 255, "%.1f m", distance);

        cairo_show_text (cairo, label);
      }
  }
}

/* Передвигает точку point под курсор. */
static void
hyscan_gtk_map_ruler_motion_drag (HyScanGtkMapRulerPrivate *priv,
                                  GdkEventMotion           *event,
                                  HyScanGtkMapRulerPoint   *point)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->x, &point->y);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Выделяет точку под курсором. */
static void
hyscan_gtk_map_ruler_motion_hover (HyScanGtkMapRulerPrivate *priv,
                                   GdkEventMotion           *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapRulerPoint *hover_point;
  GList *point_l;

  hover_point = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapRulerPoint *point = point_l->data;
      gdouble x, y;
      gdouble sensitive_radius;

      gtk_cifro_area_value_to_point (carea, &x, &y, point->x, point->y);

      sensitive_radius = priv->radius * 2.0;
      if (fabs (x - event->x) <= sensitive_radius && fabs (y - event->y) <= sensitive_radius)
        hover_point = point;
    }

  if (hover_point != priv->hover_point)
    {
      priv->hover_point = hover_point;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }
}

static gboolean
hyscan_gtk_map_ruler_motion (HyScanGtkMapRuler *ruler,
                             GdkEventMotion    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  if (priv->drag_point)
    hyscan_gtk_map_ruler_motion_drag (priv, event, priv->drag_point);
  else
    hyscan_gtk_map_ruler_motion_hover (priv, event);

  return FALSE;
}

/* Обработка нажатия мыши на узле. */
static gboolean
hyscan_gtk_map_ruler_btn_click_point (HyScanGtkMapRuler *ruler,
                                      GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  /* Двойной клик, планируем удаление точки. */
  if (event->type == GDK_2BUTTON_PRESS &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      priv->remove_point = priv->hover_point;

      return TRUE;
    }

  /* Мышь зажата на точке, начинаем ее перетаскивать. */
  if (event->type == GDK_BUTTON_PRESS &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      priv->drag_point = priv->hover_point;

      return TRUE;
    }

  /* Мышь отпущена во время удаления, отпускаем ввод. */
  if (event->type == GDK_BUTTON_RELEASE && priv->remove_point)
    {
      priv->points = g_list_remove (priv->points, priv->hover_point);
      g_clear_pointer (&priv->remove_point, g_free);
      priv->drag_point = NULL;
      priv->hover_point = NULL;

      hyscan_gtk_map_release_input (priv->map, ruler);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return TRUE;
    }

  /* Мышь отпущена во время перетаскивания, отпускаем ввод. */
  if (event->type == GDK_BUTTON_RELEASE && priv->drag_point)
    {
      priv->drag_point = NULL;

      hyscan_gtk_map_release_input (priv->map, ruler);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return TRUE;
    }


  return FALSE;
}

static gboolean
hyscan_gtk_map_ruler_btn_click_add (HyScanGtkMapRuler *ruler,
                                    GdkEventButton *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  if (event->type == GDK_BUTTON_RELEASE &&
      priv->remove_point == NULL &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
      HyScanGtkMapRulerPoint *point;

      point = g_new (HyScanGtkMapRulerPoint, 1);
      gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->x, &point->y);

      priv->points = g_list_append (priv->points, point);
      priv->hover_point = point;

      hyscan_gtk_map_release_input (priv->map, ruler);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return TRUE;
    }

  return FALSE;
}

static gboolean
hyscan_gtk_map_ruler_button_press_release (HyScanGtkMapRuler *ruler,
                                           GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != 1)
    return FALSE;

  if (priv->hover_point)
    return hyscan_gtk_map_ruler_btn_click_point (ruler, event);
  else
    return hyscan_gtk_map_ruler_btn_click_add (ruler, event);

}

HyScanGtkMapRuler *
hyscan_gtk_map_ruler_new (HyScanGtkMap *map)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_RULER, "map", map, NULL);
}
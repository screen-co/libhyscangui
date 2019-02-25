#include "hyscan-gtk-map-ruler.h"
#include <math.h>

#define SNAP_DISTANCE 6.0           /* Максимальное расстояние прилипания курсора мыши к звену ломаной. */
#define EARTH_RADIUS  6378137.0      /* Радиус Земли. */

#define IS_INSIDE(x, a, b) ((a) < (b) ? (a) < (x) && (x) < (b) : (b) < (x) && (x) < (a))
#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)

enum
{
  PROP_O,
};

struct _HyScanGtkMapRulerPrivate
{
  HyScanGtkMap              *map;                      /* Виджет карты. */

  PangoLayout               *pango_layout;             /* Раскладка шрифта. */

  GList                     *points;                   /* Список вершин ломаной, длину которой необходимо измерить. */
  HyScanGtkMapPoint         *hover_point;              /* Вершина ломаной под курсором мыши. */
  HyScanGtkMapPoint         *drag_point;               /* Вершина ломаной, которая сейчас перетаскивается. */
  HyScanGtkMapPoint         *remove_point;             /* Вершина ломаной, которую надо удалить при отжатии кнопки. */

  GList                     *hover_section;            /* Указатель на отрезок ломаной под курсором мыши. */
  HyScanGtkMapPoint          section_point;            /* Точка под курсором мыши в середине отрезка. */

  gdouble                    radius;                   /* Радиус точки - вершины ломаной. */
  gdouble                    line_width;               /* Ширина линии ломаной. */
  GdkRGBA                   *color;                    /* Цвет элементов. */
};

static void                     hyscan_gtk_map_ruler_set_property             (GObject                  *object,
                                                                               guint                     prop_id,
                                                                               const GValue             *value,
                                                                               GParamSpec               *pspec);
static void                     hyscan_gtk_map_ruler_object_constructed       (GObject                  *object);
static void                     hyscan_gtk_map_ruler_object_finalize          (GObject                  *object);
static void                     hyscan_gtk_map_ruler_draw                     (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static gboolean                 hyscan_gtk_map_ruler_button_press_release     (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventButton           *event);
static gboolean                 hyscan_gtk_map_ruler_motion                   (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventMotion           *event);
static GList *                  hyscan_gtk_map_ruler_get_segment_under_cursor (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventMotion           *event);
static HyScanGtkMapPoint * hyscan_gtk_map_ruler_get_point_under_cursor   (HyScanGtkMapRulerPrivate *priv,
                                                                               GdkEventMotion           *event);
static void                     hyscan_gtk_map_ruler_motion_hover             (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventMotion           *event);
static gboolean                 hyscan_gtk_map_ruler_btn_click_point          (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventButton           *event);
static gboolean                 hyscan_gtk_map_ruler_btn_click_add            (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventButton           *event);
static gdouble                  hyscan_gtk_map_ruler_measure                  (HyScanGeoGeodetic         coord1,
                                                                               HyScanGeoGeodetic         coord2);
static gdouble                  hyscan_gtk_map_ruler_get_distance             (HyScanGtkMap             *map,
                                                                               GList                    *points);
static void                     hyscan_gtk_map_ruler_motion_drag              (HyScanGtkMapRulerPrivate *priv,
                                                                               GdkEventMotion           *event,
                                                                               HyScanGtkMapPoint   *point);
static gdouble                  hyscan_gtk_map_ruler_distance                 (HyScanGtkMapPoint   *p1,
                                                                               HyScanGtkMapPoint   *p2,
                                                                               gdouble                   x0,
                                                                               gdouble                   y0,
                                                                               gdouble                  *x,
                                                                               gdouble                  *y);
static gboolean                 hyscan_gtk_map_ruler_configure                (HyScanGtkMap *map,
                                                                               GdkEvent                 *event,
                                                                               HyScanGtkMapRuler        *ruler);
static void                     hyscan_gtk_map_ruler_draw_vertices            (HyScanGtkMapRulerPrivate *priv,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_line                (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_hover_section       (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_label               (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_impl                (HyScanGtkMapPinLayer     *priv,
                                                                               cairo_t                  *cairo);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapRuler, hyscan_gtk_map_ruler, HYSCAN_TYPE_GTK_MAP_PIN_LAYER)

static void
hyscan_gtk_map_ruler_class_init (HyScanGtkMapRulerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapPinLayerClass *pin_class = HYSCAN_GTK_MAP_PIN_LAYER_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_ruler_set_property;
  object_class->constructed = hyscan_gtk_map_ruler_object_constructed;
  object_class->finalize = hyscan_gtk_map_ruler_object_finalize;

  pin_class->draw = hyscan_gtk_map_ruler_draw_impl;
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

  g_object_get (gtk_map_ruler,
                "map", &priv->map,
                "color", &priv->color,
                NULL);
  priv->radius = 3.0;
  priv->line_width = 1.0;

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  // g_signal_connect_swapped (priv->map, "visible-draw",
  //                           G_CALLBACK (hyscan_gtk_map_ruler_draw), gtk_map_ruler);
  //
  g_signal_connect (priv->map, "configure-event",
                    G_CALLBACK (hyscan_gtk_map_ruler_configure), gtk_map_ruler);
  // g_signal_connect_swapped (priv->map, "button-press-event",
  //                           G_CALLBACK (hyscan_gtk_map_ruler_button_press_release), gtk_map_ruler);
  // g_signal_connect_swapped (priv->map, "button-release-event",
  //                           G_CALLBACK (hyscan_gtk_map_ruler_button_press_release), gtk_map_ruler);
  g_signal_connect_swapped (priv->map, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_motion), gtk_map_ruler);
}

static void
hyscan_gtk_map_ruler_object_finalize (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->map);
  g_clear_pointer (&priv->color, gdk_rgba_free);

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->finalize (object);
}

/* Обработка сигнала "configure-event" виджета карты. */
static gboolean
hyscan_gtk_map_ruler_configure (HyScanGtkMap      *map,
                                GdkEvent          *event,
                                HyScanGtkMapRuler *ruler)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (map), NULL);

  return FALSE;
}

/* Определяет расстояние между двумя географическими координатами. */
static gdouble
hyscan_gtk_map_ruler_measure (HyScanGeoGeodetic coord1,
                              HyScanGeoGeodetic coord2)
{
  gdouble lon1r;
  gdouble lat1r;

  gdouble lon2r;
  gdouble lat2r;

  gdouble u;
  gdouble v;

  lat1r = DEG2RAD (coord1.lat);
  lon1r = DEG2RAD (coord1.lon);
  lat2r = DEG2RAD (coord2.lat);
  lon2r = DEG2RAD (coord2.lon);

  u = sin ((lat2r - lat1r) / 2);
  v = sin ((lon2r - lon1r) / 2);
  return 2.0 * EARTH_RADIUS * asin (sqrt (u * u + cos (lat1r) * cos (lat2r) * v * v));
}

/* Определяет общую длину ломаной по всем узлам или -1.0 если длина не определена. */
static gdouble
hyscan_gtk_map_ruler_get_distance (HyScanGtkMap *map,
                                   GList        *points)
{
  GList *point_l;
  HyScanGtkMapPoint *point;

  HyScanGeoGeodetic coord0, coord1;

  gdouble distance;

  /* Меньше двух точек, расстояние посчитать не получится. */
  if (points == NULL || points->next == NULL)
    return -1.0;

  point_l = points;
  point = point_l->data;
  hyscan_gtk_map_value_to_geo (map, &coord1, point->x, point->y);

  distance = 0.0;
  for (point_l = point_l->next; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      coord0 = coord1;
      hyscan_gtk_map_value_to_geo (map, &coord1, point->x, point->y);
      distance += hyscan_gtk_map_ruler_measure (coord0, coord1);
    }

  return distance;
}

/* Подписывает общее расстояние над последней вершиной ломаной. */
static void
hyscan_gtk_map_ruler_draw_label (HyScanGtkMapRuler *ruler,
                                 cairo_t           *cairo)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GtkCifroArea *carea;
  GList *points;
  HyScanGtkMapPoint *last_point;

  gdouble distance;

  gchar label[255];
  gint height;

  gdouble x;
  gdouble y;

  carea = GTK_CIFRO_AREA (priv->map);

  points = hyscan_gtk_map_pin_layer_get_points (HYSCAN_GTK_MAP_PIN_LAYER (ruler));
  distance = hyscan_gtk_map_ruler_get_distance (priv->map, points);

  if (distance < 0.0)
    return;

  if (distance < 1000.0)
    g_snprintf (label, sizeof (label), "%.0f m", distance);
  else
    g_snprintf (label, sizeof (label), "%.2f km", distance / 1000.0);

  pango_layout_set_text (priv->pango_layout, label, -1);
  pango_layout_get_size (priv->pango_layout, NULL, &height);
  height /= PANGO_SCALE;

  last_point = g_list_last (points)->data;
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, last_point->x, last_point->y);
  cairo_move_to (cairo, x + priv->radius, y - height - priv->radius);
  pango_cairo_show_layout (cairo, priv->pango_layout);
}

/* Рисует точку над звеном ломаной, если звено под курсором мыши. */
static void
hyscan_gtk_map_ruler_draw_hover_section (HyScanGtkMapRuler *ruler,
                                         cairo_t           *cairo)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GtkCifroArea *carea;

  gdouble x;
  gdouble y;

  if (priv->hover_section == NULL)
    return;

  carea = GTK_CIFRO_AREA (priv->map);

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, priv->section_point.x, priv->section_point.y);

  gdk_cairo_set_source_rgba (cairo, priv->color);
  cairo_set_line_width (cairo, priv->line_width);

  cairo_new_path (cairo);
  cairo_arc (cairo, x, y, priv->radius * 2.0, 0, 2.0 * G_PI);
  cairo_stroke (cairo);
}

/* Рисует ломаную линии. */
static void
hyscan_gtk_map_ruler_draw_line (HyScanGtkMapRuler *ruler,
                                cairo_t           *cairo)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  HyScanGtkMap *map = priv->map;
  GtkCifroArea *carea;

  GList *points;
  GList *point_l;
  HyScanGtkMapPoint *point = NULL;

  gdouble x;
  gdouble y;

  carea = GTK_CIFRO_AREA (map);

  gdk_cairo_set_source_rgba (cairo, priv->color);
  cairo_set_line_width (cairo, priv->line_width);

  cairo_new_path (cairo);
  points = hyscan_gtk_map_pin_layer_get_points (HYSCAN_GTK_MAP_PIN_LAYER (ruler));
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);
      cairo_line_to (cairo, x, y);
    }
  cairo_stroke (cairo);
}

/* Рисует элементы линейки по сигналу "visible-draw". */
static void
hyscan_gtk_map_ruler_draw_impl (HyScanGtkMapPinLayer *layer,
                                cairo_t              *cairo)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (layer);

  /* Родительский класс рисует сами метки. */
  HYSCAN_GTK_MAP_PIN_LAYER_CLASS (hyscan_gtk_map_ruler_parent_class)->draw (layer, cairo);

  /* Дальше рисуем соединения между метками и прочее. */
  hyscan_gtk_map_ruler_draw_line (ruler, cairo);
  hyscan_gtk_map_ruler_draw_label (ruler, cairo);
  hyscan_gtk_map_ruler_draw_hover_section (ruler, cairo);
}

/* Передвигает точку point под указателем мыши. */
static void
hyscan_gtk_map_ruler_motion_drag (HyScanGtkMapRulerPrivate *priv,
                                  GdkEventMotion           *event,
                                  HyScanGtkMapPoint        *point)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->x, &point->y);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Определяет расстояние от точки (x0, y0) до прямой через точки p1 и p2
 * и координаты (x, y) ближайшей точки на этой прямой. */
static gdouble
hyscan_gtk_map_ruler_distance (HyScanGtkMapPoint *p1,
                               HyScanGtkMapPoint *p2,
                               gdouble            x0,
                               gdouble            y0,
                               gdouble           *x,
                               gdouble           *y)
{
  gdouble dist;
  gdouble a, b, c;
  gdouble dist_ab2;

  a = p1->y - p2->y;
  b = p2->x - p1->x;
  c = p1->x * p2->y - p2->x * p1->y;

  dist_ab2 = pow (a, 2) + pow (b, 2);
  dist = fabs (a * x0 + b * y0 + c) / sqrt (dist_ab2);
  *x = (b * (b * x0 - a * y0) - a * c) / dist_ab2;
  *y = (a * (-b * x0 + a * y0) - b * c) / dist_ab2;

  return dist;
}

/* Находит звено ломаной в SNAP_DISTANCE-окрестности курсора мыши. */
static GList *
hyscan_gtk_map_ruler_get_segment_under_cursor (HyScanGtkMapRuler *ruler,
                                               GdkEventMotion    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  HyScanGtkMap *map = priv->map;
  GtkCifroArea *carea;
  GList *points;
  GList *point_l;

  gdouble cur_x, cur_y;
  gdouble scale_x, scale_y;

  carea = GTK_CIFRO_AREA (map);
  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);
  gtk_cifro_area_point_to_value (carea, event->x, event->y, &cur_x, &cur_y);
  points = hyscan_gtk_map_pin_layer_get_points (HYSCAN_GTK_MAP_PIN_LAYER (ruler));
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      HyScanGtkMapPoint *prev_point;

      gdouble distance;
      gdouble nearest_x, nearest_y;

      if (point_l->prev == NULL)
        continue;

      prev_point = point_l->prev->data;

      /* Надо, чтобы курсор мыши был между точками хотя бы по одной из осей. */
      if (!IS_INSIDE (cur_x, point->x, prev_point->x) && !IS_INSIDE (cur_y, point->y, prev_point->y))
        continue;

      /* Определяем расстояние от курсоры мыши до прямой между точками. */
      distance = hyscan_gtk_map_ruler_distance (prev_point, point, cur_x, cur_y, &nearest_x, &nearest_y);
      distance /= scale_x;
      if (distance > SNAP_DISTANCE)
        continue;

      /* Проверяем, что ближайшая точка на прямой попала внутрь отрезка между точками. */
      if (!IS_INSIDE (nearest_x, point->x, prev_point->x) || !IS_INSIDE (nearest_y, point->y, prev_point->y))
        continue;

      priv->section_point.x = nearest_x;
      priv->section_point.y = nearest_y;

      return point_l;
    }

  return NULL;
}

/* Определяет, находится ли под курсором один из узлов ломаной. */
static HyScanGtkMapPoint *
hyscan_gtk_map_ruler_get_point_under_cursor (HyScanGtkMapRulerPrivate *priv,
                                             GdkEventMotion           *event)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPoint *hover_point;
  GList *point_l;

  /* Под курсором мыши находится точка. */
  hover_point = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      gdouble x, y;
      gdouble sensitive_radius;

      gtk_cifro_area_value_to_point (carea, &x, &y, point->x, point->y);

      sensitive_radius = priv->radius * 2.0;
      if (fabs (x - event->x) <= sensitive_radius && fabs (y - event->y) <= sensitive_radius)
        {
          hover_point = point;
          break;
        }
    }

  return hover_point;
}

/* Выделяет точку под курсором мыши, если она находится на отрезке. */
static void
hyscan_gtk_map_ruler_motion_hover (HyScanGtkMapRuler *ruler,
                                   GdkEventMotion    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  HyScanGtkMapPoint *hover_point;
  GList *hover_section;

  /* Под курсором мыши находится конец какого-то отрезка. */
  hover_point = hyscan_gtk_map_ruler_get_point_under_cursor (priv, event);
  if (hover_point != priv->hover_point)
    {
      priv->hover_point = hover_point;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }

  /* Под курсором мыши находится точки внутри отрезка. */
  hover_section = hover_point == NULL ? hyscan_gtk_map_ruler_get_segment_under_cursor (ruler, event) : NULL;
  if (hover_section != NULL || priv->hover_section != hover_section)
    {
      priv->hover_section = hover_section;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }
}

/* Обрабатывает сигнал "motion-notify-event". */
static gboolean
hyscan_gtk_map_ruler_motion (HyScanGtkMapRuler *ruler,
                             GdkEventMotion    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  if (!hyscan_gtk_map_pin_layer_is_active (HYSCAN_GTK_MAP_PIN_LAYER (ruler)))
    return FALSE;

  if (FALSE && priv->drag_point)
    hyscan_gtk_map_ruler_motion_drag (priv, event, priv->drag_point);
  else
    hyscan_gtk_map_ruler_motion_hover (ruler, event);

  return FALSE;
}

/* Обработка нажатия мыши на вершине ломаной. */
static gboolean
hyscan_gtk_map_ruler_btn_click_point (HyScanGtkMapRuler *ruler,
                                      GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  /* Двойной клик, захватываем ввод и планируем удаление вершины. */
  if (event->type == GDK_2BUTTON_PRESS &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      priv->remove_point = priv->hover_point;

      return TRUE;
    }

  /* Мышь зажата на вершине, захватываем ввод и начинаем перетаскивать вершину. */
  if (event->type == GDK_BUTTON_PRESS &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      priv->drag_point = priv->hover_point;

      return TRUE;
    }

  /* Мышь отпущена во время удаления, удаляем вершину и отпускаем ввод. */
  if (event->type == GDK_BUTTON_RELEASE && priv->remove_point)
    {
      priv->points = g_list_remove (priv->points, priv->hover_point);
      g_clear_pointer (&priv->remove_point, hyscan_gtk_map_point_free);
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

/* Добавляет новый узел ломаной при нажатии кнопкой мыши. */
static gboolean
hyscan_gtk_map_ruler_btn_click_add (HyScanGtkMapRuler *ruler,
                                    GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  /* Мышь зажата между концами одного из отрезков.
   * Создаём точку и начинаем ее перетаскивать. */
  if (event->type == GDK_BUTTON_PRESS &&
      priv->hover_section != NULL &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      priv->hover_point = hyscan_gtk_map_point_copy (&priv->section_point);
      priv->points = g_list_insert_before (priv->points, priv->hover_section, priv->hover_point);
      priv->drag_point = priv->hover_point;

      priv->hover_section = NULL;

      return TRUE;
    }

  /* Просто где-то кликнули мышкой - добавляем точку в конец списка узлов. */
  if (event->type == GDK_BUTTON_RELEASE &&
      priv->remove_point == NULL &&
      hyscan_gtk_map_grab_input (priv->map, ruler))
    {
      HyScanGtkMapPoint point;

      gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.x, &point.y);
      priv->hover_point = hyscan_gtk_map_point_copy (&point);
      priv->points = g_list_append (priv->points, priv->hover_point);

      hyscan_gtk_map_release_input (priv->map, ruler);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return TRUE;
    }

  return FALSE;
}

/* Обрабатывает сигнал кнопки мыши "button-release-event". */
static gboolean
hyscan_gtk_map_ruler_button_press_release (HyScanGtkMapRuler *ruler,
                                           GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;

  if (!hyscan_gtk_map_pin_layer_is_active (HYSCAN_GTK_MAP_PIN_LAYER (ruler)))
    return FALSE;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != 1)
    return FALSE;

  if (priv->hover_point)
    return hyscan_gtk_map_ruler_btn_click_point (ruler, event);
  else
    return hyscan_gtk_map_ruler_btn_click_add (ruler, event);

}

/**
 * hyscan_gtk_map_ruler_new:
 * @map: виджет карты #HyScanGtkMap
 *
 * Создаёт новый слой с линейкой.
 *
 * Returns: новый объект #HyScanGtkMapRuler. Для удаления g_object_unref().
 */
HyScanGtkMapRuler *
hyscan_gtk_map_ruler_new (HyScanGtkMap *map)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_RULER, "map", map, NULL);
}

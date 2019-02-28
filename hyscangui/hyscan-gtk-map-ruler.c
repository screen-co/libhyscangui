#include "hyscan-gtk-map-ruler.h"
#include <math.h>

#define SNAP_DISTANCE       6.0           /* Максимальное расстояние прилипания курсора мыши к звену ломаной. */
#define EARTH_RADIUS        6378137.0     /* Радиус Земли. */

#define LINE_COLOR_DEFAULT  "rgba( 80, 120, 180, 0.5)"
#define LABEL_COLOR_DEFAULT "rgba( 33,  33,  33, 1.0)"
#define BG_COLOR_DEFAULT    "rgba(255, 255, 255, 0.6)"

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

  GList                     *hover_section;            /* Указатель на отрезок ломаной под курсором мыши. */
  HyScanGtkMapPoint          section_point;            /* Точка под курсором мыши в середине отрезка. */

  /* Стиль оформления. */
  gdouble                    radius;                   /* Радиус точки - вершины ломаной. */
  GdkRGBA                    line_color;               /* Цвет линии. */
  gdouble                    line_width;               /* Ширина линии. */
  gdouble                    label_padding;            /* Отступ подписи от края подложки. */
  GdkRGBA                    label_color;              /* Цвет текста подписи. */
  GdkRGBA                    bg_color;                 /* Цвет подложки подписи. */
};

static void                     hyscan_gtk_map_ruler_object_constructed       (GObject                  *object);
static void                     hyscan_gtk_map_ruler_object_finalize          (GObject                  *object);
static void                     hyscan_gtk_map_ruler_interface_init           (HyScanGtkLayerInterface  *iface);
static GList *                  hyscan_gtk_map_ruler_get_segment_under_cursor (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventMotion           *event);
static void                     hyscan_gtk_map_ruler_motion_notify            (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventMotion           *event);
static gboolean                 hyscan_gtk_map_ruler_btn_click_add            (HyScanGtkMapRuler        *ruler,
                                                                               GdkEventButton           *event);
static gdouble                  hyscan_gtk_map_ruler_measure                  (HyScanGeoGeodetic         coord1,
                                                                               HyScanGeoGeodetic         coord2);
static gdouble                  hyscan_gtk_map_ruler_get_distance             (HyScanGtkMap             *map,
                                                                               GList                    *points);
static gdouble                  hyscan_gtk_map_ruler_distance                 (HyScanGtkMapPoint   *p1,
                                                                               HyScanGtkMapPoint   *p2,
                                                                               gdouble                   x0,
                                                                               gdouble                   y0,
                                                                               gdouble                  *x,
                                                                               gdouble                  *y);
static gboolean                 hyscan_gtk_map_ruler_configure                (HyScanGtkMap *map,
                                                                               GdkEvent                 *event,
                                                                               HyScanGtkMapRuler        *ruler);
static void                     hyscan_gtk_map_ruler_draw_line                (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_hover_section       (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_label               (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_impl                (HyScanGtkMapPinLayer     *layer,
                                                                               cairo_t                  *cairo);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapRuler, hyscan_gtk_map_ruler, HYSCAN_TYPE_GTK_MAP_PIN_LAYER,
                         G_ADD_PRIVATE (HyScanGtkMapRuler)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_ruler_interface_init))

static void
hyscan_gtk_map_ruler_class_init (HyScanGtkMapRulerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapPinLayerClass *pin_class = HYSCAN_GTK_MAP_PIN_LAYER_CLASS (klass);

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
hyscan_gtk_map_ruler_object_constructed (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->constructed (object);

  hyscan_gtk_map_ruler_set_line_width (gtk_map_ruler, 2.0);
  gdk_rgba_parse (&color, LINE_COLOR_DEFAULT);
  hyscan_gtk_map_ruler_set_line_color (gtk_map_ruler, color);
  gdk_rgba_parse (&color, BG_COLOR_DEFAULT);
  hyscan_gtk_map_ruler_set_bg_color (gtk_map_ruler, color);
  gdk_rgba_parse (&color, LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_ruler_set_label_color (gtk_map_ruler, color);
  hyscan_gtk_map_pin_layer_set_pin_size (pin_layer, 4);
  hyscan_gtk_map_pin_layer_set_pin_shape (pin_layer, HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE);

  priv->label_padding = 5;
}

static void
hyscan_gtk_map_ruler_object_finalize (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->finalize (object);
}

static gpointer
hyscan_gtk_map_ruler_handle (HyScanGtkLayerContainer *container,
                             GdkEventMotion          *event,
                             HyScanGtkMapPinLayer    *layer)
{
  if (hyscan_gtk_map_ruler_get_segment_under_cursor (HYSCAN_GTK_MAP_RULER (layer), event) != NULL)
    return layer;

  return NULL;
}

static void
hyscan_gtk_map_ruler_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapRulerPrivate *priv = HYSCAN_GTK_MAP_RULER (gtk_layer)->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  /* Выполняем реализацию в родительском классе. */
  hyscan_gtk_layer_parent_interface->added (gtk_layer, container);

  priv->map = HYSCAN_GTK_MAP (container);

  g_signal_connect (priv->map, "configure-event",
                    G_CALLBACK (hyscan_gtk_map_ruler_configure), gtk_layer);
  g_signal_connect_swapped (priv->map, "button-release-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_btn_click_add), gtk_layer);
  g_signal_connect_swapped (priv->map, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_ruler_motion_notify), gtk_layer);
  g_signal_connect (priv->map, "handle",
                    G_CALLBACK (hyscan_gtk_map_ruler_handle), gtk_layer);
}

static void
hyscan_gtk_map_ruler_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->added = hyscan_gtk_map_ruler_added;
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
  gint width;
  gdouble margin = 5.0;

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
  pango_layout_get_size (priv->pango_layout, &width, &height);
  height /= PANGO_SCALE;
  width /= PANGO_SCALE;

  last_point = g_list_last (points)->data;
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, last_point->x, last_point->y);

  cairo_save (cairo);
  cairo_translate (cairo,
                   x + priv->radius + priv->label_padding,
                   y - height - priv->radius - priv->label_padding);

  cairo_rectangle (cairo,
                   -priv->label_padding, -priv->label_padding,
                   width  + 2.0 * priv->label_padding,
                   height + 2.0 * priv->label_padding);
  gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
  cairo_fill (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->label_color);
  cairo_move_to (cairo, 0, 0);
  pango_cairo_show_layout (cairo, priv->pango_layout);
  cairo_restore (cairo);
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

  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
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

  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
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

  /* Рисуем соединения между метками и прочее. */
  hyscan_gtk_map_ruler_draw_line (ruler, cairo);
  hyscan_gtk_map_ruler_draw_hover_section (ruler, cairo);

  /* Родительский класс рисует сами метки. */
  HYSCAN_GTK_MAP_PIN_LAYER_CLASS (hyscan_gtk_map_ruler_parent_class)->draw (layer, cairo);

  hyscan_gtk_map_ruler_draw_label (ruler, cairo);
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

  gconstpointer howner;

  /* Если какой-то хэндл захвачен или редактирование запрещено, то не реагируем на точки. */
  howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (map));
  if (howner != NULL)
    return NULL;
  if (!hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (map)))
    return NULL;

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

/* Выделяет точку под курсором мыши, если она находится на отрезке. */
static void
hyscan_gtk_map_ruler_motion_notify (HyScanGtkMapRuler *ruler,
                                    GdkEventMotion *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GList *hover_section;

  /* Под курсором мыши находится точки внутри отрезка. */
  hover_section = hyscan_gtk_map_ruler_get_segment_under_cursor (ruler, event);
  if (hover_section != NULL || priv->hover_section != hover_section)
    {
      priv->hover_section = hover_section;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }
}

/* Обработчик "button-release-event". Добавляет новый узел ломаной при отпускании кнопкой мыши. */
static gboolean
hyscan_gtk_map_ruler_btn_click_add (HyScanGtkMapRuler *ruler,
                                    GdkEventButton    *event)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (ruler);

  HyScanGtkLayerContainer *container;
  gconstpointer howner;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  if (howner != ruler)
    return GDK_EVENT_PROPAGATE;

  /* Мышь зажата между концами одного из отрезков.
   * Создаём точку и начинаем ее перетаскивать. */
  if (priv->hover_section != NULL)
    {
      HyScanGtkMapPoint *new_point;

      new_point = hyscan_gtk_map_pin_layer_insert_before (pin_layer, &priv->section_point, priv->hover_section);
      hyscan_gtk_map_pin_layer_start_drag (pin_layer, new_point);

      priv->hover_section = NULL;

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_grid_queue_draw (HyScanGtkMapRuler *ruler)
{
  if (ruler->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (ruler)))
    gtk_widget_queue_draw (GTK_WIDGET (ruler->priv->map));
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
hyscan_gtk_map_ruler_new ()
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_RULER, NULL);
}

void
hyscan_gtk_map_ruler_set_line_width (HyScanGtkMapRuler *ruler,
                                     gdouble            width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->line_width = width;
  ruler->priv->radius = 1.5 * width;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

void
hyscan_gtk_map_ruler_set_line_color (HyScanGtkMapRuler *ruler,
                                     GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->line_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

void
hyscan_gtk_map_ruler_set_label_color (HyScanGtkMapRuler *ruler,
                                      GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->label_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

void
hyscan_gtk_map_ruler_set_bg_color (HyScanGtkMapRuler *ruler,
                                   GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->bg_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

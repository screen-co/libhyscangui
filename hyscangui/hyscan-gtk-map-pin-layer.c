#include "hyscan-gtk-map-pin-layer.h"
#include <hyscan-gtk-layer.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP,
  PROP_COLOR,
  PROP_SHAPE,
  PROP_RADIUS,
};

typedef enum
{
  MODE_NONE,               /* Режим без взаимодейстия. */
  MODE_DRAG,               /* Режим перетаскивания точки. */
} HyScanGtkMapPinLayerMode;

typedef struct
{
  /* Проверяет, что точка виджета с координатами (x, y) находится на поверхности маркера point. */
  gboolean (*is_inside)    (HyScanGtkMapPinLayerPrivate *priv,
                            HyScanGtkMapPoint           *point,
                            gdouble                      x,
                            gdouble                      y);

  /* Рисует маркер point. */
  void     (*draw_vertex)  (HyScanGtkMapPinLayerPrivate *priv,
                            HyScanGtkMapPoint           *point,
                            cairo_t                     *cairo);
} HyScanGtkMapPinVtable;

struct _HyScanGtkMapPinLayerPrivate
{
  HyScanGtkMap              *map;                      /* Виджет карты. */

  PangoLayout               *pango_layout;             /* Раскладка шрифта. */

  HyScanGtkMapPinLayerMode   mode;                     /* Режим работы слоя. */

  GList                     *points;                   /* Список вершин ломаной, длину которой необходимо измерить. */
  HyScanGtkMapPoint         *hover_point;              /* Вершина ломаной под курсором мыши. */
  HyScanGtkMapPoint         *drag_point;               /* Вершина ломаной, которая перемещается в данный момент. */

  gdouble                    radius;                   /* Радиус точки - вершины ломаной. */
  GdkRGBA                   *color;                    /* Цвет элементов. */

  gboolean                   visible;                  /* Признак того, что слой виден. */
  HyScanGtkMapPinVtable      pin_vtable;               /* Таблица функций для отрисовки маркеров. */
};

static void          hyscan_gtk_map_pin_layer_set_property              (GObject                     *object,
                                                                         guint                        prop_id,
                                                                         const GValue                *value,
                                                                         GParamSpec                  *pspec);
static void          hyscan_gtk_map_pin_layer_get_property              (GObject                     *object,
                                                                         guint                        prop_id,
                                                                         GValue                      *value,
                                                                         GParamSpec                  *pspec);
static void          hyscan_gtk_map_pin_layer_object_constructed        (GObject                     *object);
static void          hyscan_gtk_map_pin_layer_object_finalize           (GObject                     *object);
static void          hyscan_gtk_map_pin_layer_interface_init            (HyScanGtkLayerInterface     *iface);
static void          hyscan_gtk_map_pin_layer_draw                      (HyScanGtkMapPinLayer        *pin_layer,
                                                                         cairo_t                     *cairo);
static gboolean      hyscan_gtk_map_pin_layer_button_release            (HyScanGtkLayer              *layer,
                                                                         GdkEventButton              *event);
static gboolean      hyscan_gtk_map_pin_layer_motion_notify             (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEventMotion              *event);
static void          hyscan_gtk_map_pin_layer_motion_hover              (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEventMotion              *event);
static void          hyscan_gtk_map_pin_layer_motion_drag               (HyScanGtkMapPinLayerPrivate *priv,
                                                                         GdkEventMotion              *event,
                                                                         HyScanGtkMapPoint           *point);
static gboolean      hyscan_gtk_map_pin_layer_key_press                 (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEventKey                 *event);
static gboolean      hyscan_gtk_map_pin_layer_configure                 (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEvent                    *event);
static void          hyscan_gtk_map_pin_layer_draw_vertices             (HyScanGtkMapPinLayerPrivate *priv,
                                                                         cairo_t                     *cairo);
static void          hyscan_gtk_map_pin_layer_draw_pins                 (HyScanGtkMapPinLayer        *pin_layer,
                                                                         cairo_t                     *cairo);
static HyScanGtkMapPoint *   hyscan_gtk_map_pin_layer_get_point_at      (HyScanGtkMapPinLayer        *pin_layer,
                                                                         gdouble                      x,
                                                                         gdouble                      y);
static gpointer      hyscan_gtk_map_pin_layer_handle                    (HyScanGtkLayerContainer     *container,
                                                                         GdkEventMotion              *event,
                                                                         HyScanGtkMapPinLayer        *layer);
static void          hyscan_gtk_map_pin_layer_draw_vertex_c             (HyScanGtkMapPinLayerPrivate *priv,
                                                                         HyScanGtkMapPoint           *point,
                                                                         cairo_t                     *cairo);
static void          hyscan_gtk_map_pin_layer_draw_vertex               (HyScanGtkMapPinLayerPrivate *priv,
                                                                         HyScanGtkMapPoint           *point,
                                                                         cairo_t                     *cairo);
static gboolean      hyscan_gtk_map_pin_layer_is_inside_c               (HyScanGtkMapPinLayerPrivate *priv,
                                                                         HyScanGtkMapPoint           *point,
                                                                         gdouble                      x,
                                                                         gdouble                      y);
static gboolean      hyscan_gtk_map_pin_layer_is_inside                 (HyScanGtkMapPinLayerPrivate *priv,
                                                                         HyScanGtkMapPoint           *point,
                                                                         gdouble                      x,
                                                                         gdouble                      y);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPinLayer, hyscan_gtk_map_pin_layer, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapPinLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_pin_layer_interface_init))

static void
hyscan_gtk_map_pin_layer_class_init (HyScanGtkMapPinLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_pin_layer_set_property;
  object_class->get_property = hyscan_gtk_map_pin_layer_get_property;
  object_class->constructed = hyscan_gtk_map_pin_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_pin_layer_object_finalize;

  klass->draw = hyscan_gtk_map_pin_layer_draw_pins;

  g_object_class_install_property (object_class, PROP_COLOR,
    g_param_spec_boxed ("color", "Color", "GdkRGBA", GDK_TYPE_RGBA,
                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_RADIUS,
    g_param_spec_uint ("radius", "Marker Radius", "", 1, G_MAXUINT, 5,
                       G_PARAM_WRITABLE));

  g_object_class_install_property (object_class, PROP_SHAPE,
    g_param_spec_uint ("shape", "Marker Shape", "",
                       HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE,
                       HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN,
                       HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE,
                       G_PARAM_WRITABLE));
}

static void
hyscan_gtk_map_pin_layer_init (HyScanGtkMapPinLayer *gtk_map_pin_layer)
{
  gtk_map_pin_layer->priv = hyscan_gtk_map_pin_layer_get_instance_private (gtk_map_pin_layer);
}

static void
hyscan_gtk_map_pin_layer_get_property (GObject        *object,
                                       guint           prop_id,
                                       GValue         *value,
                                       GParamSpec     *pspec)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      g_value_set_object (value, priv->map);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, priv->color);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_pin_layer_set_shape (HyScanGtkMapPinLayer *layer,
                                    guint                 shape)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  switch (shape)
    {
    case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN:
      priv->pin_vtable.draw_vertex = hyscan_gtk_map_pin_layer_draw_vertex;
      priv->pin_vtable.is_inside   = hyscan_gtk_map_pin_layer_is_inside;
      return;

     case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE:
       priv->pin_vtable.draw_vertex = hyscan_gtk_map_pin_layer_draw_vertex_c;
       priv->pin_vtable.is_inside   = hyscan_gtk_map_pin_layer_is_inside_c;
       return;

     default:
       g_warning ("Unknown shape %u", shape);
    }
}

static void
hyscan_gtk_map_pin_layer_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
      break;

    case PROP_COLOR:
      priv->color = g_value_dup_boxed (value);
      break;

    case PROP_SHAPE:
      hyscan_gtk_map_pin_layer_set_shape (gtk_map_pin_layer, g_value_get_uint (value));
      break;

    case PROP_RADIUS:
      priv->radius = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_pin_layer_object_constructed (GObject *object)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_pin_layer_parent_class)->constructed (object);

  priv->mode = MODE_NONE;

  if (priv->color == NULL)
    {
      GdkRGBA default_color;
      gdk_rgba_parse (&default_color, "#ffffff");
      priv->color = gdk_rgba_copy (&default_color);
    }

  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (gtk_map_pin_layer), TRUE);
}

/* Обработчик сигнала "handle". Определяет, есть ли под указателем мыши хэндл. */
static gpointer
hyscan_gtk_map_pin_layer_handle (HyScanGtkLayerContainer *container,
                                 GdkEventMotion          *event,
                                 HyScanGtkMapPinLayer    *layer)
{
  if (hyscan_gtk_map_pin_layer_get_point_at (layer, event->x, event->y))
    return layer;

  return NULL;
}

static void
hyscan_gtk_map_pin_layer_added (HyScanGtkLayer          *gtk_layer,
                                HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (gtk_layer)->priv;

  /* Проверяем, что контенейнер - это карта. */
  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  priv->map = g_object_ref (container);

  /* Сигналы контейнера. */
  g_signal_connect (container, "handle", G_CALLBACK (hyscan_gtk_map_pin_layer_handle), gtk_layer);

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  g_signal_connect_swapped (container, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_draw), gtk_layer);

  /* Сигналы GtkWidget. */
  g_signal_connect_swapped (container, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_configure), gtk_layer);
  g_signal_connect_swapped (container, "button-release-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_button_release), gtk_layer);
  g_signal_connect_swapped (container, "key-press-event",
                             G_CALLBACK (hyscan_gtk_map_pin_layer_key_press), gtk_layer);
  g_signal_connect_swapped (container, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_motion_notify), gtk_layer);
}

static void
hyscan_gtk_map_pin_layer_set_visible (HyScanGtkLayer *layer,
                                      gboolean        visible)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_pin_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  return priv->visible;
}

static void
hyscan_gtk_map_pin_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_pin_layer_added;
  iface->set_visible = hyscan_gtk_map_pin_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_pin_layer_get_visible;
}

static void
hyscan_gtk_map_pin_layer_object_finalize (GObject *object)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->map);
  g_clear_pointer (&priv->color, gdk_rgba_free);

  G_OBJECT_CLASS (hyscan_gtk_map_pin_layer_parent_class)->finalize (object);
}

/* Обработка сигнала "configure-event" виджета карты. */
static gboolean
hyscan_gtk_map_pin_layer_configure (HyScanGtkMapPinLayer *pin_layer,
                                    GdkEvent          *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);

  return FALSE;
}

/* Рисует метку в виде точки. */
static void
hyscan_gtk_map_pin_layer_draw_vertex_c (HyScanGtkMapPinLayerPrivate *priv,
                                        HyScanGtkMapPoint           *point,
                                        cairo_t                     *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  gdouble radius;
  gdouble x, y;

  radius = (point == priv->hover_point) ? 2.0 * priv->radius : priv->radius;

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);

  cairo_arc (cairo, x, y, radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, priv->color);
  cairo_fill (cairo);
}

/* Рисует метку в виде булавки. */
static void
hyscan_gtk_map_pin_layer_draw_vertex (HyScanGtkMapPinLayerPrivate *priv,
                                      HyScanGtkMapPoint           *point,
                                      cairo_t                     *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  gdouble radius;
  gdouble h;
  gdouble x, y;

  radius = (point == priv->hover_point) ? 1.2 * priv->radius : priv->radius;
  h = 2.0 * radius;

  gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);

  /* Тень под маркером. */
  {
    GdkRGBA color = {.red = .5, .green = .5, .blue = .5, .alpha = .3};

    cairo_save (cairo);
    cairo_translate(cairo, x, y);
    cairo_scale (cairo, 1.0, 0.33);
    cairo_translate(cairo, -x, -y);

    cairo_arc (cairo, x, y, 0.8 * radius, 0, 2.0 * G_PI);
    gdk_cairo_set_source_rgba (cairo, &color);
    cairo_fill (cairo);
    cairo_restore (cairo);
  }

  /* Сам маркер. */
  {
    GdkRGBA color = {.red = 1.0, .green = 1.0, .blue = 1.0, .alpha = .9};

    cairo_new_path (cairo);
    cairo_line_to (cairo, x, y);
    cairo_arc (cairo, x, y - h, radius, -G_PI - G_PI / 10.0, G_PI / 10.0);
    cairo_close_path (cairo);
    gdk_cairo_set_source_rgba (cairo, priv->color);
    cairo_fill (cairo);

    cairo_new_sub_path (cairo);
    cairo_arc (cairo, x, y - h, 0.5 * radius, 0, 2.0 * G_PI);
    gdk_cairo_set_source_rgba (cairo, &color);
    cairo_fill (cairo);
  }
}

/* Определяет, покрывает ли изображение маркера @point виджет в точке x, y. */
static gboolean
hyscan_gtk_map_pin_layer_is_inside (HyScanGtkMapPinLayerPrivate *priv,
                                    HyScanGtkMapPoint           *point,
                                    gdouble                      x,
                                    gdouble                      y)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  gdouble x_point, y_point;
  gdouble radius;
  gdouble h;

  gtk_cifro_area_value_to_point (carea, &x_point, &y_point, point->x, point->y);

  radius = (point == priv->hover_point) ? 1.2 * priv->radius : priv->radius;
  h = 2.0 * radius;

  return fabs (x - x_point) <= radius && y < y_point && y > (y_point - h - radius);
}

/* Рисует узлы ломаной линии. */
static void
hyscan_gtk_map_pin_layer_draw_vertices (HyScanGtkMapPinLayerPrivate *priv,
                                        cairo_t                     *cairo)
{
  GList *point_l;

  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    priv->pin_vtable.draw_vertex (priv, point_l->data, cairo);
}

/* Рисует элементы линейки по сигналу "visible-draw". */
static void
hyscan_gtk_map_pin_layer_draw_pins (HyScanGtkMapPinLayer *pin_layer,
                                    cairo_t              *cairo)
{
  hyscan_gtk_map_pin_layer_draw_vertices (pin_layer->priv, cairo);
}

/* Рисует элементы линейки по сигналу "visible-draw". */
static void
hyscan_gtk_map_pin_layer_draw (HyScanGtkMapPinLayer *pin_layer,
                               cairo_t           *cairo)
{
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (pin_layer)))
    return;

  HYSCAN_GTK_MAP_PIN_LAYER_GET_CLASS (pin_layer)->draw (pin_layer, cairo);
}

/* Передвигает точку point под курсор. */
static void
hyscan_gtk_map_pin_layer_motion_drag (HyScanGtkMapPinLayerPrivate *priv,
                                      GdkEventMotion              *event,
                                      HyScanGtkMapPoint           *point)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->x, &point->y);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Выделяет точку под курсором мыши, если она находится на отрезке. */
static void
hyscan_gtk_map_pin_layer_motion_hover (HyScanGtkMapPinLayer     *pin_layer,
                                       GdkEventMotion           *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  HyScanGtkMapPoint *hover_point;

  /* Под курсором мыши находится конец какого-то отрезка. */
  hover_point = hyscan_gtk_map_pin_layer_get_point_at (pin_layer, event->x, event->y);
  if (hover_point != priv->hover_point)
    {
      priv->hover_point = hover_point;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }
}

/* Обрабатывает сигнал "motion-notify-event". */
static gboolean
hyscan_gtk_map_pin_layer_motion_notify (HyScanGtkMapPinLayer *pin_layer,
                                        GdkEventMotion       *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_pin_layer_motion_drag (priv, event, priv->drag_point);
  else
    hyscan_gtk_map_pin_layer_motion_hover (pin_layer, event);

  return FALSE;
}

/* Обработка Gdk-событий, которые дошли из контейнера. */
static gboolean
hyscan_gtk_map_pin_layer_key_press (HyScanGtkMapPinLayer *pin_layer,
                                    GdkEventKey          *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  if (event->keyval == GDK_KEY_Delete && priv->mode == MODE_DRAG)
    {
      priv->points = g_list_remove (priv->points, priv->drag_point);
      g_clear_pointer (&priv->drag_point, hyscan_gtk_map_point_free);
      priv->mode = MODE_NONE;
      
      hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
hyscan_gtk_map_pin_layer_stop_drag (HyScanGtkMapPinLayerPrivate *priv)
{
  priv->mode = MODE_NONE;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
}

/* Обработка "button-release-event" на слое. */
static gboolean
hyscan_gtk_map_pin_layer_button_release (HyScanGtkLayer *layer,
                                         GdkEventButton *event)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  HyScanGtkLayerContainer *container;
  gconstpointer iowner;
  gconstpointer howner;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  iowner = hyscan_gtk_layer_container_get_input_owner (container);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  if ((howner != layer) && (howner != NULL || iowner != layer))
    return GDK_EVENT_PROPAGATE;

  if (priv->mode == MODE_NONE && priv->hover_point == NULL && howner == NULL && iowner == layer)
    /* Курсор над пустым местом - добавляем новую точку. */
    {
      HyScanGtkMapPoint point;

      gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.x, &point.y);
      hyscan_gtk_map_pin_layer_insert_before (pin_layer, &point, NULL);
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }
  else if (priv->mode == MODE_NONE && priv->hover_point != NULL && howner == layer)
    /* Курсор над точкой - начинаем её перетаскивать. */
    {
      hyscan_gtk_map_pin_layer_start_drag (pin_layer, priv->hover_point);

      return GDK_EVENT_STOP;
    }
  else if (priv->mode == MODE_DRAG)
    /* Мышь тянет точку - заканчиваем перетаскивать. */
    {
      hyscan_gtk_map_pin_layer_stop_drag (priv);

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Определяет, покрывает ли изображение маркера @point виджет в точке x, y. */
static gboolean
hyscan_gtk_map_pin_layer_is_inside_c (HyScanGtkMapPinLayerPrivate *priv,
                                      HyScanGtkMapPoint           *point,
                                      gdouble                      x,
                                      gdouble                      y)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  gdouble x_point, y_point;
  gdouble sensitive_radius;

  gtk_cifro_area_value_to_point (carea, &x_point, &y_point, point->x, point->y);

  sensitive_radius = priv->radius * 2.0;

  return fabs (x - x_point) <= sensitive_radius && fabs (y - y_point) <= sensitive_radius;
}

/* Находит точку на слое в окрестности указанных координат виджета. */
static HyScanGtkMapPoint *
hyscan_gtk_map_pin_layer_get_point_at (HyScanGtkMapPinLayer *pin_layer,
                                       gdouble               x,
                                       gdouble               y)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  HyScanGtkMapPoint *hover_point;
  GList *point_l;

  /* Под курсором мыши находится точка. */
  hover_point = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;

      if (priv->pin_vtable.is_inside (priv, point, x, y))
        {
          hover_point = point;
          break;
        }
    }

  return hover_point;
}

/**
 * hyscan_gtk_map_pin_layer_new:
 * @map: виджет карты #HyScanGtkMap
 *
 * Создаёт новый слой с булавками. На слое размещаются отметки различных
 * местоположений.
 *
 * Returns: новый объект #HyScanGtkMapPinLayer. Для удаления g_object_unref().
 */
HyScanGtkMapPinLayer *
hyscan_gtk_map_pin_layer_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PIN_LAYER, NULL);
}

/**
 * hyscan_gtk_map_pin_layer_clear:
 * @pin_layer: указатель на #HyScanGtkMapPinLayer
 *
 * Удаляет все точки на слое.
 */
void
hyscan_gtk_map_pin_layer_clear (HyScanGtkMapPinLayer *pin_layer)
{
  HyScanGtkMapPinLayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (pin_layer));
  priv = pin_layer->priv;

  if (priv->points == NULL)
    return;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  priv->points = NULL;
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_pin_layer_get_points:
 * @pin_layer: указатель на #HyScanGtkMapPinLayer
 *
 * Возвращает список точек #HyScanGtkMapPoint в слое.
 *
 * Returns: (transfer none): (element-type HyScanGtkMapPoint): список точек
 */
GList *
hyscan_gtk_map_pin_layer_get_points (HyScanGtkMapPinLayer *pin_layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (pin_layer), NULL);

  return pin_layer->priv->points;
}

HyScanGtkMap *
hyscan_gtk_map_pin_layer_get_map (HyScanGtkMapPinLayer *pin_layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (pin_layer), NULL);

  return pin_layer->priv->map;
}

/**
 * hyscan_gtk_map_pin_layer_get_color:
 * @pin_layer
 * Returns: (transfer none):
 */
GdkRGBA *
hyscan_gtk_map_pin_layer_get_color (HyScanGtkMapPinLayer *pin_layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (pin_layer), NULL);

  return pin_layer->priv->color;
}

/**
 * hyscan_gtk_map_pin_layer_insert_before:
 * @pin_layer:
 * @point:
 * @sibling:
 *
 * Добавляет @point в список точек слоя, помещая её перед указанной позицией @sibling.
 *
 * Returns: (transfer none): указатель на добавленную точку.
 */
HyScanGtkMapPoint *
hyscan_gtk_map_pin_layer_insert_before  (HyScanGtkMapPinLayer *pin_layer,
                                         HyScanGtkMapPoint    *point,
                                         GList                *sibling)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  HyScanGtkMapPoint *new_point;

  new_point = hyscan_gtk_map_point_copy (point);
  priv->points = g_list_insert_before (priv->points, sibling, new_point);

  return new_point;
}

/**
 * hyscan_gtk_map_pin_layer_start_drag:
 * @layer: указатель на #HyScanGtkMapPinLayer
 * @handle_point: указатель на точку #HyScanGtkMapPoint на слое @layer
 *
 * Начинает перетаскивание точки @handle_point.
 */
void
hyscan_gtk_map_pin_layer_start_drag (HyScanGtkMapPinLayer *layer,
                                     HyScanGtkMapPoint    *handle_point)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (layer)->priv;
  gconstpointer howner;
  HyScanGtkLayerContainer *container;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  /* Проверяем, что хэндл у нас, но мы ещё не в режиме перемещения. */
  if (howner != layer || priv->mode != MODE_NONE)
    return;

  priv->mode = MODE_DRAG;
  priv->drag_point = handle_point;
  hyscan_gtk_layer_container_set_handle_grabbed (container, layer);

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

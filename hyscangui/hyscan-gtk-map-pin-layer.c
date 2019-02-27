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

enum
{
  PIN_NORMAL,
  PIN_HOVER,
  PIN_DRAG,
};

typedef enum
{
  MODE_NONE,               /* Режим без взаимодейстия. */
  MODE_DRAG,               /* Режим перетаскивания точки. */
} HyScanGtkMapPinLayerMode;

typedef struct
{
  cairo_surface_t           *surface;      /* Поверхность маркера. */

  gdouble                    offset_x;     /* Координата x проколотой точки на поверхности surface. */
  gdouble                    offset_y;     /* Координата y проколотой точки на поверхности surface. */

  /* Хэндл маркера считаем прямоугольным. */
  gdouble                    handle_x0;    /* Координата x верхней левой точки хэндла. */
  gdouble                    handle_y0;    /* Координата y верхней левой точки хэндла. */
  gdouble                    handle_x1;    /* Координата x нижней правой точки хэндла. */
  gdouble                    handle_y1;    /* Координата y нижней правой точки хэндла. */
} HyScanGtkMapPinLayerPin;

typedef HyScanGtkMapPinLayerPin * (*HyScanGtkMapCreatePinFunc) (HyScanGtkMapPinLayer *layer,
                                                                guint                 state);

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

  HyScanGtkMapPinLayerPin   *pin;                      /* Внешний вид обычного маркера. */
  HyScanGtkMapPinLayerPin   *pin_hover;                /* Внешний вид маркера под указателем мыши. */
  HyScanGtkMapCreatePinFunc  pin_create_func;          /* Фабрика маркеров. */

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
static void          hyscan_gtk_map_pin_layer_update_marker             (HyScanGtkMapPinLayer        *layer);
static void          hyscan_gtk_map_pin_layer_pin_free                  (HyScanGtkMapPinLayerPin     *pin);
static void          hyscan_gtk_map_pin_layer_set_shape                 (HyScanGtkMapPinLayer        *layer,
                                                                         guint                        shape);
static void          hyscan_gtk_map_pin_layer_added                     (HyScanGtkLayer              *gtk_layer,
                                                                         HyScanGtkLayerContainer     *container);
static void          hyscan_gtk_map_pin_layer_set_visible               (HyScanGtkLayer              *layer,
                                                                         gboolean                     visible);
static gboolean      hyscan_gtk_map_pin_layer_get_visible               (HyScanGtkLayer              *layer);

static HyScanGtkMapPinLayerPin *
                     hyscan_gtk_map_pin_layer_create_marker_pin         (HyScanGtkMapPinLayer        *layer,
                                                                         guint                        state);
static HyScanGtkMapPinLayerPin *
                     hyscan_gtk_map_pin_layer_create_marker_circle      (HyScanGtkMapPinLayer        *layer,
                                                                         guint                        state);
static inline HyScanGtkMapPinLayerPin *
                     hyscan_gtk_map_pin_layer_get_pin                   (HyScanGtkMapPinLayerPrivate *priv,
                                                                         HyScanGtkMapPoint           *point);

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

/* Создаёт метку в виде круга. */
static HyScanGtkMapPinLayerPin *
hyscan_gtk_map_pin_layer_create_marker_circle (HyScanGtkMapPinLayer *layer,
                                               guint                 state)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;
  gdouble radius;

  HyScanGtkMapPinLayerPin *pin;
  cairo_t *cairo;
  GdkRGBA color;

  /* Увеличиваем размер для выделенных меток. */
  radius = (state == PIN_HOVER) ? 2.0 * priv->radius : priv->radius;

  pin = g_slice_new (HyScanGtkMapPinLayerPin);
  pin->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, (guint) (2.0 * radius), (guint) (2.0 * radius));
  pin->offset_x = radius;
  pin->offset_y = radius;
  pin->handle_x0 = -radius;
  pin->handle_y0 = -radius;
  pin->handle_x1 = radius;
  pin->handle_y1 = radius;

  /* Основной внешний круг. */
  cairo = cairo_create (pin->surface);
  cairo_arc (cairo, radius, radius, radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, priv->color);
  cairo_fill (cairo);

  /* Внутренний круг белого цвета. */
  gdk_rgba_parse (&color, "#fff");
  cairo_arc (cairo, radius, radius, .5 * radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &color);
  cairo_fill (cairo);

  cairo_destroy (cairo);

  return pin;
}

/* Создаёт маркер в виде булавки. */
static HyScanGtkMapPinLayerPin *
hyscan_gtk_map_pin_layer_create_marker_pin (HyScanGtkMapPinLayer *layer,
                                            guint                 state)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  HyScanGtkMapPinLayerPin *pin;
  cairo_t *cairo;

  gdouble radius;
  gdouble r_shadow;
  gdouble bezier_param;
  gdouble x, y;
  guint w, h;

  /* Увеличиваем размер для выделенных меток. */
  radius = (state == PIN_HOVER) ? 1.2 * priv->radius : priv->radius;

  /* Определяем размеры пропорционально радиусу. */
  r_shadow = radius * 0.1;
  bezier_param = radius * 4.0;
  x = bezier_param * .33;
  y = bezier_param * .75;

  w = (guint) ceil (2.0 * x);
  h = (guint) ceil (y + r_shadow);

  pin = g_slice_new (HyScanGtkMapPinLayerPin);
  pin->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  pin->offset_x = x;
  pin->offset_y = h;
  pin->handle_x0 = -x;
  pin->handle_y0 = -y;
  pin->handle_x1 = x;
  pin->handle_y1 = 0;

  cairo = cairo_create (pin->surface);
  /* Тень под маркером. */
  {
    GdkRGBA color = {.red = 0, .green = 0, .blue = 0, .alpha = .2};

    cairo_save (cairo);
    cairo_translate(cairo, x, y);
    cairo_scale (cairo, 3.0, 1.0);
    cairo_translate(cairo, -x, -y);

    cairo_arc (cairo, x, y, r_shadow, 0, 2.0 * G_PI);
    gdk_cairo_set_source_rgba (cairo, &color);
    cairo_fill (cairo);
    cairo_restore (cairo);
  }

  /* Сам маркер. */
  {
    GdkRGBA color = {.red = 1.0, .green = 1.0, .blue = 1.0, .alpha = .9};

    /* Кривая Безье в форме булавки. */
    cairo_new_path (cairo);
    cairo_move_to (cairo, x, y);
    cairo_rel_curve_to (cairo, -bezier_param, -bezier_param, bezier_param, -bezier_param, 0, 0);
    cairo_close_path (cairo);

    /* Заливка. */
    gdk_cairo_set_source_rgba (cairo, priv->color);
    cairo_fill_preserve (cairo);

    /* Обводка. */
    cairo_set_line_width (cairo, 1.0);
    gdk_cairo_set_source_rgba (cairo, &color);
    cairo_stroke (cairo);

    /* Круглое ушко в булавке. */
    cairo_new_sub_path (cairo);
    cairo_arc (cairo, x, y * .33, 0.5 * radius, 0, 2.0 * G_PI);
    gdk_cairo_set_source_rgba (cairo, &color);
    cairo_fill (cairo);
  }

  cairo_destroy (cairo);

  return pin;
}

/* Установка формы маркера. */
static void
hyscan_gtk_map_pin_layer_set_shape (HyScanGtkMapPinLayer *layer,
                                    guint                 shape)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  switch (shape)
    {
    case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN:
      priv->pin_create_func = hyscan_gtk_map_pin_layer_create_marker_pin;
      return;

     case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE:
       priv->pin_create_func = hyscan_gtk_map_pin_layer_create_marker_circle;
       return;

     default:
       g_warning ("Unknown shape %u", shape);
    }
}

/* Освобождает память, занятую структурой pin. */
static void
hyscan_gtk_map_pin_layer_pin_free (HyScanGtkMapPinLayerPin *pin)
{
  g_clear_pointer (&pin->surface, cairo_surface_destroy);
  g_slice_free (HyScanGtkMapPinLayerPin, pin);
}

/* Пересоздаёт маркеры. */
static void
hyscan_gtk_map_pin_layer_update_marker (HyScanGtkMapPinLayer *layer)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  g_clear_pointer (&priv->pin, hyscan_gtk_map_pin_layer_pin_free);
  g_clear_pointer (&priv->pin_hover, hyscan_gtk_map_pin_layer_pin_free);
  priv->pin = priv->pin_create_func (layer, PIN_NORMAL);
  priv->pin_hover = priv->pin_create_func (layer, PIN_HOVER);
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
      hyscan_gtk_map_pin_layer_update_marker (gtk_map_pin_layer);
      break;

    case PROP_SHAPE:
      hyscan_gtk_map_pin_layer_set_shape (gtk_map_pin_layer, g_value_get_uint (value));
      hyscan_gtk_map_pin_layer_update_marker (gtk_map_pin_layer);
      break;

    case PROP_RADIUS:
      priv->radius = g_value_get_uint (value);
      hyscan_gtk_map_pin_layer_update_marker (gtk_map_pin_layer);
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
  if (hyscan_gtk_map_pin_layer_get_point_at (layer, event->x, event->y) != NULL)
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
                                    GdkEvent             *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);

  return FALSE;
}

/* Рисует узлы ломаной линии. */
static void
hyscan_gtk_map_pin_layer_draw_vertices (HyScanGtkMapPinLayerPrivate *priv,
                                        cairo_t                     *cairo)
{
  GList *point_l;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPinLayerPin *pin;

  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      gdouble x, y;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);
      pin = hyscan_gtk_map_pin_layer_get_pin (priv, point);

      cairo_set_source_surface (cairo, pin->surface, x - pin->offset_x, y - pin->offset_y);
      cairo_paint (cairo);
    }
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
                               cairo_t              *cairo)
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

  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return GDK_EVENT_PROPAGATE;

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

/* Возвращает маркер, соответствующий точке point. */
static inline HyScanGtkMapPinLayerPin *
hyscan_gtk_map_pin_layer_get_pin (HyScanGtkMapPinLayerPrivate *priv,
                                  HyScanGtkMapPoint           *point)
{
  return (point == priv->hover_point) ? priv->pin_hover : priv->pin;
}

/* Находит точку на слое в окрестности указанных координат виджета. */
static HyScanGtkMapPoint *
hyscan_gtk_map_pin_layer_get_point_at (HyScanGtkMapPinLayer *pin_layer,
                                       gdouble               x,
                                       gdouble               y)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPoint *hover_point;
  GList *point_l;
  HyScanGtkMapPinLayerPin *pin;

  gconstpointer howner;

  /* Если какой-то хэндл захвачен или редактирование запрещено, то не реагируем на точки. */
  howner = hyscan_gtk_layer_container_get_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map));
  if (howner != NULL)
    return NULL;

  if (!hyscan_gtk_layer_container_get_changes_allowed (HYSCAN_GTK_LAYER_CONTAINER (priv->map)))
    return NULL;

  /* Проверяем каждую точку, лежит ли она в поле. */
  hover_point = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      gdouble x_point, y_point;

      gtk_cifro_area_value_to_point (carea, &x_point, &y_point, point->x, point->y);
      pin = hyscan_gtk_map_pin_layer_get_pin (priv, point);

      if (x_point + pin->handle_x0 < x && x < x_point + pin->handle_x1 &&
          y_point + pin->handle_y0 < y && y < y_point + pin->handle_y1)
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

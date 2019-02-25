#include "hyscan-gtk-map-pin-layer.h"
#include <hyscan-gtk-layer.h>
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP,
  PROP_COLOR,
};

typedef enum
{
  MODE_NONE,               /* Режим без взаимодейстия. */
  MODE_DRAG,               /* Режим перетаскивания точки. */
} HyScanGtkMapPinLayerMode;

struct _HyScanGtkMapPinLayerPrivate
{
  HyScanGtkMap              *map;                      /* Виджет карты. */

  PangoLayout               *pango_layout;             /* Раскладка шрифта. */

  HyScanGtkMapPinLayerMode   mode;                     /* Режим работы слоя. */

  GList                     *points;                   /* Список вершин ломаной, длину которой необходимо измерить. */
  HyScanGtkMapPoint         *hover_point;              /* Вершина ломаной под курсором мыши. */


  gdouble                    radius;                   /* Радиус точки - вершины ломаной. */
  GdkRGBA                   *color;                    /* Цвет элементов. */

  gboolean                   visible;                  /* Признак того, что слой виден. */
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
static gboolean      hyscan_gtk_map_pin_layer_interact                  (HyScanGtkLayer              *layer,
                                                                         GdkEvent                    *event);
static gboolean      hyscan_gtk_map_pin_layer_motion                    (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEventMotion              *event);
static void          hyscan_gtk_map_pin_layer_motion_hover              (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEventMotion              *event);
static void          hyscan_gtk_map_pin_layer_motion_drag               (HyScanGtkMapPinLayerPrivate *priv,
                                                                         GdkEventMotion              *event,
                                                                         HyScanGtkMapPoint           *point);
static gboolean      hyscan_gtk_map_pin_layer_configure                 (HyScanGtkMapPinLayer        *pin_layer,
                                                                         GdkEvent                    *event);
static void          hyscan_gtk_map_pin_layer_draw_vertices             (HyScanGtkMapPinLayerPrivate *priv,
                                                                         cairo_t                     *cairo);
static void          hyscan_gtk_map_pin_layer_draw_pins                 (HyScanGtkMapPinLayer        *pin_layer,
                                                                         cairo_t                     *cairo);

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
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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
  priv->radius = 5.0;

  if (priv->color == NULL)
    {
      GdkRGBA default_color;
      gdk_rgba_parse (&default_color, "#ffffff");
      priv->color = gdk_rgba_copy (&default_color);
    }

  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (gtk_map_pin_layer), TRUE);
}

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
                             G_CALLBACK (hyscan_gtk_map_pin_layer_interact), gtk_layer);
  g_signal_connect_swapped (container, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_motion), gtk_layer);
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

/* Рисует узлы ломаной линии. */
static void
hyscan_gtk_map_pin_layer_draw_vertices (HyScanGtkMapPinLayerPrivate *priv,
                                        cairo_t                  *cairo)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  GList *point_l;
  HyScanGtkMapPoint *point = NULL;

  gdouble x, y;

  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      gdouble radius;
      point = point_l->data;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->x, point->y);

      radius = (point == priv->hover_point) ? 2.0 * priv->radius : priv->radius;

      cairo_arc (cairo, x, y, radius, 0, 2.0 * G_PI);
      gdk_cairo_set_source_rgba (cairo, priv->color);
      cairo_fill (cairo);
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
hyscan_gtk_map_pin_layer_motion (HyScanGtkMapPinLayer *pin_layer,
                                 GdkEventMotion       *event)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  // if (!priv->active)
  //   return FALSE;

  if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_pin_layer_motion_drag (priv, event, priv->hover_point);
  else
    hyscan_gtk_map_pin_layer_motion_hover (pin_layer, event);

  return FALSE;
}

/* Обработка Gdk-событий, которые дошли из контейнера. */
static gboolean
hyscan_gtk_map_pin_layer_interact (HyScanGtkLayer *layer,
                                   GdkEvent       *event)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (layer)->priv;
  HyScanGtkLayerContainer *container;

  GdkEventButton *event_btn;
  gconstpointer iowner;
  gconstpointer howner;

  if (event->type != GDK_BUTTON_RELEASE)
    return GDK_EVENT_PROPAGATE;

  event_btn = (GdkEventButton *) event;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event_btn->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  iowner = hyscan_gtk_layer_container_get_input_owner (container);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  switch (priv->mode)
    {
    case MODE_NONE:
      if (priv->hover_point == NULL && iowner == layer && howner == NULL)
        /* Курсор над пустым местом - добавляем новую точку. */
        {
          HyScanGtkMapPoint point;

          gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event_btn->x, event_btn->y, &point.x, &point.y);
          priv->hover_point = hyscan_gtk_map_point_copy (&point);
          priv->points = g_list_append (priv->points, priv->hover_point);
          priv->mode = MODE_NONE;
          hyscan_gtk_layer_container_set_handle_grabbed (container, NULL);
          gtk_widget_queue_draw (GTK_WIDGET (priv->map));

          return GDK_EVENT_STOP;
        }
      else if (priv->hover_point != NULL && howner == layer)
        /* Курсор над точкой - начинаем её перетаскивать. */
        {
          priv->mode = MODE_DRAG;
          hyscan_gtk_layer_container_set_handle_grabbed (container, layer);
          gtk_widget_queue_draw (GTK_WIDGET (priv->map));

          return GDK_EVENT_STOP;
        }

      return GDK_EVENT_PROPAGATE;

    case MODE_DRAG:
      priv->mode = MODE_NONE;

      hyscan_gtk_layer_container_set_handle_grabbed (container, NULL);
      return GDK_EVENT_STOP; /* Нажатие обработано. */
    }

  return GDK_EVENT_PROPAGATE;
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
hyscan_gtk_map_pin_layer_new (GdkRGBA     *color)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PIN_LAYER,
                       "color", color, NULL);
}

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
 * Returns: (transfer-none):
 */
GdkRGBA *
hyscan_gtk_map_pin_layer_get_color (HyScanGtkMapPinLayer *pin_layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (pin_layer), NULL);

  return pin_layer->priv->color;
}


HyScanGtkMapPoint *
hyscan_gtk_map_pin_layer_get_point_at (HyScanGtkMapPinLayer *pin_layer,
                                       gdouble               x,
                                       gdouble               y)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPoint *hover_point;
  GList *point_l;

  /* Под курсором мыши находится точка. */
  hover_point = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      gdouble x_point, y_point;
      gdouble sensitive_radius;

      gtk_cifro_area_value_to_point (carea, &x_point, &y_point, point->x, point->y);

      sensitive_radius = priv->radius * 2.0;
      if (fabs (x - x_point) <= sensitive_radius && fabs (y - y_point) <= sensitive_radius)
        {
          hover_point = point;
          break;
        }
    }

  return hover_point;
}

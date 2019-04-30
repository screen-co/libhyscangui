/* hyscan-gtk-map-pin-layer.c
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
 * SECTION: hyscan-gtk-map-pin-layer
 * @Short_description: Слой с отметками географических точек.
 * @Title: HyScanGtkLayerContainer
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Данный слой позволяет отмечать на карте некоторые целевые точки. Каждая точка
 * отмечается маркером, внешний вид которого можно задать с помощью функций:
 *
 * - hyscan_gtk_map_pin_layer_set_color_prime()    "color-prime"
 * - hyscan_gtk_map_pin_layer_set_color_second()   "color-second"
 * - hyscan_gtk_map_pin_layer_set_color_stroke()   "color-stroke"
 * - hyscan_gtk_map_pin_layer_set_pin_size()
 * - hyscan_gtk_map_pin_layer_set_pin_shape()
 *
 */

#include "hyscan-gtk-map-pin-layer.h"
#include <hyscan-gtk-layer.h>
#include <math.h>

#define DEFAULT_SIZE          8
#define DEFAULT_COLOR_PRIME   "#995599"
#define DEFAULT_COLOR_STROKE  "#884488"
#define DEFAULT_COLOR_SECOND  "#ffffff"

enum
{
  PROP_O,
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

  gdouble                    offset_x;     /* Координата x целевой точки на поверхности маркера. */
  gdouble                    offset_y;     /* Координата y целевой точки на поверхности маркера. */

  /* Область на поверхности маркера, клик по которой считается захватом хэндла. */
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

  /* Стиль оформления. */
  gdouble                    pin_size;                 /* Размер маркера (фактический размер макера зависит от
                                                        * его формы, см. pin_create_func). */
  GdkRGBA                    pin_color_prime;          /* Основной цвет маркера. */
  GdkRGBA                    pin_color_second;         /* Вспомогательный цвет маркера. */
  GdkRGBA                    pin_color_stroke;         /* Цвет обводки маркера. */
  gdouble                    pin_stroke_width;

  gboolean                   visible;                  /* Признак того, что слой виден. */

  HyScanGtkMapPinLayerPin   *pin;                      /* Внешний вид обычного маркера. */
  HyScanGtkMapPinLayerPin   *pin_hover;                /* Внешний вид маркера под указателем мыши. */
  HyScanGtkMapCreatePinFunc  pin_create_func;          /* Фабрика маркеров. */
};

static void          hyscan_gtk_map_pin_layer_object_constructed        (GObject                     *object);
static void          hyscan_gtk_map_pin_layer_object_finalize           (GObject                     *object);
static void          hyscan_gtk_map_pin_layer_interface_init            (HyScanGtkLayerInterface     *iface);
static void          hyscan_gtk_map_pin_layer_draw                      (HyScanGtkMap                *map,
                                                                         cairo_t                     *cairo,
                                                                         HyScanGtkMapPinLayer        *pin_layer);
static gboolean      hyscan_gtk_map_pin_layer_button_release            (GtkWidget                   *widget,
                                                                         GdkEventButton              *event,
                                                                         HyScanGtkLayer              *layer);
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
static void          hyscan_gtk_map_pin_layer_draw_vertices             (HyScanGtkMapPinLayer        *pin_layer,
                                                                         cairo_t                     *cairo);
static HyScanGtkMapPoint *   hyscan_gtk_map_pin_layer_get_point_at      (HyScanGtkMapPinLayer        *pin_layer,
                                                                         gdouble                      x,
                                                                         gdouble                      y);
static gconstpointer hyscan_gtk_map_pin_layer_handle_grab               (HyScanGtkLayerContainer     *container,
                                                                         GdkEventMotion              *event,
                                                                         HyScanGtkMapPinLayer        *layer);
static void          hyscan_gtk_map_pin_layer_update_marker             (HyScanGtkMapPinLayer        *layer);
static void          hyscan_gtk_map_pin_layer_pin_free                  (HyScanGtkMapPinLayerPin     *pin);
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

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPinLayer, hyscan_gtk_map_pin_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapPinLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_pin_layer_interface_init))

static void
hyscan_gtk_map_pin_layer_class_init (HyScanGtkMapPinLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_pin_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_pin_layer_object_finalize;

  klass->draw = hyscan_gtk_map_pin_layer_draw_vertices;
}

static void
hyscan_gtk_map_pin_layer_init (HyScanGtkMapPinLayer *gtk_map_pin_layer)
{
  gtk_map_pin_layer->priv = hyscan_gtk_map_pin_layer_get_instance_private (gtk_map_pin_layer);
}

/* Создаёт метку в виде круга. */
static HyScanGtkMapPinLayerPin *
hyscan_gtk_map_pin_layer_create_marker_circle (HyScanGtkMapPinLayer *layer,
                                               guint                 state)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;
  gdouble radius;
  gdouble margin;
  guint width;

  HyScanGtkMapPinLayerPin *pin;
  cairo_t *cairo;

  /* Увеличиваем размер для выделенных меток. */
  radius = (state == PIN_HOVER) ? 2.0 * priv->pin_size : priv->pin_size;

  margin = 2.0 * priv->pin_stroke_width;
  width = (guint) ceil (2.0 * radius + 2.0 * margin);

  pin = g_slice_new (HyScanGtkMapPinLayerPin);
  pin->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, width);
  pin->offset_x = radius + margin;
  pin->offset_y = radius + margin;
  pin->handle_x0 = -radius;
  pin->handle_y0 = -radius;
  pin->handle_x1 = radius;
  pin->handle_y1 = radius;

  /* Основной внешний круг. */
  cairo = cairo_create (pin->surface);
  cairo_arc (cairo, pin->offset_x, pin->offset_y, radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_prime);
  cairo_fill_preserve (cairo);
  cairo_set_line_width (cairo, priv->pin_stroke_width);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_stroke);
  cairo_stroke (cairo);

  /* Внутренний круг белого цвета. */
  cairo_arc (cairo, pin->offset_x, pin->offset_y, .5 * radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_second);
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
  radius = (state == PIN_HOVER) ? 1.2 * priv->pin_size : priv->pin_size;

  /* Определяем размеры пропорционально радиусу. */
  r_shadow = radius * 0.1;
  bezier_param = radius * 4.0;
  x = bezier_param * .33;
  y = bezier_param * .77;

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
    cairo_save (cairo);
    cairo_translate(cairo, x, y);
    cairo_scale (cairo, 3.0, 1.0);
    cairo_translate(cairo, -x, -y);

    cairo_arc (cairo, x, y, r_shadow, 0, 2.0 * G_PI);
    cairo_set_source_rgba (cairo, 0, 0, 0, .2);
    cairo_fill (cairo);
    cairo_restore (cairo);
  }

  /* Сам маркер. */
  {
    /* Кривая Безье в форме булавки. */
    cairo_new_path (cairo);
    cairo_move_to (cairo, x, y);
    cairo_rel_curve_to (cairo, -bezier_param, -bezier_param, bezier_param, -bezier_param, 0, 0);
    cairo_close_path (cairo);

    /* Заливка. */
    gdk_cairo_set_source_rgba (cairo, &priv->pin_color_prime);
    cairo_fill_preserve (cairo);

    /* Обводка. */
    cairo_set_line_width (cairo, priv->pin_stroke_width);
    gdk_cairo_set_source_rgba (cairo, &priv->pin_color_stroke);
    cairo_stroke (cairo);

    /* Круглое ушко в булавке. */
    cairo_new_sub_path (cairo);
    cairo_arc (cairo, x, y * .33, 0.5 * radius, 0, 2.0 * G_PI);
    gdk_cairo_set_source_rgba (cairo, &priv->pin_color_second);
    cairo_fill (cairo);
  }

  cairo_destroy (cairo);

  return pin;
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
hyscan_gtk_map_pin_layer_object_constructed (GObject *object)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;
  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_pin_layer_parent_class)->constructed (object);

  priv->mode = MODE_NONE;
  gtk_map_pin_layer->priv->pin_stroke_width = 1.0;

  hyscan_gtk_map_pin_layer_set_pin_shape (gtk_map_pin_layer, HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN);
  hyscan_gtk_map_pin_layer_set_pin_size (gtk_map_pin_layer, DEFAULT_SIZE);
  gdk_rgba_parse (&color, DEFAULT_COLOR_PRIME);
  hyscan_gtk_map_pin_layer_set_color_prime (gtk_map_pin_layer, color);
  gdk_rgba_parse (&color, DEFAULT_COLOR_STROKE);
  hyscan_gtk_map_pin_layer_set_color_stroke (gtk_map_pin_layer, color);
  gdk_rgba_parse (&color, DEFAULT_COLOR_SECOND);
  hyscan_gtk_map_pin_layer_set_color_second (gtk_map_pin_layer, color);

  hyscan_gtk_layer_set_visible (HYSCAN_GTK_LAYER (gtk_map_pin_layer), TRUE);
}

/* Обработчик сигнала "handle-grab". Захватывает хэндл под указателем мыши. */
static gconstpointer
hyscan_gtk_map_pin_layer_handle_grab (HyScanGtkLayerContainer *container,
                                      GdkEventMotion          *event,
                                      HyScanGtkMapPinLayer    *layer)
{
  HyScanGtkMapPoint *point;

  point = hyscan_gtk_map_pin_layer_get_point_at (layer, event->x, event->y);
  if (point != NULL)
    return hyscan_gtk_map_pin_layer_start_drag (layer, point);

  return NULL;
}

/* Обработчик сигнала "handle-release".
 * Возвращает %TRUE, если мы разрешаем отпустить хэндл. */
static gboolean
hyscan_gtk_map_pin_layer_handle_release (HyScanGtkLayerContainer *container,
                                         GdkEventMotion          *event,
                                         HyScanGtkMapPinLayer    *layer)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;
  HyScanGtkMapPoint *drag_point;

  if (hyscan_gtk_layer_container_get_handle_grabbed (container) != layer)
    return FALSE;

  if (priv->mode != MODE_DRAG)
    return FALSE;

  priv->mode = MODE_NONE;
  drag_point = priv->drag_point;
  hyscan_gtk_map_value_to_geo (priv->map, &drag_point->geo, drag_point->c2d);

  return TRUE;
}

static void
hyscan_gtk_map_pin_layer_point_update (HyScanGtkMapPoint *point,
                                       HyScanGtkMap      *map)
{
  hyscan_gtk_map_geo_to_value (map, point->geo, &point->c2d);
}

/* Обновляет координаты точек при смене картографической проекции. */
static void
hyscan_gtk_map_pin_layer_projection_notify (HyScanGtkMap         *map,
                                            GParamSpec           *pspec,
                                            HyScanGtkMapPinLayer *layer)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  g_list_foreach (priv->points, (GFunc) hyscan_gtk_map_pin_layer_point_update, map);
}

static void
hyscan_gtk_map_pin_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (gtk_layer)->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gtk_layer);
  g_clear_object (&priv->map);
}

/* Регистрирует слой в карте. */
static void
hyscan_gtk_map_pin_layer_added (HyScanGtkLayer          *gtk_layer,
                                HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (gtk_layer)->priv;

  /* Проверяем, что контенейнер - это карта. */
  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);

  /* Сигналы контейнера. */
  g_signal_connect (container, "handle-grab", G_CALLBACK (hyscan_gtk_map_pin_layer_handle_grab), gtk_layer);
  g_signal_connect (container, "handle-release", G_CALLBACK (hyscan_gtk_map_pin_layer_handle_release), gtk_layer);

  /* Сигналы карты. */
  g_signal_connect (container, "notify::projection", G_CALLBACK (hyscan_gtk_map_pin_layer_projection_notify), gtk_layer);

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  g_signal_connect_after (container, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_pin_layer_draw), gtk_layer);

  /* Сигналы GtkWidget. */
  g_signal_connect_after (container, "button-release-event",
                          G_CALLBACK (hyscan_gtk_map_pin_layer_button_release), gtk_layer);
  g_signal_connect_swapped (container, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_configure), gtk_layer);
  g_signal_connect_swapped (container, "key-press-event",
                             G_CALLBACK (hyscan_gtk_map_pin_layer_key_press), gtk_layer);
  g_signal_connect_swapped (container, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_pin_layer_motion_notify), gtk_layer);
}

/* Устанавливает видимость слоя.
 * Реализация HyScanGtkLayerInterface.set_visible().*/
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

/* Возвращает %TRUE, если слой видимый.
 * Реализация HyScanGtkLayerInterface.get_visible(). */
static gboolean
hyscan_gtk_map_pin_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  return priv->visible;
}

/* Захватывает пользовательский ввод в контейнере.
 * Реализация HyScanGtkLayerInterface.grab_input(). */
static gboolean
hyscan_gtk_map_pin_layer_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv =pin_layer->priv;

  if (priv->map == NULL)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);

  return TRUE;
}

static gboolean
hyscan_gtk_map_pin_layer_load_key_file (HyScanGtkLayer *gtk_layer,
                                        GKeyFile       *key_file,
                                        const gchar    *group)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (gtk_layer);
  GdkRGBA color;

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "color-stroke", DEFAULT_COLOR_STROKE);
  hyscan_gtk_map_pin_layer_set_color_stroke (pin_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "color-prime", DEFAULT_COLOR_PRIME);
  hyscan_gtk_map_pin_layer_set_color_prime (pin_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "color-second", DEFAULT_COLOR_SECOND);
  hyscan_gtk_map_pin_layer_set_color_second (pin_layer, color);

  return TRUE;
}

/* Реализация интерфейса HyScanGtkLayerInterface. */
static void
hyscan_gtk_map_pin_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_pin_layer_added;
  iface->removed = hyscan_gtk_map_pin_layer_removed;
  iface->grab_input = hyscan_gtk_map_pin_layer_grab_input;
  iface->set_visible = hyscan_gtk_map_pin_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_pin_layer_get_visible;
  iface->load_key_file = hyscan_gtk_map_pin_layer_load_key_file;
}

static void
hyscan_gtk_map_pin_layer_object_finalize (GObject *object)
{
  HyScanGtkMapPinLayer *gtk_map_pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (object);
  HyScanGtkMapPinLayerPrivate *priv = gtk_map_pin_layer->priv;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_clear_object (&priv->pango_layout);
  g_clear_pointer (&priv->pin, hyscan_gtk_map_pin_layer_pin_free);
  g_clear_pointer (&priv->pin_hover, hyscan_gtk_map_pin_layer_pin_free);

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

/* Рисует отметки на карте.
 * Реализация HyScanGtkMapPinLayerClass.draw().*/
static void
hyscan_gtk_map_pin_layer_draw_vertices (HyScanGtkMapPinLayer *pin_layer,
                                        cairo_t              *cairo)
{
  GList *point_l;
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPinLayerPin *pin;

  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPoint *point = point_l->data;
      gdouble x, y;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->c2d.x, point->c2d.y);
      pin = hyscan_gtk_map_pin_layer_get_pin (priv, point);

      cairo_set_source_surface (cairo, pin->surface, x - pin->offset_x, y - pin->offset_y);
      cairo_paint (cairo);
    }
}

/* Рисует элементы слоя по сигналу "visible-draw". */
static void
hyscan_gtk_map_pin_layer_draw (HyScanGtkMap         *map,
                               cairo_t              *cairo,
                               HyScanGtkMapPinLayer *pin_layer)
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

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->c2d.x, &point->c2d.y);
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

/* Обработка "button-release-event" на слое.
 * Создаёт новую точку в том месте, где пользователь кликнул мышью. */
static gboolean
hyscan_gtk_map_pin_layer_button_release (GtkWidget      *widget,
                                         GdkEventButton *event,
                                         HyScanGtkLayer *layer)
{
  HyScanGtkMapPinLayer *pin_layer = HYSCAN_GTK_MAP_PIN_LAYER (layer);
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;

  HyScanGtkLayerContainer *container;
  HyScanGtkMapPoint point;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);

  if (!hyscan_gtk_layer_container_get_changes_allowed (container))
    return GDK_EVENT_PROPAGATE;

  /* Проверяем, что у нас есть право ввода. */
  if (hyscan_gtk_layer_container_get_input_owner (container) != layer)
    return GDK_EVENT_PROPAGATE;

  if (priv->mode != MODE_NONE)
    return GDK_EVENT_PROPAGATE;

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.c2d.x, &point.c2d.y);
  hyscan_gtk_map_pin_layer_insert_before (pin_layer, &point, NULL);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
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

      gtk_cifro_area_value_to_point (carea, &x_point, &y_point, point->c2d.x, point->c2d.y);
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

static void
hyscan_gtk_map_grid_queue_draw (HyScanGtkMapPinLayer *layer)
{
  if (layer->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (layer)))
    gtk_widget_queue_draw (GTK_WIDGET (layer->priv->map));
}

/**
 * hyscan_gtk_map_pin_layer_new:
 * @map: виджет карты #HyScanGtkMap
 *
 * Создаёт новый слой с булавками. На слое размещаются отметки различных
 * местоположений.
 *
 * Returns: новый объект #HyScanGtkMapPinLayer.
 */
HyScanGtkLayer *
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
hyscan_gtk_map_pin_layer_clear (HyScanGtkMapPinLayer *layer)
{
  HyScanGtkMapPinLayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer));
  priv = layer->priv;

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
hyscan_gtk_map_pin_layer_get_points (HyScanGtkMapPinLayer *layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer), NULL);

  return layer->priv->points;
}

/**
 * hyscan_gtk_map_pin_layer_insert_before:
 * @pin_layer:
 * @point:
 * @sibling:
 *
 * Добавляет @point в список точек слоя, помещая её перед указанной позицией @sibling.
 *
 * Returns: (transfer none): указатель на добавленную точку
 */
HyScanGtkMapPoint *
hyscan_gtk_map_pin_layer_insert_before  (HyScanGtkMapPinLayer *pin_layer,
                                         HyScanGtkMapPoint    *point,
                                         GList                *sibling)
{
  HyScanGtkMapPinLayerPrivate *priv = pin_layer->priv;
  HyScanGtkMapPoint *new_point;

  new_point = hyscan_gtk_map_point_copy (point);
  hyscan_gtk_map_value_to_geo (priv->map, &new_point->geo, new_point->c2d);
  priv->points = g_list_insert_before (priv->points, sibling, new_point);

  return new_point;
}

/**
 * hyscan_gtk_map_pin_layer_start_drag:
 * @layer: указатель на #HyScanGtkMapPinLayer
 * @handle_point: указатель на точку #HyScanGtkMapPoint на слое @layer
 *
 * Начинает перетаскивание точки @handle_point.
 *
 * Returns: указатель на хэндл, если началось перетаскивание; иначе %NULL
 */
gconstpointer
hyscan_gtk_map_pin_layer_start_drag (HyScanGtkMapPinLayer *layer,
                                     HyScanGtkMapPoint    *handle_point)
{
  HyScanGtkMapPinLayerPrivate *priv = HYSCAN_GTK_MAP_PIN_LAYER (layer)->priv;
  gconstpointer howner;
  HyScanGtkLayerContainer *container;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  /* Проверяем, что хэндл свободен и мы ещё не в режиме перемещения. */
  if (howner != NULL)
    {
      g_warning ("HyScanGtkMapPinLayer: failed to start drag - handle is grabbed");
      return NULL;
    }
  if (priv->mode == MODE_DRAG)
    {
      g_warning ("HyScanGtkMapPinLayer: failed to start drag - already in drag mode");
      return NULL;
    }

  priv->mode = MODE_DRAG;
  priv->drag_point = handle_point;

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return layer;
}

void
hyscan_gtk_map_pin_layer_set_pin_size (HyScanGtkMapPinLayer *layer,
                                       guint                 size)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer));

  layer->priv->pin_size = size;
  hyscan_gtk_map_pin_layer_update_marker (layer);
  hyscan_gtk_map_grid_queue_draw (layer);
}

void
hyscan_gtk_map_pin_layer_set_color_prime (HyScanGtkMapPinLayer *layer,
                                          GdkRGBA color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer));

  layer->priv->pin_color_prime = color;
  hyscan_gtk_map_pin_layer_update_marker (layer);
  hyscan_gtk_map_grid_queue_draw (layer);
}

void
hyscan_gtk_map_pin_layer_set_color_second (HyScanGtkMapPinLayer *layer,
                                           GdkRGBA               color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer));

  layer->priv->pin_color_second = color;
  hyscan_gtk_map_pin_layer_update_marker (layer);
  hyscan_gtk_map_grid_queue_draw (layer);
}

void
hyscan_gtk_map_pin_layer_set_color_stroke (HyScanGtkMapPinLayer *layer,
                                           GdkRGBA               color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN_LAYER (layer));

  layer->priv->pin_color_stroke = color;
  hyscan_gtk_map_pin_layer_update_marker (layer);
  hyscan_gtk_map_grid_queue_draw (layer);
}


/* Установка формы маркера. */
void
hyscan_gtk_map_pin_layer_set_pin_shape (HyScanGtkMapPinLayer         *layer,
                                        HyScanGtkMapPinLayerPinShape  shape)
{
  HyScanGtkMapPinLayerPrivate *priv = layer->priv;

  switch (shape)
    {
    case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_PIN:
      priv->pin_create_func = hyscan_gtk_map_pin_layer_create_marker_pin;
      break;

    case HYSCAN_GTK_MAP_PIN_LAYER_SHAPE_CIRCLE:
      priv->pin_create_func = hyscan_gtk_map_pin_layer_create_marker_circle;
      break;

    default:
      g_warning ("HyScanGtkMapPinLayer: unknown shape %u", shape);
      return;
    }

  hyscan_gtk_map_pin_layer_update_marker (layer);
  hyscan_gtk_map_grid_queue_draw (layer);
}

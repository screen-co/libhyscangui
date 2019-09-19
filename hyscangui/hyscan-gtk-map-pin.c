/* hyscan-gtk-map-pin.c
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
 * SECTION: hyscan-gtk-map-pin
 * @Short_description: Слой с отметками географических точек.
 * @Title: HyScanGtkMapPin
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Данный слой позволяет отмечать на карте некоторые целевые точки. Каждая точка
 * отмечается маркером, внешний вид которого можно задать с помощью функций:
 *
 * - hyscan_gtk_map_pin_set_color_prime(), "prime-color" - основной цвет,
 * - hyscan_gtk_map_pin_set_color_second(), "second-color" - вспомогательный цвет,
 * - hyscan_gtk_map_pin_set_color_stroke(), "stroke-color" - цвет обводки
 * - hyscan_gtk_map_pin_set_marker_size() - размер маркера
 * - hyscan_gtk_map_pin_set_marker_shape() - форма маркера
 *
 * Пользователь может взаимодействовать со слоем:
 * - чтобы добавить точку, необходимо передать право ввода слою и кликнуть по карте.
 * - чтобы активизировать точку, необходимо клинкуть по этой точке; после этого
 *   можно выполнить одно из следующих действий:
 *   - переместить активную точку, кликнув на новое место на карте,
 *   - отменить перемещение, нажав клавишу Escape,
 *   - удалить активную точку, нажав клавишу Delete.
 *
 */

#include "hyscan-gtk-map-pin.h"
#include <math.h>

#define DEFAULT_SIZE          8
#define DEFAULT_COLOR_PRIME   "#699857"
#define DEFAULT_COLOR_STROKE  "#537845"
#define DEFAULT_COLOR_SECOND  "#ffffff"

#define HANDLE_TYPE_NAME "map_pin"

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
} HyScanGtkMapPinMode;

/* Отдельная метка. */
struct _HyScanGtkMapPinItem
{
  HyScanGtkMapPoint  point;   /* Координаты. */
  gchar             *title;   /* Название. */
  gint               number;  /* Порядковый номер. */
};

/* Маркер - визуальное представление метки. */
typedef struct
{
  cairo_surface_t           *surface;      /* Поверхность маркера. */

  gdouble                    offset_x;     /* Координата x целевой точки на поверхности маркера. */
  gdouble                    offset_y;     /* Координата y целевой точки на поверхности маркера. */
  gboolean                   has_text;     /* Признак возможности размещения текста на маркере. */
  gdouble                    text_x;       /* Координата x положения текста на маркере. */
  gdouble                    text_y;       /* Координата y положения текста на маркере. */

  /* Область на поверхности маркера, клик по которой считается захватом хэндла. */
  gdouble                    handle_x0;    /* Координата x верхней левой точки хэндла. */
  gdouble                    handle_y0;    /* Координата y верхней левой точки хэндла. */
  gdouble                    handle_x1;    /* Координата x нижней правой точки хэндла. */
  gdouble                    handle_y1;    /* Координата y нижней правой точки хэндла. */
} HyScanGtkMapPinMarker;

typedef HyScanGtkMapPinMarker * (*HyScanGtkMapPinFactory) (HyScanGtkMapPin *layer,
                                                           guint            state);

struct _HyScanGtkMapPinPrivate
{
  HyScanGtkMap              *map;                      /* Виджет карты. */

  PangoLayout               *pango_layout;             /* Раскладка шрифта. */

  HyScanGtkMapPinMode        mode;                     /* Режим работы слоя. */

  GList                     *points;                   /* Список вершин ломаной, длину которой необходимо измерить. */
  HyScanGtkMapPinItem       *hover_point;              /* Вершина ломаной под курсором мыши. */
  HyScanGtkMapPinItem       *drag_item;                /* Вершина ломаной, которая перемещается в данный момент. */
  HyScanGtkMapPoint          drag_origin;              /* Изначальное положение точки, которая перемещается. */
  gboolean                   drag_temporary;           /* Перемещаемая точка в случае отмены должна быть удалена. */
  HyScanGtkMapPinItem       *found_handle;             /* Последний найденный хэндл. */

  /* Стиль оформления. */
  gdouble                    marker_size;                 /* Размер маркера (фактический размер макера зависит от
                                                        * его формы, см. pin_create_func). */
  GdkRGBA                    pin_color_prime;          /* Основной цвет маркера. */
  GdkRGBA                    pin_color_second;         /* Вспомогательный цвет маркера. */
  GdkRGBA                    pin_color_stroke;         /* Цвет обводки маркера. */
  gdouble                    pin_stroke_width;

  gboolean                   visible;                  /* Признак того, что слой виден. */

  HyScanGtkMapPinMarker     *pin;                      /* Внешний вид обычного маркера. */
  HyScanGtkMapPinMarker     *pin_hover;                /* Внешний вид маркера под указателем мыши. */
  HyScanGtkMapPinFactory     pin_create_func;          /* Фабрика маркеров. */
};

static void          hyscan_gtk_map_pin_object_constructed        (GObject                  *object);
static void          hyscan_gtk_map_pin_object_finalize           (GObject                  *object);
static void          hyscan_gtk_map_pin_interface_init            (HyScanGtkLayerInterface  *iface);
static void          hyscan_gtk_map_pin_draw                      (HyScanGtkMap             *map,
                                                                   cairo_t                  *cairo,
                                                                   HyScanGtkMapPin          *pin_layer);
static gboolean      hyscan_gtk_map_pin_motion_notify             (HyScanGtkMapPin          *pin_layer,
                                                                   GdkEventMotion           *event);
static void          hyscan_gtk_map_pin_motion_drag               (HyScanGtkMapPinPrivate   *priv,
                                                                   GdkEventMotion           *event,
                                                                   HyScanGtkMapPoint        *point);
static gboolean      hyscan_gtk_map_pin_key_press                 (HyScanGtkMapPin          *pin_layer,
                                                                   GdkEventKey              *event);
static gboolean      hyscan_gtk_map_pin_configure                 (HyScanGtkMapPin          *pin_layer,
                                                                   GdkEvent                 *event);
static void          hyscan_gtk_map_pin_draw_vertices             (HyScanGtkMapPin          *pin_layer,
                                                                   cairo_t                  *cairo);
static void          hyscan_gtk_map_pin_changed                   (HyScanGtkMapPin          *layer);
static HyScanGtkMapPinItem *
                     hyscan_gtk_map_pin_get_item_at               (HyScanGtkMapPin          *pin_layer,
                                                                   gdouble                   x,
                                                                   gdouble                   y);
static void          hyscan_gtk_map_pin_marker_update             (HyScanGtkMapPin          *layer);
static void          hyscan_gtk_map_pin_marker_free               (HyScanGtkMapPinMarker    *pin);
static void          hyscan_gtk_map_pin_added                     (HyScanGtkLayer           *gtk_layer,
                                                                   HyScanGtkLayerContainer  *container);
static void          hyscan_gtk_map_pin_set_visible               (HyScanGtkLayer           *layer,
                                                                   gboolean                  visible);
static gboolean      hyscan_gtk_map_pin_get_visible               (HyScanGtkLayer           *layer);

static HyScanGtkMapPinMarker *
                     hyscan_gtk_map_pin_create_marker_pin         (HyScanGtkMapPin          *layer,
                                                                   guint                     state);
static HyScanGtkMapPinMarker *
                     hyscan_gtk_map_pin_create_marker_circle      (HyScanGtkMapPin          *layer,
                                                                   guint                     state);
static inline HyScanGtkMapPinMarker *
                     hyscan_gtk_map_pin_get_marker                (HyScanGtkMapPinPrivate   *priv,
                                                                   HyScanGtkMapPinItem      *item);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapPin, hyscan_gtk_map_pin, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapPin)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_pin_interface_init))

static void
hyscan_gtk_map_pin_class_init (HyScanGtkMapPinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_pin_object_constructed;
  object_class->finalize = hyscan_gtk_map_pin_object_finalize;

  klass->changed = hyscan_gtk_map_pin_changed;
  klass->draw = hyscan_gtk_map_pin_draw_vertices;
}

static void
hyscan_gtk_map_pin_init (HyScanGtkMapPin *gtk_map_pin)
{
  gtk_map_pin->priv = hyscan_gtk_map_pin_get_instance_private (gtk_map_pin);
}

static void
hyscan_gtk_map_pin_stop_drag (HyScanGtkMapPin *layer)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;

  if (priv->mode != MODE_DRAG)
    return;

  priv->drag_item = NULL;
  priv->hover_point = NULL;
  priv->mode = MODE_NONE;
  hyscan_gtk_layer_container_set_handle_grabbed (HYSCAN_GTK_LAYER_CONTAINER (priv->map), NULL);
}

static void
hyscan_gtk_map_pin_changed (HyScanGtkMapPin *layer)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;
  GList *points;

  if (priv->hover_point == NULL && priv->drag_item == NULL)
    return;

  points = hyscan_gtk_map_pin_get_points (layer);

  /* Очищаем указатель на hover_point. */
  if (priv->hover_point != NULL && !g_list_find (points, priv->hover_point))
    priv->hover_point = NULL;

  /* Завершаем перетаскивание, если перетаскиваемая точка была удалена. */
  if (priv->drag_item != NULL && !g_list_find (points, priv->drag_item))
    hyscan_gtk_map_pin_stop_drag (layer);
}

/* Создаёт метку в виде круга. */
static HyScanGtkMapPinMarker *
hyscan_gtk_map_pin_create_marker_circle (HyScanGtkMapPin *layer,
                                         guint            state)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;
  gdouble radius;
  gdouble margin;
  guint width;

  HyScanGtkMapPinMarker *marker;
  cairo_t *cairo;

  /* Увеличиваем размер для выделенных меток. */
  radius = (state == PIN_HOVER) ? 2.0 * priv->marker_size : priv->marker_size;

  margin = 2.0 * priv->pin_stroke_width;
  width = (guint) ceil (2.0 * radius + 2.0 * margin);

  marker = g_slice_new (HyScanGtkMapPinMarker);
  marker->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, width);
  marker->has_text = FALSE;
  marker->offset_x = radius + margin;
  marker->offset_y = radius + margin;
  marker->handle_x0 = -radius;
  marker->handle_y0 = -radius;
  marker->handle_x1 = radius;
  marker->handle_y1 = radius;

  /* Основной внешний круг. */
  cairo = cairo_create (marker->surface);
  cairo_arc (cairo, marker->offset_x, marker->offset_y, radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_prime);
  cairo_fill_preserve (cairo);
  cairo_set_line_width (cairo, priv->pin_stroke_width);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_stroke);
  cairo_stroke (cairo);

  /* Внутренний круг белого цвета. */
  cairo_arc (cairo, marker->offset_x, marker->offset_y, .5 * radius, 0, 2.0 * G_PI);
  gdk_cairo_set_source_rgba (cairo, &priv->pin_color_second);
  cairo_fill (cairo);

  cairo_destroy (cairo);

  return marker;
}

/* Создаёт маркер в виде булавки. */
static HyScanGtkMapPinMarker *
hyscan_gtk_map_pin_create_marker_pin (HyScanGtkMapPin *layer,
                                      guint            state)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;

  HyScanGtkMapPinMarker *marker;
  cairo_t *cairo;

  gdouble default_size;
  gint label_height;
  gdouble radius;
  gdouble r_shadow;
  gdouble bezier_param;
  gdouble x, y;
  guint w, h;

  if (priv->pango_layout != NULL)
    {
      pango_layout_set_text (priv->pango_layout, "0123456789", -1);
      pango_layout_get_size (priv->pango_layout, NULL, &label_height);
      label_height /= PANGO_SCALE;
    }
  else
    {
      label_height = priv->marker_size;
    }

  default_size = MAX (priv->marker_size, label_height);

  /* Увеличиваем размер для выделенных меток. */
  radius = (state == PIN_HOVER) ? 1.2 * default_size : default_size;

  /* Определяем размеры пропорционально радиусу. */
  r_shadow = radius * 0.1;
  bezier_param = radius * 2.5;
  x = bezier_param * .33;
  y = bezier_param * .77;

  w = (guint) ceil (2.0 * x);
  h = (guint) ceil (y + r_shadow);

  marker = g_slice_new (HyScanGtkMapPinMarker);
  marker->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  marker->has_text = TRUE;
  marker->text_x = 0;
  marker->text_y = 0.6 * h - label_height / 2.0;
  marker->offset_x = x;
  marker->offset_y = h;
  marker->handle_x0 = -x;
  marker->handle_y0 = -y;
  marker->handle_x1 = x;
  marker->handle_y1 = 0;

  cairo = cairo_create (marker->surface);
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
  }

  cairo_destroy (cairo);

  return marker;
}

/* Освобождает память, занятую структурой pin. */
static void
hyscan_gtk_map_pin_marker_free (HyScanGtkMapPinMarker *pin)
{
  g_clear_pointer (&pin->surface, cairo_surface_destroy);
  g_slice_free (HyScanGtkMapPinMarker, pin);
}

/* Пересоздаёт маркеры. */
static void
hyscan_gtk_map_pin_marker_update (HyScanGtkMapPin *layer)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;

  g_clear_pointer (&priv->pin, hyscan_gtk_map_pin_marker_free);
  g_clear_pointer (&priv->pin_hover, hyscan_gtk_map_pin_marker_free);
  priv->pin = priv->pin_create_func (layer, PIN_NORMAL);
  priv->pin_hover = priv->pin_create_func (layer, PIN_HOVER);
}

static void
hyscan_gtk_map_pin_object_constructed (GObject *object)
{
  HyScanGtkMapPin *gtk_map_pin = HYSCAN_GTK_MAP_PIN (object);
  HyScanGtkMapPinPrivate *priv = gtk_map_pin->priv;
  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_pin_parent_class)->constructed (object);

  priv->mode = MODE_NONE;
  gtk_map_pin->priv->pin_stroke_width = 1.0;

  hyscan_gtk_map_pin_set_marker_shape (gtk_map_pin, HYSCAN_GTK_MAP_PIN_SHAPE_PIN);
  hyscan_gtk_map_pin_set_marker_size (gtk_map_pin, DEFAULT_SIZE);
  gdk_rgba_parse (&color, DEFAULT_COLOR_PRIME);
  hyscan_gtk_map_pin_set_color_prime (gtk_map_pin, color);
  gdk_rgba_parse (&color, DEFAULT_COLOR_STROKE);
  hyscan_gtk_map_pin_set_color_stroke (gtk_map_pin, color);
  gdk_rgba_parse (&color, DEFAULT_COLOR_SECOND);
  hyscan_gtk_map_pin_set_color_second (gtk_map_pin, color);
}

/* Находит хэндл в точке (x, y) логической СК. */
static gboolean
hyscan_gtk_map_pin_handle_find (HyScanGtkLayer       *layer,
                                gdouble               x,
                                gdouble               y,
                                HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPin *pin = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin->priv;
  HyScanGtkMapPinItem *item;

  item = hyscan_gtk_map_pin_get_item_at (pin, x, y);
  if (item != NULL)
    {
      priv->found_handle = item;
      handle->val_x = item->point.c2d.x;
      handle->val_y = item->point.c2d.y;
      handle->type_name = HANDLE_TYPE_NAME;

      return TRUE;
    }

  return FALSE;
}

/* Захватывает ранее найденный хэндл handle. */
static void
hyscan_gtk_map_pin_handle_click (HyScanGtkLayer       *layer,
                                 GdkEventButton       *event,
                                 HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPin *pin = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin->priv;

  g_return_if_fail (priv->found_handle != NULL);
  g_return_if_fail (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0);

  hyscan_gtk_map_pin_start_drag (pin, priv->found_handle, FALSE);
}

/* Возвращает %TRUE, если мы разрешаем отпустить хэндл. */
static gboolean
hyscan_gtk_map_pin_handle_release (HyScanGtkLayer *layer,
                                   GdkEventButton *event,
                                   gconstpointer   howner)
{
  HyScanGtkMapPin *pin = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin->priv;
  HyScanGtkMapPoint *drag_point;

  if (howner != layer)
    return FALSE;

  if (priv->mode != MODE_DRAG)
    return FALSE;

  priv->mode = MODE_NONE;
  drag_point = &priv->drag_item->point;
  hyscan_gtk_map_value_to_geo (priv->map, &drag_point->geo, drag_point->c2d);

  return TRUE;
}

static void
hyscan_gtk_map_pin_point_update (HyScanGtkMapPoint *point,
                                 HyScanGtkMap      *map)
{
  hyscan_gtk_map_geo_to_value (map, point->geo, &point->c2d);
}

/* Обновляет координаты точек при смене картографической проекции. */
static void
hyscan_gtk_map_pin_projection_notify (HyScanGtkMap    *map,
                                      GParamSpec      *pspec,
                                      HyScanGtkMapPin *layer)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;

  g_list_foreach (priv->points, (GFunc) hyscan_gtk_map_pin_point_update, map);
}

static void
hyscan_gtk_map_pin_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapPinPrivate *priv = HYSCAN_GTK_MAP_PIN (gtk_layer)->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gtk_layer);
  g_clear_object (&priv->map);
}

/* Регистрирует слой в карте. */
static void
hyscan_gtk_map_pin_added (HyScanGtkLayer          *gtk_layer,
                          HyScanGtkLayerContainer *container)
{
  HyScanGtkMapPinPrivate *priv = HYSCAN_GTK_MAP_PIN (gtk_layer)->priv;

  /* Проверяем, что контенейнер - это карта. */
  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);

  /* Сигналы карты. */
  g_signal_connect (container, "notify::projection", G_CALLBACK (hyscan_gtk_map_pin_projection_notify), gtk_layer);

  /* Подключаемся к сигналам перерисовки видимой области карты. */
  g_signal_connect_after (container, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_pin_draw), gtk_layer);

  /* Сигналы GtkWidget. */
  g_signal_connect_swapped (container, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_pin_configure), gtk_layer);
  g_signal_connect_swapped (container, "key-press-event",
                             G_CALLBACK (hyscan_gtk_map_pin_key_press), gtk_layer);
  g_signal_connect_swapped (container, "motion-notify-event",
                            G_CALLBACK (hyscan_gtk_map_pin_motion_notify), gtk_layer);
}

/* Устанавливает видимость слоя.
 * Реализация HyScanGtkLayerInterface.set_visible().*/
static void
hyscan_gtk_map_pin_set_visible (HyScanGtkLayer *layer,
                                gboolean        visible)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Возвращает %TRUE, если слой видимый.
 * Реализация HyScanGtkLayerInterface.get_visible(). */
static gboolean
hyscan_gtk_map_pin_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  return priv->visible;
}

/* Захватывает пользовательский ввод в контейнере.
 * Реализация HyScanGtkLayerInterface.grab_input(). */
static gboolean
hyscan_gtk_map_pin_grab_input (HyScanGtkLayer *layer)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  if (priv->map == NULL)
    return FALSE;

  hyscan_gtk_layer_container_set_input_owner (HYSCAN_GTK_LAYER_CONTAINER (priv->map), layer);

  return TRUE;
}

static gboolean
hyscan_gtk_map_pin_load_key_file (HyScanGtkLayer *gtk_layer,
                                  GKeyFile       *key_file,
                                  const gchar    *group)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (gtk_layer);
  GdkRGBA color;

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "stroke-color", DEFAULT_COLOR_STROKE);
  hyscan_gtk_map_pin_set_color_stroke (pin_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "prime-color", DEFAULT_COLOR_PRIME);
  hyscan_gtk_map_pin_set_color_prime (pin_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "second-color", DEFAULT_COLOR_SECOND);
  hyscan_gtk_map_pin_set_color_second (pin_layer, color);

  return TRUE;
}

static void
hyscan_gtk_map_pin_item_free (HyScanGtkMapPinItem *item)
{
  g_free (item->title);
  g_slice_free (HyScanGtkMapPinItem, item);
}

static void
hyscan_gtk_map_pin_object_finalize (GObject *object)
{
  HyScanGtkMapPin *gtk_map_pin = HYSCAN_GTK_MAP_PIN (object);
  HyScanGtkMapPinPrivate *priv = gtk_map_pin->priv;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_pin_item_free);
  g_clear_object (&priv->pango_layout);
  g_clear_pointer (&priv->pin, hyscan_gtk_map_pin_marker_free);
  g_clear_pointer (&priv->pin_hover, hyscan_gtk_map_pin_marker_free);

  G_OBJECT_CLASS (hyscan_gtk_map_pin_parent_class)->finalize (object);
}

/* Обработка сигнала "configure-event" виджета карты. */
static gboolean
hyscan_gtk_map_pin_configure (HyScanGtkMapPin *pin_layer,
                              GdkEvent        *event)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), NULL);
  hyscan_gtk_map_pin_marker_update (pin_layer);

  return FALSE;
}

/* Рисует отметки на карте.
 * Реализация HyScanGtkMapPinClass.draw().*/
static void
hyscan_gtk_map_pin_draw_vertices (HyScanGtkMapPin *pin_layer,
                                  cairo_t         *cairo)
{
  GList *point_l;
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPinMarker *marker;

  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPinItem *item = point_l->data;
      HyScanGtkMapPoint *point = &item->point;
      gdouble x, y;

      gtk_cifro_area_visible_value_to_point (carea, &x, &y, point->c2d.x, point->c2d.y);
      marker = hyscan_gtk_map_pin_get_marker (priv, item);

      cairo_set_source_surface (cairo, marker->surface, x - marker->offset_x, y - marker->offset_y);
      cairo_paint (cairo);

      /* Размещаем текст на маркере. */
      if (marker->has_text && item->title != NULL)
        {
          gint width, height;

          pango_layout_set_text (priv->pango_layout, item->title, -1);
          pango_layout_get_size (priv->pango_layout, &width, &height);
          width /= PANGO_SCALE;
          height /= PANGO_SCALE;
          cairo_move_to (cairo, x - marker->text_x - width / 2.0, y - marker->text_y - height);
          gdk_cairo_set_source_rgba (cairo, &priv->pin_color_second);
          pango_cairo_show_layout (cairo, priv->pango_layout);
        }
    }
}

/* Рисует элементы слоя по сигналу "visible-draw". */
static void
hyscan_gtk_map_pin_draw (HyScanGtkMap    *map,
                         cairo_t         *cairo,
                         HyScanGtkMapPin *pin_layer)
{
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (pin_layer)))
    return;

  HYSCAN_GTK_MAP_PIN_GET_CLASS (pin_layer)->draw (pin_layer, cairo);
}

/* Передвигает точку point под курсор. */
static void
hyscan_gtk_map_pin_motion_drag (HyScanGtkMapPinPrivate *priv,
                                GdkEventMotion         *event,
                                HyScanGtkMapPoint      *point)
{
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gtk_cifro_area_point_to_value (carea, event->x, event->y, &point->c2d.x, &point->c2d.y);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Показывает указанный хэнлд handle. */
static void
hyscan_gtk_map_pin_handle_show (HyScanGtkLayer       *layer,
                                HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;
  HyScanGtkMapPinItem *hover_point;

  hover_point = handle == NULL ? NULL : priv->found_handle;

  if (hover_point != priv->hover_point)
    {
      priv->hover_point = hover_point;
      gtk_widget_queue_draw (GTK_WIDGET (priv->map));
    }
}

/* Обрабатывает сигнал "motion-notify-event". */
static gboolean
hyscan_gtk_map_pin_motion_notify (HyScanGtkMapPin *pin_layer,
                                  GdkEventMotion  *event)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  if (priv->mode == MODE_DRAG)
    hyscan_gtk_map_pin_motion_drag (priv, event, &priv->drag_item->point);

  return FALSE;
}

/* Обработка Gdk-событий, которые дошли из контейнера. */
static gboolean
hyscan_gtk_map_pin_key_press (HyScanGtkMapPin *pin_layer,
                              GdkEventKey     *event)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  if (priv->mode != MODE_DRAG)
    return GDK_EVENT_PROPAGATE;

  if (event->keyval == GDK_KEY_Delete || (event->keyval == GDK_KEY_Escape && priv->drag_temporary))
    {
      /* Удаляем перетаскиваемую точку. */
      priv->points = g_list_remove (priv->points, priv->drag_item);
      hyscan_gtk_map_pin_item_free (priv->drag_item);
      hyscan_gtk_map_pin_stop_drag (pin_layer);

      /* Сообщаем, что список точек изменился. */
      HYSCAN_GTK_MAP_PIN_GET_CLASS (pin_layer)->changed (pin_layer);

      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  else if (event->keyval == GDK_KEY_Escape)
    {
      /* Возвращаем перетаскиваемую точку обратно. */
      priv->drag_item->point = priv->drag_origin;
      hyscan_gtk_map_pin_stop_drag (pin_layer);

      gtk_widget_queue_draw (GTK_WIDGET (priv->map));

      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

/* Создаёт новую точку в том месте, где пользователь кликнул мышью. */
static gboolean
hyscan_gtk_map_pin_handle_create (HyScanGtkLayer *layer,
                                  GdkEventButton *event)
{
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;

  HyScanGtkLayerContainer *container;
  HyScanGeoCartesian2D point;

  /* Обрабатываем только нажатия левой кнопки. */
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);

  /* Проверяем, что у нас есть право ввода. */
  if (hyscan_gtk_layer_container_get_input_owner (container) != layer)
    return GDK_EVENT_PROPAGATE;

  /* Если это условие не выполняется, то какая-то ошибка в логике программы. */
  g_return_val_if_fail (priv->mode == MODE_NONE, GDK_EVENT_PROPAGATE);

  gtk_cifro_area_point_to_value (GTK_CIFRO_AREA (priv->map), event->x, event->y, &point.x, &point.y);
  hyscan_gtk_map_pin_insert_before (pin_layer, &point, NULL);
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return GDK_EVENT_STOP;
}

/* Возвращает маркер, соответствующий точке point. */
static inline HyScanGtkMapPinMarker *
hyscan_gtk_map_pin_get_marker (HyScanGtkMapPinPrivate *priv,
                               HyScanGtkMapPinItem    *item)
{
  return (item == priv->hover_point) ? priv->pin_hover : priv->pin;
}

/* Находит точку на слое в окрестности указанных логических координат. */
static HyScanGtkMapPinItem *
hyscan_gtk_map_pin_get_item_at (HyScanGtkMapPin *pin_layer,
                                gdouble          x,
                                gdouble          y)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  HyScanGtkMapPinItem *hover_item;
  GList *point_l;
  HyScanGtkMapPinMarker *marker;
  gdouble scale_x, scale_y;

  /* Проверяем каждую точку, лежит ли она в поле действия маркера. */
  gtk_cifro_area_get_scale (carea, &scale_x, &scale_y);
  hover_item = NULL;
  for (point_l = priv->points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapPinItem *item = point_l->data;
      HyScanGtkMapPoint *point = &item->point;

      marker = hyscan_gtk_map_pin_get_marker (priv, item);

      if (point->c2d.x + marker->handle_x0 * scale_x < x && x < point->c2d.x + marker->handle_x1 * scale_x &&
          point->c2d.y - marker->handle_y0 * scale_y > y && y > point->c2d.y - marker->handle_y1 * scale_y)
        {
          hover_item = item;
          break;
        }
    }

  return hover_item;
}

/* Даёт автоматические названия маркерам. */
static void
hyscan_gtk_map_pin_item_auto_title (HyScanGtkMapPin     *pin_layer,
                                    HyScanGtkMapPinItem *item)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;
  GList *last_point;
  gint last_number = 0;

  if ((last_point = g_list_last (priv->points)) != NULL)
    {
      HyScanGtkMapPinItem *last_item = last_point->data;
      last_number = last_item->number;
    }

  item->number = last_number + 1;
  item->title = item->number < 10 ? g_strdup_printf ("%d", item->number) : NULL;
}

static void
hyscan_gtk_map_pin_queue_draw (HyScanGtkMapPin *layer)
{
  if (layer->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (layer)))
    gtk_widget_queue_draw (GTK_WIDGET (layer->priv->map));
}

/* Реализация интерфейса HyScanGtkLayerInterface. */
static void
hyscan_gtk_map_pin_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_pin_added;
  iface->removed = hyscan_gtk_map_pin_removed;
  iface->grab_input = hyscan_gtk_map_pin_grab_input;
  iface->set_visible = hyscan_gtk_map_pin_set_visible;
  iface->get_visible = hyscan_gtk_map_pin_get_visible;
  iface->load_key_file = hyscan_gtk_map_pin_load_key_file;
  iface->handle_find = hyscan_gtk_map_pin_handle_find;
  iface->handle_click = hyscan_gtk_map_pin_handle_click;
  iface->handle_release = hyscan_gtk_map_pin_handle_release;
  iface->handle_create = hyscan_gtk_map_pin_handle_create;
  iface->handle_show = hyscan_gtk_map_pin_handle_show;
}

/**
 * hyscan_gtk_map_pin_new:
 * @map: виджет карты #HyScanGtkMap
 *
 * Создаёт новый слой с булавками. На слое размещаются отметки различных
 * местоположений.
 *
 * Returns: новый объект #HyScanGtkMapPin.
 */
HyScanGtkLayer *
hyscan_gtk_map_pin_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_PIN, NULL);
}

/**
 * hyscan_gtk_map_pin_clear:
 * @pin_layer: указатель на #HyScanGtkMapPin
 *
 * Удаляет все точки на слое.
 */
void
hyscan_gtk_map_pin_clear (HyScanGtkMapPin *layer)
{
  HyScanGtkMapPinPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer));
  priv = layer->priv;

  if (priv->points == NULL)
    return;

  g_list_free_full (priv->points, (GDestroyNotify) hyscan_gtk_map_pin_item_free);
  priv->points = NULL;

  HYSCAN_GTK_MAP_PIN_GET_CLASS (layer)->changed (layer);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/**
 * hyscan_gtk_map_pin_get_points:
 * @pin_layer: указатель на #HyScanGtkMapPin
 *
 * Возвращает список точек #HyScanGtkMapPoint в слое.
 *
 * Returns: (transfer none) (element-type HyScanGtkMapPinItem): список точек
 */
GList *
hyscan_gtk_map_pin_get_points (HyScanGtkMapPin *layer)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer), NULL);

  return layer->priv->points;
}

/**
 * hyscan_gtk_map_pin_insert_before:
 * @pin_layer: указатель на #HyScanGtkMapPin
 * @point: точка для добавления #HyScanGeoCartesian2D
 * @sibling: позиция, перед которой необходимо поместить точку
 *
 * Добавляет @point в список точек слоя, помещая её перед указанной позицией @sibling.
 *
 * Returns: (transfer none): указатель на добавленную точку
 */
HyScanGtkMapPinItem *
hyscan_gtk_map_pin_insert_before  (HyScanGtkMapPin      *pin_layer,
                                   HyScanGeoCartesian2D *point,
                                   GList                *sibling)
{
  HyScanGtkMapPinPrivate *priv = pin_layer->priv;
  HyScanGtkMapPinItem *item;


  item = g_slice_new (HyScanGtkMapPinItem);
  item->point.c2d = *point;
  hyscan_gtk_map_value_to_geo (priv->map, &item->point.geo, item->point.c2d);
  hyscan_gtk_map_pin_item_auto_title (pin_layer, item);

  priv->points = g_list_insert_before (priv->points, sibling, item);

  /* Оповещаем, что список точек был изменён. */
  HYSCAN_GTK_MAP_PIN_GET_CLASS (pin_layer)->changed (pin_layer);

  return item;
}

/**
 * hyscan_gtk_map_pin_start_drag:
 * @layer: указатель на #HyScanGtkMapPin
 * @handle_point: указатель на точку #HyScanGtkMapPoint на слое @layer
 * @delete_on_cancel: удаляет точку при отмене перемещения
 *
 * Начинает перетаскивание точки @handle_point.
 *
 * Returns: %TRUE, если началось перетаскивание; иначе %FALSE
 */
gboolean
hyscan_gtk_map_pin_start_drag (HyScanGtkMapPin     *layer,
                               HyScanGtkMapPinItem *handle_point,
                               gboolean             delete_on_cancel)
{
  HyScanGtkMapPinPrivate *priv = HYSCAN_GTK_MAP_PIN (layer)->priv;
  gconstpointer howner;
  HyScanGtkLayerContainer *container;

  container = HYSCAN_GTK_LAYER_CONTAINER (priv->map);
  howner = hyscan_gtk_layer_container_get_handle_grabbed (container);

  /* Проверяем, что хэндл свободен и мы ещё не в режиме перемещения. */
  if (howner != NULL)
    {
      g_warning ("HyScanGtkMapPin: failed to start drag - handle is grabbed");
      return FALSE;
    }
  if (priv->mode == MODE_DRAG)
    {
      g_warning ("HyScanGtkMapPin: failed to start drag - already in drag mode");
      return FALSE;
    }

  priv->mode = MODE_DRAG;
  priv->drag_origin = handle_point->point;
  priv->drag_item = handle_point;
  priv->drag_temporary = delete_on_cancel;

  gtk_widget_grab_focus (GTK_WIDGET (priv->map));
  gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  hyscan_gtk_layer_container_set_handle_grabbed (container, layer);

  return TRUE;
}

/**
 * hyscan_gtk_map_pin_set_marker_size:
 * @layer: указатель на слой #HyScanGtkMapPin
 * @size: характерный размер метки в пикселах
 *
 * Устанавливает характерный размер метки в пикселах. Фактический размер метки
 * зависит от её формы, которая определяетя функцией hyscan_gtk_map_pin_set_marker_shape().
 */
void
hyscan_gtk_map_pin_set_marker_size (HyScanGtkMapPin *layer,
                                    guint            size)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer));

  layer->priv->marker_size = size;
  hyscan_gtk_map_pin_marker_update (layer);
  hyscan_gtk_map_pin_queue_draw (layer);
}

/**
 * hyscan_gtk_map_pin_set_color_prime:
 * @layer: указатель на слой #HyScanGtkMapPin
 * @color: основной цвет маркера
 *
 * Устанавливает основной цвет маркера
 */
void
hyscan_gtk_map_pin_set_color_prime (HyScanGtkMapPin *layer,
                                    GdkRGBA          color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer));

  layer->priv->pin_color_prime = color;
  hyscan_gtk_map_pin_marker_update (layer);
  hyscan_gtk_map_pin_queue_draw (layer);
}

/**
 * hyscan_gtk_map_pin_set_color_second:
 * @layer: указатель на слой #HyScanGtkMapPin
 * @color: вспомогательный цвет маркера
 *
 * Устанваливает вспомогательный цвет маркера
 */
void
hyscan_gtk_map_pin_set_color_second (HyScanGtkMapPin *layer,
                                     GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer));

  layer->priv->pin_color_second = color;
  hyscan_gtk_map_pin_marker_update (layer);
  hyscan_gtk_map_pin_queue_draw (layer);
}

/**
 * hyscan_gtk_map_pin_set_color_stroke:
 * @layer: указатель на слой #HyScanGtkMapPin
 * @color: цвет обводки
 *
 * Устанавливает цвет обводки элементов
 */
void
hyscan_gtk_map_pin_set_color_stroke (HyScanGtkMapPin *layer,
                                     GdkRGBA          color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_PIN (layer));

  layer->priv->pin_color_stroke = color;
  hyscan_gtk_map_pin_marker_update (layer);
  hyscan_gtk_map_pin_queue_draw (layer);
}


/**
 * hyscan_gtk_map_pin_set_marker_shape:
 * @layer: указатель на слой #HyScanGtkMapPin
 * @shape: устанавливает форму маркера
 *
 * Устанваливает форму маркера. Смотри #HyScanGtkMapPinMarkerShape для списка
 * доступных форм.
 */
void
hyscan_gtk_map_pin_set_marker_shape (HyScanGtkMapPin            *layer,
                                     HyScanGtkMapPinMarkerShape  shape)
{
  HyScanGtkMapPinPrivate *priv = layer->priv;

  switch (shape)
    {
    case HYSCAN_GTK_MAP_PIN_SHAPE_PIN:
      priv->pin_create_func = hyscan_gtk_map_pin_create_marker_pin;
      break;

    case HYSCAN_GTK_MAP_PIN_SHAPE_CIRCLE:
      priv->pin_create_func = hyscan_gtk_map_pin_create_marker_circle;
      break;

    default:
      g_warning ("HyScanGtkMapPin: unknown shape %u", shape);
      return;
    }

  hyscan_gtk_map_pin_marker_update (layer);
  hyscan_gtk_map_pin_queue_draw (layer);
}

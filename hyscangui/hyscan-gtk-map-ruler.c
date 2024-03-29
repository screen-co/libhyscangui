/* hyscan-gtk-map-ruler.c
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
 * SECTION: hyscan-gtk-map-ruler
 * @Short_description: Слой с линейкой для измерения расстояний.
 * @Title: HyScanGtkMapRuler
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Данный слой позволяет измерять длину пути по двум и более точкам. Расчет
 * расстояния основывается на предположении, что Земля имеет форму шара.
 *
 * Внешний вид определяется функциями или параметрами конфигурации:
 *
 * - hyscan_gtk_map_ruler_set_line_width()    "line-width"
 * - hyscan_gtk_map_ruler_set_label_color()   "color-label"
 * - hyscan_gtk_map_ruler_set_line_color()    "color-line"
 * - hyscan_gtk_map_ruler_set_bg_color()      "color-bg"
 *
 */

#include "hyscan-gtk-map-ruler.h"
#include "hyscan-gtk-map.h"
#include "hyscan-cairo.h"
#include "hyscan-gtk-layer-param.h"
#include <hyscan-cartesian.h>
#include <glib/gi18n-lib.h>
#include <math.h>
#include <cairo-sdline.h>
#include <hyscan-param-proxy.h>

#define SNAP_DISTANCE       10.0          /* Максимальное расстояние прилипания курсора мыши к звену ломаной. */
#define EARTH_RADIUS        6378137.0     /* Радиус Земли. */

#define IS_INSIDE(x, a, b) ((a) < (b) ? (a) < (x) && (x) < (b) : (b) < (x) && (x) < (a))
#define DEG2RAD(x) ((x) * G_PI / 180.0)
#define RAD2DEG(x) ((x) * 180.0 / G_PI)

#define HANDLE_TYPE_NAME   "map_ruler"                /* Название хэндла линейки. */

struct _HyScanGtkMapRulerPrivate
{
  HyScanGtkMap              *map;                      /* Виджет карты. */
  HyScanParamProxy          *params;                   /* Параметры оформления слоя. */

  PangoLayout               *pango_layout;             /* Раскладка шрифта. */

  GList                     *hover_section;            /* Указатель на отрезок ломаной под курсором мыши. */
  HyScanGeoCartesian2D       section_point;            /* Точка под курсором мыши в на отрезке hover_section. */

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
static GList *                  hyscan_gtk_map_ruler_get_segment_at           (HyScanGtkMapRuler        *ruler,
                                                                               gdouble                   x,
                                                                               gdouble                   y,
                                                                               HyScanGeoCartesian2D     *nearest_);
static gdouble                  hyscan_gtk_map_ruler_measure                  (HyScanGeoPoint            coord1,
                                                                               HyScanGeoPoint            coord2);
static gdouble                  hyscan_gtk_map_ruler_get_distance             (HyScanGtkMap             *map,
                                                                               GList                    *points);
static gboolean                 hyscan_gtk_map_ruler_configure                (HyScanGtkMap *map,
                                                                               GdkEvent                 *event,
                                                                               HyScanGtkMapRuler        *ruler);
static void                     hyscan_gtk_map_ruler_draw_line                (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_hover_section       (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_label               (HyScanGtkMapRuler        *ruler,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_draw_impl                (HyScanGtkMapPin          *layer,
                                                                               cairo_t                  *cairo);
static void                     hyscan_gtk_map_ruler_changed_impl             (HyScanGtkMapPin          *layer);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapRuler, hyscan_gtk_map_ruler, HYSCAN_TYPE_GTK_MAP_PIN,
                         G_ADD_PRIVATE (HyScanGtkMapRuler)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_ruler_interface_init))

static void
hyscan_gtk_map_ruler_class_init (HyScanGtkMapRulerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapPinClass *pin_class = HYSCAN_GTK_MAP_PIN_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_ruler_object_constructed;
  object_class->finalize = hyscan_gtk_map_ruler_object_finalize;

  pin_class->draw = hyscan_gtk_map_ruler_draw_impl;
  pin_class->changed = hyscan_gtk_map_ruler_changed_impl;
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
  HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;
  HyScanParam *parent_param;
  HyScanGtkLayerParam *param_controller;

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->constructed (object);

  param_controller = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (param_controller, "map-ruler");
  hyscan_gtk_layer_param_add_rgba (param_controller, "/bg-color", &priv->bg_color);
  hyscan_gtk_layer_param_add_rgba (param_controller, "/label-color", &priv->label_color);
  hyscan_gtk_layer_param_add_rgba (param_controller, "/line-color", &priv->line_color);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (param_controller), "/line-width", &priv->line_width);
  hyscan_gtk_layer_param_set_default (param_controller);

  priv->params = hyscan_param_proxy_new ();
  parent_param = hyscan_gtk_layer_parent_interface->get_param (HYSCAN_GTK_LAYER (object));

  hyscan_param_proxy_add (priv->params, "/", parent_param, "/");
  hyscan_param_proxy_add (priv->params, "/", HYSCAN_PARAM (param_controller), "/");
  hyscan_param_proxy_bind (priv->params);

  g_object_unref (parent_param);
  g_object_unref (param_controller);

  hyscan_gtk_map_pin_set_marker_size (pin_layer, SNAP_DISTANCE / 2.0);
  hyscan_gtk_map_pin_set_marker_shape (pin_layer, HYSCAN_GTK_MAP_PIN_SHAPE_CIRCLE);

  priv->radius = SNAP_DISTANCE;
  priv->label_padding = 5;
}

static void
hyscan_gtk_map_ruler_object_finalize (GObject *object)
{
  HyScanGtkMapRuler *gtk_map_ruler = HYSCAN_GTK_MAP_RULER (object);
  HyScanGtkMapRulerPrivate *priv = gtk_map_ruler->priv;

  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->params);

  G_OBJECT_CLASS (hyscan_gtk_map_ruler_parent_class)->finalize (object);
}

/* Захватывает хэндл.
 * Если указатель мыши над одним из отрезков, то помещает новую точку в это место и
 * начинает ее перетаскивать. */
static void
hyscan_gtk_map_ruler_handle_drag (HyScanGtkLayer       *layer,
                                  GdkEventButton       *event,
                                  HyScanGtkLayerHandle *handle)
{
  if (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0)
    {
      HyScanGtkMapPin *pin_layer = HYSCAN_GTK_MAP_PIN (layer);
      GList *found_section = handle->user_data;
      HyScanGtkMapPinItem *new_item;
      HyScanGeoCartesian2D coordinate;

      coordinate.x = handle->val_x;
      coordinate.y = handle->val_y;
      new_item = hyscan_gtk_map_pin_insert_before (pin_layer, &coordinate, found_section);

      hyscan_gtk_map_pin_start_drag (pin_layer, new_item, TRUE);
    }

  hyscan_gtk_layer_parent_interface->handle_drag (layer, event, handle);
}

/* Показывает/прячет хэндл.
 * Хэндл может быть либо на каком-то отрезке ломаной (создает новую вершину),
 * либо на вершине (перемещает эту вершину). */
static void
hyscan_gtk_map_ruler_handle_show (HyScanGtkLayer       *layer,
                                  HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (layer);
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GList *hover_section;

  /* Надо спрятать хэндл. */
  if (handle == NULL)
    {
      hover_section = NULL;
      hyscan_gtk_layer_parent_interface->handle_show (layer, NULL);
    }

  /* Надо показать хэндл из этого класса. */
  else if (g_strcmp0 (handle->type_name, HANDLE_TYPE_NAME) == 0)
    {
      hover_section = handle->user_data;
      priv->section_point.x = handle->val_x;
      priv->section_point.y = handle->val_y;
      hyscan_gtk_layer_parent_interface->handle_show (layer, NULL);
    }

  /* Надо показать хэндл из родительского класса. */
  else
    {
      hover_section = NULL;
      hyscan_gtk_layer_parent_interface->handle_show (layer, handle);
    }

  if (hover_section != NULL || hover_section != priv->hover_section)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  priv->hover_section = hover_section;
}

static void
hyscan_gtk_map_ruler_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapRulerPrivate *priv = HYSCAN_GTK_MAP_RULER (gtk_layer)->priv;

  g_return_if_fail (priv->map != NULL);

  g_signal_handlers_disconnect_by_data (priv->map, gtk_layer);
  g_clear_object (&priv->map);

  hyscan_gtk_layer_parent_interface->removed (gtk_layer);
}

/* Находит хэндл в точке с логическими координатами (x, y). */
static gboolean
hyscan_gtk_map_ruler_handle_find (HyScanGtkLayer       *layer,
                                  gdouble               x,
                                  gdouble               y,
                                  HyScanGtkLayerHandle *handle)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (layer);
  GList *found_section;
  HyScanGeoCartesian2D nearest;

  if (hyscan_gtk_layer_parent_interface->handle_find (layer, x, y, handle))
    return TRUE;

  found_section = hyscan_gtk_map_ruler_get_segment_at (ruler, x, y, &nearest);
  if (found_section != NULL)
    {
      handle->val_x = nearest.x;
      handle->val_y = nearest.y;
      handle->user_data = found_section;
      handle->type_name = HANDLE_TYPE_NAME;

      return TRUE;
    }

  return FALSE;
}

static void
hyscan_gtk_map_ruler_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapRulerPrivate *priv = HYSCAN_GTK_MAP_RULER (gtk_layer)->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  /* Выполняем реализацию в родительском классе. */
  hyscan_gtk_layer_parent_interface->added (gtk_layer, container);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));

  g_signal_connect (priv->map, "configure-event",
                    G_CALLBACK (hyscan_gtk_map_ruler_configure), gtk_layer);
}

static HyScanParam *
hyscan_gtk_map_ruler_get_param (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (gtk_layer);

  return g_object_ref (HYSCAN_PARAM (ruler->priv->params));
}

static void
hyscan_gtk_map_ruler_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->added = hyscan_gtk_map_ruler_added;
  iface->removed = hyscan_gtk_map_ruler_removed;
  iface->handle_find = hyscan_gtk_map_ruler_handle_find;
  iface->handle_show = hyscan_gtk_map_ruler_handle_show;
  iface->handle_drag = hyscan_gtk_map_ruler_handle_drag;
  iface->get_param = hyscan_gtk_map_ruler_get_param;
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
hyscan_gtk_map_ruler_measure (HyScanGeoPoint coord1,
                              HyScanGeoPoint coord2)
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

  HyScanGeoPoint coord0, coord1;

  gdouble distance;

  /* Меньше двух точек, расстояние посчитать не получится. */
  if (points == NULL || points->next == NULL)
    return -1.0;

  point_l = points;
  point = point_l->data;
  hyscan_gtk_map_value_to_geo (map, &coord1, point->c2d);

  distance = 0.0;
  for (point_l = point_l->next; point_l != NULL; point_l = point_l->next)
    {
      point = point_l->data;

      coord0 = coord1;
      hyscan_gtk_map_value_to_geo (map, &coord1, point->c2d);
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

  gdouble x;
  gdouble y;

  carea = GTK_CIFRO_AREA (priv->map);

  points = hyscan_gtk_map_pin_get_points (HYSCAN_GTK_MAP_PIN (ruler));
  distance = hyscan_gtk_map_ruler_get_distance (priv->map, points);

  if (distance < 0.0)
    return;

  if (distance < 1.0)
    g_snprintf (label, sizeof (label), "%.1f %s", distance * 100.0, _("cm"));
  else if (distance < 100.0)
    g_snprintf (label, sizeof (label), "%.2f %s", distance, _("m"));
  else if (distance < 1000.0)
    g_snprintf (label, sizeof (label), "%.1f %s", distance, _("m"));
  else
    g_snprintf (label, sizeof (label), "%.2f %s", distance / 1000.0, _("km"));

  pango_layout_set_text (priv->pango_layout, label, -1);
  pango_layout_get_size (priv->pango_layout, &width, &height);
  height /= PANGO_SCALE;
  width /= PANGO_SCALE;

  last_point = g_list_last (points)->data;
  gtk_cifro_area_visible_value_to_point (carea, &x, &y, last_point->c2d.x, last_point->c2d.y);

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
  cairo_arc (cairo, x, y, priv->radius, 0, 2.0 * G_PI);
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
  gint i;
  HyScanGtkMapPoint *point = NULL;

  gdouble x[2];
  gdouble y[2];

  carea = GTK_CIFRO_AREA (map);

  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_set_line_width (cairo, priv->line_width);

  cairo_new_path (cairo);
  points = hyscan_gtk_map_pin_get_points (HYSCAN_GTK_MAP_PIN (ruler));
  for (point_l = points, i = 0; point_l != NULL; point_l = point_l->next, ++i)
    {
      gint cur_idx;
      point = point_l->data;

      cur_idx = i % 2;
      gtk_cifro_area_visible_value_to_point (carea, &x[cur_idx], &y[cur_idx], point->c2d.x, point->c2d.y);

      if (i == 0)
        continue;

      hyscan_cairo_line_to (cairo, x[0], y[0], x[1], y[1]);
    }
  cairo_stroke (cairo);
}

/* Обработчик изменения списка точек слоя. */
static void
hyscan_gtk_map_ruler_changed_impl (HyScanGtkMapPin *layer)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (layer);
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  GList *points;

  /* Вызываем обработчик родительского класса. */
  HYSCAN_GTK_MAP_PIN_CLASS (hyscan_gtk_map_ruler_parent_class)->changed (layer);

  if (priv->hover_section == NULL)
    return;

  /* Если отрезок под курсором был удалён, то очищаем указатель на него. */
  points = hyscan_gtk_map_pin_get_points (layer);
  if (g_list_position (points, priv->hover_section) == -1)
    priv->hover_section = NULL;
}

/* Рисует элементы линейки по сигналу "visible-draw". */
static void
hyscan_gtk_map_ruler_draw_impl (HyScanGtkMapPin *layer,
                                cairo_t          *cairo)
{
  HyScanGtkMapRuler *ruler = HYSCAN_GTK_MAP_RULER (layer);

  /* Рисуем соединения между метками и прочее. */
  hyscan_gtk_map_ruler_draw_line (ruler, cairo);
  hyscan_gtk_map_ruler_draw_hover_section (ruler, cairo);

  /* Родительский класс рисует сами метки. */
  HYSCAN_GTK_MAP_PIN_CLASS (hyscan_gtk_map_ruler_parent_class)->draw (layer, cairo);

  hyscan_gtk_map_ruler_draw_label (ruler, cairo);
}

/* Находит звено ломаной в SNAP_DISTANCE-окрестности курсора мыши. */
static GList *
hyscan_gtk_map_ruler_get_segment_at (HyScanGtkMapRuler    *ruler,
                                     gdouble               x,
                                     gdouble               y,
                                     HyScanGeoCartesian2D *nearest_)
{
  HyScanGtkMapRulerPrivate *priv = ruler->priv;
  HyScanGtkMap *map = priv->map;
  GtkCifroArea *carea;
  GList *points;
  GList *point_l;

  HyScanGeoCartesian2D cursor;
  gdouble scale_x;
  gdouble snap_distance;

  carea = GTK_CIFRO_AREA (map);
  gtk_cifro_area_get_scale (carea, &scale_x, NULL);
  snap_distance = SNAP_DISTANCE * scale_x;
  cursor.x = x;
  cursor.y = y;
  points = hyscan_gtk_map_pin_get_points (HYSCAN_GTK_MAP_PIN (ruler));
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGeoCartesian2D *point = &((HyScanGtkMapPoint *) point_l->data)->c2d;
      HyScanGeoCartesian2D *prev_point;

      gdouble distance;
      HyScanGeoCartesian2D nearest;

      if (point_l->prev == NULL)
        continue;

      prev_point = &((HyScanGtkMapPoint *) point_l->prev->data)->c2d;

      /* Надо, чтобы курсор мыши был между точками хотя бы по одной из осей. */
      if (!IS_INSIDE (cursor.x, point->x, prev_point->x) &&
          !IS_INSIDE (cursor.y, point->y, prev_point->y))
        {
          continue;
        }

      /* Определяем расстояние от курсоры мыши до прямой между точками. */
      distance = hyscan_cartesian_distance_to_line (prev_point, point, &cursor, &nearest);
      if (distance > snap_distance)
        continue;

      /* Проверяем, что ближайшая точка на прямой попала внутрь отрезка между точками. */
      if (!IS_INSIDE (nearest.x, point->x, prev_point->x) ||
          !IS_INSIDE (nearest.y, point->y, prev_point->y))
        {
          continue;
        }

      *nearest_ = nearest;

      return point_l;
    }

  return NULL;
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
 * Returns: новый объект #HyScanGtkMapRuler
 */
HyScanGtkLayer *
hyscan_gtk_map_ruler_new ()
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_RULER, NULL);
}

/**
 * hyscan_gtk_map_ruler_set_line_width:
 * @ruler: указатель на слой #HyScanGtkMapRuler
 * @width: толщина линии линейки
 *
 * Устанавливает толщину линий линейки
 */
void
hyscan_gtk_map_ruler_set_line_width (HyScanGtkMapRuler *ruler,
                                     gdouble            width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));
  g_return_if_fail (width > 0);

  ruler->priv->line_width = width;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

/**
 * hyscan_gtk_map_ruler_set_line_color:
 * @ruler: указатель на слой #HyScanGtkMapRuler
 * @color: цвет линии линейки
 *
 * Устанавливает цвет линий линейки
 */
void
hyscan_gtk_map_ruler_set_line_color (HyScanGtkMapRuler *ruler,
                                     GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->line_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

/**
 * hyscan_gtk_map_ruler_set_label_color:
 * @ruler: указатель на слой #HyScanGtkMapRuler
 * @color: цвет текста подписи
 *
 * Устанавливает цвет текста подписи
 */
void
hyscan_gtk_map_ruler_set_label_color (HyScanGtkMapRuler *ruler,
                                      GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->label_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

/**
 * hyscan_gtk_map_ruler_set_bg_color:
 * @ruler: указатель на слой #HyScanGtkMapRuler
 * @color: цвет подложки подписи
 *
 * Устанавливает цвет подложки подписи
 */
void
hyscan_gtk_map_ruler_set_bg_color (HyScanGtkMapRuler *ruler,
                                   GdkRGBA            color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_RULER (ruler));

  ruler->priv->bg_color = color;
  hyscan_gtk_map_grid_queue_draw (ruler);
}

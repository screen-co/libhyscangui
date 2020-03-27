/* hyscan-gtk-map-grid.c
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
 * SECTION: hyscan-gtk-map-grid
 * @Short_description: Слой карты #HyScanGtkMap
 * @Title: HyScanGtkMapGrid
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap
 *
 * Слой с изображением координатной сетки.
 *
 * Стиль оформления сетки можно задать с помощью функций класса или свойств из через
 * #HyScanParam (см. hyscan_gtk_layer_get_param()):
 *
 * - hyscan_gtk_map_grid_set_bg_color()      "/bg-color"
 * - hyscan_gtk_map_grid_set_label_color()   "/label-color"
 * - hyscan_gtk_map_grid_set_line_color()    "/line-color"
 * - hyscan_gtk_map_grid_set_line_width()    "/line-width"
 * - hyscan_gtk_map_grid_set_step_width()
 *
 */

#include "hyscan-gtk-map-grid.h"
#include "hyscan-gtk-layer-param.h"
#include <glib/gi18n-lib.h>
#include <math.h>
#include <hyscan-cartesian.h>

#define MAX_LAT             90.0      /* Максимальное по модулю значение широты. */
#define MAX_LON             180.0     /* Максимальное по модулю значение долготы. */
#define LINE_POINTS_NUM     5         /* Количество точек, по которым строится линия сетки. */

#define GRID_STEP           200       /* Среднее расстояние между соседними линиями сетки. */

enum
{
  PROP_O,
  PROP_UNITS,
};

struct _HyScanGtkMapGridPrivate
{
  HyScanGtkMap                     *map;                /* Виджет карты, на котором показывается сетка. */
  HyScanUnits                      *units;              /* Форматирование единиц измерения. */

  PangoLayout                      *pango_layout;       /* Раскладка шрифта. */

  HyScanGtkLayerParam              *param;              /* Параметры оформления слоя. */
  GdkRGBA                           line_color;         /* Цвет линий координатной сетки. */
  gdouble                           line_width;         /* Ширина линии координатной сетки. */
  GdkRGBA                           label_color;        /* Цвет подписей. */
  GdkRGBA                           bg_color;           /* Фоновый цвет подписей. */
  GdkRGBA                           stroke_color;       /* Цвет обводки подписей. */

  guint                             step_width;         /* Шаг сетки в пикселях. */

  gboolean                          visible;            /* Признак того, что сетка отображается. */

};

static void     hyscan_gtk_map_grid_set_property            (GObject                 *object,
                                                             guint                    prop_id,
                                                             const GValue            *value,
                                                             GParamSpec              *pspec);
static void     hyscan_gtk_map_grid_object_constructed      (GObject                 *object);
static void     hyscan_gtk_map_grid_object_finalize         (GObject                 *object);
static void     hyscan_gtk_map_grid_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_map_grid_queue_draw              (HyScanGtkMapGrid        *grid);
static void     hyscan_gtk_map_grid_draw                    (HyScanGtkMap            *map,
                                                             cairo_t                 *cairo,
                                                             HyScanGtkMapGrid        *grid);
static gboolean hyscan_gtk_map_grid_configure               (HyScanGtkMapGrid        *grid,
                                                             GdkEvent                *screen);
static void     hyscan_gtk_map_grid_draw_north              (HyScanGtkMapGrid        *grid,
                                                             cairo_t                 *cairo,
                                                             gint                     x,
                                                             gint                     y);
static void     hyscan_gtk_map_grid_label                   (HyScanGtkMapGrid        *grid,
                                                             cairo_t                 *cairo,
                                                             const gchar             *label,
                                                             gboolean                 rotate,
                                                             HyScanGeoCartesian2D    *point);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapGrid, hyscan_gtk_map_grid, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapGrid)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_grid_interface_init))

static void
hyscan_gtk_map_grid_class_init (HyScanGtkMapGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_grid_set_property;
  object_class->constructed = hyscan_gtk_map_grid_object_constructed;
  object_class->finalize = hyscan_gtk_map_grid_object_finalize;

  g_object_class_install_property (object_class, PROP_UNITS,
                                   g_param_spec_object ("units", "Units", "Measure units",
                                                        HYSCAN_TYPE_UNITS,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_grid_init (HyScanGtkMapGrid *gtk_map_grid)
{
  gtk_map_grid->priv = hyscan_gtk_map_grid_get_instance_private (gtk_map_grid);
}

static void
hyscan_gtk_map_grid_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanGtkMapGrid *grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = grid->priv;

  switch (prop_id)
    {
    case PROP_UNITS:
      priv->units = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_grid_object_constructed (GObject *object)
{
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->constructed (object);

  priv->param = hyscan_gtk_layer_param_new ();
  hyscan_gtk_layer_param_set_stock_schema (priv->param, "map-grid");
  hyscan_gtk_layer_param_add_rgba (priv->param, "/bg-color", &priv->bg_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/label-color", &priv->label_color);
  hyscan_gtk_layer_param_add_rgba (priv->param, "/line-color", &priv->line_color);
  hyscan_param_controller_add_double (HYSCAN_PARAM_CONTROLLER (priv->param), "/line-width", &priv->line_width);
  hyscan_gtk_layer_param_set_default (priv->param);

  hyscan_gtk_map_grid_set_step_width (gtk_map_grid, GRID_STEP);

  g_signal_connect_swapped (priv->units, "notify::geo", G_CALLBACK (hyscan_gtk_map_grid_queue_draw), gtk_map_grid);
}

static void
hyscan_gtk_map_grid_object_finalize (GObject *object)
{
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;

  g_clear_object (&priv->pango_layout);
  g_clear_object (&priv->units);
  g_object_unref (priv->map);
  g_object_unref (priv->param);

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_grid_set_visible (HyScanGtkLayer *layer,
                                 gboolean        visible)
{
  HyScanGtkMapGrid *grid = HYSCAN_GTK_MAP_GRID (layer);
  HyScanGtkMapGridPrivate *priv = grid->priv;
  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

static gboolean
hyscan_gtk_map_grid_get_visible (HyScanGtkLayer *layer)
{
  return HYSCAN_GTK_MAP_GRID (layer)->priv->visible;
}

static void
hyscan_gtk_map_grid_added (HyScanGtkLayer          *gtk_layer,
                           HyScanGtkLayerContainer *container)
{
  HyScanGtkMapGridPrivate *priv = HYSCAN_GTK_MAP_GRID (gtk_layer)->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));

  g_signal_connect_after (priv->map, "area-draw",
                          G_CALLBACK (hyscan_gtk_map_grid_draw), gtk_layer);
  g_signal_connect_swapped (priv->map, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_grid_configure), gtk_layer);
}

static HyScanParam *
hyscan_gtk_map_grid_get_param (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapGrid *grid_layer = HYSCAN_GTK_MAP_GRID (gtk_layer);
  HyScanGtkMapGridPrivate *priv = grid_layer->priv;

  return g_object_ref (priv->param);
}

static void
hyscan_gtk_map_grid_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->set_visible = hyscan_gtk_map_grid_set_visible;
  iface->get_visible = hyscan_gtk_map_grid_get_visible;
  iface->added = hyscan_gtk_map_grid_added;
  iface->get_param = hyscan_gtk_map_grid_get_param;
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_grid_configure (HyScanGtkMapGrid *grid,
                               GdkEvent         *screen)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  gint width;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), "01234 km");

  pango_layout_get_size (priv->pango_layout, &width, NULL);
  if (width < 0)
    width = 0;

  width /= PANGO_SCALE;

  return GDK_EVENT_PROPAGATE;
}

/* Рисует подпись линии широты/долготы в указанной точке. */
static void
hyscan_gtk_map_grid_label (HyScanGtkMapGrid     *grid,
                           cairo_t              *cairo,
                           const gchar          *label,
                           gboolean              rotate,
                           HyScanGeoCartesian2D *point)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  gint text_width, text_height;

  /* Рисуем подпись. */
  cairo_save (cairo);

  pango_layout_set_text (priv->pango_layout, label, -1);

  pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
  text_height /= PANGO_SCALE;
  text_width /= PANGO_SCALE;

  cairo_translate (cairo, point->x, point->y);
  if (rotate)
    cairo_rotate (cairo, -G_PI_2);

  cairo_translate (cairo, -text_width / 2.0, 0);

  cairo_rectangle (cairo, 0, 0, text_width, text_height);
  gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
  cairo_fill_preserve (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->stroke_color);
  cairo_set_line_width (cairo, 0.5);
  cairo_stroke (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->label_color);
  pango_cairo_show_layout (cairo, priv->pango_layout);
  cairo_restore (cairo);
}

/* Рисуем линию по точкам points. */
static void
hyscan_gtk_map_grid_draw_line (HyScanGtkMapGrid  *grid,
                               cairo_t           *cairo,
                               HyScanGeoGeodetic *points,
                               gsize              points_len,
                               gboolean           prefer_y,
                               const gchar       *label)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  guint i;

  guint width, height;
  HyScanGeoCartesian2D pt1, pt0;
  HyScanGeoCartesian2D left_top = {0, 0},
                       right_top = {0, 0},
                       left_bottom = {0, 0},
                       cross = {0, 0};
  gboolean cross_x = FALSE,  /* Признак пересечения с верхней границей (left_top - right_top). */
           cross_y = FALSE;  /* Признак пересечения с левой границей (left_top - left_bottom). */

  /* Определяем координаты углов. */
  gtk_cifro_area_get_size (GTK_CIFRO_AREA (priv->map), &width, &height);
  right_top.x = width;
  left_bottom.y = height;

  /* Рисуем линию покусочно через указанные точки. */
  cairo_new_path (cairo);
  for (i = 0; i < points_len; ++i, pt1 = pt0)
    {
      HyScanGeoCartesian2D value;

      hyscan_gtk_map_geo_to_value (priv->map, points[i], &value);
      gtk_cifro_area_value_to_point (carea, &pt0.x, &pt0.y, value.x, value.y);
      cairo_line_to (cairo, pt0.x, pt0.y);

      /* Ищем пересечение линии с границей видимой области.
       * Если пересечение с предпочтительной линией уже найдено, то пропускаем этот шаг. */
      if (i == 0 || (cross_y && prefer_y) || (cross_x && !prefer_y))
        continue;

      cross_y = hyscan_cartesian_intersection (&pt1, &pt0, &left_top, &left_bottom, &cross) &&
                0 < cross.y && cross.y < height;
      if (cross_y && prefer_y)
        continue;

      cross_x = hyscan_cartesian_intersection (&pt1, &pt0, &left_top, &right_top, &cross) &&
                0 < cross.x && cross.x < width;
    }

  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);

  if (!cross_x && !cross_y)
    return;

  hyscan_gtk_map_grid_label (grid, cairo, label, prefer_y ? cross_y : !cross_x, &cross);
}

/* Выравнивает шаги координатной сетки при больших значениях шага. */
static void
hyscan_gtk_map_grid_adjust_step_big (gdouble  step_length,
                                     gdouble *step,
                                     gint    *power)
{
  gdouble steps[]       = { 60.0, 45.0, 30.0, 15.0};
  gdouble step_limits[] = { 52.5, 37.5, 22.5};

  guint i = 0;

  /* Выбираем один из шагов согласно step_limits. */
  while (i < G_N_ELEMENTS (step_limits) && step_length < step_limits[i])
    i++;

  *step = steps[i];
  *power = 1;
}

/* Выравнивает шаги координатной сетки при малых значениях шага. */
static void
hyscan_gtk_map_grid_adjust_step_small (gdouble  step_length,
                                       gdouble *step,
                                       gint    *power)
{
  gint power_ret;
  gdouble step_ret;

  gdouble axis_1_width_delta;
  gdouble axis_2_width_delta;
  gdouble axis_5_width_delta;

  gdouble axis_1_score;
  gdouble axis_2_score;
  gdouble axis_5_score;

  power_ret = 0;
  while ((step_length > 10) || (step_length < 1))
    {
      if (step_length > 10)
        {
          step_length /= 10.0;
          power_ret = power_ret + 1;
        }
      if (step_length < 1)
        {
          step_length *= 10.0;
          power_ret = power_ret - 1;
        }
    }

  if (step_length > 7.5)
    {
      step_length /= 10.0;
      power_ret = power_ret + 1;
    }

  /* Выбираем с каким шагом рисовать сетку: 1, 2 или 5 (плюс их степени). */

  /* Расчитываем отношение размера ячейки для трёх возможных вариантов сетки к
   * предпочтительному размеру ячейки определённым пользователем. */
  axis_1_width_delta = 1.0 / step_length;
  axis_2_width_delta = 2.0 / step_length;
  axis_5_width_delta = 5.0 / step_length;

  /* Расчитываем "вес" каждого варианта. */
  axis_1_score = (axis_1_width_delta >= 1.0) ? 1.0 / axis_1_width_delta : axis_1_width_delta;
  axis_2_score = (axis_2_width_delta >= 1.0) ? 1.0 / axis_2_width_delta : axis_2_width_delta;
  axis_5_score = (axis_5_width_delta >= 1.0) ? 1.0 / axis_5_width_delta : axis_5_width_delta;

  if ((axis_1_score > axis_2_score) && (axis_1_score > axis_5_score))
    step_ret = 1.0 * pow (10, power_ret);
  else if (axis_2_score > axis_5_score)
    step_ret = 2.0 * pow (10, power_ret);
  else
    step_ret = 5.0 * pow (10, power_ret);

  *step = step_ret;
  *power = power_ret;
}

/* Подстройка шага и начала координатной сетки.
 * На основе gtk_cifro_area_get_axis_step(). */
static void
hyscan_gtk_map_grid_adjust_step (gdouble  step_length,
                                 gdouble *from,
                                 gdouble *step,
                                 gint    *power)
{
  gint power_ret;
  gdouble from_ret;
  gdouble step_ret;

  /* Для больших шагов делаем естественную для карт разбивку, кратную 15. */
  if (step_length > 12.5)
    hyscan_gtk_map_grid_adjust_step_big (step_length, &step_ret, &power_ret);
  else
    hyscan_gtk_map_grid_adjust_step_small (step_length, &step_ret, &power_ret);

  from_ret = step_ret * floor (*from / step_ret);
  if (from_ret < *from)
    from_ret += step_ret;

  *from = from_ret;
  *step = step_ret;
  *power = power_ret;
}

/* Рисует стрелку с направлением на север. */
static void
hyscan_gtk_map_grid_draw_north (HyScanGtkMapGrid *grid,
                                cairo_t          *cairo,
                                gint              x,
                                gint              y)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  gint width, height;

  cairo_save (cairo);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, gtk_cifro_area_get_angle (GTK_CIFRO_AREA (priv->map)));
  cairo_new_path (cairo);
  cairo_move_to (cairo,  0,  -20);
  cairo_line_to (cairo,  10,  12);
  cairo_line_to (cairo,  0,   8);
  cairo_line_to (cairo, -10,  12);
  cairo_close_path (cairo);
  gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
  cairo_fill (cairo);
  cairo_restore (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->label_color);
  pango_layout_set_text (priv->pango_layout, "N", 1);
  pango_layout_get_pixel_size (priv->pango_layout, &width, &height);
  cairo_move_to (cairo, x - width / 2.0, y - height / 2.0);
  pango_cairo_show_layout (cairo, priv->pango_layout);
}

/* Рисует координатную сетку по сигналу "area-draw". */
static void
hyscan_gtk_map_grid_draw (HyScanGtkMap     *map,
                          cairo_t          *cairo,
                          HyScanGtkMapGrid *grid)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gdouble from_x, from_y, to_x, to_y;
  gdouble min_x, min_y, max_x, max_y;

  gint steps;

  guint width;
  guint height;

  gint value_power;

  HyScanGeoGeodetic coords1_geo, coords2_geo;
  HyScanGeoCartesian2D coords1, coords2;

  HyScanGeoGeodetic from_geo, to_geo;
  HyScanGeoCartesian2D from, to;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    return;

  /* Определяем размеры видимой области. */
  gtk_cifro_area_get_size (carea, &width, &height);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_limits (carea, &min_x, &max_x, &min_y, &max_y);
  from.x = CLAMP (from_x, min_x, max_x);
  from.y = CLAMP (from_y, min_y, max_y);
  to.x = CLAMP (to_x, min_x, max_x);
  to.y = CLAMP (to_y, min_y, max_y);

  hyscan_gtk_map_value_to_geo (priv->map, &from_geo, from);
  hyscan_gtk_map_value_to_geo (priv->map, &to_geo, to);

  /* Рисуем параллели (latitude = const). */
  {
    gdouble from_lat, to_lat;
    gdouble lat_step;
    gdouble lat;

    steps = MAX(1, height / priv->step_width);

    /* Определяем границу видимой области по широте. */
    coords1.y = from.y;
    coords2.y = to.y;
    coords1.x = coords2.x = (from.x + to.x) / 2.0;
    hyscan_gtk_map_value_to_geo (priv->map, &coords1_geo, coords1);
    hyscan_gtk_map_value_to_geo (priv->map, &coords2_geo, coords2);
    from_lat = CLAMP (coords1_geo.lat, -MAX_LAT, MAX_LAT);
    to_lat = CLAMP (coords2_geo.lat, -MAX_LAT, MAX_LAT);

    lat_step = (to_lat - from_lat) / steps;
    hyscan_gtk_map_grid_adjust_step (lat_step, &from_lat, &lat_step, &value_power);

    lat = from_lat;
    while (lat <= to_lat)
      {
        gchar *label;
        gsize i;
        HyScanGeoGeodetic points[LINE_POINTS_NUM];

        for (i = 0; i < LINE_POINTS_NUM; i++)
          {
            points[i].lat = lat;
            points[i].lon = (to_geo.lon - from_geo.lon) * i / (LINE_POINTS_NUM - 1) + from_geo.lon;
          }

        label = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LAT, lat, -value_power);
        hyscan_gtk_map_grid_draw_line (grid, cairo, points, LINE_POINTS_NUM, TRUE, label);
        g_free (label);
        lat += lat_step;
      }
  }

  /* Рисуем меридианы (longitude = const). */
  {
    gdouble from_lon, to_lon;
    gdouble lon_step;
    gdouble lon;

    steps = MAX (1, width / priv->step_width);

    /* Определяем границу видимой области по долготе. */
    coords1.x = from.x;
    coords2.x = to.x;
    coords1.y = coords2.y = (from.y + to.y) / 2.0;
    hyscan_gtk_map_value_to_geo (priv->map, &coords1_geo, coords1);
    hyscan_gtk_map_value_to_geo (priv->map, &coords2_geo, coords2);
    from_lon = CLAMP (coords1_geo.lon, -MAX_LON, MAX_LON);
    to_lon = CLAMP (coords2_geo.lon, -MAX_LON, MAX_LON);

    lon_step = (to_lon - from_lon) / steps;
    hyscan_gtk_map_grid_adjust_step (lon_step, &from_lon, &lon_step, &value_power);

    lon = from_lon;
    while (lon <= to_lon)
      {
        gchar *label;
        gsize i;
        HyScanGeoGeodetic points[LINE_POINTS_NUM];

        for (i = 0; i < LINE_POINTS_NUM; i++)
          {
            points[i].lat = (to_geo.lat - from_geo.lat) * i / (LINE_POINTS_NUM - 1) + from_geo.lat;
            points[i].lon = lon;
          }

        label = hyscan_units_format (priv->units, HYSCAN_UNIT_TYPE_LON, lon, -value_power);
        hyscan_gtk_map_grid_draw_line (grid, cairo, points, LINE_POINTS_NUM, FALSE, label);
        g_free (label);
        lon += lon_step;
      }
  }

  hyscan_gtk_map_grid_draw_north (grid, cairo, width - 30, height - 50);
}

static void
hyscan_gtk_map_grid_queue_draw (HyScanGtkMapGrid *grid)
{
  if (grid->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    gtk_widget_queue_draw (GTK_WIDGET (grid->priv->map));
}

/**
 * hyscan_gtk_map_grid_new:
 * @units: форматирование единиц измерения
 *
 * Создаёт слой координатной сетки.
 *
 * Returns: указатель на созданный слой
 */
HyScanGtkLayer *
hyscan_gtk_map_grid_new (HyScanUnits *units)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GRID,
                       "units", units,
                       NULL);
}

/**
 * hyscan_gtk_map_grid_set_bg_color:
 * @grid: указатель на #HyScanGtkMapGrid
 * @color: цвет подложки подписей
 *
 * Устанавливает цвет подложки подписей.
 */
void
hyscan_gtk_map_grid_set_bg_color (HyScanGtkMapGrid *grid,
                                  GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->bg_color = color;

  /* Цвет обводки делаем чуть темнее или светлее. */
  color.red += color.red > 0.5 ? -0.15 : 0.15;
  color.green += color.green > 0.5 ? -0.15 : 0.15;
  color.blue += color.blue > 0.5 ? -0.15 : 0.15;
  grid->priv->stroke_color = color;

  hyscan_gtk_map_grid_queue_draw (grid);
}

/**
 * hyscan_gtk_map_grid_set_line_color:
 * @grid: указатель на #HyScanGtkMapGrid
 * @color: цвет линий координатной сетки
 *
 * Устанавливает цвет линий координатной сетки.
 */
void
hyscan_gtk_map_grid_set_line_color (HyScanGtkMapGrid *grid,
                                    GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->line_color = color;
  hyscan_gtk_map_grid_queue_draw (grid);
}

/**
 * hyscan_gtk_map_grid_set_line_width:
 * @grid: указатель на #HyScanGtkMapGrid
 * @width: толщина линий координатной сетки
 *
 * Устанавливает толщину линии координатной сетки.
 */
void
hyscan_gtk_map_grid_set_line_width (HyScanGtkMapGrid *grid,
                                    gdouble           width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));
  g_return_if_fail (width > 0);

  grid->priv->line_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

/**
 * hyscan_gtk_map_grid_set_label_color:
 * @grid: указатель на #HyScanGtkMapGrid
 * @color: цвет текста подписей
 *
 * Устанавливает цвет текста подписей.
 */
void
hyscan_gtk_map_grid_set_label_color (HyScanGtkMapGrid *grid,
                                     GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->label_color = color;
  hyscan_gtk_map_grid_queue_draw (grid);
}

/**
 * hyscan_gtk_map_grid_set_step_width:
 * @grid: указатель на #HyScanGtkMapGrid
 * @width: расстояние между двумя соседними линиями координатной сетки
 *
 * Устанавливает желаемое расстояние между двумя соседними линиями координатной
 * сетки.
 */
void
hyscan_gtk_map_grid_set_step_width (HyScanGtkMapGrid *grid,
                                    guint             width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->step_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

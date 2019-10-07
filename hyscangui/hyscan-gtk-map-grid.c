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
 * Стиль оформления сетки можно задать с помощью функций класса или свойств из файла
 * конфигурации:
 *
 * - hyscan_gtk_map_grid_set_bg_color()      "bg-color"
 * - hyscan_gtk_map_grid_set_label_color()   "label-color"
 * - hyscan_gtk_map_grid_set_line_color()    "line-color"
 * - hyscan_gtk_map_grid_set_line_width()    "line-width"
 * - hyscan_gtk_map_grid_set_step_width()
 *
 * Загрузить конфигурационный файл можно функциями hyscan_gtk_layer_load_key_file()
 * или hyscan_gtk_layer_container_load_key_file().
 */

#include "hyscan-gtk-map-grid.h"
#include <glib/gi18n-lib.h>
#include <math.h>

#define MAX_LAT             90.0      /* Максимальное по модулю значение широты. */
#define MAX_LON             180.0     /* Максимальное по модулю значение долготы. */
#define LINE_POINTS_NUM     5         /* Количество точек, по которым строится линия сетки. */

#define LINE_WIDTH          0.5       /* Толщина линии координатной сетки. */
#define GRID_STEP           200       /* Среднее расстояние между соседними линиями сетки. */

#define LINE_COLOR_DEFAULT  "rgba( 80, 120, 180, 0.5)"   /* Цвет линий по умолчанию. */
#define LABEL_COLOR_DEFAULT "rgba( 33,  33,  33, 1.0)"   /* Цвет текста подписей по умолчанию. */
#define BG_COLOR_DEFAULT    "rgba(255, 255, 255, 0.6)"   /* Цвет фона подписей по умолчанию. */

enum
{
  PROP_O,
};

struct _HyScanGtkMapGridPrivate
{
  HyScanGtkMap                     *map;                /* Виджет карты, на котором показывается сетка. */

  PangoLayout                      *pango_layout;       /* Раскладка шрифта. */

  GdkRGBA                           line_color;         /* Цвет линий координатной сетки. */
  gdouble                           line_width;         /* Ширина линии координатной сетки. */
  GdkRGBA                           label_color;        /* Цвет подписей. */
  GdkRGBA                           bg_color;           /* Фоновый цвет подписей. */
  guint                             label_padding;      /* Отступы подписей от края видимой области. */

  guint                             step_width;         /* Шаг сетки в пикселях. */

  gboolean                          visible;            /* Признак того, что сетка отображается. */

};

static void     hyscan_gtk_map_grid_object_constructed      (GObject                 *object);
static void     hyscan_gtk_map_grid_object_finalize         (GObject                 *object);
static void     hyscan_gtk_map_grid_interface_init          (HyScanGtkLayerInterface *iface);
static void     hyscan_gtk_map_grid_draw                    (HyScanGtkMap            *map,
                                                             cairo_t                 *cairo,
                                                             HyScanGtkMapGrid        *grid);
static gboolean hyscan_gtk_map_grid_configure               (HyScanGtkMapGrid        *grid,
                                                             GdkEvent                *screen);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapGrid, hyscan_gtk_map_grid, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapGrid)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_grid_interface_init))

static void
hyscan_gtk_map_grid_class_init (HyScanGtkMapGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_gtk_map_grid_object_constructed;
  object_class->finalize = hyscan_gtk_map_grid_object_finalize;
}

static void
hyscan_gtk_map_grid_init (HyScanGtkMapGrid *gtk_map_grid)
{
  gtk_map_grid->priv = hyscan_gtk_map_grid_get_instance_private (gtk_map_grid);
}

static void
hyscan_gtk_map_grid_object_constructed (GObject *object)
{
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;
  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->constructed (object);

  priv->label_padding = 2;

  gdk_rgba_parse (&color, LINE_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_line_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_label_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, BG_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_bg_color (gtk_map_grid, color);

  hyscan_gtk_map_grid_set_line_width (gtk_map_grid, LINE_WIDTH);

  hyscan_gtk_map_grid_set_step_width (gtk_map_grid, GRID_STEP);
}

static void
hyscan_gtk_map_grid_object_finalize (GObject *object)
{
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;

  g_clear_object (&priv->pango_layout);
  g_object_unref (priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_grid_set_visible (HyScanGtkLayer *layer,
                                 gboolean        visible)
{
  HyScanGtkMapGridPrivate *priv = HYSCAN_GTK_MAP_GRID (layer)->priv;
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

static gboolean
hyscan_gtk_map_grid_load_key_file (HyScanGtkLayer *gtk_layer,
                                   GKeyFile       *key_file,
                                   const gchar    *group)
{
  HyScanGtkMapGrid *grid_layer = HYSCAN_GTK_MAP_GRID (gtk_layer);
  HyScanGtkMapGridPrivate *priv = grid_layer->priv;

  gdouble width;
  GdkRGBA color;

  width = g_key_file_get_double (key_file, group, "line-width", NULL);
  hyscan_gtk_map_grid_set_line_width (grid_layer, width > 0 ? width : LINE_WIDTH);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "bg-color", BG_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_bg_color (grid_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "label-color", LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_label_color (grid_layer, color);

  hyscan_gtk_layer_load_key_file_rgba (&color, key_file, group, "line-color", LINE_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_line_color (grid_layer, color);

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return TRUE;
}

static void
hyscan_gtk_map_grid_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->set_visible = hyscan_gtk_map_grid_set_visible;
  iface->get_visible = hyscan_gtk_map_grid_get_visible;
  iface->added = hyscan_gtk_map_grid_added;
  iface->load_key_file = hyscan_gtk_map_grid_load_key_file;
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
  priv->label_padding = width / 20;

  return GDK_EVENT_PROPAGATE;
}

/* Форматирует значение value, округляя его до 10^value_power знака. */
static void
hyscan_gtk_map_grid_format_label (gdouble  value,
                                  gint     value_power,
                                  gchar   *string,
                                  gsize    length)
{
  gchar text_format[128];

  if (value_power > 0)
    value_power = 0;
  g_snprintf (text_format, sizeof (text_format), "%%.%df°", -value_power);
  g_snprintf (string, length, text_format, value);
}

/* Рисуем параллель на широте latitude. */
static void
hyscan_gtk_map_grid_draw_lat (HyScanGtkMapGrid *grid,
                              cairo_t          *cairo,
                              gdouble           latitude,
                              gdouble           from_lon,
                              gdouble           to_lon,
                              gint              value_power)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  HyScanGeoCartesian2D point;
  gdouble grid_x;
  gdouble grid_y;

  gchar label[255];

  guint i;
  gdouble step;

  gint text_height;
  gint text_width;

  HyScanGeoGeodetic geo;

  geo.lat = latitude;
  geo.lon = from_lon;

  /* Рисуем подпись. */
  hyscan_gtk_map_grid_format_label (geo.lat, value_power, label, sizeof (label));
  hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
  gtk_cifro_area_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
  pango_layout_set_text (priv->pango_layout, label, -1);
  pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
  text_height /= PANGO_SCALE;
  text_width /= PANGO_SCALE;
  cairo_save (cairo);
  cairo_translate (cairo, priv->label_padding, grid_y - priv->label_padding);
  cairo_rotate (cairo, -G_PI / 2.0);

  cairo_rectangle (cairo,
                   -priv->label_padding, -priv->label_padding,
                   text_width + 2.0 * priv->label_padding, text_height + 2.0 * priv->label_padding);
  gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
  cairo_fill (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->label_color);
  pango_cairo_show_layout (cairo, priv->pango_layout);
  cairo_restore (cairo);

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    return;

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lon - from_lon) / (LINE_POINTS_NUM - 1);
  for (i = 0; i < LINE_POINTS_NUM; ++i)
    {
      geo.lon = from_lon + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
      gtk_cifro_area_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);
}

/* Рисует линию меридиана на долготе longitude. */
static void
hyscan_gtk_map_grid_draw_lon (HyScanGtkMapGrid *grid,
                              cairo_t          *cairo,
                              gdouble           longitude,
                              gdouble           from_lat,
                              gdouble           to_lat,
                              gint              value_power)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gdouble grid_x, grid_y;
  gchar label[255];
  guint i;
  gdouble step;

  gint text_width;
  gint text_height;

  HyScanGeoCartesian2D point;
  HyScanGeoGeodetic geo;

  geo.lon = longitude;
  geo.lat = to_lat;

  /* Рисуем подпись. */
  cairo_save (cairo);
  hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
  gtk_cifro_area_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
  hyscan_gtk_map_grid_format_label (longitude, value_power, label, sizeof (label));
  pango_layout_set_text (priv->pango_layout, label, -1);
  pango_layout_get_size (priv->pango_layout, &text_width, &text_height);
  text_height /= PANGO_SCALE;
  text_width /= PANGO_SCALE;

  cairo_translate (cairo, grid_x + priv->label_padding, priv->label_padding);
  cairo_rectangle (cairo,
                   -priv->label_padding, -priv->label_padding,
                   text_width + 2.0 * priv->label_padding, text_height + 2.0 * priv->label_padding);
  gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
  cairo_fill (cairo);

  gdk_cairo_set_source_rgba (cairo, &priv->label_color);
  pango_cairo_show_layout (cairo, priv->pango_layout);
  cairo_restore (cairo);

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    return;

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lat - from_lat) / (LINE_POINTS_NUM - 1);
  for (i = 0; i < LINE_POINTS_NUM; ++i)
    {
      geo.lat = from_lat + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
      gtk_cifro_area_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);
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

  /* Определяем размеры видимой области. */
  gtk_cifro_area_get_visible_size (carea, &width, &height);
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
        hyscan_gtk_map_grid_draw_lat (grid, cairo, lat, from_geo.lon, to_geo.lon, value_power);
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
        hyscan_gtk_map_grid_draw_lon (grid, cairo, lon, from_geo.lat, to_geo.lat, value_power);
        lon += lon_step;
      }
  }
}

static void
hyscan_gtk_map_grid_queue_draw (HyScanGtkMapGrid *grid)
{
  if (grid->priv->map != NULL && hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    gtk_widget_queue_draw (GTK_WIDGET (grid->priv->map));
}

/**
 * hyscan_gtk_map_grid_new:
 *
 * Создаёт слой координатной сетки.
 *
 * Returns: указатель на созданный слой
 */
HyScanGtkLayer *
hyscan_gtk_map_grid_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GRID, NULL);
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

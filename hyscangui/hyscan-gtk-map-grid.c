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
 * Слой с изображением координатной сетки и текущего масштаба карты. Масштаб карты
 * выводится в двух видах:
 * 1. численный масштаб в виде дроби, показывающий степень уменьшения проекции,
 *    например, 1:10000; степень уменьшения рассчитывается на основе PPI дисплея;
 * 2. графическое изображение линейки с подписью соответствующей её длины на местности.
 *
 * Стиль оформления сетки можно задать с помощью функций класса или свойств из файла
 * конфигурации:
 *
 * - hyscan_gtk_map_grid_set_bg_color()      "bg-color"
 * - hyscan_gtk_map_grid_set_label_color()   "label-color"
 * - hyscan_gtk_map_grid_set_line_color()    "line-color"
 * - hyscan_gtk_map_grid_set_line_width()    "line-width"
 * - hyscan_gtk_map_grid_set_step_width()
 * - hyscan_gtk_map_grid_set_scale_width()
 *
 * Загрузить конфигурационный файл можно функциями hyscan_gtk_layer_load_key_file()
 * или hyscan_gtk_layer_container_load_key_file().
 */

#include "hyscan-gtk-map-grid.h"
#include <glib/gi18n.h>
#include <math.h>

#define MAX_LAT             90.0      /* Максимальное по модулю значение широты. */
#define MAX_LON             180.0     /* Максимальное по модулю значение долготы. */
#define MIN_SCALE_SIZE_PX   50.0      /* Минимальная длина линейки масштаба. */
#define LINE_POINTS_NUM     5         /* Количество точек, по которым строится линия сетки. */

#define LINE_WIDTH          0.5       /* Толщина линии координатной сетки. */
#define GRID_STEP           200       /* Среднее расстояние между соседними линиями сетки. */
#define SCALE_WIDTH         2.0       /* Толщина линии линейки масштаба. */

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
  guint                             min_scale_size;     /* Минимальная длина линейки масштаба. */

  GdkRGBA                           line_color;         /* Цвет линий координатной сетки. */
  gdouble                           line_width;         /* Ширина линии координатной сетки. */
  GdkRGBA                           label_color;        /* Цвет подписей. */
  GdkRGBA                           bg_color;           /* Фоновый цвет подписей. */
  guint                             label_padding;      /* Отступы подписей от края видимой области. */
  gdouble                           scale_width;        /* Ширина линии линейки масштаба. */

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
static void     hyscan_gtk_map_grid_draw_grid               (HyScanGtkMapGrid        *grid,
                                                             cairo_t                 *cairo);
static void     hyscan_gtk_map_grid_draw_scale              (HyScanGtkMapGrid        *grid,
                                                             cairo_t                 *cairo);

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
  priv->min_scale_size = MIN_SCALE_SIZE_PX;

  gdk_rgba_parse (&color, LINE_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_line_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_label_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, BG_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_bg_color (gtk_map_grid, color);

  hyscan_gtk_map_grid_set_line_width (gtk_map_grid, LINE_WIDTH);
  hyscan_gtk_map_grid_set_scale_width (gtk_map_grid, SCALE_WIDTH);

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

  g_signal_connect_after (priv->map, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_grid_draw), gtk_layer);
  g_signal_connect_swapped (priv->map, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_grid_configure), gtk_layer);
}

static gboolean
hyscan_gtk_map_grid_load_key_file (HyScanGtkLayer          *gtk_layer,
                                   GKeyFile                *key_file,
                                   const gchar             *group)
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
  priv->min_scale_size = 2 * priv->label_padding + MAX (MIN_SCALE_SIZE_PX, width);

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
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
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

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lon - from_lon) / (LINE_POINTS_NUM - 1);
  for (i = 0; i < LINE_POINTS_NUM; ++i)
    {
      geo.lon = from_lon + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
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

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lat - from_lat) / (LINE_POINTS_NUM - 1);
  for (i = 0; i < LINE_POINTS_NUM; ++i)
    {
      geo.lat = from_lat + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);

  /* Рисуем подпись. */
  cairo_save (cairo);
  hyscan_gtk_map_geo_to_value (priv->map, geo, &point);
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, point.x, point.y);
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

/* Рисует координатную сетку и масштаб по сигналу "area-draw". */
static void
hyscan_gtk_map_grid_draw (HyScanGtkMap     *map,
                          cairo_t          *cairo,
                          HyScanGtkMapGrid *grid)
{
  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (grid)))
    return;

  hyscan_gtk_map_grid_draw_scale (grid, cairo);
  hyscan_gtk_map_grid_draw_grid (grid, cairo);
}

/* Рисует линейку масштаба карты. */
static void
hyscan_gtk_map_grid_draw_scale (HyScanGtkMapGrid *grid,
                                cairo_t          *cairo)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  guint width, height;
  gdouble x0, y0;

  gdouble scale;
  gdouble metres;

  /* Координаты размещения линейки. */
  gtk_cifro_area_get_visible_size (carea, &width, &height);
  x0 = width - priv->label_padding;
  y0 = height - priv->label_padding;

  /* Определяем размер линейки, чтобы он был круглым числом метров. */
  scale = hyscan_gtk_map_get_pixel_scale (priv->map);
  metres = 1.6 * priv->min_scale_size / scale;
  metres = pow (10, floor (log10 (metres)));
  while (metres * scale < priv->min_scale_size)
    metres *= 2.0;

  /* Рисуем линейку и подпись к ней. */
  {
    PangoRectangle inc_rect, logical_rect;

    gchar label[128];
    gint label_width, label_height;

    gchar scale_name[128];
    gint scale_name_width, scale_name_height;

    gdouble ruler_width;

    gdouble spacing;

    gdouble real_scale;

    real_scale = 1.0 / hyscan_gtk_map_get_scale (priv->map);

    /* Название масштаба. */
    if (real_scale > 10.0)
      g_snprintf (scale_name, sizeof (scale_name), "1:%.0f", real_scale);
    else if (real_scale > 1.0)
      g_snprintf (scale_name, sizeof (scale_name), "1:%.1f", real_scale);
    else if (real_scale > 0.1)
      g_snprintf (scale_name, sizeof (scale_name), "%.1f:1", 1 / real_scale);
    else
      g_snprintf (scale_name, sizeof (scale_name), "%.0f:1", 1 / real_scale);

    /* Различный форматы записи длины линейки масштаба. */
    if (metres > 1000.0)
      g_snprintf (label, sizeof (label), "%.0f %s", metres / 1000.0, _("km"));
    else if (metres > 1.0)
      g_snprintf (label, sizeof (label), "%.0f %s", metres, _("m"));
    else if (metres > 0.01)
      g_snprintf (label, sizeof (label), "%.0f %s", metres * 100.0, _("cm"));
    else
      g_snprintf (label, sizeof (label), "%.0f %s", metres * 1000.0, _("mm"));

    /* Вычисляем размеры подписи линейки. */
    pango_layout_set_text (priv->pango_layout, label, -1);
    pango_layout_get_pixel_extents (priv->pango_layout, &inc_rect, &logical_rect);
    label_width = logical_rect.width;
    label_height = logical_rect.height;
    spacing = label_height;

    /* Вычисляем размеры названия масштаба. */
    pango_layout_set_text (priv->pango_layout, scale_name, -1);
    pango_layout_get_pixel_extents (priv->pango_layout, &inc_rect, &logical_rect);
    scale_name_width = logical_rect.width;
    scale_name_height = logical_rect.height;

    ruler_width = metres * scale;

    /* Рисуем подложку. */
    gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
    cairo_rectangle (cairo, x0 + priv->label_padding, y0 + priv->label_padding,
                     - (scale_name_width + ruler_width + spacing + 2.0 * priv->label_padding),
                     - (label_height + 2.0 * priv->label_padding + priv->scale_width));
    cairo_fill (cairo);

    /* Рисуем линейку масштаба. */
    cairo_move_to (cairo, x0 - priv->scale_width / 2.0, y0 - label_height / 2.0);
    cairo_rel_line_to (cairo, 0, label_height / 2.0);
    cairo_rel_line_to (cairo, -ruler_width, 0);
    cairo_rel_line_to (cairo, 0, -label_height / 2.0);
    gdk_cairo_set_source_rgba (cairo, &priv->label_color);
    cairo_set_line_width (cairo, priv->scale_width);
    cairo_stroke (cairo);

    /* Рисуем название масштаба. */
    pango_layout_set_text (priv->pango_layout, scale_name, -1);
    cairo_move_to (cairo,
                   x0 - ruler_width - spacing - scale_name_width,
                   y0 - scale_name_height - priv->scale_width);
    pango_cairo_show_layout (cairo, priv->pango_layout);

    /* Рисуем подпись линейке. */
    {
      pango_layout_set_text (priv->pango_layout, label, -1);
      cairo_move_to (cairo,
                     x0 - label_width / 2.0 - ruler_width / 2.0,
                     y0 - label_height - priv->scale_width);
      pango_cairo_show_layout (cairo, priv->pango_layout);
    }
  }
}

/* Рисует координатную сетку с подписями. */
static void
hyscan_gtk_map_grid_draw_grid (HyScanGtkMapGrid *grid,
                               cairo_t          *cairo)
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
 * Устанавливает цвет подложки подписей
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
 * hyscan_gtk_map_grid_set_scale_width:
 * @grid: указатель на #HyScanGtkMapGrid
 * @width: толщина линии в изображении линейки масштаба
 *
 * Устанавливает толщину линии в изображении линейки масштаба.
 */
void
hyscan_gtk_map_grid_set_scale_width (HyScanGtkMapGrid *grid,
                                     gdouble           width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));
  g_return_if_fail (width > 0);

  grid->priv->scale_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

/**
 * hyscan_gtk_map_grid_set_label_color:
 * @grid: указатель на #HyScanGtkMapGrid
 * @color: цвет текста подписей
 *
 * Устанавливает цвет текста подписей
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

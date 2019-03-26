#include "hyscan-gtk-map-grid.h"
#include <math.h>

#define MAX_LAT             90.0      /* Максимальное по модулю значение широты. */
#define MAX_LON             180.0     /* Максимальное по модулю значение долготы. */
#define MAX_SCALE_SIZE_PX   100.0     /* Максимальная длина линейки масштаба. */
#define LINE_POINTS_NUM     5         /* Количество точек, по которым строится линия сетки. */

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
  gdouble                           scale_width;        /* Ширина линии линейки масштаба. */

  guint                             step_width;         /* Шаг сетки в пикселах. */

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

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapGrid, hyscan_gtk_map_grid, G_TYPE_OBJECT,
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
  GdkRGBA color;

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->constructed (object);

  gdk_rgba_parse (&color, LINE_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_line_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, LABEL_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_label_color (gtk_map_grid, color);

  gdk_rgba_parse (&color, BG_COLOR_DEFAULT);
  hyscan_gtk_map_grid_set_bg_color (gtk_map_grid, color);

  hyscan_gtk_map_grid_set_line_width (gtk_map_grid, 0.5);
  hyscan_gtk_map_grid_set_scale_width (gtk_map_grid, 2.0);

  hyscan_gtk_map_grid_set_step_width (gtk_map_grid, 200);
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

  priv->map = g_object_ref (container);

  g_signal_connect_after (priv->map, "visible-draw",
                          G_CALLBACK (hyscan_gtk_map_grid_draw), gtk_layer);
  g_signal_connect_swapped (priv->map, "configure-event",
                            G_CALLBACK (hyscan_gtk_map_grid_configure), gtk_layer);
}

static void
hyscan_gtk_map_grid_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->set_visible = hyscan_gtk_map_grid_set_visible;
  iface->get_visible = hyscan_gtk_map_grid_get_visible;
  iface->added = hyscan_gtk_map_grid_added;
}

/* Обновление раскладки шрифта по сигналу "configure-event". */
static gboolean
hyscan_gtk_map_grid_configure (HyScanGtkMapGrid *grid,
                               GdkEvent         *screen)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  gint size;

  g_clear_object (&priv->pango_layout);
  priv->pango_layout = gtk_widget_create_pango_layout (GTK_WIDGET (priv->map), "-0123456789.°km");

  pango_layout_get_size (priv->pango_layout, NULL, &size);
  if (size < 0)
    size = 0;

  size /= PANGO_SCALE;
  priv->label_padding = (guint) size / 3;

  return FALSE;
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
  hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
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
      hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
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

  HyScanGeoGeodetic geo;

  geo.lon = longitude;
  geo.lat = to_lat;

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lat - from_lat) / (LINE_POINTS_NUM - 1);
  for (i = 0; i < LINE_POINTS_NUM; ++i)
    {
      geo.lat = from_lat + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_set_line_width (cairo, priv->line_width);
  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);

  /* Рисуем подпись. */
  cairo_save (cairo);
  hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
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
  metres = .8 * MAX_SCALE_SIZE_PX / scale;
  metres = pow (10, floor (log10 (metres)));
  while (metres * scale < MAX_SCALE_SIZE_PX / 2.0)
    metres *= 2.0;

  /* Рисуем линейку и подпись к ней. */
  {
    gchar label[256];
    PangoRectangle inc_rect, logical_rect;

    gint text_width;
    gint text_height;
    gdouble ruler_width;

    gdouble spacing;

    gdouble real_scale;

    real_scale = 1.0 / hyscan_gtk_map_get_scale (priv->map);

    /* Формируем текст надписи и вычисляем её размер. */
    if (metres < 1000.0)
      g_snprintf (label, sizeof (label), "1:%.0f, %.0f m", real_scale, metres);
    else
      g_snprintf (label, sizeof (label), "1:%.0f, %.0f km", real_scale, metres / 1000.0);
    pango_layout_set_text (priv->pango_layout, label, -1);
    pango_layout_get_pixel_extents (priv->pango_layout, &inc_rect, &logical_rect);
    text_width = logical_rect.width;
    text_height = inc_rect.height + inc_rect.y;
    spacing = text_height;

    /* Рисуем линейку. */
    ruler_width = metres * scale;

    gdk_cairo_set_source_rgba (cairo, &priv->bg_color);
    cairo_rectangle (cairo, x0 + priv->label_padding, y0 + priv->label_padding,
                    -(text_width + ruler_width + spacing + 2.0 * priv->label_padding), - text_height - 2.0 * priv->label_padding);
    cairo_fill (cairo);


    cairo_move_to (cairo, x0 - priv->scale_width / 2.0, y0 - text_height / 2.0);
    cairo_rel_line_to (cairo, 0, text_height / 2.0);
    cairo_rel_line_to (cairo, -ruler_width, 0);
    cairo_rel_line_to (cairo, 0, -text_height / 2.0);
    gdk_cairo_set_source_rgba (cairo, &priv->label_color);
    cairo_set_line_width (cairo, priv->scale_width);
    cairo_stroke (cairo);

    /* Рисуем надпись. */
    cairo_move_to (cairo, x0 - text_width - ruler_width - spacing, y0 - text_height);
    pango_cairo_show_layout (cairo, priv->pango_layout);
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

  HyScanGeoGeodetic coords1;
  HyScanGeoGeodetic coords2;

  HyScanGeoGeodetic from_geo;
  HyScanGeoGeodetic to_geo;

  /* Определяем размеры видимой области. */
  gtk_cifro_area_get_visible_size (carea, &width, &height);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_limits (carea, &min_x, &max_x, &min_y, &max_y);
  from_x = CLAMP (from_x, min_x, max_x);
  to_x = CLAMP (to_x, min_x, max_x);
  from_y = CLAMP (from_y, min_y, max_y);
  to_y = CLAMP (to_y, min_y, max_y);

  hyscan_gtk_map_value_to_geo (priv->map, &from_geo, from_x, from_y);
  hyscan_gtk_map_value_to_geo (priv->map, &to_geo, to_x, to_y);

  /* Рисуем параллели (latitude = const). */
  {
    gdouble from_lat, to_lat;
    gdouble lat_step;
    gdouble lat;

    steps = MAX(1, height / priv->step_width);

    /* Определяем границу видимой области по широте. */
    hyscan_gtk_map_value_to_geo (priv->map, &coords1, (from_x + to_x) / 2.0, from_y);
    hyscan_gtk_map_value_to_geo (priv->map, &coords2, (from_x + to_x) / 2.0, to_y);
    from_lat = CLAMP (coords1.lat, -MAX_LAT, MAX_LAT);
    to_lat = CLAMP (coords2.lat, -MAX_LAT, MAX_LAT);

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
    hyscan_gtk_map_value_to_geo (priv->map, &coords1, from_x, (from_y + to_y) / 2.0);
    hyscan_gtk_map_value_to_geo (priv->map, &coords2, to_x, (from_y + to_y) / 2.0);
    from_lon = CLAMP (coords1.lon, -MAX_LON, MAX_LON);
    to_lon = CLAMP (coords2.lon, -MAX_LON, MAX_LON);

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

HyScanGtkMapGrid *
hyscan_gtk_map_grid_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GRID, NULL);
}

void
hyscan_gtk_map_grid_set_bg_color (HyScanGtkMapGrid *grid,
                                  GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->bg_color = color;
  hyscan_gtk_map_grid_queue_draw (grid);
}

void
hyscan_gtk_map_grid_set_line_color (HyScanGtkMapGrid *grid,
                                    GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->line_color = color;
  hyscan_gtk_map_grid_queue_draw (grid);
}

void
hyscan_gtk_map_grid_set_line_width (HyScanGtkMapGrid *grid,
                                    gdouble           width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->line_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

void
hyscan_gtk_map_grid_set_scale_width (HyScanGtkMapGrid *grid,
                                     gdouble           width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->scale_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

void
hyscan_gtk_map_grid_set_label_color  (HyScanGtkMapGrid *grid,
                                      GdkRGBA           color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->label_color = color;
  hyscan_gtk_map_grid_queue_draw (grid);
}

void
hyscan_gtk_map_grid_set_step_width (HyScanGtkMapGrid *grid,
                                    guint             width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_GRID (grid));

  grid->priv->step_width = width;
  hyscan_gtk_map_grid_queue_draw (grid);
}

#include "hyscan-gtk-map-grid.h"
#include <math.h>

enum
{
  PROP_O,
  PROP_MAP
};

struct _HyScanGtkMapGridPrivate
{
  HyScanGtkMap                     *map;

  guint                             points;     /* Количество точек, по которым строятся линии сетки (для прямых достаточно 2 точек.) */
  guint                             margin;     /* Отступ подписи линии сетки. */
  guint                             step_width; /* Шаг сетки в пикселах. */
};

static void    hyscan_gtk_map_grid_set_property             (GObject               *object,
                                                             guint                  prop_id,
                                                             const GValue          *value,
                                                             GParamSpec            *pspec);
static void    hyscan_gtk_map_grid_object_constructed       (GObject               *object);
static void    hyscan_gtk_map_grid_object_finalize          (GObject               *object);
static void    hyscan_gtk_map_grid_draw                     (HyScanGtkMapGrid      *grid,
                                                             cairo_t               *cairo);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanGtkMapGrid, hyscan_gtk_map_grid, G_TYPE_OBJECT)

static void
hyscan_gtk_map_grid_class_init (HyScanGtkMapGridClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_grid_set_property;

  object_class->constructed = hyscan_gtk_map_grid_object_constructed;
  object_class->finalize = hyscan_gtk_map_grid_object_finalize;

  g_object_class_install_property (object_class, PROP_MAP,
    g_param_spec_object ("map", "Map", "GtkMap widget", HYSCAN_TYPE_GTK_MAP,
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
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;

  switch (prop_id)
    {
    case PROP_MAP:
      priv->map = g_value_dup_object (value);
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

  priv->points = 5;
  priv->margin = 5;
  priv->step_width = 100;

  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_grid_draw), gtk_map_grid);
}

static void
hyscan_gtk_map_grid_object_finalize (GObject *object)
{
  HyScanGtkMapGrid *gtk_map_grid = HYSCAN_GTK_MAP_GRID (object);
  HyScanGtkMapGridPrivate *priv = gtk_map_grid->priv;

  g_object_unref (priv->map);

  G_OBJECT_CLASS (hyscan_gtk_map_grid_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_grid_format_label (gdouble value,
                                  gint    value_power,
                                  gchar  *string,
                                  gsize   length)
{
  gchar text_format[128];

  if (value_power > 0)
    value_power = 0;
  g_snprintf (text_format, sizeof(text_format), "%%.%df°", -value_power);
  g_snprintf (string, length, text_format, value);
}

/* Рисуем параллель на широте latitude. */
static void
hyscan_gtk_map_grid_draw_lat (HyScanGtkMapGrid *grid,
                              gdouble           latitude,
                              gdouble           from_lon,
                              gdouble           to_lon,
                              gint              value_power,
                              cairo_t          *cairo)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gdouble grid_x;
  gdouble grid_y;

  gchar label[255];

  guint i;
  gdouble step;

  HyScanGeoGeodetic geo;

  geo.lat = latitude;
  geo.lon = from_lon;

  /* Рисуем подпись. */
  hyscan_gtk_map_grid_format_label (geo.lat, value_power, label, sizeof (label));
  hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
  cairo_move_to (cairo, priv->margin, grid_y - priv->margin);
  cairo_show_text (cairo, label);

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lon - from_lon) / (priv->points - 1);
  for (i = 0; i < priv->points; ++i)
    {
      geo.lon = from_lon + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_stroke (cairo);
}

/* Рисует линию меридиана на долготе longitude. */
static void
hyscan_gtk_map_grid_draw_lon (HyScanGtkMapGrid *grid,
                              gdouble           longitude,
                              gdouble           from_lat,
                              gdouble           to_lat,
                              gint              value_power,
                              cairo_t          *cairo)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);

  gdouble grid_x, grid_y;
  gchar label[255];
  guint i;
  gdouble step;

  HyScanGeoGeodetic geo;

  geo.lon = longitude;
  geo.lat = to_lat;

  /* Рисуем подпись. */
  hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
  gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
  hyscan_gtk_map_grid_format_label (longitude, value_power, label, sizeof (label));
  cairo_move_to (cairo, grid_x + priv->margin, priv->margin + 10 /* 10 = font height */);
  cairo_show_text (cairo, label);

  /* Рисуем линию. */
  cairo_new_path (cairo);
  step = (to_lat - from_lat) / (priv->points - 1);
  for (i = 0; i < priv->points; ++i)
    {
      geo.lat = from_lat + step * i;
      hyscan_gtk_map_geo_to_value (priv->map, geo, &grid_x, &grid_y);
      gtk_cifro_area_visible_value_to_point (carea, &grid_x, &grid_y, grid_x, grid_y);
      cairo_line_to (cairo, grid_x, grid_y);
    }
  cairo_stroke (cairo);
}

static void
hyscan_gtk_map_grid_adjust_step (gdouble  step_length,
                                 gdouble *from,
                                 gdouble *step,
                                 gint    *power)
{
  gint power_ret;
  gdouble from_ret;
  gdouble step_ret;

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

  if (step_length > 5)
    {
      step_length /= 10.0;
      power_ret = power_ret + 1;
    }

  /* Выбираем с каким шагом рисовать сетку: 1, 2 или 5 (плюс их степени). */

  /* Расчитываем разность между размером ячейки для трёх возможных вариантов сетки и
   * предпочтительным размером ячейки определённым пользователем. */

  gdouble axis_1_width_delta;
  gdouble axis_2_width_delta;
  gdouble axis_5_width_delta;

  gdouble axis_1_score;
  gdouble axis_2_score;
  gdouble axis_5_score;

  axis_1_width_delta = 1.0 - step_length;
  axis_2_width_delta = 2.0 - step_length;
  axis_5_width_delta = 5.0 - step_length;

  /* Расчитываем "вес" каждого варианта. */
  axis_1_score = (axis_1_width_delta >= 0.0) ? 1.0 / axis_1_width_delta : -0.1 / axis_1_width_delta;
  axis_2_score = (axis_2_width_delta >= 0.0) ? 1.0 / axis_2_width_delta : -0.1 / axis_2_width_delta;
  axis_5_score = (axis_5_width_delta >= 0.0) ? 1.0 / axis_5_width_delta : -0.1 / axis_5_width_delta;

  if ((axis_1_score > axis_2_score) && (axis_1_score > axis_5_score))
    step_ret = 1.0 * pow (10, power_ret);
  else if (axis_2_score > axis_5_score)
    step_ret = 2.0 * pow (10, power_ret);
  else
    step_ret = 5.0 * pow (10, power_ret);

  from_ret = step_ret * floor (*from / step_ret);
  if (from_ret < *from)
    from_ret += step_ret;

  *from = from_ret;
  *step = step_ret;
  *power = power_ret;
}

static void
hyscan_gtk_map_draw_lats ()
{

}

static void
hyscan_gtk_map_grid_draw (HyScanGtkMapGrid *grid,
                          cairo_t          *cairo)
{
  HyScanGtkMapGridPrivate *priv = grid->priv;
  GtkCifroArea *carea = GTK_CIFRO_AREA (priv->map);
  gdouble from_x, from_y, to_x, to_y;
  gdouble min_x, min_y, max_x, max_y;

  gdouble lat_step;
  gint steps;

  guint step_w = 100;
  guint width, height;

  gint value_power;

  gdouble from_lat, to_lat;

  HyScanGeoGeodetic coords1;
  HyScanGeoGeodetic coords2;

  gtk_cifro_area_get_visible_size (carea, &width, &height);
  gtk_cifro_area_get_view (carea, &from_x, &to_x, &from_y, &to_y);
  gtk_cifro_area_get_limits (carea, &min_x, &max_x, &min_y, &max_y);
  from_x = CLAMP (from_x, min_x, max_x);
  to_x = CLAMP (to_x, min_x, max_x);
  from_y = CLAMP (from_y, min_y, max_y);
  to_y = CLAMP (to_y, min_y, max_y);
  
  hyscan_gtk_map_value_to_geo (priv->map, &coords1, (from_x + to_x) / 2, from_y);
  hyscan_gtk_map_value_to_geo (priv->map, &coords2, (from_x + to_x) / 2, to_y);

  steps = width / step_w;
  from_lat = isinf (coords1.lat) ? -90.0 : coords1.lat;
  to_lat = isinf (coords2.lat) ? 90.0 : coords2.lat;
  lat_step = (to_lat - from_lat) / steps;
  hyscan_gtk_map_grid_adjust_step (lat_step, &from_lat, &lat_step, &value_power);

  cairo_set_line_width (cairo, 0.5);
  cairo_set_source_rgba (cairo, 0, 0, 0.2, 0.2);

  /* Рисуем параллели. */
  gdouble lat = from_lat;
  hyscan_gtk_map_value_to_geo (priv->map, &coords1, from_x, from_y);
  hyscan_gtk_map_value_to_geo (priv->map, &coords2, to_x, to_y);
  while (lat <= to_lat)
    {
      hyscan_gtk_map_grid_draw_lat (grid, lat, coords1.lon, coords2.lon, value_power, cairo);
      lat += lat_step;
    }

  /* Рисуем меридианы. */
  gdouble from_lon, to_lon, lon_step;

  hyscan_gtk_map_value_to_geo (priv->map, &coords1, from_x, (from_y + to_y) / 2);
  hyscan_gtk_map_value_to_geo (priv->map, &coords2, to_x, (from_y + to_y) / 2);

  steps = height / step_w;
  from_lon = isinf (coords1.lon) ? -180.0 : coords1.lon;
  to_lon = isinf (coords2.lat) ? 180.0 : coords2.lon;
  lon_step = (to_lon - from_lon) / steps;
  hyscan_gtk_map_grid_adjust_step (lon_step, &from_lon, &lon_step, &value_power);

  gdouble lon = from_lon;
  hyscan_gtk_map_value_to_geo (priv->map, &coords1, from_x, from_y);
  hyscan_gtk_map_value_to_geo (priv->map, &coords2, to_x, to_y);
  while (lon <= to_lon)
    {
      hyscan_gtk_map_grid_draw_lon (grid, lon, coords1.lat, coords2.lat, value_power, cairo);
      lon += lon_step;
    }
}

HyScanGtkMapGrid *
hyscan_gtk_map_grid_new (HyScanGtkMap* map)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_GRID, "map", map, NULL);
}

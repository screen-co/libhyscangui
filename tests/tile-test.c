/* tile-test.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
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

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#include <hyscan-gtk-map-tile.h>

typedef struct {
  guint   zoom;
  gdouble x_tile;
  gdouble y_tile;

  gdouble x_val;
  gdouble y_val;
} TestPoint;

gdouble scales[] = {
  100.0,            /* zoom 0. */
  50.0,             /* zoom 1. */
  25.0,             /* zoom 2. */
  20.0              /* zoom 3. */
};

TestPoint points[] = {
  { .zoom = 0, .x_tile = 0, .y_tile = 0, .x_val = -100.0, .y_val = 100.0 },
  { .zoom = 0, .x_tile = 1, .y_tile = 1, .x_val = 0,      .y_val = 0},
  { .zoom = 1, .x_tile = 1, .y_tile = 1, .x_val = -50.0,  .y_val = 50.0 },
  { .zoom = 2, .x_tile = 1, .y_tile = 2, .x_val = -75.0,  .y_val = 50.0 },
  { .zoom = 3, .x_tile = 3, .y_tile = 1, .x_val = -40.0,  .y_val = 80.0 },
};

int
main (int    argc,
      char **argv)
{
  HyScanGtkMapTileGrid *grid;
  guint i;

  grid = hyscan_gtk_map_tile_grid_new (-100.0, 100.0, -100.0, 100.0, 0, 256);
  hyscan_gtk_map_tile_grid_set_scales (grid, scales, G_N_ELEMENTS (scales));

  for (i = 0; i < G_N_ELEMENTS (points); i++)
    {
      HyScanGeoCartesian2D from, to;
      gdouble x_val, y_val, x_tile, y_tile;
      HyScanGtkMapTile *tile;

      TestPoint *point = &points[i];

      tile = hyscan_gtk_map_tile_new (grid, (guint) point->x_tile, (guint) point->y_tile, point->zoom);

      hyscan_gtk_map_tile_grid_tile_to_value (grid, point->zoom, point->x_tile, point->y_tile, &x_val, &y_val);
      g_assert_cmpfloat (point->x_val, ==, x_val);
      g_assert_cmpfloat (point->y_val, ==, y_val);

      hyscan_gtk_map_tile_grid_value_to_tile (grid, point->zoom, point->x_val, point->y_val, &x_tile, &y_tile);
      g_assert_cmpfloat (point->x_tile, ==, x_tile);
      g_assert_cmpfloat (point->y_tile, ==, y_tile);

      hyscan_gtk_map_tile_get_bounds (tile, &from, &to);
      g_assert_cmpfloat (from.x, ==, point->x_val);
      g_assert_cmpfloat (from.y, ==, point->y_val);
      g_assert_cmpfloat (to.x,   ==, point->x_val + scales[point->zoom]);
      g_assert_cmpfloat (to.y,   ==, point->y_val - scales[point->zoom]);
    }

  g_message ("Test finished successfully");

  return 0;
}

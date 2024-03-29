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

#include <hyscan-map-tile.h>

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

void
print_iter_order (gint *order,
                  guint n_x,
                  guint n_y)
{
  guint x, y;
  g_print ("Iterate order:\n");

  for (y = 0; y < n_y; y++)
    {
      for (x = 0; x < n_x; x++)
        g_print ("%6d", order[x + y * n_x]);

      g_print ("\n");
    }

  g_print ("\n\n");
}

void
test_iter (guint from_x,
           guint to_x,
           guint from_y,
           guint to_y)
{
  HyScanMapTileIter iter;
  guint x, y;
  gsize i = 0;
  gsize n_elements;
  gint *tiles;
  guint n_x, n_y;

  n_x = (to_x - from_x + 1);
  n_y = (to_y - from_y + 1);
  n_elements = n_x * n_y;
  tiles = g_new0 (gint, n_elements);

  hyscan_map_tile_iter_init (&iter, from_x, to_x, from_y, to_y);
  while (hyscan_map_tile_iter_next (&iter, &x, &y))
    tiles[(x - from_x) + (y - from_y) * n_x] = ++i;

  print_iter_order (tiles, n_x, n_y);

  g_assert (i == n_elements);
  for (i = 0; i < n_elements; ++i)
    g_assert (tiles[i] > 0);

  g_free (tiles);
}

int
main (int    argc,
      char **argv)
{
  HyScanMapTileGrid *grid;
  gsize i;

  grid = hyscan_map_tile_grid_new (-100.0, 100.0, -100.0, 100.0, 0, 256);
  hyscan_map_tile_grid_set_scales (grid, scales, G_N_ELEMENTS (scales));

  for (i = 0; i < G_N_ELEMENTS (points); i++)
    {
      HyScanGeoCartesian2D from, to;
      gdouble x_val, y_val, x_tile, y_tile;
      HyScanMapTile *tile;

      TestPoint *point = &points[i];

      tile = hyscan_map_tile_new (grid, (guint) point->x_tile, (guint) point->y_tile, point->zoom);

      hyscan_map_tile_grid_tile_to_value (grid, point->zoom, point->x_tile, point->y_tile, &x_val, &y_val);
      g_assert_cmpfloat (point->x_val, ==, x_val);
      g_assert_cmpfloat (point->y_val, ==, y_val);

      hyscan_map_tile_grid_value_to_tile (grid, point->zoom, point->x_val, point->y_val, &x_tile, &y_tile);
      g_assert_cmpfloat (point->x_tile, ==, x_tile);
      g_assert_cmpfloat (point->y_tile, ==, y_tile);

      hyscan_map_tile_get_bounds (tile, &from, &to);
      g_assert_cmpfloat (from.x, ==, point->x_val);
      g_assert_cmpfloat (from.y, ==, point->y_val);
      g_assert_cmpfloat (to.x,   ==, point->x_val + scales[point->zoom]);
      g_assert_cmpfloat (to.y,   ==, point->y_val - scales[point->zoom]);
    }

  test_iter (0, 5, 0, 5);
  test_iter (102, 110, 99, 103);

  g_print ("Test finished successfully");

  return 0;
}

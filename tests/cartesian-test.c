/* cartesian-test.c
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

#include <hyscan-cartesian.h>

static HyScanGeoCartesian2D points_inside[][2] = {
  {{ -1.0, -1.0}, {  2.0,  2.0   }},
  {{  0.5,  1.0}, {  0.5, -100.0 }},
  {{  0.5,  0.5}, { -2.0,  2.0   }},
  {{  0.5,  2.0}, {  0.6, -2.0   }},
};

static HyScanGeoCartesian2D points_outside[][2] = {
  {{  -1.0,  0.0}, { -2.0, 2.0}},
};

int
main (int    argc,
      char **argv)
{
  guint i;

  HyScanGeoCartesian2D from = {.x = 0.0, .y = 0.0 };
  HyScanGeoCartesian2D   to = {.x = 1.0, .y = 1.0 };

  for (i = 0; i < G_N_ELEMENTS (points_inside); ++i)
    {
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][0], &points_inside[i][1], &from, &to));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][1], &points_inside[i][0], &from, &to));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][0], &points_inside[i][1], &to, &from));
      g_assert_true (hyscan_cartesian_is_inside (&points_inside[i][1], &points_inside[i][0], &to, &from));
    }

  for (i = 0; i < G_N_ELEMENTS (points_outside); ++i)
    {
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][0], &points_outside[i][1], &from, &to));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][1], &points_outside[i][0], &from, &to));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][0], &points_outside[i][1], &to, &from));
      g_assert_false (hyscan_cartesian_is_inside (&points_outside[i][1], &points_outside[i][0], &to, &from));
    }

  g_message ("Test done successfully");

  return 0;
}

/* hyscan-gtk-waterfall.h
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
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
 * SECTION: hyscan-gtk-waterfall-tools
 * @Title: HyScanGtkWaterfall Tools
 * @Short_description: вспомогательные функции для водопада
 *
 */
#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>

/**
 * hyscan_gtk_waterfall_tools_distance:
 * Функция определяет расстояние между точками.
 *
 * @start: начало
 * @end: конец
 *
 * Returns: расстояние.
 */
gdouble
hyscan_gtk_waterfall_tools_distance(HyScanCoordinates *start,
                                    HyScanCoordinates *end)
{
  return sqrt (pow (ABS(end->x - start->x), 2) + pow (ABS(end->y - start->y), 2));
}

/**
 * hyscan_gtk_waterfall_tools_angle:
 * @start: начало отрезка
 * @end: конец отрезка
 *
 * Функция определяет угол наклона отрезка.
 *
 * Returns: угол.
 */
gdouble
hyscan_gtk_waterfall_tools_angle (HyScanCoordinates *start,
                                  HyScanCoordinates *end)
{
  if (start->x == end->x)
    return 0;

  return -atan ((end->y - start->y) / (end->x - start->x));
}

/**
 * hyscan_gtk_waterfall_tools_middle:
 * @start: начало отрезка
 * @end: конец отрезка
 *
 * Функция возвращает середину отрезка.
 *
 * Returns: середина отрезка.
 */
HyScanCoordinates
hyscan_gtk_waterfall_tools_middle (HyScanCoordinates *start,
                                   HyScanCoordinates *end)
{
  HyScanCoordinates ret;

  ret.x = start->x + (end->x - start->x) / 2.0;
  ret.y = start->y + (end->y - start->y) / 2.0;

  return ret;
}

/**
 * hyscan_gtk_waterfall_tools_line_in_square:
 * @line_start: начало отрезка
 * @line_end: конец отрезка
 * @square_start: начальная координата прямоугольника
 * @square_end: конечная координата прямоугольника
 *
 * Функция проверяет, попадает ли отрезок хоть одной точкой внутрь прямоугольника.
 *
 * Returns: TRUE, если попадает.
 */
gboolean
hyscan_gtk_waterfall_tools_line_in_square (HyScanCoordinates *line_start,
                                           HyScanCoordinates *line_end,
                                           HyScanCoordinates *square_start,
                                           HyScanCoordinates *square_end)
{
  gboolean inside, tilted;
  gdouble k, b, ys, ye, xs, xe;

  inside  = hyscan_gtk_waterfall_tools_point_in_square (line_start, square_start, square_end);
  inside |= hyscan_gtk_waterfall_tools_point_in_square (line_end, square_start, square_end);

  if (inside)
    return TRUE;


  tilted = hyscan_gtk_waterfall_tools_line_k_b_calc (line_start, line_end, &k, &b);

  if (!tilted)
    {
      return (line_start->x > MIN (square_start->x, square_end->x) &&
              line_start->x < MAX (square_start->x, square_end->x));
    }

  /* Придётся искать место пересечения c вертикальными... */
  ys = k * MIN (square_start->x, square_end->x) + b;
  ye = k * MAX (square_start->x, square_end->x) + b;
  /* ... и горизонтальными границами. */
  xs = (MIN (square_start->y, square_end->y) - b) / k;
  xe = (MAX (square_start->y, square_end->y) - b) / k;

  if ((ys > MIN (square_start->y, square_end->y) && ys < MAX (square_start->y, square_end->y)) ||
      (ye > MIN (square_start->y, square_end->y) && ye < MAX (square_start->y, square_end->y)) ||
      (xs > MIN (square_start->x, square_end->x) && xs < MAX (square_start->x, square_end->x)) ||
      (xe > MIN (square_start->x, square_end->x) && xe < MAX (square_start->x, square_end->x)))
      {
        return TRUE;
      }

  return FALSE;
}

/**
 * hyscan_gtk_waterfall_tools_line_k_b_calc:
 * @line_start: начало отрезка
 * @line_end: конец отрезка
 * @k: коэффициент k
 * @b: коэффициент b
 *
 * Функция подсчитывает коэффициеты k и b прямой, на которой лежит отрезок.
 * Прямые описываются уравнением y = k * x + b. Эта функция
 * решает это уравнение, зная начало и конец отрезка. Единственный
 * случай, когда уравнение решить невозможно, это когда прямая вертикальна.
 *
 * Returns: TRUE, если коэффициенты удалось определить.
 */
gboolean
hyscan_gtk_waterfall_tools_line_k_b_calc (HyScanCoordinates *start,
                                          HyScanCoordinates *end,
                                          gdouble           *k,
                                          gdouble           *b)
{
   if (start->x == end->x)
    return FALSE;

   *k = (end->y - start->y) / (end->x - start->x);
   *b = start->y - *k * start->x;

   return TRUE;
}

/**
 * hyscan_gtk_waterfall_tools_point_in_square:
 * @point: координаты точки
 * @square_start: начальная координата прямоугольника
 * @square_end: конечная координата прямоугольника
 *
 * Функция определяет, лежит ли точка внутри прямоугольника.
 *
 * Returns: TRUE, если лежит внутри прямоугольника.
 */
gboolean
hyscan_gtk_waterfall_tools_point_in_square (HyScanCoordinates *point,
                                            HyScanCoordinates *square_start,
                                            HyScanCoordinates *square_end)
{
  HyScanCoordinates nstart, nend;
  gboolean inside;

  nstart.x = MIN (square_start->x, square_end->x);
  nstart.y = MIN (square_start->y, square_end->y);
  nend.x   = MAX (square_start->x, square_end->x);
  nend.y   = MAX (square_start->y, square_end->y);

  inside = (point->x <= nend.x && point->x >= nstart.x && point->y <= nend.y && point->y >= nstart.y);
  return inside;
}

/**
 * hyscan_gtk_waterfall_tools_make_handle_pattern:
 * @radius: радиус
 * @inner: внутренний цвет
 * @outer: внешний цвет
 *
 * Функция создает паттерн для хэндлов.
 *
 * Returns: (transfer full): паттерн.
 */
cairo_pattern_t *
hyscan_gtk_waterfall_tools_make_handle_pattern (gdouble radius,
                                                GdkRGBA inner,
                                                GdkRGBA outer)
{
  cairo_pattern_t *pattern;

  gdouble r0  = 0.0 * radius;
  gdouble r1  = 1.0 * radius;
  gdouble st0 = 0.60;
  gdouble st1 = 0.80;
  gdouble st2 = 1.00;

  pattern = cairo_pattern_create_radial (0, 0, r0, 0, 0, r1);

  cairo_pattern_add_color_stop_rgba (pattern, st0, inner.red, inner.green, inner.blue, inner.alpha);
  cairo_pattern_add_color_stop_rgba (pattern, st1, outer.red, outer.green, outer.blue, outer.alpha);
  cairo_pattern_add_color_stop_rgba (pattern, st2, outer.red, outer.green, outer.blue, 0.0);

  return pattern;
}

/**
 * hyscan_cairo_set_source_gdk_rgba:
 * Вспомогательная функция установки цвета.
 *
 * @cr: контекст cairo
 * @rgba: цвет, который требуется установить
 */
void
hyscan_cairo_set_source_gdk_rgba (cairo_t *cr,
                                  GdkRGBA *rgba)
{
  cairo_set_source_rgba (cr, rgba->red, rgba->green, rgba->blue, rgba->alpha);
}

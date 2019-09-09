/* hyscan-cairo.c
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

/**
 * SECTION: hyscan-cairo
 * @Short_description: Вспомогательные функции для рисования с помощью cairo.
 * @Title: HyScanCairo
 *
 */


#include "hyscan-cairo.h"

/*
 * Clipping routines for line.
 *
 * Clipping based heavily on code from http://www.ncsa.uiuc.edu/Vis/Graphics/src/clipCohSuth.c
 *
 */
#define CLIP_PADDING           10
#define CLIP_LEFT_EDGE         0x1
#define CLIP_RIGHT_EDGE        0x2
#define CLIP_BOTTOM_EDGE       0x4
#define CLIP_TOP_EDGE          0x8
#define CLIP_INSIDE(a)         (!a)
#define CLIP_REJECT(a,b)       (a&b)
#define CLIP_ACCEPT(a,b)       (!(a|b))

/*
 * Internal clip-encoding routine.
 * Calculates a segement-based clipping encoding for a point against a rectangle.
 *
 * Params:
 *  - X coordinate of point;
 *  - Y coordinate of point;
 *  - X coordinate of left edge of the rectangle;
 *  - Y coordinate of top edge of the rectangle;
 *  - X coordinate of right edge of the rectangle;
 *  - Y coordinate of bottom edge of the rectangle.
 *
 */
static gint
_clipEncode (gint64 x,
             gint64 y,
             gint32 left,
             gint32 top,
             gint32 right,
             gint32 bottom)
{
  gint code = 0;

  if (x < left)
    {
      code |= CLIP_LEFT_EDGE;
    }
  else if (x > right)
    {
      code |= CLIP_RIGHT_EDGE;
    }

  if (y < top)
    {
      code |= CLIP_TOP_EDGE;
    }
  else if (y > bottom)
    {
      code |= CLIP_BOTTOM_EDGE;
    }

  return code;
}

/*
 * Clip line to a the clipping rectangle of a surface.
 *
 * Params:
 *  - Target surface to draw on;
 *  - Pointer to X coordinate of first point of line;
 *  - Pointer to Y coordinate of first point of line;
 *  - Pointer to X coordinate of second point of line;
 *  - Pointer to Y coordinate of second point of line.
 *
 */
static gboolean
_clipLine (gint32   width,
           gint32   height,
           gint32  *x1,
           gint32  *y1,
           gint32  *x2,
           gint32  *y2)
{
  gint code1, code2;
  gint32 left, right, top, bottom;
  gint64 x1_64, x2_64, y1_64, y2_64;
  gint64 swaptmp;
  gdouble m;
  gdouble v;

  gboolean draw = FALSE;

  /*
   * Get clipping boundary
   */
  left = -CLIP_PADDING;
  right = width + CLIP_PADDING;
  top = -CLIP_PADDING;
  bottom = height + CLIP_PADDING;

  x1_64 = *x1;
  x2_64 = *x2;
  y1_64 = *y1;
  y2_64 = *y2;

  while (1)
    {
      code1 = _clipEncode (x1_64, y1_64, left, top, right, bottom);
      code2 = _clipEncode (x2_64, y2_64, left, top, right, bottom);
      if (CLIP_ACCEPT(code1, code2))
        {
          draw = TRUE;
          break;
        }
      else if (CLIP_REJECT(code1, code2))
        {
          break;
        }
      else
        {
          if (CLIP_INSIDE(code1))
            {
              swaptmp = x2_64;
              x2_64 = x1_64;
              x1_64 = swaptmp;

              swaptmp = y2_64;
              y2_64 = y1_64;
              y1_64 = swaptmp;

              swaptmp = code2;
              code2 = code1;
              code1 = swaptmp;
            }

          if (x2_64 != x1_64)
            {
              m = (gdouble) (y2_64 - y1_64) / (gdouble) (x2_64 - x1_64);
            }
          else
            {
              m = 1.0;
            }

          if (code1 & CLIP_LEFT_EDGE)
            {
              v = ((left - x1_64) * m);
              y1_64 += (gint32) CLAMP (v, G_MININT32, G_MAXINT32);
              x1_64 = left;
            }
          else if (code1 & CLIP_RIGHT_EDGE)
            {
              v = ((right - x1_64) * m);
              y1_64 += (gint32) CLAMP (v, G_MININT32, G_MAXINT32);
              x1_64 = right;
            }
          else if (code1 & CLIP_BOTTOM_EDGE)
            {
              if (x2_64 != x1_64)
                {
                  v = ((bottom - y1_64) / m);
                  x1_64 += (gint32) CLAMP (v, G_MININT32, G_MAXINT32);
                }
              y1_64 = bottom;
            }
          else if (code1 & CLIP_TOP_EDGE)
            {
              if (x2_64 != x1_64)
                {
                  v = ((top - y1_64) / m);
                  x1_64 += (gint32) CLAMP (v, G_MININT32, G_MAXINT32);
                }
              y1_64 = top;
            }
        }
    }

  *x1 = CLAMP (x1_64, G_MININT32, G_MAXINT32);
  *x2 = CLAMP (x2_64, G_MININT32, G_MAXINT32);
  *y1 = CLAMP (y1_64, G_MININT32, G_MAXINT32);
  *y2 = CLAMP (y2_64, G_MININT32, G_MAXINT32);

  return draw;
}

/**
 * hyscan_cairo_line_to:
 * @cairo: контекст cairo
 * @x1: координата X начала отрезка
 * @y1: координата Y начала отрезка
 * @x2: координата X конца отрезка
 * @y2: координата Y конца отрезка
 *
 * Функция добавляет в текущий путь часть отрезка с конца в точках (x1, y1)
 * и (x2, y2). Добавленный отрезок будет обрезан до размеров поверхности для рисования.
 *
 * Использование функции решает проблему рисования линий по координатам с
 * большими числовыми значениями.
 */
void
hyscan_cairo_line_to (cairo_t *cairo,
                      gdouble  x1,
                      gdouble  y1,
                      gdouble  x2,
                      gdouble  y2)
{
  cairo_surface_t *surface;
  gint32 x1_int, y1_int, x2_int, y2_int;

  gint width, height;

  surface = cairo_get_target (cairo);

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

  x1_int = x1;
  y1_int = y1;
  x2_int = x2;
  y2_int = y2;

  if (!_clipLine (width, height, &x1_int, &y1_int, &x2_int, &y2_int))
    return;

  cairo_move_to (cairo, x1_int, y1_int);
  cairo_line_to (cairo, x2_int, y2_int);
}

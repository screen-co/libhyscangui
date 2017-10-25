#include "hyscan-gtk-waterfall-tools.h"
#include <math.h>


/* Функция вычисляет расстояние между точками. */
gdouble
hyscan_gtk_waterfall_tools_radius (HyScanCoordinates *start,
                                   HyScanCoordinates *end)
{
  return sqrt (pow (ABS(end->x - start->x), 2) + pow (ABS(end->y - start->y), 2));
}

gdouble
hyscan_gtk_waterfall_tools_angle (HyScanCoordinates *start,
                                  HyScanCoordinates *end)
{
  if (start->x == end->x)
    return 0;

  return -atan ((end->y - start->y) / (end->x - start->x));
}

HyScanCoordinates
hyscan_gtk_waterfall_tools_middle (HyScanCoordinates *start,
                                   HyScanCoordinates *end)
{
  HyScanCoordinates ret;

  ret.x = start->x + (end->x - start->x) / 2.0;
  ret.y = start->y + (end->y - start->y) / 2.0;

  return ret;
}


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

gboolean
hyscan_gtk_waterfall_tools_point_in_square_w (HyScanCoordinates *point,
                                              HyScanCoordinates *square_start,
                                              gdouble                        dx,
                                              gdouble                        dy)
{
  g_message ("not implemented, lol");
  return FALSE;
}

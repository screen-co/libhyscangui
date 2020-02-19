/* hyscan-gtk-map-track-draw-fill.c
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
 * SECTION: hyscan-gtk-map-track-draw-fill
 * @Short_description: рисование галса в виде заливки зоны покрытия
 * @Title: HyScanGtkMapTrackDrawFill
 * @See_also: #HyScanGtkMapTrack
 *
 * Класс для рисования галса на карте в виде заливки зоны покрытия.
 *
 */

#include <hyscan-cartesian.h>
#include "hyscan-gtk-map-track-draw-fill.h"

struct _HyScanGtkMapTrackDrawFillPrivate
{
};

static void    hyscan_gtk_map_track_draw_fill_interface_init           (HyScanGtkMapTrackDrawInterface *iface);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackDrawFill, hyscan_gtk_map_track_draw_fill, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackDrawFill)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, hyscan_gtk_map_track_draw_fill_interface_init))

static void
hyscan_gtk_map_track_draw_fill_class_init (HyScanGtkMapTrackDrawFillClass *klass)
{
}

static void
hyscan_gtk_map_track_draw_fill_init (HyScanGtkMapTrackDrawFill *gtk_map_track_draw_fill)
{
  gtk_map_track_draw_fill->priv = hyscan_gtk_map_track_draw_fill_get_instance_private (gtk_map_track_draw_fill);
}

/* Рисует часть галса по точкам points. */
static void
hyscan_gtk_map_track_draw_chunk (HyScanGeoCartesian2D *from,
                                 HyScanGeoCartesian2D *to,
                                 gdouble               scale,
                                 GList                *chunk_start,
                                 GList                *chunk_end,
                                 cairo_t              *cairo)
{
  GList *point_l;
  HyScanGtkMapTrackPoint *point;
  gdouble x, y;
  gboolean r1_set = FALSE, l1_set = FALSE;

  struct
  {
    gdouble x, y;
  } rc, re, rc1, re1, lc, le, lc1, le1;

  /* Рисуем полосы от бортов. */
  cairo_save (cairo);
  cairo_set_operator (cairo, CAIRO_OPERATOR_SOURCE);
  cairo_set_dash (cairo, NULL, 0, 0);
  cairo_set_line_width (cairo, 1.0);


  chunk_start = chunk_start->prev != NULL ? chunk_start->prev : chunk_start;
  chunk_end = (chunk_end != NULL && chunk_end->next != NULL) ? chunk_end->next : chunk_end;

  for (point_l = chunk_start; point_l != chunk_end; point_l = point_l->next)
    {
      point = point_l->data;

      /* Правый борт. */
      if (point->r_dist > 0)
        {
          /* Координаты точки на поверхности cairo. */
          rc.x = (point->r_start_c2d.x - from->x) / scale;
          rc.y = (from->y - to->y) / scale - (point->r_start_c2d.y - to->y) / scale;
          re.x = (point->r_end_c2d.x - from->x) / scale;
          re.y = (from->y - to->y) / scale - (point->r_end_c2d.y - to->y) / scale;

          if (r1_set)
            {
              cairo_set_source_rgba (cairo, 0, 1.0, 0, 0.2);
              cairo_move_to (cairo, rc.x, rc.y);
              cairo_line_to (cairo, re.x, re.y);
              cairo_line_to (cairo, re1.x, re1.y);
              cairo_line_to (cairo, rc1.x, rc1.y);
              cairo_close_path (cairo);
              cairo_fill_preserve (cairo);
              cairo_stroke (cairo);
            }

          re1 = re;
          rc1 = rc;
          r1_set = TRUE;
        }

      /* Левый борт. */
      if (point->l_dist > 0)
        {
          /* Координаты точки на поверхности cairo. */
          lc.x = (point->l_start_c2d.x - from->x) / scale;
          lc.y = (from->y - to->y) / scale - (point->l_start_c2d.y - to->y) / scale;
          le.x = (point->l_end_c2d.x - from->x) / scale;
          le.y = (from->y - to->y) / scale - (point->l_end_c2d.y - to->y) / scale;

          if (l1_set)
            {
              cairo_set_source_rgba (cairo, 1.0, 0, 0, 0.2);
              cairo_move_to (cairo, lc.x, lc.y);
              cairo_line_to (cairo, le.x, le.y);
              cairo_line_to (cairo, le1.x, le1.y);
              cairo_line_to (cairo, lc1.x, lc1.y);
              cairo_close_path (cairo);
              cairo_fill_preserve (cairo);
              cairo_stroke (cairo);
            }

          le1 = le;
          lc1 = lc;
          l1_set = TRUE;
        }
    }

  cairo_restore (cairo);

  /* Рисуем линию движения. */
  cairo_set_source_rgba (cairo, 1.0, 1.0, 0.4, 1.0);
  cairo_set_line_width (cairo, 1.0);
  cairo_new_path (cairo);
  for (point_l = chunk_start; point_l != chunk_end; point_l = point_l->next)
    {
      point = point_l->data;

      /* Координаты точки на поверхности cairo. */
      x = (point->center_c2d.x - from->x) / scale;
      y = (from->y - to->y) / scale - (point->center_c2d.y - to->y) / scale;

      cairo_line_to (cairo, x, y);
    }
  cairo_stroke (cairo);
}

static void
hyscan_gtk_map_track_draw_fill_draw_region (HyScanGtkMapTrackDraw *track_draw,
                                            GList                 *points,
                                            cairo_t               *cairo,
                                            gdouble                scale,
                                            HyScanGeoCartesian2D  *from,
                                            HyScanGeoCartesian2D  *to)
{
  (void) track_draw;

  GList *point_l, *prev_point_l = NULL, *next_point_l = NULL;
  GList *chunk_start = NULL, *chunk_end = NULL;

  /* Для каждого узла определяем, попадает ли он в указанную область. */
  prev_point_l = point_l = points;
  while (point_l != NULL)
    {
      HyScanGtkMapTrackPoint *next_track_point;
      HyScanGeoCartesian2D *prev_point, *cur_point, *next_point = NULL;
      gboolean is_inside = FALSE;

      next_point_l = point_l->next;
      next_track_point = next_point_l != NULL ? next_point_l->data : NULL;

      cur_point  = &((HyScanGtkMapTrackPoint *) point_l->data)->center_c2d;
      prev_point = &((HyScanGtkMapTrackPoint *) prev_point_l->data)->center_c2d;

      /* 3. Полосы от бортов в указанной области. */
      if (!is_inside)
        {
          HyScanGtkMapTrackPoint *cur_track_point = point_l->data;

          is_inside = hyscan_cartesian_is_inside (&cur_track_point->l_start_c2d, &cur_track_point->l_end_c2d, from, to) ||
                      hyscan_cartesian_is_inside (&cur_track_point->l_start_c2d, &cur_track_point->center_c2d, from, to) ||
                      hyscan_cartesian_is_inside (&cur_track_point->r_start_c2d, &cur_track_point->center_c2d, from, to) ||
                      hyscan_cartesian_is_inside (&cur_track_point->r_start_c2d, &cur_track_point->r_end_c2d, from, to);
        }

      /* 4. Область (from - to) меджу двумя полосами. */
      if (!is_inside && next_track_point != NULL)
        {
          HyScanGeoCartesian2D vertices[4];
          HyScanGtkMapTrackPoint *cur_track_point = point_l->data;

          vertices[0] = cur_track_point->r_start_c2d;
          vertices[1] = next_track_point->r_start_c2d;
          vertices[2] = next_track_point->r_end_c2d;
          vertices[3] = cur_track_point->r_end_c2d;
          is_inside = hyscan_cartesian_is_inside_polygon (vertices, 4, from);

          if (!is_inside)
            {
              vertices[0] = cur_track_point->l_start_c2d;
              vertices[1] = next_track_point->l_start_c2d;
              vertices[2] = next_track_point->l_end_c2d;
              vertices[3] = cur_track_point->l_end_c2d;
              is_inside = hyscan_cartesian_is_inside_polygon (vertices, 4, from);
            }
        }

      /* 1. Отрезок до предыдущего узла в указанной области. */
      if (!is_inside)
        {
          is_inside = hyscan_cartesian_is_inside (prev_point, cur_point, from, to);
        }

      /* 2. Отрезок до следующего узла в указанной области. */
      if (!is_inside && next_point_l != NULL)
        {
          next_point = &((HyScanGtkMapTrackPoint *) next_point_l->data)->center_c2d;
          is_inside = hyscan_cartesian_is_inside (next_point, cur_point, from, to);
        }

      if (is_inside)
        {
          chunk_start = (chunk_start == NULL) ? prev_point_l : chunk_start;
          chunk_end = next_point_l;
        }
      else if (chunk_start != NULL)
        {
          hyscan_gtk_map_track_draw_chunk (from, to, scale, chunk_start, chunk_end, cairo);
          chunk_start = chunk_end = NULL;
        }

      prev_point_l = point_l;
      point_l = point_l->next;
    }

  if (chunk_start != NULL)
    hyscan_gtk_map_track_draw_chunk (from, to, scale, chunk_start, chunk_end, cairo);
}


static void
hyscan_gtk_map_track_draw_fill_interface_init (HyScanGtkMapTrackDrawInterface *iface)
{
  iface->draw_region = hyscan_gtk_map_track_draw_fill_draw_region;
}


HyScanGtkMapTrackDraw *
hyscan_gtk_map_track_draw_fill_new (void)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_DRAW_FILL, NULL);
}

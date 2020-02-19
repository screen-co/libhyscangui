/* hyscan-gtk-map-track-draw.h
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

#ifndef __HYSCAN_GTK_MAP_TRACK_DRAW_H__
#define __HYSCAN_GTK_MAP_TRACK_DRAW_H__

#include <hyscan-geo.h>
#include <cairo.h>
#include <hyscan-param.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK_DRAW            (hyscan_gtk_map_track_draw_get_type ())
#define HYSCAN_GTK_MAP_TRACK_DRAW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDraw))
#define HYSCAN_IS_GTK_MAP_TRACK_DRAW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW))
#define HYSCAN_GTK_MAP_TRACK_DRAW_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDrawInterface))

typedef struct _HyScanGtkMapTrackDraw HyScanGtkMapTrackDraw;
typedef struct _HyScanGtkMapTrackDrawInterface HyScanGtkMapTrackDrawInterface;

typedef struct
{
  HyScanGeoGeodetic               geo;              /* Географические координаты точки и курс движения. */
  guint32                         index;            /* Индекс записи в канале данных. */
  gint64                          time;             /* Время фиксации. */
  gboolean                        wait_amp;         /* Признак, что ожидаются данные по амплитуде. */
  gdouble                         l_width;          /* Дальность по левому борту, метры. */
  gdouble                         r_width;          /* Дальность по правому борту, метры. */

  /* Координаты на картографической проекции (зависят от текущей проекции). */
  gdouble                         angle;            /* Курс судна, рад (с поправкой на смещение приёмника). */
  gdouble                         l_angle;          /* Направление антенны левого борта по курсу. */
  gdouble                         r_angle;          /* Направление антенны правого борта по курсу. */
  HyScanGeoCartesian2D            center_c2d;       /* Координаты центра судна на проекции. */
  HyScanGeoCartesian2D            l_start_c2d;      /* Координаты начала дальности по левому борту. */
  HyScanGeoCartesian2D            l_end_c2d;        /* Координаты конца дальности по левому борту. */
  HyScanGeoCartesian2D            r_start_c2d;      /* Координаты начала дальности по правому борту. */
  HyScanGeoCartesian2D            r_end_c2d;        /* Координаты конца дальности по правому борту. */
  gdouble                         dist;             /* Расстояние от начала галса. */
  gdouble                         l_dist;           /* Дальность по левому борту, единицы проекции. */
  gdouble                         r_dist;           /* Дальность по правому борту, единицы проекции. */
  gdouble                         scale;            /* Масштаб перевода из метров в логические координаты. */
  gboolean                        straight;         /* Признак того, что точка находится на прямолинейном участке. */
} HyScanGtkMapTrackPoint;

struct _HyScanGtkMapTrackDrawInterface
{
  GTypeInterface       g_iface;

  HyScanParam *        (*get_param)                  (HyScanGtkMapTrackDraw *track_draw);

  void                 (*draw_region)                (HyScanGtkMapTrackDraw *track_draw,
                                                      GList                 *points,
                                                      cairo_t               *cairo,
                                                      gdouble                scale,
                                                      HyScanGeoCartesian2D  *from,
                                                      HyScanGeoCartesian2D  *to);
};

HYSCAN_API
GType                  hyscan_gtk_map_track_draw_get_type                  (void);

HYSCAN_API
void                   hyscan_gtk_map_track_draw_region                    (HyScanGtkMapTrackDraw      *track_draw,
                                                                            GList                      *points,
                                                                            cairo_t                    *cairo,
                                                                            gdouble                     scale,
                                                                            HyScanGeoCartesian2D       *from,
                                                                            HyScanGeoCartesian2D       *to);

HYSCAN_API
HyScanParam *          hyscan_gtk_map_track_draw_get_param                 (HyScanGtkMapTrackDraw      *track_draw);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_DRAW_H__ */

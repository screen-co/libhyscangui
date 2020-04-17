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

#include <cairo.h>
#include <hyscan-param.h>
#include <hyscan-map-track.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK_DRAW            (hyscan_gtk_map_track_draw_get_type ())
#define HYSCAN_GTK_MAP_TRACK_DRAW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDraw))
#define HYSCAN_IS_GTK_MAP_TRACK_DRAW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW))
#define HYSCAN_GTK_MAP_TRACK_DRAW_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_DRAW, HyScanGtkMapTrackDrawInterface))

typedef struct _HyScanGtkMapTrackDraw HyScanGtkMapTrackDraw;
typedef struct _HyScanGtkMapTrackDrawInterface HyScanGtkMapTrackDrawInterface;

/**
 * HyScanGtkMapTrackDrawInterface:
 * @g_iface: базовый интерфейс
 * @get_param: функция считывает параметры объекта отрисовки
 * @draw_region: функция рисует галс на указанной области поверхности cairo.
 */
struct _HyScanGtkMapTrackDrawInterface
{
  GTypeInterface       g_iface;

  HyScanParam *        (*get_param)                  (HyScanGtkMapTrackDraw      *track_draw);

  void                 (*draw_region)                (HyScanGtkMapTrackDraw      *track_draw,
                                                      HyScanMapTrackData        *data,
                                                      cairo_t                    *cairo,
                                                      gdouble                     scale,
                                                      HyScanGeoCartesian2D       *from,
                                                      HyScanGeoCartesian2D       *to,
                                                      GCancellable               *cancellable);
};

HYSCAN_API
GType                    hyscan_gtk_map_track_draw_get_type                  (void);

HYSCAN_API
void                     hyscan_gtk_map_track_draw_region                    (HyScanGtkMapTrackDraw        *track_draw,
                                                                              HyScanMapTrackData          *data,
                                                                              cairo_t                      *cairo,
                                                                              gdouble                       scale,
                                                                              HyScanGeoCartesian2D         *from,
                                                                              HyScanGeoCartesian2D         *to,
                                                                              GCancellable                 *cancellable);

HYSCAN_API
HyScanParam *            hyscan_gtk_map_track_draw_get_param                 (HyScanGtkMapTrackDraw        *track_draw);

void                     hyscan_gtk_map_track_draw_scale                     (HyScanGeoCartesian2D         *global,
                                                                              HyScanGeoCartesian2D         *from,
                                                                              HyScanGeoCartesian2D         *to,
                                                                              gdouble                       scale,
                                                                              HyScanGeoCartesian2D         *local);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_DRAW_H__ */

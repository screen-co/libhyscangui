/* hyscan-gtk-map-track.h
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

#ifndef __HYSCAN_GTK_MAP_TRACK_H__
#define __HYSCAN_GTK_MAP_TRACK_H__

#include <hyscan-param.h>
#include <hyscan-gtk-map-tiled-layer.h>
#include <hyscan-geo-projection.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK             (hyscan_gtk_map_track_get_type ())
#define HYSCAN_GTK_MAP_TRACK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK, HyScanGtkMapTrack))
#define HYSCAN_IS_GTK_MAP_TRACK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK))
#define HYSCAN_GTK_MAP_TRACK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TRACK, HyScanGtkMapTrackClass))
#define HYSCAN_IS_GTK_MAP_TRACK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TRACK))
#define HYSCAN_GTK_MAP_TRACK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TRACK, HyScanGtkMapTrackClass))

typedef struct _HyScanGtkMapTrack HyScanGtkMapTrack;
typedef struct _HyScanGtkMapTrackPrivate HyScanGtkMapTrackPrivate;
typedef struct _HyScanGtkMapTrackClass HyScanGtkMapTrackClass;

/**
 * HyScanGtkMapTrackStyle:
 *
 * @color_left: Цвет левого борта.
 * @color_right: Цвет правого борта.
 * @color_track: Цвет линии движения.
 * @color_stroke: Цвет обводки.
 * @color_shadow: Цвет затенения (рекомендуется полупрозрачный чёрный).
 * @bar_width: Толщина линии дальности.
 * @bar_margin: Расстояние между соседними линиями дальности.
 * @line_width: Толщина линии движения.
 * @stroke_width: Толщина линии обводки.
 *
 */
typedef struct {
  GdkRGBA color_left;
  GdkRGBA color_right;
  GdkRGBA color_track;
  GdkRGBA color_stroke;
  GdkRGBA color_shadow;
  gdouble bar_width;
  gdouble bar_margin;
  gdouble line_width;
  gdouble stroke_width;
} HyScanGtkMapTrackStyle;

struct _HyScanGtkMapTrack
{
  GObject                   parent_instance;
  HyScanGtkMapTrackPrivate *priv;
};

struct _HyScanGtkMapTrackClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_track_get_type         (void);

HYSCAN_API
HyScanGtkMapTrack *    hyscan_gtk_map_track_new              (HyScanDB               *db,
                                                              HyScanCache            *cache,
                                                              const gchar            *project_name,
                                                              const gchar            *track_name,
                                                              HyScanGtkMapTiledLayer *tiled_layer,
                                                              HyScanGeoProjection    *projection);
HYSCAN_API
void                   hyscan_gtk_map_track_draw             (HyScanGtkMapTrack      *track,
                                                              cairo_t                *cairo,
                                                              gdouble                 scale,
                                                              HyScanGeoCartesian2D   *from,
                                                              HyScanGeoCartesian2D   *to,
                                                              HyScanGtkMapTrackStyle *style);

HYSCAN_API
gboolean               hyscan_gtk_map_track_view             (HyScanGtkMapTrack      *track,
                                                              HyScanGeoCartesian2D   *from,
                                                              HyScanGeoCartesian2D   *to);

HYSCAN_API
gboolean               hyscan_gtk_map_track_update           (HyScanGtkMapTrack      *track);

HYSCAN_API
void                   hyscan_gtk_map_track_set_projection   (HyScanGtkMapTrack      *track,
                                                              HyScanGeoProjection    *projection);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_H__ */

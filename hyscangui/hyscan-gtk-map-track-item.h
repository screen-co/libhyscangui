/* hyscan-gtk-map-track-item.h
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

#ifndef __HYSCAN_GTK_MAP_TRACK_ITEM_H__
#define __HYSCAN_GTK_MAP_TRACK_ITEM_H__

#include <hyscan-param.h>
#include <hyscan-gtk-map-tiled.h>
#include <hyscan-geo-projection.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK_ITEM             (hyscan_gtk_map_track_item_get_type ())
#define HYSCAN_GTK_MAP_TRACK_ITEM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_ITEM, HyScanGtkMapTrackItem))
#define HYSCAN_IS_GTK_MAP_TRACK_ITEM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_ITEM))
#define HYSCAN_GTK_MAP_TRACK_ITEM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TRACK_ITEM, HyScanGtkMapTrackItemClass))
#define HYSCAN_IS_GTK_MAP_TRACK_ITEM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TRACK_ITEM))
#define HYSCAN_GTK_MAP_TRACK_ITEM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_ITEM, HyScanGtkMapTrackItemClass))

typedef struct _HyScanGtkMapTrackItem HyScanGtkMapTrackItem;
typedef struct _HyScanGtkMapTrackItemPrivate HyScanGtkMapTrackItemPrivate;
typedef struct _HyScanGtkMapTrackItemClass HyScanGtkMapTrackItemClass;


struct _HyScanGtkMapTrackItem
{
  GObject                       parent_instance;
  HyScanGtkMapTrackItemPrivate *priv;
};

struct _HyScanGtkMapTrackItemClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                   hyscan_gtk_map_track_item_get_type      (void);

HYSCAN_API
HyScanGtkMapTrackItem * hyscan_gtk_map_track_item_new           (HyScanDB                   *db,
                                                                 HyScanCache                *cache,
                                                                 const gchar                *project_name,
                                                                 const gchar                *track_name,
                                                                 HyScanGtkMapTiled          *tiled_layer,
                                                                 HyScanGeoProjection        *projection);

HYSCAN_API
gboolean                hyscan_gtk_map_track_item_has_nmea      (HyScanGtkMapTrackItem      *track);

HYSCAN_API
gboolean                hyscan_gtk_map_track_item_points_lock   (HyScanGtkMapTrackItem      *track,
                                                                 GList                     **points);
HYSCAN_API
void                    hyscan_gtk_map_track_item_points_unlock (HyScanGtkMapTrackItem      *track);

HYSCAN_API                                                      
gboolean               hyscan_gtk_map_track_item_view           (HyScanGtkMapTrackItem      *track,
                                                                 HyScanGeoCartesian2D       *from,
                                                                 HyScanGeoCartesian2D       *to);
                                                                
HYSCAN_API                                                      
gboolean               hyscan_gtk_map_track_item_update         (HyScanGtkMapTrackItem      *track);

HYSCAN_API
void                   hyscan_gtk_map_track_item_set_projection (HyScanGtkMapTrackItem      *track,
                                                                 HyScanGeoProjection        *projection);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_ITEM_H__ */

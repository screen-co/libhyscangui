/* hyscan-gtk-map-track-layer.h
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

#include <hyscan-gtk-map-tiled.h>
#include <hyscan-gtk-map-track-item.h>
#include <hyscan-db.h>
#include <hyscan-cancellable.h>

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

struct _HyScanGtkMapTrack
{
  HyScanGtkMapTiled parent_instance;

  HyScanGtkMapTrackPrivate *priv;
};

struct _HyScanGtkMapTrackClass
{
  HyScanGtkMapTiledClass parent_class;
};


HYSCAN_API
GType                     hyscan_gtk_map_track_get_type            (void);

HYSCAN_API
HyScanGtkLayer *          hyscan_gtk_map_track_new                 (HyScanDB           *db,
                                                                    HyScanCache        *cache);

HYSCAN_API
void                      hyscan_gtk_map_track_set_project         (HyScanGtkMapTrack  *track_layer,
                                                                    const gchar        *project);

HYSCAN_API
void                      hyscan_gtk_map_track_set_tracks          (HyScanGtkMapTrack  *track_layer,
                                                                    gchar             **tracks);

HYSCAN_API
gchar **                  hyscan_gtk_map_track_get_tracks          (HyScanGtkMapTrack  *track_layer);

HYSCAN_API
HyScanGtkMapTrackItem *   hyscan_gtk_map_track_lookup              (HyScanGtkMapTrack  *track_layer,
                                                                    const gchar        *track_name);

HYSCAN_API
void                      hyscan_gtk_map_track_view                (HyScanGtkMapTrack  *track_layer,
                                                                    const gchar        *track_name,
                                                                    gboolean            zoom_in,
                                                                    HyScanCancellable  *cancellable);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_H__ */

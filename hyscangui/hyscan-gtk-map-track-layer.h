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

#ifndef __HYSCAN_GTK_MAP_TRACK_LAYER_H__
#define __HYSCAN_GTK_MAP_TRACK_LAYER_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TRACK_LAYER             (hyscan_gtk_map_track_layer_get_type ())
#define HYSCAN_GTK_MAP_TRACK_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_LAYER, HyScanGtkMapTrackLayer))
#define HYSCAN_IS_GTK_MAP_TRACK_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_LAYER))
#define HYSCAN_GTK_MAP_TRACK_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TRACK_LAYER, HyScanGtkMapTrackLayerClass))
#define HYSCAN_IS_GTK_MAP_TRACK_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TRACK_LAYER))
#define HYSCAN_GTK_MAP_TRACK_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TRACK_LAYER, HyScanGtkMapTrackLayerClass))

typedef struct _HyScanGtkMapTrackLayer HyScanGtkMapTrackLayer;
typedef struct _HyScanGtkMapTrackLayerPrivate HyScanGtkMapTrackLayerPrivate;
typedef struct _HyScanGtkMapTrackLayerClass HyScanGtkMapTrackLayerClass;

struct _HyScanGtkMapTrackLayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapTrackLayerPrivate *priv;
};

struct _HyScanGtkMapTrackLayerClass
{
  GInitiallyUnownedClass parent_class;
};

enum
{
  HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_RMC,
  HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_NMEA_DPT,
  HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_PORT,
  HYSCAN_GTK_MAP_TRACK_LAYER_CHNL_STARBOARD,
};

HYSCAN_API
GType                  hyscan_gtk_map_track_layer_get_type            (void);

HYSCAN_API
HyScanGtkLayer *       hyscan_gtk_map_track_layer_new                 (HyScanDB               *db,
                                                                       const gchar            *project,
                                                                       HyScanCache            *cache);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_track_enable        (HyScanGtkMapTrackLayer *track_layer,
                                                                       const gchar            *track_name,
                                                                       gboolean                enable);

HYSCAN_API
guint                  hyscan_gtk_map_track_layer_track_get_channel   (HyScanGtkMapTrackLayer *track_layer,
                                                                       const gchar            *track_name,
                                                                       guint                   channel);

HYSCAN_API
guint                  hyscan_gtk_map_track_layer_track_max_channel   (HyScanGtkMapTrackLayer *track_layer,
                                                                       const gchar            *track_name,
                                                                       guint                   channel);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_track_set_channel   (HyScanGtkMapTrackLayer *track_layer,
                                                                       const gchar            *track_name,
                                                                       guint                   channel,
                                                                       guint                   channel_num);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_set_color_track     (HyScanGtkMapTrackLayer *track_layer,
                                                                       GdkRGBA                 color);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_set_color_port      (HyScanGtkMapTrackLayer *track_layer,
                                                                       GdkRGBA                 color);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_set_color_starboard (HyScanGtkMapTrackLayer *track_layer,
                                                                       GdkRGBA                 color);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_set_bar_width       (HyScanGtkMapTrackLayer *track_layer,
                                                                       gboolean                 bar_width);

HYSCAN_API
void                   hyscan_gtk_map_track_layer_set_bar_margin      (HyScanGtkMapTrackLayer *track_layer,
                                                                       gdouble                 bar_margin);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_LAYER_H__ */

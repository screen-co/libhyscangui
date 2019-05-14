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

enum
{
  HYSCAN_GTK_MAP_TRACK_CHNL_NMEA_RMC,
  HYSCAN_GTK_MAP_TRACK_CHNL_NMEA_DPT,
  HYSCAN_GTK_MAP_TRACK_CHNL_PORT,
  HYSCAN_GTK_MAP_TRACK_CHNL_STARBOARD,
};

typedef struct {
  GdkRGBA                    color_left;      /* Цвет левого борта. */
  GdkRGBA                    color_right;     /* Цвет правого борта. */
  GdkRGBA                    color_track;     /* Цвет линии движения. */
  GdkRGBA                    color_stroke;    /* Цвет обводки. */
  GdkRGBA                    color_shadow;    /* Цвет затенения (рекомендуется полупрозрачный чёрный). */
  gdouble                    bar_width;       /* Толщина линии ширины. */
  gdouble                    bar_margin;      /* Расстояние между соседними линиями ширины. */
  gdouble                    line_width;      /* Толщина линии движения. */
  gdouble                    stroke_width;    /* Толщина линии обводки. */
} HyScanGtkMapTrackStyle;

struct _HyScanGtkMapTrack
{
  GObject parent_instance;

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

HYSCAN_API
void                   hyscan_gtk_map_track_set_visible      (HyScanGtkMapTrack      *track,
                                                              gboolean                visible);

HYSCAN_API
gboolean               hyscan_gtk_map_track_get_visible      (HyScanGtkMapTrack      *track);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TRACK_H__ */

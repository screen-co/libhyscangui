#ifndef __HYSCAN_GEO_PROJECTION_H__
#define __HYSCAN_GEO_PROJECTION_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <hyscan-geo.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GEO_PROJECTION            (hyscan_geo_projection_get_type ())
#define HYSCAN_GEO_PROJECTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GEO_PROJECTION, HyScanGeoProjection))
#define HYSCAN_IS_GEO_PROJECTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GEO_PROJECTION))
#define HYSCAN_GEO_PROJECTION_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GEO_PROJECTION, HyScanGeoProjectionInterface))

typedef struct _HyScanGeoProjection HyScanGeoProjection;
typedef struct _HyScanGeoProjectionInterface HyScanGeoProjectionInterface;

struct _HyScanGeoProjectionInterface
{
  GTypeInterface       g_iface;

  void                 (*value_to_geo)         (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic      *coords,
                                                gdouble                 x,
                                                gdouble                 y);

  void                 (*geo_to_value)         (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic       coords,
                                                gdouble                *x,
                                                gdouble                *y);

  void                 (*get_limits)           (HyScanGeoProjection    *geo_projection,
                                                gdouble                *min_x,
                                                gdouble                *max_x,
                                                gdouble                *min_y,
                                                gdouble                *max_y);

  gdouble              (*get_scale)            (HyScanGeoProjection    *geo_projection,
                                                HyScanGeoGeodetic       coords);
};

HYSCAN_API
GType                  hyscan_geo_projection_get_type         (void);



HYSCAN_API
void                   hyscan_geo_projection_value_to_geo     (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic      *coords,
                                                               gdouble                 x,
                                                               gdouble                 y);

HYSCAN_API
void                   hyscan_geo_projection_geo_to_value     (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic       coords,
                                                               gdouble                *x,
                                                               gdouble                *y);

HYSCAN_API
void                   hyscan_geo_projection_get_limits       (HyScanGeoProjection    *geo_projection,
                                                               gdouble                *min_x,
                                                               gdouble                *max_x,
                                                               gdouble                *min_y,
                                                               gdouble                *max_y);

HYSCAN_API
gdouble                hyscan_geo_projection_get_scale        (HyScanGeoProjection    *geo_projection,
                                                               HyScanGeoGeodetic       coords);

G_END_DECLS

#endif /* __HYSCAN_GEO_PROJECTION_H__ */

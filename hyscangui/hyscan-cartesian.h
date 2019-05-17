#ifndef __HYSCAN_CARTESIAN_H__
#define __HYSCAN_CARTESIAN_H__

#include <hyscan-geo.h>

HYSCAN_API
gboolean        hyscan_cartesian_is_point_inside     (HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to);

HYSCAN_API
gboolean         hyscan_cartesian_is_inside          (HyScanGeoCartesian2D *segment_end,
                                                      HyScanGeoCartesian2D *segment_start,
                                                      HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to);

HYSCAN_API
gdouble          hyscan_cartesian_distance_to_line   (HyScanGeoCartesian2D *p1,
                                                      HyScanGeoCartesian2D *p2,
                                                      HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *nearest_point);

HYSCAN_API
gdouble          hyscan_cartesian_distance           (HyScanGeoCartesian2D *p1,
                                                      HyScanGeoCartesian2D *p2);

HYSCAN_API
void             hyscan_cartesian_rotate             (HyScanGeoCartesian2D *point,
                                                      HyScanGeoCartesian2D *center,
                                                      gdouble               angle,
                                                      HyScanGeoCartesian2D *rotated);

HYSCAN_API
void             hyscan_cartesian_rotate_area        (HyScanGeoCartesian2D *area_from,
                                                      HyScanGeoCartesian2D *area_to,
                                                      HyScanGeoCartesian2D *center,
                                                      gdouble               angle,
                                                      HyScanGeoCartesian2D *rotated_from,
                                                      HyScanGeoCartesian2D *rotated_to);

#endif /* __HYSCAN_CARTESIAN_H__ */

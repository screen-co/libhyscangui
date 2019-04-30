#ifndef __HYSCAN_CARTESIAN_H__
#define __HYSCAN_CARTESIAN_H__

#include <hyscan-geo.h>

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

#endif /* __HYSCAN_CARTESIAN_H__ */

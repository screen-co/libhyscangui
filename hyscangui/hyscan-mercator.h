#ifndef __HYSCAN_MERCATOR_H__
#define __HYSCAN_MERCATOR_H__

#include <glib-object.h>
#include <hyscan-geo.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MERCATOR             (hyscan_mercator_get_type ())
#define HYSCAN_MERCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MERCATOR, HyScanMercator))
#define HYSCAN_IS_MERCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MERCATOR))
#define HYSCAN_MERCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MERCATOR, HyScanMercatorClass))
#define HYSCAN_IS_MERCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MERCATOR))
#define HYSCAN_MERCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MERCATOR, HyScanMercatorClass))

typedef struct _HyScanMercator HyScanMercator;
typedef struct _HyScanMercatorPrivate HyScanMercatorPrivate;
typedef struct _HyScanMercatorClass HyScanMercatorClass;

struct _HyScanMercator
{
  GObject parent_instance;

  HyScanMercatorPrivate *priv;
};

struct _HyScanMercatorClass
{
  GObjectClass parent_class;
};

GType                  hyscan_mercator_get_type         (void);

HYSCAN_API
gdouble                hyscan_mercator_get_scale        (HyScanMercator    *mercator,
                                                         HyScanGeoGeodetic  coords);

HYSCAN_API
void                   hyscan_mercator_value_to_geo     (HyScanMercator    *mercator,
                                                         HyScanGeoGeodetic *coords,
                                                         gdouble            x,
                                                         gdouble            y);

HYSCAN_API
void                   hyscan_mercator_geo_to_value     (HyScanMercator    *mercator,
                                                         HyScanGeoGeodetic  coords,
                                                         gdouble           *x,
                                                         gdouble           *y);

HYSCAN_API
void                   hyscan_mercator_get_limits       (HyScanMercator    *mercator,
                                                         gdouble           *min_x,
                                                         gdouble           *max_x,
                                                         gdouble           *min_y,
                                                         gdouble           *max_y);

HYSCAN_API
HyScanMercator *       hyscan_mercator_new              (HyScanGeoEllipsoidParam p);


G_END_DECLS

#endif /* __HYSCAN_MERCATOR_H__ */

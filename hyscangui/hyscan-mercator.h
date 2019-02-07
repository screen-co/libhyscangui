#ifndef __HYSCAN_MERCATOR_H__
#define __HYSCAN_MERCATOR_H__

#include <hyscan-geo-projection.h>

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
HyScanGeoProjection *  hyscan_mercator_new              (HyScanGeoEllipsoidParam p,
                                                         gdouble                 min_lat,
                                                         gdouble                 max_lat);

G_END_DECLS

#endif /* __HYSCAN_MERCATOR_H__ */

#ifndef __HYSCAN_PSEUDO_MERCATOR_H__
#define __HYSCAN_PSEUDO_MERCATOR_H__

#include <hyscan-geo-projection.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PSEUDO_MERCATOR             (hyscan_pseudo_mercator_get_type ())
#define HYSCAN_PSEUDO_MERCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PSEUDO_MERCATOR, HyScanPseudoMercator))
#define HYSCAN_IS_PSEUDO_MERCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PSEUDO_MERCATOR))
#define HYSCAN_PSEUDO_MERCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PSEUDO_MERCATOR, HyScanPseudoMercatorClass))
#define HYSCAN_IS_PSEUDO_MERCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PSEUDO_MERCATOR))
#define HYSCAN_PSEUDO_MERCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PSEUDO_MERCATOR, HyScanPseudoMercatorClass))

typedef struct _HyScanPseudoMercator HyScanPseudoMercator;
typedef struct _HyScanPseudoMercatorPrivate HyScanPseudoMercatorPrivate;
typedef struct _HyScanPseudoMercatorClass HyScanPseudoMercatorClass;

struct _HyScanPseudoMercator
{
  GObject parent_instance;

  HyScanPseudoMercatorPrivate *priv;
};

struct _HyScanPseudoMercatorClass
{
  GObjectClass parent_class;
};

GType                  hyscan_pseudo_mercator_get_type         (void);

HYSCAN_API
HyScanGeoProjection *  hyscan_pseudo_mercator_new              (void);


G_END_DECLS

#endif /* __HYSCAN_PSEUDO_MERCATOR_H__ */

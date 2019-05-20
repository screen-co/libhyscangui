#ifndef __HYSCAN_GTK_MAP_WFMARK_LAYER_H__
#define __HYSCAN_GTK_MAP_WFMARK_LAYER_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-mark-loc-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER             (hyscan_gtk_map_wfmark_layer_get_type ())
#define HYSCAN_GTK_MAP_WFMARK_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER, HyScanGtkMapWfmarkLayer))
#define HYSCAN_IS_GTK_MAP_WFMARK_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER))
#define HYSCAN_GTK_MAP_WFMARK_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER, HyScanGtkMapWfmarkLayerClass))
#define HYSCAN_IS_GTK_MAP_WFMARK_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER))
#define HYSCAN_GTK_MAP_WFMARK_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_WFMARK_LAYER, HyScanGtkMapWfmarkLayerClass))

typedef struct _HyScanGtkMapWfmarkLayer HyScanGtkMapWfmarkLayer;
typedef struct _HyScanGtkMapWfmarkLayerPrivate HyScanGtkMapWfmarkLayerPrivate;
typedef struct _HyScanGtkMapWfmarkLayerClass HyScanGtkMapWfmarkLayerClass;

struct _HyScanGtkMapWfmarkLayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapWfmarkLayerPrivate *priv;
};

struct _HyScanGtkMapWfmarkLayerClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType            hyscan_gtk_map_wfmark_layer_get_type (void);

HYSCAN_API
HyScanGtkLayer * hyscan_gtk_map_wfmark_layer_new      (HyScanMarkLocModel *model);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_WFMARK_LAYER_H__ */

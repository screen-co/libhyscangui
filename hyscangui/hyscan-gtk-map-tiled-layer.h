#ifndef __HYSCAN_GTK_MAP_TILED_LAYER_H__
#define __HYSCAN_GTK_MAP_TILED_LAYER_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-gtk-map-tile.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILED_LAYER             (hyscan_gtk_map_tiled_layer_get_type ())
#define HYSCAN_GTK_MAP_TILED_LAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayer))
#define HYSCAN_IS_GTK_MAP_TILED_LAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER))
#define HYSCAN_GTK_MAP_TILED_LAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayerClass))
#define HYSCAN_IS_GTK_MAP_TILED_LAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILED_LAYER))
#define HYSCAN_GTK_MAP_TILED_LAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILED_LAYER, HyScanGtkMapTiledLayerClass))

typedef struct _HyScanGtkMapTiledLayer HyScanGtkMapTiledLayer;
typedef struct _HyScanGtkMapTiledLayerPrivate HyScanGtkMapTiledLayerPrivate;
typedef struct _HyScanGtkMapTiledLayerClass HyScanGtkMapTiledLayerClass;

struct _HyScanGtkMapTiledLayer
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapTiledLayerPrivate *priv;
};

struct _HyScanGtkMapTiledLayerClass
{
  GInitiallyUnownedClass parent_class;

  void                (*fill_tile)                     (HyScanGtkMapTiledLayer *tiled_layer,
                                                        HyScanGtkMapTile       *tile);
};

HYSCAN_API
GType                hyscan_gtk_map_tiled_layer_get_type            (void);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_draw                (HyScanGtkMapTiledLayer *tiled_layer,
                                                                     cairo_t                *cairo);


HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_set_area_mod        (HyScanGtkMapTiledLayer *tiled_layer,
                                                                     HyScanGeoCartesian2D   *point0,
                                                                     HyScanGeoCartesian2D   *point1);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_set_param_mod       (HyScanGtkMapTiledLayer *tiled_layer);

HYSCAN_API
gboolean             hyscan_gtk_map_tiled_layer_has_cache           (HyScanGtkMapTiledLayer *tiled_layer);

HYSCAN_API
void                 hyscan_gtk_map_tiled_layer_request_draw        (HyScanGtkMapTiledLayer *tiled_layer);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILED_LAYER_H__ */

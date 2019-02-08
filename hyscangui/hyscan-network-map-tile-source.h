#ifndef __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__
#define __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__

#include <hyscan-gtk-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE             (hyscan_network_map_tile_source_get_type ())
#define HYSCAN_NETWORK_MAP_TILE_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSource))
#define HYSCAN_IS_NETWORK_MAP_TILE_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE))
#define hyscan_network_map_tile_source_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSourceClass))
#define HYSCAN_IS_NETWORK_MAP_TILE_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE))
#define hyscan_network_map_tile_source_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_NETWORK_MAP_TILE_SOURCE, HyScanNetworkMapTileSourceClass))

typedef struct _HyScanNetworkMapTileSource HyScanNetworkMapTileSource;
typedef struct _HyScanNetworkMapTileSourcePrivate HyScanNetworkMapTileSourcePrivate;
typedef struct _HyScanNetworkMapTileSourceClass HyScanNetworkMapTileSourceClass;

struct _HyScanNetworkMapTileSource
{
  GObject parent_instance;

  HyScanNetworkMapTileSourcePrivate *priv;
};

struct _HyScanNetworkMapTileSourceClass
{
  GObjectClass parent_class;
};

GType                          hyscan_network_map_tile_source_get_type         (void);

HYSCAN_API
HyScanNetworkMapTileSource *   hyscan_network_map_tile_source_new              (const gchar *url_format);

G_END_DECLS

#endif /* __HYSCAN_NETWORK_MAP_TILE_SOURCE_H__ */

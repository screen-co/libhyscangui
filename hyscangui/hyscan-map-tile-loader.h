#ifndef __HYSCAN_MAP_TILE_LOADER_H__
#define __HYSCAN_MAP_TILE_LOADER_H__

#include <hyscan-gtk-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MAP_TILE_LOADER             (hyscan_map_tile_loader_get_type ())
#define HYSCAN_MAP_TILE_LOADER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MAP_TILE_LOADER, HyScanMapTileLoader))
#define HYSCAN_IS_MAP_TILE_LOADER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MAP_TILE_LOADER))
#define HYSCAN_MAP_TILE_LOADER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MAP_TILE_LOADER, HyScanMapTileLoaderClass))
#define HYSCAN_IS_MAP_TILE_LOADER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MAP_TILE_LOADER))
#define HYSCAN_MAP_TILE_LOADER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MAP_TILE_LOADER, HyScanMapTileLoaderClass))

typedef struct _HyScanMapTileLoader HyScanMapTileLoader;
typedef struct _HyScanMapTileLoaderPrivate HyScanMapTileLoaderPrivate;
typedef struct _HyScanMapTileLoaderClass HyScanMapTileLoaderClass;

struct _HyScanMapTileLoader
{
  GObject parent_instance;

  HyScanMapTileLoaderPrivate *priv;
};

struct _HyScanMapTileLoaderClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_map_tile_loader_get_type         (void);

HYSCAN_API
HyScanMapTileLoader *  hyscan_map_tile_loader_new              (void);

HYSCAN_API
GThread *              hyscan_map_tile_loader_start            (HyScanMapTileLoader    *loader,
                                                                HyScanGtkMapTileSource *source,
                                                                gdouble                 from_x,
                                                                gdouble                 to_x,
                                                                gdouble                 from_y,
                                                                gdouble                 to_y);

HYSCAN_API
void                   hyscan_map_tile_loader_stop             (HyScanMapTileLoader    *loader);

G_END_DECLS

#endif /* __HYSCAN_MAP_TILE_LOADER_H__ */

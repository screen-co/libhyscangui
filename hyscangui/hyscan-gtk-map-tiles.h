#ifndef __HYSCAN_GTK_MAP_TILES_H__
#define __HYSCAN_GTK_MAP_TILES_H__

#include <hyscan-api.h>
#include <hyscan-gtk-map-tile-source.h>
#include <hyscan-gtk-map.h>
#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILES             (hyscan_gtk_map_tiles_get_type ())
#define HYSCAN_GTK_MAP_TILES(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILES, HyScanGtkMapTiles))
#define HYSCAN_IS_GTK_MAP_TILES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILES))
#define HYSCAN_GTK_MAP_TILES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILES, HyScanGtkMapTilesClass))
#define HYSCAN_IS_GTK_MAP_TILES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILES))
#define HYSCAN_GTK_MAP_TILES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILES, HyScanGtkMapTilesClass))

typedef struct _HyScanGtkMapTiles HyScanGtkMapTiles;
typedef struct _HyScanGtkMapTilesPrivate HyScanGtkMapTilesPrivate;
typedef struct _HyScanGtkMapTilesClass HyScanGtkMapTilesClass;

struct _HyScanGtkMapTiles
{
  GObject parent_instance;

  HyScanGtkMapTilesPrivate *priv;
};

struct _HyScanGtkMapTilesClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType               hyscan_gtk_map_tiles_get_type (void);

HYSCAN_API
HyScanGtkMapTiles * hyscan_gtk_map_tiles_new      (HyScanCache             *cache,
                                                   HyScanGtkMapTileSource  *source);

HYSCAN_API
void                hyscan_gtk_map_tiles_set_zoom (HyScanGtkMapTiles       *layer,
                                                   guint                    zoom);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILES_H__ */

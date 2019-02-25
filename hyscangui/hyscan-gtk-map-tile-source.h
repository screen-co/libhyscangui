#ifndef __HYSCAN_GTK_MAP_TILE_SOURCE_H__
#define __HYSCAN_GTK_MAP_TILE_SOURCE_H__

#include <glib-object.h>
#include <cairo.h>
#include <hyscan-api.h>
#include <hyscan-gtk-map-tile.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILE_SOURCE            (hyscan_gtk_map_tile_source_get_type ())
#define HYSCAN_GTK_MAP_TILE_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, HyScanGtkMapTileSource))
#define HYSCAN_IS_GTK_MAP_TILE_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE))
#define HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, HyScanGtkMapTileSourceInterface))

typedef struct _HyScanGtkMapTileSource HyScanGtkMapTileSource;
typedef struct _HyScanGtkMapTileSourceInterface HyScanGtkMapTileSourceInterface;

struct _HyScanGtkMapTileSourceInterface
{
  GTypeInterface       g_iface;

  gboolean             (*fill_tile)               (HyScanGtkMapTileSource        *source,
                                                   HyScanGtkMapTile              *tile);

  void                 (*get_zoom_limits)         (HyScanGtkMapTileSource        *source,
                                                   guint                         *min_zoom,
                                                   guint                         *max_zoom);

  guint                (*get_tile_size)           (HyScanGtkMapTileSource        *source);
};

HYSCAN_API
GType      hyscan_gtk_map_tile_source_get_type                  (void);

HYSCAN_API
void       hyscan_gtk_map_tile_source_get_zoom_limits           (HyScanGtkMapTileSource *source,
                                                                 guint                  *min_zoom,
                                                                 guint                  *max_zoom);
HYSCAN_API
guint      hyscan_gtk_map_tile_source_get_tile_size             (HyScanGtkMapTileSource *source);

HYSCAN_API
gboolean   hyscan_gtk_map_tile_source_fill                      (HyScanGtkMapTileSource *source,
                                                                 HyScanGtkMapTile       *tile);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILE_SOURCE_H__ */

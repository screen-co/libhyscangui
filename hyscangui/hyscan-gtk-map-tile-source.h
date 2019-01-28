#ifndef __HYSCAN_GTK_MAP_TILE_SOURCE_H__
#define __HYSCAN_GTK_MAP_TILE_SOURCE_H__

#include <glib-object.h>
#include <cairo.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILE_SOURCE            (hyscan_gtk_map_tile_source_get_type ())
#define HYSCAN_GTK_MAP_TILE_SOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, HyScanGtkMapTileSource))
#define HYSCAN_IS_GTK_MAP_TILE_SOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE))
#define HYSCAN_GTK_MAP_TILE_SOURCE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_TILE_SOURCE, HyScanGtkMapTileSourceInterface))

typedef struct _HyScanGtkMapTile
{
  guint                        x;              /* Положение по x в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        y;              /* Положение по y в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        zoom;           /* Масштаб. */

  cairo_surface_t             *surface;        /* Поверхность cairo с изображением тайла. */
} HyScanGtkMapTile;

typedef struct _HyScanGtkMapTileSource HyScanGtkMapTileSource;
typedef struct _HyScanGtkMapTileSourceInterface HyScanGtkMapTileSourceInterface;

struct _HyScanGtkMapTileSourceInterface
{
  GTypeInterface       g_iface;

  gboolean             (*find_tile)             (HyScanGtkMapTileSource         *tsource,
                                                 HyScanGtkMapTile               *tile);
};

GType      hyscan_gtk_map_tile_source_get_type                  (void);

HYSCAN_API
gboolean   hyscan_gtk_map_tile_source_find                 (HyScanGtkMapTileSource         *tsource,
                                                            HyScanGtkMapTile               *tile);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILE_SOURCE_H__ */

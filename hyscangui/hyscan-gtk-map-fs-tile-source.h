#ifndef __HYSCAN_GTK_MAP_FS_TILE_SOURCE_H__
#define __HYSCAN_GTK_MAP_FS_TILE_SOURCE_H__

#include <hyscan-gtk-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE             (hyscan_gtk_map_fs_tile_source_get_type ())
#define HYSCAN_GTK_MAP_FS_TILE_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE, HyScanGtkMapFsTileSource))
#define HYSCAN_IS_GTK_MAP_FS_TILE_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE))
#define HYSCAN_GTK_MAP_FS_TILE_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE, HyScanGtkMapFsTileSourceClass))
#define HYSCAN_IS_GTK_MAP_FS_TILE_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE))
#define HYSCAN_GTK_MAP_FS_TILE_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_FS_TILE_SOURCE, HyScanGtkMapFsTileSourceClass))

typedef struct _HyScanGtkMapFsTileSource HyScanGtkMapFsTileSource;
typedef struct _HyScanGtkMapFsTileSourcePrivate HyScanGtkMapFsTileSourcePrivate;
typedef struct _HyScanGtkMapFsTileSourceClass HyScanGtkMapFsTileSourceClass;

struct _HyScanGtkMapFsTileSource
{
  GObject parent_instance;

  HyScanGtkMapFsTileSourcePrivate *priv;
};

struct _HyScanGtkMapFsTileSourceClass
{
  GObjectClass parent_class;
};

GType hyscan_gtk_map_fs_tile_source_get_type (void);

HYSCAN_API
HyScanGtkMapFsTileSource * hyscan_gtk_map_fs_tile_source_new (const gchar            *dir,
                                                              HyScanGtkMapTileSource *fb_source);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_FS_TILE_SOURCE_H__ */

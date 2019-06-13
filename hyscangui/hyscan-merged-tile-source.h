#ifndef __HYSCAN_MERGED_TILE_SOURCE_H__
#define __HYSCAN_MERGED_TILE_SOURCE_H__

#include <hyscan-gtk-map-tile-source.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MERGED_TILE_SOURCE             (hyscan_merged_tile_source_get_type ())
#define HYSCAN_MERGED_TILE_SOURCE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MERGED_TILE_SOURCE, HyScanMergedTileSource))
#define HYSCAN_IS_MERGED_TILE_SOURCE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MERGED_TILE_SOURCE))
#define HYSCAN_MERGED_TILE_SOURCE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MERGED_TILE_SOURCE, HyScanMergedTileSourceClass))
#define HYSCAN_IS_MERGED_TILE_SOURCE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MERGED_TILE_SOURCE))
#define HYSCAN_MERGED_TILE_SOURCE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MERGED_TILE_SOURCE, HyScanMergedTileSourceClass))

typedef struct _HyScanMergedTileSource HyScanMergedTileSource;
typedef struct _HyScanMergedTileSourcePrivate HyScanMergedTileSourcePrivate;
typedef struct _HyScanMergedTileSourceClass HyScanMergedTileSourceClass;

struct _HyScanMergedTileSource
{
  GObject parent_instance;

  HyScanMergedTileSourcePrivate *priv;
};

struct _HyScanMergedTileSourceClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                    hyscan_merged_tile_source_get_type           (void);

HYSCAN_API
HyScanMergedTileSource * hyscan_merged_tile_source_new                (void);

HYSCAN_API
gboolean                 hyscan_merged_tile_source_append             (HyScanMergedTileSource *merged,
                                                                       HyScanGtkMapTileSource *source);

G_END_DECLS

#endif /* __HYSCAN_MERGED_TILE_SOURCE_H__ */

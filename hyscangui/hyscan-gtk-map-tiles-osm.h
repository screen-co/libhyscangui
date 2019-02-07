#ifndef __HYSCAN_GTK_MAP_TILES_OSM_H__
#define __HYSCAN_GTK_MAP_TILES_OSM_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILES_OSM             (hyscan_gtk_map_tiles_osm_get_type ())
#define HYSCAN_GTK_MAP_TILES_OSM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILES_OSM, HyScanGtkMapTilesOsm))
#define HYSCAN_IS_GTK_MAP_TILES_OSM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILES_OSM))
#define HYSCAN_GTK_MAP_TILES_OSM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILES_OSM, HyScanGtkMapTilesOsmClass))
#define HYSCAN_IS_GTK_MAP_TILES_OSM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILES_OSM))
#define HYSCAN_GTK_MAP_TILES_OSM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILES_OSM, HyScanGtkMapTilesOsmClass))

typedef struct _HyScanGtkMapTilesOsm HyScanGtkMapTilesOsm;
typedef struct _HyScanGtkMapTilesOsmPrivate HyScanGtkMapTilesOsmPrivate;
typedef struct _HyScanGtkMapTilesOsmClass HyScanGtkMapTilesOsmClass;

/* !!! Change GObject to type of the base class. !!! */
struct _HyScanGtkMapTilesOsm
{
  GObject parent_instance;

  HyScanGtkMapTilesOsmPrivate *priv;
};

/* !!! Change GObjectClass to type of the base class. !!! */
struct _HyScanGtkMapTilesOsmClass
{
  GObjectClass parent_class;
};

GType                        hyscan_gtk_map_tiles_osm_get_type         (void);

HYSCAN_API
HyScanGtkMapTilesOsm *       hyscan_gtk_map_tiles_osm_new              (const gchar *host,
                                                                        const gchar *uri_format);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILES_OSM_H__ */

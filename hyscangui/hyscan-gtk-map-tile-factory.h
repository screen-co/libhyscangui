#ifndef __HYSCAN_GTK_MAP_FILE_FACTORY_H__
#define __HYSCAN_GTK_MAP_FILE_FACTORY_H__

#include <glib-object.h>
#include <cairo.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_FILE_FACTORY            (hyscan_gtk_map_file_factory_get_type ())
#define HYSCAN_GTK_MAP_FILE_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_FILE_FACTORY, HyScanGtkMapTileFactory))
#define HYSCAN_IS_GTK_MAP_FILE_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_FILE_FACTORY))
#define HYSCAN_GTK_MAP_FILE_FACTORY_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), HYSCAN_TYPE_GTK_MAP_FILE_FACTORY, HyScanGtkMapTileFactoryInterface))

typedef struct _HyScanGtkMapTile
{
  guint                        x;              /* Положение по x в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        y;              /* Положение по y в проекции Меркатора, от 0 до 2^zoom - 1. */
  guint                        zoom;           /* Масштаб. */

  cairo_surface_t             *surface;        /* Поверхность cairo с изображением тайла. */
} HyScanGtkMapTile;

typedef struct _HyScanGtkMapTileFactory HyScanGtkMapTileFactory;
typedef struct _HyScanGtkMapTileFactoryInterface HyScanGtkMapTileFactoryInterface;

struct _HyScanGtkMapTileFactoryInterface
{
  GTypeInterface       g_iface;

  gboolean             (*create_tile)             (HyScanGtkMapTileFactory        *factory,
                                                   HyScanGtkMapTile               *tile);
};

GType      hyscan_gtk_map_file_factory_get_type                  (void);

HYSCAN_API
gboolean   hyscan_gtk_map_file_factory_create                    (HyScanGtkMapTileFactory *factory,
                                                                  HyScanGtkMapTile *tile);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_FILE_FACTORY_H__ */

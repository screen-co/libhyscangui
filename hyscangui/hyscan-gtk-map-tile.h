#ifndef __HYSCAN_GTK_MAP_TILE_H__
#define __HYSCAN_GTK_MAP_TILE_H__

#include <glib-object.h>
#include <hyscan-api.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_TILE             (hyscan_gtk_map_tile_get_type ())
#define HYSCAN_GTK_MAP_TILE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTile))
#define HYSCAN_IS_GTK_MAP_TILE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_TILE))
#define HYSCAN_GTK_MAP_TILE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTileClass))
#define HYSCAN_IS_GTK_MAP_TILE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_TILE))
#define HYSCAN_GTK_MAP_TILE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_TILE, HyScanGtkMapTileClass))

typedef struct _HyScanGtkMapTile HyScanGtkMapTile;
typedef struct _HyScanGtkMapTilePrivate HyScanGtkMapTilePrivate;
typedef struct _HyScanGtkMapTileClass HyScanGtkMapTileClass;

struct _HyScanGtkMapTile
{
  GObject parent_instance;

  HyScanGtkMapTilePrivate *priv;
};

struct _HyScanGtkMapTileClass
{
  GObjectClass parent_class;
};

GType                  hyscan_gtk_map_tile_get_type         (void);

HYSCAN_API
HyScanGtkMapTile *     hyscan_gtk_map_tile_new              (guint              x,
                                                             guint              y,
                                                             guint              z,
                                                             guint              size);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_x            (HyScanGtkMapTile *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_y            (HyScanGtkMapTile *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_zoom         (HyScanGtkMapTile *tile);

HYSCAN_API
guint                  hyscan_gtk_map_tile_get_size         (HyScanGtkMapTile *tile);

HYSCAN_API
gboolean               hyscan_gtk_map_tile_set_pixbuf       (HyScanGtkMapTile *tile,
                                                             GdkPixbuf        *pixbuf);

HYSCAN_API
GdkPixbuf *            hyscan_gtk_map_tile_get_pixbuf       (HyScanGtkMapTile *tile);

HYSCAN_API
gint                   hyscan_gtk_map_tile_compare          (HyScanGtkMapTile *a,
                                                             HyScanGtkMapTile *b);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_TILE_H__ */

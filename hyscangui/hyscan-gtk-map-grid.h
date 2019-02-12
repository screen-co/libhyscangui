#ifndef __HYSCAN_GTK_MAP_GRID_H__
#define __HYSCAN_GTK_MAP_GRID_H__

#include <hyscan-gtk-map.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_GRID             (hyscan_gtk_map_grid_get_type ())
#define HYSCAN_GTK_MAP_GRID(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_GRID, HyScanGtkMapGrid))
#define HYSCAN_IS_GTK_MAP_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_GRID))
#define HYSCAN_GTK_MAP_GRID_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_GRID, HyScanGtkMapGridClass))
#define HYSCAN_IS_GTK_MAP_GRID_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_GRID))
#define HYSCAN_GTK_MAP_GRID_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_GRID, HyScanGtkMapGridClass))

typedef struct _HyScanGtkMapGrid HyScanGtkMapGrid;
typedef struct _HyScanGtkMapGridPrivate HyScanGtkMapGridPrivate;
typedef struct _HyScanGtkMapGridClass HyScanGtkMapGridClass;

struct _HyScanGtkMapGrid
{
  GObject parent_instance;

  HyScanGtkMapGridPrivate *priv;
};

struct _HyScanGtkMapGridClass
{
  GObjectClass parent_class;
};

HYSCAN_API
HyScanGtkMapGrid *     hyscan_gtk_map_grid_new   (HyScanGtkMap *map);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_GRID_H__ */

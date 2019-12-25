#ifndef __HYSCAN_GTK_MAP_DIRECTION_H__
#define __HYSCAN_GTK_MAP_DIRECTION_H__

#include <hyscan-planner-selection.h>
#include <hyscan-gtk-map.h>
#include <hyscan-nav-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_DIRECTION             (hyscan_gtk_map_direction_get_type ())
#define HYSCAN_GTK_MAP_DIRECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_DIRECTION, HyScanGtkMapDirection))
#define HYSCAN_IS_GTK_MAP_DIRECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_DIRECTION))
#define HYSCAN_GTK_MAP_DIRECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_DIRECTION, HyScanGtkMapDirectionClass))
#define HYSCAN_IS_GTK_MAP_DIRECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_DIRECTION))
#define HYSCAN_GTK_MAP_DIRECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_DIRECTION, HyScanGtkMapDirectionClass))

typedef struct _HyScanGtkMapDirection HyScanGtkMapDirection;
typedef struct _HyScanGtkMapDirectionPrivate HyScanGtkMapDirectionPrivate;
typedef struct _HyScanGtkMapDirectionClass HyScanGtkMapDirectionClass;

struct _HyScanGtkMapDirection
{
  GtkBox parent_instance;

  HyScanGtkMapDirectionPrivate *priv;
};

struct _HyScanGtkMapDirectionClass
{
  GtkBoxClass parent_class;
};

HYSCAN_API
GType        hyscan_gtk_map_direction_get_type         (void);

HYSCAN_API
GtkWidget *  hyscan_gtk_map_direction_new              (HyScanGtkMap           *map,
                                                        HyScanPlannerSelection *selection,
                                                        HyScanNavModel         *model);


G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_DIRECTION_H__ */

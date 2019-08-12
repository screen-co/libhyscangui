#ifndef __HYSCAN_GTK_MAP_PLANNER_H__
#define __HYSCAN_GTK_MAP_PLANNER_H__

#include <hyscan-gtk-layer.h>
#include <hyscan-mark-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_PLANNER             (hyscan_gtk_map_planner_get_type ())
#define HYSCAN_GTK_MAP_PLANNER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_PLANNER, HyScanGtkMapPlanner))
#define HYSCAN_IS_GTK_MAP_PLANNER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_PLANNER))
#define HYSCAN_GTK_MAP_PLANNER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_PLANNER, HyScanGtkMapPlannerClass))
#define HYSCAN_IS_GTK_MAP_PLANNER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_PLANNER))
#define HYSCAN_GTK_MAP_PLANNER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_PLANNER, HyScanGtkMapPlannerClass))

typedef struct _HyScanGtkMapPlanner HyScanGtkMapPlanner;
typedef struct _HyScanGtkMapPlannerPrivate HyScanGtkMapPlannerPrivate;
typedef struct _HyScanGtkMapPlannerClass HyScanGtkMapPlannerClass;

struct _HyScanGtkMapPlanner
{
  GInitiallyUnowned parent_instance;

  HyScanGtkMapPlannerPrivate *priv;
};

struct _HyScanGtkMapPlannerClass
{
  GInitiallyUnownedClass parent_class;
};

HYSCAN_API
GType            hyscan_gtk_map_planner_get_type (void);

HYSCAN_API
HyScanGtkLayer * hyscan_gtk_map_planner_new      (HyScanMarkModel *model);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_PLANNER_H__ */

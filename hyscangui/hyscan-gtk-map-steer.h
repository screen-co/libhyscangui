#ifndef __HYSCAN_GTK_MAP_STEER_H__
#define __HYSCAN_GTK_MAP_STEER_H__

#include <hyscan-nav-model.h>
#include <gtk/gtk.h>
#include <hyscan-planner-selection.h>
#include <gtk-cifro-area.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_MAP_STEER             (hyscan_gtk_map_steer_get_type ())
#define HYSCAN_GTK_MAP_STEER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_MAP_STEER, HyScanGtkMapSteer))
#define HYSCAN_IS_GTK_MAP_STEER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_MAP_STEER))
#define HYSCAN_GTK_MAP_STEER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_MAP_STEER, HyScanGtkMapSteerClass))
#define HYSCAN_IS_GTK_MAP_STEER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_MAP_STEER))
#define HYSCAN_GTK_MAP_STEER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_MAP_STEER, HyScanGtkMapSteerClass))

typedef struct _HyScanGtkMapSteer HyScanGtkMapSteer;
typedef struct _HyScanGtkMapSteerPrivate HyScanGtkMapSteerPrivate;
typedef struct _HyScanGtkMapSteerClass HyScanGtkMapSteerClass;

struct _HyScanGtkMapSteer
{
  GtkCifroArea parent_instance;

  HyScanGtkMapSteerPrivate *priv;
};

struct _HyScanGtkMapSteerClass
{
  GtkCifroAreaClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_map_steer_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_map_steer_new              (HyScanNavModel           *model,
                                                              HyScanPlannerSelection   *selection);

G_END_DECLS

#endif /* __HYSCAN_GTK_MAP_STEER_H__ */

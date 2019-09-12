#ifndef __HYSCAN_GTK_PLANNER_STATUS_H__
#define __HYSCAN_GTK_PLANNER_STATUS_H__

#include <hyscan-api.h>
#include <gtk/gtk.h>
#include <hyscan-planner-model.h>
#include "hyscan-gtk-map-planner.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PLANNER_STATUS             (hyscan_gtk_planner_status_get_type ())
#define HYSCAN_GTK_PLANNER_STATUS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PLANNER_STATUS, HyScanGtkPlannerStatus))
#define HYSCAN_IS_GTK_PLANNER_STATUS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PLANNER_STATUS))
#define HYSCAN_GTK_PLANNER_STATUS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PLANNER_STATUS, HyScanGtkPlannerStatusClass))
#define HYSCAN_IS_GTK_PLANNER_STATUS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PLANNER_STATUS))
#define HYSCAN_GTK_PLANNER_STATUS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PLANNER_STATUS, HyScanGtkPlannerStatusClass))

typedef struct _HyScanGtkPlannerStatus HyScanGtkPlannerStatus;
typedef struct _HyScanGtkPlannerStatusPrivate HyScanGtkPlannerStatusPrivate;
typedef struct _HyScanGtkPlannerStatusClass HyScanGtkPlannerStatusClass;

struct _HyScanGtkPlannerStatus
{
  GtkLabel parent_instance;

  HyScanGtkPlannerStatusPrivate *priv;
};

struct _HyScanGtkPlannerStatusClass
{
  GtkLabelClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_planner_status_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_planner_status_new              (HyScanGtkMapPlanner *planner);

G_END_DECLS

#endif /* __HYSCAN_GTK_PLANNER_STATUS_H__ */

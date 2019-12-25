#ifndef __HYSCAN_GTK_PLANNER_ORIGIN_H__
#define __HYSCAN_GTK_PLANNER_ORIGIN_H__

#include <gtk/gtk.h>
#include <hyscan-planner-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PLANNER_ORIGIN             (hyscan_gtk_planner_origin_get_type ())
#define HYSCAN_GTK_PLANNER_ORIGIN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PLANNER_ORIGIN, HyScanGtkPlannerOrigin))
#define HYSCAN_IS_GTK_PLANNER_ORIGIN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PLANNER_ORIGIN))
#define HYSCAN_GTK_PLANNER_ORIGIN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PLANNER_ORIGIN, HyScanGtkPlannerOriginClass))
#define HYSCAN_IS_GTK_PLANNER_ORIGIN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PLANNER_ORIGIN))
#define HYSCAN_GTK_PLANNER_ORIGIN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PLANNER_ORIGIN, HyScanGtkPlannerOriginClass))

typedef struct _HyScanGtkPlannerOrigin HyScanGtkPlannerOrigin;
typedef struct _HyScanGtkPlannerOriginPrivate HyScanGtkPlannerOriginPrivate;
typedef struct _HyScanGtkPlannerOriginClass HyScanGtkPlannerOriginClass;

struct _HyScanGtkPlannerOrigin
{
  GtkBox parent_instance;

  HyScanGtkPlannerOriginPrivate *priv;
};

struct _HyScanGtkPlannerOriginClass
{
  GtkBoxClass parent_class;
};

HYSCAN_API
GType                  hyscan_gtk_planner_origin_get_type         (void);

HYSCAN_API
GtkWidget *            hyscan_gtk_planner_origin_new              (HyScanPlannerModel *model);

G_END_DECLS

#endif /* __HYSCAN_GTK_PLANNER_ORIGIN_H__ */

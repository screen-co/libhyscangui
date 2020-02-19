#ifndef __HYSCAN_GTK_PLANNER_SHIFT_H__
#define __HYSCAN_GTK_PLANNER_SHIFT_H__

#include <gtk/gtk.h>
#include <hyscan-gtk-map-planner.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_GTK_PLANNER_SHIFT             (hyscan_gtk_planner_shift_get_type ())
#define HYSCAN_GTK_PLANNER_SHIFT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_GTK_PLANNER_SHIFT, HyScanGtkPlannerShift))
#define HYSCAN_IS_GTK_PLANNER_SHIFT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_GTK_PLANNER_SHIFT))
#define HYSCAN_GTK_PLANNER_SHIFT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_GTK_PLANNER_SHIFT, HyScanGtkPlannerShiftClass))
#define HYSCAN_IS_GTK_PLANNER_SHIFT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_GTK_PLANNER_SHIFT))
#define HYSCAN_GTK_PLANNER_SHIFT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_GTK_PLANNER_SHIFT, HyScanGtkPlannerShiftClass))

typedef struct _HyScanGtkPlannerShift HyScanGtkPlannerShift;
typedef struct _HyScanGtkPlannerShiftPrivate HyScanGtkPlannerShiftPrivate;
typedef struct _HyScanGtkPlannerShiftClass HyScanGtkPlannerShiftClass;

struct _HyScanGtkPlannerShift
{
  GtkGrid parent_instance;

  HyScanGtkPlannerShiftPrivate *priv;
};

struct _HyScanGtkPlannerShiftClass
{
  GtkGridClass parent_class;
};

HYSCAN_API
GType                      hyscan_gtk_planner_shift_get_type         (void);

HYSCAN_API
GtkWidget *                hyscan_gtk_planner_shift_new              (void);

HYSCAN_API
void                       hyscan_gtk_planner_shift_set_viewer       (HyScanGtkPlannerShift    *shift,
                                                                      HyScanGtkMapPlanner      *viewer);

HYSCAN_API
void                       hyscan_gtk_planner_shift_set_track        (HyScanGtkPlannerShift    *shift,
                                                                      const HyScanPlannerTrack *track,
                                                                      const HyScanPlannerZone  *zone);

HYSCAN_API
void                       hyscan_gtk_planner_shift_save             (HyScanGtkPlannerShift    *shift,
                                                                      HyScanObjectModel        *model);


G_END_DECLS

#endif /* __HYSCAN_GTK_PLANNER_SHIFT_H__ */

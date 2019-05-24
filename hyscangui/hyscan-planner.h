#ifndef __HYSCAN_PLANNER_H__
#define __HYSCAN_PLANNER_H__

#include <hyscan-geo.h>
#include "hyscan-navigation-model.h"

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER             (hyscan_planner_get_type ())
#define HYSCAN_PLANNER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER, HyScanPlanner))
#define HYSCAN_IS_PLANNER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))
#define HYSCAN_IS_PLANNER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER))
#define HYSCAN_PLANNER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER, HyScanPlannerClass))

typedef struct _HyScanPlanner HyScanPlanner;
typedef struct _HyScanPlannerTrack HyScanPlannerTrack;
typedef struct _HyScanPlannerPrivate HyScanPlannerPrivate;
typedef struct _HyScanPlannerClass HyScanPlannerClass;

struct _HyScanPlannerTrack
{
  gchar             *id;
  gchar             *name;
  HyScanGeoGeodetic  start;
  HyScanGeoGeodetic  end;
};

struct _HyScanPlanner
{
  GObject parent_instance;

  HyScanPlannerPrivate *priv;
};

struct _HyScanPlannerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_planner_get_type         (void);

HYSCAN_API
HyScanPlanner *        hyscan_planner_new              (void);

HYSCAN_API
void                   hyscan_planner_load_ini         (HyScanPlanner            *planner,
                                                        const gchar              *file_name);

HYSCAN_API
void                   hyscan_planner_track_free       (HyScanPlannerTrack       *track);

HYSCAN_API
HyScanPlannerTrack *   hyscan_planner_track_copy       (const HyScanPlannerTrack *track);

HYSCAN_API
void                   hyscan_planner_set_nav_model    (HyScanPlanner            *planner,
                                                        HyScanNavigationModel    *model);

HYSCAN_API
void                   hyscan_planner_create           (HyScanPlanner            *planner,
                                                        HyScanPlannerTrack       *track);

HYSCAN_API
void                   hyscan_planner_update           (HyScanPlanner            *planner,
                                                        const HyScanPlannerTrack *track);

HYSCAN_API
void                   hyscan_planner_delete           (HyScanPlanner            *planner,
                                                        const gchar              *id);

HYSCAN_API
GHashTable *           hyscan_planner_get              (HyScanPlanner            *planner);

HYSCAN_API
gboolean               hyscan_planner_delta            (HyScanPlanner            *planner,
                                                        gdouble                  *distance,
                                                        gdouble                  *angle,
                                                        guint64                  *time_passed);

HYSCAN_API
void                   hyscan_planner_save_ini         (HyScanPlanner            *planner,
                                                        const gchar              *file_name);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_H__ */

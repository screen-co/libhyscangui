#ifndef __HYSCAN_PLANNER_SELECTION_H__
#define __HYSCAN_PLANNER_SELECTION_H__

#include <hyscan-planner-model.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_PLANNER_SELECTION             (hyscan_planner_selection_get_type ())
#define HYSCAN_PLANNER_SELECTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_PLANNER_SELECTION, HyScanPlannerSelection))
#define HYSCAN_IS_PLANNER_SELECTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_PLANNER_SELECTION))
#define HYSCAN_PLANNER_SELECTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_PLANNER_SELECTION, HyScanPlannerSelectionClass))
#define HYSCAN_IS_PLANNER_SELECTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_PLANNER_SELECTION))
#define HYSCAN_PLANNER_SELECTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_PLANNER_SELECTION, HyScanPlannerSelectionClass))

typedef struct _HyScanPlannerSelection HyScanPlannerSelection;
typedef struct _HyScanPlannerSelectionPrivate HyScanPlannerSelectionPrivate;
typedef struct _HyScanPlannerSelectionClass HyScanPlannerSelectionClass;

struct _HyScanPlannerSelection
{
  GObject parent_instance;

  HyScanPlannerSelectionPrivate *priv;
};

struct _HyScanPlannerSelectionClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                       hyscan_planner_selection_get_type         (void);

HYSCAN_API
HyScanPlannerSelection *    hyscan_planner_selection_new              (HyScanPlannerModel      *model);

HYSCAN_API
gchar **                    hyscan_planner_selection_get_tracks       (HyScanPlannerSelection  *selection);

HYSCAN_API
gchar *                     hyscan_planner_selection_get_active_track (HyScanPlannerSelection  *selection);

HYSCAN_API
HyScanPlannerModel *        hyscan_planner_selection_get_model        (HyScanPlannerSelection  *selection);

HYSCAN_API
gchar *                     hyscan_planner_selection_get_zone         (HyScanPlannerSelection  *selection,
                                                                       gint                    *vertex_index);

HYSCAN_API
void                        hyscan_planner_selection_set_zone         (HyScanPlannerSelection  *selection,
                                                                       const gchar             *zone_id,
                                                                       gint                     vertex_index);

HYSCAN_API
void                        hyscan_planner_selection_activate         (HyScanPlannerSelection  *selection,
                                                                       const gchar             *track_id);

HYSCAN_API
void                        hyscan_planner_selection_set_tracks       (HyScanPlannerSelection  *selection,
                                                                       gchar                  **tracks);

HYSCAN_API
void                        hyscan_planner_selection_append           (HyScanPlannerSelection  *selection,
                                                                       const gchar             *track_id);

HYSCAN_API
void                        hyscan_planner_selection_remove           (HyScanPlannerSelection  *selection,
                                                                       const gchar             *track_id);

HYSCAN_API
void                        hyscan_planner_selection_remove_all       (HyScanPlannerSelection  *selection);

HYSCAN_API
gboolean                    hyscan_planner_selection_contains         (HyScanPlannerSelection  *selection,
                                                                       const gchar             *track_id);

G_END_DECLS

#endif /* __HYSCAN_PLANNER_SELECTION_H__ */

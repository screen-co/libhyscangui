#ifndef __HYSCAN_TRACK_LIST_MODEL_H__
#define __HYSCAN_TRACK_LIST_MODEL_H__

#include <glib-object.h>
#include <hyscan-db-info.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_TRACK_LIST_MODEL             (hyscan_track_list_model_get_type ())
#define HYSCAN_TRACK_LIST_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_TRACK_LIST_MODEL, HyScanTrackListModel))
#define HYSCAN_IS_TRACK_LIST_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_TRACK_LIST_MODEL))
#define HYSCAN_TRACK_LIST_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_TRACK_LIST_MODEL, HyScanTrackListModelClass))
#define HYSCAN_IS_TRACK_LIST_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_TRACK_LIST_MODEL))
#define HYSCAN_TRACK_LIST_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_TRACK_LIST_MODEL, HyScanTrackListModelClass))

typedef struct _HyScanTrackListModel HyScanTrackListModel;
typedef struct _HyScanTrackListModelPrivate HyScanTrackListModelPrivate;
typedef struct _HyScanTrackListModelClass HyScanTrackListModelClass;

struct _HyScanTrackListModel
{
  GObject parent_instance;

  HyScanTrackListModelPrivate *priv;
};

struct _HyScanTrackListModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_track_list_model_get_type         (void);

HYSCAN_API
HyScanTrackListModel * hyscan_track_list_model_new              (void);

HYSCAN_API
GHashTable *           hyscan_track_list_model_get              (HyScanTrackListModel *tlist_model);

HYSCAN_API
void                   hyscan_track_list_model_set_active       (HyScanTrackListModel *tlist_model,
                                                                 const gchar          *track_name,
                                                                 gboolean              active);

HYSCAN_API
gboolean               hyscan_track_list_model_get_active       (HyScanTrackListModel *tlist_model,
                                                                 const gchar          *track_name);


G_END_DECLS

#endif /* __HYSCAN_TRACK_LIST_MODEL_H__ */

#ifndef __HYSCAN_MARK_LOC_MODEL_H__
#define __HYSCAN_MARK_LOC_MODEL_H__

#include <glib-object.h>
#include <hyscan-db.h>
#include <hyscan-cache.h>
#include <hyscan-mark-location.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_MARK_LOC_MODEL             (hyscan_mark_loc_model_get_type ())
#define HYSCAN_MARK_LOC_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModel))
#define HYSCAN_IS_MARK_LOC_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_MARK_LOC_MODEL))
#define HYSCAN_MARK_LOC_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModelClass))
#define HYSCAN_IS_MARK_LOC_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_MARK_LOC_MODEL))
#define HYSCAN_MARK_LOC_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_MARK_LOC_MODEL, HyScanMarkLocModelClass))

typedef struct _HyScanMarkLocModel HyScanMarkLocModel;
typedef struct _HyScanMarkLocModelPrivate HyScanMarkLocModelPrivate;
typedef struct _HyScanMarkLocModelClass HyScanMarkLocModelClass;

struct _HyScanMarkLocModel
{
  GObject parent_instance;

  HyScanMarkLocModelPrivate *priv;
};

struct _HyScanMarkLocModelClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_mark_loc_model_get_type         (void);

HYSCAN_API
HyScanMarkLocModel *   hyscan_mark_loc_model_new              (HyScanDB    *db,
                                                               HyScanCache *cache);

HYSCAN_API
void                   hyscan_mark_loc_model_set_project      (HyScanMarkLocModel *ml_model,
                                                               const gchar        *project);

HYSCAN_API
GHashTable *           hyscan_mark_loc_model_get              (HyScanMarkLocModel *ml_model);

G_END_DECLS

#endif /* __HYSCAN_MARK_LOC_MODEL_H__ */

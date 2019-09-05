#ifndef __HYSCAN_LIST_STORE_H__
#define __HYSCAN_LIST_STORE_H__

#include <glib-object.h>
#include <hyscan-api.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_LIST_STORE             (hyscan_list_store_get_type ())
#define HYSCAN_LIST_STORE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_LIST_STORE, HyScanListStore))
#define HYSCAN_IS_LIST_STORE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_LIST_STORE))
#define HYSCAN_LIST_STORE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_LIST_STORE, HyScanListStoreClass))
#define HYSCAN_IS_LIST_STORE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_LIST_STORE))
#define HYSCAN_LIST_STORE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_LIST_STORE, HyScanListStoreClass))

typedef struct _HyScanListStore HyScanListStore;
typedef struct _HyScanListStorePrivate HyScanListStorePrivate;
typedef struct _HyScanListStoreClass HyScanListStoreClass;

struct _HyScanListStore
{
  GObject parent_instance;

  HyScanListStorePrivate *priv;
};

struct _HyScanListStoreClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType             hyscan_list_store_get_type    (void);

HYSCAN_API
HyScanListStore * hyscan_list_store_new         (void);

HYSCAN_API
void              hyscan_list_store_append      (HyScanListStore *store,
                                                 const gchar     *id);

HYSCAN_API
void              hyscan_list_store_remove      (HyScanListStore *store,
                                                 const gchar     *id);

HYSCAN_API
void              hyscan_list_store_remove_all  (HyScanListStore *store);

HYSCAN_API
gboolean          hyscan_list_store_contains    (HyScanListStore *store,
                                                 const gchar     *id);

G_END_DECLS

#endif /* __HYSCAN_LIST_STORE_H__ */

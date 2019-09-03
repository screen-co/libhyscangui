#include <gio/gio.h>
#include "hyscan-list-store.h"

struct _HyScanListStorePrivate
{
  GPtrArray      *items;
};

static void    hyscan_list_store_interface_init        (GListModelInterface    *iface);
static void    hyscan_list_store_object_constructed    (GObject                *object);
static void    hyscan_list_store_object_finalize       (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanListStore, hyscan_list_store, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanListStore)
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, hyscan_list_store_interface_init))

static void
hyscan_list_store_class_init (HyScanListStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_list_store_object_constructed;
  object_class->finalize = hyscan_list_store_object_finalize;

}

static void
hyscan_list_store_init (HyScanListStore *list_store)
{
  list_store->priv = hyscan_list_store_get_instance_private (list_store);
}

static void
hyscan_list_store_object_constructed (GObject *object)
{
  HyScanListStore *list_store = HYSCAN_LIST_STORE (object);
  HyScanListStorePrivate *priv = list_store->priv;

  G_OBJECT_CLASS (hyscan_list_store_parent_class)->constructed (object);

  priv->items = g_ptr_array_new_with_free_func (g_free);
}

static void
hyscan_list_store_object_finalize (GObject *object)
{
  HyScanListStore *list_store = HYSCAN_LIST_STORE (object);
  HyScanListStorePrivate *priv = list_store->priv;

  g_ptr_array_unref (priv->items);

  G_OBJECT_CLASS (hyscan_list_store_parent_class)->finalize (object);
}

static GType
hyscan_list_store_get_item_type (GListModel *list)
{
  return G_TYPE_STRING;
}

static guint
hyscan_list_store_get_n_items (GListModel *list)
{
  HyScanListStore *store = HYSCAN_LIST_STORE (list);
  HyScanListStorePrivate *priv = store->priv;

  return priv->items->len;
}

static gpointer
hyscan_list_store_get_item (GListModel *list,
                            guint       position)
{
  HyScanListStore *store = HYSCAN_LIST_STORE (list);
  HyScanListStorePrivate *priv = store->priv;

  const gchar *id = NULL;

  if (position > priv->items->len - 1)
    return NULL;

  id = g_ptr_array_index (priv->items, position);

  return g_strdup (id);
}

static void
hyscan_list_store_interface_init (GListModelInterface *iface)
{
  iface->get_item = hyscan_list_store_get_item;
  iface->get_item_type = hyscan_list_store_get_item_type;
  iface->get_n_items = hyscan_list_store_get_n_items;
}

HyScanListStore *
hyscan_list_store_new (void)
{
  return g_object_new (HYSCAN_TYPE_LIST_STORE, NULL);
}

void
hyscan_list_store_append (HyScanListStore *store,
                          const gchar     *id)
{
  HyScanListStorePrivate *priv;
  gchar *item;

  g_return_if_fail (HYSCAN_IS_LIST_STORE (store));
  priv = store->priv;

  if (hyscan_list_store_contains (store, id))
    return;

  item = g_strdup (id);
  g_ptr_array_add (priv->items, item);

  g_list_model_items_changed (G_LIST_MODEL (store), priv->items->len - 1, 0, 1);
}

void
hyscan_list_store_remove (HyScanListStore *store,
                          const gchar     *id)
{
  HyScanListStorePrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_LIST_STORE (store));
  priv = store->priv;

  for (i = 0; i < priv->items->len; ++i)
    {
      gchar *item;

      item = g_ptr_array_index (priv->items, i);
      if (g_strcmp0 (id, item) != 0)
        continue;

      g_ptr_array_remove_index_fast (priv->items, i);
      g_list_model_items_changed (G_LIST_MODEL (store), i, 1, 0);
      --i;
    }
}

gboolean
hyscan_list_store_contains (HyScanListStore *store,
                            const gchar     *id)
{
  HyScanListStorePrivate *priv;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_LIST_STORE (store), FALSE);
  priv = store->priv;

  for (i = 0; i < priv->items->len; ++i)
    {
      if (g_strcmp0 (id, g_ptr_array_index (priv->items, i)) == 0)
        return TRUE;
    }

  return FALSE;
}

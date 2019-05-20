#include "hyscan-track-list-model.h"

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

struct _HyScanTrackListModelPrivate
{
  GHashTable *table;
};

static void    hyscan_track_list_model_object_constructed       (GObject               *object);
static void    hyscan_track_list_model_object_finalize          (GObject               *object);

static guint       hyscan_track_list_model_signals[SIGNAL_LAST] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (HyScanTrackListModel, hyscan_track_list_model, G_TYPE_OBJECT)

static void
hyscan_track_list_model_class_init (HyScanTrackListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_track_list_model_object_constructed;
  object_class->finalize = hyscan_track_list_model_object_finalize;

  /**
   * HyScanTrackListModel::changed:
   * @ml_model: указатель на #HyScanTrackListModel
   *
   * Сигнал посылается при изменении списка активных элементов.
   */
  hyscan_track_list_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_TRACK_LIST_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_track_list_model_init (HyScanTrackListModel *track_list_model)
{
  track_list_model->priv = hyscan_track_list_model_get_instance_private (track_list_model);
}

static void
hyscan_track_list_model_object_constructed (GObject *object)
{
  HyScanTrackListModel *track_list_model = HYSCAN_TRACK_LIST_MODEL (object);
  HyScanTrackListModelPrivate *priv = track_list_model->priv;

  G_OBJECT_CLASS (hyscan_track_list_model_parent_class)->constructed (object);

  priv->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
hyscan_track_list_model_object_finalize (GObject *object)
{
  HyScanTrackListModel *track_list_model = HYSCAN_TRACK_LIST_MODEL (object);
  HyScanTrackListModelPrivate *priv = track_list_model->priv;

  g_hash_table_unref (priv->table);

  G_OBJECT_CLASS (hyscan_track_list_model_parent_class)->finalize (object);
}

/**
 * hyscan_track_list_model_new:
 *
 * Returns: новый объект #HyScanTrackListModel
 */
HyScanTrackListModel *
hyscan_track_list_model_new (void)
{
  return g_object_new (HYSCAN_TYPE_TRACK_LIST_MODEL, NULL);
}


GHashTable *
hyscan_track_list_model_get (HyScanTrackListModel *tlist_model)
{
  g_return_val_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model), NULL);

  return g_hash_table_ref (tlist_model->priv->table);
}

void
hyscan_track_list_model_set_active (HyScanTrackListModel *tlist_model,
                                    const gchar          *track_name,
                                    gboolean              active)
{
  HyScanTrackListModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model));
  priv = tlist_model->priv;

  g_hash_table_insert (priv->table, g_strdup (track_name), GINT_TO_POINTER (active));

  g_signal_emit (tlist_model, hyscan_track_list_model_signals[SIGNAL_CHANGED], 0);
}

gboolean
hyscan_track_list_model_get_active (HyScanTrackListModel *tlist_model,
                                    const gchar          *track_name)
{
  HyScanTrackListModelPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_TRACK_LIST_MODEL (tlist_model), FALSE);
  priv = tlist_model->priv;

  return GPOINTER_TO_INT (g_hash_table_lookup (priv->table, g_strdup (track_name)));
}

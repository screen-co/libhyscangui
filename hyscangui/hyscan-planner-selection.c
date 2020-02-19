#include "hyscan-planner-selection.h"

enum
{
  PROP_O,
  PROP_MODEL
};

enum
{
  SIGNAL_TRACKS_CHANGED,
  SIGNAL_ZONE_CHANGED,
  SIGNAL_ACTIVATED,
  SIGNAL_LAST,
};

struct _HyScanPlannerSelectionPrivate
{
  HyScanPlannerModel          *model;           /* Модель объектов планировщика. */
  GHashTable                  *objects;         /* Объекты планировщика. */
  GArray                      *tracks;          /* Массив выбранных галсов. */
  gint                         vertex_index;    /* Вершина в выбранной зоне. */
  gchar                       *zone_id;         /* Выбранная зона. */
  gchar                       *active_track;    /* Активный галс, по которому идёт навигация. */
};

static void    hyscan_planner_selection_set_property             (GObject                *object,
                                                                  guint                   prop_id,
                                                                  const GValue           *value,
                                                                  GParamSpec             *pspec);
static void    hyscan_planner_selection_object_constructed       (GObject                *object);
static void    hyscan_planner_selection_object_finalize          (GObject                *object);
static void    hyscan_planner_selection_changed                  (HyScanPlannerSelection *selection);
static void    hyscan_planner_selection_clear_func               (gpointer data);

static guint hyscan_planner_selection_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlannerSelection, hyscan_planner_selection, G_TYPE_OBJECT)

static void
hyscan_planner_selection_class_init (HyScanPlannerSelectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_planner_selection_set_property;

  object_class->constructed = hyscan_planner_selection_object_constructed;
  object_class->finalize = hyscan_planner_selection_object_finalize;

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "HyScanPlannerModel", "Planner objects model",
                         HYSCAN_TYPE_PLANNER_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanPlannerSelection::tracks-changed:
   * @planner: указатель на #HyScanGtkMapPlanner
   * @tracks: NULL-терминированный массив выбранных галсов
   *
   * Сигнал посылается при изменении списка выбранных галсов.
   */
  hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED] =
    g_signal_new ("tracks-changed", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * HyScanPlannerSelection::zone-changed:
   * @planner: указатель на #HyScanGtkMapPlanner
   * @zone_id: идентификатор выбранной зоны или %NULL
   *
   * Сигнал посылается при изменении выбранной зоны.
   */
  hyscan_planner_selection_signals[SIGNAL_ZONE_CHANGED] =
    g_signal_new ("zone-changed", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * HyScanPlannerSelection::activated:
   * @planner: указатель на #HyScanGtkMapPlanner
   *
   * Сигнал посылается при изменении активного галса.
   */
  hyscan_planner_selection_signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated", HYSCAN_TYPE_PLANNER_SELECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_planner_selection_init (HyScanPlannerSelection *planner_selection)
{
  planner_selection->priv = hyscan_planner_selection_get_instance_private (planner_selection);
}

static void
hyscan_planner_selection_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  HyScanPlannerSelection *planner_selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = planner_selection->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      priv->model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_planner_selection_object_constructed (GObject *object)
{
  HyScanPlannerSelection *selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = selection->priv;

  G_OBJECT_CLASS (hyscan_planner_selection_parent_class)->constructed (object);

  priv->tracks = g_array_new (TRUE, FALSE, sizeof (gchar *));
  g_array_set_clear_func (priv->tracks, hyscan_planner_selection_clear_func);

  priv->objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));
  priv->vertex_index = -1;

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_planner_selection_changed), selection);
}

static void
hyscan_planner_selection_object_finalize (GObject *object)
{
  HyScanPlannerSelection *planner_selection = HYSCAN_PLANNER_SELECTION (object);
  HyScanPlannerSelectionPrivate *priv = planner_selection->priv;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  g_clear_object (&priv->model);
  g_array_free (priv->tracks, TRUE);
  g_free (priv->active_track);

  G_OBJECT_CLASS (hyscan_planner_selection_parent_class)->finalize (object);
}

static void
hyscan_planner_selection_clear_func (gpointer data)
{
  gchar **track_ptr = data;

  g_free (*track_ptr);
}

/* Проверяем, что выбранные галсы остались в базе данных. */
static void
hyscan_planner_selection_changed (HyScanPlannerSelection *selection)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  guint i;
  guint len_before;

  g_clear_pointer (&priv->objects, g_hash_table_destroy);
  priv->objects = hyscan_object_model_get (HYSCAN_OBJECT_MODEL (priv->model));

  if (priv->tracks->len == 0)
    return;

  len_before = priv->tracks->len;

  /* Удаляем из массива tracks все галсы, которых больше нет в модели. */
  if (priv->objects == NULL)
    {
      hyscan_planner_selection_remove_all (selection);
      return;
    }

  for (i = 0; i < priv->tracks->len;)
    {
      HyScanObject *object;
      gchar *id;

      id = g_array_index (priv->tracks, gchar *, i);
      object = g_hash_table_lookup (priv->objects, id);
      if (object == NULL || object->type != HYSCAN_PLANNER_TRACK)
        {
          g_array_remove_index (priv->tracks, i);
          continue;
        }

      ++i;
    }

  if (len_before != priv->tracks->len)
    g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

/**
 * hyscan_planner_selection_new:
 * @model: указатель на #HyScanPlannerModel
 *
 * Returns: (transfer full): указатель на объект #HyScanPlannerSelection, для
 *   удаления g_object_unref().
 */
HyScanPlannerSelection *
hyscan_planner_selection_new (HyScanPlannerModel *model)
{
  return g_object_new (HYSCAN_TYPE_PLANNER_SELECTION,
                       "model", model,
                       NULL);
}

/**
 * hyscan_planner_selection_get_tracks:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: (transfer full): NULL-терминированный массив выбранных галсов
 */
gchar **
hyscan_planner_selection_get_tracks (HyScanPlannerSelection *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_strdupv ((gchar **) selection->priv->tracks->data);
}

/**
 * hyscan_planner_selection_get_active_track:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: идентификатор активного галса, для удаления g_free().
 */
gchar *
hyscan_planner_selection_get_active_track (HyScanPlannerSelection  *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_strdup (selection->priv->active_track);
}

/**
 * hyscan_planner_selection_get_model:
 * @selection: указатель на #HyScanPlannerSelection
 *
 * Returns: (transfer full): модель объектов планировщика, для удаления g_object_unref().
 */
HyScanPlannerModel *
hyscan_planner_selection_get_model (HyScanPlannerSelection  *selection)
{
  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), NULL);

  return g_object_ref (selection->priv->model);
}

gchar *
hyscan_planner_selection_get_zone (HyScanPlannerSelection *selection,
                                   gint                   *vertex_index)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), FALSE);

  if (priv->zone_id == NULL)
    return NULL;

  vertex_index != NULL ? (*vertex_index = priv->vertex_index) : 0;

  return g_strdup (priv->zone_id);
}

void
hyscan_planner_selection_set_zone (HyScanPlannerSelection *selection,
                                   const gchar            *zone_id,
                                   gint                    vertex_index)
{
  HyScanPlannerSelectionPrivate *priv = selection->priv;
  HyScanPlannerZone *object;
  gint last_vertex_index;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));

  if (zone_id != NULL)
    {
      object = g_hash_table_lookup (priv->objects, zone_id);
      g_return_if_fail (object != NULL && object->type == HYSCAN_PLANNER_ZONE);

      last_vertex_index = (gint) object->points_len - 1;
    }
  else
    {
      last_vertex_index = 0;
    }

  vertex_index = MIN (vertex_index, last_vertex_index);

  if (g_strcmp0 (priv->zone_id, zone_id) == 0 && priv->vertex_index == vertex_index)
    return;

  priv->zone_id = g_strdup (zone_id);
  priv->vertex_index = vertex_index;

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_ZONE_CHANGED], 0);
}

void
hyscan_planner_selection_activate (HyScanPlannerSelection *selection,
                                   const gchar            *track_id)
{
  HyScanPlannerSelectionPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  g_free (priv->active_track);
  priv->active_track = g_strdup (track_id);

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_ACTIVATED], 0);
}

void
hyscan_planner_selection_set_tracks (HyScanPlannerSelection  *selection,
                                     gchar                  **tracks)
{
  HyScanPlannerSelectionPrivate *priv;
  gchar *track_id;
  gint i;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  g_array_remove_range (priv->tracks, 0, priv->tracks->len);
  for (i = 0; tracks[i] != NULL; ++i)
    {
      track_id = g_strdup (tracks[i]);
      g_array_append_val (priv->tracks, track_id);
    }

  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

void
hyscan_planner_selection_append (HyScanPlannerSelection  *selection,
                                 const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  HyScanObject *object;
  gchar *new_track_id;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  if (hyscan_planner_selection_contains (selection, track_id))
    return;

  object = g_hash_table_lookup (priv->objects, track_id);
  g_return_if_fail (object != NULL && object->type == HYSCAN_PLANNER_TRACK);

  new_track_id = g_strdup (track_id);
  g_array_append_val (priv->tracks, new_track_id);
  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

void
hyscan_planner_selection_remove (HyScanPlannerSelection  *selection,
                                 const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  guint i;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  for (i = 0; i < priv->tracks->len; i++)
    {
      if (g_strcmp0 (track_id, g_array_index (priv->tracks, gchar *, i)) != 0)
        continue;

      g_array_remove_index (priv->tracks, i);
      g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
      return;
    }
}

void
hyscan_planner_selection_remove_all (HyScanPlannerSelection  *selection)
{
  HyScanPlannerSelectionPrivate *priv;

  g_return_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection));
  priv = selection->priv;

  if (priv->tracks->len == 0)
    return;

  g_array_remove_range (priv->tracks, 0, priv->tracks->len);
  g_signal_emit (selection, hyscan_planner_selection_signals[SIGNAL_TRACKS_CHANGED], 0, priv->tracks->data);
}

gboolean
hyscan_planner_selection_contains (HyScanPlannerSelection  *selection,
                                   const gchar             *track_id)
{
  HyScanPlannerSelectionPrivate *priv;
  guint i;

  g_return_val_if_fail (HYSCAN_IS_PLANNER_SELECTION (selection), FALSE);
  priv = selection->priv;

  if (priv->tracks == NULL)
    return FALSE;

  for (i = 0; i < priv->tracks->len; ++i)
    {
      const gchar *id;

      id = g_array_index (priv->tracks, gchar *, i);
      if (g_strcmp0 (id, track_id) == 0)
        return TRUE;
    }

  return FALSE;
}

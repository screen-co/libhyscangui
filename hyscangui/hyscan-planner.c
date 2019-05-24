#include "hyscan-planner.h"

#define TRACK_ID_LEN  33

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST,
};

struct _HyScanPlannerPrivate
{
  GMutex      lock;
  GHashTable *tracks;
};

static void         hyscan_planner_object_constructed       (GObject               *object);
static void         hyscan_planner_object_finalize          (GObject               *object);
static GHashTable * hyscan_planner_table_new                (void);

static guint        hyscan_planner_signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (HyScanPlanner, hyscan_planner, G_TYPE_OBJECT)

static void
hyscan_planner_class_init (HyScanPlannerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = hyscan_planner_object_constructed;
  object_class->finalize = hyscan_planner_object_finalize;

  hyscan_planner_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_PLANNER,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_planner_init (HyScanPlanner *planner)
{
  planner->priv = hyscan_planner_get_instance_private (planner);
}

static void
hyscan_planner_object_constructed (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  G_OBJECT_CLASS (hyscan_planner_parent_class)->constructed (object);

  g_mutex_init (&priv->lock);
  priv->tracks = hyscan_planner_table_new ();
}

static void
hyscan_planner_object_finalize (GObject *object)
{
  HyScanPlanner *planner = HYSCAN_PLANNER (object);
  HyScanPlannerPrivate *priv = planner->priv;

  g_mutex_clear (&priv->lock);
  g_hash_table_unref (priv->tracks);

  G_OBJECT_CLASS (hyscan_planner_parent_class)->finalize (object);
}

static GHashTable *
hyscan_planner_table_new (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) hyscan_planner_track_free);
}

HyScanPlanner *
hyscan_planner_new (void)
{
  return g_object_new (HYSCAN_TYPE_PLANNER, NULL);
}

/* Функция генерирует идентификатор. */
static gchar *
hyscan_planner_create_id (void)
{
  static gchar dict[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  guint i;
  gchar *id;

  id = g_new (gchar, TRACK_ID_LEN + 1);
  id[TRACK_ID_LEN] = '\0';

  for (i = 0; i < TRACK_ID_LEN; i++)
    {
      gint rnd;

      rnd = g_random_int_range (0, sizeof (dict) - 1);
      id[i] = dict[rnd];
    }

  return id;
}

void
hyscan_planner_save_ini (HyScanPlanner *planner,
                         const gchar   *file_name)
{
  HyScanPlannerPrivate *priv;
  GKeyFile *key_file;
  GHashTableIter iter;
  HyScanPlannerTrack *track;
  gchar *id;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));
  priv = planner->priv;

  key_file = g_key_file_new ();

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &track))
    {
      if (track->name != NULL)
        g_key_file_set_string (key_file, id, "name", track->name);
      g_key_file_set_double (key_file, track->id, "start_lat", track->start.lat);
      g_key_file_set_double (key_file, track->id, "start_lon", track->start.lon);
      g_key_file_set_double (key_file, track->id, "end_lat", track->end.lat);
      g_key_file_set_double (key_file, track->id, "end_lon", track->end.lon);
    }

  g_mutex_unlock (&priv->lock);

  g_key_file_save_to_file (key_file, file_name, NULL);
  g_key_file_unref (key_file);
}

void
hyscan_planner_load_ini (HyScanPlanner *planner,
                         const gchar   *file_name)
{
  HyScanPlannerPrivate *priv;
  GKeyFile *key_file;
  gchar **groups;
  gint i;
  
  g_return_if_fail (HYSCAN_IS_PLANNER (planner));
  priv = planner->priv;
  
  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, file_name, G_KEY_FILE_NONE, NULL);
  
  g_mutex_lock (&priv->lock);
  g_hash_table_remove_all (priv->tracks);
  groups = g_key_file_get_groups (key_file, NULL);
  for (i = 0; groups[i] != NULL; ++i)
    {
      HyScanPlannerTrack track;
      HyScanPlannerTrack *track_copy;
      gchar *name;

      name = g_key_file_get_string (key_file, groups[i], "name", NULL);

      track.id = groups[i];
      track.name = name;
      track.start.lat = g_key_file_get_double (key_file, groups[i], "start_lat", NULL);
      track.start.lon = g_key_file_get_double (key_file, groups[i], "start_lon", NULL);
      track.end.lat = g_key_file_get_double (key_file, groups[i], "end_lat", NULL);
      track.end.lon = g_key_file_get_double (key_file, groups[i], "end_lon", NULL);

      track_copy = hyscan_planner_track_copy (&track);
      g_hash_table_insert (priv->tracks, track_copy->id, track_copy);
      g_free (name);
    }
  g_mutex_unlock (&priv->lock);

  g_strfreev (groups);
  g_key_file_free (key_file);
  
  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}

/**
 * hyscan_planner_get:
 * @planner:
 *
 * Returns: (element-type HyScanPlannerTrack): таблица запланированных галсов.
 *   Для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_planner_get (HyScanPlanner *planner)
{
  HyScanPlannerPrivate *priv;
  GHashTable *tracks;
  GHashTableIter iter;
  HyScanPlannerTrack *track;

  g_return_val_if_fail (HYSCAN_IS_PLANNER (planner), NULL);
  priv = planner->priv;

  tracks = hyscan_planner_table_new ();

  g_mutex_lock (&priv->lock);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track))
    {
      HyScanPlannerTrack *track_copy;

      track_copy = hyscan_planner_track_copy (track);
      g_hash_table_insert (tracks, track_copy->id, track_copy);
    }

  g_mutex_unlock (&priv->lock);

  return tracks;
}

HyScanPlannerTrack *
hyscan_planner_track_copy (const HyScanPlannerTrack *track)
{
  HyScanPlannerTrack *copy;

  copy = g_slice_new (HyScanPlannerTrack);
  copy->start = track->start;
  copy->end = track->end;
  copy->name = g_strdup (track->name);
  copy->id = g_strdup (track->id);

  return copy;
}

void
hyscan_planner_track_free (HyScanPlannerTrack *track)
{
  g_free (track->id);
  g_free (track->name);
  g_slice_free (HyScanPlannerTrack, track);
}

void
hyscan_planner_delete (HyScanPlanner *planner,
                       const gchar   *id)
{
  HyScanPlannerPrivate *priv = planner->priv;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));

  g_mutex_lock (&priv->lock);
  g_hash_table_remove (priv->tracks, id);
  g_mutex_unlock (&priv->lock);

  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}

void
hyscan_planner_update (HyScanPlanner            *planner,
                       const HyScanPlannerTrack *track)
{
  HyScanPlannerPrivate *priv = planner->priv;
  HyScanPlannerTrack *track_copy;

  g_return_if_fail (HYSCAN_IS_PLANNER (planner));

  track_copy = hyscan_planner_track_copy (track);
  g_mutex_lock (&priv->lock);

  if (track_copy->id == NULL)
    track_copy->id = hyscan_planner_create_id ();

  g_hash_table_replace (priv->tracks, track_copy->id, track_copy);
  g_mutex_unlock (&priv->lock);

  g_signal_emit (planner, hyscan_planner_signals[SIGNAL_CHANGED], 0);
}

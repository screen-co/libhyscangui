/*
 * \file hyscan-gtk-waterfall-state.c
 *
 * \brief Исходный файл класса хранения параметров водопада.
 * \author Dmitriev Alexander (m1n7@yandex.ru)
 * \date 2017
 * \license Проприетарная лицензия ООО "Экран"
 *
 */
#include "hyscan-gtk-waterfall-state.h"
#include <hyscan-gui-marshallers.h>
#include <string.h>

enum
{
  PROP_CACHE = 1
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

enum
{
  DETAIL_SOURCES,
  DETAIL_TILE_FLAGS,
  DETAIL_TRACK,
  DETAIL_SPEED,
  DETAIL_VELOCITY,
  DETAIL_AMPFACT_PARAMS,
  DETAIL_DPTFACT_PARAMS,
  DETAIL_CACHE,
  DETAIL_LAST
};

struct _HyScanGtkWaterfallStatePrivate
{
  HyScanCache                *cache;
  HyScanWaterfallDisplayType  disp_type;
  HyScanSourceType            lsource;
  HyScanSourceType            rsource;
  HyScanTileFlags             tile_flags;

  HyScanAmplitudeFactory     *af;
  HyScanDepthFactory         *df;

  HyScanDB                   *db;
  gchar                      *project;
  gchar                      *track;

  gfloat                      speed;
  GArray                     *velocity;
};

static void     hyscan_gtk_waterfall_state_set_property          (GObject                *object,
                                                                  guint                   prop_id,
                                                                  const GValue           *value,
                                                                  GParamSpec             *pspec);

static void     hyscan_gtk_waterfall_state_object_constructed    (GObject                 *object);
static void     hyscan_gtk_waterfall_state_object_finalize       (GObject                 *object);

static guint    hyscan_gtk_waterfall_state_signals[SIGNAL_LAST] = {0};
static GQuark   hyscan_gtk_waterfall_state_details[DETAIL_LAST] = {0};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (HyScanGtkWaterfallState, hyscan_gtk_waterfall_state, HYSCAN_TYPE_GTK_LAYER_CONTAINER);

static void
hyscan_gtk_waterfall_state_class_init (HyScanGtkWaterfallStateClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  obj_class->set_property = hyscan_gtk_waterfall_state_set_property;
  obj_class->constructed = hyscan_gtk_waterfall_state_object_constructed;
  obj_class->finalize = hyscan_gtk_waterfall_state_object_finalize;

  hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_GTK_WATERFALL_STATE,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  hyscan_gtk_waterfall_state_details[DETAIL_SOURCES]      = g_quark_from_static_string ("sources");
  hyscan_gtk_waterfall_state_details[DETAIL_TILE_FLAGS]   = g_quark_from_static_string ("tile-flags");
  hyscan_gtk_waterfall_state_details[DETAIL_TRACK]        = g_quark_from_static_string ("track");
  hyscan_gtk_waterfall_state_details[DETAIL_SPEED]        = g_quark_from_static_string ("speed");
  hyscan_gtk_waterfall_state_details[DETAIL_VELOCITY]     = g_quark_from_static_string ("velocity");
  hyscan_gtk_waterfall_state_details[DETAIL_AMPFACT_PARAMS] = g_quark_from_static_string ("amp-factory");
  hyscan_gtk_waterfall_state_details[DETAIL_DPTFACT_PARAMS] = g_quark_from_static_string ("dpt-factory");

  g_object_class_install_property (obj_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "HyScanCache interface",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_waterfall_state_init (HyScanGtkWaterfallState *self)
{
  self->priv = hyscan_gtk_waterfall_state_get_instance_private (self);
}

static void
hyscan_gtk_waterfall_state_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkWaterfallState *self = HYSCAN_GTK_WATERFALL_STATE (object);
  HyScanGtkWaterfallStatePrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_waterfall_state_object_constructed (GObject *object)
{
  HyScanGtkWaterfallState *self = HYSCAN_GTK_WATERFALL_STATE (object);
  HyScanGtkWaterfallStatePrivate *priv = self->priv;
  G_OBJECT_CLASS (hyscan_gtk_waterfall_state_parent_class)->constructed (object);

  /* Задаем умолчания. */
  priv->disp_type     = HYSCAN_WATERFALL_DISPLAY_SIDESCAN;
  priv->lsource       = HYSCAN_SOURCE_SIDE_SCAN_PORT;
  priv->rsource       = HYSCAN_SOURCE_SIDE_SCAN_STARBOARD;
  priv->tile_flags    = 0;
  priv->speed         = 1.0;

  priv->af = hyscan_amplitude_factory_new (priv->cache);
  priv->df = hyscan_depth_factory_new (priv->cache);
}

static void
hyscan_gtk_waterfall_state_object_finalize (GObject *object)
{
  HyScanGtkWaterfallState *self = HYSCAN_GTK_WATERFALL_STATE (object);
  HyScanGtkWaterfallStatePrivate *priv = self->priv;

  g_clear_object (&priv->cache);
  g_clear_object (&priv->af);
  g_clear_object (&priv->df);

  g_clear_object (&priv->db);
  g_free (priv->project);
  g_free (priv->track);

  if (priv->velocity != NULL)
    g_array_unref (priv->velocity);

  G_OBJECT_CLASS (hyscan_gtk_waterfall_state_parent_class)->finalize (object);
}

/* Функция устанавливает режим отображения эхолот. */
void
hyscan_gtk_waterfall_state_echosounder (HyScanGtkWaterfallState *self,
                                        HyScanSourceType         source)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->disp_type = HYSCAN_WATERFALL_DISPLAY_ECHOSOUNDER;
  priv->lsource = priv->rsource = source;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SOURCES], NULL);
}

/* Функция устанавливает режим отображения водопад. */
void
hyscan_gtk_waterfall_state_sidescan (HyScanGtkWaterfallState *self,
                                     HyScanSourceType         lsource,
                                     HyScanSourceType         rsource)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->disp_type = HYSCAN_WATERFALL_DISPLAY_SIDESCAN;
  priv->lsource = lsource;
  priv->rsource = rsource;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SOURCES], NULL);
}

/* Функция устанавливает тип тайла (наклонная или горизонтальная дальность). */
void
hyscan_gtk_waterfall_state_set_tile_flags (HyScanGtkWaterfallState *self,
                                           HyScanTileFlags          flags)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  self->priv->tile_flags = flags;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_TILE_FLAGS], NULL);
}

/* Функция задает БД, проект и галс */
void
hyscan_gtk_waterfall_state_set_track (HyScanGtkWaterfallState *self,
                                      HyScanDB                *db,
                                      const gchar             *project,
                                      const gchar             *track)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (db == NULL || project == NULL || track == NULL)
    return;

  g_clear_object (&priv->db);
  g_clear_pointer (&priv->project, g_free);
  g_clear_pointer (&priv->track, g_free);

  priv->db = g_object_ref (db);
  priv->project = g_strdup (project);
  priv->track = g_strdup (track);

  hyscan_amplitude_factory_set_track (priv->af, db, project, track);
  hyscan_depth_factory_set_track (priv->df, db, project, track);

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_TRACK], NULL);
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_AMPFACT_PARAMS], NULL);
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_DPTFACT_PARAMS], NULL);
}

/* Функция устанавливает скорость судна. */
void
hyscan_gtk_waterfall_state_set_ship_speed (HyScanGtkWaterfallState *self,
                                           gfloat                   speed)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  priv->speed = speed;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_SPEED], NULL);
}

/* Функция устанавливает скорость звука. */
void
hyscan_gtk_waterfall_state_set_sound_velocity (HyScanGtkWaterfallState *self,
                                               GArray                  *velocity)
{
  GArray *copy = NULL;
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (velocity != NULL)
    {
      copy = g_array_new (FALSE, FALSE, sizeof (HyScanSoundVelocity));
      g_array_set_size (copy, velocity->len);
      memcpy (copy->data, velocity->data, sizeof (HyScanSoundVelocity) * velocity->len);
    }

  if (priv->velocity != NULL)
    g_array_unref (priv->velocity);

  priv->velocity = copy;
  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_VELOCITY], NULL);
}

/* Функция возвращает тип отображения и источники. */
HyScanWaterfallDisplayType
hyscan_gtk_waterfall_state_get_sources (HyScanGtkWaterfallState *self,
                                        HyScanSourceType        *lsource,
                                        HyScanSourceType        *rsource)
{
  HyScanGtkWaterfallStatePrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), -1);
  priv = self->priv;

  if (lsource != NULL)
    *lsource = priv->lsource;
  if (rsource != NULL)
    *rsource = priv->rsource;

  return priv->disp_type;
}

/* Функция возвращает тип тайла. */
HyScanTileFlags
hyscan_gtk_waterfall_state_get_tile_flags (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), 0);
  return self->priv->tile_flags;
}

HyScanAmplitudeFactory *
hyscan_gtk_waterfall_state_get_amp_factory (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return g_object_ref (self->priv->af);
}

HyScanDepthFactory *
hyscan_gtk_waterfall_state_get_dpt_factory (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return g_object_ref (self->priv->df);
}

/* Функция возвращает систему кэширования. */
HyScanCache *
hyscan_gtk_waterfall_state_get_cache (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return (self->priv->cache != NULL) ? g_object_ref (self->priv->cache) : NULL;
}

void
hyscan_gtk_waterfall_state_get_track (HyScanGtkWaterfallState *self,
                                      HyScanDB               **db,
                                      gchar                  **project,
                                      gchar                  **track)
{
  HyScanGtkWaterfallStatePrivate *priv;
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  priv = self->priv;

  if (db != NULL)
    *db = (priv->db != NULL) ? g_object_ref (priv->db) : NULL;
  if (project != NULL)
    *project = (priv->project != NULL) ? g_strdup (priv->project) : NULL;
  if (track != NULL)
    *track = (priv->track != NULL) ? g_strdup (priv->track) : NULL;
}

/* Функция возвращает скорость судна. */
gfloat
hyscan_gtk_waterfall_state_get_ship_speed (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), 0);
  return self->priv->speed;
}

/* Функция возвращает профиль скорости звука. */
GArray *
hyscan_gtk_waterfall_state_get_sound_velocity (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return (self->priv->velocity != NULL) ? g_array_ref (self->priv->velocity) : NULL;
}

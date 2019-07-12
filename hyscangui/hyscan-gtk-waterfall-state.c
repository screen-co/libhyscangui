/* hyscan-gtk-waterfall-state.c
 *
 * Copyright 2017-2019 Screen LLC, Alexander Dmitriev <m1n7@yandex.ru>
 *
 * This file is part of HyScanGui library.
 *
 * HyScanGui is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanGui is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanGui имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanGui на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-gtk-waterfall-state
 * @Title: HyScanGtkWaterfallState
 * @Short_description: хранение параметров водопада
 *
 * С помощью функций-сеттеров устанавливаются параметры и в этот же момент
 * эмиттируется детализированный сигнал "changed".
 *
 * Есть 2 функции для задания типа отображения (водопад или эхолот):
 * - #hyscan_gtk_waterfall_state_echosounder задает режим отображения "эхолот"
 *   (один борт, более поздние данные справа)
 * - #hyscan_gtk_waterfall_state_sidescan задает режим отображения "ГБО"
 *   (два борта, более поздние данные сверху)
 * - #hyscan_gtk_waterfall_state_get_sources возвращает режим отображения и
 *   выбранные источники.
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

  /**
   * HyScanGtkWaterfallState::changed:
   * @state: объект, получивший сигнал
   * @user_data: данные, определенные в момент подключения к сигналу
   *
   * Сигнал отправляется когда изменяются параметры водопада. Сигнал поддерживает
   * детализацию и можно подписаться только на определенные изменения.
   * |[<!-- language="C" -->
   * g_signal_connect (state, "changed::track",
   *                   G_CALLBACK (concrete_layer_track_changed),
   *                   layer)
   * ]|
   */
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

/**
 * hyscan_gtk_waterfall_state_echosounder:
 * @self: объект #HyScanGtkWaterfallState
 * @source: отображаемый тип данных
 *
 * Функция устанавливает режим отображения эхолот.
 * Эмиттирует #GtkAdjustment::changed с "sources".
 */
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

/**
 * hyscan_gtk_waterfall_state_sidescan:
 * @self: объект #HyScanGtkWaterfallState
 * @lsource: тип данных, отображаемый слева
 * @rsource: тип данных, отображаемый справа
 *
 * Функция устанавливает режим отображения водопад.
 * Эмиттирует #GtkAdjustment::changed с ":sources".
 */
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

/**
 * hyscan_gtk_waterfall_state_set_tile_flags:
 * @self: объект #HyScanGtkWaterfallState
 * @type: флаги тайлогенерации
 *
 * Функция устанавливает флаги тайлогенератора.
 * Эмиттирует #GtkAdjustment::changed с ":tile-flags".
 */
void
hyscan_gtk_waterfall_state_set_tile_flags (HyScanGtkWaterfallState *self,
                                           HyScanTileFlags          flags)
{
  g_return_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self));
  self->priv->tile_flags = flags;

  g_signal_emit (self, hyscan_gtk_waterfall_state_signals[SIGNAL_CHANGED],
                       hyscan_gtk_waterfall_state_details[DETAIL_TILE_FLAGS], NULL);
}

/**
 * hyscan_gtk_waterfall_state_set_track:
 * @self: объект #HyScanGtkWaterfallState
 * @db: объект #HyScanDB
 * @project: имя проекта
 * @track: имя галса
 *
 * Функция задает БД, проект и галс
 * Эмиттирует последовательно #GtkAdjustment::changed с ":track",
 * #GtkAdjustment::changed с ":amp-factory", #GtkAdjustment::changed с ":dpt-factory".
 */
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

/**
 * hyscan_gtk_waterfall_state_set_ship_speed:
 * @self: объект #HyScanGtkWaterfallState
 * @speed: скорость судна в м/с
 *
 * Функция устанавливает скорость судна.
 * Эмиттирует #GtkAdjustment::changed с ":speed".
 */
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

/**
 * hyscan_gtk_waterfall_state_set_sound_velocity:
 * @self: объект #HyScanGtkWaterfallState
 * @velocity: профиль скорости звука
 *
 * Функция устанавливает скорость звука.
 * Эмиттирует #GtkAdjustment::changed с ":velocity".
 */
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

/**
 * hyscan_gtk_waterfall_state_get_sources:
 * @self: объект #HyScanGtkWaterfallState
 * @lsource: (out) (optional): тип данных слева
 * @rsource: (out) (optional): тип данных справа

 * Функция возвращает тип отображения и источники, как противоположность
 * функциям hyscan_gtk_waterfall_state_echosounder и hyscan_gtk_waterfall_state_sidescan.
 * В случае режима эхолот источники справа и слева будут идентичны.
 *
 * Returns: тип отображения.
 */
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

/**
 * hyscan_gtk_waterfall_state_get_tile_flags:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает тип тайла.
 *
 * Returns: тип тайла.
 */
HyScanTileFlags
hyscan_gtk_waterfall_state_get_tile_flags (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), 0);
  return self->priv->tile_flags;
}

/**
 * hyscan_gtk_waterfall_state_get_amp_factory:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает фабрику акустических данных.
 *
 * Returns: (transfer full): #HyScanAmplitudeFactory.
 */
HyScanAmplitudeFactory *
hyscan_gtk_waterfall_state_get_amp_factory (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return g_object_ref (self->priv->af);
}

/**
 * hyscan_gtk_waterfall_state_get_dpt_factory:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает фабрику данных глубины.
 *
 * Returns: (transfer full): #HyScanDepthFactory.
 */
HyScanDepthFactory *
hyscan_gtk_waterfall_state_get_dpt_factory (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return g_object_ref (self->priv->df);
}

/**
 * hyscan_gtk_waterfall_state_get_cache:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает систему кэширования.
 *
 * Returns: (transfer full): объект #HyScanCache.
 */
HyScanCache *
hyscan_gtk_waterfall_state_get_cache (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return (self->priv->cache != NULL) ? g_object_ref (self->priv->cache) : NULL;
}

/**
 * hyscan_gtk_waterfall_state_get_track:
 * @self: объект #HyScanGtkWaterfallState
 * @db: (out) (optional): объект #HyScanDB
 * @project: (out) (optional): имя проекта
 * @track: (out) (optional): имя галса
 *
 * Функция возвращает БД, проект и галс.
 */
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

/**
 * hyscan_gtk_waterfall_state_get_ship_speed:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает скорость судна
 *
 * Returns: скорость судна.
 */
gfloat
hyscan_gtk_waterfall_state_get_ship_speed (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), 0);
  return self->priv->speed;
}

/**
 * hyscan_gtk_waterfall_state_get_sound_velocity:
 * @self: объект #HyScanGtkWaterfallState
 *
 * Функция возвращает профиль скорости звука.
 *
 * Returns: (element-type HyScanSoundVelocity) (transfer container):
 * профиль скорости звука.
 *
 */
GArray *
hyscan_gtk_waterfall_state_get_sound_velocity (HyScanGtkWaterfallState *self)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_WATERFALL_STATE (self), NULL);
  return (self->priv->velocity != NULL) ? g_array_ref (self->priv->velocity) : NULL;
}

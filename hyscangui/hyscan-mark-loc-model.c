/* hyscan-mark-loc-model.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * SECTION: hyscan-mark-loc-model
 * @Short_description: Модель местоположения акустических меток
 * @Title: HyScanMarkLocModel
 * @See_also: #HyScanMarkLocation, #HyScanObjectModel
 *
 * Модель #HyScanMarkLocModel отслеживает изменение меток в проекте и определяет их
 * географическое местоположение на основе NMEA-данных.
 *
 * Для отслеживания изменений в модели пользователю необходимо подключиться
 * к сигналу #HyScanMarkLocModel::changed. Список меток можно получить в виде
 * хэш-таблицы с элементами #HyScanMarkLocation через функцию hyscan_mark_loc_model_get().
 *
 * Функции класса:
 * - hyscan_mark_loc_model_new() - создает новую модель;
 * - hyscan_mark_loc_model_set_project() - устанавливает имя проекта;
 * - hyscan_mark_loc_model_get() - получает данные модели.
 *
 */

#include "hyscan-mark-loc-model.h"
#include <hyscan-db-info.h>
#include <hyscan-object-model.h>
#include <hyscan-projector.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-nmea-parser.h>
#include <string.h>
#include <hyscan-depthometer.h>

// todo: вместо этих макросов брать номера каналов из каких-то пользовательских настроек
#define ACOUSTIC_CHANNEL 1   /* Канал ГБО с акустическими данными. */
#define NMEA_RMC_CHANNEL 1   /* Канал NMEA с навигационными данными. */
#define NMEA_DPT_CHANNEL 2   /* Канал NMEA с данными эхолота. */

#define CHANGED_NONE       0u            /* Нет изменений. */
#define CHANGED_TRACKS     1u            /* Изменилась информация о галсах проекта. */
#define CHANGED_PROJECT    1u << 1u      /* Изменился проект. */
#define CHANGED_MARKS      1u << 2u      /* Изменились метки водопада. */
#define CHANGED_SHUTDOWN   1u << 3u      /* Признак завершения работы. */

enum
{
  PROP_O,
  PROP_DB,
  PROP_CACHE
};

enum
{
  SIGNAL_CHANGED,
  SIGNAL_LAST
};

typedef struct
{
  guint          changed;      /* Битовая маска изменённых параметров. */
  gchar         *project;      /* Имя проекта, на который надо переключится. */
} HyScanMarkLocModelState;

struct _HyScanMarkLocModelPrivate
{
  HyScanDB                *db;           /* База данных. */
  HyScanCache             *cache;        /* Кэш. */
  HyScanDBInfo            *db_info;      /* Модель галсов в БД. */
  HyScanObjectModel       *mark_model;   /* Модель меток. */
  HyScanGeo               *geo;          /* Перевод географических координат. */

  gchar                   *project;      /* Название проекта. */
  GHashTable              *track_names;  /* Хэш-таблица соотвествтия id галса с его названием.*/

  GThread                 *processor;    /* Поток обработки меток. */
  HyScanMarkLocModelState  state;        /* Состояние для передачи изменений из основного потока в поток обработки. */
  GCond                    cond;         /* Сигнализатор изменения состояние. */
  GMutex                   mutex;        /* Мьютек для доступа к state. */

  GHashTable              *locations;    /* Хэш-таблица с метками HyScanMarkLocation.*/
  GRWLock                  mark_lock;    /* Блокировка доступа к locations. */
};

static void                   hyscan_mark_loc_model_set_property             (GObject               *object,
                                                                              guint                  prop_id,
                                                                              const GValue          *value,
                                                                              GParamSpec            *pspec);
static void                   hyscan_mark_loc_model_object_constructed       (GObject               *object);
static void                   hyscan_mark_loc_model_object_finalize          (GObject               *object);
static void                   hyscan_mark_loc_model_db_changed               (HyScanMarkLocModel    *ml_model);
static void                   hyscan_mark_loc_model_model_changed            (HyScanMarkLocModel    *ml_model);
static GHashTable *           hyscan_mark_loc_model_create_table             (void);
static gboolean               hyscan_mark_loc_model_emit_changed             (gpointer               data);
static gpointer               hyscan_mark_loc_model_process                  (gpointer               data);
static HyScanMarkLocation *   hyscan_mark_loc_model_load                     (HyScanMarkLocModel    *ml_model,
                                                                              HyScanMarkWaterfall   *mark);
static gboolean               hyscan_mark_loc_model_load_nav                 (HyScanMarkLocModel    *ml_model,
                                                                              HyScanMarkLocation    *location);
static gboolean               hyscan_mark_loc_model_load_offset              (HyScanMarkLocModel    *ml_model,
                                                                              HyScanMarkLocation    *location,
                                                                              HyScanSourceType       source);
static gboolean               hyscan_mark_loc_model_load_time                (HyScanMarkLocModel    *ml_model,
                                                                              HyScanMarkLocation    *location,
                                                                              HyScanSourceType       source);
static gboolean               hyscan_mark_loc_model_load_geo                 (HyScanMarkLocModel    *ml_model,
                                                                              HyScanMarkLocation    *location);
inline static HyScanNavData * hyscan_mark_loc_model_rmc_data_new             (HyScanMarkLocModel    *ml_model,
                                                                              const gchar           *track_name,
                                                                              HyScanNMEAField        field);
static inline gdouble         hyscan_mark_loc_model_weight                   (gint64                 mtime,
                                                                              gint64                 ltime,
                                                                              gint64                 rtime,
                                                                              gdouble                lval,
                                                                              gdouble                rval);
static inline gdouble         hyscan_mark_loc_model_direction                (HyScanSourceType       source_type);

static guint       hyscan_mark_loc_model_signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (HyScanMarkLocModel, hyscan_mark_loc_model, G_TYPE_OBJECT)

static void
hyscan_mark_loc_model_class_init (HyScanMarkLocModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_mark_loc_model_set_property;

  object_class->constructed = hyscan_mark_loc_model_object_constructed;
  object_class->finalize = hyscan_mark_loc_model_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache",
                         "The HyScanCache for caching internal structures",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Database",
                         "The HyScanDb containing project with waterfall marks to be displayed",
                         HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * HyScanMarkLocModel::changed:
   * @ml_model: указатель на #HyScanMarkLocModel
   *
   * Сигнал посылается при изменении списка меток.
   */
  hyscan_mark_loc_model_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed", HYSCAN_TYPE_MARK_LOC_MODEL, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
hyscan_mark_loc_model_init (HyScanMarkLocModel *mark_loc_model)
{
  mark_loc_model->priv = hyscan_mark_loc_model_get_instance_private (mark_loc_model);
}

static void
hyscan_mark_loc_model_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  HyScanMarkLocModel *mark_loc_model = HYSCAN_MARK_LOC_MODEL (object);
  HyScanMarkLocModelPrivate *priv = mark_loc_model->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_mark_loc_model_object_constructed (GObject *object)
{
  HyScanMarkLocModel *ml_model = HYSCAN_MARK_LOC_MODEL (object);
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  HyScanGeoGeodetic origin = {.lat = 0.0, .lon = 0.0, .h = 0.0};

  G_OBJECT_CLASS (hyscan_mark_loc_model_parent_class)->constructed (object);

  g_mutex_init (&priv->mutex);
  g_cond_init (&priv->cond);
  g_rw_lock_init (&priv->mark_lock);
  priv->track_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->locations = hyscan_mark_loc_model_create_table ();

  /* Модели данных. */
  priv->mark_model = hyscan_object_model_new (HYSCAN_TYPE_OBJECT_DATA_WFMARK);
  priv->db_info = hyscan_db_info_new (priv->db);
  priv->geo = hyscan_geo_new (origin, HYSCAN_GEO_ELLIPSOID_WGS84);
  priv->processor = g_thread_new ("mark-loc", hyscan_mark_loc_model_process, ml_model);

  g_signal_connect_swapped (priv->mark_model, "changed",
                            G_CALLBACK (hyscan_mark_loc_model_model_changed), ml_model);
  g_signal_connect_swapped (priv->db_info, "tracks-changed",
                            G_CALLBACK (hyscan_mark_loc_model_db_changed), ml_model);
}

static void
hyscan_mark_loc_model_object_finalize (GObject *object)
{
  HyScanMarkLocModel *mark_loc_model = HYSCAN_MARK_LOC_MODEL (object);
  HyScanMarkLocModelPrivate *priv = mark_loc_model->priv;

  /* Завершаем работу потока обработки. */
  g_mutex_lock (&priv->mutex);
  priv->state.changed |= CHANGED_SHUTDOWN;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);
  g_thread_join (priv->processor);

  g_cond_clear (&priv->cond);
  g_mutex_clear (&priv->mutex);

  /* Отключаемся от всех сигналов. */
  g_signal_handlers_disconnect_by_data (priv->mark_model, mark_loc_model);
  g_signal_handlers_disconnect_by_data (priv->db_info, mark_loc_model);

  g_rw_lock_clear (&priv->mark_lock);

  g_hash_table_destroy (priv->track_names);
  g_hash_table_destroy (priv->locations);

  g_object_unref (priv->mark_model);
  g_object_unref (priv->db_info);
  g_object_unref (priv->db);
  g_object_unref (priv->geo);
  g_object_unref (priv->cache);
  g_free (priv->project);

  G_OBJECT_CLASS (hyscan_mark_loc_model_parent_class)->finalize (object);
}

/* Возвращает коэффициент поправки на направление источника.
 * "1" - левый борт, "-1" - правый борт. */
static inline gdouble
hyscan_mark_loc_model_direction (HyScanSourceType source_type)
{
  switch (source_type)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_LOW:
    case HYSCAN_SOURCE_BATHYMETRY_STARBOARD:
      return -1;

    case HYSCAN_SOURCE_SIDE_SCAN_PORT:
    case HYSCAN_SOURCE_SIDE_SCAN_PORT_HI:
    case HYSCAN_SOURCE_SIDE_SCAN_PORT_LOW:
    case HYSCAN_SOURCE_BATHYMETRY_PORT:
      return 1;

    default:
      return 0;
    }
}

/* Определяет географические координаты метки. */
static gboolean
hyscan_mark_loc_model_load_geo (HyScanMarkLocModel *ml_model,
                                HyScanMarkLocation *location)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  HyScanGeoCartesian2D position;

  /* Положение метки относительно судна. */
  position.x = 0;
  position.y = location->offset;

  hyscan_geo_set_origin (priv->geo, location->center_geo, HYSCAN_GEO_ELLIPSOID_WGS84);
  return hyscan_geo_topoXY2geo (priv->geo, &location->mark_geo, position, 0);
}

/* Определяет время фиксации метки. */
static gboolean
hyscan_mark_loc_model_load_time (HyScanMarkLocModel *ml_model,
                                 HyScanMarkLocation *location,
                                 HyScanSourceType    source)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  const HyScanMarkWaterfall *mark = location->mark;

  gint32 project_id, track_id, channel_id;
  const gchar *acoustic_channel_name;

  acoustic_channel_name = hyscan_channel_get_id_by_types (source, HYSCAN_CHANNEL_DATA, ACOUSTIC_CHANNEL);
  project_id = hyscan_db_project_open (priv->db, priv->project);
  track_id = hyscan_db_track_open (priv->db, project_id, location->track_name);
  channel_id = hyscan_db_channel_open (priv->db, track_id, acoustic_channel_name);

  if (channel_id > 0)
    location->time = hyscan_db_channel_get_data_time (priv->db, channel_id, mark->index);
  else
    location->time = -1;

  hyscan_db_close (priv->db, channel_id);
  hyscan_db_close (priv->db, track_id);
  hyscan_db_close (priv->db, project_id);

  return location->time >= 0;
}

inline static HyScanNavData *
hyscan_mark_loc_model_rmc_data_new (HyScanMarkLocModel *ml_model,
                                    const gchar        *track_name,
                                    HyScanNMEAField     field)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  return HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                  priv->project, track_name, NMEA_RMC_CHANNEL,
                                                  HYSCAN_NMEA_DATA_RMC, field));
}

/* Находит средневзвешенное значение между lval и rval. */
static inline gdouble
hyscan_mark_loc_model_weight (gint64  mtime,
                              gint64  ltime,
                              gint64  rtime,
                              gdouble lval,
                              gdouble rval)
{
  gint64 dtime;
  gdouble rweight, lweight;

  dtime = rtime - ltime;

  if (dtime == 0)
    return lval;

  rweight = 1.0 - (gdouble) (rtime - mtime) / dtime;
  lweight = 1.0 - (gdouble) (mtime - ltime) / dtime;

  return lweight * lval + rweight * rval;
}

/* Определяет положение и курс судна в момент фиксации метки. */
static gboolean
hyscan_mark_loc_model_load_nav (HyScanMarkLocModel *ml_model,
                                HyScanMarkLocation *location)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  HyScanNavData *lat_data, *lon_data, *angle_data;

  HyScanGeoGeodetic lgeo, rgeo;
  guint32 lindex, rindex;
  gint64 ltime, rtime;

  HyScanAntennaOffset offset;
  gboolean found = FALSE;

  lat_data   = hyscan_mark_loc_model_rmc_data_new (ml_model, location->track_name, HYSCAN_NMEA_FIELD_LAT);
  lon_data   = hyscan_mark_loc_model_rmc_data_new (ml_model, location->track_name, HYSCAN_NMEA_FIELD_LON);
  angle_data = hyscan_mark_loc_model_rmc_data_new (ml_model, location->track_name, HYSCAN_NMEA_FIELD_TRACK);

  if (hyscan_nav_data_find_data (lat_data, location->time, &lindex, &rindex, &ltime, &rtime) != HYSCAN_DB_FIND_OK)
    goto exit;

  /* Пробуем распарсить навигационные данные по найденным индексам. */
  found =  hyscan_nav_data_get (lat_data, NULL, lindex, NULL, &lgeo.lat) &&
           hyscan_nav_data_get (lon_data, NULL, lindex, NULL, &lgeo.lon) &&
           hyscan_nav_data_get (angle_data, NULL, lindex, NULL, &lgeo.h) &&
           hyscan_nav_data_get (lat_data, NULL, rindex, NULL, &rgeo.lat) &&
           hyscan_nav_data_get (lon_data, NULL, rindex, NULL, &rgeo.lon) &&
           hyscan_nav_data_get (angle_data, NULL, rindex, NULL, &rgeo.h);

  if (!found) // todo: если в текущем индексе нет данных, то пробовать брать соседние
    goto exit;

  /* Ищем средневзвешенное значение. */
  location->center_geo.lat = hyscan_mark_loc_model_weight (location->time, ltime, rtime, lgeo.lat, rgeo.lat);
  location->center_geo.lon = hyscan_mark_loc_model_weight (location->time, ltime, rtime, lgeo.lon, rgeo.lon);
  location->center_geo.h   = hyscan_mark_loc_model_weight (location->time, ltime, rtime, lgeo.h, rgeo.h);

  /* Поправка на поворот датчика GPS. */
  offset = hyscan_nav_data_get_offset (lat_data);
  if (offset.psi != 0)
    location->center_geo.h += offset.psi / G_PI * 180.0;

  /* Поправка на смещение датчика GPS. */
  if (offset.x != 0 || offset.y != 0)
    {
      HyScanGeoCartesian2D shift;
      HyScanGeoGeodetic center;

      shift.x = -offset.x;
      shift.y = offset.y;

      hyscan_geo_set_origin (priv->geo, location->center_geo, HYSCAN_GEO_ELLIPSOID_WGS84);
      hyscan_geo_topoXY2geo (priv->geo, &center, shift, 0);
      location->center_geo.lat = center.lat;
      location->center_geo.lon = center.lon;
    }

exit:
  g_object_unref (angle_data);
  g_object_unref (lat_data);
  g_object_unref (lon_data);

  return found;
}

/* Определяет расстояние от метки до борта судна. */
static gboolean
hyscan_mark_loc_model_load_offset (HyScanMarkLocModel *ml_model,
                                   HyScanMarkLocation *location,
                                   HyScanSourceType    source)
{
  HyScanProjector *projector;
  HyScanAcousticData *acoustic_data;
  gdouble depth;
  gdouble direction;
  HyScanAntennaOffset amp_offset;

  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  const HyScanMarkWaterfall *mark = location->mark;

  HyScanNavData *dpt_data;

  acoustic_data = hyscan_acoustic_data_new (priv->db, priv->cache,
                                            priv->project, location->track_name,
                                            source, ACOUSTIC_CHANNEL, FALSE);

  if (acoustic_data == NULL)
    {
      g_warning ("HyScanMarkLocModel: failed to open acoustic data");
      return FALSE;
    }

  /* Добавляем поворот антенны. */
  amp_offset = hyscan_amplitude_get_offset (HYSCAN_AMPLITUDE (acoustic_data));
  if (amp_offset.psi != 0)
    location->center_geo.h -= amp_offset.psi / G_PI * 180.0;

  /* Добавляем смещение x, y. */
  if (amp_offset.x != 0 || amp_offset.y != 0)
    {
      HyScanGeoCartesian2D shift;
      HyScanGeoGeodetic center;

      shift.x = amp_offset.x;
      shift.y = -amp_offset.y;

      hyscan_geo_set_origin (priv->geo, location->center_geo, HYSCAN_GEO_ELLIPSOID_WGS84);
      hyscan_geo_topoXY2geo (priv->geo, &center, shift, 0);
      location->center_geo.lat = center.lat;
      location->center_geo.lon = center.lon;
    }

  /* Находим глубину при возможности. Делаем поправки на заглубление эхолота и ГБО. */
  dpt_data =  HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                       priv->project, location->track_name, NMEA_DPT_CHANNEL,
                                                       HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH));
  if (dpt_data != NULL)
    {
      HyScanDepthometer *depthometer;
      HyScanAntennaOffset  dpt_offset;

      dpt_offset = hyscan_nav_data_get_offset (dpt_data);
      depthometer = hyscan_depthometer_new (dpt_data, priv->cache);
      depth = hyscan_depthometer_get (depthometer, NULL, location->time);
      if (depth < 0)
        depth = 0;

      depth += dpt_offset.z;
    }
  else
    {
      depth = 0;
    }
  depth -= amp_offset.z;

  /* Определяем дистанцию до точки. */
  direction = hyscan_mark_loc_model_direction (source);
  if (direction == 0)
    {
      location->offset = 0.0;
    }
  else
    {
      projector = hyscan_projector_new (HYSCAN_AMPLITUDE (acoustic_data));
      hyscan_projector_count_to_coord (projector, mark->count, &location->offset, depth);
      location->offset *= direction;
      g_clear_object (&projector);
    }

  g_clear_object (&dpt_data);
  g_clear_object (&acoustic_data);

  return TRUE;
}

/* Загружает геолокационные данные по метке. */
static HyScanMarkLocation *
hyscan_mark_loc_model_load (HyScanMarkLocModel  *ml_model,
                            HyScanMarkWaterfall *mark)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  HyScanMarkLocation *location;

  HyScanSourceType source;
  const gchar *track_name;

  location = hyscan_mark_location_new ();
  location->mark = mark;

  /* Получаем название галса. */
  track_name = mark->track != NULL ? g_hash_table_lookup (priv->track_names, mark->track) : NULL;
  if (track_name == NULL)
    return location;

  location->track_name = g_strdup (track_name);

  source = hyscan_source_get_type_by_id (mark->source);
  if (source == HYSCAN_SOURCE_INVALID)
    return location;

  location->loaded = hyscan_mark_loc_model_load_time (ml_model, location, source) &&
                     hyscan_mark_loc_model_load_nav (ml_model, location) &&
                     hyscan_mark_loc_model_load_offset (ml_model, location, source) &&
                     hyscan_mark_loc_model_load_geo (ml_model, location);

  return location;
}

/* Обработчик сигнала HyScanDBInfo::tracks-changed. */
static void
hyscan_mark_loc_model_db_changed (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  g_mutex_lock (&priv->mutex);
  priv->state.changed |= CHANGED_TRACKS;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);
}

static gboolean
hyscan_mark_loc_model_emit_changed (gpointer data)
{
  HyScanMarkLocModel *ml_model = HYSCAN_MARK_LOC_MODEL (data);

  g_signal_emit (ml_model, hyscan_mark_loc_model_signals[SIGNAL_CHANGED], 0);

  return G_SOURCE_REMOVE;
}

static gpointer
hyscan_mark_loc_model_process (gpointer data)
{
  HyScanMarkLocModel *ml_model = HYSCAN_MARK_LOC_MODEL (data);
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  HyScanMarkLocModelState new_state;
  HyScanMarkLocModelState *state = &priv->state;

  memset (&new_state, 0, sizeof (new_state));

  /* В цикле копируем текущее состояния из основного потока и проверяем, что изменилось. */
  while (!(new_state.changed & CHANGED_SHUTDOWN))
    {
      {
        g_mutex_lock (&priv->mutex);

        while (state->changed == CHANGED_NONE)
          g_cond_wait (&priv->cond, &priv->mutex);

        new_state = *state;
        memset (state, 0, sizeof (*state));

        g_mutex_unlock (&priv->mutex);
      }

      if (new_state.changed & CHANGED_PROJECT)
        {
          g_free (priv->project);
          priv->project = new_state.project;
          hyscan_db_info_set_project (priv->db_info, priv->project);
          hyscan_object_model_set_project (priv->mark_model, priv->db, priv->project);

          new_state.changed |= CHANGED_TRACKS;
        }

      /* Составляем таблицу соответствия ИД — название по каждому галсу. */
      if (new_state.changed & CHANGED_TRACKS)
        {
          GHashTable *tracks;
          GHashTableIter iter;
          HyScanTrackInfo *track_info;

          tracks = hyscan_db_info_get_tracks (priv->db_info);

          /* Загружаем информацию по названиям галсов и их ИД. */
          g_hash_table_remove_all (priv->track_names);
          g_hash_table_iter_init (&iter, tracks);
          while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_info))
            {
              if (track_info->id == NULL)
                continue;

              g_hash_table_insert (priv->track_names, g_strdup (track_info->id), g_strdup (track_info->name));
            }

          g_hash_table_destroy (tracks);

          new_state.changed |= CHANGED_MARKS;
        }

      /* Загружаем список меток и их местоположение. */
      if (new_state.changed & CHANGED_MARKS)
        {
          GHashTable *wfmarks;
          GHashTable *new_locations, *prev_locations;

          new_locations = hyscan_mark_loc_model_create_table ();

          /* Получаем список меток. */
          wfmarks = hyscan_object_model_get (priv->mark_model);
          if (wfmarks != NULL)
            {
              GHashTableIter iter;
              gchar *key;
              HyScanMarkWaterfall *mark;
              HyScanMarkLocation *location;

              /* Загружаем гео-данные по меткам. */
              g_hash_table_iter_init (&iter, wfmarks);
              while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &mark))
                {
                  g_hash_table_iter_steal (&iter);

                  location = hyscan_mark_loc_model_load (ml_model, mark);
                  g_hash_table_insert (new_locations, key, location);
                }

              g_hash_table_unref (wfmarks);
            }

          g_rw_lock_writer_lock (&priv->mark_lock);
          prev_locations = priv->locations;
          priv->locations = new_locations;
          g_rw_lock_writer_unlock (&priv->mark_lock);

          g_hash_table_unref (prev_locations);

          g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, hyscan_mark_loc_model_emit_changed,
                           g_object_ref (ml_model), g_object_unref);
        }
    }

  return NULL;
}

/* Обработчик сигнала HyScanMarkModel::changed. */
static void
hyscan_mark_loc_model_model_changed (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  g_mutex_lock (&priv->mutex);
  priv->state.changed |= CHANGED_MARKS;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);
}

static GHashTable *
hyscan_mark_loc_model_create_table (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_mark_location_free);
}

/**
 * hyscan_mark_loc_model_new:
 * @db: указатель на базу данных #HyScanDb
 * @cache: кэш
 *
 * Создаёт модель данных положений меток.
 */
HyScanMarkLocModel *
hyscan_mark_loc_model_new (HyScanDB    *db,
                           HyScanCache *cache)
{
  return g_object_new (HYSCAN_TYPE_MARK_LOC_MODEL,
                       "db", db,
                       "cache", cache, NULL);
}

/**
 * hyscan_mark_loc_model_set_project:
 * @ml_model: указатель на #HyScanMarkLocModel
 * @project: название проекта
 *
 * Устанавливает название проекта
 */
void
hyscan_mark_loc_model_set_project (HyScanMarkLocModel *ml_model,
                                   const gchar        *project)
{
  HyScanMarkLocModelPrivate *priv;

  g_return_if_fail (HYSCAN_IS_MARK_LOC_MODEL (ml_model));
  priv = ml_model->priv;

  g_mutex_lock (&priv->mutex);
  g_free (priv->state.project);
  priv->state.project = g_strdup (project);
  priv->state.changed |= CHANGED_PROJECT;
  g_cond_signal (&priv->cond);
  g_mutex_unlock (&priv->mutex);
}

/**
 * hyscan_mark_loc_model_get:
 * @ml_model: указатель на #HyScanMarkLocModel
 *
 * Returns: (element-type HyScanMarkLocation): хэш-таблица со списком положений
 *   меток #HyScanMarkLocation. Для удаления g_hash_table_unref().
 */
GHashTable *
hyscan_mark_loc_model_get (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv;
  GHashTable *marks;
  GHashTableIter iter;
  const HyScanMarkLocation *mark;
  const gchar *key;

  g_return_val_if_fail (HYSCAN_IS_MARK_LOC_MODEL (ml_model), NULL);
  priv = ml_model->priv;

  marks = hyscan_mark_loc_model_create_table ();

  g_rw_lock_reader_lock (&priv->mark_lock);

  g_hash_table_iter_init (&iter, priv->locations);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &mark))
    g_hash_table_insert (marks, g_strdup (key), hyscan_mark_location_copy (mark));

  g_rw_lock_reader_unlock (&priv->mark_lock);

  return marks;
}

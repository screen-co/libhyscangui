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
 * @Short_description: Модель местоположения меток
 * @Title: HyScanMarkLocModel
 * @See_also: #HyScanMarkLocation, #HyScanMarkModel
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
#include <hyscan-mark-model.h>
#include <hyscan-projector.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-nmea-parser.h>

#define ACOUSTIC_CHANNEL        1                    /* Канал с акустическими данными. */
#define NMEA_RMC_CHANNEL        1                    /* Канал с навигационными данными. */

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

struct _HyScanMarkLocModelPrivate
{
  HyScanDB        *db;                          /* База данных. */
  HyScanCache     *cache;                       /* Кэш. */
  gchar           *project;                     /* Название проекта. */

  HyScanDBInfo    *db_info;                     /* Модель галсов в БД. */
  HyScanMarkModel *mark_model;                  /* Модель меток. */

  GRWLock          mark_lock;                   /* Блокировка данных по меткам. .*/
  GHashTable      *track_names;                 /* Хэш-таблица соотвествтия id галса с его названием.*/
  GHashTable      *locations;                   /* Хэш-таблица с метками HyScanMarkLocation.*/
};

static void         hyscan_mark_loc_model_set_property             (GObject               *object,
                                                                    guint                  prop_id,
                                                                    const GValue          *value,
                                                                    GParamSpec            *pspec);
static void         hyscan_mark_loc_model_object_constructed       (GObject               *object);
static void         hyscan_mark_loc_model_object_finalize          (GObject               *object);
static void         hyscan_mark_loc_model_db_changed               (HyScanMarkLocModel    *ml_model);
static void         hyscan_mark_loc_model_model_changed            (HyScanMarkLocModel    *ml_model);
static GHashTable * hyscan_mark_loc_model_create_table             (void);

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

  /* Remove this call then class is derived from GObject.
     This call is strongly needed then class is derived from GtkWidget. */
  G_OBJECT_CLASS (hyscan_mark_loc_model_parent_class)->constructed (object);

  g_rw_lock_init (&priv->mark_lock);
  priv->track_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->locations = hyscan_mark_loc_model_create_table ();

  /* Модели данных. */
  priv->mark_model = hyscan_mark_model_new ();
  priv->db_info = hyscan_db_info_new (priv->db);

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

  g_rw_lock_clear (&priv->mark_lock);

  g_hash_table_destroy (priv->track_names);
  g_hash_table_destroy (priv->locations);

  g_object_unref (priv->mark_model);
  g_object_unref (priv->db_info);
  g_object_unref (priv->db);
  g_object_unref (priv->cache);
  g_free (priv->project);

  G_OBJECT_CLASS (hyscan_mark_loc_model_parent_class)->finalize (object);
}

/* Возвращает %TRUE, если источник данных соответствует правому борту. */
static inline gboolean
hyscan_mark_loc_model_is_starboard (HyScanSourceType source_type)
{
  switch (source_type)
    {
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_HI:
    case HYSCAN_SOURCE_SIDE_SCAN_STARBOARD_LOW:
      return TRUE;

    default:
      return FALSE;
    }
}

/* Загружает геолокационные данные по метке.
 * Функция должна вызываться за g_rw_lock_reader_lock (&priv->mark_lock). */
static gboolean
hyscan_mark_loc_model_load_location (HyScanMarkLocModel *ml_model,
                                     HyScanMarkLocation *location)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  const HyScanWaterfallMark *mark = location->mark;
  const gchar *track_name;

  location->loaded = FALSE;

  /* Получаем название галса. */
  track_name = g_hash_table_lookup (priv->track_names, mark->track);
  if (track_name == NULL)
    return location->loaded;
  // todo: Cache objects: amplitude, projector, lat_data, lon_data...
  
  /* Получаем временную метку из канала акустических данных. */
  {
    gint32 project_id, track_id, channel_id;
    const gchar *acoustic_channel_name;

    acoustic_channel_name = hyscan_channel_get_name_by_types (mark->source0, HYSCAN_CHANNEL_DATA, ACOUSTIC_CHANNEL);
    project_id = hyscan_db_project_open (priv->db, priv->project);
    track_id = hyscan_db_track_open (priv->db, project_id, track_name);
    channel_id = hyscan_db_channel_open (priv->db, track_id, acoustic_channel_name);

    location->time = hyscan_db_channel_get_data_time (priv->db, channel_id, mark->index0);

    hyscan_db_close (priv->db, channel_id);
    hyscan_db_close (priv->db, track_id);
    hyscan_db_close (priv->db, project_id);
  }
  
  /* Находим географические координаты метки и курс. */
  {
    HyScanNavData *lat_data, *lon_data, *angle_data;
    guint32 lindex, rindex;
    HyScanGeoGeodetic lgeo, rgeo;
    gint64 ltime, rtime;

    // todo: определять, какой номер канала с навигационными данными. Сейчас захардкожен "NMEA_RMC_CHANNEL"
    lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                        priv->project, track_name, NMEA_RMC_CHANNEL,
                                                        HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
    lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                        priv->project, track_name, NMEA_RMC_CHANNEL,
                                                        HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
    angle_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache,
                                                          priv->project, track_name, NMEA_RMC_CHANNEL,
                                                          HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));

    if (hyscan_nav_data_find_data (lat_data, location->time, &lindex, &rindex, &ltime, &rtime) == HYSCAN_DB_FIND_OK)
      {
        gboolean found;

        /* Пробуем распарсить навигационные данные по найденным индексам. */
        found =  hyscan_nav_data_get (lat_data, lindex, NULL, &lgeo.lat) &&
                 hyscan_nav_data_get (lon_data, lindex, NULL, &lgeo.lon) &&
                 hyscan_nav_data_get (angle_data, lindex, NULL, &lgeo.h);
        found &=  hyscan_nav_data_get (lat_data, rindex, NULL, &rgeo.lat) &&
                  hyscan_nav_data_get (lon_data, rindex, NULL, &rgeo.lon) &&
                  hyscan_nav_data_get (angle_data, rindex, NULL, &rgeo.h);

        if (found)
          {
            gint64 dtime = rtime - ltime;

            /* Ищем средневзвешанное значение. */
            if (dtime > 0)
              {
                gdouble rweight, lweight;

                rweight = (gdouble) (rtime - location->time) / dtime;
                lweight = (gdouble) (location->time - ltime) / dtime;

                location->center_geo.lat = lweight * lgeo.lat + rweight * rgeo.lat;
                location->center_geo.lon = lweight * lgeo.lon + rweight * rgeo.lon;
                location->center_geo.h = lweight * lgeo.h + rweight * rgeo.h;
              }
            else
              {
                location->center_geo = lgeo;
              }

            location->loaded = TRUE;
          }
        else
          {
            // todo: если в текущем индексе нет данных, то пробовать брать соседние
            g_warning ("HyScanMarkLocModel: mark location was not found");
          }
      }

    g_object_unref (angle_data);
    g_object_unref (lat_data);
    g_object_unref (lon_data);
  }

  /* Определяем дальность расположения метки от тракетории. */
  {
    HyScanProjector *projector;
    HyScanAcousticData *acoustic_data;


    acoustic_data = hyscan_acoustic_data_new (priv->db, priv->cache,
                                              priv->project, track_name,
                                              mark->source0, ACOUSTIC_CHANNEL, FALSE);
    projector = hyscan_projector_new (HYSCAN_AMPLITUDE (acoustic_data));
    hyscan_projector_count_to_coord (projector, mark->count0, &location->offset, 0);
    if (hyscan_mark_loc_model_is_starboard(mark->source0))
      location->offset *= -1.0;

    g_object_unref (acoustic_data);
    g_object_unref (projector);
  }

  return location->loaded;
}

/* Перезагружает данные priv->marks.
 * Функция должна вызываться за g_rw_lock_writer_lock (&priv->mark_lock). */
static void
hyscan_mark_loc_model_reload_locations (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;

  GHashTableIter iter;
  HyScanMarkLocation *location;

  g_hash_table_iter_init (&iter, priv->locations);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &location))
    hyscan_mark_loc_model_load_location (ml_model, location);
}

/* Обработчик сигнала HyScanDBInfo::tracks-changed.
 * Составляет таблицу соответствия ИД - название по каждому галсу. */
static void
hyscan_mark_loc_model_db_changed (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  GHashTable *tracks;
  GHashTableIter iter;
  HyScanTrackInfo *track_info;

  tracks = hyscan_db_info_get_tracks (priv->db_info);

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Загружаем информацию по названиям галсов и их ИД. */
  g_hash_table_remove_all (priv->track_names);
  g_hash_table_iter_init (&iter, tracks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &track_info))
    g_hash_table_insert (priv->track_names, g_strdup (track_info->id), g_strdup (track_info->name));

  /* Перезагружаем геолокационную информацию. */
  hyscan_mark_loc_model_reload_locations (ml_model);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  g_hash_table_destroy (tracks);

  g_signal_emit (ml_model, hyscan_mark_loc_model_signals[SIGNAL_CHANGED], 0);
}

static gboolean
hyscan_mark_loc_model_model_append_mark (gpointer key,
                                         gpointer value,
                                         gpointer user_data)
{
  HyScanMarkLocModel *ml_model = HYSCAN_MARK_LOC_MODEL (user_data);
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  HyScanMarkLocation *location;

  location = hyscan_mark_location_new ();
  location->mark = value;

  g_hash_table_insert (priv->locations, key, location);

  return TRUE;
}

/* Обработчик сигнала HyScanMarkModel::changed.
 * Обновляет список меток. */
static void
hyscan_mark_loc_model_model_changed (HyScanMarkLocModel *ml_model)
{
  HyScanMarkLocModelPrivate *priv = ml_model->priv;
  GHashTable *wfmarks;

  /* Получаем список меток. */
  wfmarks = hyscan_mark_model_get (priv->mark_model);

  g_rw_lock_writer_lock (&priv->mark_lock);

  /* Загружаем гео-данные по меткам. */
  // todo: сделать обновление текущего списка меток priv->marks, а не полную перезагрузку
  g_hash_table_remove_all (priv->locations);
  g_hash_table_foreach_steal (wfmarks, hyscan_mark_loc_model_model_append_mark, ml_model);
  hyscan_mark_loc_model_reload_locations (ml_model);

  g_rw_lock_writer_unlock (&priv->mark_lock);

  g_hash_table_unref (wfmarks);

  g_signal_emit (ml_model, hyscan_mark_loc_model_signals[SIGNAL_CHANGED], 0);
}

static GHashTable *
hyscan_mark_loc_model_create_table (void)
{
  return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) hyscan_mark_location_free);
}

/**
 * hyscan_mark_loc_model_new:
 * @db
 * @cache
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
 * @ml_model
 * @project
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

  g_free (priv->project);
  priv->project = g_strdup (project);

  hyscan_db_info_set_project (priv->db_info, priv->project);
  hyscan_mark_model_set_project (priv->mark_model, priv->db, priv->project);
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

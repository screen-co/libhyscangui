/* hyscan-gtk-map-track.c
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
 * SECTION: hyscan-gtk-map-track
 * @Short_description: Изображение галса на карте
 * @Title: HyScanGtkMapTrack
 *
 * HyScanGtkMapTrack позволяет изображать галс на карте в виде линии движения
 * судна и дальности обнаружения по каждому борту.
 *
 * Функции:
 * - hyscan_gtk_map_track_new() - создает новый объект;
 * - hyscan_gtk_map_track_set_projection() - устанавлиает картографическую проекцию;
 * - hyscan_gtk_map_track_update() - проверяет и загруажет новые данные по галсу;
 * - hyscan_gtk_map_track_draw() - рисует галс на указанном участке;
 * - hyscan_gtk_map_track_view() - определяет границы галса.
 *
 * Кроме того #HyScanGtkMapTrack реализует интерфейс #HyScanParam, с помощью
 * которого можно установить каналы данных, используемые для загрузки информации
 * по галсу.
 *
 */

#include "hyscan-gtk-map-track.h"
#include <hyscan-acoustic-data.h>
#include <hyscan-amplitude.h>
#include <hyscan-cartesian.h>
#include <hyscan-core-common.h>
#include <hyscan-depthometer.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-projector.h>
#include <hyscan-data-schema-builder.h>
#include <glib/gi18n.h>
#include <cairo.h>
#include <math.h>

/* Ключи схемы данных настроек галса. */
#define ENUM_NMEA_CHANNEL     "nmea-channel"
#define KEY_CHANNEL_RMC       "/channel-rmc"
#define KEY_CHANNEL_DPT       "/channel-dpt"
#define KEY_CHANNEL_PORT      "/channel-port"
#define KEY_CHANNEL_STARBOARD "/channel-starboard"

/* Номера каналов по умолчанию. */
#define DEFAULT_CHANNEL_RMC       1
#define DEFAULT_CHANNEL_DPT       2
#define DEFAULT_CHANNEL_PORT      1
#define DEFAULT_CHANNEL_STARBOARD 1

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_NAME,
  PROP_CACHE,
  PROP_PROJECTION,
  PROP_TILED_LAYER,
};

/* Каналы данных для изображения галса. */
enum
{
  CHANNEL_NMEA_RMC,
  CHANNEL_NMEA_DPT,
  CHANNEL_PORT,
  CHANNEL_STARBOARD,
};

typedef struct {
  HyScanGeoGeodetic               geo;              /* Географические координаты точки и курс движения. */
  guint32                         index;            /* Индекс записи в канале данных. */
  gint64                          time;             /* Время фиксации. */
  HyScanDBFindStatus              l_find_status;    /* Статус поиска дальности по левому борту. */
  HyScanDBFindStatus              r_find_status;    /* Статус поиска дальности по правому борту. */
  gdouble                         l_width;          /* Дальность по левому борту, метры. */
  gdouble                         r_width;          /* Дальность по правому борту, метры. */

  /* Координаты на картографической проекции (зависят от текущей проекции). */
  HyScanGeoCartesian2D            c2d;              /* Координаты точки галса на проекции. */
  HyScanGeoCartesian2D            l_c2d;            /* Координаты конца дальности по левому борту. */
  HyScanGeoCartesian2D            r_c2d;            /* Координаты конца дальности по правому борту. */
  gdouble                         dist;             /* Расстояние от начала галса. */
  gdouble                         l_dist;           /* Дальность по левому борту, единицы проекции. */
  gdouble                         r_dist;           /* Дальность по правому борту, единицы проекции. */
} HyScanGtkMapTrackLayerPoint;

typedef struct {
  HyScanAmplitude                *amplitude;         /* Амплитудные данные трека. */
  HyScanProjector                *projector;         /* Сопоставления индексов и отсчётов реальным координатам. */
} HyScanGtkMapTrackLayerBoard;

struct _HyScanGtkMapTrackPrivate
{
  HyScanDB                       *db;                /* База данных. */
  HyScanCache                    *cache;             /* Кэш. */
  HyScanGtkMapTiledLayer         *tiled_layer;       /* Тайловый слой, на котором размещён галс. */
  HyScanGeoProjection            *projection;        /* Картографическая проекция. */
  HyScanDataSchema               *schema;            /* Схема параметров галса (номера каналов данных). */

  gchar                          *project;           /* Название проекта. */
  gchar                          *name;              /* Название галса. */

  gboolean                        opened;            /* Признак того, что каналы галса открыты. */
  gboolean                        loaded;            /* Признак того, что данные галса загружены. */
  gboolean                        writeable;         /* Признак того, что галс открыт для записи. */
  GRWLock                         lock;              /* Блокировка доступа к точкам галса. */

  /* Каналы данных. */
  guint                           channel_starboard; /* Номер канала правого борта. */
  guint                           channel_port;      /* Номер канала левого борта. */
  guint                           channel_rmc;       /* Номер канала nmea с RMC. */
  guint                           channel_dpt;       /* Номер канала nmea с DPT. */

  HyScanGtkMapTrackLayerBoard     port;              /* Данные левого борта. */
  HyScanGtkMapTrackLayerBoard     starboard;         /* Данные правого борта. */
  HyScanDepthometer              *depthometer;       /* Определение глубины. */
  HyScanNavData                  *lat_data;          /* Навигационные данные - широта. */
  HyScanNavData                  *lon_data;          /* Навигационные данные - долгота. */
  HyScanNavData                  *angle_data;        /* Навигационные данные - курс в градусах. */

  guint32                         lat_mod_count;     /* Mod-count канала навигационных данных. */
  guint32                         first_index;       /* Первый индекс в канале навигационных данных. */
  guint32                         last_index;        /* Последний индекс в канале навигационных данных. */

  GList                          *points;            /* Список точек трека HyScanGtkMapTrackLayerPoint. */
};

static void    hyscan_gtk_map_track_interface_init     (HyScanParamInterface  *iface);
static void    hyscan_gtk_map_track_set_property       (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_gtk_map_track_schema_build       (HyScanGtkMapTrack     *track);
static guint   hyscan_gtk_map_track_get_channel        (HyScanGtkMapTrack     *track,
                                                        guint                  channel);
static void    hyscan_gtk_map_track_set_channel_real   (HyScanGtkMapTrack     *track,
                                                        guint                  channel,
                                                        guint                  channel_num);
static void    hyscan_gtk_map_track_set_channel        (HyScanGtkMapTrack     *track,
                                                        guint                  channel,
                                                        guint                  channel_num);
static void    hyscan_gtk_map_track_object_constructed (GObject               *object);
static void    hyscan_gtk_map_track_object_finalize    (GObject               *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrack, hyscan_gtk_map_track, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrack)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_gtk_map_track_interface_init))

static void
hyscan_gtk_map_track_class_init (HyScanGtkMapTrackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_set_property;

  object_class->constructed = hyscan_gtk_map_track_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_object_finalize;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Database", "The HyScanDB for reading track data", HYSCAN_TYPE_DB,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "The HyScanCache for internal structures", HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECTION,
    g_param_spec_object ("projection", "Map projection", "The projection of track coordinates",
                         HYSCAN_TYPE_GEO_PROJECTION,
                         G_PARAM_WRITABLE));
  g_object_class_install_property (object_class, PROP_TILED_LAYER,
    g_param_spec_object ("tiled-layer", "Tiled layer", "The HyScanGtkMapTiledLayer responsible for view updates",
                         HYSCAN_TYPE_GTK_MAP_TILED_LAYER,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project name", "The project containing track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Track name", "The name of the track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_init (HyScanGtkMapTrack *gtk_map_track)
{
  gtk_map_track->priv = hyscan_gtk_map_track_get_instance_private (gtk_map_track);
}

static void
hyscan_gtk_map_track_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = track->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECTION:
      hyscan_gtk_map_track_set_projection (track, g_value_get_object (value));
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    case PROP_TILED_LAYER:
      priv->tiled_layer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_track_object_constructed (GObject *object)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = track->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_track_parent_class)->constructed (object);

  g_rw_lock_init (&priv->lock);

  /* Устанавливаем номера каналов по умолчанию. */
  hyscan_gtk_map_track_schema_build (track);
  hyscan_gtk_map_track_set_channel_real (track, CHANNEL_NMEA_RMC,  DEFAULT_CHANNEL_RMC);
  hyscan_gtk_map_track_set_channel_real (track, CHANNEL_NMEA_DPT,  DEFAULT_CHANNEL_DPT);
  hyscan_gtk_map_track_set_channel_real (track, CHANNEL_STARBOARD, DEFAULT_CHANNEL_STARBOARD);
  hyscan_gtk_map_track_set_channel_real (track, CHANNEL_PORT,      DEFAULT_CHANNEL_PORT);
}

static void
hyscan_gtk_map_track_object_finalize (GObject *object)
{
  HyScanGtkMapTrack *gtk_map_track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = gtk_map_track->priv;

  g_rw_lock_clear (&priv->lock);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->tiled_layer);
  g_clear_object (&priv->projection);
  g_clear_object (&priv->schema);

  g_free (priv->project);
  g_free (priv->name);

  g_clear_object (&priv->port.amplitude);
  g_clear_object (&priv->port.projector);
  g_clear_object (&priv->starboard.amplitude);
  g_clear_object (&priv->starboard.projector);

  g_clear_object (&priv->depthometer);
  g_clear_object (&priv->lat_data);
  g_clear_object (&priv->lon_data);
  g_clear_object (&priv->angle_data);

  g_list_free_full (priv->points, g_free);

  G_OBJECT_CLASS (hyscan_gtk_map_track_parent_class)->finalize (object);
}

/* Реализация HyScanParamInterface.get.
 * Получает параметры галса. */
static gboolean
hyscan_gtk_map_track_param_get (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (param);
  const gchar * const * names;
  gint i;

  names = hyscan_param_list_params (list);
  if (names == NULL)
    return FALSE;

  for (i = 0; names[i] != NULL; ++i)
    {
      gint channel;

      if (g_str_equal (names[i], KEY_CHANNEL_RMC))
        {
          channel = hyscan_gtk_map_track_get_channel (track, CHANNEL_NMEA_RMC);
          hyscan_param_list_set_enum (list, names[i], channel);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_DPT))
        {
          channel = hyscan_gtk_map_track_get_channel (track, CHANNEL_NMEA_DPT);
          hyscan_param_list_set_enum (list, names[i], channel);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_PORT))
        {
          channel = hyscan_gtk_map_track_get_channel (track, CHANNEL_PORT);
          hyscan_param_list_set_boolean (list, names[i], channel > 0);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_STARBOARD))
        {
          channel = hyscan_gtk_map_track_get_channel (track, CHANNEL_STARBOARD);
          hyscan_param_list_set_boolean (list, names[i], channel > 0);
        }
    }

  return TRUE;
}

/* Реализация HyScanParamInterface.set.
 * Устанавливает параметры галса. */
static gboolean
hyscan_gtk_map_track_param_set (HyScanParam     *param,
                                HyScanParamList *list)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (param);
  HyScanGtkMapTrackPrivate *priv = track->priv;
  const gchar * const * names;
  gint i;

  names = hyscan_param_list_params (list);
  if (names == NULL)
    return FALSE;

  for (i = 0; names[i] != NULL; ++i)
    {
      guint channel;
      gint channel_num;
      
      if (g_str_equal (names[i], KEY_CHANNEL_RMC))
        {
          channel = CHANNEL_NMEA_RMC;
          channel_num = hyscan_param_list_get_integer (list, names[i]);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_DPT))
        {
          channel = CHANNEL_NMEA_DPT;
          channel_num = hyscan_param_list_get_integer (list, names[i]);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_STARBOARD))
        {
          channel = CHANNEL_STARBOARD;
          channel_num = hyscan_param_list_get_boolean (list, names[i]) ? DEFAULT_CHANNEL_STARBOARD : 0;
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_PORT))
        {
          channel = CHANNEL_PORT;
          channel_num = hyscan_param_list_get_boolean (list, names[i]) ? DEFAULT_CHANNEL_PORT : 0;
        }
      else
        {
          continue;
        }
      
      hyscan_gtk_map_track_set_channel (track, channel, channel_num);
    }

  /* Сообщаем об изменнении параметров и необходимости перерисовки. */
  hyscan_gtk_map_tiled_layer_set_param_mod (priv->tiled_layer);
  hyscan_gtk_map_tiled_layer_request_draw (priv->tiled_layer);

  return TRUE;
}

/* Реализация HyScanParamInterface.schema.
 * Возвращает схему параметров галса. */
static HyScanDataSchema *
hyscan_gtk_map_track_param_schema (HyScanParam *param)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (param);
  HyScanGtkMapTrackPrivate *priv = track->priv;

  return g_object_ref (priv->schema);
}

static void
hyscan_gtk_map_track_interface_init (HyScanParamInterface *iface)
{
  iface->get = hyscan_gtk_map_track_param_get;
  iface->set = hyscan_gtk_map_track_param_set;
  iface->schema = hyscan_gtk_map_track_param_schema;
}

/* Возвращает максимальный доступный номер канала для указанного источника данных. */
static guint
hyscan_gtk_map_track_max_channel (HyScanGtkMapTrack *track,
                                  guint              channel)
{
  HyScanGtkMapTrackPrivate *priv;
  guint max_channel_num = 0;
  gint i;
  gchar **channel_list;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track), 0);
  priv = track->priv;

  gint32 project_id, track_id;

  project_id = hyscan_db_project_open (priv->db, priv->project);
  track_id = hyscan_db_track_open (priv->db, project_id, priv->name);
  channel_list = hyscan_db_channel_list (priv->db, track_id);

  if (channel_list == NULL)
    goto exit;

  for (i = 0; channel_list[i] != NULL; ++i)
    {
      guint channel_num;
      HyScanSourceType source;
      HyScanChannelType type;

      hyscan_channel_get_types_by_id (channel_list[i], &source, &type, &channel_num);

      /* Отфильтровываем каналы по типу данных. */
      switch (channel)
        {
        case CHANNEL_NMEA_RMC:
        case CHANNEL_NMEA_DPT:
          if (source != HYSCAN_SOURCE_NMEA)
            continue;
          break;

        case CHANNEL_PORT:
          if (source != HYSCAN_SOURCE_SIDE_SCAN_PORT)
            continue;
          break;

        case CHANNEL_STARBOARD:
          if (source != HYSCAN_SOURCE_SIDE_SCAN_STARBOARD)
            continue;
          break;

        default:
          continue;
        }

      max_channel_num = MAX (max_channel_num, channel_num);
    }

exit:
  g_clear_pointer (&channel_list, g_strfreev);
  hyscan_db_close (priv->db, track_id);
  hyscan_db_close (priv->db, project_id);

  return max_channel_num;
}

static void
hyscan_gtk_map_track_schema_build_nmea_enum (HyScanGtkMapTrack       *track,
                                             HyScanDataSchemaBuilder *builder,
                                             guint                   *channel_last)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;

  gint32 project_id = -1, track_id = -1;
  gchar **channels;

  guint max_channel = 0;
  guint i;

  /* Создаём перечисление и "пустое" значение. */
  hyscan_data_schema_builder_enum_create (builder, ENUM_NMEA_CHANNEL);
  hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, 0, "disabled", "Disabled", NULL);

  /* Получаем из БД информацию о каналах галса. */
  project_id = hyscan_db_project_open (priv->db, priv->project);
  if (project_id < 0)
    goto exit;

  track_id = hyscan_db_track_open (priv->db, project_id, priv->name);
  if (track_id < 0)
    goto exit;

  channels = hyscan_db_channel_list (priv->db, track_id);

  if (channels == NULL)
    goto exit;

  for (i = 0; channels[i] != NULL; ++i)
    {
      gchar *sensor_name;
      gint32 channel_id, param_id;

      HyScanSourceType source;
      HyScanChannelType type;
      guint channel_num;

      hyscan_channel_get_types_by_id (channels[i], &source, &type, &channel_num);
      if (source != HYSCAN_SOURCE_NMEA)
        continue;

      channel_id = hyscan_db_channel_open (priv->db, track_id, channels[i]);
      if (channel_id < 0)
        continue;

      param_id = hyscan_db_channel_param_open (priv->db, channel_id);
      hyscan_db_close (priv->db, channel_id);

      if (param_id < 0)
        continue;

      sensor_name = hyscan_core_params_load_sensor_info (priv->db, param_id);
      hyscan_db_close (priv->db, param_id);

      /* Добавляем канал в enum. */
      hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, channel_num, channels[i], sensor_name, NULL);
      max_channel = MAX (max_channel, channel_num);

      g_free (sensor_name);
    }

exit:
  g_clear_pointer (&channels, g_strfreev);

  if (track_id > 0)
    hyscan_db_close (priv->db, track_id);

  if (project_id > 0)
    hyscan_db_close (priv->db, project_id);

  if (channel_last != NULL)
    *channel_last = max_channel;
}

/* Создаёт схему параметров галса. */
static void
hyscan_gtk_map_track_schema_build (HyScanGtkMapTrack *track)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  HyScanDataSchemaBuilder *builder;
  guint max_channel;

  builder = hyscan_data_schema_builder_new ("gtk-map-track");
  hyscan_data_schema_builder_node_set_name (builder, "/", "Track settings", "Configure track channel data");

  /* Формируем перечисление NMEA-каналов. */
  hyscan_gtk_map_track_schema_build_nmea_enum (track, builder, &max_channel);

  /* Настройка канала RMC-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_RMC, "RMC Channel",
                                              "The NMEA-channel with RMC sentences",
                                              ENUM_NMEA_CHANNEL, MIN (DEFAULT_CHANNEL_RMC, max_channel));

  /* Настройка канала DPT-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_DPT, "DPT Channel",
                                              "The NMEA-channel with DPT sentences",
                                              ENUM_NMEA_CHANNEL, MIN (DEFAULT_CHANNEL_DPT, max_channel));

  /* Настройка левого борта. */
  max_channel = hyscan_gtk_map_track_max_channel (track, CHANNEL_PORT);
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_PORT, "Port Channel",
                                                 "Show side-scan port channel data", max_channel > 0);
  if (max_channel == 0)
    hyscan_data_schema_builder_key_set_access (builder, KEY_CHANNEL_PORT, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Настройка правого борта. */
  max_channel = hyscan_gtk_map_track_max_channel (track, CHANNEL_STARBOARD);
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_STARBOARD, "Starboard Channel",
                                                 "Show side-scan starboard channel data", max_channel > 0);
  if (max_channel == 0)
    hyscan_data_schema_builder_key_set_access (builder, KEY_CHANNEL_STARBOARD, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Записываем полученную схему данных. */
  g_clear_object (&priv->schema);
  priv->schema = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);
}

/* Устанавливает номер канала для указанного трека.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_set_channel_real (HyScanGtkMapTrack *track,
                                       guint              channel,
                                       guint              channel_num)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  guint max_channel;

  max_channel = hyscan_gtk_map_track_max_channel (track, channel);
  channel_num = MIN (channel_num, max_channel);

  switch (channel)
    {
    case CHANNEL_NMEA_DPT:
      priv->channel_dpt = channel_num;
      break;

    case CHANNEL_NMEA_RMC:
      priv->channel_rmc = channel_num;
      break;

    case CHANNEL_STARBOARD:
      priv->channel_starboard = channel_num;
      break;

    case CHANNEL_PORT:
      priv->channel_port = channel_num;
      break;

    default:
      g_warning ("HyScanGtkMapTrack: invalid channel");
    }
}

/* Определяет ширину трека в момент времени time по борту board. */
static gdouble
hyscan_gtk_map_track_width (HyScanGtkMapTrackLayerBoard *board,
                            HyScanDepthometer           *depthometer,
                            gint64                       time,
                            HyScanDBFindStatus          *find_status)
{
  guint32 amp_rindex, amp_lindex;
  guint32 nvals;
  gdouble depth;

  gdouble distance = 0.0;
  HyScanDBFindStatus find_status_ret = HYSCAN_DB_FIND_FAIL;

  if (board->amplitude == NULL)
    goto exit;

  find_status_ret = hyscan_amplitude_find_data (board->amplitude, time, &amp_lindex, &amp_rindex, NULL, NULL);

  if (find_status_ret != HYSCAN_DB_FIND_OK)
    goto exit;

  depth = (depthometer != NULL) ? hyscan_depthometer_get (depthometer, time) : 0.0;
  if (depth < 0.0)
    depth = 0.0;

  hyscan_amplitude_get_size_time (board->amplitude, amp_lindex, &nvals, NULL);
  hyscan_projector_count_to_coord (board->projector, nvals, &distance, depth);

exit:
  *find_status = find_status_ret;

  return distance;
}

/* Вычисляет координаты галса в СК картографической проекции.
 * Если @reverse = %TRUE, то обрабатываются предыдущие от @points точки.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_cartesian (HyScanGtkMapTrack *track,
                                GList             *points,
                                gboolean           reverse)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  GList *point_l;

  for (point_l = points; point_l != NULL; point_l = reverse ? point_l->prev : point_l->next)
    {
      HyScanGtkMapTrackLayerPoint *point, *neighbour_point;
      GList *neighbour;
      gdouble scale;
      gdouble angle;
      gdouble angle_sin, angle_cos;

      point = point_l->data;

      hyscan_geo_projection_geo_to_value (priv->projection, point->geo, &point->c2d);

      /* Определяем расстояние от начала галса на основе соседней точки. */
      neighbour = reverse ? point_l->next : point_l->prev;
      neighbour_point = neighbour != NULL ? neighbour->data : NULL;
      if (neighbour_point != NULL)
        {
          point->dist = neighbour_point->dist +
                        (reverse ? -1 : 1) *  hyscan_cartesian_distance (&point->c2d, &neighbour_point->c2d);
        }
      else
        {
          point->dist = 0;
        }

      /* Масштаб перевода из метров в логические координаты. */
      scale = hyscan_geo_projection_get_scale (priv->projection, point->geo);

      /* Правый и левый борт. */
      angle = point->geo.h / 180.0 * G_PI;
      angle_sin = sin (angle);
      angle_cos = cos (angle);
      point->r_dist = point->r_width / scale;
      point->l_dist = point->l_width / scale;
      point->r_c2d.x = point->c2d.x + point->r_dist * angle_cos;
      point->r_c2d.y = point->c2d.y - point->r_dist * angle_sin;
      point->l_c2d.x = point->c2d.x - point->l_dist * angle_cos;
      point->l_c2d.y = point->c2d.y + point->l_dist * angle_sin;
    }
}

/* Вычисляет границы области, внутри которой размещается часть галса points. */
static void
hyscan_gtk_map_track_layer_points_view (GList                *points,
                                        GList                *end,
                                        HyScanGeoCartesian2D *from,
                                        HyScanGeoCartesian2D *to)
{
  HyScanGtkMapTrackLayerPoint *point;
  GList *point_l;

  gdouble max_margin = 0;
  gdouble from_x = G_MAXDOUBLE;
  gdouble to_x = G_MINDOUBLE;
  gdouble from_y = G_MAXDOUBLE;
  gdouble to_y = G_MINDOUBLE;

  /* Определеяем границы координат путевых точек галса. */
  for (point_l = points; point_l != end; point_l = point_l->next)
    {
      point = point_l->data;

      from_x = MIN (from_x, point->c2d.x);
      to_x = MAX (to_x, point->c2d.x);
      from_y = MIN (from_y, point->c2d.y);
      to_y = MAX (to_y, point->c2d.y);

      max_margin = MAX (max_margin, point->r_dist);
      max_margin = MAX (max_margin, point->l_dist);
    }

  /* Добавим к границам отступы в размере длины максимальной дальности. */
  from_x -= max_margin;
  to_x += max_margin;
  from_y -= max_margin;
  to_y += max_margin;

  from->x = from_x;
  from->y = from_y;
  to->x = to_x;
  to->y = to_y;
}

/* Загружает точки points с индексами от @first до @last, добавляя их после @head
 * или до @tail. В итоге формируется итоговый список:
 *
 * [ result ] = [ head ] + [ points ] + [ tail ]
 *                         ^        ^
 *                       first     last
 *
 * Возвращает указатель на итоговый список result.
 *
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static GList *
hyscan_gtk_map_track_load_range (HyScanGtkMapTrack *track,
                                 GList             *head,
                                 GList             *tail,
                                 guint32            first,
                                 guint32            last)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  HyScanGeoCartesian2D from, to;
  GList *result;
  GList *points = NULL;
  guint32 index;

  /* В текущей реализации нельзя одновременно добавить head и tail. */
  g_return_val_if_fail (head == NULL || tail == NULL, NULL);

  for (index = first; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoGeodetic coords;
      HyScanGtkMapTrackLayerPoint *point;

      if (!hyscan_nav_data_get (priv->lat_data, index, &time, &coords.lat))
        continue;

      if (!hyscan_nav_data_get (priv->lon_data, index, &time, &coords.lon))
        continue;

      if (!hyscan_nav_data_get (priv->angle_data, index, &time, &coords.h))
        continue;

      point = g_new (HyScanGtkMapTrackLayerPoint, 1);
      point->index = index;
      point->geo = coords;

      /* Определяем ширину отснятых данных в этот момент. */
      point->r_width = hyscan_gtk_map_track_width (&priv->starboard, priv->depthometer, time,
                                                               &point->r_find_status);
      point->l_width = hyscan_gtk_map_track_width (&priv->port, priv->depthometer, time,
                                                               &point->l_find_status);

      points = g_list_append (points, point);
    }

  /* Переводим географические координаты в логические. */
  if (tail != NULL)
    {
      result = g_list_concat (points, tail);
      hyscan_gtk_map_track_cartesian (track, tail->prev, TRUE);
    }
  else
    {
      result = g_list_concat (head, points);
      hyscan_gtk_map_track_cartesian (track, points, FALSE);
    }

  /* Отмечаем на тайловом слое устаревшую область. */
  if (points != NULL)
    {
      hyscan_gtk_map_track_layer_points_view (points->prev != NULL ? points->prev : points, tail, &from, &to);
      hyscan_gtk_map_tiled_layer_set_area_mod (HYSCAN_GTK_MAP_TILED_LAYER (priv->tiled_layer), &from, &to);
    }


  return result;
}

/* Удаляет из трека путевые точки, которые не попадают в указанный диапазон.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_remove_expired (HyScanGtkMapTrack *track,
                                                 guint32            first_index,
                                                 guint32            last_index)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  GList *point_l;
  HyScanGtkMapTrackLayerPoint *point;

  if (priv->points == NULL)
    return;

  /* Проходим все точки галса и оставляем только актуальную информацию: 1-3. */
  point_l = priv->points;

  /* 1. Удаляем точки до first_index. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index >= first_index)
        break;

      g_free (point);
      priv->points = g_list_delete_link (priv->points, point_l);

      point_l = next;
    }

  /* 2. Оставляем точки до last_index, по которым загружены данные бортов. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index > last_index)
        break;

      /* Не сохраняем точки у которых был статус HYSCAN_DB_FIND_GREATER, т.к. теперь надеемся получить по ним данные. */
      if (point->l_find_status == HYSCAN_DB_FIND_GREATER || point->r_find_status == HYSCAN_DB_FIND_GREATER)
        break;

      point_l = next;
    }

  /* 3. Остальное удаляем. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      g_free (point);
      priv->points = g_list_delete_link (priv->points, point_l);

      point_l = next;
    }

  /* Обновляем {first,last}_index у галса. */
  if (priv->points != NULL)
    {
      point = priv->points->data;
      priv->first_index = point->index;
      point = g_list_last (priv->points)->data;
      priv->last_index = point->index;
    }
}

/* Добавляет в галс недостающие по краям путевые точки, дополняя указанного диапазон.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_load_edges (HyScanGtkMapTrack *track,
                                             guint32            first_index,
                                             guint32            last_index)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  
  /* Если в галсе нет точек, загружаем весь диапазон. */
  if (priv->points == NULL)
    {
      priv->points = hyscan_gtk_map_track_load_range (track, NULL, NULL,
                                                      first_index, last_index);
      return;
    }

  /* Добавляем точки в начало списка. */
  if (priv->first_index > first_index)
    {
      priv->points = hyscan_gtk_map_track_load_range (track, NULL, priv->points,
                                                      first_index, priv->first_index - 1);
    }

  /* Добавляем точки в конец списка. */
  if (priv->last_index < last_index)
    {
      priv->points = hyscan_gtk_map_track_load_range (track, priv->points, NULL,
                                                      priv->last_index + 1, last_index);
    }
}

/* Открывает (или переоткрывает) каналы данных в указанном галсе.
 * Функция должна вызываться за g_rw_lock_writer_lock(). */
static void
hyscan_gtk_map_track_open (HyScanGtkMapTrack *track)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  HyScanNMEAParser *dpt_parser;

  g_clear_object (&priv->starboard.amplitude);
  g_clear_object (&priv->starboard.projector);
  g_clear_object (&priv->port.amplitude);
  g_clear_object (&priv->port.projector);
  g_clear_object (&priv->lat_data);
  g_clear_object (&priv->lon_data);
  g_clear_object (&priv->angle_data);
  g_clear_object (&priv->depthometer);

  if (priv->channel_starboard > 0)
    {
      priv->starboard.amplitude = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (priv->db, priv->cache, priv->project,
                                                                              priv->name,
                                                                              HYSCAN_SOURCE_SIDE_SCAN_STARBOARD,
                                                                              priv->channel_starboard, FALSE));
      priv->starboard.projector = hyscan_projector_new (priv->starboard.amplitude);
    }
  if (priv->channel_port > 0)
    {
      priv->port.amplitude = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (priv->db, priv->cache, priv->project,
                                                                         priv->name,
                                                                         HYSCAN_SOURCE_SIDE_SCAN_PORT,
                                                                         priv->channel_port, FALSE));
      priv->port.projector = hyscan_projector_new (priv->port.amplitude);
    }


  if (priv->channel_rmc > 0)
    {
      priv->lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                priv->name, priv->channel_rmc,
                                                                HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
      priv->lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                priv->name, priv->channel_rmc,
                                                                HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
      priv->angle_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                                   priv->name, priv->channel_rmc,
                                                                   HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));
      priv->writeable = hyscan_nav_data_is_writable (priv->lat_data);
    }

  if (priv->channel_dpt > 0)
    {
      dpt_parser = hyscan_nmea_parser_new (priv->db, priv->cache,
                                           priv->project, priv->name, priv->channel_dpt,
                                           HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH);
      if (dpt_parser != NULL)
        priv->depthometer = hyscan_depthometer_new (HYSCAN_NAV_DATA (dpt_parser), priv->cache);

      g_clear_object (&dpt_parser);
    }

  priv->loaded = FALSE;
  priv->opened = TRUE;
}

/* Проверяет, изменились ли данные галса. */
static gboolean
hyscan_gtk_map_track_has_changed (HyScanGtkMapTrack *track)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  
  if (priv->lat_data == NULL)
    return FALSE;

  if (!priv->loaded)
    return TRUE;

  return priv->writeable && priv->lat_mod_count != hyscan_nav_data_get_mod_count (priv->lat_data);
}

/* Загружает путевые точки трека и его ширину. Возвращает %TRUE, если данные изменились.
   Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static gboolean
hyscan_gtk_map_track_load (HyScanGtkMapTrack *track)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  guint32 first_index, last_index;
  guint32 mod_count;

  /* Открываем каналы данных. */
  if (!priv->opened)
    hyscan_gtk_map_track_open (track);
  /* Если уже загружены актуальные данные, то всё ок. */
  else if (!hyscan_gtk_map_track_has_changed (track))
    return FALSE;

  /* Без навигационных данных ничего не получится загрузить. */
  if (priv->lat_data == NULL)
    {
      g_list_free_full (priv->points, g_free);
      priv->points = NULL;

      return TRUE;
    }

  /* Приводим список точек в соответствие с флагом priv->loaded. */
  if (!priv->loaded && priv->points != NULL)
    {
      g_list_free_full (priv->points, g_free);
      priv->points = NULL;
    }

  /* Запоминаем mod_count, по которому получаются данные. */
  mod_count = hyscan_nav_data_get_mod_count (priv->lat_data);
  hyscan_nav_data_get_range (priv->lat_data, &first_index, &last_index);

  hyscan_gtk_map_track_remove_expired (track, first_index, last_index);
  hyscan_gtk_map_track_load_edges (track, first_index, last_index);

  priv->lat_mod_count = mod_count;
  priv->first_index = first_index;
  priv->last_index = last_index;

  priv->loaded = TRUE;

  return TRUE;
}

/* Рисует часть галса по точкам points. */
static void
hyscan_gtk_map_track_layer_draw_chunk (HyScanGeoCartesian2D   *from,
                                       HyScanGeoCartesian2D   *to,
                                       gdouble                 scale,
                                       GList                  *chunk_start,
                                       GList                  *chunk_end,
                                       cairo_t                *cairo,
                                       HyScanGtkMapTrackStyle *style)
{
  GList *point_l;
  HyScanGtkMapTrackLayerPoint *point, *next_point;
  gdouble x, y;

  gdouble threshold;

  /* Делим весь отрезок на зоны длины threshold. В каждой зоне одна полоса. */
  threshold = scale * (style->bar_margin + style->bar_width);

  /* Рисуем полосы от бортов. */
  cairo_set_line_width (cairo, style->bar_width);
  cairo_new_path (cairo);
  for (point_l = chunk_start; point_l != chunk_end; point_l = point_l->next)
    {
      gdouble x0, y0;
      gdouble shadow_len;

      point = point_l->data;
      next_point = point_l->next ? point_l->next->data : NULL;

      /* Рисуем полосу, только если следующая полоса лежит в другой зоне. */
      if (next_point == NULL || round (next_point->dist / threshold) == round (point->dist / threshold))
        continue;

      /* Координаты точки на поверхности cairo. */
      x0 = (point->c2d.x - from->x) / scale;
      y0 = (from->y - to->y) / scale - (point->c2d.y - to->y) / scale;

      cairo_save (cairo);
      cairo_translate (cairo, x0, y0);
      cairo_rotate (cairo, point->geo.h / 180 * G_PI);

      /* Правый борт. */
      if (point->r_dist > 0)
        {
          cairo_rectangle (cairo, 0, -style->bar_width / 2.0, point->r_dist / scale, style->bar_width);
          cairo_set_line_width (cairo, style->stroke_width);
          gdk_cairo_set_source_rgba (cairo, &style->color_stroke);
          cairo_stroke_preserve (cairo);
          gdk_cairo_set_source_rgba (cairo, &style->color_right);
          cairo_fill (cairo);
        }

      /* Левый борт. */
      if (point->l_dist > 0)
        {
          cairo_rectangle (cairo, 0, -style->bar_width / 2.0, -point->l_dist / scale, style->bar_width);
          cairo_set_line_width (cairo, style->stroke_width);
          gdk_cairo_set_source_rgba (cairo, &style->color_stroke);
          cairo_stroke_preserve (cairo);
          gdk_cairo_set_source_rgba (cairo, &style->color_left);
          cairo_fill (cairo);
        }

      /* Тень в пересечении линии движения и полос от бортов. */
      if (point->r_dist > 0 || point->l_dist > 0)
        {
          gdk_cairo_set_source_rgba (cairo, &style->color_shadow);
          shadow_len =  MIN (point->r_dist / scale, point->l_dist / scale) - 1.0;
          shadow_len = MIN (style->bar_width, shadow_len);
          cairo_rectangle (cairo, - shadow_len / 2.0, - style->bar_width / 2.0, shadow_len, style->bar_width);
          cairo_fill (cairo);
        }

      cairo_restore (cairo);
    }

  /* Рисуем линию движения. */
  gdk_cairo_set_source_rgba (cairo, &style->color_track);
  cairo_set_line_width (cairo, style->line_width);
  cairo_new_path (cairo);
  for (point_l = chunk_start; point_l != chunk_end; point_l = point_l->next)
    {
      point = point_l->data;

      /* Координаты точки на поверхности cairo. */
      x = (point->c2d.x - from->x) / scale;
      y = (from->y - to->y) / scale - (point->c2d.y - to->y) / scale;

      cairo_line_to (cairo, x, y);
    }
  cairo_stroke (cairo);
}

/* Рисует галс внутри указанной области from - to. */
static void
hyscan_gtk_map_track_layer_draw_region (HyScanGtkMapTrack      *track,
                                        cairo_t                *cairo,
                                        gdouble                 scale,
                                        HyScanGeoCartesian2D   *from,
                                        HyScanGeoCartesian2D   *to,
                                        HyScanGtkMapTrackStyle *style)
{
  HyScanGtkMapTrackPrivate *priv = track->priv;
  GList *point_l, *prev_point_l = NULL, *next_point_l = NULL;
  GList *chunk_start = NULL, *chunk_end = NULL;

  /* Для каждого узла определяем, попадает ли он в указанную область. */
  prev_point_l = point_l = priv->points;
  while (point_l != NULL)
    {
      HyScanGeoCartesian2D *prev_point, *cur_point, *next_point;
      gboolean is_inside;

      cur_point  = &((HyScanGtkMapTrackLayerPoint *) point_l->data)->c2d;
      prev_point = &((HyScanGtkMapTrackLayerPoint *) prev_point_l->data)->c2d;

      /* 1. Отрезок до предыдущего узла в указанной области. */
      is_inside = hyscan_cartesian_is_inside (prev_point, cur_point, from, to);

      /* 2. Отрезок до следующего узла в указанной области. */
      if (!is_inside && next_point_l != NULL)
        {
          next_point = &((HyScanGtkMapTrackLayerPoint *) next_point_l->data)->c2d;
          is_inside = hyscan_cartesian_is_inside (next_point, cur_point, from, to);
        }

      /* 3. Полосы от бортов в указанной области. */
      if (!is_inside)
        {
          HyScanGtkMapTrackLayerPoint *cur_track_point = point_l->data;

          is_inside = hyscan_cartesian_is_inside (&cur_track_point->l_c2d, &cur_track_point->r_c2d, from, to);
        }

      if (is_inside)
        {
          chunk_start = (chunk_start == NULL) ? prev_point_l : chunk_start;
          chunk_end = next_point_l;
        }
      else if (chunk_start != NULL)
        {
          hyscan_gtk_map_track_layer_draw_chunk (from, to, scale, chunk_start, chunk_end, cairo, style);
          chunk_start = chunk_end = NULL;
        }

      prev_point_l = point_l;
      point_l = point_l->next;
      next_point_l = point_l != NULL ? point_l->next : NULL;
    }

  if (chunk_start != NULL)
    hyscan_gtk_map_track_layer_draw_chunk (from, to, scale, chunk_start, chunk_end, cairo, style);
}


/**
 * hyscan_gtk_map_track_new:
 * @db: указатель на базу данных #HyScanDB
 * @cache: кэш #HyScanCache
 * @project_name: название проекта
 * @track_name: название галса
 *
 * Создаёт галс с параметрами по умолчанию.
 *
 * Returns: новый объект #HyScanGtkMapTrack. Для удаления g_object_unref().
 */
HyScanGtkMapTrack *
hyscan_gtk_map_track_new (HyScanDB               *db,
                          HyScanCache            *cache,
                          const gchar            *project_name,
                          const gchar            *track_name,
                          HyScanGtkMapTiledLayer *tiled_layer,
                          HyScanGeoProjection    *projection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK,
                       "db", db,
                       "cache", cache,
                       "tiled-layer", tiled_layer,
                       "project", project_name,
                       "name", track_name,
                       "projection", projection, NULL);
}

/**
 * hyscan_gtk_map_track_draw:
 * @track: указатель на #HyScanGtkMapTrack
 * @cairo: поверхность для рисования
 * @scale: масштаб
 * @from: граница видимой области
 * @to: граница видимой области
 * @style: указатель на стиль оформление галса #HyScanGtkMapTrackStyle
 *
 * Рисует на поверхности @cairo часть галса, которая находится внутри прямоугольной
 * области от @from до @to. Галс рисуется в масштабе @scale и оформлением в стиле @style.
 */
void
hyscan_gtk_map_track_draw (HyScanGtkMapTrack      *track,
                           cairo_t                *cairo,
                           gdouble                 scale,
                           HyScanGeoCartesian2D   *from,
                           HyScanGeoCartesian2D   *to,
                           HyScanGtkMapTrackStyle *style)
{
  HyScanGtkMapTrackPrivate *priv;
  HyScanGtkMapTrackLayerPoint *start_point;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track));

  priv = track->priv;

  /* Загружаем новые данные по треку (если что-то изменилось в канале данных). */
  g_rw_lock_writer_lock (&priv->lock);
  hyscan_gtk_map_track_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  g_rw_lock_reader_lock (&priv->lock);
  if (priv->points == NULL)
    {
      g_rw_lock_reader_unlock (&priv->lock);
      return;
    }

  /* Линия галса. */
  hyscan_gtk_map_track_layer_draw_region (track, cairo, scale, from, to, style);

  /* Точка начала трека. */
  start_point = priv->points->data;
  if (hyscan_cartesian_is_point_inside (&start_point->c2d, from, to))
  {
    gdouble x, y;

    /* Координаты точки на поверхности cairo. */
    x = (start_point->c2d.x - from->x) / scale;
    y = (from->y - to->y) / scale - (start_point->c2d.y - to->y) / scale;


    cairo_arc (cairo, x, y, 2.0 * style->line_width, 0, 2.0 * G_PI);

    gdk_cairo_set_source_rgba (cairo, &style->color_track);
    cairo_fill_preserve (cairo);

    cairo_set_line_width (cairo, style->stroke_width);
    gdk_cairo_set_source_rgba (cairo, &style->color_stroke);
    cairo_stroke (cairo);
  }

  g_rw_lock_reader_unlock (&priv->lock);
}

/**
 * hyscan_gtk_map_track_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrackLayer
 * @track_name: название галса
 * @from: (out): координата левой верхней точки границы
 * @to: (out): координата правой нижней точки границы
 *
 * Возвращает границы области, в которой находится галс
 *
 * Returns: %TRUE, если получилось определить границы галса; иначе %FALSE
 */
gboolean
hyscan_gtk_map_track_view (HyScanGtkMapTrack    *track,
                           HyScanGeoCartesian2D *from,
                           HyScanGeoCartesian2D *to)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track), FALSE);
  priv = track->priv;

  /* Если данные по галсу не загружены, то загружаем. */
  g_rw_lock_writer_lock (&priv->lock);
  hyscan_gtk_map_track_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  g_rw_lock_reader_lock (&priv->lock);
  if (priv->points == NULL)
    {
      g_rw_lock_reader_unlock (&priv->lock);

      return FALSE;
    }

  hyscan_gtk_map_track_layer_points_view (priv->points, NULL, from, to);
  g_rw_lock_reader_unlock (&priv->lock);

  return TRUE;
}

/* Устанавливает номер канала для указанного трека. Чтобы не загружать данные по
 * указанном истоничку, необходимо передать @channel_num = 0. Максимальный
 * доступный номер канала можно получить с помощью функции hyscan_gtk_map_track_max_channel().
 */
static void
hyscan_gtk_map_track_set_channel (HyScanGtkMapTrack *track,
                                  guint              channel,
                                  guint              channel_num)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track));
  priv = track->priv;

  g_rw_lock_writer_lock (&priv->lock);

  hyscan_gtk_map_track_set_channel_real (track, channel, channel_num);

  /* Переоткрываем трек с новыми номерами каналов. */
  priv->opened = FALSE;
  priv->loaded = FALSE;

  g_rw_lock_writer_unlock (&priv->lock);
}

/* Возвращает номер канала указанного источника или 0, если канал не открыт или
 * произошла ошибка. */
static guint
hyscan_gtk_map_track_get_channel (HyScanGtkMapTrack *track,
                                  guint              channel)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track), 0);
  priv = track->priv;

  switch (channel)
    {
    case CHANNEL_NMEA_DPT:
      return priv->channel_dpt;

    case CHANNEL_NMEA_RMC:
      return priv->channel_rmc;

    case CHANNEL_STARBOARD:
      return priv->channel_starboard;

    case CHANNEL_PORT:
      return priv->channel_port;

    default:
      g_warning ("HyScanGtkMapTrack: invalid channel");
      return 0;
    }
}

/**
 * hyscan_gtk_map_track_update:
 * @track: указатель на #HyScanGtkMapTrack
 *
 * Проверяет, есть ли новые данные в галсе и есть есть, то загружает их
 *
 * Returns: %TRUE, если были загружены новые данные
 */
gboolean
hyscan_gtk_map_track_update (HyScanGtkMapTrack *track)
{
  HyScanGtkMapTrackPrivate *priv;
  gboolean any_changes;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track), FALSE);
  priv = track->priv;

  g_rw_lock_writer_lock (&priv->lock);
  any_changes = hyscan_gtk_map_track_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  return any_changes;
}

/**
 * hyscan_gtk_map_track_set_projection:
 * @track: указатель на #HyScanGtkMapTrack
 * @projection: проекция #HyScanGeoProjection
 *
 * Устанавливает картографическую проекцию, в которой рисуется изображение
 * галса.
 */
void
hyscan_gtk_map_track_set_projection (HyScanGtkMapTrack   *track,
                                     HyScanGeoProjection *projection)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track));
  priv = track->priv;

  g_rw_lock_writer_lock (&priv->lock);
  g_clear_object (&priv->projection);
  priv->projection = g_object_ref (projection);
  hyscan_gtk_map_track_cartesian (track, priv->points, FALSE);
  g_rw_lock_writer_unlock (&priv->lock);
}

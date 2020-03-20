/* hyscan-gtk-map-track-item.c
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
 * @Title: HyScanGtkMapTrackItem
 *
 * HyScanGtkMapTrackItem позволяет изображать галс на карте в виде линии движения
 * судна и дальности обнаружения по каждому борту.
 *
 * Класс выступает как вспомогательный для слоя галсов #HyScanGtkMapTrack и производит
 * внутри себя загрузку данных по галсу и расчёт координат точек.
 *
 * Функции:
 * - hyscan_gtk_map_track_item_new() - создаёт новый объект;
 * - hyscan_gtk_map_track_item_set_projection() - устанавливает картографическую проекцию;
 * - hyscan_gtk_map_track_item_update() - проверяет и загружает новые данные по галсу;
 * - hyscan_gtk_map_track_item_points_lock() - получает список точек галса;
 * - hyscan_gtk_map_track_item_view() - определяет границы галса.
 *
 * Кроме того #HyScanGtkMapTrackItem реализует интерфейс #HyScanParam, с помощью
 * которого можно установить каналы данных, используемые для загрузки информации
 * по галсу.
 *
 */

#include "hyscan-gtk-map-track-item.h"
#include <glib/gi18n-lib.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-cartesian.h>
#include <hyscan-core-common.h>
#include <hyscan-data-schema-builder.h>
#include <hyscan-depthometer.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-projector.h>
#include <hyscan-quality.h>
#include <hyscan-nav-smooth.h>
#include <math.h>

/* Ключи схемы данных настроек галса. */
#define ENUM_NMEA_CHANNEL     "nmea-channel"
#define KEY_CHANNEL_RMC       "/channel-rmc"
#define KEY_CHANNEL_DPT       "/channel-dpt"
#define KEY_CHANNEL_PORT      "/channel-port"
#define KEY_CHANNEL_STARBOARD "/channel-starboard"
#define KEY_TARGET_QUALITY    "/target-quality"

#define STRAIGHT_LINE_MAX_ANGLE 0.26   /* Максимальное изменение курса на прямолинейном участке, рад. */
#define STRAIGHT_LINE_MIN_DIST  30.0   /* Минимальная длина прямолинейного участка, метры. */
#define DEFAULT_HAPERTURE       0.15   /* Апертура антенны по умолчанию (если её нет в параметрах галса). */
#define SOUND_VELOCITY          1500.  /* Скорость звука. */

/* Параметры по умолчанию. */
#define DEFAULT_CHANNEL_RMC       1
#define DEFAULT_CHANNEL_DPT       2
#define DEFAULT_CHANNEL_PORT      1
#define DEFAULT_CHANNEL_STARBOARD 1
#define DEFAULT_QUALITY           0.5

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

typedef struct
{
  guint                           channel;           /* Номер канала. */
  HyScanAntennaOffset             offset;            /* Смещение антенны. */
  gboolean                        writeable;         /* Признак записи в канал. */
  guint32                         mod_count;         /* Mod-count амплитудных данных данных. */
  GList                          *points;            /* Точки галса HyScanGtkMapTrackPoint. */

  HyScanGtkMapTrackDrawSource     source;            /* Источник. */
  HyScanAmplitude                *amplitude;         /* Амплитудные данные трека. */
  HyScanQuality                  *quality;           /* Объект для определения качества данных. */
  HyScanProjector                *projector;         /* Сопоставления индексов и отсчётов реальным координатам. */

  gdouble                         antenna_length;    /* Длина антенны. */
  gdouble                         beam_width;        /* Ширина луча, радианы. */
  gdouble                         near_field;        /* Граница ближней зоны. */
} HyScanGtkMapTrackItemSide;

typedef struct
{
  guint                           channel;           /* Номер канала nmea с RMC. */
  HyScanAntennaOffset             offset;            /* Смещение антенны. */
  gboolean                        writeable;         /* Признак записи в канал. */
  guint32                         mod_count;         /* Mod-count канала навигационных данных. */
  GList                          *points;            /* Точки галса HyScanGtkMapTrackPoint. */

  HyScanNavData                  *lat_data;          /* Навигационные данные - широта. */
  HyScanNavData                  *lon_data;          /* Навигационные данные - долгота. */
  HyScanNavData                  *trk_data;          /* Навигационные данные - курс в градусах. */
  HyScanNavSmooth                *lat_smooth;        /* Интерполяция данных широты. */
  HyScanNavSmooth                *lon_smooth;        /* Интерполяция данных долготы. */
  HyScanNavSmooth                *trk_smooth;        /* Интерполяция данных курса. */
} HyScanGtkMapTrackItemNav;

typedef struct
{
  guint                           channel;           /* Номер канала nmea с DPT. */
  HyScanAntennaOffset             offset;            /* Смещение эхолота. */
  HyScanDepthometer              *meter;             /* Определение глубины. */
} HyScanGtkMapTrackDepth;

struct _HyScanGtkMapTrackItemPrivate
{
  HyScanDB                       *db;                /* База данных. */
  HyScanCache                    *cache;             /* Кэш. */
  HyScanGtkMapTiled              *tiled_layer;       /* Тайловый слой, на котором размещён галс. */
  HyScanDataSchema               *schema;            /* Схема параметров галса (номера каналов данных). */

  gchar                          *project;           /* Название проекта. */
  gchar                          *name;              /* Название галса. */

  gboolean                        opened;            /* Признак того, что каналы галса открыты. */
  gboolean                        loaded;            /* Признак того, что данные галса загружены. */
  gdouble                         quality;           /* Целевое качество для покрытия. */

  GRWLock                         lock;              /* Блокировка доступа к точкам галса и текущей проекции. */
  HyScanGeoProjection            *projection;        /* Картографическая проекция. */

  GMutex                          mutex;             /* Блокировка доступа к новой проекции. */
  HyScanGeoProjection            *new_projection;    /* Новая проекция. */

  HyScanGtkMapTrackItemSide       port;              /* Данные левого борта. */
  HyScanGtkMapTrackItemSide       starboard;         /* Данные правого борта. */
  HyScanGtkMapTrackItemNav        nav;               /* Данные навигации. */
  HyScanGtkMapTrackDepth          depth;             /* Данные по глубине. */

  HyScanGeoCartesian2D            extent_from;       /* Минимальные координаты точек галса. */
  HyScanGeoCartesian2D            extent_to;         /* Максимальные координаты точек галса. */
};

static void     hyscan_gtk_map_track_item_interface_init      (HyScanParamInterface      *iface);
static void     hyscan_gtk_map_track_item_set_property        (GObject                   *object,
                                                               guint                      prop_id,
                                                               const GValue              *value,
                                                               GParamSpec                *pspec);
static void     hyscan_gtk_map_track_item_object_constructed  (GObject                   *object);
static void     hyscan_gtk_map_track_item_object_finalize     (GObject                   *object);
static void     hyscan_gtk_map_track_item_schema_build        (HyScanGtkMapTrackItem     *track);
static void     hyscan_gtk_map_track_item_set_channel         (HyScanGtkMapTrackItem     *track,
                                                               guint                      channel,
                                                               guint                      channel_num);
static void     hyscan_gtk_map_track_item_set_quality         (HyScanGtkMapTrackItem     *track,
                                                               gdouble                    quality);
static void     hyscan_gtk_map_track_item_load_side           (HyScanGtkMapTrackItem     *track,
                                                               HyScanGtkMapTrackItemSide *side);
static void     hyscan_gtk_map_track_item_remove_expired      (GList                     *points,
                                                               guint32                    first_index,
                                                               guint32                    last_index);
static void     hyscan_gtk_map_track_item_reset_extent        (HyScanGtkMapTrackItemPrivate *priv);
static void     hyscan_gtk_map_track_item_update_extent       (GList                     *points,
                                                               HyScanGeoCartesian2D      *from,
                                                               HyScanGeoCartesian2D      *to);
inline static void hyscan_gtk_map_track_item_extend           (HyScanGtkMapTrackPoint    *point,
                                                               HyScanGeoCartesian2D      *from,
                                                               HyScanGeoCartesian2D      *to);
static guint    hyscan_gtk_map_track_item_max_channel         (HyScanGtkMapTrackItem     *track,
                                                               guint                      channel);
static void hyscan_gtk_map_track_item_schema_build_nmea_enum  (HyScanGtkMapTrackItem     *track,
                                                               HyScanDataSchemaBuilder   *builder,
                                                               guint                     *channel_last);
static guint32  hyscan_gtk_map_track_item_get_counts          (HyScanGtkMapTrackItemSide *side,
                                                               gdouble                    quality,
                                                               guint32                    index,
                                                               guint32                    n_counts);
static void     hyscan_gtk_map_track_item_load_length_by_idx  (HyScanGtkMapTrackItem     *track,
                                                               HyScanGtkMapTrackItemSide *side,
                                                               HyScanGtkMapTrackPoint    *point);
static gboolean hyscan_gtk_map_track_item_is_straight         (GList                     *l_point);
static void     hyscan_gtk_map_track_item_cartesian           (HyScanGtkMapTrackItem     *track,
                                                               GList                     *points);
static void     hyscan_gtk_map_track_item_move_point          (HyScanGeoCartesian2D      *point,
                                                               gdouble                    angle,
                                                               gdouble                    length,
                                                               HyScanGeoCartesian2D      *destination);
static gboolean hyscan_gtk_map_track_item_load                (HyScanGtkMapTrackItem     *track);
static gboolean hyscan_gtk_map_track_item_has_changed         (HyScanGtkMapTrackItem     *track);
static void     hyscan_gtk_map_track_item_open                (HyScanGtkMapTrackItem     *track);
static void     hyscan_gtk_map_track_item_open_depth          (HyScanGtkMapTrackItemPrivate *priv);
static void     hyscan_gtk_map_track_item_open_nav            (HyScanGtkMapTrackItemPrivate *priv);
static void     hyscan_gtk_map_track_item_open_side           (HyScanGtkMapTrackItemPrivate *priv,
                                                               HyScanGtkMapTrackItemSide *side,
                                                               HyScanSourceType           source);
static void     hyscan_gtk_map_track_item_load_nav            (HyScanGtkMapTrackItem     *track);


G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackItem, hyscan_gtk_map_track_item, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackItem)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_PARAM, hyscan_gtk_map_track_item_interface_init))

static void
hyscan_gtk_map_track_item_class_init (HyScanGtkMapTrackItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_item_set_property;

  object_class->constructed = hyscan_gtk_map_track_item_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_item_object_finalize;

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
    g_param_spec_object ("tiled-layer", "Tiled layer", "The HyScanGtkMapTiled responsible for view updates",
                         HYSCAN_TYPE_GTK_MAP_TILED,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project name", "The project containing track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_NAME,
    g_param_spec_string ("name", "Track name", "The name of the track", NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_item_init (HyScanGtkMapTrackItem *gtk_map_track_item)
{
  gtk_map_track_item->priv = hyscan_gtk_map_track_item_get_instance_private (gtk_map_track_item);
}

static void
hyscan_gtk_map_track_item_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkMapTrackItem *track = HYSCAN_GTK_MAP_TRACK_ITEM (object);
  HyScanGtkMapTrackItemPrivate *priv = track->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PROJECTION:
      hyscan_gtk_map_track_item_set_projection (track, g_value_get_object (value));
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
hyscan_gtk_map_track_item_object_constructed (GObject *object)
{
  HyScanGtkMapTrackItem *track = HYSCAN_GTK_MAP_TRACK_ITEM (object);
  HyScanGtkMapTrackItemPrivate *priv = track->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_track_item_parent_class)->constructed (object);

  priv->starboard.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT;
  priv->port.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT;

  g_rw_lock_init (&priv->lock);
  g_mutex_init (&priv->mutex);

  /* Область, в которой находится галс. */
  hyscan_gtk_map_track_item_reset_extent (priv);

  /* Устанавливаем параметры по умолчанию. */
  hyscan_gtk_map_track_item_schema_build (track);
  hyscan_gtk_map_track_item_set_channel (track, CHANNEL_NMEA_RMC, DEFAULT_CHANNEL_RMC);
  hyscan_gtk_map_track_item_set_channel (track, CHANNEL_NMEA_DPT, DEFAULT_CHANNEL_DPT);
  hyscan_gtk_map_track_item_set_channel (track, CHANNEL_STARBOARD, DEFAULT_CHANNEL_STARBOARD);
  hyscan_gtk_map_track_item_set_channel (track, CHANNEL_PORT, DEFAULT_CHANNEL_PORT);
  hyscan_gtk_map_track_item_set_quality (track, DEFAULT_QUALITY);
}

static void
hyscan_gtk_map_track_item_object_finalize (GObject *object)
{
  HyScanGtkMapTrackItem *gtk_map_track_item = HYSCAN_GTK_MAP_TRACK_ITEM (object);
  HyScanGtkMapTrackItemPrivate *priv = gtk_map_track_item->priv;

  g_rw_lock_clear (&priv->lock);

  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->tiled_layer);
  g_clear_object (&priv->projection);
  g_clear_object (&priv->schema);

  g_free (priv->project);
  g_free (priv->name);

  g_clear_object (&priv->port.amplitude);
  g_clear_object (&priv->port.quality);
  g_clear_object (&priv->port.projector);
  g_clear_object (&priv->starboard.amplitude);
  g_clear_object (&priv->starboard.quality);
  g_clear_object (&priv->starboard.projector);

  g_clear_object (&priv->depth.meter);
  g_clear_object (&priv->nav.lat_data);
  g_clear_object (&priv->nav.lon_data);
  g_clear_object (&priv->nav.trk_data);
  g_clear_object (&priv->nav.lat_smooth);
  g_clear_object (&priv->nav.lon_smooth);
  g_clear_object (&priv->nav.trk_smooth);

  g_mutex_clear (&priv->mutex);
  g_clear_object (&priv->new_projection);

  g_list_free_full (priv->nav.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);
  g_list_free_full (priv->port.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);
  g_list_free_full (priv->starboard.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);

  G_OBJECT_CLASS (hyscan_gtk_map_track_item_parent_class)->finalize (object);
}

/* Реализация HyScanParamInterface.get.
 * Получает параметры галса. */
static gboolean
hyscan_gtk_map_track_item_param_get (HyScanParam     *param,
                                     HyScanParamList *list)
{
  HyScanGtkMapTrackItem *track = HYSCAN_GTK_MAP_TRACK_ITEM (param);
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
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
          channel = priv->nav.channel;
          hyscan_param_list_set_enum (list, names[i], channel);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_DPT))
        {
          channel = priv->depth.channel;
          hyscan_param_list_set_enum (list, names[i], channel);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_PORT))
        {
          channel = priv->port.channel;
          hyscan_param_list_set_boolean (list, names[i], channel > 0);
        }
      else if (g_str_equal (names[i], KEY_CHANNEL_STARBOARD))
        {
          channel = priv->starboard.channel;
          hyscan_param_list_set_boolean (list, names[i], channel > 0);
        }
      else if (g_str_equal (names[i], KEY_TARGET_QUALITY))
        {
          hyscan_param_list_set_double (list, names[i], priv->quality);
        }
    }

  return TRUE;
}

/* Реализация HyScanParamInterface.set.
 * Устанавливает параметры галса. */
static gboolean
hyscan_gtk_map_track_item_param_set (HyScanParam     *param,
                                     HyScanParamList *list)
{
  HyScanGtkMapTrackItem *track = HYSCAN_GTK_MAP_TRACK_ITEM (param);
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  const gchar * const * names;
  gint i;

  names = hyscan_param_list_params (list);
  if (names == NULL)
    return FALSE;

  g_rw_lock_writer_lock (&priv->lock);
  for (i = 0; names[i] != NULL; ++i)
    {
      gdouble quality;
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
      else if (g_str_equal (names[i], KEY_TARGET_QUALITY))
        {
          quality = hyscan_param_list_get_double (list, names[i]);
          hyscan_gtk_map_track_item_set_quality (track, quality);
          continue;
        }
      else
        {
          continue;
        }

      hyscan_gtk_map_track_item_set_channel (track, channel, channel_num);
    }
  g_rw_lock_writer_unlock (&priv->lock);

  /* Сообщаем об изменнении параметров и необходимости перерисовки. */
  hyscan_gtk_map_tiled_set_param_mod (priv->tiled_layer);
  hyscan_gtk_map_tiled_request_draw (priv->tiled_layer);

  return TRUE;
}

/* Реализация HyScanParamInterface.schema.
 * Возвращает схему параметров галса. */
static HyScanDataSchema *
hyscan_gtk_map_track_item_param_schema (HyScanParam *param)
{
  HyScanGtkMapTrackItem *track = HYSCAN_GTK_MAP_TRACK_ITEM (param);
  HyScanGtkMapTrackItemPrivate *priv = track->priv;

  return g_object_ref (priv->schema);
}

static void
hyscan_gtk_map_track_item_interface_init (HyScanParamInterface *iface)
{
  iface->get = hyscan_gtk_map_track_item_param_get;
  iface->set = hyscan_gtk_map_track_item_param_set;
  iface->schema = hyscan_gtk_map_track_item_param_schema;
}

/* Возвращает максимальный доступный номер канала для указанного источника данных. */
static guint
hyscan_gtk_map_track_item_max_channel (HyScanGtkMapTrackItem *track,
                                       guint                  channel)
{
  HyScanGtkMapTrackItemPrivate *priv;
  guint max_channel_num = 0;
  gint i;
  gchar **channel_list;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track), 0);
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
hyscan_gtk_map_track_item_schema_build_nmea_enum (HyScanGtkMapTrackItem   *track,
                                                  HyScanDataSchemaBuilder *builder,
                                                  guint                   *channel_last)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;

  gint32 project_id = -1, track_id = -1;
  gchar **channels = NULL;

  guint max_channel = 0;
  guint i;

  /* Создаём перечисление и "пустое" значение. */
  hyscan_data_schema_builder_enum_create (builder, ENUM_NMEA_CHANNEL);
  hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, 0, "disabled", N_("Disabled"), NULL);

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
      if (source != HYSCAN_SOURCE_NMEA || type != HYSCAN_CHANNEL_DATA)
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

      /* Добавляем канал в enum. В качестве идентификатора ставим имя датчика,
       * поскольку нас интересует именно датчик, а не номер канала. */
      hyscan_data_schema_builder_enum_value_create (builder, ENUM_NMEA_CHANNEL, channel_num, sensor_name, sensor_name, NULL);
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
hyscan_gtk_map_track_item_schema_build (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  HyScanDataSchemaBuilder *builder;
  guint max_channel;

  builder = hyscan_data_schema_builder_new_with_gettext ("gtk-map-track", GETTEXT_PACKAGE);
  hyscan_data_schema_builder_node_set_name (builder, "/", N_("Track settings"), N_("Configure track channel data"));

  /* Формируем перечисление NMEA-каналов. */
  hyscan_gtk_map_track_item_schema_build_nmea_enum (track, builder, &max_channel);

  /* Настройка канала RMC-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_RMC, N_("RMC Channel"),
                                              N_("The NMEA-channel with RMC sentences"),
                                              ENUM_NMEA_CHANNEL, MIN (DEFAULT_CHANNEL_RMC, max_channel));

  /* Настройка канала DPT-строк. */
  hyscan_data_schema_builder_key_enum_create (builder, KEY_CHANNEL_DPT, N_("DPT Channel"),
                                              N_("The NMEA-channel with DPT sentences"),
                                              ENUM_NMEA_CHANNEL, MIN (DEFAULT_CHANNEL_DPT, max_channel));

  /* Настройка левого борта. */
  max_channel = hyscan_gtk_map_track_item_max_channel (track, CHANNEL_PORT);
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_PORT, N_("Port Channel"),
                                                 N_("Show side-scan port channel data"), max_channel > 0);
  if (max_channel == 0)
    hyscan_data_schema_builder_key_set_access (builder, KEY_CHANNEL_PORT, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  /* Настройка правого борта. */
  max_channel = hyscan_gtk_map_track_item_max_channel (track, CHANNEL_STARBOARD);
  hyscan_data_schema_builder_key_boolean_create (builder, KEY_CHANNEL_STARBOARD, N_("Starboard Channel"),
                                                 N_("Show side-scan starboard channel data"), max_channel > 0);
  if (max_channel == 0)
    hyscan_data_schema_builder_key_set_access (builder, KEY_CHANNEL_STARBOARD, HYSCAN_DATA_SCHEMA_ACCESS_READ);

  hyscan_data_schema_builder_key_double_create (builder, KEY_TARGET_QUALITY, N_("Target Quality"),
                                                N_("Minimum quality to display"), DEFAULT_QUALITY);
  hyscan_data_schema_builder_key_double_range (builder, KEY_TARGET_QUALITY, 0.0, 1.0, 0.1);

  /* Записываем полученную схему данных. */
  g_clear_object (&priv->schema);
  priv->schema = hyscan_data_schema_builder_get_schema (builder);

  g_object_unref (builder);
}

/* Устанавливает номер канала для указанного трека.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_item_set_channel (HyScanGtkMapTrackItem *track,
                                       guint                  channel,
                                       guint                  channel_num)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  guint max_channel;

  max_channel = hyscan_gtk_map_track_item_max_channel (track, channel);
  channel_num = MIN (channel_num, max_channel);

  switch (channel)
    {
    case CHANNEL_NMEA_DPT:
      priv->depth.channel = channel_num;
      break;

    case CHANNEL_NMEA_RMC:
      priv->nav.channel = channel_num;
      break;

    case CHANNEL_STARBOARD:
      priv->starboard.channel = channel_num;
      break;

    case CHANNEL_PORT:
      priv->port.channel = channel_num;
      break;

    default:
      g_warning ("HyScanGtkMapTrackItem: invalid channel");
    }

  /* Ставим флаг о необходимости переоткрыть галс. */
  priv->opened = FALSE;
  priv->loaded = FALSE;
}

/* Устанавливает целевое качество покрытия галса.
 * Функция должна вызываться за g_rw_lock_reader_lock (&priv->lock); */
static void
hyscan_gtk_map_track_item_set_quality (HyScanGtkMapTrackItem *track,
                                       gdouble                quality)
{
  HyScanGtkMapTrackItemPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track));
  priv = track->priv;

  priv->quality = quality;

  /* Ставим флаг о необходимости переоткрыть галс. */
  priv->opened = FALSE;
  priv->loaded = FALSE;
}

static guint32
hyscan_gtk_map_track_item_get_counts (HyScanGtkMapTrackItemSide *side,
                                      gdouble                    quality,
                                      guint32                    index,
                                      guint32                    n_counts)
{
  guint32 test_counts[20];
  gdouble test_values[20];
  const guint test_len = G_N_ELEMENTS (test_counts);
  guint i;

  if (side->quality == NULL)
    return n_counts;

  for (i = 0; i < test_len; i++)
    test_counts[i] = (i + 1) * n_counts / (test_len + 1);

  if (!hyscan_quality_get_values (side->quality, index, test_counts, test_values, test_len))
    return n_counts;

  for (i = 0; i < test_len; i++)
    {
      if (test_values[i] < quality)
        return test_counts[i];
    }

  return n_counts;
}

/* Определяет отображаемую длину луча по борту side для индекса index. */
static void
hyscan_gtk_map_track_item_load_length_by_idx (HyScanGtkMapTrackItem     *track,
                                              HyScanGtkMapTrackItemSide *side,
                                              HyScanGtkMapTrackPoint    *point)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  HyScanAntennaOffset *amp_offset = &side->offset;
  gint64 time;
  guint32 n_points, count;
  gdouble depth;
  gdouble length;

  hyscan_amplitude_get_size_time (side->amplitude, point->index, &n_points, &time);

  /* Определяем глубину под собой. */
  if (priv->depth.meter != NULL)
    {
      HyScanAntennaOffset *depth_offset = &priv->depth.offset;

      depth = hyscan_depthometer_get (priv->depth.meter, NULL, time);
      if (depth < 0)
        depth = 0;

      depth += depth_offset->vertical;
    }
  else
    {
      depth = 0;
    }
  depth -= amp_offset->vertical;

  /* Используем дальность, в пределах которой достигнуто необходимое качество. */
   count = hyscan_gtk_map_track_item_get_counts (side, priv->quality, point->index, n_points);

  /* Проекция дальности. */
  if (hyscan_projector_count_to_coord (side->projector, count, &length, depth))
    point->b_length_m = length;
  else
    point->b_length_m = 0;

  /* Проекция ближней зоны. */
  point->nr_length_m = side->near_field > depth ? sqrt (side->near_field * side->near_field - depth * depth) : 0.0;
  point->nr_length_m = MIN (point->nr_length_m, point->b_length_m);
}

/* Определяет, находится ли точка на прямолинейном отрезке. */
static gboolean
hyscan_gtk_map_track_item_is_straight (GList *l_point)
{
  HyScanGtkMapTrackPoint *point = l_point->data;
  gdouble prev_dist, next_dist;
  gdouble min_distance;
  gboolean is_straight = TRUE;
  GList *list;

  min_distance = STRAIGHT_LINE_MIN_DIST / point->scale;

  /* Смотрим предыдущие строки. */
  list = l_point->prev;
  prev_dist = 0.0;
  while (prev_dist < min_distance && list != NULL)
    {
      HyScanGtkMapTrackPoint *point_prev = list->data;

      if (ABS (point->b_angle - point_prev->b_angle) > STRAIGHT_LINE_MAX_ANGLE)
        {
          is_straight = FALSE;
          break;
        }

      prev_dist = ABS (point->dist_along - point_prev->dist_along);
      list = list->prev;
    }

  /* Смотрим следующие строки. */
  list = l_point->next;
  next_dist = 0.0;
  while (next_dist < min_distance && list != NULL)
    {
      HyScanGtkMapTrackPoint *point_next = list->data;

      if (ABS (point->b_angle - point_next->b_angle) > STRAIGHT_LINE_MAX_ANGLE)
        {
          is_straight = FALSE;
          break;
        }

      next_dist = ABS (point->dist_along - point_next->dist_along);
      list = list->next;
    }

  return is_straight || (next_dist + prev_dist >= min_distance);
}

/* Находит координаты точки destination, находящейся от точки point
 * на расстоянии length по направлению angle. */
static void
hyscan_gtk_map_track_item_move_point (HyScanGeoCartesian2D *point,
                                      gdouble               angle,
                                      gdouble               length,
                                      HyScanGeoCartesian2D *destination)
{
  destination->x = point->x + length * cos (angle);
  destination->y = point->y + length * sin (angle);
}

/* Вычисляет координаты галса в СК картографической проекции.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_item_cartesian (HyScanGtkMapTrackItem *track,
                                     GList                 *points)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  GList *point_l;

  /* Вычисляем положение приёмника GPS в картографической проекции. */
  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapTrackPoint *point = point_l->data;
      hyscan_geo_projection_geo_to_value (priv->projection, point->geo, &point->ship_c2d);
    }

  for (point_l = points; point_l != NULL; point_l = point_l->next)
    {
      HyScanGtkMapTrackPoint *point, *neighbour_point;
      HyScanGtkMapTrackItemSide *side;
      GList *neighbour;
      gdouble hdg, hdg_sin, hdg_cos;
      HyScanAntennaOffset *amp_offset, *nav_offset;

      point = point_l->data;
      point->scale = hyscan_geo_projection_get_scale (priv->projection, point->geo);

      /* Делаем поправки на смещение приёмника GPS: 1-2. */
      nav_offset = &priv->nav.offset;

      /* 1. Поправка курса. */
      hdg = point->geo.h / 180.0 * G_PI - nav_offset->yaw;
      hdg_sin = sin (hdg);
      hdg_cos = cos (hdg);

      /* 2. Поправка смещений x, y. */
      point->ship_c2d.x -= nav_offset->forward / point->scale * hdg_sin;
      point->ship_c2d.y -= nav_offset->forward / point->scale * hdg_cos;
      point->ship_c2d.x -= nav_offset->starboard / point->scale * hdg_cos;
      point->ship_c2d.y -= -nav_offset->starboard / point->scale * hdg_sin;

      /* Определяем положения антенн левого и правого бортов ГБО. */
      if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT)
        amp_offset = &priv->port.offset;
      else if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
        amp_offset = &priv->starboard.offset;
      else
        amp_offset = NULL;

      if (amp_offset != NULL)
        {
          point->b_angle = hdg - amp_offset->yaw;
          point->start_c2d.x = point->ship_c2d.x +
                                 (amp_offset->forward * hdg_sin + amp_offset->starboard * hdg_cos) / point->scale;
          point->start_c2d.y = point->ship_c2d.y +
                                 (amp_offset->forward * hdg_cos - amp_offset->starboard * hdg_sin) / point->scale;
        }

      /* Определяем расстояние от начала галса на основе соседней точки. */
      neighbour = point_l->prev;
      neighbour_point = neighbour != NULL ? neighbour->data : NULL;
      if (neighbour_point != NULL)
        {
          point->dist_along = neighbour_point->dist_along +
                              hyscan_cartesian_distance (&point->ship_c2d, &neighbour_point->ship_c2d);
        }
      else
        {
          point->dist_along = 0;
        }

      /* Правый и левый борт. */
      if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_LEFT)
        side = &priv->port;
      else if (point->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
        side = &priv->starboard;
      else
        side = NULL;

      if (side != NULL)
        {
          gdouble beam_width = side->beam_width;
          gdouble near_field;
          gdouble angle;

          /* Направление луча в СК картографической проекции. */
          if (side->source == HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_RIGHT)
            angle = -point->b_angle;
          else
            angle = G_PI - point->b_angle;

          /* Переводим из метров в масштаб проекции. */
          point->b_dist = point->b_length_m / point->scale;
          near_field = side->near_field / point->scale;
          point->aperture = side->antenna_length / point->scale;

          /* Урезаем ближнюю зону до длины луча при необходимости. */
          near_field = MIN (near_field, point->b_dist);

          /* Находим координаты точек диаграммы направленности. */
          hyscan_gtk_map_track_item_move_point (&point->start_c2d, angle, point->b_dist, &point->fr_c2d);
          hyscan_gtk_map_track_item_move_point (&point->start_c2d, angle, near_field, &point->nr_c2d);
          hyscan_gtk_map_track_item_move_point (&point->start_c2d, angle + beam_width / 2, point->b_dist, &point->fr1_c2d);
          hyscan_gtk_map_track_item_move_point (&point->start_c2d, angle - beam_width / 2, point->b_dist, &point->fr2_c2d);
        }
    }

    /* todo: надо ли это, если есть качество? */
    for (point_l = points; point_l != NULL; point_l = point_l->next)
      {
        HyScanGtkMapTrackPoint *point = point_l->data;

        point->straight = hyscan_gtk_map_track_item_is_straight (point_l);
      }
}

/* Расширяет область from - to так, чтобы в нее поместилась точка point. */
inline static void
hyscan_gtk_map_track_item_extend (HyScanGtkMapTrackPoint *point,
                                  HyScanGeoCartesian2D   *from,
                                  HyScanGeoCartesian2D   *to)
{
  from->x = MIN (from->x, point->ship_c2d.x - 1.1 * point->b_dist);
  from->y = MIN (from->y, point->ship_c2d.y - 1.1 * point->b_dist);
  to->x   = MAX (to->x, point->ship_c2d.x + 1.1 * point->b_dist);
  to->y   = MAX (to->y, point->ship_c2d.y + 1.1 * point->b_dist);
}

/* Загружает новые точки по индексам навигации.
 *
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_item_load_nav (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  HyScanGtkMapTrackItemNav *nav = &priv->nav;
  GList *last_link;
  GList *points = NULL;
  guint32 first, last, index;
  guint32 mod_count;

  if (nav->lat_data == NULL)
    return;

  mod_count = hyscan_nav_data_get_mod_count (nav->lat_data);

  hyscan_nav_data_get_range (nav->lat_data, &first, &last);
  hyscan_gtk_map_track_item_remove_expired (nav->points, first, last);

  /* Определяем индекс навигационных данных, с которого надо начать загрузку. */
  if ((last_link = g_list_last (nav->points)) != NULL)
    index = ((HyScanGtkMapTrackPoint *) last_link->data)->index + 1;
  else
    index = first;

  for (; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoGeodetic coords;
      HyScanGtkMapTrackPoint point = { 0 };

      if (!hyscan_nav_data_get (nav->lat_data, NULL, index, &time, &coords.lat))
        continue;

      if (!hyscan_nav_data_get (nav->lon_data, NULL, index, &time, &coords.lon))
        continue;

      if (!hyscan_nav_data_get (nav->trk_data, NULL, index, &time, &coords.h))
        continue;

      point.source = HYSCAN_GTK_MAP_TRACK_DRAW_SOURCE_NAV;
      point.time = time;
      point.index = index;
      point.geo = coords;

      points = g_list_append (points, hyscan_gtk_map_track_point_copy(&point));
    }

  /* Переводим географические координаты в логические. */
  nav->points = g_list_concat (nav->points, points);
  hyscan_gtk_map_track_item_cartesian (track, points);

  /* Отмечаем на тайловом слое устаревшую область. */
  if (points != NULL)
    {
      GList *link;
      HyScanGtkMapTrackPoint *point, *prev_point;

      for (link = points; link != NULL; link = link->next)
        {
          if (link->prev == NULL)
            continue;

          prev_point = link->prev->data;
          point = link->data;

          hyscan_gtk_map_tiled_set_area_mod (HYSCAN_GTK_MAP_TILED (priv->tiled_layer),
                                             &point->ship_c2d, &prev_point->ship_c2d);

          /* Расширяем область, внутри которой находится изображение галса. */
          hyscan_gtk_map_track_item_extend (point, &priv->extent_from, &priv->extent_to);
        }
    }

  nav->mod_count = mod_count;
}

/* Загружает новые точки по индексам амплитуды на указанном борту.
 *
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_item_load_side (HyScanGtkMapTrackItem     *track,
                                     HyScanGtkMapTrackItemSide *side)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  HyScanGtkMapTrackItemNav *nav = &priv->nav;
  GList *points = NULL;
  guint32 index, first, last;
  GList *last_link;
  guint32 mod_count;

  if (side->amplitude == NULL || nav->lat_data == NULL)
    return;

  mod_count = hyscan_amplitude_get_mod_count (side->amplitude);
  hyscan_amplitude_get_range (side->amplitude, &first, &last);
  hyscan_gtk_map_track_item_remove_expired (side->points, first, last);

  /* Определяем индекс амплитудных данных, с которого надо начать загрузку. */
  if ((last_link = g_list_last (side->points)) != NULL)
    index = ((HyScanGtkMapTrackPoint *) last_link->data)->index + 1;
  else
    index = first;

  for (; index <= last; ++index)
    {
      gint64 time;
      HyScanGeoGeodetic coords;
      HyScanGtkMapTrackPoint point = { 0 };
      gboolean nav_found;

      /* Определяем время получения амплитудных данных. */
      hyscan_amplitude_get_size_time (side->amplitude, index, NULL, &time);

      /* Находим навигационные данные для этой метки времени. */
      nav_found = hyscan_nav_smooth_get (nav->lat_smooth, NULL, time, &coords.lat) &&
                  hyscan_nav_smooth_get (nav->lon_smooth, NULL, time, &coords.lon) &&
                  hyscan_nav_smooth_get (nav->trk_smooth, NULL, time, &coords.h);

      if (!nav_found)
        continue;

      point.source = side->source;
      point.index = index;
      point.geo = coords;
      hyscan_gtk_map_track_item_load_length_by_idx (track, side, &point);

      points = g_list_append (points, hyscan_gtk_map_track_point_copy (&point));
    }

  /* Переводим географические координаты в логические. */
  side->points = g_list_concat (side->points, points);
  hyscan_gtk_map_track_item_cartesian (track, points);

  /* Отмечаем на тайловом слое устаревшую область. */
  if (points != NULL)
    {
      GList *link;
      HyScanGtkMapTrackPoint *point;

      for (link = points->prev != NULL ? points->prev : points; link != NULL; link = link->next)
        {
          point = link->data;
          hyscan_gtk_map_tiled_set_area_mod (HYSCAN_GTK_MAP_TILED (priv->tiled_layer),
                                             &point->ship_c2d, &point->fr_c2d);

          /* Расширяем область, внутри которой находится изображение галса. */
          hyscan_gtk_map_track_item_extend (point, &priv->extent_from, &priv->extent_to);
        }
    }

  side->mod_count = mod_count;
}

/* Расширяет область from - to так, чтобы в нее поместились все точки points. */
static void
hyscan_gtk_map_track_item_update_extent (GList                *points,
                                         HyScanGeoCartesian2D *from,
                                         HyScanGeoCartesian2D *to)
{
  GList *link;

  for (link = points; link != NULL; link = link->next)
    hyscan_gtk_map_track_item_extend (link->data, from, to);
}

/* Удаляет из трека путевые точки, которые не попадают в указанный диапазон.
 * Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static void
hyscan_gtk_map_track_item_remove_expired (GList   *points,
                                          guint32  first_index,
                                          guint32  last_index)
{
  GList *point_l;
  HyScanGtkMapTrackPoint *point;

  if (points == NULL)
    return;

  /* Проходим все точки галса и оставляем только актуальную информацию: 1-3. */
  point_l = points;

  /* 1. Удаляем точки до first_index. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index >= first_index)
        break;

      hyscan_gtk_map_track_point_free (point);
      points = g_list_delete_link (points, point_l);

      point_l = next;
    }

  /* 2. Оставляем точки до last_index, по которым загружены данные бортов. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      if (point->index > last_index)
        break;

      point_l = next;
    }

  /* 3. Остальное удаляем. */
  while (point_l != NULL)
    {
      GList *next = point_l->next;
      point = point_l->data;

      hyscan_gtk_map_track_point_free (point);
      points = g_list_delete_link (points, point_l);

      point_l = next;
    }
}

static void
hyscan_gtk_map_track_item_open_side (HyScanGtkMapTrackItemPrivate *priv,
                                     HyScanGtkMapTrackItemSide    *side,
                                     HyScanSourceType              source)
{
  HyScanAcousticDataInfo info;
  gdouble lambda;
  guint channel = side->channel;

  /* Удаляем текущие объекты. */
  g_clear_object (&side->amplitude);
  g_clear_object (&side->quality);
  g_clear_object (&side->projector);
  side->writeable = FALSE;

  if (channel == 0)
    return;

  side->amplitude = HYSCAN_AMPLITUDE (hyscan_acoustic_data_new (priv->db, priv->cache, priv->project,
                                                                          priv->name, source, channel, FALSE));
  if (side->amplitude == NULL)
    {
      g_warning ("HyScanGtkMapTrackItem: failed to open acoustic data");
      return;
    }

  side->writeable = hyscan_amplitude_is_writable (side->amplitude);
  side->quality = hyscan_quality_new (side->amplitude, priv->nav.trk_data);
  side->projector = hyscan_projector_new (side->amplitude);
  side->offset = hyscan_amplitude_get_offset (side->amplitude);

  /* Параметры диаграммы направленности. */
  info = hyscan_amplitude_get_info (side->amplitude);
  lambda = SOUND_VELOCITY / info.signal_frequency;
  side->antenna_length = info.antenna_haperture > 0 ? info.antenna_haperture : DEFAULT_HAPERTURE;
  side->beam_width = asin (lambda / side->antenna_length);
  side->near_field = (side->antenna_length * side->antenna_length) / lambda;
}

static void
hyscan_gtk_map_track_item_open_nav (HyScanGtkMapTrackItemPrivate *priv)
{
  HyScanGtkMapTrackItemNav *nav = &priv->nav;

  g_clear_object (&nav->lat_data);
  g_clear_object (&nav->lon_data);
  g_clear_object (&nav->trk_data);
  g_clear_object (&nav->lat_smooth);
  g_clear_object (&nav->lon_smooth);
  g_clear_object (&nav->trk_smooth);
  nav->writeable = FALSE;

  if (nav->channel == 0)
    return;

  nav->lat_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                           priv->name, nav->channel,
                                                           HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LAT));
  nav->lon_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                           priv->name, nav->channel,
                                                           HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_LON));
  nav->trk_data = HYSCAN_NAV_DATA (hyscan_nmea_parser_new (priv->db, priv->cache, priv->project,
                                                           priv->name, nav->channel,
                                                           HYSCAN_NMEA_DATA_RMC, HYSCAN_NMEA_FIELD_TRACK));
  nav->lat_smooth = hyscan_nav_smooth_new (nav->lat_data);
  nav->lon_smooth = hyscan_nav_smooth_new (nav->lon_data);
  nav->trk_smooth = hyscan_nav_smooth_new_circular (nav->trk_data);
  nav->writeable = hyscan_nav_data_is_writable (nav->lat_data);
  nav->offset = hyscan_nav_data_get_offset (nav->lat_data);
}

static void
hyscan_gtk_map_track_item_open_depth (HyScanGtkMapTrackItemPrivate *priv)
{
  HyScanNMEAParser *dpt_parser;
  
  g_clear_object (&priv->depth.meter);
  
  if (priv->depth.channel == 0)
    return;
  
  dpt_parser = hyscan_nmea_parser_new (priv->db, priv->cache,
                                       priv->project, priv->name, priv->depth.channel,
                                       HYSCAN_NMEA_DATA_DPT, HYSCAN_NMEA_FIELD_DEPTH);
  if (dpt_parser == NULL)
    return;
  
  priv->depth.meter = hyscan_depthometer_new (HYSCAN_NAV_DATA (dpt_parser), priv->cache);
  priv->depth.offset = hyscan_nav_data_get_offset (HYSCAN_NAV_DATA (dpt_parser));
  g_object_unref (dpt_parser);
}

/* Открывает (или переоткрывает) каналы данных в указанном галсе.
 * Функция должна вызываться за g_rw_lock_writer_lock(). */
static void
hyscan_gtk_map_track_item_open (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;

  hyscan_gtk_map_track_item_open_nav (priv);
  hyscan_gtk_map_track_item_open_depth (priv);
  hyscan_gtk_map_track_item_open_side (priv, &priv->starboard, HYSCAN_SOURCE_SIDE_SCAN_STARBOARD);
  hyscan_gtk_map_track_item_open_side (priv, &priv->port, HYSCAN_SOURCE_SIDE_SCAN_PORT);

  priv->loaded = FALSE;
  priv->opened = TRUE;
}

/* Проверяет, изменились ли данные галса. */
static gboolean
hyscan_gtk_map_track_item_has_changed (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  HyScanGtkMapTrackItemNav *nav = &priv->nav;
  HyScanGtkMapTrackItemSide *port = &priv->port, *starboard = &priv->starboard;

  if (nav->lat_data == NULL)
    return FALSE;

  if (!priv->loaded)
    return TRUE;

  return (nav->writeable && nav->mod_count != hyscan_nav_data_get_mod_count (nav->lat_data)) ||
         (port->writeable && port->mod_count != hyscan_amplitude_get_mod_count (port->amplitude)) ||
         (starboard->writeable && starboard->mod_count != hyscan_amplitude_get_mod_count (starboard->amplitude));
}

static void
hyscan_gtk_map_track_item_reset_extent (HyScanGtkMapTrackItemPrivate *priv)
{
  priv->extent_from.x = G_MAXDOUBLE;
  priv->extent_from.y = G_MAXDOUBLE;
  priv->extent_to.x = -G_MAXDOUBLE;
  priv->extent_to.y = -G_MAXDOUBLE;
}

/* Загружает путевые точки трека и его ширину. Возвращает %TRUE, если данные изменились.
   Функция должна вызываться за g_rw_lock_writer_lock (&track->lock) */
static gboolean
hyscan_gtk_map_track_item_load (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv = track->priv;
  gboolean proj_changed;

  /* Проверяем, не изменилась ли проекция. */
  g_mutex_lock (&priv->mutex);
  if ((proj_changed = (priv->new_projection != NULL)))
    {
      g_clear_object (&priv->projection);
      priv->projection = priv->new_projection;
      priv->new_projection = NULL;
    }
  g_mutex_unlock (&priv->mutex);

  /* Если проекция изменилась, то пересчитываем координаты точек. */
  if (proj_changed)
    {
      hyscan_gtk_map_track_item_cartesian (track, priv->nav.points);
      hyscan_gtk_map_track_item_cartesian (track, priv->port.points);
      hyscan_gtk_map_track_item_cartesian (track, priv->starboard.points);

      /* Обновляем границы галса. */
      hyscan_gtk_map_track_item_reset_extent (priv);
      hyscan_gtk_map_track_item_update_extent (priv->nav.points, &priv->extent_from, &priv->extent_to);
      hyscan_gtk_map_track_item_update_extent (priv->port.points, &priv->extent_from, &priv->extent_to);
      hyscan_gtk_map_track_item_update_extent (priv->starboard.points, &priv->extent_from, &priv->extent_to);
    }

  /* Открываем каналы данных. */
  if (!priv->opened)
    hyscan_gtk_map_track_item_open (track);

  /* Если уже загружены актуальные данные, то всё ок. */
  else if (!hyscan_gtk_map_track_item_has_changed (track))
    return FALSE;

  /* Приводим список точек в соответствие с флагом priv->loaded. */
  if (!priv->loaded)
    {
      hyscan_gtk_map_track_item_reset_extent (priv);
      g_list_free_full (priv->nav.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);
      g_list_free_full (priv->port.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);
      g_list_free_full (priv->starboard.points, (GDestroyNotify) hyscan_gtk_map_track_point_free);
      priv->nav.points = NULL;
      priv->port.points = NULL;
      priv->starboard.points = NULL;
    }

  /* Загружаем точки. */
  hyscan_gtk_map_track_item_load_nav (track);
  hyscan_gtk_map_track_item_load_side (track, &priv->port);
  hyscan_gtk_map_track_item_load_side (track, &priv->starboard);

  priv->loaded = TRUE;

  return TRUE;
}

/**
 * hyscan_gtk_map_track_item_new:
 * @db: указатель на базу данных #HyScanDB
 * @cache: кэш #HyScanCache
 * @project_name: название проекта
 * @track_name: название галса
 *
 * Создаёт галс с параметрами по умолчанию.
 *
 * Returns: новый объект #HyScanGtkMapTrackItem. Для удаления g_object_unref().
 */
HyScanGtkMapTrackItem *
hyscan_gtk_map_track_item_new (HyScanDB            *db,
                               HyScanCache         *cache,
                               const gchar         *project_name,
                               const gchar         *track_name,
                               HyScanGtkMapTiled   *tiled_layer,
                               HyScanGeoProjection *projection)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_ITEM,
                       "db", db,
                       "cache", cache,
                       "tiled-layer", tiled_layer,
                       "project", project_name,
                       "name", track_name,
                       "projection", projection, NULL);
}

/**
 * hyscan_gtk_map_track_item_has_nmea:
 * @track: указатель на #HyScanGtkMapTrackItem
 *
 * Определяет, установлен ли в галсе канал навигационных данных
 *
 * Returns: возвращает %TRUE, если канал навигационных данных установлен
 */
gboolean
hyscan_gtk_map_track_item_has_nmea (HyScanGtkMapTrackItem *track)
{
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track), FALSE);

  return track->priv->nav.channel > 0;
}

/**
 * hyscan_gtk_map_track_item_points_lock:
 * @track: указатель на #HyScanGtkMapTrackItem
 * @data: (out): структура со списком точек
 *
 * Функция получает список точек галса, одновременное блокируя к ним доступ другим
 * пользователям. После того, как работа со списком завершена, необходимо разблокировать
 * его вызовом hyscan_gtk_map_track_item_points_unlock().
 */
gboolean
hyscan_gtk_map_track_item_points_lock (HyScanGtkMapTrackItem     *track,
                                       HyScanGtkMapTrackDrawData *data)
{
  HyScanGtkMapTrackItemPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track), FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  priv = track->priv;

  /* Загружаем новые данные по треку (если что-то изменилось в канале данных). */
  g_rw_lock_writer_lock (&priv->lock);
  hyscan_gtk_map_track_item_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  g_rw_lock_reader_lock (&priv->lock);
  data->port = priv->port.points;
  data->starboard = priv->starboard.points;
  data->nav = priv->nav.points;
  data->from = priv->extent_from;
  data->to = priv->extent_to;

  return TRUE;
}

/**
 * hyscan_gtk_map_track_item_points_unlock:
 * @track: указатель на #HyScanGtkMapTrackItem
 *
 * Функция разблокирует доступ к точкам галса, который ранее был заблокирован
 * вызовом hyscan_gtk_map_track_item_points_lock().
 */
void
hyscan_gtk_map_track_item_points_unlock (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track));

  priv = track->priv;
  g_rw_lock_reader_unlock (&priv->lock);
}

/**
 * hyscan_gtk_map_track_item_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrack
 * @track_name: название галса
 * @from: (out): координата левой верхней точки границы
 * @to: (out): координата правой нижней точки границы
 *
 * Возвращает границы области, в которой находится галс.
 *
 * Returns: %TRUE, если получилось определить границы галса; иначе %FALSE
 */
gboolean
hyscan_gtk_map_track_item_view (HyScanGtkMapTrackItem *track,
                                HyScanGeoCartesian2D  *from,
                                HyScanGeoCartesian2D  *to)
{
  HyScanGtkMapTrackItemPrivate *priv;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track), FALSE);
  priv = track->priv;

  /* Если данные по галсу не загружены, то загружаем. */
  g_rw_lock_writer_lock (&priv->lock);
  hyscan_gtk_map_track_item_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  g_rw_lock_reader_lock (&priv->lock);
  if (priv->nav.points == NULL)
    {
      g_rw_lock_reader_unlock (&priv->lock);

      return FALSE;
    }

  *from = priv->extent_from;
  *to = priv->extent_to;
  g_rw_lock_reader_unlock (&priv->lock);

  return TRUE;
}

/**
 * hyscan_gtk_map_track_item_update:
 * @track: указатель на #HyScanGtkMapTrackItem
 *
 * Проверяет, есть ли новые данные в галсе и есть есть, то загружает их
 *
 * Returns: %TRUE, если были загружены новые данные
 */
gboolean
hyscan_gtk_map_track_item_update (HyScanGtkMapTrackItem *track)
{
  HyScanGtkMapTrackItemPrivate *priv;
  gboolean any_changes;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track), FALSE);
  priv = track->priv;

  g_rw_lock_writer_lock (&priv->lock);
  any_changes = hyscan_gtk_map_track_item_load (track);
  g_rw_lock_writer_unlock (&priv->lock);

  return any_changes;
}

/**
 * hyscan_gtk_map_track_item_set_projection:
 * @track: указатель на #HyScanGtkMapTrackItem
 * @projection: проекция #HyScanGeoProjection
 *
 * Устанавливает картографическую проекцию, в которой рисуется изображение
 * галса.
 */
void
hyscan_gtk_map_track_item_set_projection (HyScanGtkMapTrackItem *track,
                                          HyScanGeoProjection   *projection)
{
  HyScanGtkMapTrackItemPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_ITEM (track));
  priv = track->priv;

  /* Пока что не перепроецируем, чтобы не блокировать GUI, а просто запоминаем новую проекцию. */
  g_mutex_lock (&priv->mutex);
  g_clear_object (&priv->new_projection);
  priv->new_projection = g_object_ref (projection);
  g_mutex_unlock (&priv->mutex);
}

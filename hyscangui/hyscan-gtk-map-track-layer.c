/* hyscan-gtk-map-track-layer.c
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
 * SECTION: hyscan-gtk-map-track-layer
 * @Short_description: слой карты с изображением галсов
 * @Title: HyScanGtkMapTrackLayer
 * @See_also: #HyScanGtkLayer
 *
 * Слой отображает галсы выбранного проекта. Каждый галс изображется линией
 * движения, которая строится по GPS-отметкам, и линиями ширины трека.
 *
 * Для создания и отображения слоя используются функции:
 * - hyscan_gtk_map_track_layer_new() создает слой
 * - hyscan_gtk_map_track_layer_track_enable() устанавливает видимость
 *   указанного галса
 *
 * По умолчанию слой самостоятельно выбирает одни из доступных каналов данных
 * для получения навигационной и акустической информации по галсу. Чтобы получить
 * или установить номера используемых каналов, следует обратиться к функциям:
 * - hyscan_gtk_map_track_layer_track_get_channel()
 * - hyscan_gtk_map_track_layer_track_max_channel()
 * - hyscan_gtk_map_track_layer_track_set_channel().
 *
 * Стиль оформления галсов можно определить с помощью следующих сеттеров:
 * - hyscan_gtk_map_track_layer_set_color_track()
 * - hyscan_gtk_map_track_layer_set_color_port()
 * - hyscan_gtk_map_track_layer_set_color_starboard()
 * - hyscan_gtk_map_track_layer_set_bar_width()
 * - hyscan_gtk_map_track_layer_set_bar_margin()
 *
 * Также слой поддерживает загрузку конфигурационных ini-файлов. Подробнее в
 * функции hyscan_gtk_layer_container_load_key_file().
 *
 */

#include "hyscan-gtk-map-track-layer.h"
#include <hyscan-cartesian.h>
#include <hyscan-gtk-map.h>
#include <hyscan-nmea-parser.h>
#include <hyscan-projector.h>
#include <hyscan-acoustic-data.h>
#include <hyscan-depthometer.h>
#include <math.h>

/* Стиль оформления по умолчанию. */
#define DEFAULT_COLOR_TRACK           "#AC67C6"                     /* Цвет линии движения. */
#define DEFAULT_COLOR_PORT            "#DDC3BB"                     /* Цвет левого борта. */
#define DEFAULT_COLOR_STARBOARD       "#C4DDBB"                     /* Цвет правого борта. */
#define DEFAULT_COLOR_STROKE          "#000000"                     /* Цвет обводки некоторых элементов. */
#define DEFAULT_COLOR_SHADOW          "rgba(150, 150, 150, 0.5)"    /* Цвет затенения. */
#define DEFAULT_BAR_WIDTH             3                             /* Ширина полос дальности. */
#define DEFAULT_BAR_MARGIN            30                            /* Расстояние между соседними полосами. */
#define DEFAULT_LINE_WIDTH            1                             /* Толщина линии движения. */
#define DEFAULT_STROKE_WIDTH          1.0                           /* Толщина обводки. */

#define REFRESH_INTERVAL              500                           /* Период проверки каналов данных на новые данные, мс. */

/* Раскомментируйте строку ниже для вывода отладочной информации о скорости отрисовки слоя. */
// #define HYSCAN_GTK_MAP_DEBUG_FPS

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_CACHE,
  PROP_LIST_MODEL
};

struct _HyScanGtkMapTrackLayerPrivate
{
  HyScanGtkMap              *map;              /* Карта. */
  gboolean                   visible;          /* Признак видимости слоя. */
  guint                      draw_tag;         /* Тег функции перерисовки слоя. */

  HyScanDB                  *db;               /* База данных. */
  gchar                     *project;          /* Название проекта. */
  HyScanListModel           *list_model;       /* Модель списка активных галсов. */
  HyScanCache               *cache;            /* Кэш для навигационных данных. */

  GMutex                     t_lock;           /* Доступ к модификации таблицы tracks. */
  GHashTable                *tracks;           /* Таблица отображаемых галсов:
                                                * ключ - название галса, значение - HyScanGtkMapTrackLayerTrack. */
  GRWLock                    a_lock;           /* Доступ к модификации массива active_tracks. */
  gchar                    **active_tracks;    /* NULL-терминированный массив названий видимых галсов. */

  HyScanGtkMapTrackStyle     style;            /* Стиль оформления. */
};

static void     hyscan_gtk_map_track_layer_interface_init           (HyScanGtkLayerInterface        *iface);
static void     hyscan_gtk_map_track_layer_set_property             (GObject                        *object,
                                                                     guint                           prop_id,
                                                                     const GValue                   *value,
                                                                     GParamSpec                     *pspec);
static void     hyscan_gtk_map_track_layer_object_constructed       (GObject                        *object);
static void     hyscan_gtk_map_track_layer_object_finalize          (GObject                        *object);
static void     hyscan_gtk_map_track_layer_fill_tile                (HyScanGtkMapTiledLayer         *tiled_layer,
                                                                     HyScanGtkMapTile               *tile);
static void     hyscan_gtk_map_track_layer_list_changed             (HyScanGtkMapTrackLayer         *track_layer);
static HyScanGtkMapTrack * hyscan_gtk_map_track_layer_get_track     (HyScanGtkMapTrackLayer         *track_layer,
                                                                     const gchar                    *track_name,
                                                                     gboolean                        create_if_not_exists);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackLayer, hyscan_gtk_map_track_layer, HYSCAN_TYPE_GTK_MAP_TILED_LAYER,
                         G_ADD_PRIVATE (HyScanGtkMapTrackLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_track_layer_interface_init))

static void
hyscan_gtk_map_track_layer_class_init (HyScanGtkMapTrackLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapTiledLayerClass *tiled_class = HYSCAN_GTK_MAP_TILED_LAYER_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_layer_set_property;
  object_class->constructed = hyscan_gtk_map_track_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_layer_object_finalize;

  tiled_class->fill_tile = hyscan_gtk_map_track_layer_fill_tile;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Db", "HyScanDb", HYSCAN_TYPE_DB,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name", NULL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("data-cache", "Data Cache", "HyScanCache", HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_LIST_MODEL,
    g_param_spec_object ("track-list-model", "Track List Model",
                         "The HyScanListModel with a list of active (visible) tracks",
                         HYSCAN_TYPE_LIST_MODEL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_layer_init (HyScanGtkMapTrackLayer *gtk_map_track_layer)
{
  gtk_map_track_layer->priv = hyscan_gtk_map_track_layer_get_instance_private (gtk_map_track_layer);
}

static void
hyscan_gtk_map_track_layer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      priv->project = g_value_dup_string (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_LIST_MODEL:
      priv->list_model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_track_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackStyle *style = &priv->style;

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->constructed (object);

  g_mutex_init (&priv->t_lock);
  g_rw_lock_init (&priv->a_lock);
  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  priv->active_tracks = g_new0 (gchar *, 1);

  g_signal_connect_swapped (priv->list_model, "changed", G_CALLBACK (hyscan_gtk_map_track_layer_list_changed), track_layer);

  /* Оформление трека. */
  style->bar_width = DEFAULT_BAR_WIDTH;
  style->bar_margin = DEFAULT_BAR_MARGIN;
  style->line_width = DEFAULT_LINE_WIDTH;
  style->stroke_width = DEFAULT_STROKE_WIDTH;
  gdk_rgba_parse (&style->color_left, DEFAULT_COLOR_PORT);
  gdk_rgba_parse (&style->color_right, DEFAULT_COLOR_STARBOARD);
  gdk_rgba_parse (&style->color_track, DEFAULT_COLOR_TRACK);
  gdk_rgba_parse (&style->color_stroke, DEFAULT_COLOR_STROKE);
  gdk_rgba_parse (&style->color_shadow, DEFAULT_COLOR_SHADOW);
}

static void
hyscan_gtk_map_track_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  g_mutex_clear (&priv->t_lock);
  g_rw_lock_clear (&priv->a_lock);
  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_free (priv->project);
  g_hash_table_destroy (priv->tracks);
  g_clear_pointer (&priv->active_tracks, g_strfreev);

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->finalize (object);
}

/* Обработчик сигнала HyScanListModel::changed. */
static void
hyscan_gtk_map_track_layer_list_changed (HyScanGtkMapTrackLayer *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_rw_lock_writer_lock (&priv->a_lock);
  g_clear_pointer (&priv->active_tracks, g_strfreev);
  priv->active_tracks = hyscan_list_model_get (priv->list_model);
  g_rw_lock_writer_unlock (&priv->a_lock);

  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

static void
hyscan_gtk_map_track_layer_fill_tile (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanGtkMapTile       *tile)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (tiled_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  gint i;

  HyScanGeoCartesian2D from, to;
  gdouble scale;

  cairo_t *cairo;
  cairo_surface_t *surface;
  guint tile_size;

  tile_size = hyscan_gtk_map_tile_get_size (tile);
  hyscan_gtk_map_tile_get_bounds (tile, &from, &to);
  scale = hyscan_gtk_map_tile_get_scale (tile);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile_size, tile_size);
  cairo = cairo_create (surface);

  g_rw_lock_reader_lock (&priv->a_lock);
  for (i = 0; priv->active_tracks[i] != NULL; ++i)
    {
      HyScanGtkMapTrack *track;

      track = hyscan_gtk_map_track_layer_get_track (track_layer, priv->active_tracks[i], TRUE);
      hyscan_gtk_map_track_draw (track, cairo, scale, &from, &to, &priv->style);
    }
  g_rw_lock_reader_unlock (&priv->a_lock);

  hyscan_gtk_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cairo);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_track_layer_draw (HyScanGtkMap            *map,
                                 cairo_t                 *cairo,
                                 HyScanGtkMapTrackLayer  *track_layer)
{
#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  static GTimer *debug_timer;
  static gdouble frame_time[25];
  static guint64 frame_idx = 0;

  if (debug_timer == NULL)
    debug_timer = g_timer_new ();
  g_timer_start (debug_timer);
  frame_idx++;
#endif

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    return;

  hyscan_gtk_map_tiled_layer_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer), cairo);


#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  {
    guint dbg_i = 0;
    gdouble dbg_time = 0;
    guint dbg_frames;

    dbg_frames = G_N_ELEMENTS (frame_time);

    frame_idx = (frame_idx + 1) % dbg_frames;
    frame_time[frame_idx] = g_timer_elapsed (debug_timer, NULL);
    for (dbg_i = 0; dbg_i < dbg_frames; ++dbg_i)
      dbg_time += frame_time[dbg_i];

    dbg_time /= (gdouble) dbg_frames;
    g_message ("hyscan_gtk_map_track_layer_draw: %.2f fps",
               1.0 / dbg_time);
  }
#endif
}

/* Обрабатывает сигнал об изменении картографической проекции. */
static void
hyscan_gtk_map_track_layer_projection_notify (HyScanGtkMap           *map,
                                              GParamSpec             *pspec,
                                              HyScanGtkMapTrackLayer *layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = layer->priv;
  GHashTableIter iter;

  gchar *track_name;
  HyScanGtkMapTrack *track;
  HyScanGeoProjection *projection;

  projection = hyscan_gtk_map_get_projection (map);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_name, (gpointer *) &track))
    hyscan_gtk_map_track_set_projection (track, projection);

  g_object_unref (projection);
}

/* Запрашивает перерисовку слоя, если есть изменения в каналах данных. */
static gboolean
hyscan_gtk_map_track_layer_redraw (gpointer data)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (data);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  GHashTableIter iter;
  gboolean any_changes;

  HyScanGtkMapTrack *track;
  gchar *key;

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    return G_SOURCE_CONTINUE;

  /* Загружаем новые данные из каждого галса и запоминаем, есть ли изменения.. */
  any_changes = FALSE;
  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
    any_changes = hyscan_gtk_map_track_update (track) || any_changes;

  if (any_changes)
    hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));

  return G_SOURCE_CONTINUE;
}

/* Реализация HyScanGtkLayerInterface.added.
 * Обрабатывает добавление слой на карту. */
static void
hyscan_gtk_map_track_layer_added (HyScanGtkLayer          *gtk_layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);
  g_signal_connect_after (priv->map, "visible-draw", 
                          G_CALLBACK (hyscan_gtk_map_track_layer_draw), track_layer);
  g_signal_connect (priv->map, "notify::projection",
                    G_CALLBACK (hyscan_gtk_map_track_layer_projection_notify), track_layer);

  hyscan_gtk_layer_parent_interface->added (gtk_layer, container);
  priv->draw_tag = g_timeout_add (REFRESH_INTERVAL, hyscan_gtk_map_track_layer_redraw, track_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Обрабатывает удаление слоя с карты. */
static void
hyscan_gtk_map_track_layer_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_source_remove (priv->draw_tag);
  priv->draw_tag = 0;
  hyscan_gtk_layer_parent_interface->removed (gtk_layer);

  g_signal_handlers_disconnect_by_data (priv->map, track_layer);
  g_clear_object (&priv->map);

  /* Удаляем все галсы, потому что они держат ссылку на track_layer и не дадут ему удалиться. */
  g_hash_table_remove_all (priv->tracks);
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_track_layer_set_visible (HyScanGtkLayer *layer,
                                        gboolean        visible)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_track_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  return priv->visible;
}

/* Реализация HyScanGtkLayerInterface.load_key_file.
 * Загружает параметры отображения слоя из конфигурационного файла. */
static gboolean
hyscan_gtk_map_track_layer_load_key_file (HyScanGtkLayer *gtk_layer,
                                          GKeyFile       *key_file,
                                          const gchar    *group)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (gtk_layer);
  GdkRGBA rgba;
  gdouble value;

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "track-color", DEFAULT_COLOR_TRACK);
  hyscan_gtk_map_track_layer_set_color_track (track_layer, rgba);

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "port-color", DEFAULT_COLOR_PORT);
  hyscan_gtk_map_track_layer_set_color_port (track_layer, rgba);

  hyscan_gtk_layer_load_key_file_rgba (&rgba, key_file, group,
                                       "starboard-color", DEFAULT_COLOR_STARBOARD);
  hyscan_gtk_map_track_layer_set_color_starboard (track_layer, rgba);

  value = g_key_file_get_double (key_file, group, "bar-width", NULL);
  hyscan_gtk_map_track_layer_set_bar_width (track_layer, (value > 0.0) ? value : DEFAULT_BAR_WIDTH);

  value = g_key_file_get_double (key_file, group, "bar-margin", NULL);
  hyscan_gtk_map_track_layer_set_bar_margin (track_layer, (value > 0.0) ? value : DEFAULT_BAR_MARGIN);

  return TRUE;
}

static void
hyscan_gtk_map_track_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->load_key_file = hyscan_gtk_map_track_layer_load_key_file;
  iface->added = hyscan_gtk_map_track_layer_added;
  iface->removed = hyscan_gtk_map_track_layer_removed;
  iface->set_visible = hyscan_gtk_map_track_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_track_layer_get_visible;
}

/* Ищет галс в хэш-таблице; если галс не найден, то создает новый и добавляет
 * его в таблицу. */
static HyScanGtkMapTrack *
hyscan_gtk_map_track_layer_get_track (HyScanGtkMapTrackLayer *track_layer,
                                      const gchar            *track_name,
                                      gboolean                create_if_not_exists)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapTrack *track;

  g_mutex_lock (&priv->t_lock);
  track = g_hash_table_lookup (priv->tracks, track_name);
  if (track == NULL && create_if_not_exists)
    {
      HyScanGeoProjection *projection;

      projection = hyscan_gtk_map_get_projection (priv->map);
      track = hyscan_gtk_map_track_new (priv->db, priv->cache, priv->project, track_name,
                                        HYSCAN_GTK_MAP_TILED_LAYER (track_layer), projection);
      g_hash_table_insert (priv->tracks, g_strdup (track_name), track);
      g_object_unref (projection);
    }
  g_mutex_unlock (&priv->t_lock);

  return track;
}

/**
 * hyscan_gtk_map_track_layer_new:
 * @db: база данных #HyScanDB
 * @project: название проекта
 * @list_model: модель данных активных галсов
 * @cache: кэш данных
 *
 * Returns: указатель на новый слой #HyScanGtkMapTrackLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_track_layer_new (HyScanDB             *db,
                                const gchar          *project,
                                HyScanListModel      *list_model,
                                HyScanCache          *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LAYER,
                       "db", db,
                       "project", project,
                       "track-list-model", list_model,
                       "data-cache", cache, NULL);
}

/**
 * hyscan_gtk_map_track_layer_track_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrackLayer
 * @track_name: название галса
 *
 * Показывает галс @track_name на карте. Функция устанавливает границы видимой
 * области карты так, чтобы галс был виден полностью.
 *
 * Returns: %TRUE, если получилось определить границы галса; иначе %FALSE
 */
gboolean 
hyscan_gtk_map_track_layer_track_view (HyScanGtkMapTrackLayer *track_layer,
                                       const gchar            *track_name)
{
  HyScanGtkMapTrackLayerPrivate *priv;
  HyScanGtkMapTrack *track;
  HyScanGeoCartesian2D from, to;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer), FALSE);
  g_return_val_if_fail (track_name != NULL, FALSE);
  priv = track_layer->priv;
  
  g_return_val_if_fail (priv->map != NULL, FALSE);

  /* Находим запрошенный галс. */
  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name, TRUE);

  hyscan_gtk_map_track_view (track, &from, &to);
  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map), from.x, to.x, from.y, to.y);

  return TRUE;
}

/**
 * hyscan_gtk_map_track_layer_set_color_track:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет линии движения
 *
 * Устанавливает цвет линии движения судна.
 */
void
hyscan_gtk_map_track_layer_set_color_track (HyScanGtkMapTrackLayer *track_layer,
                                            GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->style.color_track = color;
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

/**
 * hyscan_gtk_map_track_layer_set_color_port:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет полос дальности для левого борта
 *
 * Устанавливает цвет полос дальности для левого борта.
 */
void
hyscan_gtk_map_track_layer_set_color_port (HyScanGtkMapTrackLayer *track_layer,
                                           GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->style.color_left = color;
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

/**
 * hyscan_gtk_map_track_layer_set_color_starboard:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @color: цвет полос дальности для правого борта
 *
 * Устанавливает цвет полос дальности для правого борта.
 */
void
hyscan_gtk_map_track_layer_set_color_starboard (HyScanGtkMapTrackLayer *track_layer,
                                                GdkRGBA                 color)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->style.color_right = color;
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

/**
 * hyscan_gtk_map_track_layer_set_bar_width:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @bar_width: ширина полосы в пикселях
 *
 * Устанавливает ширину линии дальности.
 */
void
hyscan_gtk_map_track_layer_set_bar_width (HyScanGtkMapTrackLayer *track_layer,
                                          gboolean                 bar_width)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->style.bar_width = bar_width;
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

/**
 * hyscan_gtk_map_track_layer_set_bar_margin:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @bar_margin: отступ в пикселях вдоль линии трека
 *
 * Устанавливает расстоянием между двумя соседними линиями дальности.
 */
void
hyscan_gtk_map_track_layer_set_bar_margin (HyScanGtkMapTrackLayer *track_layer,
                                           gdouble                 bar_margin)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->style.bar_margin = bar_margin;

  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (track_layer));
}

/**
 * hyscan_gtk_map_track_layer_lookup:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @track_name: название галса
 *
 * Returns: (transfer full): указатель на найденный галс #HyScanGtkMapTrack.
 *   Для удаления g_object_unref().
 */
HyScanGtkMapTrack *
hyscan_gtk_map_track_layer_lookup (HyScanGtkMapTrackLayer *track_layer,
                                   const gchar            *track_name)
{
  HyScanGtkMapTrack *track;
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer), NULL);

  track = hyscan_gtk_map_track_layer_get_track (track_layer, track_name, TRUE);

  return g_object_ref (track);
}

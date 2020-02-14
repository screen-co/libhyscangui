/* hyscan-gtk-map-track.c
 *
 * Copyright 2019-2020 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
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
 * @Title: HyScanGtkMapTrack
 * @See_also: #HyScanGtkLayer
 *
 * Слой отображает галсы выбранного проекта. Каждый галс изображется линией
 * движения, которая строится по GPS-отметкам, и линиями ширины трека.
 *
 * Конкретный вид галса определяется используемым классом отрисовки галса. См.
 * интерфейс #HyScanGtkMapTrackDraw и его реализацию в классах
 * - #HyScanGtkMapTrackDrawBar - траектория движения и линии дальности,
 * - #HyScanGtkMapTrackDrawBeam - заливка области покрытия.
 *
 * Для создания и отображения слоя используются функции:
 * - hyscan_gtk_map_track_new() создает слой
 * - hyscan_gtk_map_track_set_tracks() устанавливает список видимых галсов
 *
 * По умолчанию слой самостоятельно выбирает одни из доступных каналов данных
 * для получения навигационной и акустической информации по галсу. Чтобы получить
 * или установить номера используемых каналов, следует получить объект галса #HyScanGtkMapTrackItem
 * и установить его параметры через HyScanParam:
 * - hyscan_gtk_map_track_lookup() - получение объекта галса
 *
 * Также слой поддерживает загрузку стилей через #HyScanParam. Подробнее в
 * функции hyscan_gtk_layer_get_param().
 *
 */

#include "hyscan-gtk-map-track.h"
#include "hyscan-gtk-map-track-draw-bar.h"
#include "hyscan-gtk-map-track-draw.h"
#include "hyscan-gtk-map.h"
#include "hyscan-gtk-map-track-draw-beam.h"
#include <hyscan-cartesian.h>
#include <hyscan-projector.h>
#include <hyscan-param-proxy.h>

#define REFRESH_INTERVAL              300000     /* Период обновления данных, мкс. */

/* Раскомментируйте строку ниже для вывода отладочной информации о скорости отрисовки слоя. */
// #define HYSCAN_GTK_MAP_DEBUG_FPS

enum
{
  PROP_O,
  PROP_DB,
  PROP_PROJECT,
  PROP_CACHE
};

struct _HyScanGtkMapTrackPrivate
{
  HyScanGtkMap                   *map;              /* Карта. */
  gboolean                        visible;          /* Признак видимости слоя. */
  GThread                        *watcher;          /* Тег функции перерисовки слоя. */
  gboolean                        shutdown;         /* Признак удаления слоя. */

  HyScanDB                       *db;               /* База данных. */
  gchar                          *project;          /* Название проекта. */
  HyScanCache                    *cache;            /* Кэш для навигационных данных. */

  GMutex                          t_lock;           /* Доступ к модификации таблицы tracks. */
  GHashTable                     *tracks;           /* Таблица отображаемых галсов:
                                                     * ключ - название галса, значение - HyScanGtkMapTrackItem. */
  GRWLock                         a_lock;           /* Доступ к модификации массива active_tracks. */
  gchar                         **active_tracks;    /* NULL-терминированный массив названий видимых галсов. */

  HyScanGtkMapTrackDrawType       draw_type;        /* Активный тип объекта для рисования. */
  HyScanGtkMapTrackDraw          *draw_bar;         /* Объект для рисования галса полосами. */
  HyScanGtkMapTrackDraw          *draw_beam;        /* Объект для рисования галса лучами. */
  HyScanParamProxy               *param;            /* Параметры слоя. */
};

static void     hyscan_gtk_map_track_interface_init               (HyScanGtkLayerInterface     *iface);
static void     hyscan_gtk_map_track_set_property                 (GObject                     *object,
                                                                   guint                        prop_id,
                                                                   const GValue                *value,
                                                                   GParamSpec                  *pspec);
static void     hyscan_gtk_map_track_object_constructed           (GObject                     *object);
static void     hyscan_gtk_map_track_object_finalize              (GObject                     *object);
static void     hyscan_gtk_map_track_fill_tile                    (HyScanGtkMapTiled           *tiled_layer,
                                                                   HyScanMapTile               *tile);
static void     hyscan_gtk_map_track_param_set                    (HyScanGtkLayer              *gtk_layer);
static HyScanGtkMapTrackItem * hyscan_gtk_map_track_get_track     (HyScanGtkMapTrack           *track_layer,
                                                                   const gchar                 *track_name,
                                                                   gboolean                     create_if_not_exists);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrack, hyscan_gtk_map_track, HYSCAN_TYPE_GTK_MAP_TILED,
                         G_ADD_PRIVATE (HyScanGtkMapTrack)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_track_interface_init))

static void
hyscan_gtk_map_track_class_init (HyScanGtkMapTrackClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapTiledClass *tiled_class = HYSCAN_GTK_MAP_TILED_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_set_property;
  object_class->constructed = hyscan_gtk_map_track_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_object_finalize;

  tiled_class->fill_tile = hyscan_gtk_map_track_fill_tile;

  g_object_class_install_property (object_class, PROP_DB,
    g_param_spec_object ("db", "Db", "HyScanDb", HYSCAN_TYPE_DB,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PROJECT,
    g_param_spec_string ("project", "Project", "Project name", NULL,
                          G_PARAM_WRITABLE));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("data-cache", "Data Cache", "HyScanCache", HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_init (HyScanGtkMapTrack *gtk_map_track)
{
  HyScanGtkMapTrackPrivate *priv;

  gtk_map_track->priv = hyscan_gtk_map_track_get_instance_private (gtk_map_track);
  priv = gtk_map_track->priv;

  g_mutex_init (&priv->t_lock);
  g_rw_lock_init (&priv->a_lock);
  priv->tracks = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  priv->active_tracks = g_new0 (gchar *, 1);
}

static void
hyscan_gtk_map_track_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapTrack *gtk_map_track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = gtk_map_track->priv;

  switch (prop_id)
    {
    case PROP_DB:
      priv->db = g_value_dup_object (value);
      break;

    case PROP_PROJECT:
      hyscan_gtk_map_track_set_project (gtk_map_track, g_value_get_string (value));
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
hyscan_gtk_map_track_object_constructed (GObject *object)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;
  HyScanParam *child_param;

  G_OBJECT_CLASS (hyscan_gtk_map_track_parent_class)->constructed (object);

  /* Параметры слоя включают в себя параметры каждого из классов отрисовки. */
  priv->param = hyscan_param_proxy_new ();
  hyscan_param_proxy_node_set_name (priv->param, "/", "Tracks", NULL);

  /* Класс отрисовки полосами. */
  priv->draw_bar = hyscan_gtk_map_track_draw_bar_new ();
  g_signal_connect_swapped (priv->draw_bar, "param-changed", G_CALLBACK (hyscan_gtk_map_track_param_set), track_layer);
  child_param = hyscan_gtk_map_track_draw_get_param (priv->draw_bar);
  hyscan_param_proxy_add (priv->param, "/bar", child_param, "/");
  g_object_unref (child_param);

  /* Класс отрисовки лучами. */
  priv->draw_beam = hyscan_gtk_map_track_draw_beam_new ();
  g_signal_connect_swapped (priv->draw_beam, "param-changed", G_CALLBACK (hyscan_gtk_map_track_param_set), track_layer);
  child_param = hyscan_gtk_map_track_draw_get_param (priv->draw_beam);
  hyscan_param_proxy_add (priv->param, "/beam", child_param, "/");
  g_object_unref (child_param);

  hyscan_param_proxy_bind (priv->param);
}

static void
hyscan_gtk_map_track_object_finalize (GObject *object)
{
  HyScanGtkMapTrack *gtk_map_track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = gtk_map_track->priv;

  g_mutex_clear (&priv->t_lock);
  g_rw_lock_clear (&priv->a_lock);
  g_clear_object (&priv->db);
  g_clear_object (&priv->cache);
  g_clear_object (&priv->draw_bar);
  g_clear_object (&priv->draw_beam);
  g_clear_object (&priv->param);
  g_free (priv->project);
  g_hash_table_destroy (priv->tracks);
  g_clear_pointer (&priv->active_tracks, g_strfreev);

  G_OBJECT_CLASS (hyscan_gtk_map_track_parent_class)->finalize (object);
}

/* Функция рисует на тайле все активные галсы. */
static void
hyscan_gtk_map_track_fill_tile (HyScanGtkMapTiled *tiled_layer,
                                HyScanMapTile     *tile)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (tiled_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackDrawType draw_type;
  HyScanGtkMapTrackDraw *track_draw;
  gchar **active_tracks;
  gint i;

  HyScanGeoCartesian2D from, to;
  HyScanGeoCartesian2D max, min;
  gdouble scale;

  cairo_t *cairo;
  cairo_surface_t *surface;
  guint tile_size;

  tile_size = hyscan_map_tile_get_size (tile);
  hyscan_map_tile_get_bounds (tile, &from, &to);
  scale = hyscan_map_tile_get_scale (tile);
  max.x = MAX (from.x, to.x);
  max.y = MAX (from.y, to.y);
  min.x = MIN (from.x, to.x);
  min.y = MIN (from.y, to.y);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile_size, tile_size);
  cairo = cairo_create (surface);

  /* Определяем, кто будет рисовать галс. */
  draw_type = g_atomic_int_get (&priv->draw_type);
  if (draw_type == HYSCAN_GTK_MAP_TRACK_BEAM)
    track_draw = priv->draw_beam;
  else
    track_draw = priv->draw_bar;

  active_tracks = hyscan_gtk_map_track_get_tracks (track_layer);
  for (i = 0; active_tracks[i] != NULL; ++i)
    {
      HyScanGtkMapTrackDrawData data;
      HyScanGtkMapTrackItem *track;

      track = hyscan_gtk_map_track_get_track (track_layer, active_tracks[i], TRUE);
      if (track == NULL)
        continue;

      if (!hyscan_gtk_map_track_item_points_lock (track, &data))
        continue;

      /* Рисуем галс только в том случае, если он лежит внутри тайла. */
      if (!(data.to.x < min.x || data.from.x > max.x || data.to.y < min.y || data.from.y > max.y))
        hyscan_gtk_map_track_draw_region (track_draw, &data, cairo, scale, &from, &to);

      hyscan_gtk_map_track_item_points_unlock (track);
    }
  g_strfreev (active_tracks);

  hyscan_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (cairo);
}

/* Рисует слой по сигналу "visible-draw". */
static void
hyscan_gtk_map_track_draw (HyScanGtkMap      *map,
                           cairo_t           *cairo,
                           HyScanGtkMapTrack *track_layer)
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

  /* Делегируем всё рисование тайловому слою. */
  hyscan_gtk_map_tiled_draw (HYSCAN_GTK_MAP_TILED (track_layer), cairo);


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
    g_message ("hyscan_gtk_map_track_draw: %.2f fps",
               1.0 / dbg_time);
  }
#endif
}

/* Обрабатывает сигнал об изменении картографической проекции. */
static void
hyscan_gtk_map_track_projection_notify (HyScanGtkMap      *map,
                                        GParamSpec        *pspec,
                                        HyScanGtkMapTrack *layer)
{
  HyScanGtkMapTrackPrivate *priv = layer->priv;
  GHashTableIter iter;

  gchar *track_name;
  HyScanGtkMapTrackItem *track;
  HyScanGeoProjection *projection;

  projection = hyscan_gtk_map_get_projection (map);

  g_hash_table_iter_init (&iter, priv->tracks);
  while (g_hash_table_iter_next (&iter, (gpointer *) &track_name, (gpointer *) &track))
    hyscan_gtk_map_track_item_set_projection (track, projection);

  /* Сообщаем об изменении параметров и необходимости перерисовки. */
  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (layer));

  g_object_unref (projection);
}

/* Запрашивает перерисовку слоя, если есть изменения в каналах данных. */
static gpointer
hyscan_gtk_map_track_watcher (gpointer data)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (data);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  GHashTableIter iter;
  gboolean any_changes;

  HyScanGtkMapTrackItem *track;
  gchar *key;

  while (TRUE)
    {
      g_usleep (REFRESH_INTERVAL);

      if (g_atomic_int_get (&priv->shutdown))
        break;

      if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
        continue;

      /* Загружаем новые данные из каждого галса и запоминаем, есть ли изменения.. */
      any_changes = FALSE;
      g_hash_table_iter_init (&iter, priv->tracks);
      while (!any_changes && g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &track))
        any_changes = hyscan_gtk_map_track_item_update (track);

      if (any_changes)
        hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
    }

  return NULL;
}

/* Реализация HyScanGtkLayerInterface.added.
 * Обрабатывает добавление слой на карту. */
static void
hyscan_gtk_map_track_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (gtk_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  g_signal_connect_after (priv->map, "visible-draw", 
                          G_CALLBACK (hyscan_gtk_map_track_draw), track_layer);
  g_signal_connect (priv->map, "notify::projection",
                    G_CALLBACK (hyscan_gtk_map_track_projection_notify), track_layer);

  hyscan_gtk_layer_parent_interface->added (gtk_layer, container);
  priv->watcher = g_thread_new ("map-track", hyscan_gtk_map_track_watcher, track_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Обрабатывает удаление слоя с карты. */
static void
hyscan_gtk_map_track_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (gtk_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  g_return_if_fail (priv->map != NULL);

  g_atomic_int_set (&priv->shutdown, TRUE);
  g_clear_pointer (&priv->watcher, g_thread_join);
  hyscan_gtk_layer_parent_interface->removed (gtk_layer);

  g_signal_handlers_disconnect_by_data (priv->map, track_layer);
  g_clear_object (&priv->map);

  /* Удаляем все галсы, потому что они держат ссылку на track_layer и не дадут ему удалиться. */
  g_hash_table_remove_all (priv->tracks);
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_track_set_visible (HyScanGtkLayer *layer,
                                  gboolean        visible)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  priv->visible = visible;

  if (priv->map != NULL)
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_track_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  return priv->visible;
}

static void
hyscan_gtk_map_track_param_set (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (gtk_layer);

  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (track_layer));
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
}

/* Реализация HyScanGtkLayerInterface.get_param.
 * Получает параметры стиля оформления слоя. */
static HyScanParam *
hyscan_gtk_map_track_get_param (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (gtk_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  return g_object_ref (priv->param);
}

static void
hyscan_gtk_map_track_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->get_param = hyscan_gtk_map_track_get_param;
  iface->added = hyscan_gtk_map_track_added;
  iface->removed = hyscan_gtk_map_track_removed;
  iface->set_visible = hyscan_gtk_map_track_set_visible;
  iface->get_visible = hyscan_gtk_map_track_get_visible;
}

/* Ищет галс в хэш-таблице; если галс не найден, то создает новый и добавляет
 * его в таблицу. */
static HyScanGtkMapTrackItem *
hyscan_gtk_map_track_get_track (HyScanGtkMapTrack *track_layer,
                                const gchar       *track_name,
                                gboolean           create_if_not_exists)
{
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackItem *track;

  g_mutex_lock (&priv->t_lock);
  track = g_hash_table_lookup (priv->tracks, track_name);
  if (track == NULL && priv->project != NULL && create_if_not_exists)
    {
      HyScanGeoProjection *projection;

      projection = hyscan_gtk_map_get_projection (priv->map);
      track = hyscan_gtk_map_track_item_new (priv->db, priv->cache, priv->project, track_name,
                                             HYSCAN_GTK_MAP_TILED (track_layer), projection);
      g_hash_table_insert (priv->tracks, g_strdup (track_name), track);
      g_object_unref (projection);
    }
  g_mutex_unlock (&priv->t_lock);

  return track;
}

/* Коллбэк функция для hyscan_gtk_map_track_view. */
static void
hyscan_gtk_map_track_view_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      callback_data)
{
  HyScanGtkMapTrack *track = HYSCAN_GTK_MAP_TRACK (source_object);
  HyScanGtkMapTrackPrivate *priv = track->priv;
  GTask *task = G_TASK (res);
  HyScanGeoCartesian2D *view;
  gboolean zoom_in;
  gdouble prev_scale = -1.0;
  gdouble margin = 0;

  view = g_task_propagate_pointer (task, NULL);
  if (view == NULL)
    return;

  zoom_in = GPOINTER_TO_INT (callback_data);
  if (!zoom_in)
    gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &prev_scale, NULL);

  if (view[0].x == view[1].x || view[0].y == view[1].y)
    margin = prev_scale;

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map),
                           view[0].x - margin, view[1].x + margin,
                           view[0].y - margin, view[1].y + margin);

  if (!zoom_in)
    {
      gtk_cifro_area_set_scale (GTK_CIFRO_AREA (priv->map), prev_scale, prev_scale,
                                (view[0].x + view[1].x) / 2.0, (view[0].y + view[1].y) / 2.0);
    }

  gtk_cifro_area_set_view (GTK_CIFRO_AREA (priv->map), view[0].x, view[1].x, view[0].y, view[1].y);
  g_free (view);
}

/* Асинхронная загрузка координат галса. */
static void
hyscan_gtk_map_track_view_load (GTask        *task,
                                gpointer      source_object,
                                gpointer      task_data,
                                GCancellable *cancellable)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (source_object);
  HyScanGeoCartesian2D *view;
  HyScanGtkMapTrackItem *track;
  gchar *track_name;
  gboolean result = FALSE;

  view = g_new0 (HyScanGeoCartesian2D, 2);

  /* Находим запрошенный галс. */
  track_name = task_data;
  track = hyscan_gtk_map_track_get_track (track_layer, track_name, TRUE);
  if (track == NULL)
    goto exit;

  result = hyscan_gtk_map_track_item_view (track, &view[0], &view[1]);

exit:
  if (result)
    {
      g_task_return_pointer (task, view, g_free);
    }
  else
    {
      g_free (view);
      g_task_return_pointer (task, NULL, NULL);
    }
}

/**
 * hyscan_gtk_map_track_new:
 * @db: база данных #HyScanDB
 * @project: название проекта
 * @cache: кэш данных
 *
 * Returns: указатель на новый слой #HyScanGtkMapTrack
 */
HyScanGtkLayer *
hyscan_gtk_map_track_new (HyScanDB        *db,
                          HyScanCache     *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK,
                       "db", db,
                       "data-cache", cache, NULL);
}

/**
 * hyscan_gtk_map_track_set_project:
 * @track_layer: указатель на #HyScanGtkMapTrack
 * @project: название проекта
 *
 * Меняет название проекта.
 */
void
hyscan_gtk_map_track_set_project (HyScanGtkMapTrack *track_layer,
                                  const gchar       *project)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer));
  priv = track_layer->priv;

  g_mutex_lock (&priv->t_lock);

  /* Удаляем информацию о галсах. */
  g_hash_table_remove_all (priv->tracks);

  /* Меняем название проекта. */
  g_free (priv->project);
  priv->project = g_strdup (project);

  g_mutex_unlock (&priv->t_lock);

  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (track_layer));
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
}

/**
 * hyscan_gtk_map_track_set_tracks:
 * @track_layer: указатель на #HyScanGtkMapTrack
 * @tracks: (nullable) (array zero-terminated=1): нуль-терминированный массив с названиями галсов
 *
 * Устанавливает видимые на слое галсы.
 */
void
hyscan_gtk_map_track_set_tracks (HyScanGtkMapTrack  *track_layer,
                                 gchar             **tracks)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer));
  priv = track_layer->priv;

  g_rw_lock_writer_lock (&priv->a_lock);
  g_clear_pointer (&priv->active_tracks, g_strfreev);
  priv->active_tracks = (tracks == NULL) ? g_new0 (gchar *, 1) : g_strdupv (tracks);
  g_rw_lock_writer_unlock (&priv->a_lock);

  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (track_layer));
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
}

/**
 * hyscan_gtk_map_track_get_tracks:
 * @track_layer: указатель на #HyScanGtkMapTrack
 *
 * Возвращает список названий галсов, которые отображаются на слое.
 *
 * Returns: (array zero-terminated=1): нуль-терминированный массив названий видимых галсов,
 *   для удаления g_strfreev()
 */
gchar **
hyscan_gtk_map_track_get_tracks (HyScanGtkMapTrack *track_layer)
{
  HyScanGtkMapTrackPrivate *priv;
  gchar **tracks;

  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer), NULL);
  priv = track_layer->priv;

  g_rw_lock_reader_lock (&priv->a_lock);
  tracks = g_strdupv (priv->active_tracks);
  g_rw_lock_reader_unlock (&priv->a_lock);

  return tracks;
}

/**
 * hyscan_gtk_map_track_lookup:
 * @track_layer: указатель на #HyScanGtkMapTrack
 * @track_name: название галса
 *
 * Находит объект галса #HyScanGtkMapTrackItem по его названию.
 *
 * Returns: (transfer full): указатель на найденный галс #HyScanGtkMapTrackItem.
 *   Для удаления g_object_unref().
 */
HyScanGtkMapTrackItem *
hyscan_gtk_map_track_lookup (HyScanGtkMapTrack *track_layer,
                             const gchar       *track_name)
{
  HyScanGtkMapTrackItem *track;
  g_return_val_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer), NULL);

  track = hyscan_gtk_map_track_get_track (track_layer, track_name, TRUE);
  if (track == NULL)
    return NULL;

  return g_object_ref (track);
}

/**
 * hyscan_gtk_map_track_view:
 * @track_layer: указатель на слой #HyScanGtkMapTrack
 * @track_name: название галса
 * @zoom_in: надо ли вписывать галс в видимую область
 * @cancellable: объект для отмены перемещения
 *
 * Перемешает видимую область карты к галсу с названием @track_name. Если установлен
 * флаг @zoom_in, то функция устанавливает границы видимой области карты так,
 * чтобы галс был виден полностью.
 */
void
hyscan_gtk_map_track_view (HyScanGtkMapTrack *track_layer,
                           const gchar       *track_name,
                           gboolean           zoom_in,
                           HyScanCancellable *cancellable)
{
  HyScanGtkMapTrackPrivate *priv;
  GTask *task;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer));
  g_return_if_fail (track_name != NULL);
  priv = track_layer->priv;
  
  g_return_if_fail (priv->map != NULL);

  /* Запускаем задачу в отдельном потоке. */
  task = g_task_new (track_layer, G_CANCELLABLE (cancellable), hyscan_gtk_map_track_view_cb, GINT_TO_POINTER (zoom_in));
  g_task_set_task_data (task, g_strdup (track_name), g_free);
  g_task_run_in_thread (task, hyscan_gtk_map_track_view_load);
  g_object_unref (task);
}

/**
 * hyscan_gtk_map_track_set_draw_type:
 * @track_layer: указатель на #HyScanGtkMapTrack
 * @type: тип отрисовки
 *
 * Функция устанавливает тип отрисовки галсов слоя.
 */
void
hyscan_gtk_map_track_set_draw_type (HyScanGtkMapTrack         *track_layer,
                                    HyScanGtkMapTrackDrawType  type)
{
  HyScanGtkMapTrackPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer));

  priv = track_layer->priv;
  g_atomic_int_set (&priv->draw_type, type);

  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (track_layer));
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
}

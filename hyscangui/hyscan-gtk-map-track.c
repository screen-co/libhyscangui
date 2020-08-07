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
 * или установить номера используемых каналов, следует получить объект галса #HyScanMapTrack
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
  PROP_MODEL,
};

struct _HyScanGtkMapTrackPrivate
{
  HyScanGtkMap                   *map;              /* Карта. */
  gboolean                        visible;          /* Признак видимости слоя. */
  gboolean                        shutdown;         /* Признак удаления слоя. */

  HyScanMapTrackModel            *model;

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
                                                                   HyScanMapTile               *tile,
                                                                   GCancellable                *cancellable);
static void     hyscan_gtk_map_track_param_set                    (HyScanGtkLayer              *gtk_layer);
static void     hyscan_gtk_map_track_set_tracks                   (HyScanGtkMapTrack           *track_layer);

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

  g_object_class_install_property (object_class, PROP_MODEL,
    g_param_spec_object ("model", "Model", "HyScanMapTrackModel", HYSCAN_TYPE_MAP_TRACK_MODEL,
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
  HyScanGtkMapTrack *gtk_map_track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = gtk_map_track->priv;

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
  priv->draw_bar = hyscan_gtk_map_track_draw_bar_new (priv->model);
  g_signal_connect_swapped (priv->draw_bar, "param-changed", G_CALLBACK (hyscan_gtk_map_track_param_set), track_layer);
  child_param = hyscan_gtk_map_track_draw_get_param (priv->draw_bar);
  hyscan_param_proxy_add (priv->param, "/bar", child_param, "/");
  g_object_unref (child_param);

  /* Класс отрисовки лучами (покрытие). */
  priv->draw_beam = hyscan_gtk_map_track_draw_beam_new (priv->model);
  g_signal_connect_swapped (priv->draw_beam, "param-changed", G_CALLBACK (hyscan_gtk_map_track_param_set), track_layer);
  child_param = hyscan_gtk_map_track_draw_get_param (priv->draw_beam);
  hyscan_param_proxy_add (priv->param, "/beam", child_param, "/");
  g_object_unref (child_param);

  hyscan_param_proxy_bind (priv->param);

  priv->active_tracks = g_new0 (gchar *, 1);
  g_signal_connect_swapped (priv->model, "notify::active-tracks", G_CALLBACK (hyscan_gtk_map_track_set_tracks), track_layer);
}

static void
hyscan_gtk_map_track_object_finalize (GObject *object)
{
  HyScanGtkMapTrack *gtk_map_track = HYSCAN_GTK_MAP_TRACK (object);
  HyScanGtkMapTrackPrivate *priv = gtk_map_track->priv;

  g_rw_lock_clear (&priv->a_lock);
  g_clear_object (&priv->draw_bar);
  g_clear_object (&priv->draw_beam);
  g_clear_object (&priv->param);
  g_clear_pointer (&priv->active_tracks, g_strfreev);

  G_OBJECT_CLASS (hyscan_gtk_map_track_parent_class)->finalize (object);
}

/* Функция рисует на тайле все активные галсы. */
static void
hyscan_gtk_map_track_fill_tile (HyScanGtkMapTiled *tiled_layer,
                                HyScanMapTile     *tile,
                                GCancellable      *cancellable)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (tiled_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackDrawType draw_type;
  HyScanGtkMapTrackDraw *track_draw;
  gchar **active_tracks;
  gint i;

  HyScanGeoCartesian2D from, to;
  gdouble scale;

  cairo_t *cairo;
  cairo_surface_t *surface;
  guint tile_size;

  tile_size = hyscan_map_tile_get_size (tile);
  hyscan_map_tile_get_bounds (tile, &from, &to);
  scale = hyscan_map_tile_get_scale (tile);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, tile_size, tile_size);
  cairo = cairo_create (surface);

  /* Определяем, кто будет рисовать галс. */
  draw_type = g_atomic_int_get (&priv->draw_type);
  if (draw_type == HYSCAN_GTK_MAP_TRACK_BEAM)
    track_draw = priv->draw_beam;
  else
    track_draw = priv->draw_bar;

  active_tracks = hyscan_map_track_model_get_tracks (priv->model);
  for (i = 0; active_tracks[i] != NULL; ++i)
    {
      hyscan_gtk_map_track_draw_region (track_draw, active_tracks[i], cairo, scale, &from, &to, cancellable);

      if (g_cancellable_is_cancelled (cancellable))
        break;
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
  HyScanGeoProjection *projection;

  projection = hyscan_gtk_map_get_projection (map);
  hyscan_map_track_model_set_projection (priv->model, projection);
  g_object_unref (projection);
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

  g_signal_connect_swapped (priv->model, "changed", G_CALLBACK (hyscan_gtk_map_tiled_request_draw), track_layer);
  g_signal_connect_swapped (priv->model, "param-set", G_CALLBACK (hyscan_gtk_map_tiled_request_draw), track_layer);
  g_signal_connect_swapped (priv->model, "param-set", G_CALLBACK (hyscan_gtk_map_tiled_set_param_mod), track_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Обрабатывает удаление слоя с карты. */
static void
hyscan_gtk_map_track_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTrack *track_layer = HYSCAN_GTK_MAP_TRACK (gtk_layer);
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;

  g_return_if_fail (priv->map != NULL);

  hyscan_gtk_layer_parent_interface->removed (gtk_layer);

  g_signal_handlers_disconnect_by_data (priv->model, track_layer);
  g_signal_handlers_disconnect_by_data (priv->map, track_layer);
  g_clear_object (&priv->map);
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

  return g_object_ref (HYSCAN_PARAM (priv->param));
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
  HyScanGtkMapTrackPrivate *priv = track_layer->priv;
  HyScanGeoCartesian2D *view;
  HyScanMapTrackModelInfo *info;
  gchar *track_name;
  gboolean result = FALSE;

  view = g_new0 (HyScanGeoCartesian2D, 2);

  /* Находим запрошенный галс. */
  track_name = task_data;
  info = hyscan_map_track_model_lock (priv->model, track_name);
  if (info == NULL)
    goto exit;

  result = hyscan_map_track_view (info->track, &view[0], &view[1]);
  hyscan_map_track_model_unlock (info);

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
 * @model: модель данных #H
 * @project: название проекта
 * @cache: кэш данных
 *
 * Returns: указатель на новый слой #HyScanGtkMapTrack
 */
HyScanGtkLayer *
hyscan_gtk_map_track_new (HyScanMapTrackModel *model)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK,
                       "model", model,
                       NULL);
}

static void
hyscan_gtk_map_track_area_mod (HyScanGtkMapTrack *track_layer,
                               GList             *sections)
{
  HyScanMapTrackMod *section;
  GList *link;

  for (link = sections; link != NULL; link = link->next)
    {
      section = link->data;
      hyscan_gtk_map_tiled_set_area_mod (HYSCAN_GTK_MAP_TILED (track_layer), &section->from, &section->to);
    }
}

/**
 * Устанавливает слежку за активными галсами.
 */
static void
hyscan_gtk_map_track_set_tracks (HyScanGtkMapTrack  *track_layer)
{
  HyScanGtkMapTrackPrivate *priv;
  gint i;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK (track_layer));
  priv = track_layer->priv;

  /* Отключаемся от сигналов. */
  for (i = 0; priv->active_tracks[i] != NULL; i++)
    {
      HyScanMapTrackModelInfo *info;

      info = hyscan_map_track_model_get (priv->model, priv->active_tracks[i]);
      if (info == NULL)
        continue;

      g_signal_handlers_disconnect_by_data (info->track, track_layer);
      hyscan_map_track_model_info_unref (info);
    }

  /* Обновляем список галсов. */
  g_rw_lock_writer_lock (&priv->a_lock);
  g_clear_pointer (&priv->active_tracks, g_strfreev);
  priv->active_tracks = hyscan_map_track_model_get_tracks (priv->model);
  g_rw_lock_writer_unlock (&priv->a_lock);

  /* Подключаемся к сигналам. */
  for (i = 0; priv->active_tracks[i] != NULL; i++)
    {
      HyScanMapTrackModelInfo *info;

      info = hyscan_map_track_model_get (priv->model, priv->active_tracks[i]);
      if (info == NULL)
        continue;

      g_signal_connect_swapped (info->track, "area-mod", G_CALLBACK (hyscan_gtk_map_track_area_mod), track_layer);
      hyscan_map_track_model_info_unref (info);
    }

  hyscan_gtk_map_tiled_set_param_mod (HYSCAN_GTK_MAP_TILED (track_layer));
  hyscan_gtk_map_tiled_request_draw (HYSCAN_GTK_MAP_TILED (track_layer));
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

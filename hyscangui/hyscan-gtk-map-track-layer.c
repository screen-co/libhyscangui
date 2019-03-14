/* hyscan-gtk-map-track-layer.c
 *
 * Copyright 2019 Screen LLC, Alexey Sakhnov <alexsakhnov@gmail.com>
 *
 * This file is part of HyScanCore library.
 *
 * HyScanCore is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCore is distributed in the hope that it will be useful,
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

/* HyScanCore имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCore на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-gtk-map-track-layer
 * @Short_description: слой карты с треком движения объекта
 * @Title: HyScanGtkMapTrackLayer
 * @See_also: #HyScanGtkLayer
 *
 * Класс позволяет выводить на карте #HyScanGtkMap слой с изображением движения
 * объекта в режиме реального времени.
 *
 * Слой изображет трек движения в виде линии пройденного маршрута, а текущее
 * положение объекта в виде стрелки, направленной по курсе движения.
 *
 * Выводимые данные получаются из модели #HyScanNavigationModel, которая
 * указывается при создании слоя в hyscan_gtk_map_track_layer_new().
 *
 */

#include <hyscan-cache.h>
#include "hyscan-gtk-map-track-layer.h"
#include "hyscan-gtk-map.h"
#include "hyscan-navigation-model.h"

#define ARROW_SIZE (32)      /* Размер маркера, изображающего движущийся объект. */

/* Раскомментируйте строку ниже для вывода отладочной информации о скорости отрисовки слоя. */
#define HYSCAN_GTK_MAP_DEBUG_FPS

enum
{
  PROP_O,
  PROP_NAV_MODEL,
  PROP_CACHE,
};

typedef struct
{
  guint            x;
  guint            y;
  gdouble          scale;
  cairo_surface_t *surface;
} HyScanGtkMapTrackLayerTile;

struct _HyScanGtkMapTrackLayerPrivate
{
  HyScanGtkMap               *map;             /* Виджет карты, на котором размещен слой. */
  HyScanCache                *cache;           /* Кэш отрисованных тайлов трека. */
  HyScanNavigationModel      *nav_model;       /* Модель навигационных данных, которые отображаются. */

  gboolean                   visible;          /* Признак видимости слоя. */

  GList                      *track;           /* Список точек трека. */
  GMutex                      track_lock;      /* Блокировка доступа к точкам трека.
                                                * todo: убрать его, если все в одном потоке */

  cairo_surface_t            *arrow_surface;   /* Поверхность с изображением маркера. */
  gdouble                     arrow_x;         /* Координата x нулевой точки на поверхности маркера. */
  gdouble                     arrow_y;         /* Координата y нулевой точки на поверхности маркера. */
};

static void    hyscan_gtk_map_track_layer_interface_init           (HyScanGtkLayerInterface       *iface);
static void    hyscan_gtk_map_track_layer_set_property             (GObject                       *object,
                                                                    guint                          prop_id,
                                                                    const GValue                  *value,
                                                                    GParamSpec                    *pspec);
static void     hyscan_gtk_map_track_layer_object_constructed      (GObject                       *object);
static void     hyscan_gtk_map_track_layer_object_finalize         (GObject                       *object);
static void     hyscan_gtk_map_track_layer_removed                 (HyScanGtkLayer                *layer);
static void     hyscan_gtk_map_track_layer_draw                    (HyScanGtkMap                  *map,
                                                                    cairo_t                       *cairo,
                                                                    HyScanGtkMapTrackLayer        *track_layer);
static void     hyscan_gtk_map_track_layer_proj_notify             (HyScanGtkMapTrackLayer        *track_layer,
                                                                    GParamSpec                    *pspec);
static void     hyscan_gtk_map_track_layer_added                   (HyScanGtkLayer                *layer,
                                                                    HyScanGtkLayerContainer       *container);
static gboolean hyscan_gtk_map_track_layer_get_visible             (HyScanGtkLayer                *layer);
static void     hyscan_gtk_map_track_layer_set_visible             (HyScanGtkLayer                *layer,
                                                                    gboolean                       visible);
static void     hyscan_gtk_map_track_create_arrow                  (HyScanGtkMapTrackLayerPrivate *priv);
static void     hyscan_gtk_map_track_layer_model_changed           (HyScanGtkMapTrackLayer        *track_layer,
                                                                    gdouble                        time,
                                                                    HyScanGeoGeodetic             *coord);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackLayer, hyscan_gtk_map_track_layer, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanGtkMapTrackLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_track_layer_interface_init))

static void
hyscan_gtk_map_track_layer_class_init (HyScanGtkMapTrackLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_track_layer_set_property;

  object_class->constructed = hyscan_gtk_map_track_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_track_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_NAV_MODEL,
    g_param_spec_object ("nav-model", "Navigation model", "HyScanNavigationModel",
                          HYSCAN_TYPE_NAVIGATION_MODEL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Tile cache", "HyScanCache",
                          HYSCAN_TYPE_CACHE,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_track_layer_init (HyScanGtkMapTrackLayer *gtk_map_track_layer)
{
  gtk_map_track_layer->priv = hyscan_gtk_map_track_layer_get_instance_private (gtk_map_track_layer);
}

static void
hyscan_gtk_map_track_layer_set_property (GObject     *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkMapTrackLayer *gtk_map_track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = gtk_map_track_layer->priv;

  switch (prop_id)
    {
    case PROP_NAV_MODEL:
      priv->nav_model = g_value_dup_object (value);
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
hyscan_gtk_map_track_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->constructed (object);

  g_mutex_init (&priv->track_lock);
  hyscan_gtk_map_track_create_arrow (priv);
  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_track_layer_model_changed), track_layer);
}

static void
hyscan_gtk_map_track_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (object);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->nav_model, track_layer);
  g_clear_object (&priv->nav_model);
  g_clear_object (&priv->cache);
  g_clear_pointer (&priv->arrow_surface, cairo_surface_destroy);

  g_mutex_lock (&priv->track_lock);
  g_list_free_full (priv->track, (GDestroyNotify) hyscan_gtk_map_point_free);
  g_mutex_unlock (&priv->track_lock);

  g_mutex_clear (&priv->track_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_track_layer_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_track_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_track_layer_added;
  iface->removed = hyscan_gtk_map_track_layer_removed;
  iface->set_visible = hyscan_gtk_map_track_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_track_layer_get_visible;
}

/* Обработчик сигнала "changed" модели. */
static void
hyscan_gtk_map_track_layer_model_changed (HyScanGtkMapTrackLayer *track_layer,
                                          gdouble                 time,
                                          HyScanGeoGeodetic      *coord)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapPoint point;

  point.geo.lat = coord->lat;
  point.geo.lon = coord->lon;
  point.geo.h = coord->h;
  if (priv->map != NULL)
    hyscan_gtk_map_geo_to_value (priv->map, point.geo, &point.x, &point.y);

  g_mutex_lock (&priv->track_lock);
  priv->track = g_list_append (priv->track, hyscan_gtk_map_point_copy (&point));
  g_mutex_unlock (&priv->track_lock);

  gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Создаёт cairo-поверхность с изображением объекта. */
static void
hyscan_gtk_map_track_create_arrow (HyScanGtkMapTrackLayerPrivate *priv)
{
  cairo_t *cairo;
  guint line_width = 1;

  g_clear_pointer (&priv->arrow_surface, cairo_surface_destroy);

  priv->arrow_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                    ARROW_SIZE + 2 * line_width, ARROW_SIZE + 2 * line_width);
  cairo = cairo_create (priv->arrow_surface);

  cairo_translate (cairo, line_width, line_width);

  cairo_line_to (cairo, .2 * ARROW_SIZE, ARROW_SIZE);
  cairo_line_to (cairo, ARROW_SIZE / 2.0, 0);
  cairo_line_to (cairo, .8 * ARROW_SIZE, ARROW_SIZE);
  cairo_close_path (cairo);

  cairo_set_source_rgb (cairo, 1.0, 1.0, 1.0);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, line_width);
  cairo_set_source_rgb (cairo, 0, 0, 0);
  cairo_stroke (cairo);

  cairo_destroy (cairo);

  priv->arrow_x = -ARROW_SIZE / 2.0 - (gdouble) line_width;
  priv->arrow_y = -ARROW_SIZE - (gdouble) line_width;
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

/* Реализация HyScanGtkLayerInterface.removed.
 * Отключается от сигналов карты при удалении слоя с карты. */
static void
hyscan_gtk_map_track_layer_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
}

/* Рисует кусок трека от chunk_start до chunk_end. */
static void
hyscan_gtk_map_track_layer_draw_chunk (HyScanGtkMapTrackLayer *track_layer,
                                       cairo_t                *cairo,
                                       GList                  *chunk_start,
                                       GList                  *chunk_end)
{
  GList *chunk_l;
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  cairo_new_path (cairo);
  for (chunk_l = chunk_start; chunk_l != chunk_end; chunk_l = chunk_l->next)
  {
    HyScanGtkMapPoint *point = chunk_l->data;
    gdouble x, y;

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, point->x, point->y);
    cairo_line_to (cairo, x, y);
  }

  cairo_set_line_width (cairo, 2.0);
  cairo_set_source_rgb (cairo, 0, 0, 0);
  cairo_stroke (cairo);
}

/* Рисует трек в указанном регионе. */
static void
hyscan_gtk_map_track_layer_draw_region (HyScanGtkMapTrackLayer *track_layer,
                                        cairo_t                *cairo,
                                        gdouble                 from_x,
                                        gdouble                 to_x,
                                        gdouble                 from_y,
                                        gdouble                 to_y)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GList *track_l;

  GList *chunk_start = NULL;
  GList *chunk_end = NULL;

  /* Стараемся минимально блокировать мутекс и рисуем уже после разблокировки. */
  g_mutex_lock (&priv->track_lock);
  if (priv->track == NULL)
    {
      g_mutex_unlock (&priv->track_lock);
      return;
    }

  /* Ищем куски трека, которые полностью попадают в указанный регион и рисуем покусочно. */
  for (track_l = priv->track; track_l != NULL; track_l = track_l->next)
    {
      HyScanGtkMapPoint *point = track_l->data;
      gboolean is_inside;

      is_inside = from_x <= point->x && point->x <= to_x && from_y <= point->y && point->y <= to_y;
      if (is_inside && chunk_start == NULL)
        {
          chunk_start = track_l->prev == NULL ? track_l->prev : track_l;
        }
      else if (!is_inside && chunk_start != NULL)
        {
          chunk_end = track_l->next != NULL ? track_l->next : track_l;
          hyscan_gtk_map_track_layer_draw_chunk (track_layer, cairo, chunk_start, chunk_end);

          chunk_start = NULL;
          chunk_end = NULL;
        }
    }

  /* Рисуем последний кусок. */
  if (chunk_start != NULL)
    hyscan_gtk_map_track_layer_draw_chunk (track_layer, cairo, chunk_start, chunk_end);
  g_mutex_unlock (&priv->track_lock);
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_track_layer_draw (HyScanGtkMap           *map,
                                 cairo_t                *cairo,
                                 HyScanGtkMapTrackLayer *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  gdouble from_x, to_x, from_y, to_y;

  gdouble x, y;
  gdouble bearing;

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  static GTimer *debug_timer;

  if (debug_timer == NULL)
    debug_timer = g_timer_new ();
  g_timer_start (debug_timer);
#endif

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    return;

  /* Получаем координаты последней точки - маркера текущего местоположения. */
  {
    GList *path_l;
    HyScanGtkMapPoint *last_point;

    g_mutex_lock (&priv->track_lock);

    if (priv->track == NULL)
      {
        g_mutex_unlock (&priv->track_lock);
        return;
      }

    path_l = g_list_last (priv->track);
    last_point = path_l->data;
    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, last_point->x, last_point->y);
    bearing = last_point->geo.h;

    g_mutex_unlock (&priv->track_lock);
  }

  /* Рисуем трек. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_track_layer_draw_region (track_layer, cairo, from_x, to_x, from_y, to_y);

  /* Рисуем маркер движущегося объекта. */
  cairo_save (cairo);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, bearing);
  cairo_set_source_surface (cairo, priv->arrow_surface, priv->arrow_x, priv->arrow_y);
  cairo_paint (cairo);
  cairo_restore (cairo);

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  g_message ("hyscan_gtk_map_track_layer_draw: %.2f fps; length = %d",
             1.0 / g_timer_elapsed (debug_timer, NULL),
             g_list_length (priv->track));
#endif
}

/* Переводит координаты путевых точек трека из географичекских в СК проекции карты. */
static void
hyscan_gtk_map_track_layer_update_points (HyScanGtkMapTrackLayerPrivate *priv)
{
  HyScanGtkMapPoint *point;
  GList *track_l;

  g_mutex_lock (&priv->track_lock);
  for (track_l = priv->track; track_l != NULL; track_l = track_l->next)
    {
      point = track_l->data;
      hyscan_gtk_map_geo_to_value (priv->map, point->geo, &point->x, &point->y);
    }
  g_mutex_unlock (&priv->track_lock);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_track_layer_proj_notify (HyScanGtkMapTrackLayer *track_layer,
                                        GParamSpec             *pspec)
{
  hyscan_gtk_map_track_layer_update_points (track_layer->priv);
}

/* Реализация HyScanGtkLayerInterface.added.
 * Подключается к сигналам карты при добавлении слоя на карту. */
static void
hyscan_gtk_map_track_layer_added (HyScanGtkLayer          *layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTrackLayer *track_layer = HYSCAN_GTK_MAP_TRACK_LAYER (layer);
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  priv->map = g_object_ref (container);
  hyscan_gtk_map_track_layer_update_points (track_layer->priv);

  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_track_layer_draw), track_layer);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_track_layer_proj_notify), track_layer);
}

/**
 * hyscan_gtk_map_track_layer_new:
 * @nav_model: указатель на модель навигационных данных #HyScanNavigationModel
 * @cache: указатель на кэш #HyScanCache
 *
 * Создает новый слой с треком движения объекта.
 *
 * Returns: указатель на #HyScanGtkMapTrackLayer. Для удаления g_object_unref()
 */
HyScanGtkMapTrackLayer *
hyscan_gtk_map_track_layer_new (HyScanNavigationModel *nav_model,
                                HyScanCache           *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LAYER,
                       "nav-model", nav_model,
                       "cache", cache,
                       NULL);
}

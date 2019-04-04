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
#include <hyscan-gtk-map.h>
#include <hyscan-navigation-model.h>
#include <hyscan-gtk-map-tile.h>
#include <string.h>
#include <math.h>
#include "hyscan-gtk-map-track-layer.h"

#define CACHE_HEADER_MAGIC     0xcfadec2d    /* Идентификатор заголовка кэша. */
#define TILE_SIZE              256           /* Размер тайла. */
#define ARROW_SIZE             (32)          /* Размер маркера, изображающего движущийся объект. */
#define LINE_COLOR            "rgba(20,  40,  100, 0.9)"
#define LINE_LOST_COLOR       "rgba(20,  40,  100, 0.2)"
#define ARROW_DEFAULT_STROKE  LINE_COLOR
#define ARROW_DEFAULT_FILL    "rgba(255, 255, 255, 0.9)"
#define ARROW_LOST_STROKE     "rgba(20,  40,  100, 0.2)"
#define ARROW_LOST_FILL       "rgba(255, 255, 255, 0.2)"

/* Раскомментируйте строку ниже для вывода отладочной информации о скорости отрисовки слоя. */
// #define HYSCAN_GTK_MAP_DEBUG_FPS

enum
{
  PROP_O,
  PROP_NAV_MODEL,
  PROP_CACHE,
};

enum
{
  ARROW_DEFAULT,
  ARROW_LOST,
};

typedef struct
{
  gdouble                              x_val;          /* Координата x тайла в логических единицах. */
  gdouble                              y_val;          /* Координата y тайла в логических единицах. */
  gdouble                              scale;          /* Масштаб (логические единицы / пиксель). */
} HyScanGtkMapTrackLayerTileParams;

typedef struct
{
  HyScanGtkMapPoint                    coord;          /* Путевая точка. */
  gdouble                              time;           /* Время фиксации точки. */
  gboolean                             start;          /* Признак того, что точка является началом куска. */
} HyScanGtkMapTrackLayerPoint;

typedef struct
{
  guint32                              magic;          /* Идентификатор заголовка. */

  gsize                                size;           /* Размер данных поверхности. */
  HyScanGtkMapTrackLayerTileParams     params;         /* Параметры тайла. */
} HyScanGtkMapTrackLayerCacheHeader;

typedef struct
{
  /* Параметры для идентификации. */
  gint                                 x;              /* Координата x в СК тайлов. */
  gint                                 y;              /* Координата y в СК тайлов. */
  gint                                 scale_idx;      /* Индекс масштаба карты. */

  /* Кэшируемые данные. */
  HyScanGtkMapTrackLayerTileParams     params;         /* Параметры тайла. */
  cairo_surface_t                     *surface;        /* Поверхность cairo с изображением тайлам. */
} HyScanGtkMapTrackLayerTile;

typedef struct
{
  cairo_surface_t            *surface;   /* Поверхность с изображением маркера. */
  gdouble                     x;         /* Координата x нулевой точки на поверхности маркера. */
  gdouble                     y;         /* Координата y нулевой точки на поверхности маркера. */
} HyScanGtkMapTrackLayerArrow;

struct _HyScanGtkMapTrackLayerPrivate
{
  HyScanGtkMap                 *map;             /* Виджет карты, на котором размещен слой. */
  HyScanCache                  *cache;           /* Кэш отрисованных тайлов трека. */
  HyScanNavigationModel        *nav_model;       /* Модель навигационных данных, которые отображаются. */
  guint64                       life_time;       /* Время жизни точки трека, секунды. */

  gboolean                      visible;          /* Признак видимости слоя. */

  /* Информация о треке. */
  GList                        *track;           /* Список точек трека. В местах обрыва находится NULL. */
  guint                         mod_count;       /* Номер изменений параметров (картографической проекции). */
  HyScanGtkMapTrackLayerPoint  *track_end;       /* Указатель на последнюю точку трека. */
  gboolean                      track_lost;      /* Признак того, что сигнал потерян:
                                                  * g_list_last (priv->track_end) == NULL. */
  GMutex                        track_lock;      /* Блокировка доступа к точкам трека.
                                                  * todo: убрать его, если все в одном потоке */

  /* Кэширование. */
  HyScanBuffer                 *cache_buffer;    /* Буфер для получения заголовка кэша. */
  HyScanBuffer                 *tile_buffer;     /* Буфер для получения поверхности тайла. */
  HyScanBuffer                 *cache_wrapper;   /* Буфер для обёртки заголовка кэша. */
  HyScanBuffer                 *tile_wrapper;    /* Буфер для обёртки поверхности тайла. */
  gchar                        *cache_key;       /* Ключ записи кэша. */
  gulong                        cache_key_len;   /* Длина ключа. */

  /* Внешний вид. */
  HyScanGtkMapTrackLayerArrow   arrow_default;   /* Маркер объекта в обычном режиме. */
  HyScanGtkMapTrackLayerArrow   arrow_lost;      /* Маркер объекта, если сигнал потерян. */

  GdkRGBA                       line_color;      /* Цвет линии трека. */
  GdkRGBA                       line_lost_color; /* Цвет линии при обрыве связи. */
  gdouble                       line_width;      /* Ширина линии трека. */
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
static void     hyscan_gtk_map_track_create_arrow                  (HyScanGtkMapTrackLayerPrivate *priv,
                                                                    HyScanGtkMapTrackLayerArrow   *arrow,
                                                                    guint                          type);
static void     hyscan_gtk_map_track_layer_model_changed           (HyScanGtkMapTrackLayer        *track_layer,
                                                                    gdouble                        time,
                                                                    HyScanGeoGeodetic             *coord);
static void     hyscan_gtk_map_track_layer_update_cache_key        (HyScanGtkMapTrackLayer        *track_layer,
                                                                    HyScanGtkMapTrackLayerTile    *tile);
static void     hyscan_gtk_map_track_layer_point_free              (HyScanGtkMapTrackLayerPoint   *point);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTrackLayer, hyscan_gtk_map_track_layer, G_TYPE_INITIALLY_UNOWNED,
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

  priv->tile_buffer = hyscan_buffer_new ();
  priv->cache_buffer = hyscan_buffer_new ();
  priv->tile_wrapper = hyscan_buffer_new ();
  priv->cache_wrapper = hyscan_buffer_new ();

  g_mutex_init (&priv->track_lock);

  priv->line_width = 2.0;
  gdk_rgba_parse (&priv->line_color, LINE_COLOR);
  gdk_rgba_parse (&priv->line_lost_color, LINE_LOST_COLOR);
  hyscan_gtk_map_track_create_arrow (priv, &priv->arrow_default, ARROW_DEFAULT);
  hyscan_gtk_map_track_create_arrow (priv, &priv->arrow_lost, ARROW_LOST);
  hyscan_gtk_map_track_layer_set_lifetime (track_layer, 600);

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
  g_clear_pointer (&priv->arrow_default.surface, cairo_surface_destroy);
  g_clear_pointer (&priv->arrow_lost.surface, cairo_surface_destroy);
  g_clear_object (&priv->tile_buffer);
  g_clear_object (&priv->cache_buffer);
  g_clear_object (&priv->tile_wrapper);
  g_clear_object (&priv->cache_wrapper);
  g_free (priv->cache_key);

  g_mutex_lock (&priv->track_lock);
  g_list_free_full (priv->track, (GDestroyNotify) hyscan_gtk_map_track_layer_point_free);
  g_clear_pointer (&priv->track_end, hyscan_gtk_map_track_layer_point_free);
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

/* Удаляет из кэша тайлы, содержащие указанный отрезок point0 - point1. */
static void
hyscan_gtk_map_track_layer_invalidate_section (HyScanGtkMapTrackLayer *track_layer,
                                               HyScanGtkMapPoint      *point0,
                                               HyScanGtkMapPoint      *point1)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapTileGrid grid;

  gdouble *scales;
  guint scales_len;
  guint scale_idx;

  /* Для каждого масштаба удаляем из кэша тайлы, на которых лежит этот отрезок. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &grid.min_x, &grid.max_x, &grid.min_y, &grid.max_y);
  scales = hyscan_gtk_map_get_scales (priv->map, &scales_len);
  for (scale_idx = 0; scale_idx < scales_len; scale_idx++)
    {
      HyScanGtkMapTrackLayerTile tile;
      gdouble x_tile, y_tile;

      gint from_x, to_x, from_y, to_y, x, y;

      grid.tiles_num = (grid.max_x - grid.min_x) / scales[scale_idx] / TILE_SIZE;

      /* Определяем на каких тайлах лежат концы отрезка. */
      hyscan_gtk_map_tile_grid_value_to_tile (&grid, point0->x, point0->y, &x_tile, &y_tile);
      from_x = (gint) x_tile;
      from_y = (gint) y_tile;

      hyscan_gtk_map_tile_grid_value_to_tile (&grid, point1->x, point1->y, &x_tile, &y_tile);
      to_x = (gint) x_tile;
      to_y = (gint) y_tile;

      /* Делаем, чтобы было from_{x,y} < to_{x,y}. */
      {
        gint tmp;

        tmp = MAX (from_x, to_x);
        from_x = MIN (from_x, to_x);
        to_x = tmp;

        tmp = MAX (from_y, to_y);
        from_y = MIN (from_y, to_y);
        to_y = tmp;
      }

      /* Проходим все тайлы, на котором лежит отрезок и удаляем их из кэша. */
      tile.scale_idx = scale_idx;
      for (x = from_x; x <= to_x; ++x)
        for (y = from_y; y <= to_y; ++y)
          {
            tile.x = x;
            tile.y = y;

            hyscan_gtk_map_track_layer_update_cache_key (track_layer, &tile);
            hyscan_cache_set (priv->cache, priv->cache_key, NULL, NULL);
          }
    }
  g_free (scales);
}

/* Удаляет "истекшие" путевые точки, т.е. те, у которых время жизни закончилось. */
static void
hyscan_gtk_map_track_layer_delete_expired (HyScanGtkMapTrackLayer *track_layer,
                                           gdouble                 time)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  HyScanGtkMapTrackLayerPoint *cur_point = NULL;
  HyScanGtkMapTrackLayerPoint *prev_point = NULL;
  GList *track_l;

  gdouble delete_time;

  delete_time = time - priv->life_time;
  track_l = priv->track;
  while (track_l != NULL)
    {
      cur_point = track_l->data;
      GList *next = track_l->next;

      if (cur_point->time > delete_time)
        break;

      if (prev_point != NULL)
        {
          hyscan_gtk_map_track_layer_invalidate_section (track_layer, &cur_point->coord, &prev_point->coord);
          hyscan_gtk_map_track_layer_point_free (prev_point);
        }

      prev_point = track_l->data;
      priv->track = g_list_delete_link (priv->track, track_l);

      track_l = next;
    }

  if (prev_point != NULL && track_l != NULL)
    {
      cur_point = track_l->data;
      hyscan_gtk_map_track_layer_invalidate_section (track_layer, &cur_point->coord, &prev_point->coord);
      hyscan_gtk_map_track_layer_point_free (prev_point);
    }
}

/* Удаляет из кэша те тайлы, на которых лежит последний отрезок трека.
 * Выполняется в потоке, откуда пришел сигнал "changed", т.е. в Main Loop. */
static void
hyscan_gtk_map_track_layer_invalidate_cache (HyScanGtkMapTrackLayer *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  GList *list_end;
  HyScanGtkMapTrackLayerPoint *track_point0, *track_point1 = NULL;

  if (priv->cache == NULL || priv->map == NULL)
    return;

  /* Определяем концы последнего отрезка: point0 и point1. */
  list_end = g_list_last (priv->track);
  if (list_end == NULL || list_end->prev == NULL)
    return;

  track_point0 = list_end->data;
  track_point1 = list_end->prev->data;

  /* Удаляем из кэша тайлы, содержащие найденный отрезок. */
  hyscan_gtk_map_track_layer_invalidate_section (track_layer, &track_point0->coord, &track_point1->coord);
}

/* Освобождает память, занятую структурой HyScanGtkMapTrackLayerPoint. */
static void
hyscan_gtk_map_track_layer_point_free (HyScanGtkMapTrackLayerPoint *point)
{
  g_slice_free (HyScanGtkMapTrackLayerPoint, point);
}

/* Копирует структуру HyScanGtkMapTrackLayerPoint. */
static HyScanGtkMapTrackLayerPoint *
hyscan_gtk_map_track_layer_point_copy (HyScanGtkMapTrackLayerPoint *point)
{
  HyScanGtkMapTrackLayerPoint *copy;

  copy = g_slice_new (HyScanGtkMapTrackLayerPoint);
  copy->start = point->start;
  copy->time = point->time;
  copy->coord = point->coord;

  return copy;
}

/* Обработчик сигнала "changed" модели. */
static void
hyscan_gtk_map_track_layer_model_changed (HyScanGtkMapTrackLayer *track_layer,
                                          gdouble                 time,
                                          HyScanGeoGeodetic      *coord)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  g_mutex_lock (&priv->track_lock);

  if (coord != NULL)
    /* Добавляем новую точку в трек. */
    {
      HyScanGtkMapTrackLayerPoint point;

      point.time = time;
      point.start = priv->track_lost;
      point.coord.geo.lat = coord->lat;
      point.coord.geo.lon = coord->lon;
      point.coord.geo.h = coord->h;
      if (priv->map != NULL)
        hyscan_gtk_map_geo_to_value (priv->map, point.coord.geo, &point.coord.x, &point.coord.y);

      priv->track = g_list_append (priv->track, hyscan_gtk_map_track_layer_point_copy (&point));

      g_clear_pointer (&priv->track_end, hyscan_gtk_map_track_layer_point_free);
      priv->track_end = hyscan_gtk_map_track_layer_point_copy (&point);

      hyscan_gtk_map_track_layer_invalidate_cache (track_layer);
    }

  /* Фиксируем обрыв. */
  priv->track_lost = (coord == NULL);

  hyscan_gtk_map_track_layer_delete_expired (track_layer, time);

  g_mutex_unlock (&priv->track_lock);

  if (hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (track_layer)))
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));
}

/* Создаёт cairo-поверхность с изображением объекта. */
static void
hyscan_gtk_map_track_create_arrow (HyScanGtkMapTrackLayerPrivate *priv,
                                   HyScanGtkMapTrackLayerArrow   *arrow,
                                   guint                          type)
{
  cairo_t *cairo;
  guint line_width = 1;
  GdkRGBA color_fill;
  GdkRGBA color_stroke;

  if (type == ARROW_LOST)
    {
      gdk_rgba_parse (&color_fill, ARROW_LOST_FILL);
      gdk_rgba_parse (&color_stroke, ARROW_LOST_STROKE);
    }
  else
    {
      gdk_rgba_parse (&color_fill, ARROW_DEFAULT_FILL);
      gdk_rgba_parse (&color_stroke, ARROW_DEFAULT_STROKE);
    }

  g_clear_pointer (&arrow->surface, cairo_surface_destroy);

  arrow->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                    ARROW_SIZE + 2 * line_width, ARROW_SIZE + 2 * line_width);
  cairo = cairo_create (arrow->surface);

  cairo_translate (cairo, line_width, line_width);

  cairo_line_to (cairo, .2 * ARROW_SIZE, ARROW_SIZE);
  cairo_line_to (cairo, .5 * ARROW_SIZE, 0);
  cairo_line_to (cairo, .8 * ARROW_SIZE, ARROW_SIZE);
  cairo_line_to (cairo, .5 * ARROW_SIZE, .8 * ARROW_SIZE);
  cairo_close_path (cairo);

  gdk_cairo_set_source_rgba (cairo, &color_fill);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, line_width);
  gdk_cairo_set_source_rgba (cairo, &color_stroke);
  cairo_stroke (cairo);

  cairo_destroy (cairo);

  arrow->x = -.5 * ARROW_SIZE - (gdouble) line_width;
  arrow->y = -.8 * ARROW_SIZE - (gdouble) line_width;
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
                                       gdouble                 from_x,
                                       gdouble                 to_x,
                                       gdouble                 from_y,
                                       gdouble                 to_y,
                                       gdouble                 scale,
                                       GList                  *chunk_start,
                                       GList                  *chunk_end)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GList *chunk_l;
  gboolean finish;

  chunk_l = chunk_start;
  finish = (chunk_l == NULL);

  cairo_set_line_width (cairo, priv->line_width);

  cairo_new_path (cairo);
  while (!finish)
    {
      HyScanGtkMapTrackLayerPoint *track_point = chunk_l->data;
      HyScanGtkMapPoint *point = &track_point->coord;
      gdouble x, y;

      x = (point->x - from_x) / scale;
      y = (to_y - from_y) / scale - (point->y - from_y) / scale;

      /* Точка из нового отрезка - завершаем рисование прошлого отрезка. */
      if (track_point->start && cairo_has_current_point (cairo))
        {
          gdouble prev_x, prev_y;

          cairo_get_current_point (cairo, &prev_x, &prev_y);
          gdk_cairo_set_source_rgba (cairo, &priv->line_color);
          cairo_stroke (cairo);

          cairo_move_to (cairo, prev_x, prev_y);
        }

      cairo_line_to (cairo, x, y);

      /* Точка после потери сигнала - рисуем lost-линию. */
      if (track_point->start)
        {
          gdk_cairo_set_source_rgba (cairo, &priv->line_lost_color);
          cairo_stroke (cairo);

          cairo_move_to (cairo, x, y);
        }

      finish = (chunk_l == chunk_end || chunk_l->next == NULL);
      chunk_l = chunk_l->next;
    }

  gdk_cairo_set_source_rgba (cairo, &priv->line_color);
  cairo_stroke (cairo);
}

static inline gboolean
hyscan_gtk_map_track_layer_is_point_inside (HyScanGtkMapPoint *point,
                                            gdouble            from_x,
                                            gdouble            to_x,
                                            gdouble            from_y,
                                            gdouble            to_y)
{
  return point->x > from_x && point->x < to_x && point->y > from_y && point->y < to_y;
}

/* Возвращает TRUE, если boundary находится между val1 и val2. */
static inline gboolean
hyscan_gtk_map_track_layer_is_cross (gdouble val1,
                                     gdouble val2,
                                     gdouble boundary)
{
  return ((val1 < boundary) - (val2 > boundary) == 0);
}

/* Рисует трек в указанном регионе. */
static void
hyscan_gtk_map_track_layer_draw_region (HyScanGtkMapTrackLayer *track_layer,
                                        cairo_t                *cairo,
                                        gdouble                 from_x,
                                        gdouble                 to_x,
                                        gdouble                 from_y,
                                        gdouble                 to_y,
                                        gdouble                 scale)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  GList *track_l;

  GList *chunk_start = NULL;
  GList *chunk_end = NULL;

  GList *point1_l = NULL, *point0_l = NULL;

  /* Стараемся минимально блокировать мутекс и рисуем уже после разблокировки. */
  g_mutex_lock (&priv->track_lock);
  if (priv->track == NULL)
    {
      g_mutex_unlock (&priv->track_lock);
      return;
    }

  /* Ищем куски трека, которые полностью попадают в указанный регион и рисуем покусочно. */
  for (track_l = priv->track; track_l != NULL; track_l = track_l->next, point1_l = point0_l)
    {
      HyScanGtkMapTrackLayerPoint *track_point0, *track_point1;
      HyScanGtkMapPoint *point0, *point1;
      gboolean is_inside;

      point0_l = track_l;

      if (point1_l == NULL)
        continue;

      /* Проверяем, находится ли отрезок (point1, point0) в указанной области. */
      track_point0 = point0_l->data;
      track_point1 = point1_l->data;
      point0 = &track_point0->coord;
      point1 = &track_point1->coord;

      /* Либо один из концов отрезка лежит внутри области... */
      is_inside = hyscan_gtk_map_track_layer_is_point_inside (point0, from_x, to_x, from_y, to_y) ||
                  hyscan_gtk_map_track_layer_is_point_inside (point1, from_x, to_x, from_y, to_y);

      /* ... либо отрезок может пересекать эту область. */
      if (!is_inside)
        {
          gboolean cross_x, cross_y;

          cross_x = hyscan_gtk_map_track_layer_is_cross (point0->x, point1->x, from_x) ||
                    hyscan_gtk_map_track_layer_is_cross (point0->x, point1->x, to_x);
          cross_y = hyscan_gtk_map_track_layer_is_cross (point0->y, point1->y, from_y) ||
                    hyscan_gtk_map_track_layer_is_cross (point0->y, point1->y, to_y);

          is_inside = cross_x && cross_y;
        }

      if (is_inside && chunk_start == NULL)
        /* Фиксируем начало куска. */
        {
          chunk_start = point1_l;
        }

      else if (!is_inside && chunk_start != NULL)
        /* Фиксируем конец куска. */
        {
          chunk_end = point0_l;
          hyscan_gtk_map_track_layer_draw_chunk (track_layer, cairo, from_x, to_x, from_y, to_y, scale, chunk_start, chunk_end);

          chunk_start = NULL;
          chunk_end = NULL;
        }
    }

  /* Рисуем последний кусок. */
  if (chunk_start != NULL)
    hyscan_gtk_map_track_layer_draw_chunk (track_layer, cairo, from_x, to_x, from_y, to_y, scale, chunk_start, chunk_end);
  g_mutex_unlock (&priv->track_lock);
}

/* Заполняет поверхность тайла и информацию о его местоположении. */
static void
hyscan_gtk_map_track_layer_fill_tile (HyScanGtkMapTrackLayer     *track_layer,
                                      HyScanGtkMapTileGrid       *grid,
                                      HyScanGtkMapTrackLayerTile *tile)
{
  gpointer data;
  gssize dsize;
  cairo_t *tile_cairo;

  gdouble x_val, y_val;
  gdouble x_val_to, y_val_to;

  tile_cairo = cairo_create (tile->surface);

  /* Перед перерисовкой очищаем поверхность тайла до прозрачного состояния. */
  data = cairo_image_surface_get_data (tile->surface);
  dsize = cairo_image_surface_get_stride (tile->surface);
  dsize *= cairo_image_surface_get_height (tile->surface);
  cairo_surface_flush (tile->surface);
  memset (data, 0, dsize);
  cairo_surface_mark_dirty (tile->surface);

  /* Заполняем поверхность тайла. */
  hyscan_gtk_map_tile_grid_tile_to_value (grid, tile->x, tile->y, &x_val, &y_val);
  hyscan_gtk_map_tile_grid_tile_to_value (grid, tile->x + 1, tile->y + 1, &x_val_to, &y_val_to);
  hyscan_gtk_map_track_layer_draw_region (track_layer, tile_cairo, x_val, x_val_to, y_val_to, y_val, tile->params.scale);

  /* Запоминаем местоположение тайла в логической СК. */
  tile->params.x_val = x_val;
  tile->params.y_val = y_val;

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  {
    GRand *rand;
    gchar tile_num[100];

    rand = g_rand_new ();
    g_snprintf (tile_num, sizeof (tile_num), "tile %d, %d", tile->x, tile->y);
    cairo_move_to (tile_cairo, TILE_SIZE / 2.0, TILE_SIZE / 2.0);
    cairo_set_source_rgba (tile_cairo,
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           0.1);
    cairo_paint (tile_cairo);
    cairo_set_source_rgb (tile_cairo, 0.2, 0.2, 0);
    cairo_show_text (tile_cairo, tile_num);
  }
#endif

  cairo_destroy (tile_cairo);
}

/* Обновляет ключ кэширования. */
static void
hyscan_gtk_map_track_layer_update_cache_key (HyScanGtkMapTrackLayer     *track_layer,
                                             HyScanGtkMapTrackLayerTile *tile)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  if (priv->cache_key == NULL)
    {
      priv->cache_key = g_strdup_printf ("track_tile.%u.%d.%d.%u", G_MAXUINT, G_MAXINT, G_MAXINT, G_MAXUINT);
      priv->cache_key_len = strlen (priv->cache_key);
    }

  g_snprintf (priv->cache_key, priv->cache_key_len,
              "track_tile.%u.%d.%d.%u", priv->mod_count, tile->x, tile->y, tile->scale_idx);
}

/* Получает из кэша информацию о тайле. */
static gboolean
hyscan_gtk_map_track_layer_cache_get (HyScanGtkMapTrackLayer     *track_layer,
                                      HyScanGtkMapTrackLayerTile *tile)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  HyScanGtkMapTrackLayerCacheHeader header;

  guint32 size;
  gpointer cached_data;
  gpointer tile_data;

  if (priv->cache == NULL)
    return FALSE;

  /* Ищем в кэше. */
  hyscan_gtk_map_track_layer_update_cache_key (track_layer, tile);
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->cache_key, NULL, sizeof (header), priv->cache_buffer, priv->tile_buffer))
    return FALSE;

  /* Верифицируем. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.size != hyscan_buffer_get_size (priv->tile_buffer)))
    {
      return FALSE;
    }

  /* Верифицируем масштаб - он мог измениться при смене проекции карты. */
  if (fabs (tile->params.scale - header.params.scale) / header.params.scale > 1e-6)
    return FALSE;

  /* Копируем. */
  tile_data = cairo_image_surface_get_data (tile->surface);
  cached_data = hyscan_buffer_get_data (priv->tile_buffer, &size);
  memcpy (tile_data, cached_data, size);
  cairo_surface_mark_dirty (tile->surface);

  tile->params = header.params;

  return TRUE;
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_track_layer_cache_set (HyScanGtkMapTrackLayer     *track_layer,
                                      HyScanGtkMapTrackLayerTile *tile)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  HyScanGtkMapTrackLayerCacheHeader header;

  gpointer tile_data;
  guint32 size;

  if (priv->cache == NULL)
    return;

  /* Оборачиваем в буфер пиксельные данные. */
  size = cairo_image_surface_get_stride (tile->surface);
  size *= cairo_image_surface_get_height (tile->surface);
  tile_data = cairo_image_surface_get_data (tile->surface);
  hyscan_buffer_wrap_data (priv->tile_wrapper, HYSCAN_DATA_BLOB, (gpointer) tile_data, size);

  /* Оборачиваем в буфер заголовок с информацией об изображении. */
  header.magic = CACHE_HEADER_MAGIC;
  header.size = size;
  header.params = tile->params;
  hyscan_buffer_wrap_data (priv->cache_wrapper, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Помещаем все данные в кэш. */
  hyscan_gtk_map_track_layer_update_cache_key (track_layer, tile);
  hyscan_cache_set2 (priv->cache, priv->cache_key, NULL, priv->cache_wrapper, priv->tile_wrapper);
}

/* Рисует трек сразу на всю видимую область.
 * Альтернатива рисованию тайлами hyscan_gtk_map_track_layer_draw_tiles().
 * При небольшом треке экономит время, потому что не надо определять тайлы и
 * лезть в кэш. */
static void
hyscan_gtk_map_track_layer_draw_full (HyScanGtkMapTrackLayer *track_layer,
                                      cairo_t                *cairo)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;
  gdouble from_x, to_x, from_y, to_y;
  gdouble scale;

  cairo_t *layer_cairo;
  cairo_surface_t *layer_surface;
  guint width, height;

  /* Создаем поверхность слоя. */
  gtk_cifro_area_get_visible_size (GTK_CIFRO_AREA (priv->map), &width, &height);
  layer_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  layer_cairo = cairo_create (layer_surface);

  /* Рисуем трек на слое. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &from_x, &to_x, &from_y, &to_y);
  hyscan_gtk_map_get_scale_idx (priv->map, &scale);
  hyscan_gtk_map_track_layer_draw_region (track_layer, layer_cairo, from_x, to_x, from_y, to_y, scale);

  /* Переносим слой на поверхность виджета. */
  cairo_set_source_surface (cairo, layer_surface, 0, 0);
  cairo_paint (cairo);

  cairo_destroy (layer_cairo);
  cairo_surface_destroy (layer_surface);
}

/* Рисует трек по тайлам. */
static void
hyscan_gtk_map_track_layer_draw_tiles (HyScanGtkMapTrackLayer *track_layer,
                                       cairo_t                *cairo)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  HyScanGtkMapTileGrid grid;
  HyScanGtkMapRect rect;
  gint scale_idx;
  gdouble scale;

  gint from_tile_x, to_tile_x, from_tile_y, to_tile_y;

  HyScanGtkMapTrackLayerTile tile;
  cairo_t *tile_cairo;

  /* Инициализируем тайловую сетку grid. */
  gtk_cifro_area_get_limits (GTK_CIFRO_AREA (priv->map), &grid.min_x, &grid.max_x, &grid.min_y, &grid.max_y);
  scale_idx = hyscan_gtk_map_get_scale_idx (priv->map, &scale);
  grid.tiles_num = (grid.max_x - grid.min_x) / scale / TILE_SIZE;

  /* Определяем область рисования. */
  gtk_cifro_area_get_view (GTK_CIFRO_AREA (priv->map), &rect.from.x, &rect.to.x, &rect.from.y, &rect.to.y);
  hyscan_gtk_map_tile_grid_bound (&grid, &rect, &from_tile_x, &to_tile_x, &from_tile_y, &to_tile_y);
  // g_message ("Grid bounds: (%d, %d) - (%d, %d)", from_tile_x, to_tile_x, from_tile_y, to_tile_y);

  /* Инициализируем тайл. */
  tile.surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, TILE_SIZE, TILE_SIZE);
  tile.scale_idx = scale_idx;
  tile.params.scale = scale;
  tile_cairo = cairo_create (tile.surface);

  /* Рисуем тайлы по очереди. */
  for (tile.x = from_tile_x; tile.x <= to_tile_x; tile.x++)
    for (tile.y = from_tile_y; tile.y <= to_tile_y; tile.y++)
      {
        gdouble x_source, y_source;

        /* Заполняем. */
        if (!hyscan_gtk_map_track_layer_cache_get (track_layer, &tile))
          {
            hyscan_gtk_map_track_layer_fill_tile (track_layer, &grid, &tile);
            hyscan_gtk_map_track_layer_cache_set (track_layer, &tile);
          }

        /* Рисуем. */
        gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map),
                                               &x_source, &y_source,
                                               tile.params.x_val, tile.params.y_val);
        cairo_set_source_surface (cairo, tile.surface, x_source, y_source);
        cairo_paint (cairo);
      }

  cairo_surface_destroy (tile.surface);
  cairo_destroy (tile_cairo);
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_track_layer_draw (HyScanGtkMap           *map,
                                 cairo_t                *cairo,
                                 HyScanGtkMapTrackLayer *track_layer)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  gdouble x, y;
  gdouble bearing;

  HyScanGtkMapTrackLayerArrow *arrow;

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

  /* Получаем координаты последней точки - маркера текущего местоположения. */
  {
    HyScanGtkMapTrackLayerPoint *last_track_point;
    HyScanGtkMapPoint *last_point;

    g_mutex_lock (&priv->track_lock);

    last_track_point = priv->track_end;
    if (last_track_point == NULL)
      {
        g_mutex_unlock (&priv->track_lock);
        return;
      }

    last_point = &last_track_point->coord;

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, last_point->x, last_point->y);
    bearing = last_point->geo.h;

    g_mutex_unlock (&priv->track_lock);
  }

  /* Рисуем трек тайлами по одной из стратегий. */
  if (priv->cache == NULL)
    hyscan_gtk_map_track_layer_draw_full (track_layer, cairo);
  else
    hyscan_gtk_map_track_layer_draw_tiles (track_layer, cairo);

  /* Рисуем маркер движущегося объекта. */
  cairo_save (cairo);
  cairo_translate (cairo, x, y);
  cairo_rotate (cairo, bearing);
  arrow = priv->track_lost ? &priv->arrow_lost : &priv->arrow_default;
  cairo_set_source_surface (cairo, arrow->surface, arrow->x, arrow->y);
  cairo_paint (cairo);
  cairo_restore (cairo);

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
    g_message ("hyscan_gtk_map_track_layer_draw: %.2f fps; length = %d",
               1.0 / dbg_time,
               g_list_length (priv->track));
  }
#endif
}

/* Переводит координаты путевых точек трека из географичекских в СК проекции карты. */
static void
hyscan_gtk_map_track_layer_update_points (HyScanGtkMapTrackLayerPrivate *priv)
{
  HyScanGtkMapTrackLayerPoint *point;
  GList *track_l;

  g_mutex_lock (&priv->track_lock);

  ++priv->mod_count;

  if (priv->track_end != NULL)
    {
      hyscan_gtk_map_geo_to_value (priv->map, priv->track_end->coord.geo,
                                   &priv->track_end->coord.x, &priv->track_end->coord.y);
    }

  for (track_l = priv->track; track_l != NULL; track_l = track_l->next)
    {
      point = track_l->data;
      hyscan_gtk_map_geo_to_value (priv->map, point->coord.geo, &point->coord.x, &point->coord.y);
    }

  g_mutex_unlock (&priv->track_lock);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_track_layer_proj_notify (HyScanGtkMapTrackLayer *track_layer,
                                        GParamSpec             *pspec)
{
  HyScanGtkMapTrackLayerPrivate *priv = track_layer->priv;

  hyscan_gtk_map_track_layer_update_points (priv);
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
 * Returns: указатель на #HyScanGtkMapTrackLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_track_layer_new (HyScanNavigationModel *nav_model,
                                HyScanCache           *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TRACK_LAYER,
                       "nav-model", nav_model,
                       "cache", cache,
                       NULL);
}

/**
 * hyscan_gtk_map_track_layer_set_lifetime:
 * @track_layer: указатель на #HyScanGtkMapTrackLayer
 * @lifetime: время жизни точек трека, секунды
 */
void
hyscan_gtk_map_track_layer_set_lifetime (HyScanGtkMapTrackLayer *track_layer,
                                         guint64                 lifetime)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TRACK_LAYER (track_layer));

  track_layer->priv->life_time = lifetime;
}

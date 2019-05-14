/* hyscan-gtk-map-way-layer.c
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
 * SECTION: hyscan-gtk-map-way-layer
 * @Short_description: слой карты с треком движения объекта
 * @Title: HyScanGtkMapWayLayer
 * @See_also: #HyScanGtkLayer
 *
 * Класс позволяет выводить на карте #HyScanGtkMap слой с изображением движения
 * объекта в режиме реального времени.
 *
 * Слой изображет трек движения в виде линии пройденного маршрута, а текущее
 * положение объекта в виде стрелки, направленной по курсе движения. Стиль
 * оформления можно задать параметрами конфигурации:
 *
 * - "line-width"
 * - "line-color"
 * - "line-lost-color"
 * - "arrow-default-fill"
 * - "arrow-default-stroke"
 * - "arrow-lost-fill"
 * - "arrow-lost-stroke"
 *
 * Выводимые данные получаются из модели #HyScanNavigationModel, которая
 * указывается при создании слоя в hyscan_gtk_map_way_layer_new().
 *
 */

#include "hyscan-gtk-map-way-layer.h"
#include <hyscan-cartesian.h>
#include <hyscan-gtk-map.h>
#include <hyscan-navigation-model.h>
#include <string.h>
#include <math.h>

#define ARROW_SIZE            32          /* Размер маркера, изображающего движущийся объект. */
#define LIFETIME              600
#define LINE_WIDTH            2.0
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
};

typedef enum
{
  STATE_ACTUAL,                    /* Тайл в актуальном состоянии. */
  STATE_OUTDATED,                  /* Тайл не в актуальном состоянии, но можно показывать. */
  STATE_IRRELEVANT                 /* Тайл не в актуальном состоянии, нельзя показывать. */
} HyScanGtkMapWayLayerModState;

typedef struct
{
  HyScanGtkMapPoint                    coord;          /* Путевая точка. */
  gdouble                              time;           /* Время фиксации точки. */
  gboolean                             start;          /* Признак того, что точка является началом куска. */
} HyScanGtkMapWayLayerPoint;

typedef struct
{
  cairo_surface_t            *surface;   /* Поверхность с изображением маркера. */
  gdouble                     x;         /* Координата x нулевой точки на поверхности маркера. */
  gdouble                     y;         /* Координата y нулевой точки на поверхности маркера. */
} HyScanGtkMapWayLayerArrow;

struct _HyScanGtkMapWayLayerPrivate
{
  HyScanGtkMap                 *map;                        /* Виджет карты, на котором размещен слой. */
  gboolean                      has_cache;                  /* Флаг, используется ли кэш? */
  HyScanNavigationModel        *nav_model;                  /* Модель навигационных данных, которые отображаются. */
  guint64                       life_time;                  /* Время жизни точки трека, секунды. */

  gboolean                      visible;                    /* Признак видимости слоя. */

  /* Информация о треке. */
  GQueue                       *track;                      /* Список точек трека. */
  guint                         track_mod_count;            /* Номер изменения точек трека. */
  gboolean                      track_lost;                 /* Признак того, что сигнал потерян. */
  GMutex                        track_lock;                 /* Блокировка доступа к точкам трека.  */

  /* Внешний вид. */
  HyScanGtkMapWayLayerArrow     arrow_default;              /* Маркер объекта в обычном режиме. */
  HyScanGtkMapWayLayerArrow     arrow_lost;                 /* Маркер объекта, если сигнал потерян. */
  GdkRGBA                       line_color;                 /* Цвет линии трека. */
  GdkRGBA                       line_lost_color;            /* Цвет линии при обрыве связи. */
  gdouble                       line_width;                 /* Ширина линии трека. */
};

static void    hyscan_gtk_map_way_layer_interface_init           (HyScanGtkLayerInterface       *iface);
static void    hyscan_gtk_map_way_layer_set_property             (GObject                       *object,
                                                                    guint                          prop_id,
                                                                    const GValue                  *value,
                                                                    GParamSpec                    *pspec);
static void     hyscan_gtk_map_way_layer_object_constructed      (GObject                       *object);
static void     hyscan_gtk_map_way_layer_object_finalize         (GObject                       *object);
static gboolean hyscan_gtk_map_way_layer_load_key_file           (HyScanGtkLayer                *layer,
                                                                    GKeyFile                      *key_file,
                                                                    const gchar                   *group);
static void     hyscan_gtk_map_way_layer_removed                 (HyScanGtkLayer                *layer);
static void     hyscan_gtk_map_way_layer_draw                    (HyScanGtkMap                  *map,
                                                                    cairo_t                       *cairo,
                                                                    HyScanGtkMapWayLayer        *way_layer);
static void     hyscan_gtk_map_way_layer_proj_notify             (HyScanGtkMapWayLayer        *way_layer,
                                                                    GParamSpec                    *pspec);
static void     hyscan_gtk_map_way_layer_added                   (HyScanGtkLayer                *layer,
                                                                    HyScanGtkLayerContainer       *container);
static gboolean hyscan_gtk_map_way_layer_get_visible             (HyScanGtkLayer                *layer);
static void     hyscan_gtk_map_way_layer_set_visible             (HyScanGtkLayer                *layer,
                                                                    gboolean                       visible);
static void     hyscan_gtk_map_track_create_arrow                  (HyScanGtkMapWayLayerArrow   *arrow,
                                                                    GdkRGBA                       *color_fill,
                                                                    GdkRGBA                       *color_stroke);
static void     hyscan_gtk_map_way_layer_model_changed           (HyScanGtkMapWayLayer        *way_layer,
                                                                    gdouble                        time,
                                                                    HyScanGeoGeodetic             *coord);
static void     hyscan_gtk_map_way_layer_point_free              (HyScanGtkMapWayLayerPoint   *point);
static void     hyscan_gtk_map_way_layer_fill_tile               (HyScanGtkMapTiledLayer        *tiled_layer,
                                                                  HyScanGtkMapTile              *tile);

static HyScanGtkLayerInterface *hyscan_gtk_layer_parent_interface = NULL;

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapWayLayer, hyscan_gtk_map_way_layer, HYSCAN_TYPE_GTK_MAP_TILED_LAYER,
                         G_ADD_PRIVATE (HyScanGtkMapWayLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_way_layer_interface_init))

static void
hyscan_gtk_map_way_layer_class_init (HyScanGtkMapWayLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  HyScanGtkMapTiledLayerClass *tiled_class = HYSCAN_GTK_MAP_TILED_LAYER_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_way_layer_set_property;
  object_class->constructed = hyscan_gtk_map_way_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_way_layer_object_finalize;

  tiled_class->fill_tile = hyscan_gtk_map_way_layer_fill_tile;

  g_object_class_install_property (object_class, PROP_NAV_MODEL,
    g_param_spec_object ("nav-model", "Navigation model", "HyScanNavigationModel",
                          HYSCAN_TYPE_NAVIGATION_MODEL,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_way_layer_init (HyScanGtkMapWayLayer *gtk_map_way_layer)
{
  gtk_map_way_layer->priv = hyscan_gtk_map_way_layer_get_instance_private (gtk_map_way_layer);
}

static void
hyscan_gtk_map_way_layer_set_property (GObject     *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  HyScanGtkMapWayLayer *gtk_map_way_layer = HYSCAN_GTK_MAP_WAY_LAYER (object);
  HyScanGtkMapWayLayerPrivate *priv = gtk_map_way_layer->priv;

  switch (prop_id)
    {
    case PROP_NAV_MODEL:
      priv->nav_model = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_way_layer_object_constructed (GObject *object)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (object);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_way_layer_parent_class)->constructed (object);

  priv->has_cache = hyscan_gtk_map_tiled_layer_has_cache (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));

  priv->track = g_queue_new ();

  g_mutex_init (&priv->track_lock);

  /* Настройки внешнего вида по умолчанию. */
  {
    GdkRGBA color_fill, color_stroke;

    hyscan_gtk_map_way_layer_set_lifetime (way_layer, LIFETIME);
    priv->line_width = LINE_WIDTH;

    gdk_rgba_parse (&priv->line_color, LINE_COLOR);
    gdk_rgba_parse (&priv->line_lost_color, LINE_LOST_COLOR);

    gdk_rgba_parse (&color_fill, ARROW_DEFAULT_FILL);
    gdk_rgba_parse (&color_stroke, ARROW_DEFAULT_STROKE);
    hyscan_gtk_map_track_create_arrow (&priv->arrow_default, &color_fill, &color_stroke);

    gdk_rgba_parse (&color_fill, ARROW_LOST_FILL);
    gdk_rgba_parse (&color_stroke, ARROW_LOST_STROKE);
    hyscan_gtk_map_track_create_arrow (&priv->arrow_lost, &color_fill, &color_stroke);
  }

  g_signal_connect_swapped (priv->nav_model, "changed", G_CALLBACK (hyscan_gtk_map_way_layer_model_changed), way_layer);
}

static void
hyscan_gtk_map_way_layer_object_finalize (GObject *object)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (object);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  g_signal_handlers_disconnect_by_data (priv->nav_model, way_layer);
  g_clear_object (&priv->nav_model);
  g_clear_pointer (&priv->arrow_default.surface, cairo_surface_destroy);
  g_clear_pointer (&priv->arrow_lost.surface, cairo_surface_destroy);

  g_mutex_lock (&priv->track_lock);
  g_queue_free_full (priv->track, (GDestroyNotify) hyscan_gtk_map_way_layer_point_free);
  g_mutex_unlock (&priv->track_lock);

  g_mutex_clear (&priv->track_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_way_layer_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_way_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  hyscan_gtk_layer_parent_interface = g_type_interface_peek_parent (iface);

  iface->added = hyscan_gtk_map_way_layer_added;
  iface->removed = hyscan_gtk_map_way_layer_removed;
  iface->set_visible = hyscan_gtk_map_way_layer_set_visible;
  iface->get_visible = hyscan_gtk_map_way_layer_get_visible;
  iface->load_key_file = hyscan_gtk_map_way_layer_load_key_file;
}

/* Загружает настройки слоя из ini-файла. */
static gboolean
hyscan_gtk_map_way_layer_load_key_file (HyScanGtkLayer *layer,
                                          GKeyFile       *key_file,
                                          const gchar    *group)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (layer);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gdouble line_width;
  GdkRGBA color_fill, color_stroke;

  /* Блокируем доступ к треку, пока не установим новые параметры. */
  g_mutex_lock (&priv->track_lock);

  /* Внешний вид линии. */
  line_width = g_key_file_get_double (key_file, group, "line-width", NULL);
  priv->line_width = line_width > 0 ? line_width : LINE_WIDTH ;
  hyscan_gtk_layer_load_key_file_rgba (&priv->line_color, key_file, group, "line-color", LINE_COLOR);
  hyscan_gtk_layer_load_key_file_rgba (&priv->line_lost_color, key_file, group, "line-lost-color", LINE_LOST_COLOR);

  /* Маркер в обычном состоянии. */
  hyscan_gtk_layer_load_key_file_rgba (&color_fill, key_file, group, "arrow-default-fill", ARROW_DEFAULT_FILL);
  hyscan_gtk_layer_load_key_file_rgba (&color_stroke, key_file, group, "arrow-default-stroke", ARROW_DEFAULT_STROKE);
  hyscan_gtk_map_track_create_arrow (&priv->arrow_default, &color_fill, &color_stroke);

  /* Маркер при потери сигнала. */
  hyscan_gtk_layer_load_key_file_rgba (&color_fill, key_file, group, "arrow-lost-fill", ARROW_LOST_FILL);
  hyscan_gtk_layer_load_key_file_rgba (&color_stroke, key_file, group, "arrow-lost-stroke", ARROW_LOST_STROKE);
  hyscan_gtk_map_track_create_arrow (&priv->arrow_lost, &color_fill, &color_stroke);

  g_mutex_unlock (&priv->track_lock);

  /* Фиксируем изменение параметров. */
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));
  /* Ставим флаг о необходимости перерисовки. */
  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));

  return TRUE;
}


/* Обрабатывает "истекшие" путевые точки, т.е. те, у которых время жизни закончилось:
 * - удаляет эти точки из трека,
 * - обновляет тайлы с удаленными точками.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_way_layer_set_expired_mod (HyScanGtkMapWayLayer *way_layer,
                                          gdouble               time)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  GList *track_l;
  GList *new_tail;

  gdouble delete_time;

  if (g_queue_is_empty (priv->track))
    return;

  /* 1. Находим new_tail - последнюю актуальную точку трека. */
  delete_time = time - priv->life_time;
  for (new_tail = priv->track->tail; new_tail != NULL; new_tail = new_tail->prev)
    {
      HyScanGtkMapWayLayerPoint *cur_point = new_tail->data;

      if (cur_point->time > delete_time)
        break;
    }

  /* 2. Делаем недействительными всё тайлы, которые содержат устаревшие точки. */
  for (track_l = priv->track->tail; track_l != new_tail; track_l = track_l->prev)
    {
      HyScanGtkMapWayLayerPoint *prev_point;
      HyScanGtkMapWayLayerPoint *cur_point;

      if (track_l->prev == NULL)
        continue;

      prev_point = track_l->prev->data;
      cur_point  = track_l->data;
      hyscan_gtk_map_tiled_layer_set_area_mod (HYSCAN_GTK_MAP_TILED_LAYER (way_layer),
                                               &cur_point->coord.c2d, &prev_point->coord.c2d);
    }

  /* 3. Удаляем устаревшие точки. */
  for (track_l = priv->track->tail; track_l != new_tail; )
    {
      GList *prev_l = track_l->prev;
      HyScanGtkMapWayLayerPoint *cur_point = track_l->data;

      g_queue_delete_link (priv->track, track_l);
      hyscan_gtk_map_way_layer_point_free (cur_point);

      track_l = prev_l;
    }
}

/* Присваивает тайлам с новыми точками актуальный номер изменения mod_count.
 * Выполняется в потоке, откуда пришел сигнал "changed", т.е. в Main Loop.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_way_layer_set_head_mod (HyScanGtkMapWayLayer *way_layer)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  GList *head_l;
  HyScanGtkMapWayLayerPoint *track_point0, *track_point1 = NULL;

  if (!priv->has_cache || priv->map == NULL)
    return;

  /* Определяем концы последнего отрезка: point0 и point1. */
  head_l = priv->track->head;
  if (head_l == NULL || head_l->next == NULL)
    return;

  track_point0 = head_l->data;
  track_point1 = head_l->next->data;

  /* Актуализируем тайлы, содержащие найденный отрезок. */
  hyscan_gtk_map_tiled_layer_set_area_mod (HYSCAN_GTK_MAP_TILED_LAYER (way_layer),
                                           &track_point0->coord.c2d, &track_point1->coord.c2d);
}

/* Освобождает память, занятую структурой HyScanGtkMapWayLayerPoint. */
static void
hyscan_gtk_map_way_layer_point_free (HyScanGtkMapWayLayerPoint *point)
{
  g_slice_free (HyScanGtkMapWayLayerPoint, point);
}

/* Копирует структуру HyScanGtkMapWayLayerPoint. */
static HyScanGtkMapWayLayerPoint *
hyscan_gtk_map_way_layer_point_copy (HyScanGtkMapWayLayerPoint *point)
{
  HyScanGtkMapWayLayerPoint *copy;

  copy = g_slice_new (HyScanGtkMapWayLayerPoint);
  copy->start = point->start;
  copy->time = point->time;
  copy->coord = point->coord;

  return copy;
}

/* Обработчик сигнала "changed" модели. */
static void
hyscan_gtk_map_way_layer_model_changed (HyScanGtkMapWayLayer *way_layer,
                                          gdouble                 time,
                                          HyScanGeoGeodetic      *coord)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  g_mutex_lock (&priv->track_lock);

  /* Фиксируем изменение состояния трека. */
  priv->track_mod_count++;

  /* Добавляем новую точку в трек. */
  if (coord != NULL)
    {
      HyScanGtkMapWayLayerPoint point;

      point.time = time;
      point.start = priv->track_lost;
      point.coord.geo.lat = coord->lat;
      point.coord.geo.lon = coord->lon;
      point.coord.geo.h = coord->h;
      if (priv->map != NULL)
        hyscan_gtk_map_geo_to_value (priv->map, point.coord.geo, &point.coord.c2d);

      g_queue_push_head (priv->track, hyscan_gtk_map_way_layer_point_copy (&point));

      hyscan_gtk_map_way_layer_set_head_mod (way_layer);
    }

  /* Фиксируем обрыв. */
  priv->track_lost = (coord == NULL);

  /* Удаляем устаревшие по life_time точки. */
  hyscan_gtk_map_way_layer_set_expired_mod (way_layer, time);

  g_mutex_unlock (&priv->track_lock);

  if (hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (way_layer)))
    hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));
}

/* Создаёт cairo-поверхность с изображением стрелки в голове трека. */
static void
hyscan_gtk_map_track_create_arrow (HyScanGtkMapWayLayerArrow *arrow,
                                   GdkRGBA                   *color_fill,
                                   GdkRGBA                   *color_stroke)
{
  cairo_t *cairo;
  guint line_width = 1;

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

  gdk_cairo_set_source_rgba (cairo, color_fill);
  cairo_fill_preserve (cairo);

  cairo_set_line_width (cairo, line_width);
  gdk_cairo_set_source_rgba (cairo, color_stroke);
  cairo_stroke (cairo);

  cairo_destroy (cairo);

  /* Координаты начала отсчета на поверхности. */
  arrow->x = -.5 * ARROW_SIZE - (gdouble) line_width;
  arrow->y = -.8 * ARROW_SIZE - (gdouble) line_width;
}

/* Реализация HyScanGtkLayerInterface.set_visible.
 * Устанавливает видимость слоя. */
static void
hyscan_gtk_map_way_layer_set_visible (HyScanGtkLayer *layer,
                                      gboolean        visible)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (layer);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  priv->visible = visible;

  hyscan_gtk_map_tiled_layer_request_draw (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));
}

/* Реализация HyScanGtkLayerInterface.get_visible.
 * Возвращает видимость слоя. */
static gboolean
hyscan_gtk_map_way_layer_get_visible (HyScanGtkLayer *layer)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (layer);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  return priv->visible;
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Отключается от сигналов карты при удалении слоя с карты. */
static void
hyscan_gtk_map_way_layer_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (layer);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  hyscan_gtk_layer_parent_interface->removed (layer);

  /* Отключаемся от карты. */
  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
}

/* Рисует кусок трека от chunk_start до chunk_end. */
static void
hyscan_gtk_map_way_layer_draw_chunk (HyScanGtkMapWayLayer *way_layer,
                                       cairo_t                *cairo,
                                       gdouble                 from_x,
                                       gdouble                 to_x,
                                       gdouble                 from_y,
                                       gdouble                 to_y,
                                       gdouble                 scale,
                                       GList                  *chunk_start,
                                       GList                  *chunk_end)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  GList *chunk_l;
  gboolean finish;

  cairo_set_line_width (cairo, priv->line_width);

  /* Рисуем линию по всем точкам от chunk_start до chunk_end.
   * В момент потери сигнала рисуем линию другим цветом (line_lost_color). */
  cairo_new_path (cairo);
  chunk_l = chunk_start;
  finish = (chunk_l == NULL);
  while (!finish)
    {
      HyScanGtkMapWayLayerPoint *track_point = chunk_l->data;
      HyScanGeoCartesian2D *point = &track_point->coord.c2d;
      gdouble x, y;

      /* Координаты точки на поверхности. */
      x = (point->x - from_x) / scale;
      y = (from_y - to_y) / scale - (point->y - to_y) / scale;

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

/* Рисует трек в указанном регионе. Возвращает mod_count нарисованного трека.  */
static guint
hyscan_gtk_map_way_layer_draw_region (HyScanGtkMapWayLayer *way_layer,
                                        cairo_t                *cairo,
                                        gdouble                 from_x,
                                        gdouble                 to_x,
                                        gdouble                 from_y,
                                        gdouble                 to_y,
                                        gdouble                 scale)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;
  GList *track_l;

  GList *chunk_start = NULL;
  GList *chunk_end = NULL;

  guint mod_count;

  GList *point1_l = NULL, *point0_l = NULL;
  HyScanGeoCartesian2D area_from = {.x = from_x, .y = from_y};
  HyScanGeoCartesian2D area_to   = {.x = to_x, .y = to_y};

  /* Блокируем доступ к треку на время прорисовки. */
  g_mutex_lock (&priv->track_lock);

  mod_count = priv->track_mod_count;
  if (g_queue_is_empty (priv->track))
    {
      g_mutex_unlock (&priv->track_lock);
      return mod_count;
    }

  /* Ищем куски трека, которые полностью попадают в указанный регион и рисуем покусочно. */
  for (track_l = priv->track->head; track_l != NULL; track_l = track_l->next, point1_l = point0_l)
    {
      HyScanGtkMapWayLayerPoint *track_point0, *track_point1;
      HyScanGeoCartesian2D *point0, *point1;
      gboolean is_inside;

      point0_l = track_l;

      if (point1_l == NULL)
        continue;

      /* Проверяем, находится ли отрезок (point1, point0) в указанной области. */
      track_point0 = point0_l->data;
      track_point1 = point1_l->data;
      point0 = &track_point0->coord.c2d;
      point1 = &track_point1->coord.c2d;

      is_inside = hyscan_cartesian_is_inside (point0, point1, &area_from, &area_to);

      if (is_inside && chunk_start == NULL)
        /* Фиксируем начало куска. */
        {
          chunk_start = point1_l;
        }

      else if (!is_inside && chunk_start != NULL)
        /* Фиксируем конец куска. */
        {
          chunk_end = point0_l;
          hyscan_gtk_map_way_layer_draw_chunk (way_layer, cairo,
                                               from_x, to_x, from_y, to_y, scale,
                                               chunk_start, chunk_end);

          chunk_start = NULL;
          chunk_end = NULL;
        }
    }

  /* Рисуем последний кусок. */
  if (chunk_start != NULL)
    {
      hyscan_gtk_map_way_layer_draw_chunk (way_layer, cairo,
                                           from_x, to_x, from_y, to_y, scale,
                                           chunk_start, chunk_end);
    }

  g_mutex_unlock (&priv->track_lock);

  return mod_count;
}

/* Заполняет поверхность тайла. Возвращает номер состояния трека на момент рисования */
static void
hyscan_gtk_map_way_layer_fill_tile (HyScanGtkMapTiledLayer *tiled_layer,
                                    HyScanGtkMapTile       *tile)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (tiled_layer);
  cairo_t *tile_cairo;

  HyScanGeoCartesian2D from, to;
  cairo_surface_t *surface;
  gint width;
  gdouble scale;

  width = hyscan_gtk_map_tile_get_size (tile);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, width);
  tile_cairo = cairo_create (surface);

  /* Заполняем поверхность тайла. */
  hyscan_gtk_map_tile_get_bounds (tile, &from, &to);
  scale = hyscan_gtk_map_tile_get_scale (tile);
  hyscan_gtk_map_way_layer_draw_region (way_layer, tile_cairo, from.x, to.x, from.y, to.y, scale);

  /* Записываем поверхность в тайл. */
  hyscan_gtk_map_tile_set_surface (tile, surface);

  cairo_surface_destroy (surface);
  cairo_destroy (tile_cairo);
}

/* Рисует трек сразу на всю видимую область.
 * Альтернатива рисованию тайлами hyscan_gtk_map_way_layer_draw_tiles().
 * При небольшом треке экономит время, потому что не надо определять тайлы и
 * лезть в кэш. */
static void
hyscan_gtk_map_way_layer_draw_full (HyScanGtkMapWayLayer *way_layer,
                                      cairo_t                *cairo)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;
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
  hyscan_gtk_map_way_layer_draw_region (way_layer, layer_cairo, from_x, to_x, from_y, to_y, scale);

  /* Переносим слой на поверхность виджета. */
  cairo_set_source_surface (cairo, layer_surface, 0, 0);
  cairo_paint (cairo);

  cairo_destroy (layer_cairo);
  cairo_surface_destroy (layer_surface);
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_way_layer_draw (HyScanGtkMap         *map,
                               cairo_t              *cairo,
                               HyScanGtkMapWayLayer *way_layer)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gdouble x, y;
  gdouble bearing;

  HyScanGtkMapWayLayerArrow *arrow;

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  static GTimer *debug_timer;
  static gdouble frame_time[25];
  static guint64 frame_idx = 0;

  if (debug_timer == NULL)
    debug_timer = g_timer_new ();
  g_timer_start (debug_timer);
  frame_idx++;
#endif

  if (!hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (way_layer)))
    return;

  /* Получаем координаты последней точки - маркера текущего местоположения. */
  {
    HyScanGtkMapWayLayerPoint *last_track_point;
    HyScanGtkMapPoint *last_point;

    g_mutex_lock (&priv->track_lock);

    if (priv->track->head == NULL)
      {
        g_mutex_unlock (&priv->track_lock);
        return;
      }

    last_track_point = priv->track->head->data;
    last_point = &last_track_point->coord;

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, last_point->c2d.x, last_point->c2d.y);
    bearing = last_point->geo.h;

    g_mutex_unlock (&priv->track_lock);
  }

  /* Рисуем трек тайлами по одной из стратегий: _draw_full() или _draw_tiles(). */
  if (priv->has_cache)
    hyscan_gtk_map_tiled_layer_draw (HYSCAN_GTK_MAP_TILED_LAYER (way_layer), cairo);
  else
    hyscan_gtk_map_way_layer_draw_full (way_layer, cairo);

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
    g_message ("hyscan_gtk_map_way_layer_draw: %.2f fps; length = %d",
               1.0 / dbg_time,
               g_queue_get_length (priv->track));
  }
#endif
}

/* Переводит координаты путевых точек трека из географичекских в СК проекции карты. */
static void
hyscan_gtk_map_way_layer_update_points (HyScanGtkMapWayLayerPrivate *priv)
{
  HyScanGtkMapWayLayerPoint *point;
  GList *track_l;

  g_mutex_lock (&priv->track_lock);

  for (track_l = priv->track->head; track_l != NULL; track_l = track_l->next)
    {
      point = track_l->data;
      hyscan_gtk_map_geo_to_value (priv->map, point->coord.geo, &point->coord.c2d);
    }

  g_mutex_unlock (&priv->track_lock);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_way_layer_proj_notify (HyScanGtkMapWayLayer *way_layer,
                                      GParamSpec             *pspec)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  /* Обновляем координаты точек согласно новой проекции. */
  hyscan_gtk_map_way_layer_update_points (priv);

  /* Фиксируем изменение параметров. */
  hyscan_gtk_map_tiled_layer_set_param_mod (HYSCAN_GTK_MAP_TILED_LAYER (way_layer));
}

/* Реализация HyScanGtkLayerInterface.added.
 * Подключается к сигналам карты при добавлении слоя на карту. */
static void
hyscan_gtk_map_way_layer_added (HyScanGtkLayer          *layer,
                                HyScanGtkLayerContainer *container)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (layer);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  /* Подключаемся к карте. */
  priv->map = g_object_ref (container);
  hyscan_gtk_map_way_layer_update_points (way_layer->priv);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_way_layer_draw), way_layer);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_way_layer_proj_notify), way_layer);

  hyscan_gtk_layer_parent_interface->added (layer, container);
}

/**
 * hyscan_gtk_map_way_layer_new:
 * @nav_model: указатель на модель навигационных данных #HyScanNavigationModel
 * @cache: указатель на кэш #HyScanCache
 *
 * Создает новый слой с треком движения объекта.
 *
 * Returns: указатель на #HyScanGtkMapWayLayer
 */
HyScanGtkLayer *
hyscan_gtk_map_way_layer_new (HyScanNavigationModel *nav_model,
                                HyScanCache           *cache)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_WAY_LAYER,
                       "nav-model", nav_model,
                       "cache", cache,
                       NULL);
}

/**
 * hyscan_gtk_map_way_layer_set_lifetime:
 * @way_layer: указатель на #HyScanGtkMapWayLayer
 * @lifetime: время жизни точек трека, секунды
 */
void
hyscan_gtk_map_way_layer_set_lifetime (HyScanGtkMapWayLayer *way_layer,
                                         guint64                 lifetime)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_WAY_LAYER (way_layer));

  way_layer->priv->life_time = lifetime;
}

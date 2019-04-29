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
#include <hyscan-gtk-map.h>
#include <hyscan-navigation-model.h>
#include <hyscan-gtk-map-tile.h>
#include <hyscan-task-queue.h>
#include <hyscan-cache.h>
#include <string.h>
#include <math.h>

#define CACHE_TRACK_MOD_MAGIC  0x4d546d64    /* Идентификатор заголовка кэша. */
#define CACHE_HEADER_MAGIC     0xcfadec2d    /* Идентификатор заголовка кэша. */
#define CACHE_KEY_LEN          127           /* Длина ключа кэша. */
#define TILE_SIZE              256           /* Размер тайла. */
#define ARROW_SIZE             (32)          /* Размер маркера, изображающего движущийся объект. */
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
  PROP_CACHE,
};

typedef enum
{
  STATE_ACTUAL,                    /* Тайл в актуальном состоянии. */
  STATE_OUTDATED,                  /* Тайл не в актуальном состоянии, но можно показывать. */
  STATE_IRRELEVANT                 /* Тайл не в актуальном состоянии, нельзя показывать. */
} HyScanGtkMapWayLayerModState;

/* Структура с информацией о последнем изменении трека внутри некоторого тайла. */
typedef struct
{
  guint32                              magic;          /* Идентификатор заголовка. */
  guint64                              mod;            /* Номер изменения трека. */
} HyScanGtkMapWayLayerTileMod;

typedef struct
{
  guint32                              magic;          /* Идентификатор заголовка. */
  guint                                mod;            /* Номер изменения трека в момент отрисовки тайла. */
  gsize                                size;           /* Размер данных поверхности. */
} HyScanGtkMapWayLayerCacheHeader;

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
  HyScanCache                  *cache;                      /* Кэш отрисованных тайлов трека. */
  HyScanNavigationModel        *nav_model;                  /* Модель навигационных данных, которые отображаются. */
  HyScanGtkMapTileGrid         *tile_grid;                  /* Тайловая сетка. */
  guint64                       life_time;                  /* Время жизни точки трека, секунды. */

  HyScanTaskQueue              *task_queue;                 /* Очередь по загрузке тайлов. */

  gboolean                      visible;                    /* Признак видимости слоя. */
  gboolean                      redraw;                     /* Признак необходимости перерисовки. */
  guint                         redraw_tag;                 /* Тэг функции, которая запрашивает перерисовку. */

  /* Информация о треке. */
  GQueue                       *track;                      /* Список точек трека. */
  guint                         track_mod_count;            /* Номер изменения точек трека. */
  guint                         param_mod_count;            /* Номер изменений параметров трека (цвета, проекция). */
  gboolean                      track_lost;                 /* Признак того, что сигнал потерян. */
  GMutex                        track_lock;                 /* Блокировка доступа к точкам трека.  */

  /* Кэширование - переменные для чтения кэша в _cache_get(). */
  HyScanBuffer                 *cache_buffer;               /* Буфер для получения заголовка кэша. */
  HyScanBuffer                 *tile_buffer;                /* Буфер для получения поверхности тайла. */
  gchar                         cache_key[CACHE_KEY_LEN];   /* Ключ кэша. */

  /* Внешний вид. */
  HyScanGtkMapWayLayerArrow   arrow_default;              /* Маркер объекта в обычном режиме. */
  HyScanGtkMapWayLayerArrow   arrow_lost;                 /* Маркер объекта, если сигнал потерян. */
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
static guint    hyscan_gtk_map_way_layer_fill_tile               (HyScanGtkMapWayLayer        *way_layer,
                                                                    HyScanGtkMapTile              *tile);
static void     hyscan_gtk_map_way_layer_cache_set               (HyScanGtkMapWayLayer        *way_layer,
                                                                    HyScanGtkMapTile              *tile,
                                                                    guint                          mod_count);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapWayLayer, hyscan_gtk_map_way_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapWayLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_way_layer_interface_init))

static void
hyscan_gtk_map_way_layer_class_init (HyScanGtkMapWayLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_way_layer_set_property;

  object_class->constructed = hyscan_gtk_map_way_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_way_layer_object_finalize;

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

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* Запрашивает перерисовку виджета карты, если пришёл хотя бы один тайл из видимой области. */
static gboolean
hyscan_gtk_map_way_layer_queue_draw (HyScanGtkMapWayLayer *layer)
{
  HyScanGtkMapWayLayerPrivate *priv = layer->priv;

  if (!g_atomic_int_compare_and_exchange (&priv->redraw, TRUE, FALSE))
    return G_SOURCE_CONTINUE;

  gtk_widget_queue_draw (GTK_WIDGET (layer->priv->map));

  return G_SOURCE_CONTINUE;
}

/* Обработчик задачи из очереди HyScanTaskQueue.
 * Заполняет переданный тайл. */
static void
hyscan_gtk_map_way_layer_process (GObject      *task,
                                    gpointer      user_data,
                                    GCancellable *cancellable)
{
  HyScanGtkMapTile *tile = HYSCAN_GTK_MAP_TILE (task);
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (user_data);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  guint mod_count;

  /* Заполняет тайл и помещаем его в кэш. */
  mod_count = hyscan_gtk_map_way_layer_fill_tile (way_layer, tile);
  hyscan_gtk_map_way_layer_cache_set (way_layer, tile, mod_count);

  /* Просим перерисовать. */
  g_atomic_int_set (&priv->redraw, TRUE);
}

static void
hyscan_gtk_map_way_layer_object_constructed (GObject *object)
{
  HyScanGtkMapWayLayer *way_layer = HYSCAN_GTK_MAP_WAY_LAYER (object);
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_way_layer_parent_class)->constructed (object);

  priv->track = g_queue_new ();
  priv->tile_buffer = hyscan_buffer_new ();
  priv->cache_buffer = hyscan_buffer_new ();

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
  g_clear_object (&priv->cache);
  g_clear_pointer (&priv->arrow_default.surface, cairo_surface_destroy);
  g_clear_pointer (&priv->arrow_lost.surface, cairo_surface_destroy);
  g_clear_object (&priv->tile_buffer);
  g_clear_object (&priv->cache_buffer);

  g_mutex_lock (&priv->track_lock);
  g_queue_free_full (priv->track, (GDestroyNotify) hyscan_gtk_map_way_layer_point_free);
  g_mutex_unlock (&priv->track_lock);

  g_mutex_clear (&priv->track_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_way_layer_parent_class)->finalize (object);
}

static void
hyscan_gtk_map_way_layer_interface_init (HyScanGtkLayerInterface *iface)
{
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

  /* Фиксируем изменение параметров. */
  ++priv->param_mod_count;

  g_mutex_unlock (&priv->track_lock);

  /* Ставим флаг о необходимости перерисовки. */
  g_atomic_int_set (&priv->redraw, TRUE);

  return TRUE;
}

static void
hyscan_gtk_map_way_layer_mod_cache_key (HyScanGtkMapWayLayer *way_layer,
                                          guint x,
                                          guint y,
                                          guint z,
                                          gchar *key,
                                          gulong key_len)
{
  g_snprintf (key, key_len,
              "GtkMapWayLayer.mc.%d.%d.%d.%d",
              way_layer->priv->param_mod_count,
              x, y, z);
}

/* Записывает, что актуальная версия тайлов с указанным отрезком point0 - point1
 * соответствует номеру изменения mod_count.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_way_layer_set_section_mod (HyScanGtkMapWayLayer *way_layer,
                                            guint                   mod_count,
                                            HyScanGtkMapPoint      *point0,
                                            HyScanGtkMapPoint      *point1)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gdouble *scales;
  guint scales_len;
  guint scale_idx;

  HyScanGtkMapWayLayerTileMod track_mod;
  HyScanBuffer *buffer;

  /* Оборачиваен номер изменения в буфер. */
  track_mod.magic = CACHE_TRACK_MOD_MAGIC;
  track_mod.mod = mod_count;
  buffer = hyscan_buffer_new ();
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, &track_mod, sizeof (track_mod));

  /* Для каждого масштаба определяем тайлы, на которых лежит этот отрезок. */
  scales = hyscan_gtk_map_get_scales_cifro (priv->map, &scales_len);
  for (scale_idx = 0; scale_idx < scales_len; scale_idx++)
    {
      gint x, y, to_x, to_y;
      gdouble x0, y0, x1, y1;

      /* Определяем на каких тайлах лежат концы отрезка. */
      hyscan_gtk_map_tile_grid_value_to_tile (priv->tile_grid, scale_idx, point0->x, point0->y, &x0, &y0);
      hyscan_gtk_map_tile_grid_value_to_tile (priv->tile_grid, scale_idx, point1->x, point1->y, &x1, &y1);

      /* Проходим все тайлы внутри найденной области и обновляем номер изменения трека. */
      to_x = MAX (x0, x1);
      to_y = MAX (y0, y1);
      for (x = MIN (x0, x1); x <= to_x; ++x)
        for (y = MIN (y0, y1); y <= to_y; ++y)
          {
            gchar cache_key[255];

            /* Обновляем в кэше номер изменения трека. */
            hyscan_gtk_map_way_layer_mod_cache_key (way_layer, x, y, scale_idx, cache_key, sizeof (cache_key));
            hyscan_cache_set (priv->cache, cache_key, NULL, buffer);
          }
    }

  g_object_unref (buffer);
  g_free (scales);
}

/* Обрабатывает "истекшие" путевые точки, т.е. те, у которых время жизни закончилось:
 * - удаляет эти точки из трека,
 * - обновляет тайлы с удаленными точками.
 *
 * Функция должна вызываться за мьютексом! */
static void
hyscan_gtk_map_way_layer_set_expired_mod (HyScanGtkMapWayLayer *way_layer,
                                            guint                   mod_count,
                                            gdouble                 time)
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
      hyscan_gtk_map_way_layer_set_section_mod (way_layer, mod_count, &cur_point->coord, &prev_point->coord);
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
hyscan_gtk_map_way_layer_set_head_mod (HyScanGtkMapWayLayer *way_layer,
                                         guint                   mod_count)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  GList *head_l;
  HyScanGtkMapWayLayerPoint *track_point0, *track_point1 = NULL;

  if (priv->cache == NULL || priv->map == NULL)
    return;

  /* Определяем концы последнего отрезка: point0 и point1. */
  head_l = priv->track->head;
  if (head_l == NULL || head_l->next == NULL)
    return;

  track_point0 = head_l->data;
  track_point1 = head_l->next->data;

  /* Актуализируем тайлы, содержащие найденный отрезок. */
  hyscan_gtk_map_way_layer_set_section_mod (way_layer, mod_count, &track_point0->coord, &track_point1->coord);
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
        hyscan_gtk_map_geo_to_value (priv->map, point.coord.geo, &point.coord.x, &point.coord.y);

      g_queue_push_head (priv->track, hyscan_gtk_map_way_layer_point_copy (&point));

      hyscan_gtk_map_way_layer_set_head_mod (way_layer, priv->track_mod_count);
    }

  /* Фиксируем обрыв. */
  priv->track_lost = (coord == NULL);

  /* Удаляем устаревшие по life_time точки. */
  hyscan_gtk_map_way_layer_set_expired_mod (way_layer, priv->track_mod_count, time);

  g_mutex_unlock (&priv->track_lock);

  if (hyscan_gtk_layer_get_visible (HYSCAN_GTK_LAYER (way_layer)))
    g_atomic_int_set (&priv->redraw, TRUE);
}

/* Создаёт cairo-поверхность с изображением стрелки в голове трека. */
static void
hyscan_gtk_map_track_create_arrow (HyScanGtkMapWayLayerArrow *arrow,
                                   GdkRGBA                     *color_fill,
                                   GdkRGBA                     *color_stroke)
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

  g_atomic_int_set (&priv->redraw, TRUE);
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

  /* Останавливаем перерисовку. */
  g_source_remove (priv->redraw_tag);
  priv->redraw_tag = 0;

  /* Останавливаем очередь. */
  hyscan_task_queue_shutdown (priv->task_queue);
  g_clear_object (&priv->task_queue);

  /* Отключаемся от карты. */
  g_signal_handlers_disconnect_by_data (priv->map, layer);
  g_clear_object (&priv->map);
  g_clear_object (&priv->tile_grid);
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
      HyScanGtkMapPoint *point = &track_point->coord;
      gdouble x, y;

      /* Координаты точки на поверхности. */
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

/* Проверяет, находится ли точка point внутри указанного региона. */
static inline gboolean
hyscan_gtk_map_way_layer_is_point_inside (HyScanGtkMapPoint *point,
                                            gdouble            from_x,
                                            gdouble            to_x,
                                            gdouble            from_y,
                                            gdouble            to_y)
{
  return point->x > from_x && point->x < to_x && point->y > from_y && point->y < to_y;
}

/* Возвращает TRUE, если значение boundary находится между val1 и val2. */
static inline gboolean
hyscan_gtk_map_way_layer_is_cross (gdouble val1,
                                     gdouble val2,
                                     gdouble boundary)
{
  return ((val1 < boundary) - (val2 > boundary) == 0);
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
      is_inside = hyscan_gtk_map_way_layer_is_point_inside (point0, from_x, to_x, from_y, to_y) ||
                  hyscan_gtk_map_way_layer_is_point_inside (point1, from_x, to_x, from_y, to_y);

      /* ... либо отрезок может пересекать эту область. */
      if (!is_inside)
        {
          gboolean cross_x, cross_y;

          cross_x = hyscan_gtk_map_way_layer_is_cross (point0->x, point1->x, from_x) ||
                    hyscan_gtk_map_way_layer_is_cross (point0->x, point1->x, to_x);
          cross_y = hyscan_gtk_map_way_layer_is_cross (point0->y, point1->y, from_y) ||
                    hyscan_gtk_map_way_layer_is_cross (point0->y, point1->y, to_y);

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
static guint
hyscan_gtk_map_way_layer_fill_tile (HyScanGtkMapWayLayer *way_layer,
                                      HyScanGtkMapTile       *tile)
{
  cairo_t *tile_cairo;

  gdouble x_val, y_val;
  gdouble x_val_to, y_val_to;
  cairo_surface_t *surface;
  gint width;
  gdouble scale;

  guint mod_count;

  width = hyscan_gtk_map_tile_get_size (tile);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, width);
  tile_cairo = cairo_create (surface);

  /* Заполняем поверхность тайла. */
  hyscan_gtk_map_tile_get_bounds (tile, &x_val, &x_val_to, &y_val, &y_val_to);
  scale = hyscan_gtk_map_tile_get_scale (tile);
  mod_count = hyscan_gtk_map_way_layer_draw_region (way_layer, tile_cairo, x_val, x_val_to, y_val_to, y_val, scale);

  /* Записываем поверхность в тайл. */
  hyscan_gtk_map_tile_set_surface (tile, surface);

#ifdef HYSCAN_GTK_MAP_DEBUG_FPS
  {
    GRand *rand;
    gchar tile_num[100];
    gint x, y;

    x = hyscan_gtk_map_tile_get_x (tile);
    y = hyscan_gtk_map_tile_get_y (tile);
    rand = g_rand_new ();
    g_snprintf (tile_num, sizeof (tile_num), "tile %d, %d", x, y);
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

  cairo_surface_destroy (surface);
  cairo_destroy (tile_cairo);

  return mod_count;
}

/* Ключ кэширования. */
static void
hyscan_gtk_map_way_layer_tile_cache_key (HyScanGtkMapWayLayer *way_layer,
                                           HyScanGtkMapTile       *tile,
                                           gchar                  *key,
                                           gulong                  key_len)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  g_snprintf (key, key_len,
              "GtkMapWayLayer.%u.%d.%d.%u",
              priv->param_mod_count,
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_zoom (tile));
}

/* Возвращает статус актуальности тайла. */
static HyScanGtkMapWayLayerModState
hyscan_gtk_map_way_layer_get_mod_actual (HyScanGtkMapWayLayer *way_layer,
                                           HyScanGtkMapTile       *tile,
                                           guint                   mod)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gboolean found;
  gchar mod_cache_key[127];
  guint x, y, z;
  HyScanBuffer *buffer;

  HyScanGtkMapWayLayerTileMod last_update;
  gint state;

  x = hyscan_gtk_map_tile_get_x (tile);
  y = hyscan_gtk_map_tile_get_y (tile);
  z = hyscan_gtk_map_tile_get_zoom (tile);

  hyscan_gtk_map_way_layer_mod_cache_key (way_layer, x, y, z, mod_cache_key, sizeof (mod_cache_key));

  buffer = hyscan_buffer_new ();
  hyscan_buffer_wrap_data (buffer, HYSCAN_DATA_BLOB, &last_update, sizeof (last_update));

  found = hyscan_cache_get (priv->cache, mod_cache_key, NULL, buffer);

  /* Если кэш не попал, то запишем в него значение, которое сделает тайл неактуальным. */
  if (!found || last_update.magic != CACHE_TRACK_MOD_MAGIC)
    {
      last_update.magic = CACHE_TRACK_MOD_MAGIC;
      last_update.mod = mod + 1;
      hyscan_cache_set (priv->cache, mod_cache_key, NULL, buffer);
    }

  if (last_update.mod > mod + 5)
    state = STATE_IRRELEVANT;
  else if (last_update.mod > mod)
    state = STATE_OUTDATED;
  else
    state = STATE_ACTUAL;

  g_object_unref (buffer);

  return state;
}

/* Получает из кэша пискельные данные тайла и записывает их в tile_buffer. */
static gboolean
hyscan_gtk_map_way_layer_cache_get (HyScanGtkMapWayLayer *way_layer,
                                      HyScanGtkMapTile       *tile,
                                      HyScanBuffer           *tile_buffer,
                                      gboolean               *refill)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;
  HyScanGtkMapWayLayerCacheHeader header;

  gboolean found = FALSE;
  gboolean refill_ = TRUE;

  if (priv->cache == NULL)
    goto exit;

  /* Ищем в кэше. */
  hyscan_gtk_map_way_layer_tile_cache_key (way_layer, tile, priv->cache_key, CACHE_KEY_LEN);
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, priv->cache_key, NULL, sizeof (header), priv->cache_buffer, tile_buffer))
    goto exit;

  /* Верифицируем. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.size != hyscan_buffer_get_size (tile_buffer)))
    {
      goto exit;
    }

  /* Проверяем, насколько актуален найденный тайл. */
  switch (hyscan_gtk_map_way_layer_get_mod_actual (way_layer, tile, header.mod))
    {
    case STATE_ACTUAL:
      refill_ = FALSE;
      found = TRUE;
      break;

    case STATE_OUTDATED:
      refill_ = TRUE;
      found = TRUE;
      break;

    case STATE_IRRELEVANT:
      refill_ = TRUE;
      found = FALSE;
      break;
    }

exit:
  *refill = refill_;

  return found;
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_way_layer_cache_set (HyScanGtkMapWayLayer *way_layer,
                                      HyScanGtkMapTile       *tile,
                                      guint                   mod_count)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gchar cache_key[CACHE_KEY_LEN];

  HyScanGtkMapWayLayerCacheHeader header;
  HyScanBuffer *header_buf;

  const guint8 *tile_data;
  HyScanBuffer *tile_buf;

  cairo_surface_t *surface;

  if (priv->cache == NULL)
    return;

  surface = hyscan_gtk_map_tile_get_surface (tile);
  if (surface == NULL)
    return;

  tile_buf = hyscan_buffer_new ();
  header_buf = hyscan_buffer_new ();

  /* Оборачиваем в буфер заголовок с информацией об изображении. */
  header.magic = CACHE_HEADER_MAGIC;
  header.mod = mod_count;
  header.size = cairo_image_surface_get_stride (surface) * cairo_image_surface_get_height (surface);
  hyscan_buffer_wrap_data (header_buf, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Оборачиваем в буфер пиксельные данные. */
  tile_data = cairo_image_surface_get_data (surface);
  hyscan_buffer_wrap_data (tile_buf, HYSCAN_DATA_BLOB, (gpointer) tile_data, header.size);

  /* Помещаем все данные в кэш. */
  hyscan_gtk_map_way_layer_tile_cache_key (way_layer, tile, cache_key, CACHE_KEY_LEN);
  hyscan_cache_set2 (priv->cache, cache_key, NULL, header_buf, tile_buf);

  cairo_surface_destroy (surface);
  g_object_unref (header_buf);
  g_object_unref (tile_buf);
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

/* Рисует трек по тайлам. */
static void
hyscan_gtk_map_way_layer_draw_tiles (HyScanGtkMapWayLayer *way_layer,
                                       cairo_t                *cairo)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  gint scale_idx;
  gdouble scale;

  gint x, y;
  gint from_tile_x, to_tile_x, from_tile_y, to_tile_y;

  /* Инициализируем тайловую сетку grid. */
  scale_idx = hyscan_gtk_map_get_scale_idx (priv->map, &scale);

  /* Определяем область рисования. */
  hyscan_gtk_map_tile_grid_get_view_cifro (priv->tile_grid, GTK_CIFRO_AREA (priv->map), scale_idx,
                                           &from_tile_x, &to_tile_x, &from_tile_y, &to_tile_y);

  /* Рисуем тайлы по очереди. */
  for (x = from_tile_x; x <= to_tile_x; x++)
    for (y = from_tile_y; y <= to_tile_y; y++)
      {
        gdouble x_val, y_val;
        gdouble x_source, y_source;
        HyScanGtkMapTile *tile;
        gboolean refill;
        gboolean found;

        /* Заполняем тайл. */
        tile = hyscan_gtk_map_tile_new (priv->tile_grid, x, y, scale_idx);
        found = hyscan_gtk_map_way_layer_cache_get (way_layer, tile, priv->tile_buffer, &refill);

        /* Если надо, отправляем тайл на перерисовку. */
        if (refill)
          hyscan_task_queue_push (priv->task_queue, G_OBJECT (tile));

        /* Если тайл наден, то рисуем его. */
        if (found)
          {
            guint32 size;
            gpointer cached_data;

            cairo_surface_t *surface;

            /* Загружаем поверхность тайла из буфера. */
            cached_data = hyscan_buffer_get_data (priv->tile_buffer, &size);
            hyscan_gtk_map_tile_set_surface_data (tile, cached_data, size);
            surface = hyscan_gtk_map_tile_get_surface (tile);

            /* Переносим поверхность тайла на изображение слоя. */
            hyscan_gtk_map_tile_get_bounds (tile, &x_val, NULL, &y_val, NULL);
            gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x_source, &y_source, x_val, y_val);
            cairo_set_source_surface (cairo, surface, x_source, y_source);
            cairo_paint (cairo);

            /* Освобождаем поверхность. */
            cairo_surface_destroy (surface);
            hyscan_gtk_map_tile_set_surface (tile, NULL);
          }

        g_object_unref (tile);
      }

  /* Отправляем на заполнение все тайлы. */
  hyscan_task_queue_push_end (priv->task_queue);
}

/* Обработчик сигнала "visible-draw".
 * Рисует на карте движение объекта. */
static void
hyscan_gtk_map_way_layer_draw (HyScanGtkMap           *map,
                                 cairo_t                *cairo,
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

    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x, &y, last_point->x, last_point->y);
    bearing = last_point->geo.h;

    g_mutex_unlock (&priv->track_lock);
  }

  /* Рисуем трек тайлами по одной из стратегий: _draw_full() или _draw_tiles(). */
  if (priv->cache == NULL)
    hyscan_gtk_map_way_layer_draw_full (way_layer, cairo);
  else
    hyscan_gtk_map_way_layer_draw_tiles (way_layer, cairo);

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
               g_list_length (priv->track));
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

  ++priv->param_mod_count;

  for (track_l = priv->track->head; track_l != NULL; track_l = track_l->next)
    {
      point = track_l->data;
      hyscan_gtk_map_geo_to_value (priv->map, point->coord.geo, &point->coord.x, &point->coord.y);
    }

  g_mutex_unlock (&priv->track_lock);
}

static void
hyscan_gtk_map_way_layer_update_grid (HyScanGtkMapWayLayerPrivate *priv)
{
  guint scales_len, i;
  gdouble *scales;

  scales = hyscan_gtk_map_get_scales_cifro (priv->map, &scales_len);
  for (i = 0; i < scales_len; ++i)
    scales[i] *= TILE_SIZE;

  // todo: lock mutex when use/update tile_grid?
  g_clear_object (&priv->tile_grid);
  priv->tile_grid = hyscan_gtk_map_tile_grid_new_from_cifro (GTK_CIFRO_AREA (priv->map), 0, TILE_SIZE);
  hyscan_gtk_map_tile_grid_set_scales (priv->tile_grid, scales, scales_len);
  g_free (scales);
}


/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_way_layer_proj_notify (HyScanGtkMapWayLayer *way_layer,
                                        GParamSpec             *pspec)
{
  HyScanGtkMapWayLayerPrivate *priv = way_layer->priv;

  hyscan_gtk_map_way_layer_update_grid (priv);
  hyscan_gtk_map_way_layer_update_points (priv);
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

  /* Создаем очередь задач по заполнению тайлов. */
  priv->task_queue = hyscan_task_queue_new (hyscan_gtk_map_way_layer_process, way_layer,
                                            (GCompareFunc) hyscan_gtk_map_tile_compare);

  /* Подключаемся к карте. */
  priv->map = g_object_ref (container);
  hyscan_gtk_map_way_layer_update_grid (way_layer->priv);
  hyscan_gtk_map_way_layer_update_points (way_layer->priv);
  g_signal_connect_after (priv->map, "visible-draw", G_CALLBACK (hyscan_gtk_map_way_layer_draw), way_layer);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_way_layer_proj_notify), way_layer);

  /* Запускаем функцию перерисовки. Ограничим ее периодом не чаще 1 раза в 40 милисекунд, чтобы не перегружать ЦП. */
  priv->redraw_tag = g_timeout_add (40, (GSourceFunc) hyscan_gtk_map_way_layer_queue_draw, way_layer);
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

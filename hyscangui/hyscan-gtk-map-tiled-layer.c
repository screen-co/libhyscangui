/* hyscan-gtk-map-tiled-layer.c
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
 * SECTION: hyscan-gtk-map-tiled-layer
 * @Short_description: Класс тайлового слоя
 * @Title: HyScanGtkMapTiledLayer
 * @See_also: #HyScanGtkLayer, #HyScanGtkMap, #GtkCifroArea
 *
 * Класс реализует интерфейса слоя #HyScanGtkLayer и предназначен для изображения
 * вычислительно сложного слоя с изменяемым содержимым в виде тайлов.
 *
 * Слой разбивает всю область рисования на тайлы и рисует изображаемую область
 * потайлово. Заполнение тайлов выполняет только при изменении параметров слоя
 * или входных данных. В остальное время слой получает тайлы из кэша, и таким
 * образом экономит вычислительные ресурсы.
 *
 * Для использования тайлового слоя необходимо унаследовать класс #HyScanGtkMapTiledLayer
 * и реализовать функцию класса #HyScanGtkMapTiledLayerClass.fill_tile.
 *
 * Класс предоставляет следующие функции:
 * - hyscan_gtk_map_tiled_layer_draw() - рисует текущую видимую область;
 * - hyscan_gtk_map_tiled_layer_set_area_mod() - фиксирует факт изменения указанной области;
 * - hyscan_gtk_map_tiled_layer_set_param_mod() - фиксирует факт изменения параметров слоя;
 * - hyscan_gtk_map_tiled_layer_request_draw() - запрашивает перерисовку слоя.
 *
 */

#include "hyscan-gtk-map-tiled-layer.h"
#include "hyscan-gtk-map.h"
#include <hyscan-cartesian.h>
#include <hyscan-task-queue.h>
#include <hyscan-buffer.h>

#define TILE_SIZE              256           /* Размер тайла. */

/* Раскомментируйте строку ниже для вывода отладочной информации на тайлах. */
// #define DEBUG_TILES

enum
{
  PROP_O,
  PROP_CACHE_SIZE
};

typedef struct
{
  HyScanMapTile                *tile;              /* Заполненный тайл. */
  guint                         fill_mod;          /* Номер изменения данных в момент отрисовки тайла. */
  guint                         actual_mod;        /* Актуальный номер изменения данных для тайла. */
  guint                         param_mod;         /* Номер изменения параметров в момент отрисовки тайла. */
  HyScanGeoCartesian2D          area_from;         /* Граница области, которую покрывает тайл. */
  HyScanGeoCartesian2D          area_to;           /* Граница области, которую покрывает тайл. */
} HyScanGtkMapTiledLayerCache;

struct _HyScanGtkMapTiledLayerPrivate
{
  HyScanGtkMap                 *map;               /* Виджет карты, на котором размещен слой. */
  HyScanMapTileGrid            *tile_grid;         /* Тайловая сетка. */
  HyScanTaskQueue              *task_queue;        /* Очередь по загрузке тайлов. */

  GRWLock                       rw_lock;           /* Блокировка доступа к cached_tiles. */
  GQueue                       *cached_tiles;      /* Список кэшированных тайлов. */
  guint                         cache_size;        /* Максимально разрешённое число тайлов в кэше. */

  guint                         mod_count;         /* Номер изменения данных. */
  guint                         param_mod_count;   /* Номер изменения в параметрах отображения слоя. */

  gboolean                      redraw;            /* Признак необходимости перерисовки. */
  guint                         redraw_tag;        /* Тэг функции, которая запрашивает перерисовку. */
};

static void    hyscan_gtk_map_tiled_layer_interface_init           (HyScanGtkLayerInterface *iface);
static void    hyscan_gtk_map_tiled_layer_set_property             (GObject                *object,
                                                                    guint                   prop_id,
                                                                    const GValue           *value,
                                                                    GParamSpec             *pspec);
static void    hyscan_gtk_map_tiled_layer_object_constructed       (GObject                *object);
static void    hyscan_gtk_map_tiled_layer_object_finalize          (GObject                *object);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTiledLayer, hyscan_gtk_map_tiled_layer, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapTiledLayer)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_tiled_layer_interface_init))

static void
hyscan_gtk_map_tiled_layer_class_init (HyScanGtkMapTiledLayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiled_layer_set_property;

  object_class->constructed = hyscan_gtk_map_tiled_layer_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiled_layer_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE_SIZE,
    g_param_spec_uint ("cache-size", "Tile cache size", "Maximum allowed number of tiles in cache",
                       30, 1000, 250,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tiled_layer_init (HyScanGtkMapTiledLayer *gtk_map_tiled_layer)
{
  gtk_map_tiled_layer->priv = hyscan_gtk_map_tiled_layer_get_instance_private (gtk_map_tiled_layer);
}

static void
hyscan_gtk_map_tiled_layer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  switch (prop_id)
    {
    case PROP_CACHE_SIZE:
      priv->cache_size = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tiled_layer_object_constructed (GObject *object)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tiled_layer_parent_class)->constructed (object);

  priv->cached_tiles = g_queue_new ();
  g_rw_lock_init (&priv->rw_lock);
}

static void
hyscan_gtk_map_tiled_layer_object_finalize (GObject *object)
{
  HyScanGtkMapTiledLayer *gtk_map_tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (object);
  HyScanGtkMapTiledLayerPrivate *priv = gtk_map_tiled_layer->priv;

  g_rw_lock_clear (&priv->rw_lock);
  g_queue_free (priv->cached_tiles);

  G_OBJECT_CLASS (hyscan_gtk_map_tiled_layer_parent_class)->finalize (object);
}

/* Получает из кэша изображение поверхности тайла и записывает их в @tile. */
static gboolean
hyscan_gtk_map_tiled_layer_cache_get (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanMapTile          *tile,
                                      gboolean               *refill)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  gboolean found = FALSE;
  gboolean refill_ = TRUE;

  GList *tile_l;

  g_rw_lock_reader_lock (&priv->rw_lock);

  /* Ищем в кэше тайл. */
  for (tile_l = priv->cached_tiles->head; tile_l != NULL; tile_l = tile_l->next)
    {
      HyScanGtkMapTiledLayerCache *cache = tile_l->data;
      HyScanMapTile *cached_tile = cache->tile;
      cairo_surface_t *surface;

      if (hyscan_map_tile_compare (cached_tile, tile) != 0)
        continue;

      found = TRUE;
      refill_ = cache->actual_mod > cache->fill_mod;

      surface = hyscan_map_tile_get_surface (cached_tile);
      hyscan_map_tile_set_surface (tile, surface);

      cairo_surface_destroy (surface);
    }

  g_rw_lock_reader_unlock (&priv->rw_lock);

  *refill = refill_;

#ifdef DEBUG_TILES
  {
    static guint total = 0;
    static guint hit = 0;

    ++total;
    if (found)
      ++hit;

    g_message ("Cache hit: %d/%d = %.2f%%", hit, total, 100.0 * hit / total);
  }
#endif

  return found;
}

static void
hyscan_gtk_map_tiled_layer_update_grid (HyScanGtkMapTiledLayerPrivate *priv)
{
  guint scales_len, i;
  gdouble *scales;

  scales = hyscan_gtk_map_get_scales_cifro (priv->map, &scales_len);
  for (i = 0; i < scales_len; ++i)
    scales[i] *= TILE_SIZE;

  // todo: lock mutex when use/update tile_grid?
  g_clear_object (&priv->tile_grid);
  priv->tile_grid = hyscan_map_tile_grid_new_from_cifro (GTK_CIFRO_AREA (priv->map), 0, TILE_SIZE);
  hyscan_map_tile_grid_set_scales (priv->tile_grid, scales, scales_len);
  g_free (scales);
}

/* Обработчик сигнала "notify::projection".
 * Пересчитывает координаты точек, если изменяется картографическая проекция. */
static void
hyscan_gtk_map_tiled_layer_proj_notify (HyScanGtkMapTiledLayer *tiled_layer,
                                        GParamSpec             *pspec)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  hyscan_gtk_map_tiled_layer_update_grid (priv);
  hyscan_gtk_map_tiled_layer_set_param_mod (tiled_layer);
}

static guint
hyscan_gtk_map_tiled_layer_fill_tile (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanMapTile          *tile)
{
  HyScanGtkMapTiledLayerClass *klass = HYSCAN_GTK_MAP_TILED_LAYER_GET_CLASS (tiled_layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;
  guint mod_count;

  g_return_val_if_fail (klass->fill_tile != NULL, 0);

  /* Запоминаем номер изменения данных, соответствующий тайлу. */
  mod_count = g_atomic_int_get (&priv->mod_count);
  klass->fill_tile (tiled_layer, tile);

#ifdef DEBUG_TILES
  {
    GRand *rand;
    gchar tile_num[100];
    gint x, y;
    guint tile_size;
    cairo_t *cairo;
    cairo_surface_t *surface;

    surface = hyscan_map_tile_get_surface (tile);
    cairo = cairo_create (surface);

    x = hyscan_map_tile_get_x (tile);
    y = hyscan_map_tile_get_y (tile);
    tile_size = hyscan_map_tile_get_size (tile);

    /* Заливка рандомного цвета. */
    rand = g_rand_new ();
    cairo_set_source_rgba (cairo,
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           g_rand_double_range (rand, 0.0, 1.0),
                           0.1);
    cairo_paint (cairo);
    cairo_set_source_rgb (cairo, 0.2, 0.2, 0);

    /* Название тайла. */
    // g_snprintf (tile_num, sizeof (tile_num), "tile %d, %d", x, y);
    // cairo_move_to (cairo, tile_size / 2.0, tile_size / 2.0);
    // cairo_show_text (cairo, tile_num); // here

    cairo_surface_destroy (surface);
    cairo_destroy (cairo);
  }
#endif

  return mod_count;
}

static void
hyscan_gtk_map_tiled_layer_cache_free (HyScanGtkMapTiledLayerCache *cache)
{
  g_object_unref (cache->tile);
  g_slice_free (HyScanGtkMapTiledLayerCache, cache);
}

static void
hyscan_gtk_map_tiled_layer_cache_trim (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  g_rw_lock_writer_lock (&priv->rw_lock);
  while (priv->cached_tiles->length > priv->cache_size)
    {
      HyScanGtkMapTiledLayerCache *cache;

      cache = g_queue_pop_tail (priv->cached_tiles);
      hyscan_gtk_map_tiled_layer_cache_free (cache);
    }
  g_rw_lock_writer_unlock (&priv->rw_lock);
}

static void
hyscan_gtk_map_tiled_layer_cache_clean (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  g_rw_lock_writer_lock (&priv->rw_lock);
  g_queue_free_full (priv->cached_tiles, (GDestroyNotify) hyscan_gtk_map_tiled_layer_cache_free);
  priv->cached_tiles = g_queue_new ();
  g_rw_lock_writer_unlock (&priv->rw_lock);
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_tiled_layer_cache_set (HyScanGtkMapTiledLayer *tiled_layer,
                                      HyScanMapTile          *tile,
                                      guint                   mod_count,
                                      guint                   param_mod_count)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;
  GList *tile_l;
  HyScanGtkMapTiledLayerCache *cache = NULL;
  gboolean found = FALSE;

  g_rw_lock_writer_lock (&priv->rw_lock);

  /* Ищем в кэше тайл. */
  for (tile_l = priv->cached_tiles->head; tile_l != NULL; tile_l = tile_l->next)
    {
      cache = tile_l->data;

      if (hyscan_map_tile_compare (cache->tile, tile) != 0)
        continue;

      /* Если тайл нашёлся, выдёргиваем его из стека. */
      g_queue_unlink (priv->cached_tiles, tile_l);
      found = TRUE;
      break;
    }

  if (G_UNLIKELY (!found))
    {
      cache = g_slice_new0 (HyScanGtkMapTiledLayerCache);
      hyscan_map_tile_get_bounds (tile, &cache->area_from, &cache->area_to);

      tile_l = g_list_alloc ();
      tile_l->data = cache;
    }
  else
    {
      g_object_unref (cache->tile);
    }

  /* Пишем в кэш актуальную информацию. */
  cache->fill_mod = mod_count;
  cache->param_mod = param_mod_count;
  cache->tile = g_object_ref (tile);

  /* Помещаем заполненный тайл на верх стека. */
  g_queue_push_head_link (priv->cached_tiles, tile_l);

  g_rw_lock_writer_unlock (&priv->rw_lock);

  hyscan_gtk_map_tiled_layer_cache_trim (tiled_layer);
}


/* Обработчик задачи из очереди HyScanTaskQueue.
 * Заполняет переданный тайл. */
static void
hyscan_gtk_map_tiled_layer_process (GObject      *task,
                                    gpointer      user_data,
                                    GCancellable *cancellable)
{
  HyScanMapTile *tile = HYSCAN_MAP_TILE (task);
  HyScanGtkMapTiledLayer *tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (user_data);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  guint mod_count, param_mod_count;

  /* Заполняет тайл и помещаем его в кэш. */
  param_mod_count = g_atomic_int_get (&priv->param_mod_count);
  mod_count = hyscan_gtk_map_tiled_layer_fill_tile (tiled_layer, tile);
  hyscan_gtk_map_tiled_layer_cache_set (tiled_layer, tile, mod_count, param_mod_count);

  /* Просим перерисовать. */
  hyscan_gtk_map_tiled_layer_request_draw (tiled_layer);
}

/* Запрашивает перерисовку виджета карты, если пришёл хотя бы один тайл из видимой области. */
static gboolean
hyscan_gtk_map_tiled_layer_queue_draw (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  if (g_atomic_int_compare_and_exchange (&priv->redraw, TRUE, FALSE))
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  return G_SOURCE_CONTINUE;
}

/* Реализация HyScanGtkLayerInterface.added.
 * Подключается к сигналам карты при добавлении слоя на карту. */
static void
hyscan_gtk_map_tiled_layer_added (HyScanGtkLayer          *layer,
                                  HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTiledLayer *tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));

  /* Создаем очередь задач по заполнению тайлов. */
  priv->task_queue = hyscan_task_queue_new (hyscan_gtk_map_tiled_layer_process, tiled_layer,
                                            (GCompareFunc) hyscan_map_tile_compare);

  /* Подключаемся к карте. */
  priv->map = g_object_ref (HYSCAN_GTK_MAP (container));
  hyscan_gtk_map_tiled_layer_update_grid (tiled_layer->priv);
  g_signal_connect_swapped (priv->map, "notify::projection", G_CALLBACK (hyscan_gtk_map_tiled_layer_proj_notify), tiled_layer);

  /* Запускаем функцию перерисовки. Ограничим ее периодом не чаще 1 раза в 40 милисекунд, чтобы не перегружать ЦП. */
  priv->redraw_tag = g_timeout_add (40, (GSourceFunc) hyscan_gtk_map_tiled_layer_queue_draw, tiled_layer);
}

/* Реализация HyScanGtkLayerInterface.removed.
 * Отключается от сигналов карты при удалении слоя с карты. */
static void
hyscan_gtk_map_tiled_layer_removed (HyScanGtkLayer *layer)
{
  HyScanGtkMapTiledLayer *tiled_layer = HYSCAN_GTK_MAP_TILED_LAYER (layer);
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

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

static void
hyscan_gtk_map_tiled_layer_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->added = hyscan_gtk_map_tiled_layer_added;
  iface->removed = hyscan_gtk_map_tiled_layer_removed;
}

/* Рисует трек по тайлам. */
void
hyscan_gtk_map_tiled_layer_draw (HyScanGtkMapTiledLayer *tiled_layer,
                                 cairo_t                *cairo)
{
  HyScanGtkMapTiledLayerPrivate *priv = tiled_layer->priv;

  gint scale_idx;
  gdouble scale;

  gint x, y;
  gint from_tile_x, to_tile_x, from_tile_y, to_tile_y;

  /* Инициализируем тайловую сетку grid. */
  scale_idx = hyscan_gtk_map_get_scale_idx (priv->map, &scale);

  /* Определяем область рисования. */
  hyscan_map_tile_grid_get_view_cifro (priv->tile_grid, GTK_CIFRO_AREA (priv->map), scale_idx,
                                       &from_tile_x, &to_tile_x, &from_tile_y, &to_tile_y);

  /* Рисуем тайлы по очереди. */
  for (x = from_tile_x; x <= to_tile_x; x++)
    for (y = from_tile_y; y <= to_tile_y; y++)
      {
        HyScanGeoCartesian2D coord;
        gdouble x_source, y_source;
        HyScanMapTile *tile;
        gboolean refill;
        gboolean found;

        /* Заполняем тайл. */
        tile = hyscan_map_tile_new (priv->tile_grid, x, y, scale_idx);
        found = hyscan_gtk_map_tiled_layer_cache_get (tiled_layer, tile, &refill);

        /* Если тайл наден, то рисуем его. */
        if (found)
          {
            cairo_surface_t *surface;

            /* Загружаем поверхность тайла из буфера. */
            surface = hyscan_map_tile_get_surface (tile);

            /* Переносим поверхность тайла на изображение слоя. */
            hyscan_map_tile_get_bounds (tile, &coord, NULL);
            gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &x_source, &y_source, coord.x, coord.y);
            cairo_set_source_surface (cairo, surface, x_source, y_source);
            cairo_paint (cairo);

            /* Освобождаем поверхность. */
            cairo_surface_destroy (surface);
          }

        /* Если надо, отправляем тайл на перерисовку. */
        if (refill)
          {
            hyscan_map_tile_set_surface (tile, NULL);
            hyscan_task_queue_push (priv->task_queue, G_OBJECT (tile));
          }

        g_object_unref (tile);
      }

  /* Отправляем на заполнение все тайлы. */
  hyscan_task_queue_push_end (priv->task_queue);
}

/**
 * hyscan_gtk_map_tiled_layer_set_area_mod:
 * @tiled_layer: слой тайлов
 * @point0: координаты начала отрезка
 * @point1: координаты конца отрезка
 *
 * Фиксирует изменение области, в которой находится отрезок @point0 - @point1,
 * например, из-за поступления новых данных. Тайлы всех масштабов, содержащие
 * изображение этой области, будут считаться более  невалидными и при следующем
 * выводе потребуют перерисовки.
 *
 */
void
hyscan_gtk_map_tiled_layer_set_area_mod (HyScanGtkMapTiledLayer *tiled_layer,
                                         HyScanGeoCartesian2D   *point0,
                                         HyScanGeoCartesian2D   *point1)
{
  HyScanGtkMapTiledLayerPrivate *priv;

  GList *cache_l;
  guint mod_count;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));
  priv = tiled_layer->priv;

  /* Увеличиваем счетчик изменений данных. */
  g_atomic_int_inc (&priv->mod_count);
  mod_count = g_atomic_int_get (&priv->mod_count);

  g_rw_lock_writer_lock (&priv->rw_lock);

  for (cache_l = priv->cached_tiles->head; cache_l != NULL; cache_l = cache_l->next)
    {
      HyScanGtkMapTiledLayerCache *cache = cache_l->data;

      /* Если отрезок проходит через тайл, то помечаем этот тайл как изменённый. */
      if (!hyscan_cartesian_is_inside (point0, point1, &cache->area_from, &cache->area_to))
        continue;

      cache->actual_mod = mod_count;
    }

  g_rw_lock_writer_unlock (&priv->rw_lock);
}

void
hyscan_gtk_map_tiled_layer_set_param_mod (HyScanGtkMapTiledLayer *tiled_layer)
{
  HyScanGtkMapTiledLayerPrivate *priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));
  priv = tiled_layer->priv;

  g_atomic_int_inc (&priv->param_mod_count);

  hyscan_gtk_map_tiled_layer_cache_clean (tiled_layer);
}

void
hyscan_gtk_map_tiled_layer_request_draw (HyScanGtkMapTiledLayer *tiled_layer)
{
  g_return_if_fail (HYSCAN_IS_GTK_MAP_TILED_LAYER (tiled_layer));

  g_atomic_int_set (&tiled_layer->priv->redraw, TRUE);
}

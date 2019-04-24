/* hyscan-gtk-map-tiles.c
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

/** Слой карт, загружает данные из тайлов. */

#include "hyscan-gtk-map-tiles.h"
#include <hyscan-task-queue.h>
#include <hyscan-gtk-map.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Раскомментируйте следующую строку для вывода отладочной информации. */
/* #define HYSCAN_GTK_MAP_TILES_DEBUG */

#define CACHE_HEADER_MAGIC     0x484d6170    /* Идентификатор заголовка кэша. */

#define TOTAL_TILES(zoom)      (((zoom) == 0 ? 1 : 2u << ((zoom) - 1)) - 1)
#define CLAMP_TILE(val, zoom)  CLAMP((gint)(val), 0, (gint)TOTAL_TILES ((zoom)))

enum
{
  PROP_O,
  PROP_CACHE,
  PROP_SOURCE,
  PROP_PRELOAD_MARGIN,
};

typedef struct
{
  guint32                      magic;               /* Идентификатор заголовка. */
  gsize                        size;                /* Размер пиксельных данных. */
} HyScanGtkMapTilesCacheHeader;

struct _HyScanGtkMapTilesPrivate
{
  HyScanGtkMap                *map;                 /* Виджет карты, на котором показываются тайлы. */
  HyScanTaskQueue             *task_queue;          /* Очередь задач по созданию тайлов. */
  HyScanGtkMapTileSource      *source;              /* Источник тайлов. */
  HyScanGtkMapTileGrid        *tile_grid;           /* Тайловая сетка. */

  /* Кэш. */
  HyScanCache                 *cache;               /* Кэш тайлов. */
  HyScanBuffer                *cache_buffer;        /* Буфер заголовка кэша данных для чтения. */
  HyScanBuffer                *tile_buffer;         /* Буфер данных поверхности тайла для чтения. */

  GList                       *filled_buffer;       /* Список заполненных тайлов, которые можно нарисовать. */
  GMutex                       filled_lock;         /* Блокировка доступа к filled_buffer. */

  /* Поверхность cairo с тайлами видимой области. */
  cairo_surface_t             *surface;             /* Поверхность cairo с тайлами. */
  gboolean                     surface_filled;      /* Признак того, что все тайлы заполнены. */
  guint                        from_x;              /* Координата по оси x левого тайла. */
  guint                        to_x;                /* Координата по оси x правого тайла. */
  guint                        from_y;              /* Координата по оси y верхнего тайла. */
  guint                        to_y;                /* Координата по оси y нижнего тайла. */
  guint                        zoom;                /* Зум отрисованных тайлов. */
  gdouble                      scale;               /* Масштаб отрисованных тайлов. */
  guint                        preload_margin;      /* Количество дополнительно загружаемых тайлов за пределами
                                                     * видимой области с каждой стороны. */

  cairo_surface_t             *dummy_tile;          /* Тайл-заглушка. Рисуется, когда настоящий тайл еще не загружен. */
  guint                        tile_size;           /* Размер тайла в пикселях. */
};

static void                 hyscan_gtk_map_tiles_set_property             (GObject                  *object,
                                                                           guint                     prop_id,
                                                                           const GValue             *value,
                                                                           GParamSpec               *pspec);
static void                 hyscan_gtk_map_tiles_interface_init           (HyScanGtkLayerInterface  *iface);
static void                 hyscan_gtk_map_tiles_object_constructed       (GObject                  *object);
static void                 hyscan_gtk_map_tiles_object_finalize          (GObject                  *object);
static void                 hyscan_gtk_map_tiles_queue_start              (HyScanGtkMapTiles        *tiles);
static void                 hyscan_gtk_map_tiles_queue_shutdown           (HyScanGtkMapTiles        *tiles);
static cairo_surface_t *    hyscan_gtk_map_tiles_create_dummy_tile        (HyScanGtkMapTilesPrivate *priv);
static void                 hyscan_gtk_map_tiles_draw                     (HyScanGtkMapTiles        *layer,
                                                                           cairo_t                  *cairo);
static void                 hyscan_gtk_map_tiles_load                     (HyScanGtkMapTile         *tile,
                                                                           HyScanGtkMapTiles        *layer,
                                                                           GCancellable             *cancellable);
static gboolean             hyscan_gtk_map_tiles_filled_buffer_flush      (HyScanGtkMapTiles        *layer);
static gboolean             hyscan_gtk_map_tiles_buffer_is_visible        (HyScanGtkMapTiles        *layer);
static void                 hyscan_gtk_map_tiles_get_cache_key            (HyScanGtkMapTile         *tile,
                                                                           gchar                    *key,
                                                                           gsize                     key_length);
static gboolean             hyscan_gtk_map_tiles_is_the_same_scale        (HyScanGtkMapTilesPrivate *priv,
                                                                           gdouble                   scale);
static gboolean             hyscan_gtk_map_tiles_draw_tile                (HyScanGtkMapTilesPrivate *priv,
                                                                           cairo_t                  *cairo,
                                                                           HyScanGtkMapTile         *tile);
static cairo_surface_t *    hyscan_gtk_map_tile_surface_make              (HyScanGtkMapTilesPrivate *priv,
                                                                           gboolean                 *filled);
static cairo_surface_t *    hyscan_gtk_map_tile_surface_scale             (cairo_surface_t          *origin,
                                                                           gdouble                   scale);
static guint                hyscan_gtk_map_tiles_get_optimal_zoom         (HyScanGtkMapTilesPrivate *priv);
static gdouble              hyscan_gtk_map_tiles_get_scaling              (HyScanGtkMapTilesPrivate *priv,
                                                                           guint                     zoom);
static void                 hyscan_gtk_map_tiles_get_view                 (HyScanGtkMapTiles        *layer,
                                                                           guint                     zoom,
                                                                           gint                     *from_tile_x,
                                                                           gint                     *to_tile_x,
                                                                           gint                     *from_tile_y,
                                                                           gint                     *to_tile_y);
static gboolean             hyscan_gtk_map_tiles_cache_get                (HyScanGtkMapTilesPrivate *priv,
                                                                           HyScanBuffer             *buffer,
                                                                           HyScanGtkMapTile         *tile);

G_DEFINE_TYPE_WITH_CODE (HyScanGtkMapTiles, hyscan_gtk_map_tiles, G_TYPE_INITIALLY_UNOWNED,
                         G_ADD_PRIVATE (HyScanGtkMapTiles)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_GTK_LAYER, hyscan_gtk_map_tiles_interface_init))

static void
hyscan_gtk_map_tiles_class_init (HyScanGtkMapTilesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_gtk_map_tiles_set_property;

  object_class->constructed = hyscan_gtk_map_tiles_object_constructed;
  object_class->finalize = hyscan_gtk_map_tiles_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE,
    g_param_spec_object ("cache", "Cache", "Cache object",
                         HYSCAN_TYPE_CACHE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_SOURCE,
    g_param_spec_object ("source", "Tile source", "GtkMapTileSource object",
                         HYSCAN_TYPE_GTK_MAP_TILE_SOURCE,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_PRELOAD_MARGIN,
    g_param_spec_uint ("preload-margin", "Preload margin", "GtkMapTileSource object", 0, G_MAXUINT, 0,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_gtk_map_tiles_init (HyScanGtkMapTiles *gtk_map_tiles)
{
  gtk_map_tiles->priv = hyscan_gtk_map_tiles_get_instance_private (gtk_map_tiles);
}

static void
hyscan_gtk_map_tiles_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_PRELOAD_MARGIN:
      priv->preload_margin = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_gtk_map_tiles_object_constructed (GObject *object)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_parent_class)->constructed (object);

  /* Создаём очередь задач по поиску тайлов. */
  hyscan_gtk_map_tiles_queue_start (gtk_map_tiles);

  priv->tile_grid = hyscan_gtk_map_tile_source_get_grid (priv->source);
  priv->tile_size = hyscan_gtk_map_tile_grid_get_tile_size (priv->tile_grid);
  priv->dummy_tile = hyscan_gtk_map_tiles_create_dummy_tile (priv);

  priv->cache_buffer = hyscan_buffer_new ();
  priv->tile_buffer = hyscan_buffer_new ();

  g_mutex_init (&priv->filled_lock);
}

static void
hyscan_gtk_map_tiles_object_finalize (GObject *object)
{
  HyScanGtkMapTiles *gtk_map_tiles = HYSCAN_GTK_MAP_TILES (object);
  HyScanGtkMapTilesPrivate *priv = gtk_map_tiles->priv;

  /* Освобождаем память. */
  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  g_clear_object (&priv->map);
  g_object_unref (priv->source);
  g_object_unref (priv->cache);
  g_object_unref (priv->cache_buffer);
  g_object_unref (priv->tile_buffer);
  cairo_surface_destroy (priv->dummy_tile);
  g_mutex_clear (&priv->filled_lock);

  G_OBJECT_CLASS (hyscan_gtk_map_tiles_parent_class)->finalize (object);
}

/* Реализация HyScanGtlLayerInterface.removed().
 * Отключается от сигналов виджета карты. */
static void
hyscan_gtk_map_tiles_removed (HyScanGtkLayer *gtk_layer)
{
  HyScanGtkMapTiles *tiles = HYSCAN_GTK_MAP_TILES (gtk_layer);
  HyScanGtkMapTilesPrivate *priv = tiles->priv;

  g_return_if_fail (priv->map != NULL);

  /* Завершаем работу очереди. */
  hyscan_gtk_map_tiles_queue_shutdown (tiles);

  /* Отключаемся от сигналов. */
  g_signal_handlers_disconnect_by_data (priv->map, gtk_layer);

  g_clear_object (&priv->tile_grid);
  g_clear_object (&priv->map);
}

/* Реализация HyScanGtlLayerInterface.added().
 * Подключается к сигналам виджета карты. */
static void
hyscan_gtk_map_tiles_added (HyScanGtkLayer          *gtk_layer,
                            HyScanGtkLayerContainer *container)
{
  HyScanGtkMapTiles *tiles = HYSCAN_GTK_MAP_TILES (gtk_layer);
  HyScanGtkMapTilesPrivate *priv = tiles->priv;

  g_return_if_fail (HYSCAN_IS_GTK_MAP (container));
  g_return_if_fail (priv->map == NULL);

  priv->map = g_object_ref (container);

  /* Запускаем очередь задач. */
  hyscan_gtk_map_tiles_queue_start (tiles);

  /* Подключаемся к сигналу обновления видимой области карты. */
  g_signal_connect_swapped (priv->map, "visible-draw",
                            G_CALLBACK (hyscan_gtk_map_tiles_draw), gtk_layer);
}

static void
hyscan_gtk_map_tiles_interface_init (HyScanGtkLayerInterface *iface)
{
  iface->get_visible = NULL;
  iface->set_visible = NULL;
  iface->added = hyscan_gtk_map_tiles_added;
  iface->removed = hyscan_gtk_map_tiles_removed;
  iface->grab_input = NULL;
  iface->get_icon_name = NULL;
}

/* Завершает работу очереди задач по загрузке тайлов и удаляет очередь. */
static void
hyscan_gtk_map_tiles_queue_shutdown (HyScanGtkMapTiles *tiles)
{
  HyScanGtkMapTilesPrivate *priv = tiles->priv;

  if (priv->task_queue == NULL)
    return;

  /* todo: этот shutdown иногда надолго блокирует GUI
   * Связано с тем, что долго слой tiles не сразу реагирует на сигнал отмены загрузки. */
  hyscan_task_queue_shutdown (priv->task_queue);
  g_clear_object (&priv->task_queue);
}

/* Создаёт очередь задач и запускает работу по загрузке тайлов. */
static void
hyscan_gtk_map_tiles_queue_start (HyScanGtkMapTiles *tiles)
{
  HyScanGtkMapTilesPrivate *priv = tiles->priv;

  if (priv->task_queue != NULL)
    return;

  priv->task_queue = hyscan_task_queue_new ((HyScanTaskQueueFunc) hyscan_gtk_map_tiles_load, tiles,
                                            (GCompareFunc) hyscan_gtk_map_tile_compare);
}

/* Создаёт изображений тайла заглушки в клеточку. */
static cairo_surface_t *
hyscan_gtk_map_tiles_create_dummy_tile (HyScanGtkMapTilesPrivate *priv)
{
  cairo_surface_t *surface;
  cairo_t *cairo;

  gdouble x = 0, y = 0;
  gdouble cell_size;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, priv->tile_size, priv->tile_size);
  cairo = cairo_create (surface);

  /* Фон. */
  cairo_set_source_rgba (cairo, 0.9, 0.9, 0.9, 1.0);
  cairo_fill (cairo);

  /* Клетка. */
  cairo_set_line_width (cairo, 2.0);
  cairo_set_source_rgba (cairo, 1.0, 1.0, 1.0, 1.0);
  cell_size = priv->tile_size / 5.0;

  while (y < priv->tile_size)
    {
      cairo_line_to (cairo, 0, y);
      cairo_line_to (cairo, priv->tile_size, y);
      cairo_stroke (cairo);

      y += cell_size;
    }

  while (x < priv->tile_size)
    {
      cairo_line_to (cairo, x, 0);
      cairo_line_to (cairo, x, priv->tile_size);
      cairo_stroke (cairo);

      x += cell_size;
    }

  cairo_destroy (cairo);

  return surface;
}

/* Помещает в кэш информацию о тайле. */
static void
hyscan_gtk_map_tiles_cache_set (HyScanGtkMapTilesPrivate *priv,
                                HyScanGtkMapTile         *tile)
{
  HyScanBuffer *header_buffer;
  HyScanGtkMapTilesCacheHeader header;

  HyScanBuffer *data_buffer;
  const guint8 *data;

  gchar cache_key[255];

  cairo_surface_t *surface;

  if (priv->cache == NULL)
    return;

  /* Будем помещать в кэш изображение pixbuf. */
  surface = hyscan_gtk_map_tile_get_surface (tile);

  /* Не получится использовать буферы priv->{tile|cache}_buffer, т.к. к ним идёт доступ из разных потоков. */
  data_buffer = hyscan_buffer_new ();
  header_buffer = hyscan_buffer_new ();

  /* Оборачиваем в буфер заголовок с информацией об изображении. */
  header.magic = CACHE_HEADER_MAGIC;
  header.size = cairo_image_surface_get_height (surface) * cairo_image_surface_get_stride (surface);
  hyscan_buffer_wrap_data (header_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));

  /* Оборачиваем в буфер пиксельные данные. */
  data = cairo_image_surface_get_data (surface);
  hyscan_buffer_wrap_data (data_buffer, HYSCAN_DATA_BLOB, (gpointer) data, header.size);

  /* Помещаем все данные в кэш. */
  hyscan_gtk_map_tiles_get_cache_key (tile, cache_key, sizeof (cache_key));
  hyscan_cache_set2 (priv->cache, cache_key, NULL, header_buffer, data_buffer);

  cairo_surface_destroy (surface);
  g_object_unref (header_buffer);
  g_object_unref (data_buffer);
}

/* Проверяет, есть ли в буфере тайлы, которые сейчас видны. */
static gboolean
hyscan_gtk_map_tiles_buffer_is_visible (HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  GList *tile_l;

  gint from_x;
  gint from_y;
  gint to_x;
  gint to_y;

  if (priv->filled_buffer == NULL)
    return FALSE;

  /* Определяем видимую область. */
  hyscan_gtk_map_tiles_get_view (layer, priv->zoom, &from_x, &to_x, &from_y, &to_y);

  /* Проверяем, есть ли в буфере тайлы из видимой области. */
  for (tile_l = priv->filled_buffer; tile_l != NULL; tile_l = tile_l->next)
    {
      HyScanGtkMapTile *tile = tile_l->data;

      gint x;
      gint y;

      guint zoom;

      /* Пропускаем тайлы другого зума. */
      zoom = hyscan_gtk_map_tile_get_zoom (tile);
      if (zoom != priv->zoom)
        continue;

      x = hyscan_gtk_map_tile_get_x (tile);
      y = hyscan_gtk_map_tile_get_y (tile);
      if (from_x <= x && x <= to_x && from_y <= y && y <= to_y)
        return TRUE;
    }

  return FALSE;
}

/* Запрашивает перерисовку виджета карты, если пришёл хотя бы один тайл из видимой области. */
static gboolean
hyscan_gtk_map_tiles_filled_buffer_flush (HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  g_mutex_lock (&priv->filled_lock);

  if (hyscan_gtk_map_tiles_buffer_is_visible (layer))
    gtk_widget_queue_draw (GTK_WIDGET (priv->map));

  g_list_free_full (priv->filled_buffer, g_object_unref);
  priv->filled_buffer = NULL;

  g_mutex_unlock (&priv->filled_lock);
  g_object_unref (layer);

  return G_SOURCE_REMOVE;
}

/* Заполняет запрошенный тайл из источника тайлов (выполняется в отдельном потоке). */
static void
hyscan_gtk_map_tiles_load (HyScanGtkMapTile  *tile,
                           HyScanGtkMapTiles *layer,
                           GCancellable       *cancellable)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  /* Если удалось заполнить новый тайл, то запрашиваем обновление области
   * виджета с новым тайлом. */
  if (hyscan_gtk_map_tile_source_fill (priv->source, tile, cancellable))
    {
      hyscan_gtk_map_tiles_cache_set (priv, tile);

      /* Добавляем тайл в буфер заполненных тайлов. */
      g_mutex_lock (&priv->filled_lock);
      priv->filled_buffer = g_list_append (priv->filled_buffer, g_object_ref (tile));
      g_mutex_unlock (&priv->filled_lock);

      gdk_threads_add_idle ((GSourceFunc) hyscan_gtk_map_tiles_filled_buffer_flush,
                            g_object_ref (layer));
    }
}

/* Устанавливает ключ кэширования для тайла @tile. */
static void
hyscan_gtk_map_tiles_get_cache_key (HyScanGtkMapTile *tile,
                                    gchar            *key,
                                    gsize             key_length)
{
  g_snprintf (key, key_length,
              "HyScanGtkMapTiles.t.%u.%u.%u.%u",
              hyscan_gtk_map_tile_get_zoom (tile),
              hyscan_gtk_map_tile_get_x (tile),
              hyscan_gtk_map_tile_get_y (tile),
              hyscan_gtk_map_tile_get_size (tile));
}

/* Функция проверяет кэш на наличие данных и считывает их в буфер tile_buffer. */
static gboolean
hyscan_gtk_map_tiles_cache_get (HyScanGtkMapTilesPrivate *priv,
                                HyScanBuffer             *tile_buffer,
                                HyScanGtkMapTile         *tile)
{
  HyScanGtkMapTilesCacheHeader header;

  gchar cache_key[255];

  if (priv->cache == NULL)
    return FALSE;

  /* Формируем ключ кэшированных данных. */
  hyscan_gtk_map_tiles_get_cache_key (tile, cache_key, sizeof (cache_key));

  /* Ищем данные в кэше. */
  hyscan_buffer_wrap_data (priv->cache_buffer, HYSCAN_DATA_BLOB, &header, sizeof (header));
  if (!hyscan_cache_get2 (priv->cache, cache_key, NULL, sizeof (header), priv->cache_buffer, tile_buffer))
    return FALSE;

  /* Верификация данных. */
  if ((header.magic != CACHE_HEADER_MAGIC) ||
      (header.size != hyscan_buffer_get_size (priv->tile_buffer)))
    {
      return FALSE;
    }

  return TRUE;
}

/* Получает целочисленные координаты верхнего левого и правого нижнего тайлов,
 * полностью покрывающих видимую область. */
static void
hyscan_gtk_map_tiles_get_view (HyScanGtkMapTiles *layer,
                               guint              zoom,
                               gint              *from_tile_x,
                               gint              *to_tile_x,
                               gint              *from_tile_y,
                               gint              *to_tile_y)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  /* Получаем тайлы, соответствующие границам видимой части карты. */
  hyscan_gtk_map_tile_grid_get_view (priv->tile_grid, GTK_CIFRO_AREA (priv->map), zoom,
                                     from_tile_x, to_tile_x, from_tile_y, to_tile_y);
}

/* Растяжение тайла при текущем масштабе карты и указанном зуме. */
static gdouble
hyscan_gtk_map_tiles_get_scaling (HyScanGtkMapTilesPrivate *priv,
                                  guint                     zoom)
{
  gdouble scale;

  /* Размер пикселя в логических единицах. */
  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);
  scale = hyscan_gtk_map_tile_grid_get_scale (priv->tile_grid, zoom) / scale;

  return scale;
}

/* Устанавливает подходящий zoom в зависимости от выбранного масштаба. */
static guint
hyscan_gtk_map_tiles_get_optimal_zoom (HyScanGtkMapTilesPrivate *priv)
{
  gdouble scale;

  gtk_cifro_area_get_scale (GTK_CIFRO_AREA (priv->map), &scale, NULL);

  return hyscan_gtk_map_tile_grid_adjust_zoom (priv->tile_grid, 1 / scale);
}

/* Проверяет, что scale и priv->scale дают один и тот же размер поверхности. */
static gboolean
hyscan_gtk_map_tiles_is_the_same_scale (HyScanGtkMapTilesPrivate *priv,
                                        gdouble                   scale)
{
  guint width;
  guint height;

  width = (guint) priv->tile_size * (priv->to_x - priv->from_x + 1);
  height = (guint) priv->tile_size * (priv->to_y - priv->from_y + 1);

  return (gint) (width / scale) == (gint) (width / priv->scale) &&
         (gint) (height / scale) == (gint) (height / priv->scale);
}

/* Риусет тайл (x, y) в контексте cairo. */
static gboolean
hyscan_gtk_map_tiles_draw_tile (HyScanGtkMapTilesPrivate *priv,
                                cairo_t                  *cairo,
                                HyScanGtkMapTile         *tile)
{
  cairo_surface_t *surface;

  guint tile_size;
  gboolean hit;

  guint x0, y0, x, y;
  gdouble x_point, y_point;

  x0 = priv->from_x;
  y0 = priv->from_y;
  x = hyscan_gtk_map_tile_get_x (tile);
  y = hyscan_gtk_map_tile_get_y (tile);
  tile_size = hyscan_gtk_map_tile_get_size (tile);

  /* Если в кэше найдена поверхность, то устанавливаем её. */
  hit = hyscan_gtk_map_tiles_cache_get (priv, priv->tile_buffer, tile);
  if (hit)
    {
      guchar *cached_data;
      guint32 size;

      cached_data = hyscan_buffer_get_data (priv->tile_buffer, &size);
      hyscan_gtk_map_tile_set_surface_data (tile, cached_data, size);
      surface = hyscan_gtk_map_tile_get_surface (tile);
    }
  else
    {
      surface = cairo_surface_reference (priv->dummy_tile);
    }

  x_point = (x - x0) * tile_size;
  y_point = (y - y0) * tile_size;

  cairo_set_source_surface (cairo, surface, x_point, y_point);
  cairo_paint (cairo);

  cairo_surface_destroy (surface);

  /* Убираем поверхность из тайла, т.к. поверхность использует память внутри priv->tile_buffer,
   * которая может быть повторно использована. */
  hyscan_gtk_map_tile_set_surface (tile, NULL);

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  /* Номер тайла для отладки. */
  {
    gchar label[255];

    cairo_move_to (cairo, (x - x0) * tile_size, (y - y0) * tile_size);
    cairo_set_source_rgb (cairo, 0, 0, 0);
    g_snprintf (label, sizeof (label), "%d, %d", x, y);
    cairo_show_text (cairo, label);
  }
#endif

  return hit;
}

/* Создаёт новую поверхность cairo с тайлами
 * и устанавливает filled = TRUE, если все тайлы заполнены. */
static cairo_surface_t *
hyscan_gtk_map_tile_surface_make (HyScanGtkMapTilesPrivate *priv,
                                  gboolean                 *filled)
{
  cairo_surface_t *surface;
  cairo_t *cairo;

  gboolean filled_ret = TRUE;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (priv->to_x - priv->from_x + 1) * priv->tile_size,
                                        (priv->to_y - priv->from_y + 1) * priv->tile_size);
  cairo = cairo_create (surface);

  /* Отрисовываем все тайлы, которые находятся в видимой области */
  {
    gint r;
    gint max_r;
    gint xc, yc;
    gint x;
    gint y;

    /* Определяем центр области и максимальное расстояние до ее границ. */
    xc = (priv->to_x + priv->from_x + 1) / 2;
    yc = (priv->to_y + priv->from_y + 1) / 2;
    max_r = MAX (MAX (priv->to_x - xc, xc - priv->from_x),
                 MAX (priv->to_y - yc, yc - priv->from_y));

    /* Рисуем тайлы двигаясь из центра (xc, yc) к краям. */
    for (r = 0; r <= max_r; ++r)
      {
        for (x = priv->from_x; x <= (gint) priv->to_x; ++x)
          {
            for (y = priv->from_y; y <= (gint) priv->to_y; ++y)
              {
                HyScanGtkMapTile *tile;
                gboolean tile_filled;

                /* Пока пропускаем тайлы снаружи радиуса. */
                if (abs (y - yc) > r || abs (x - xc) > r)
                  continue;

                /* И рисуем тайлы на расстоянии r от центра по одной из координат. */
                if (abs (x - xc) != r && abs (y - yc) != r)
                  continue;

                tile = hyscan_gtk_map_tile_new (priv->tile_grid, x, y, priv->zoom);

                /* Тайл не найден, добавляем его в очередь на загрузку. */
                tile_filled = hyscan_gtk_map_tiles_draw_tile (priv, cairo, tile);
                if (!tile_filled)
                  hyscan_task_queue_push (priv->task_queue, G_OBJECT (tile));

                filled_ret = filled_ret && tile_filled;

                g_object_unref (tile);
              }
          }
      }
  }

  /* Запускаем обработку сформированной очереди. */
  hyscan_task_queue_push_end (priv->task_queue);

  cairo_destroy (cairo);

  *filled = filled_ret;
  return surface;
}

/* Создаёт новую поверхность cairo с растянутым изображением origin. */
static cairo_surface_t *
hyscan_gtk_map_tile_surface_scale (cairo_surface_t *origin,
                                   gdouble          scale)
{
  cairo_surface_t *surface;
  cairo_t *cairo;

  guint width;
  guint height;

  width = cairo_image_surface_get_width (origin);
  height = cairo_image_surface_get_height (origin);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        (guint) fabs (width * scale),
                                        (guint) fabs (height * scale));

  cairo = cairo_create (surface);
  cairo_scale (cairo, scale, scale);
  cairo_set_source_surface (cairo, origin, 0, 0);

  /* Делаем быстрый билинейный ресемплинг изображения, чтобы gui не тормозил. */
  cairo_pattern_set_filter (cairo_get_source (cairo), CAIRO_FILTER_BILINEAR);
  cairo_paint (cairo);

  cairo_destroy (cairo);

  return surface;
}

/* Обновляет поверхность cairo, чтобы она содержала запрошенную область с тайлами.
 * Возвращает %TRUE, если поверхность была перерисована. */
static gboolean
hyscan_gtk_map_refresh_surface (HyScanGtkMapTilesPrivate *priv,
                                guint                     x0,
                                guint                     xn,
                                guint                     y0,
                                guint                     yn,
                                guint                     zoom)
{
  cairo_surface_t *surface;
  gdouble scale;

  g_return_val_if_fail (x0 <= xn && y0 <= yn, FALSE);

  scale = hyscan_gtk_map_tiles_get_scaling (priv, zoom);

  /* Если нужный регион уже отрисован, то ничего не делаем. */
  if (priv->surface_filled &&
      hyscan_gtk_map_tiles_is_the_same_scale (priv, scale) &&
      priv->from_x <= x0 && priv->to_x >= xn &&
      priv->from_y <= y0 && priv->to_y >= yn &&
      priv->zoom == zoom)
    {
      return FALSE;
    }

  /* Устанавливаем новые размеры поверхности с запасом preload_margin. */
  priv->from_x = CLAMP_TILE (x0 - priv->preload_margin, zoom);
  priv->to_x = CLAMP_TILE (xn + priv->preload_margin, zoom);
  priv->from_y = CLAMP_TILE (y0 - priv->preload_margin, zoom);
  priv->to_y = CLAMP_TILE (yn + priv->preload_margin, zoom);
  priv->zoom = zoom;
  priv->scale = scale;

  /* Рисуем все нужные тайлы в их исходном размере. */
  surface = hyscan_gtk_map_tile_surface_make (priv, &priv->surface_filled);

  /* Затем делаем ресемплинг изображения. */
  g_clear_pointer (&priv->surface, cairo_surface_destroy);
  priv->surface = hyscan_gtk_map_tile_surface_scale (surface, scale);

  cairo_surface_destroy (surface);

  return TRUE;
}

/* Проверяет, что проекция карты и источника тайлов совпадает. */
static gboolean
hyscan_gtk_map_tiles_verify_projection (HyScanGtkMapTiles *layer)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;
  HyScanGeoProjection *map_projection, *source_projection;
  guint map_hash, source_hash;

  source_projection = hyscan_gtk_map_tile_source_get_projection (priv->source);
  map_projection = hyscan_gtk_map_get_projection (priv->map);

  map_hash = hyscan_geo_projection_hash (map_projection);
  source_hash = hyscan_geo_projection_hash (source_projection);

  g_object_unref (map_projection);
  g_object_unref (source_projection);

  return map_hash == source_hash;
}

/* Рисует тайлы текущей видимой области по сигналу "visible-draw". */
static void
hyscan_gtk_map_tiles_draw (HyScanGtkMapTiles *layer,
                           cairo_t           *cairo)
{
  HyScanGtkMapTilesPrivate *priv = layer->priv;

  gboolean refreshed;
  gint x0, xn, y0, yn;
  guint zoom;

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  gdouble time;
#endif

  if (!hyscan_gtk_map_tiles_verify_projection (layer))
    return;

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  g_test_timer_start ();
#endif

  /* Устанавливаем подходящий zoom для видимой области. */
  zoom = hyscan_gtk_map_tiles_get_optimal_zoom (priv);

  /* Получаем границы тайлов, покрывающих видимую область. */
  hyscan_gtk_map_tiles_get_view (layer, zoom, &x0, &xn, &y0, &yn);

  /* Берём только валидные номера тайлов: от 0 до 2^zoom - 1. */
  x0 = CLAMP_TILE (x0, zoom);
  y0 = CLAMP_TILE (y0, zoom);
  xn = CLAMP_TILE (xn, zoom);
  yn = CLAMP_TILE (yn, zoom);

  refreshed = hyscan_gtk_map_refresh_surface (priv, x0, xn, y0, yn, zoom);

  /* Растягиваем буфер под необходимый размер тайла и переносим его на поверхность виджета. */
  {
    gdouble xs, ys;
    gdouble x_val, y_val;

    /* Получаем координаты в логической СК. */
    hyscan_gtk_map_tile_grid_tile_to_value (priv->tile_grid, zoom, priv->from_x, priv->from_y, &x_val, &y_val);

    /* Переводим в коордианты для рисования. */
    gtk_cifro_area_visible_value_to_point (GTK_CIFRO_AREA (priv->map), &xs, &ys, x_val, y_val);

    /* Рисуем буфер. */
    cairo_set_source_surface (cairo, priv->surface, xs, ys);
    cairo_paint (cairo);
  }

#ifdef HYSCAN_GTK_MAP_TILES_DEBUG
  time = g_test_timer_elapsed ();
  g_message ("Draw tiles FPS; %p; %.1f; %d", layer, 1.0 / time, refreshed);
#endif
}

/**
 * hyscan_gtk_map_tiles_new:
 * @map: указатель на объект #HyScanGtkMap
 *
 * Создаёт новый объект #HyScanGtkMapTiles, который рисует слой тайлов на карте
 * @map.
 *
 * Returns: указатель на #HyScanGtkMapTiles
 */
HyScanGtkLayer *
hyscan_gtk_map_tiles_new (HyScanCache             *cache,
                          HyScanGtkMapTileSource  *source)
{
  return g_object_new (HYSCAN_TYPE_GTK_MAP_TILES,
                       "cache", cache,
                       "source", source, NULL);
}
